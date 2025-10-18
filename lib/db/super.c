
// super.c

#define unit_test 0

#if unit_test
  use ../console;
#endif

use ../db;
use config, fs, dbstruct, dbport, cache, backup;

/**********************************************************************/

/* a super block is created if the free block table in the header overflows,*/
/* or it is removed if the free block table in the header becomes empty.    */
/* We always move HALF_MAX_FREE_BLOCK_RANGES entries between header and     */
/* super block.                                                             */

packed struct DB_SUPER
{
  LINK     next_link;                           /* link to next super block */
  A_RANGE  range[HALF_MAX_FREE_BLOCK_RANGES];      /* all entries are valid */
  byte     filler[DB_BLOCK_SIZE - LINK'size - A_RANGE'size*HALF_MAX_FREE_BLOCK_RANGES - CHECKSUM'size];
  CHECKSUM checksum;
}

/**************************************************************************/

uint count_ranges (SUPER_TABLE super)
{
  uint i;
  for (i=0; i<(uint)super.range'length; i++)
  {
    if (super.range[i].first == 0)
      break;
  }
  return i;
}

/**************************************************************************/

// remove range at index 'i'

void remove_super_range (ref SUPER_TABLE super, uint i)
{
  uint count = count_ranges(super) - 1 - i;
  super.range[i:count] = super.range[i+1:count];
  clear super.range[i+count];
}

/**********************************************************************/

/* insert the range (block_nr, count) in header's super table. */
/* return 0 if OK, -1 if super table is full.                  */

int insert_range_in_super_table (    LINK        block_nr,
                                     uint        count,
                                 ref SUPER_TABLE super)
{
  uint i, nb, nb_ranges;

  assert count > 0;

  nb_ranges = count_ranges(super);
  
  for (i=0; i<nb_ranges; i++)
  {
    ref A_RANGE r = super.range[i];

    if (block_nr > r.first + r.count)   // new block begins after i : go on
      continue;

    // there are 3 cases

    // case 1 : before i
    if (block_nr + count < r.first)   // insert at i
      break;

    // case 2 : prefix to range i
    if (block_nr + count == r.first)
    {
      r.first -= count;
      r.count += count;
      return 0;
    }

    assert block_nr == r.first + r.count;  // otherwise overlap with range

    // case 3 : at the end of range i
    r.count += count;

    // try to merge with next range
    if (i+1 < nb_ranges)
    {
      ref A_RANGE s = super.range[i+1];

      if (r.first + r.count == s.first)
      {
        r.count += s.count;
        remove_super_range (ref super, i+1);   // delete 'i+1'
      }
    }

    return 0;
  }

  if (nb_ranges == MAX_FREE_BLOCK_RANGES)   // table is full
    return -1;


  /* insert new range in slot 'i' */

  nb = nb_ranges - i;
  super.range[i+1:nb] = super.range[i:nb];

  super.range[i] = {first => block_nr,
                    count => count};

  return 0;
}

/**********************************************************************/
#begin unsafe
/**********************************************************************/

/* advised_block: if possible, get a block just after 'advised_block'. */
/* alignment    : if possible, get a block from a range having         */
/*                at least 'alignment' entries.                        */

public int allocate_block (out LINK    block_nr,
                           ref DB_INFO p,
                               LINK    advised_block,
                               WORD    alignment)
{
  int    rc;
  BUFFER *buffer;
  LINK   super_block;
  uint   i, nb_ranges;

  clear block_nr;

  if (count_ranges (p.db_header.super) == 0)    // super table is empty
  {
    if (p.db_header.super.super_link == 0)  // no super blocks
    {
      // allocate block from end-of-file
      if (p.db_header.free_space_link == LINK'max)
        return E_NOSPC;    // no space left

      /* extend physical file to avoid read error with 0 bytes in recursive_collect_original_blocks() */
      /* this could happen in case free_space_link was increased during allocation */
      /* but blocks do not exist in file because they were deallocated. */

      rc = __db_extend_file (p.db, p.db_header.free_space_link+1);
      if (rc < 0)
        return rc;

      block_nr = p.db_header.free_space_link++;

      set_db_header_dirty (ref p);
      return 0;
    }
    else   /* load a super block into header */
    {
      super_block = p.db_header.super.super_link;

      rc = open_block (out buffer,
                       ref p,
                           block_nr   => super_block,
                           clear_data => false);
      if (rc < 0)
        return rc;


      {
        ref DB_SUPER super = *((DB_SUPER *)&buffer->data[0]);

        clear p.db_header.super;
        p.db_header.super.super_link = super.next_link;
        p.db_header.super.range[0:HALF_MAX_FREE_BLOCK_RANGES] = super.range[0:HALF_MAX_FREE_BLOCK_RANGES];

        set_db_header_dirty (ref p);

        close_block (ref p, buffer);

        invalidate_buffer (ref p, super_block);

        /* add super_block itself to the list (there are half free slots in super table so no overflow can occur) */
        if (insert_range_in_super_table (block_nr => super_block, count => 1, ref p.db_header.super) < 0)
          return E_INTERN_13;
      }
    }
  }


  /* allocate a block from super table (that has at least 1 range) */

  nb_ranges = count_ranges (p.db_header.super);

  if (nb_ranges == 0)
    return E_INTERN_14;

  for (i=0;
       i < (nb_ranges - 1) &&           // i must denote a valid slot when leaving
       advised_block <= p.db_header.super.range[i].first;
       i++)
  {
  }

  /* make a little optimization effort to reduce fragmentation */

  if (p.db_header.super.range[i].count < alignment)
  {
    if (i+1 < nb_ranges &&           // try next range
        p.db_header.super.range[i+1].count >= alignment)
    {
      i++;
    }
    else if (i >= 1 &&              // try previous range (i unsigned !)
             p.db_header.super.range[i-1].count >= alignment)
    {
      i--;
    }
  }

  block_nr = p.db_header.super.range[i].first++;
  p.db_header.super.range[i].count--;

  if (p.db_header.super.range[i].count == 0)        // range is now empty
    remove_super_range (ref p.db_header.super, i);  // delete range 'i'

  set_db_header_dirty (ref p);

  return 0;
}

/**********************************************************************/

public int deallocate_block (LINK block_nr, ref DB_INFO p)
{
  int      rc;
  BUFFER   *buffer;
  DB_SUPER *super;

  /* invalidate first the block's buffer if it is present in the cache */
  invalidate_buffer (ref p, block_nr);

  /* do never decrease 'p->db_header.free_space_link' */
  /* because we're inside a transaction !             */

  if (insert_range_in_super_table (    block_nr => block_nr,
                                       count    => 1,
                                   ref super    => p.db_header.super) == 0)
  {
    set_db_header_dirty (ref p);
  }
  else    /* header's super table is full : cannot insert range */
  {
    /* we shall swap out half the entries to a new super block */

    rc = open_block (out buffer,
                     ref p,
                         block_nr   => block_nr,
                         clear_data => true);
    if (rc < 0)
      return rc;

    set_dirty (ref p, buffer);

    super = (DB_SUPER *)&buffer->data;

    /* assertion: p.db_header.super.nb_ranges == MAX_FREE_BLOCK_RANGES */

    super->next_link = p.db_header.super.super_link;
    super->range[0:HALF_MAX_FREE_BLOCK_RANGES]
      = p.db_header.super.range[HALF_MAX_FREE_BLOCK_RANGES:HALF_MAX_FREE_BLOCK_RANGES];

    close_block (ref p, buffer);


    p.db_header.super.super_link = block_nr;
    clear p.db_header.super.range[HALF_MAX_FREE_BLOCK_RANGES:HALF_MAX_FREE_BLOCK_RANGES];
    
    set_db_header_dirty (ref p);
  }

  return 0;
}

/**********************************************************************/

/* this function assumes that p->db_header is valid,             */
/*   that p->db_header.in_transaction == 0, and that             */
/*   p->db_header.rollback_link != 0L                            */
/* Neither the cache nor the transaction mecanism must be used ! */
/* In case of failure, the db_header is invalid afterwards !     */
/* this function can possibly be interrupted and called again.   */

public
int deallocate_rollback_chain (ref DB_INFO p)
{
  LINK        rollback_block_nr;
  WORD        count;
  int         rc;
  DB_ROLLBACK rollback;

  if (p.db_header.in_transaction)
    return E_INTERN_16;

  while (p.db_header.rollback_link != 0)
  {
    rollback_block_nr = p.db_header.rollback_link;


    /* read rollback block */

    rc = __db_read (p.db, rollback_block_nr, out rollback);
    if (rc < 0)
      return rc;


    /* nb blocks of the rollback chunk (including rollback block and appended header) */
    count = (WORD)(rollback.nb_blocks + 1 + (uint)(rollback.next_link == 0));


    /* maybe the rollback chunk is at end-of-file */

    if (rollback_block_nr + count == p.db_header.free_space_link)
    {
      uint    nb_ranges;
      A_RANGE *r;

      /* a simple case : the rollback chunk is at end-of-file */
      p.db_header.free_space_link = rollback_block_nr;

      /* a small optimization : if last range is at EOF, delete it.        */
      /* (we're allowed to do that because we're not inside a transaction) */
      nb_ranges = count_ranges (p.db_header.super);
      if (nb_ranges > 0)
      {
        r = &p.db_header.super.range[nb_ranges-1];
        if (r->first + r->count == p.db_header.free_space_link)
        {
          p.db_header.free_space_link -= r->count;
          clear *r;
        }
      }
    }
    else   /* the chunk must be added in the super table */
    {
      if (insert_range_in_super_table (    rollback_block_nr,
                                           count,
                                       ref p.db_header.super) < 0)
      {
        DB_SUPER super;

        /* assert: p->db_header.super.nb_ranges == MAX_FREE_BLOCK_RANGES */

        /* the header's super table is full : we shall swap out half the entries */

        clear super;
        super.next_link = p.db_header.super.super_link;
        super.range[0:HALF_MAX_FREE_BLOCK_RANGES]
            = p.db_header.super.range[HALF_MAX_FREE_BLOCK_RANGES:HALF_MAX_FREE_BLOCK_RANGES];


        /* write super block to last block of rollback chunk */
        
        if (p.pbackup != null)
          backup_block (ref p.pbackup^, p.db, block_nr => rollback_block_nr + (count - 1));

        rc = __db_write (fd         => p.db,
                         block_nr   => rollback_block_nr + (count - 1),
                         buffer     => super);
        if (rc < 0)
          return rc;


        p.db_header.super.super_link = rollback_block_nr + (count - 1);
        clear p.db_header.super.range[HALF_MAX_FREE_BLOCK_RANGES:HALF_MAX_FREE_BLOCK_RANGES];

        // now insert should work (minus the last block that was used earlier)
        if (insert_range_in_super_table (    rollback_block_nr,
                                             (WORD)(count-1),
                                         ref p.db_header.super) < 0)
          return E_INTERN_15;
      }
    }

    p.db_header.rollback_link = rollback.next_link;
  }

  /* be sure all super blocks are written before updating the header */
  rc = __db_flush_file (p.db);
  if (rc < 0)
    return rc;

  rc = __db_write (p.db, 0, p.db_header);
  if (rc < 0)
    return rc;

  /* be sure the header is written before modifying any other block */
  rc = __db_flush_file (p.db);
  if (rc < 0)
    return rc;

  return 0;
}

/**********************************************************************/
#end unsafe
/**********************************************************************/

#if unit_test

void main()
{
  SUPER_TABLE super;

  clear super;

  assert insert_range_in_super_table (block_nr => 200,  count => 20, ref super) == 0;

  // insert after
  assert insert_range_in_super_table (block_nr => 300,  count => 30, ref super) == 0;

  // insert before
  assert insert_range_in_super_table (block_nr => 100,  count => 10, ref super) == 0;

  // insert middle
  assert insert_range_in_super_table (block_nr => 250,  count => 25, ref super) == 0;
  assert insert_range_in_super_table (block_nr => 350,  count => 35, ref super) == 0;

  // insert after
  assert insert_range_in_super_table (block_nr => 500,  count => 50, ref super) == 0;

  // insert before
  assert insert_range_in_super_table (block_nr => 50,  count => 5, ref super) == 0;

  assert super.range[0].first == 50;
  assert super.range[1].first == 100;
  assert super.range[2].first == 200;
  assert super.range[3].first == 250;
  assert super.range[4].first == 300;
  assert super.range[5].first == 350;
  assert super.range[6].first == 500;
  assert count_ranges(super) == 7;

  // case 2
  assert insert_range_in_super_table (block_nr => 90,  count => 10, ref super) == 0;

  assert super.range[1].first == 90;
  assert super.range[1].count == 20;
  
  // case 3
  assert insert_range_in_super_table (block_nr => 275,  count => 10, ref super) == 0;
  
  assert super.range[3].first == 250;
  assert super.range[3].count == 35;
  
  assert count_ranges(super) == 7;
  
  // case 3 with merge
  assert insert_range_in_super_table (block_nr => 285,  count => 15, ref super) == 0;

  assert super.range[0].first == 50;      assert super.range[0].count == 5;
  assert super.range[1].first == 90;      assert super.range[1].count == 20;
  assert super.range[2].first == 200;     assert super.range[2].count == 20;
  assert super.range[3].first == 250;     assert super.range[3].count == 80;
  assert super.range[4].first == 350;     assert super.range[4].count == 35;
  assert super.range[5].first == 500;     assert super.range[5].count == 50;
  assert count_ranges(super) == 6;

  if (true)
  {
    uint i;
    for (i=0; i<MAX_FREE_BLOCK_RANGES; i++)
      printf ("%u : first=%3d count=%3d\n", i, super.range[i].first, super.range[i].count);
  }
  
  printf ("ok\n");
}

#endif
