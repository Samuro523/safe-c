
// dbstruct.h : structures used in memory only

use ../db, config, fs;
use ../set, ../thread;

/**********************************************************************/

struct LOCKING_INFO
{
  int  fd;
  long offset;
  int  size;
}

/**************************************************************************/

struct TABLE_INFO
{
  char                  table_name[MAX_TABLE_NAME_LENGTH];   /* for recovery */
  LINK                  table_block_nr;
  LOCKING_INFO          locking_info;    /* shared lock on table_block_nr */
  WORD                  record_size;
  DB_INDEX_DESCRIPTION^ index[MAX_INDEXES_PER_TABLE];    /* same as in file */
}

/**********************************************************************/

const uint MAX_SPACE_BLOCKS =         512;    /* maximum cache size (in MB)  */
const int SPACE_SIZE        = (1024*1024);    /* 1 MB (size of space blocks) */

/**********************************************************************/

struct BUFFER
{
  uint time_stamp; /* time stamp of last close_block() */
  byte nb_locks;   /* nb of times this buffer was opened */
  bool dirty;      /* true = data was modified, false = data was not modified */
  bool saved_in_rollback;     /* true = already saved once in rollback, false=not */
  bool original_data_loaded;  /* true = original data was loaded, false=not */
  BYTE data[DB_BLOCK_SIZE];  /* a block of data to be accessed or modified */
  BYTE original_data[DB_BLOCK_SIZE]; /* original data of what's now in file */
}

/**********************************************************************/

#begin unsafe

struct ARROW       /* 8 bytes (or 16 bytes if 64-bit) */
{
  ARROW* child[2];  /* two pointers to 'zero' and 'one' childs */
}

struct STUB   // empty entry
{
  byte* next;
}

#end unsafe

/**********************************************************************/

struct CACHE_INFO
{
#begin unsafe
  uint    nb_space_blocks;                 /* in (0..MAX_SPACE_BLOCKS) */
  byte*   space_block[MAX_SPACE_BLOCKS];   /* ptrs to space blocks */
  byte*   begin_alloc;    /* ptr to begin of free space */
  byte*   end_alloc;      /* ptr to end of free space */

  /* free lists (at offset 0 there's a ptr to the next item) */
  ARROW*  free_arrow;     /* free list of ARROWs */
  BUFFER* free_buffer;    /* free list of buffers */

  ARROW*  tree;           /* ptr to 32- (or 64-) level binary arrow tree */
                          /* with buffers at level 33 (or 65).           */
#end unsafe

  uint    time_stamp;     /* current time stamp */

  LINK    nb_buffers;     /* total nb of buffers (free + used) */
  LINK    nb_free_buffers;/* nb of buffers on free list */
  LINK    nb_arrows;      /* total nb of arrows (free + used) */
  LINK    nb_free_arrows; /* nb of arrows on free list */
}

/**********************************************************************/

struct BACKUP_INFO
{
  SHARED_OBJECT so;   // protects backup info
  int           output_fd;
  int           error;
  SET           interval_to_backup;
  byte[]^       page0;
  byte[]^       buffers;
  bool          waiting;
}

/**********************************************************************/

struct DB_INFO
{
  int            db;              /* handle of DB file */
  int            log;             /* handle of LOGGING file, or -1 if none */

  TABLE_INFO^    table[MAX_OPEN_TABLES];     /* entries are possibly null */

  /* during (implicit or explicit) transaction : */
  LOCKING_INFO   header_lock;     /* for exclusive lock on header */
  DB_HEADER      db_header;
  LOGGING_HEADER log_header;      /* only if log != -1 */
  int            header_lock_count;  // nb of logical locks on header(0..2)
       // updated by initialize_transaction(), terminate_transaction()
       // also updated in intern_db_backup_database().

  /* true = we're between initialize_transaction() and terminate_transaction() */
  bool           transaction_open;

  /* true = db_begin_transaction() was called, false = single-command transaction */
  bool           inside_explicit_transaction;

  /* if != 0, indicates that the entire transaction will fail */
  int            transaction_error;

  /* blocks >= this link need not be saved in rollback list */
  LINK           initial_free_space_link;

  /* header saved at transaction start */
  DB_HEADER      initial_db_header;

  /* true = db_header must be rewritten */
  bool           db_header_dirty;

  /* all fields related to the transaction cache (buffers) */
#begin unsafe
  CACHE_INFO     cache;
#end unsafe

  /* limit of cache size (in MB) */
  uint           cache_size_limit;   /* 1 .. 512 (default is 32) */
  
  BACKUP_INFO^   pbackup;   // non-null if a backup is in progress
  
  string^        database_filename;
}

/**********************************************************************/

#begin unsafe
DB_INFO^ db_info[MAX_OPEN_DATABASES];   /* null means DB is closed */
#end unsafe

/**********************************************************************/

struct INDEX_ITEM
{
  BYTE  field_compression_byte[MAX_KP_INFO_SIZE];
  BYTE  compressed_key[MAX_KEY_SIZE];
  uint  compressed_key_size;

  LINK  data_record_block_nr;
  LINK  child_btree;
}

/**************************************************************************/

// returns LINK at start of byte array
LINK THE_LINK (byte[] b);

byte THE_BYTE (byte[] b);

/**************************************************************************/
