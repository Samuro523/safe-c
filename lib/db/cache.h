
// cache.h

use config, fs, dbstruct;

/*********************************************************************/
#begin unsafe
/*********************************************************************/

int rollback_transaction (int fd, ref DB_HEADER db_header, BACKUP_INFO^ bi);

/*********************************************************************/

int open_block (out BUFFER* pbuffer,      /* result: ptr to buffer */
                ref DB_INFO p,
                    LINK    block_nr,
                    bool    clear_data);    /* true = fill with zeroes,
                                               false = read from db file */

void set_dirty (ref DB_INFO p, BUFFER* buffer);

void close_block (ref DB_INFO p, BUFFER* buffer);

/* p.db_header was changed and must be rewritten to disk */
void set_db_header_dirty (ref DB_INFO p);

/* if block is present in cache, invalidate its data */
/* (this function is called if a block is deallocated). */
void invalidate_buffer (ref DB_INFO p,
                        LINK        block_nr);

/*********************************************************************/

/* this info is used, when committing a transaction,           */
/* to deallocate the last rollback chunk within having to move */
/* the harddisk head again to end-of-file.                     */

struct ROLLBACK_CHUNK_INFO
{
  LINK first;              /* head block of last rollback chunk */
  LONG count;              /* nb blocks within last rollback chunk */
  LINK next_rollback_link; /* ptr to head of "last-1" rollback block */
}

/*********************************************************************/

/* this function is called : */
/* 1) when no more cache memory could be allocated :              */
/*    -> we don't flush the latest-accessed buffers or those that */
/*       have locks.                                              */
/* 2) when closing the transaction (final_flush == 1) :           */
/*    -> we flush all buffers (locks are not allowed here)        */

int flush_cache_buffers
  (ref DB_INFO             p,
       bool                final_flush,
   out bool                rollback_done,
   out ROLLBACK_CHUNK_INFO rollback_info); /* (filled if rollback_done) */

/*********************************************************************/

void deallocate_cache (ref DB_INFO p);

/*********************************************************************/
#end unsafe
/*********************************************************************/
