
// fs.h : database file structure

use ../db, config;

/****************************************************************************/
/* fields that can possibly be non-aligned must be accessed byte per byte ! */
/****************************************************************************/

packed struct CHECKSUM    // block checksum
{
  uint checksum;   // used to detect corrupted blocks
}

/****************************************************************************/

// data block

const uint SIZE_DATA_BLOCK_WITH_SUCCESSOR = DB_BLOCK_SIZE - LINK'size - CHECKSUM'size;

packed struct DB_CHAINED_DATA     // data block with successor
{
  LINK     chain_link;            // link to next block of record (or zero)
  BYTE     data[SIZE_DATA_BLOCK_WITH_SUCCESSOR];
  CHECKSUM checksum;
}

const uint SIZE_DATA_BLOCK_WITHOUT_SUCCESSOR = DB_BLOCK_SIZE - CHECKSUM'size;

packed struct DB_DATA             // data block without successor
{
  BYTE     data[SIZE_DATA_BLOCK_WITHOUT_SUCCESSOR];
  CHECKSUM checksum;
}

/**************************************************************************/

// index block

packed struct XHEADER
{
  WORD  nb_data_bytes;           // nb used data bytes
  bool  has_children;            // true = index node has children
}

const uint MAX_INDEX_DATA_SIZE = DB_BLOCK_SIZE - CHECKSUM'size - XHEADER'size;

packed struct DB_INDEX
{
  XHEADER  head;
  BYTE     data[MAX_INDEX_DATA_SIZE];
  CHECKSUM checksum;
}

packed struct HUGE_DB_INDEX         // used only in-memory, never on disk
{
  XHEADER  head;
  BYTE     data[2*MAX_INDEX_DATA_SIZE];
  CHECKSUM checksum;
}

/* format of 'data', until nb_data_bytes have been used :

  - LINK : ptr to child B-Tree having keys < key_value of first key_item.
           (present only if (.has_children)

  a sequence of key items, ordered by increasing key value :

  - BYTE    : nb of leading duplicate bytes in the key_value (see below)
              since last key_item, see key description;
              (not present for the first key_item of a sequence).

  - BYTE[n] : nb of duplicate trailing bytes and string compression bytes
              for string key parts, see key part description.

  - BYTE[key_size - nb_of_bytes_in_compression_info] : the key_value.

  - LINK : ptr to data record for key = key_value.

  - LINK : ptr to child B-Tree having keys >= key_value.
           (present only if (p->flags & NODE_HAS_CHILDREN)

design note: it's better to have 2 links, otherwise all index key values
             will be stored twice in the index tree.
*/

/**************************************************************************/

/* conversions to be applied on fields when building an index key value : */
const byte CV_ORDER        = 0x01;      /* byte order must be reversed    */
const byte CV_SIGNED       = 0x02;      /* reverse sign bit of first byte */
const byte CV_STRING       = 0x04;      /* clear bytes after initial '\0' */
const byte CV_WSTRING      = 0x08;      /* clear bytes after initial L'\0' */
const byte CV_FLOAT        = 0x10;      /* float to int representation */

/* system-dependant order flag */
const byte _CV_ORDER  = (byte)(SYSTEM_STORAGE_ORDER == STORAGE_LOW_HIGH ? CV_ORDER : 0);

/* conversions to be applied to a specific type when used in a key value :*/
const byte[MAX_DB_TYPES] field_flags = { /* int      */ (byte)(_CV_ORDER | CV_SIGNED),
                                         /* float    */ (byte)(_CV_ORDER | CV_SIGNED | CV_FLOAT),
                                         /* unsigned */ (byte)(_CV_ORDER),
                                         /* char     */ (byte)(0),
                                         /* byte     */ (byte)(0),
                                         /* string   */ (byte)(CV_STRING),
                                         /* wstring  */ (byte)(_CV_ORDER|CV_WSTRING) };

/**************************************************************************/

/* flag for key compression */
const byte K_LEADING_BYTES_COMPRESSION   = 0x01;

/* flag for key part compression */
const byte KP_STRING_COMPRESSION         = 0x01;  // ignore bytes after 0
const byte KP_TRAILING_BYTES_COMPRESSION = 0x02;
const byte KP_WSTRING_COMPRESSION        = 0x04;  // ignore words after 0

/* maximum number of bytes prefixed for key part compression            */
/* (nb of KP_xx bits used * nb key parts)                               */
/* ! we consider only 1 bit because bits 1 & 2 are mutually exclusive ! */
const uint MAX_KP_INFO_SIZE   = (1 * MAX_KEY_PARTS);

/**************************************************************************/

// index description block

packed struct A_KEY_PART  // 6 bytes
{
  WORD  offset;                /* offset in record : range 0 .. 65535   */
  WORD  size;                  /* part size : range 1 .. MAX_KEY_SIZE   */
  BYTE  cv_flags;              /* see above CV_xxx constants            */
  BYTE  kp_flags;              /* see above KP_xxx constant             */
}

packed struct DB_INDEX_DESCRIPTION   /* 230 bytes */
{
  char  index_name[MAX_INDEX_NAME_LENGTH]; /* (32)*/

  WORD  key_size;                /* range 1 .. MAX_KEY_SIZE               */
  BYTE  k_flags;                 /* see above K_xxx constant              */
  BYTE  nb_key_parts;            /* range 1 .. MAX_KEY_PARTS              */

  BYTE  kp_info_size;            /* size of field compression bytes       */
                                 /* (nb of bits set in all .kp_flags)     */
  BYTE  filler;

  A_KEY_PART key_part[MAX_KEY_PARTS];     /* 32 x 6 bytes */

  byte  filler2[DB_BLOCK_SIZE - 230 - CHECKSUM'size];

  CHECKSUM checksum;
}

/**************************************************************************/

packed struct HEADER_FIELD_DESC
{
  LINK  next_field_description_link;   /* link to next block with field   */
                                       /* description table, or 0.        */
  WORD  nb_fields;                     /* nb fields in this block.        */
}

/**************************************************************************/

/* a fixed-size field info contains fixed-size information about a field */

packed struct FINFO   /* fixed-size field info */    /* 6 bytes */
{
  WORD offset;              /* range 0 .. MAX_RECORD_SIZE      */
  WORD size;                /* range 1 .. MAX_RECORD_SIZE      */
  BYTE type;                /* see (TYPE_xxx) constants        */
  BYTE field_name_length;   /* range 1 .. MAX_FIELD_NAME_LENGTH*/
}

/**************************************************************************/

// table description block

packed struct DB_TABLE_DESCRIPTION
{
  LINK  next_table_link;         /* link to next table description */

  char  table_name[MAX_TABLE_NAME_LENGTH]; /*(32)*/

  LINK  index_description_link[MAX_INDEXES_PER_TABLE];          /* 16 x 4 */
  LINK  index_root_link[MAX_INDEXES_PER_TABLE]; /* 16 Btree root pointers */

  WORD  record_size;             /* range 1 .. MAX_RECORD_SIZE */
  byte  filler[LINK'size-2];

  LINK  locking_block;           /* link to dummy block used for locking */
       // lock all block when adding/removing an index.
       // lock 1 byte (as shared lock) when opening the table.

  /* definition of fields of the table : */

  HEADER_FIELD_DESC fields;

  BYTE  field_description[DB_BLOCK_SIZE - CHECKSUM'size
                          - LINK'size * (1 + 2*(uint)MAX_INDEXES_PER_TABLE + 2)
                          - (uint)MAX_TABLE_NAME_LENGTH
                          - HEADER_FIELD_DESC'size];
    /* nb_fields x (                                      */
    /* FINFO                    fixed_size_field_info     */
    /* BYTE[field_name_length]  field_name            )   */

  CHECKSUM checksum;
}

/**************************************************************************/

// an extended field description block contains additional definitions of fields of the table.
// no more than MAX_TABLE_FIELDS fields in total.

packed struct DB_EXTENDED_FIELD_DESCRIPTION
{
  HEADER_FIELD_DESC fields;

  BYTE  field_description[DB_BLOCK_SIZE - CHECKSUM'size - HEADER_FIELD_DESC'size];
    /* nb_fields x (                                      */
    /* FINFO                    fixed_size_field_info     */
    /* BYTE[field_name_length]  field_name            )   */
  CHECKSUM checksum;
}

/**************************************************************************/

packed struct TRANS  /* 8 bytes */
{
  LONG number;           /* transaction sequence number                */
  LONG signature;        /* a random signature stamped for this number */
}

/**************************************************************************/

// rollback block

const WORD MAX_SAVED_BLOCKS_PER_ROLLBACK_BLOCK  =  61;

packed struct DB_ROLLBACK
{
  LINK  next_link;   /* link to next rollback block, or 0.                */

  byte  filler[LINK'size-2]; /* mandatory for LINK alignment              */
  WORD  nb_blocks;   /* nb original blocks stored just after this block   */
                     /* in range 1 .. MAX_SAVED_BLOCKS_PER_ROLLBACK_BLOCK */
  LINK  target_link[MAX_SAVED_BLOCKS_PER_ROLLBACK_BLOCK];
                     /* table indicating where the blocks must be copied  */
                     /* during rollback.                                  */
  /* note: the header block is appended to the list of original blocks */
  /*       stored just after this block if .next_link equals zero.     */

  byte filler2[DB_BLOCK_SIZE - LINK'size * (2 + MAX_SAVED_BLOCKS_PER_ROLLBACK_BLOCK) - CHECKSUM'size];

  CHECKSUM checksum;
}

typedef byte[DB_BLOCK_SIZE] DB_BLOCK;

/**************************************************************************/

packed struct A_RANGE       /* 16 bytes */
{
  LINK  first;         /* link to first free block            */
  LINK  count;         /* nb of free blocks starting at first */
}

/**************************************************************************/

// super block (contains ranges of free blocks)

const WORD HALF_MAX_FREE_BLOCK_RANGES  =   12;
const WORD MAX_FREE_BLOCK_RANGES       = (WORD)(2 * HALF_MAX_FREE_BLOCK_RANGES);

packed struct SUPER_TABLE   // stored in header
{
  LINK    super_link;                   // link to first super block
  A_RANGE range[MAX_FREE_BLOCK_RANGES]; // ordered by increasing 'first' (16 bytes) x 12.
                                        // last entries are filled with zeroes.
}

/**************************************************************************/

// header block, always stored in block 0 of a db file

packed struct DB_HEADER
{
  LONG  db_magic;                /* denotes a DB file */

  byte  filler[3];
  bool  in_transaction;          /* normally 0 (1 = interrupted transact.) */

  LINK  rollback_link;           /* link to first block of rollback chain. */
                                 /* if .in_transaction, use for rollback   */
                                 /* else deallocate chain.                 */

  TRANS current_transaction;     /* number and signature                   */

  LINK  table_description_link;  /* link to first table description block  */
  LINK  free_space_link;         /* first free block at end-of-file        */

  char  logging_filename[MAX_LOGGING_FILENAME_LENGTH];  /* "" = no logging */

  SUPER_TABLE super;

  byte filler2[DB_BLOCK_SIZE - 8 - LINK'size - TRANS'size - 2*LINK'size
                - (uint)MAX_LOGGING_FILENAME_LENGTH - SUPER_TABLE'size - CHECKSUM'size];

  CHECKSUM checksum;
}

/**************************************************************************/

packed struct POINT  /* 12 bytes */
{
  TRANS trans;       /* unique transaction nr */
  LONG  offset;      /* logging file offset   */
}

/**************************************************************************/

// a logging file header, stored at the beginning of the log file

packed struct LOGGING_HEADER
{
  LONG  log_magic;    /* denotes a LOGGING file */

  POINT first;        /* save-points for before logging, last-1 and last */
  POINT butlast;
  POINT last;

  char  logging_filename[MAX_LOGGING_FILENAME_LENGTH];   /* for recovery */
}

/**************************************************************************/

/* command types for logging file (one BYTE) :                            */

const byte LOGTYPE_DB_CREATE_TABLE   = 100;  /* table_name, table_definition   */
const byte LOGTYPE_DB_RENAME_TABLE   = 101;  /* old_table_name, new_table_name */
const byte LOGTYPE_DB_DELETE_TABLE   = 102;  /* table_name                     */

const byte LOGTYPE_DB_CREATE_INDEX   = 110;  /* table_name, idx_name, idx_def. */
const byte LOGTYPE_DB_RENAME_INDEX   = 111;  /* table_name, o_idx_n, n_idx_n   */
const byte LOGTYPE_DB_DELETE_INDEX   = 112;  /* table_name, idx_name           */

const byte LOGTYPE_DB_INSERT         = 120;  /* table_name, record             */
const byte LOGTYPE_DB_DELETE         = 121;  /* table_name, idx_name, record   */
const byte LOGTYPE_DB_UPDATE         = 122;  /* table_name, idx_name, record   */

/* a table name is preceeded with a BYTE containing the string length     */
/* (between 1 and MAX_TABLE_NAME_LENGTH)                                  */

/* an index name is preceeded with a BYTE containing the string length    */
/* (between 1 and MAX_INDEX_NAME_LENGTH)                                  */

/* a table definition is stored as :                                      */
/* record size: WORD                                                      */
/* nb_fields: WORD                                                        */
/* for each field: WORD offset; WORD size; BYTE type;                     */
/*                 BYTE name_length; CHAR(n) name;                        */

/* an index definition is stored as :                                     */
/* nb_parts: BYTE                                                         */
/* for each part: BYTE name_length; CHAR(n) name;                         */

/* data records are stored with a preceding WORD containing their length  */
/* which varies from 1 up to MAX_RECORD_SIZE.                             */

/**************************************************************************/
