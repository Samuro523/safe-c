
// cache.c

use ../tracing, ../system, ../memory;
use ../db, config, fs, dbport, dbstruct, backup;

const bool DEBUG_ROLLBACK  = false;
const bool DEBUG_CACHE     = false;

/**********************************************************************/

/* $ check if for a read-only transaction, we never rewrite the header */
/* $ increase again the cache size after testing */
/* $ set again DEBUG 0 in db.c */
/* $ test random insertion/update/delete */
/* $ db files are perhaps corrupt (rollback during a shrinking transaction) */
/*   -> build a tool to extract data from/build a db file. */

/**********************************************************************/

// nb arrows from top to bottom of a tree, minus 1 to avoid sign bit.
const int MAX_LEVELS = (int)(LINK'size * 8) - 1;   // = 63

/**********************************************************************/
#begin unsafe
/**********************************************************************/

/* returns null if out of cache memory */

byte *allocate_cache_memory (ref DB_INFO p, uint size)
{
  byte* pmem, ptr;

  while (size > (uint)(p.cache.end_alloc - p.cache.begin_alloc))   /* not enough mem */
  {
    if (DEBUG_CACHE)
      trace ("cache: allocate_cache_memory() : starting new space %u\n", p.cache.nb_space_blocks);

    /* switch to a new space block */
    if (p.cache.nb_space_blocks >= p.cache_size_limit)
    {
      if (DEBUG_CACHE)
        trace ("cache: allocate_cache_memory() : out of space blocks !\n");
      return null;    /* out of cache memory */
    }

    /* check available virtual memory */
    if (p.cache.nb_space_blocks >= 32 &&  /* 32th block from 512, and later */
        get_virtual_memory_load () > 75)  /* over 75% memory load */
    {
      if (DEBUG_CACHE)
        trace ("cache: allocate_cache_memory() : out of virtual memory !\n");
      return null;    /* out of cache memory */
    }

    pmem = malloc((uint)SPACE_SIZE);

    p.cache.space_block[p.cache.nb_space_blocks++] = pmem;

    p.cache.begin_alloc = pmem;
    p.cache.end_alloc   = pmem + SPACE_SIZE;
  }

  ptr = p.cache.begin_alloc;
  p.cache.begin_alloc += size;

  return ptr;
}

/**********************************************************************/

/* returns null if out of cache memory */

ARROW* allocate_new_arrow (ref DB_INFO p)
{
  ARROW* ptr;

  ptr = p.cache.free_arrow;

  if (ptr == null)    /* free list is empty */
  {
    if (DEBUG_CACHE)
      trace ("cache: allocate_new_arrow() from space block\n");

    ptr = (ARROW *)allocate_cache_memory (ref p, ARROW'size);
    if (ptr == null)
      return null;

    p.cache.nb_arrows++;
  }
  else    /* use free arrow on free list */
  {
    p.cache.free_arrow = (ARROW *)((STUB *)ptr)->next;
    p.cache.nb_free_arrows--;

    if (DEBUG_CACHE)
      trace ("cache: allocate_new_arrow() from free list\n");
  }

  clear *ptr;
  return ptr;
}

/**********************************************************************/

/* returns null if out of cache memory */

BUFFER* allocate_new_buffer (ref DB_INFO p)
{
  BUFFER* ptr;

  ptr = p.cache.free_buffer;

  if (ptr == null)    /* free list is empty */
  {
    if (DEBUG_CACHE)
      trace ("cache: allocate_new_buffer() from space block\n");

    ptr = (BUFFER *)allocate_cache_memory (ref p, BUFFER'size);
    if (ptr == null)
      return null;

    p.cache.nb_buffers++;
  }
  else    /* use free buffer on free list */
  {
    p.cache.free_buffer = (BUFFER *)((STUB *)ptr)->next;
    p.cache.nb_free_buffers--;

    if (DEBUG_CACHE)
      trace ("cache: allocate_new_buffer() from free list\n");
  }

  clear *ptr;
  return ptr;
}

/**********************************************************************/

void deallocate_old_arrow (ref DB_INFO p, ARROW* a)
{
  if (DEBUG_CACHE)
    trace ("cache: deallocate_old_arrow\n");

  ((STUB *)a)->next = (byte *)p.cache.free_arrow;

  p.cache.free_arrow = a;
  p.cache.nb_free_arrows++;
}

/**********************************************************************/

void deallocate_old_buffer (ref DB_INFO p, BUFFER* b)
{
  if (DEBUG_CACHE)
    trace ("cache: deallocate_old_buffer\n");

  ((STUB *)b)->next = (byte *)p.cache.free_buffer;

  p.cache.free_buffer = b;
  p.cache.nb_free_buffers++;
}

/**********************************************************************/

int intern_rollback_transaction (    int          fd,
                                 ref DB_HEADER    db_header,
                                 ref DB_BLOCK[]   space,
                                     BACKUP_INFO^ bi)
{
  LINK        rollback_block_nr;
  int         rc, i;
  DB_ROLLBACK rollback;

  rollback_block_nr = db_header.rollback_link;

  while (rollback_block_nr != 0)
  {
    if (DEBUG_ROLLBACK)
      trace ("cache: read rollback block %d\n", (int8)rollback_block_nr);

    /* read rollback block */
    rc = __db_read (    fd         => fd,
                        block_nr   => rollback_block_nr,
                    out buffer     => rollback);
    if (rc < 0)
      return rc;

    if (DEBUG_ROLLBACK)
      trace ("cache: read %u blocks after rollback block\n", rollback.nb_blocks + (config.WORD)(rollback.next_link == 0));

    for (i=0; i<(int)rollback.nb_blocks; i++)
    {
      rc = __db_read (    fd         => fd,
                          block_nr   => rollback_block_nr + 1 + i,
                      out buffer     => space[i]);
      if (rc < 0)
        return rc;
    }

    if (rollback.next_link == 0)  // read also header
    {
      rc = __db_read (    fd         => fd,
                          block_nr   => rollback_block_nr + 1 + rollback.nb_blocks,
                      out buffer     => space[rollback.nb_blocks]);
      if (rc < 0)
        return rc;
    }

    if (DEBUG_ROLLBACK)
      trace ("cache: restore all saved blocks to their original position\n");

    for (i=0; i<(int)rollback.nb_blocks; i++)
    {
      if (bi != null)
        backup_block (ref bi^, fd, block_nr => rollback.target_link[i]);

      rc = __db_write (fd         => fd,
                       block_nr   => rollback.target_link[i],
                       buffer     => space[i]);
      if (rc < 0)
        return rc;
    }

    if (rollback.next_link == 0)    // restore also db_header
    {
      rc = __db_flush_file (fd);
      if (rc < 0)
        return rc;

      db_header'byte = space[rollback.nb_blocks];

      if (DEBUG_ROLLBACK)
        trace ("cache: finally, restore also db_header\n");

      if (db_header.in_transaction)
        return E_INTERN_25;

      rc = __db_write (fd, 0, db_header);
      if (rc < 0)
        return rc;

      rc = __db_flush_file (fd);
      if (rc < 0)
        return rc;
    }

    rollback_block_nr = rollback.next_link;
  }

  return 0;
}

/**********************************************************************/

/* This function assumes that db_header.in_transaction equals 1 before */
/*   the call (it must equal 0 after the call).                        */
/* The db_header must be updated before returning with OK.             */
/* The rollback chain is not deallocated by this function              */
/* because the last operation of this function is to update the header */
/* on file which restores the state before the transaction.            */

public int rollback_transaction (int fd, ref DB_HEADER db_header, BACKUP_INFO^ bi)
{
  DB_BLOCK[]^ space;
  int         rc;

  if (DEBUG_ROLLBACK)
    trace ("cache: begin rollback transaction\n");

  space = new DB_BLOCK[MAX_SAVED_BLOCKS_PER_ROLLBACK_BLOCK + 1];
  rc = intern_rollback_transaction (fd, ref db_header, ref space^, bi);
  free space;

  if (DEBUG_ROLLBACK)
    trace ("cache: end rollback transaction\n");

  return rc;
}

/**********************************************************************/

/* travel within or create arrows til reaching the target buffer.    */
/* returns 0 if OK, -1 if could not allocate enough arrows.          */
/* yield 'pbuffer' : ptr to leaf arrow entry pointing to the buffer. */
/* note: leaf arrow entry is null if the buffer does not exist.      */

int find_buffer_in_tree (LINK         block_nr,
                         ref DB_INFO  p,
                         out BUFFER** pbuffer)
{
  ARROW* a*, n;
  int    i;
  bool   bit;

  a = &p.cache.tree;

  for (i=0; i<MAX_LEVELS; i++)    // range 0 to 62 (63 levels)
  {
    n = *a;

    if (n == null)    /* no arrow at this level */
    {
      n = allocate_new_arrow (ref p);
      if (n == null)
      {
        pbuffer = null;
        return -1;
      }

      *a = n;
    }

    /* use highest bit first to use less arrows */

    bit = (block_nr & ((LINK)1 << (LINK)(MAX_LEVELS-1-i))) != 0;   // mask is (1 << 62) to (1 << 0)

    a = &n->child[(int)bit];
  }

  pbuffer = (BUFFER **)a;
  return 0;
}

/**********************************************************************/

/* travel to block_nr within the tree and delete all unused arrows. */
/* assertion: there is no buffer below this path.                   */

void deallocate_unused_arrow_path (LINK        block_nr,
                                   ref DB_INFO p)
{
  ARROW* a*, n, table[MAX_LEVELS]*, left, right;
  int    i;
  bool   bit;

  clear table;
  a = &p.cache.tree;

  for (i=0; i<MAX_LEVELS; i++)
  {
    n = *a;

    if (n == null)    /* no arrow at this level : done */
      break;

    table[i] = a;      /* save arrow origin's address */

    bit = (block_nr & ((LINK)1 << (LINK)(MAX_LEVELS-1-i))) != 0;

    a = &n->child[(int)bit];
  }

  while (i > 0)
  {
    i--;

    left  = (*table[i])->child[0];
    right = (*table[i])->child[1];

    if (left != null || right != null)  /* node still in use */
      break;

    deallocate_old_arrow (ref p, (*table[i]));
    (*table[i]) = null;
  }
}

/**********************************************************************/

package STAMP
  const uint TIME_STAMP_SLICE = 100;    /* keep 100 last-accessed buffers */
end STAMP;

/**********************************************************************/

/* strategy 1 (soft) : free all "old" blocks that are non-dirty */
/*                     and that have no locks on them.          */
/* (this is adequate for large read-only transactions).         */

void recursive_soft_flush (ref DB_INFO p,
                           int         level,    /* 1 (1 .. 32) or (1 .. 64) */
                           LINK        block_nr, /* 0 (filled at each level) */
                           ARROW       **a)
{
  ARROW* n, left*, right*;

  n = *a;

  if (n == null)    /* nothing at this level */
    return;

  if (level <= MAX_LEVELS)        // n points to an ARROW    level <= 63
  {
    left  = &n->child[0];
    right = &n->child[1];

    if (*left != null)
      recursive_soft_flush (ref p, level+1, block_nr, left);

    if (*right != null)
      recursive_soft_flush (ref p, level+1, block_nr + ((LINK)1 << (LINK)(MAX_LEVELS-level)), right);

    if (*left == null && *right == null)
    {
      deallocate_old_arrow (ref p, *a);
      *a = null;
    }
  }
  else   // n points to a BUFFER   level == 64
  {
    BUFFER* buf;

    buf = (BUFFER *)n;

    if ((!buf->dirty) && buf->nb_locks == 0 &&
        p.cache.time_stamp - buf->time_stamp > TIME_STAMP_SLICE)
    {
      deallocate_old_buffer (ref p, buf);
      *a = null;
    }
  }
}

/**********************************************************************/

/* collect all original blocks where block_nr < L and dirty and no locks */

int recursive_collect_original_blocks
         (ref DB_INFO p,
          int         level,      /* 1 (1 .. 33) */
          LINK        block_nr,   /* 0 (filled at each level) */
          ARROW       **a)
{
  ARROW* n, left*, right*;
  int    rc;

  n = *a;

  if (n == null)    /* nothing at this level */
    return 0;

  if (block_nr >= p.initial_free_space_link)
    return 0;

  if (level <= MAX_LEVELS)        /* n points to an ARROW */
  {
    left  = &n->child[0];
    right = &n->child[1];

    if (*left != null)
    {
      rc = recursive_collect_original_blocks (ref p, level+1, block_nr, left);
      if (rc < 0)
        return rc;
    }

    if (*right != null)
    {
      rc = recursive_collect_original_blocks
                     (ref p, level+1, block_nr + ((LINK)1 << (LINK)(MAX_LEVELS-level)), right);
      if (rc < 0)
        return rc;
    }
  }
  else   /* n points to a BUFFER */
  {
    BUFFER* buf;

    buf = (BUFFER *)n;

    if (buf->dirty && buf->nb_locks == 0 &&
        (!buf->original_data_loaded) && (!buf->saved_in_rollback))
    {
      if (DEBUG_CACHE)
        trace ("cache: recursive_collect_original_blocks() : read block %d\n", (int8)block_nr);

      rc = __db_read (    p.db,
                          block_nr,
                      out buf->original_data);
      if (rc < 0)
        return rc;

      buf->original_data_loaded = true;
    }
  }

  return 0;
}

/**********************************************************************/

/* rewrite all blocks >= L and dirty and no locks */

int recursive_write_blocks_after_eof
         (ref DB_INFO p,
          int         level,      /* 1 (1 .. 33) or (1 .. 65) */
          LINK        block_nr,   /* 0 (filled at each level) */
          ARROW       **a)
{
  ARROW* n, left*, right*;
  int    rc;

  n = *a;

  if (n == null)    /* nothing at this level */
    return 0;

  if (level <= MAX_LEVELS)        /* (*a) points to an ARROW */
  {
    left  = &n->child[0];
    right = &n->child[1];

    if (*left != null &&
        block_nr + ((LINK)1 << (LINK)(MAX_LEVELS-level)) > p.initial_free_space_link)
    {
      rc = recursive_write_blocks_after_eof (ref p, level+1, block_nr, left);
      if (rc < 0)
        return rc;
    }

    if (*right != null)
    {
      rc = recursive_write_blocks_after_eof
                     (ref p, level+1, block_nr + ((LINK)1 << (LINK)(MAX_LEVELS-level)), right);
      if (rc < 0)
        return rc;
    }
  }
  else   /* n points to a BUFFER */
  {
    BUFFER* buf;

    buf = (BUFFER *)n;

    if (buf->dirty && buf->nb_locks == 0 && block_nr >= p.initial_free_space_link)
    {
      if (DEBUG_CACHE)
        trace ("cache: recursive_write_blocks_after_eof() : write block %d\n", (int8)block_nr);

      if (block_nr >= LINK'max - 1)
        return E_NOSPC;

      rc = __db_extend_file (p.db, block_nr+1);
      if (rc < 0)
        return rc;

      if (p.pbackup != null)
        backup_block (ref p.pbackup^, p.db, block_nr => block_nr);

      rc = __db_write (p.db, block_nr, buf->data);
      if (rc < 0)
        return rc;

      buf->dirty = false;
    }
  }

  return 0;
}

/**********************************************************************/

package P_ROLLBACK

  /* must be cleared before the first recursive call */

  struct ROLLBACK_STATE_DATA
  {
    DB_ROLLBACK         rollback;
      /* current rollback block (will be written when full) */

    BUFFER*             buffer[MAX_SAVED_BLOCKS_PER_ROLLBACK_BLOCK];
      /* pointers to buffer with the original data */

    ROLLBACK_CHUNK_INFO rollback_chunk_info;
      /* info about entire chunk (will be returned to caller */
      /* for optimized deallocation of the entire chunk)     */
  }

end P_ROLLBACK;

/**********************************************************************/

/* this function can be called several times if the cache is flushed several times : */
/* n times when the cache is full, then 1 time when closing the transaction. */

int set_physical_file_in_transaction (ref DB_INFO p)
{
  int rc;

  if (p.db_header_dirty)   /* not for a read-only transaction */
  {
    /* be sure to flush all blocks of the rollback chunk */
    /* before changing the header */

    rc = __db_flush_file (p.db);
    if (rc < 0)
      return rc;

    /* at this point, we change the header's .in_transaction flag to 1 and activate the rollback chain */
    /* to indicate that the chain must be rolled back in case of interruption. */
    /* note that this can occur several times if the cache is flushed several times. */

    p.db_header.in_transaction = true;

    /* be sure there is a rollback chain */
    assert (p.db_header.rollback_link != 0);

    rc = __db_write (p.db, 0, p.db_header);
    if (rc < 0)
      return rc;

    /* be sure to flush the header before overwriting blocks < L */
    rc = __db_flush_file (p.db);
    if (rc < 0)
      return rc;
  }

  return 0;
}

/**********************************************************************/

/* must only be called if at least 1 block in rollback block */
/* or if p->db_header_dirty and p->db_header.rollback_link == null */

int write_rollback_chunk (ref DB_INFO             p,
                          ref ROLLBACK_STATE_DATA state)
{
  config.WORD i;
  int  rc;
  LINK block_nr;


  /* complete rollback block */

  state.rollback.next_link = p.db_header.rollback_link;


  /* extend the file */

  if (p.db_header.free_space_link >=
           LINK'max - (state.rollback.nb_blocks + 1)
                    - (LINK)(state.rollback.next_link == 0))
    return E_NOSPC;

  rc = __db_extend_file (p.db,
                         p.db_header.free_space_link
                              + (state.rollback.nb_blocks + 1)
                              + (LINK)(state.rollback.next_link == 0));
  if (rc < 0)
    return rc;


  /* write now the rollback block */
  if (p.pbackup != null)
    backup_block (ref p.pbackup^, p.db, block_nr => p.db_header.free_space_link);

  rc = __db_write (p.db, p.db_header.free_space_link, state.rollback);
  if (rc < 0)
    return rc;


  /* write now all original blocks */
  block_nr = p.db_header.free_space_link + 1;

  for (i=0; i<state.rollback.nb_blocks; i++)
  {
    if (p.pbackup != null)
      backup_block (ref p.pbackup^, p.db, block_nr => block_nr);

    rc = __db_write (p.db,
                     block_nr,
                     state.buffer[i]->original_data);
    if (rc < 0)
      return rc;

    block_nr++;
  }

  if (state.rollback.next_link == 0)
  {
    /* append original header to the list */
    if (p.pbackup != null)
      backup_block (ref p.pbackup^, p.db, block_nr => block_nr);

    rc = __db_write (p.db, block_nr, p.initial_db_header);
    if (rc < 0)
      return rc;
  }


  /* save return info */

  if (state.rollback_chunk_info.first == 0)
  {
    state.rollback_chunk_info.first = p.db_header.free_space_link;
    state.rollback_chunk_info.next_rollback_link = p.db_header.rollback_link;
  }

  state.rollback_chunk_info.count +=
      (state.rollback.nb_blocks + 1) + (config.LONG)(state.rollback.next_link == 0);


  /* update db_header */

  p.db_header.rollback_link = p.db_header.free_space_link;

  p.db_header.free_space_link += (state.rollback.nb_blocks + 1)
                                   + (config.WORD)(state.rollback.next_link == 0);

  p.db_header_dirty = true;

  /* clear structure for next call */
  clear state.rollback;

  return 0;
}

/**********************************************************************/

int add_rollback_buffer (ref DB_INFO             p,
                         LINK                    block_nr,
                         BUFFER                  *buffer,
                         ref ROLLBACK_STATE_DATA state)
{
  int rc;

  if (DEBUG_CACHE)
    trace ("cache: add_rollback_buffer() : block %d\n", (int8)block_nr);

  assert state.rollback.nb_blocks < MAX_SAVED_BLOCKS_PER_ROLLBACK_BLOCK;

  state.rollback.target_link[state.rollback.nb_blocks] = block_nr;
  state.buffer[state.rollback.nb_blocks] = buffer;
  state.rollback.nb_blocks++;

  if (state.rollback.nb_blocks == MAX_SAVED_BLOCKS_PER_ROLLBACK_BLOCK)
  {
    /* rollback chunk is full : save it to disk */
    rc = write_rollback_chunk (ref p, ref state);
    if (rc < 0)
      return rc;
  }

  return 0;
}

/**********************************************************************/

/* create rollback chunk for all buffers that are dirty with no locks, */
/* with block_nr < L and with saved_in_rollback == 0 (not yet saved in */
/* earlier rollback chunk.                                             */
/* assertion: original_data_loaded == true. */
/* 'state' must be cleared before the call. */

int recursive_create_rollback_chunk
         (ref DB_INFO             p,
          int                     level,      /* 1 .. 33 or 1 .. 65 */
          LINK                    block_nr,   /* filled at each level */
          ARROW                   **a,
          ref ROLLBACK_STATE_DATA state)
{
  ARROW* n, left*, right*, nleft, nright;
  int    rc;

  n = *a;

  if (n == null)    /* nothing at this level */
    return 0;

  if (block_nr >= p.initial_free_space_link)
    return 0;

  if (level <= MAX_LEVELS)        /* n points to an ARROW */
  {
    left  = &n->child[0];
    right = &n->child[1];

    nleft  = *left;
    nright = *right;

    if (nleft != null)
    {
      rc = recursive_create_rollback_chunk (ref p, level+1, block_nr, left, ref state);
      if (rc < 0)
        return rc;
    }

    if (nright != null)
    {
      rc = recursive_create_rollback_chunk
             (ref p, level+1, block_nr + ((LINK)1 << (LINK)(MAX_LEVELS-level)), right, ref state);
      if (rc < 0)
        return rc;
    }
  }
  else   /* n points to a BUFFER */
  {
    BUFFER* buf;

    buf = (BUFFER *)n;

    if (buf->dirty && buf->nb_locks == 0 && (!buf->saved_in_rollback))
    {
      assert (buf->original_data_loaded);

      rc = add_rollback_buffer (ref p, block_nr, buf, ref state);
      if (rc < 0)
        return rc;

      buf->saved_in_rollback = true;
    }
  }

  return 0;
}

/**********************************************************************/

/* write rollback chunk at EOF for all buffers             */
/* that are dirty with no locks and with block_nr < L      */
/* this involves also updating the db_header at block_nr=0 */

int create_rollback_chunk (ref DB_INFO             p,
                           out ROLLBACK_STATE_DATA state)
{
  int rc;

  clear state;

  rc = recursive_create_rollback_chunk (ref p, 1, 0, &p.cache.tree, ref state);
  if (rc < 0)
    return rc;

  if (state.rollback.nb_blocks > 0 ||   /* some blocks to write */
      (p.db_header_dirty && p.db_header.rollback_link == 0))  /* or header must be written */
  {
    rc = write_rollback_chunk (ref p, ref state);
    if (rc < 0)
      return rc;
  }

  rc = set_physical_file_in_transaction (ref p);
  if (rc < 0)
    return rc;

  return 0;
}

/**********************************************************************/

/* write all blocks with block_nr < L with dirty and no locks */

int recursive_write_dirty_blocks
         (ref DB_INFO p,
          int         level,      /* 1 .. 33 or 1 .. 65 */
          LINK        block_nr,   /* filled at each level */
          ARROW       **a)
{
  ARROW* n, left*, right*, nleft, nright;
  int    rc;

  n = *a;

  if (n == null)    /* nothing at this level */
    return 0;

  if (block_nr >= p.initial_free_space_link)
    return 0;

  if (level <= MAX_LEVELS)        /* (*a) points to an ARROW */
  {
    left  = &n->child[0];
    right = &n->child[1];

    nleft  = *left;
    nright = *right;

    if (nleft != null)
    {
      rc = recursive_write_dirty_blocks (ref p, level+1, block_nr, left);
      if (rc < 0)
        return rc;
    }

    if (nright != null)
    {
      rc = recursive_write_dirty_blocks
                     (ref p, level+1, block_nr + ((LINK)1 << (LINK)(MAX_LEVELS-level)), right);
      if (rc < 0)
        return rc;
    }
  }
  else   /* n points to a BUFFER */
  {
    BUFFER* buf;

    buf = (BUFFER *)n;

    if (buf->dirty && buf->nb_locks == 0)
    {
      if (DEBUG_CACHE)
        trace ("cache: recursive_write_dirty_blocks() : write block %d\n", (int8)block_nr);

      assert (buf->saved_in_rollback);

      if (block_nr >= LINK'max - 1)
        return E_NOSPC;

      rc = __db_extend_file (p.db, block_nr + 1);
      if (rc < 0)
        return rc;

      if (p.pbackup != null)
        backup_block (ref p.pbackup^, p.db, block_nr => block_nr);

      rc = __db_write (p.db, block_nr, buf->data);
      if (rc < 0)
        return rc;

      buf->dirty = false;

      buf->original_data = buf->data;
    }
  }

  return 0;
}

/**********************************************************************/

void free_all_tree (ref DB_INFO p, int level, LINK block_nr, ARROW **a)
{
  ARROW* n, left*, right*, nleft, nright;

  n = *a;

  if (n == null)    /* nothing at this level */
    return;

  if (level <= MAX_LEVELS)        /* n points to an ARROW */
  {
    left  = &n->child[0];
    right = &n->child[1];

    nleft  = *left;
    nright = *right;

    if (nleft != null)
      free_all_tree (ref p, level+1, block_nr, left);

    if (nright != null)
      free_all_tree (ref p, level+1, block_nr + ((LINK)1 << (LINK)(MAX_LEVELS-level)), right);

    /* reload node values because previous calls might have changed them */

    nleft  = *left;
    nright = *right;

    assert (nleft == null && nright == null);

    deallocate_old_arrow (ref p, *a);
    *a = null;
  }
  else   /* n points to a BUFFER */
  {
    BUFFER* buf;

    buf = (BUFFER *)n;

    if (buf == null)
      return;

    if (buf->dirty || buf->nb_locks > 0)
    {
      trace ("error: block_nr %d  dirty=%u  nb_locks=%u\n", (int8)block_nr, buf->dirty, buf->nb_locks);
      abort;
    }

    deallocate_old_buffer (ref p, buf);
    *a = null;
  }
}

/**********************************************************************/

/* check that : */
/* 1) all buffers have nb_locks == 0 and dirty == 0 */
/* 2) free all arrows and all buffers and check counters equality */

void check_cache_validity (ref DB_INFO p)
{
  free_all_tree (ref p, 1, 0, &p.cache.tree);

  if (p.cache.nb_arrows  != p.cache.nb_free_arrows ||
      p.cache.nb_buffers != p.cache.nb_free_buffers)
  {
    trace ("nb_buffers=%d, nb_free_buffers=%d\n", (int8)p.cache.nb_buffers, (int8)p.cache.nb_free_buffers);
    trace ("nb_arrows=%d, nb_free_arrows=%d\n", (int8)p.cache.nb_arrows, (int8)p.cache.nb_free_arrows);
    abort;
  }
}

/**********************************************************************/

/* this function is called : */
/* 1) when no more cache memory could be allocated :              */
/*    -> we don't flush the latest-accessed buffers or those that */
/*       have locks.                                              */
/* 2) when closing the transaction (final_flush == 1) :           */
/*    -> we flush all buffers (locks are not allowed here)        */

public int flush_cache_buffers
  (ref DB_INFO             p,
       bool                final_flush,
   out bool                rollback_done,
   out ROLLBACK_CHUNK_INFO rollback_info)  /* filled if rollback_done */
{
  int                 rc;
  ROLLBACK_STATE_DATA state;

  rollback_done = false;
  clear rollback_info;

  if (!final_flush)    /* cache is full */
  {
    if (DEBUG_CACHE)
      trace ("cache: flush_cache_buffers() : before soft_flush()\n");

    /* soft strategy : free all "old" blocks that are non-dirty */
    /*                 and that have no locks on them.          */
    /* (this is adequate for large read-only transactions).     */

    recursive_soft_flush (ref p, 1, 0, &p.cache.tree);

    if (DEBUG_CACHE)
      trace ("cache: flush_cache_buffers() : after soft_flush()\n");

    if (p.cache.nb_free_buffers >= p.cache.nb_buffers / 4 * 3 &&
        p.cache.nb_free_arrows  >= p.cache.nb_arrows  / 4 * 3 &&
        p.cache.nb_free_buffers >= 1000 &&
        p.cache.nb_free_arrows  >= 1000)    /* prevent loop */
    {
      /* more than 3/4 of buffers and arrows are free -> that's enough */
      return 0;
    }
  }


  /* hard flush methods */


  /* collect all original blocks where block_nr < L and dirty and no locks */

  if (DEBUG_CACHE)
    trace ("cache: flush_cache_buffers() : before recursive_collect_original_blocks()\n");

  rc = recursive_collect_original_blocks (ref p, 1, 0, &p.cache.tree);

  if (DEBUG_CACHE)
    trace ("cache: flush_cache_buffers() : after recursive_collect_original_blocks() rc=%d\n", rc);

  if (rc < 0)
    return rc;


  /* rewrite all blocks >= L and dirty and no locks */

  if (DEBUG_CACHE)
    trace ("cache: flush_cache_buffers() : before recursive_write_blocks_after_eof()\n");

  rc = recursive_write_blocks_after_eof (ref p, level => 1, block_nr => 0, &p.cache.tree);

  if (DEBUG_CACHE)
    trace ("cache: flush_cache_buffers() : after recursive_write_blocks_after_eof() rc=%d\n", rc);

  if (rc < 0)
    return rc;


  /* write rollback chunk at EOF for all buffers             */
  /* that are dirty with no locks and with block_nr < L      */
  /* this involves also updating the db_header at block_nr=0 */

  if (DEBUG_CACHE)
    trace ("cache: flush_cache_buffers() : before create_rollback_chunk()\n");

  rc = create_rollback_chunk (ref p, out state);

  if (DEBUG_CACHE)
    trace ("cache: flush_cache_buffers() : after create_rollback_chunk() rc=%d\n", rc);

  if (rc < 0)
    return rc;

  rollback_info = state.rollback_chunk_info;
  rollback_done = (rollback_info.first != 0);


  /* write all blocks with block_nr < L with dirty and no locks */

  if (DEBUG_CACHE)
    trace ("cache: flush_cache_buffers() : before recursive_write_dirty_blocks()\n");

  rc = recursive_write_dirty_blocks (ref p, 1, 0, &p.cache.tree);

  if (DEBUG_CACHE)
    trace ("cache: flush_cache_buffers() : after recursive_write_dirty_blocks() rc=%d\n", rc);

  if (rc < 0)
    return rc;


  if (!final_flush)
  {
    /* again : free all "old" blocks that are non-dirty */
    /*         and that have no locks on them.          */
    if (DEBUG_CACHE)
      trace ("cache: flush_cache_buffers() : before soft_flush()\n");

    recursive_soft_flush (ref p, 1, 0, &p.cache.tree);

    if (DEBUG_CACHE)
      trace ("cache: flush_cache_buffers() : after soft_flush()\n");
  }

  if (DEBUG_CACHE)
    trace ("cache: flush_cache_buffers() : done() : rollback:(begin=%d, count=%d, next=%d)\n",
           (int8)rollback_info.first, (int8)rollback_info.count, (int8)rollback_info.next_rollback_link);

  if (final_flush)
    check_cache_validity (ref p);

  return 0;
}

/**********************************************************************/

public int open_block (out BUFFER* pbuffer,      /* result: ptr to buffer */
                       ref DB_INFO p,
                           LINK    block_nr,
                           bool    clear_data)   /* true = fill with zeroes,
                                                    false = read from db file */
{
  BUFFER*             pbuf*, buf;
  int                 rc;
  bool                dummy1;
  ROLLBACK_CHUNK_INFO dummy2;

  if (DEBUG_CACHE)
    trace ("open_block (block_nr=%d, clear=%u)\n", (int8)block_nr, clear_data);

  clear pbuffer;

  if (block_nr <= 0 || block_nr >= p.db_header.free_space_link)
    return E_INTERN_27;


  /* travel within or create arrows til reaching the target buffer */

  if (find_buffer_in_tree (block_nr, ref p, out pbuf) < 0)  /* cache is full */
  {
    deallocate_unused_arrow_path (block_nr, ref p);

    /* free at least 3/4 of the cache and retry the operation */
    rc = flush_cache_buffers (ref p, final_flush => false, out dummy1, out dummy2);
    _unused dummy1;
    _unused dummy2;
    if (rc < 0)
      return rc;

    return open_block (out pbuffer, ref p, block_nr, clear_data);
  }

  buf = *pbuf;

  if (buf == null)    /* buffer does not exist */
  {
    buf = allocate_new_buffer (ref p);
    if (buf == null)      /* cache is full : could not create buffer */
    {
      deallocate_unused_arrow_path (block_nr, ref p);

      /* free at least 3/4 of the cache and retry the operation */
      rc = flush_cache_buffers (ref p, final_flush => false, out dummy1, out dummy2);
      _unused dummy1;
      _unused dummy2;
      if (rc < 0)
        return rc;

      return open_block (out pbuffer, ref p, block_nr, clear_data);
    }


    /* initialize the new buffer */

    buf->nb_locks = 1;

    if (clear_data)     /* set block to zeroes (already done) */
    {
      buf->dirty = true;
      if (DEBUG_CACHE)
        trace ("open_block() : clear block\n");
    }
    else                /* load original data from disk */
    {
      if (DEBUG_CACHE)
        trace ("open_block() : read block from file\n");

      rc = __db_read (p.db, block_nr, out buf->data);
      if (rc < 0)
      {
        deallocate_old_buffer (ref p, buf);
        deallocate_unused_arrow_path (block_nr, ref p);
        return rc;
      }

      buf->original_data = buf->data;
      buf->original_data_loaded = true;
    }

    *pbuf = buf;   // link buffer into leaf arrow

    pbuffer = buf;
  }
  else        /* buffer exists */
  {
    if (clear_data)
    {
      assert (buf->nb_locks == 0);

      buf->dirty = true;
      clear buf->data;
    }

    if (buf->nb_locks == 255)
    {
      trace ("fatal error: db layer : cache.c : nb_locks > 255\n");
      abort;
    }

    buf->nb_locks++;

    if (DEBUG_CACHE)
      trace ("open_block() : increase nb_locks = %u\n", buf->nb_locks);

    pbuffer = buf;
  }

  return 0;
}

/**********************************************************************/

public void set_dirty (ref DB_INFO p, BUFFER* buffer)
{
  _unused p;

  if (DEBUG_CACHE)
    trace ("set_dirty\n");

  assert (buffer != null && buffer->nb_locks > 0);

  buffer->dirty = true;
}

/**********************************************************************/

public void close_block (ref DB_INFO p, BUFFER* buffer)
{
  if (DEBUG_CACHE)
    trace ("close_block\n");

  assert (buffer != null && buffer->nb_locks > 0);

  buffer->nb_locks--;
  buffer->time_stamp = p.cache.time_stamp++;
}

/**********************************************************************/

public void set_db_header_dirty (ref DB_INFO p)
{
  p.db_header_dirty = true;
}

/**********************************************************************/

/* if block is present in cache, disable its data       */
/* (this function is called if a block is deallocated). */

public void invalidate_buffer (ref DB_INFO  p,
                               LINK         block_nr)
{
  ARROW*  a*, n;
  int     i;
  bool    bit;
  BUFFER* buf;

  if (DEBUG_CACHE)
    trace ("cache: invalidate_buffer (block %d)\n", (int8)block_nr);

  assert (block_nr < p.db_header.free_space_link);

  a = &p.cache.tree;

  for (i=0; i<MAX_LEVELS; i++)
  {
    n = *a;

    if (n == null)    /* no arrow at this level */
      return;

    /* use highest bit first to use less arrows */

    bit = (block_nr & ((LINK)1 << (LINK)(MAX_LEVELS-1-i))) != 0;

    a = &n->child[(int)bit];
  }

  n = *a;

  buf = (BUFFER *)n;
  if (buf == null)         /* buffer not found */
  {
    if (DEBUG_CACHE)
      trace ("cache: invalidate_buffer() : not found in cache\n");
    return;
  }

  if (buf->dirty)
  {
    assert (buf->nb_locks == 0);

    if (buf->original_data_loaded)
    {
      buf->data = buf->original_data;
      buf->dirty = false;

      if (DEBUG_CACHE)
        trace ("cache: invalidate_buffer() : found dirty block : clean it\n");
    }
    else
    {
      deallocate_old_buffer (ref p, buf);
      *a = null;

      deallocate_unused_arrow_path (block_nr, ref p);

      if (DEBUG_CACHE)
        trace ("cache: invalidate_buffer() : found dirty block : no original block : deallocate it\n");
    }
  }
}

/**********************************************************************/

public void deallocate_cache (ref DB_INFO p)
{
  uint i;

  if (DEBUG_CACHE)
    trace ("cache: deallocate_cache()\n");

  for (i=0; i<p.cache.nb_space_blocks; i++)
    freem (p.cache.space_block[i]);

  clear p.cache;
}

/**********************************************************************/
#end unsafe
/**********************************************************************/
