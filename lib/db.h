
/* db.h : database file */

//--------------------------------------------------------------------------

/* library-specific limits : */

/* database/logging file */
const int  MAX_LOGGING_FILENAME_LENGTH = 76;    /* max logging filename length */
const int  MAX_OPEN_DATABASES          = 16;    /* per process                 */

/* table / fields */
const int  MAX_OPEN_TABLES          =    64;    /* per database                */
const int  MAX_TABLE_NAME_LENGTH    =    32;    /* max chars of table name     */
const int  MAX_FIELD_NAME_LENGTH    =    32;    /* max chars of field name     */
const uint MAX_TABLE_FIELDS         =    64;    /* max fields in table definit.*/
const uint MAX_RECORD_SIZE          = 65500;    /* max size of a data record   */
const uint MAX_DB_TYPES             =     7;    /* nb of available field types */

/* field types */
const byte TYPE_INT       = 0x00;
const byte TYPE_FLOAT     = 0x01;
const byte TYPE_UINT      = 0x02;
const byte TYPE_CHAR      = 0x03;
const byte TYPE_BYTE      = 0x04;
const byte TYPE_STRING    = 0x05;
const byte TYPE_WSTRING   = 0x06;

/* strings for defining db types */
const string[MAX_DB_TYPES] db_field_types = {"int", "float", "uint", "char", "byte", "string", "wstring"};

/* index */
const int  MAX_INDEX_NAME_LENGTH   =    32;    /* max chars of index name     */
const int  MAX_INDEXES_PER_TABLE   =    16;    /* max indexes for a table     */
const byte MAX_KEY_PARTS           =    32;    /* max fields in index definit.*/
const uint MAX_KEY_SIZE            =   116;    /* max size of all index fields*/

//--------------------------------------------------------------------------

/* create new database file and initialize it.                       */
/* this requires exclusive file access.                              */
/* returns 0 if OK, or a negative error code.                        */

int db_create_database (string database_filename);

//--------------------------------------------------------------------------

/* create new logging file and initialize it.                        */
/* this requires exclusive file access.                              */
/* the logging_filename :                                            */
/*  . must not exceed MAX_LOGGING_FILENAME_LENGTH characters;        */
/*  . a full pathname must be given !                                */
/*  . it will be stored in the database file.                        */
/* returns 0 if OK, or a negative error code.                        */

int db_create_logging (string database_filename,
                       string logging_filename);

//--------------------------------------------------------------------------

/* delete database file and its logging file, if any.                */
/* this requires exclusive file access.                              */
/* returns 0 if OK, or a negative error code.                        */

int db_delete_database (string database_filename);

//--------------------------------------------------------------------------

/* starts recovery of the database using the logging file.           */
/* this requires exclusive file access.                              */
/* returns 0 if OK, or a negative error code.                        */

int db_recover (string database_filename,
                string logging_filename);

//--------------------------------------------------------------------------

/* change the current logging filename.                              */
/* this requires exclusive file access.                              */
/* the logging_filename :                                            */
/*  . must not exceed MAX_LOGGING_FILENAME_LENGTH characters;        */
/*  . a full pathname must be given !                                */
/*  . an empty string means that no logging is wished.               */
/*  . it will be stored in the database file.                        */
/* returns 0 if OK, or a negative error code.                        */

int db_set_logging_filename (string database_filename,
                             string logging_filename);

//--------------------------------------------------------------------------

/* obtain the current logging filename.                              */
/* the database must not already be in transaction.                  */
/* 'logging_filename' must hold (MAX_LOGGING_FILENAME_LENGTH) bytes  */
/* returns 0 if OK, or a negative error code.                        */
/* an empty string means that logging is not used.                   */

int db_get_logging_filename (    string database_filename,
                             out string(MAX_LOGGING_FILENAME_LENGTH) logging_filename);

//--------------------------------------------------------------------------

/* open existing database.                       */
/* returns a negative error code, or a db_handle */

int db_open_database (string database_filename);

//--------------------------------------------------------------------------

/* close database.                               */
/* returns 0 if OK, or a negative error code     */

int db_close_database (int db_handle);

//--------------------------------------------------------------------------

int db_set_cache_size (int  db_handle,
                       uint size_in_MB);  /* 1..512 (default is 32) */

int db_get_cache_size (    int db_handle,
                       out uint size_in_MB);  /* 1..512 (default is 32) */

//--------------------------------------------------------------------------

/* structures for the definition of a table */

struct FIELD_DEFINITION
{
  uint  offset;                 /* between 0 and MAX_RECORD_SIZE  */
  uint  size;                   /* between 1 and MAX_RECORD_SIZE  */
  byte  type;                   /* see TYPE_xxx constants above   */
  char  name[MAX_FIELD_NAME_LENGTH];  /* field name  */
}

struct TABLE_DEFINITION
{
  uint             record_size;         /* between 1 and MAX_RECORD_SIZE  */
  uint             nb_fields;           /* between 1 and MAX_TABLE_FIELDS */
  FIELD_DEFINITION field[MAX_TABLE_FIELDS];
}

/* Fields must be given by increasing 'offset'.         */
/* Holes are allowed, but overlapping fields are not.   */
/* Field names must be unique and non-null strings.     */
/* For all names : upper and lower case are distinct,   */
/*                 spaces are not allowed;              */
/*                 only letters, digits and underscores */
/*                 are allowed. No leading digits.      */

//--------------------------------------------------------------------------

/* create a new table.                                             */
/* returns 0 if OK, or a negative error code.                      */

int db_create_table (int              db_handle,
                     string           table_name,  /*MAX_TABLE_NAME_LENGTH*/
                     TABLE_DEFINITION table_definition);

//--------------------------------------------------------------------------

/*
  Example of a table_definition string :

  "int4     serial_nr      " +
  "string25 employee_name  " +
  "char12   account        " +
  "char     client_code    " +
  "byte     valid          " +
  "byte16   status_bytes   " +
  "int4     amount         "

  Available data types : int, float, uint, char, byte, string.
*/

/* construct a straightforward table definition without holes */
int db_construct_table_definition (out TABLE_DEFINITION table_definition,
                                   string               table_definition_string);

//--------------------------------------------------------------------------

/* rename a table.                                       */
/* this function requires exclusive access to the table. */
/* returns 0 if OK, or a negative error code.            */

int db_rename_table (int    db_handle,
                     string old_table_name,
                     string new_table_name);  /* 1 .. MAX_TABLE_NAME_LENGTH */

//--------------------------------------------------------------------------

/* delete a table.                                       */
/* this function requires exclusive access to the table. */
/* returns 0 if OK, or a negative error code.            */

int db_delete_table (int    db_handle,
                     string table_name);

//--------------------------------------------------------------------------

/* structures for the definition of an index */

struct INDEX_PART_DEFINITION
{
  char name[MAX_FIELD_NAME_LENGTH];   /* field name */
}

struct INDEX_DEFINITION
{
  byte                  nb_parts;      /* between 1 and MAX_KEY_PARTS */
  INDEX_PART_DEFINITION part[MAX_KEY_PARTS];
}

/* the index field name must match with a field name of the table. */
/* the total size of all parts must not exceed MAX_KEY_SIZE bytes. */

//--------------------------------------------------------------------------

/* create an index on a table.                           */
/* this function requires exclusive access to the table. */
/* returns 0 if OK, or a negative error code.            */

int db_create_index (int              db_handle,
                     string           table_name,
                     string           index_name, /* MAX_INDEX_NAME_LENGTH */
                     INDEX_DEFINITION index_definition);

//--------------------------------------------------------------------------

/*
  Example of an index_definition string : "employee_name, serial_nr"
*/

/* construct an index definition */
int db_construct_index_definition (out INDEX_DEFINITION index_definition,
                                   string               index_definition_string);

//--------------------------------------------------------------------------

/* rename an index.                                      */
/* this function requires exclusive access to the table. */
/* returns 0 if OK, or a negative error code.            */

int db_rename_index (int    db_handle,
                     string table_name,
                     string old_index_name,
                     string new_index_name); /* 1 .. MAX_INDEX_NAME_LENGTH */

//--------------------------------------------------------------------------

/* delete an index.                                      */
/* this function requires exclusive access to the table. */
/* returns 0 if OK, or a negative error code.            */

int db_delete_index (int    db_handle,
                     string table_name,
                     string index_name);

//--------------------------------------------------------------------------

/* obtain the name of the nth table of the database.                    */
/* 'table_nr' starts at index zero.                                     */
/* 'table_name' : a buffer of at least (MAX_TABLE_NAME_LENGTH) bytes.   */
/* returns 0 if OK, an error if 'table_nr' is out of range,             */
/*         or if the db_handle is invalid.                              */

int db_query_table_name (    int  db_handle,
                             int  table_nr,
                         out string(MAX_TABLE_NAME_LENGTH) table_name);

//--------------------------------------------------------------------------

/* obtain the definition of a database table.             */
/* returns 0 if OK, an error if the table does not exist  */
/* or if the db_handle is invalid.                        */

int db_query_table_definition (    int              db_handle,
                                   string           table_name,
                               out TABLE_DEFINITION table_definition);

//--------------------------------------------------------------------------

/* obtain the name of the nth index of a table.                         */
/* 'index_nr' starts at index zero.                                     */
/* 'table_name' : a buffer of at least (MAX_INDEX_NAME_LENGTH) bytes.   */
/* returns 0 if OK, E_INDEX_RANGE if 'table_nr' is out of range,        */
/* or if the table does not exist, or if the db_handle is invalid.      */

int db_query_index_name (    int    db_handle,
                             string table_name,
                             int    index_nr,
                         out string(MAX_INDEX_NAME_LENGTH) index_name);

//--------------------------------------------------------------------------

/* obtain the definition of an index.                                 */
/* returns 0 if OK, an error if the table does not exist,             */
/* if the index does not exist, or if the db_handle is invalid.       */

int db_query_index_definition (    int              db_handle,
                                   string           table_name,
                                   string           index_name,
                               out INDEX_DEFINITION index_definition);

//--------------------------------------------------------------------------

/* open a table.                                                      */
/* the function checks if the table definition matches (this avoids   */
/* errors due to non-recompiled programs).                            */
/* returns a negative error code, or a table_handle.                  */

int db_open_table (int              db_handle,
                   string           table_name,
                   TABLE_DEFINITION table_definition);

//--------------------------------------------------------------------------

/* close a table.                             */
/* returns 0 if OK, or a negative error code. */

int db_close_table (int table_handle);

//--------------------------------------------------------------------------

/* absolute retrieve modes (do not use the record value) */
const uint2 DB_FIRST            = 0;
const uint2 DB_LAST             = 5;

/* relative retrieve modes (use the record value) */
const uint2 DB_EQUAL            = 2;
const uint2 DB_SMALLER          = 1;
const uint2 DB_LARGER           = 4;
const uint2 DB_EQUAL_OR_SMALLER = 3;
const uint2 DB_EQUAL_OR_LARGER  = 6;

int db_insert   (int table_handle, byte[] record);
int db_delete   (int table_handle, byte[] record, string index_name);
int db_update   (int table_handle, byte[] record, string index_name);
int db_retrieve (int table_handle, ref byte[] record, uint2 retrieve_mode, string index_name);

//--------------------------------------------------------------------------

int db_begin_transaction    (int db_handle);
int db_end_transaction      (int db_handle);
int db_rollback_transaction (int db_handle);

/* returns nb of threads waiting at a db_command, or inside a transaction */
/* minimum 1 inside a transaction */
int db_nb_threads_waiting ();

//--------------------------------------------------------------------------

// obtain number of transactions executed so far that changed the database,
// if there are too many it might be time to do a defragment.
int db_get_write_transaction_count (int db_handle, out long count);

//--------------------------------------------------------------------------

/* defragment DB file (this can take a very long time) */
int db_defragment (string filename);

//--------------------------------------------------------------------------

// returns a negative value if error.
long db_get_database_approximate_size (int db_handle);

//--------------------------------------------------------------------------

typedef void USER_CALLBACK (int db_handle, float percent_done);

// take a backup of the database file while it is open and active.
// the function should be called by another thread of the process that has already opened the database.
// this function can return a locking error if another db_handle is busy with a transaction or a backup,
// so you should retry.
// for other db_handles, the file appear locked during the duration of the backup.
int db_backup_database (int db_handle, string output_filename, USER_CALLBACK user_callback = null);

//--------------------------------------------------------------------------

const int E_NOENT    =   -2;    /* no such file or directory */
const int E_NOSPC    =  -28;     /* no space left on device, disk is full */

//--------------------------------------------------------------------------

/* the following error codes are specific to this library : */

/* logging filename specified as argument is too long */
const int E_LOG_FN_TOO_LONG                   =  -200;

/* file has been truncated and is consequently unusable */
const int E_FILE_UNUSABLE                     =  -201;

/* too many db_open_database () calls */
const int E_TOO_MANY_OPEN_DB                  =  -202;

/* db file has bad magic number or version number */
const int E_NOT_DB_FILE                       =  -203;

/* logging file has bad magic number or version number */
const int E_NOT_LOG_FILE                      =  -204;

/* transaction nr of logging file and db file do not match */
const int E_MISMATCHING_LOG_FILE              =  -205;

/* out of range handle value, or database/table is not open */
const int E_BAD_HANDLE                        =  -206;

/* intern error - locks were not released - should never happen */
const int E_INTERN_1                          =  -207;

/* previous transaction was not closed or rolled back */
const int E_NESTED_TRANS                      =  -208;

/* db_begin_transaction() was not called previously */
const int E_NO_BEGIN_TRANS                    =  -209;

/* bad table/index/field name (too long, contains spaces, illegal chars) */
const int E_BAD_NAME                          =  -210;

/* table or index definition contains too few or too many fields */
const int E_BAD_NB_FIELDS                     =  -211;

/* table definition has bad record size (too small or too large) */
const int E_BAD_RECORD_SIZE                   =  -212;

/* table definition field has bad offset */
const int E_BAD_OFFSET                        =  -213;

/* table definition field has bad size */
const int E_BAD_SIZE                          =  -214;

/* table definition field has bad type / definition string has bad type name */
const int E_BAD_TYPE                          =  -215;

/* table/index/field name is duplicate (already exists) */
const int E_DUPLICATE_NAME                    =  -216;

/* syntax error in definition string */
const int E_SYNTAX_ERROR                      =  -217;

/* table name was not found */
const int E_TABLE_NOT_FOUND                   =  -218;

/* index definition has bad number of key parts */
const int E_BAD_NB_KEY_PARTS                  =  -219;

/* total size of all key parts is too large */
const int E_KEY_SIZE                          =  -220;

/* field name was not found */
const int E_FIELD_NOT_FOUND                   =  -221;

/* too many indexes on this table */
const int E_TOO_MANY_INDEXES                  =  -222;

/* index name was not found */
const int E_INDEX_NOT_FOUND                   =  -223;

/* last remaining index cannot be deleted if the table contains data */
const int E_LAST_INDEX                        =  -224;

/* index is out of range */
const int E_INDEX_RANGE                       =  -225;

/* intern error - index and table data are inconsistent - should never happen */
const int E_INTERN_2                          =  -226;

/* db_open_table () : table definition does not match */
const int E_MISMATCH                          =  -227;

/* too many open tables */
const int E_MAX_OPEN_TABLES                   =  -228;

/* mismatching record_size parameter */
const int E_RECORD_SIZE_MISMATCH              =  -229;

/* at least one index must be created before inserting records */
const int E_NO_INDEX                          =  -230;

/* insert / update / create_index would create duplicate key value */
const int E_DUPLICATE_KEY                     =  -231;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_3                          =  -232;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_4                          =  -233;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_5                          =  -234;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_6                          =  -235;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_7                          =  -236;

/* block has incorrect checksum - file was damaged by external process */
const int E_CORRUPTED_BLOCK                   =  -237;

/* delete/update/retrieve : record was not found (no record with such key) */
const int E_KEY_NOT_FOUND                     =  -238;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_8                          =  -239;

/* parameter 'retrieve_mode' in db_retrieve() has out of range value */
const int E_BAD_MODE                          =  -240;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_9                          =  -241;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_10                         =  -242;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_11                         =  -243;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_12                         =  -244;

/* logging file becomes too large */
const int E_LOGGING_OVERFLOW                  =  -245;

/* logging file has been truncated (recovery is incomplete) */
const int E_TRUNCATED_LOGFILE                 =  -246;

/* logging file is corrupt (recovery is incomplete) */
const int E_CORRUPT_LOGFILE                   =  -247;

/* intern error - inconsistent super data - should never happen */
const int E_INTERN_13                         =  -248;

/* intern error - inconsistent super data - should never happen */
const int E_INTERN_14                         =  -249;

/* intern error - inconsistent super data - should never happen */
const int E_INTERN_15                         =  -250;

/* intern error - inconsistent transaction state - should never happen */
const int E_INTERN_16                         =  -251;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_17                         =  -252;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_18                         =  -253;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_19                         =  -254;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_20                         =  -255;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_21                         =  -256;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_22                         =  -257;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_23                         =  -258;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_24                         =  -259;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_25                         =  -260;

/* intern error - inconsistent index data - should never happen */
const int E_INTERN_26                         =  -261;

/* intern error - bad block_nr - should never happen */
const int E_INTERN_27                         =  -262;

/* intern error - corrupt table data - should never happen */
const int E_INTERN_28                         =  -263;

//--------------------------------------------------------------------------
