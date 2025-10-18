
// config.h

/**************************************************************************/

enum STORAGE_ORDER { STORAGE_LOW_HIGH,    /* low byte followed by high byte */
                     STORAGE_HIGH_LOW };  /* high byte followed by low byte */

/**************************************************************************/

typedef uint1 BYTE;       /* 1 byte  */
typedef uint2 WORD;       /* 2 bytes */
typedef uint4 LONG;       /* 4 bytes */
typedef int8  HUGE;       /* 8 bytes */

/* define system-dependent configuration : */

#if 1 // WINDOWS
  /* system dependant data types */
  const STORAGE_ORDER SYSTEM_STORAGE_ORDER = (STORAGE_LOW_HIGH); /* see constants above */
  typedef HUGE LINK;

  /* constants used when creating a new database */
  const WORD DB_BLOCK_SIZE_SHIFTS = 9;   // denotes a 512-byte block
  const WORD DB_BLOCK_SIZE = (WORD)(1 << DB_BLOCK_SIZE_SHIFTS);
  const LONG DB_MAGIC      = 0x7D43128A + DB_BLOCK_SIZE;   /* different per system !   */
  const LONG LOG_MAGIC     = 0x13764DE6 + DB_BLOCK_SIZE;   /* different per system !   */
#endif


#if 0 // unix RM
  /* system dependant data types */
  const STORAGE_ORDER SYSTEM_STORAGE_ORDER = (STORAGE_HIGH_LOW); /* see constants above */
  typedef HUGE LINK;

  /* constants used when creating a new database */
  const WORD DB_BLOCK_SIZE_SHIFTS = 9;   // denotes a 512-byte block
  const WORD DB_BLOCK_SIZE = (WORD)(1 << DB_BLOCK_SIZE_SHIFTS);
  const LONG DB_MAGIC      = 0x4E64387C + DB_BLOCK_SIZE;   /* different per system !   */
  const LONG LOG_MAGIC     = 0x27910E36 + DB_BLOCK_SIZE;   /* different per system !   */
#endif

/**************************************************************************/
