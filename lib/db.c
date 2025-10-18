
// db.c

use db/dbfile;
use db/dbstruct, db/dbport, db/transact;
use db/table, db/index, db/open, db/query, db/cmd, db/defrag, db/backup;

/**********************************************************************/

public int db_create_database (string database_filename)
{
  int rc;
  
  ENTER ();
  rc = intern_db_create_database (database_filename);
  LEAVE ();
  
  return rc;
}

/**********************************************************************/

public int db_create_logging (string database_filename,
                              string logging_filename)
{
  int rc;

  ENTER ();
  rc = intern_db_set_logging_filename (database_filename,
                                       logging_filename,
                                       create_logging_file => true);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_delete_database (string database_filename)
{
  int rc;

  ENTER ();
  rc = intern_db_delete_database (database_filename);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_recover (string database_filename,
                       string logging_filename)
{
  int rc;
  
  ENTER ();
  rc = intern_db_recover (database_filename, logging_filename);
  LEAVE ();
  
  return rc;
}

/**********************************************************************/

public int db_set_logging_filename (string database_filename,
                                    string logging_filename)
{
  int rc;

  ENTER ();
  rc = intern_db_set_logging_filename (database_filename,
                                       logging_filename,
                                       create_logging_file => false);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_get_logging_filename (string database_filename,
                                    out string(MAX_LOGGING_FILENAME_LENGTH) logging_filename)
{
  int rc;

  ENTER ();
  rc = intern_db_get_logging_filename (database_filename, out logging_filename);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_open_database (string database_filename)
{
  int rc;

  ENTER ();
  rc = intern_db_open_database (database_filename, open_exclusive => false, open_also_logfile => true);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_close_database (int db_handle)
{
  int rc;

  ENTER ();
  rc = intern_db_close_database (db_handle);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_begin_transaction (int db_handle)
{
  int rc;

  ENTER ();
  rc = intern_db_begin_transaction (db_handle);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_end_transaction (int db_handle)
{
  int rc;

  ENTER ();
  rc = intern_db_end_transaction (db_handle);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_rollback_transaction (int db_handle)
{
  int rc;

  ENTER ();
  rc = intern_db_rollback_transaction (db_handle);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_set_cache_size (int  db_handle,
                              uint size_in_MB)  /* 1..512 (default is 32) */
{
#begin unsafe
  int      rc;
  DB_INFO* p;
  uint     size = size_in_MB;

  ENTER ();

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
  {
    LEAVE ();
    return rc;
  }

  if (size < 1)
    size = 1;
  if (size > MAX_SPACE_BLOCKS)
    size = MAX_SPACE_BLOCKS;

  p->cache_size_limit = size;

  LEAVE ();
  return 0;
#end unsafe
}

/**********************************************************************/

public int db_get_cache_size (int db_handle,
                              out uint size_in_MB)  /* 1..512 (default is 32) */
{
#begin unsafe
  int      rc;
  DB_INFO* p;

  size_in_MB = 0;

  ENTER ();

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
  {
    LEAVE ();
    return rc;
  }

  size_in_MB = p->cache_size_limit;

  LEAVE ();
  return 0;
#end unsafe
}

/**********************************************************************/

public int db_create_table (int              db_handle,
                            string           table_name,  /*MAX_TABLE_NAME_LENGTH*/
                            TABLE_DEFINITION table_definition)
{
  int rc;

  ENTER ();
  rc = intern_db_create_table (db_handle, table_name, table_definition);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_construct_table_definition (out TABLE_DEFINITION table_definition,
                                          string               table_definition_string)
{
  return intern_db_construct_table_definition (out table_definition, table_definition_string);
}

/**********************************************************************/

public int db_construct_index_definition (out INDEX_DEFINITION index_definition,
                                          string               index_definition_string)
{
  return intern_db_construct_index_definition (out index_definition, index_definition_string);
}

/**********************************************************************/

public int db_rename_table (int    db_handle,
                            string old_table_name,
                            string new_table_name)  /* 1 .. MAX_TABLE_NAME_LENGTH */
{
  int rc;

  ENTER ();
  rc = intern_db_rename_table (db_handle, old_table_name, new_table_name);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_delete_table (int    db_handle,
                            string table_name)
{
  int rc;

  ENTER ();
  rc = intern_db_delete_table (db_handle, table_name);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_create_index (int              db_handle,
                            string           table_name,
                            string           index_name, /* MAX_INDEX_NAME_LENGTH */
                            INDEX_DEFINITION index_definition)
{
  int rc;

  ENTER ();
  rc = intern_db_create_index (db_handle, table_name, index_name, index_definition);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_rename_index (int    db_handle,
                            string table_name,
                            string old_index_name,
                            string new_index_name)  /* 1 .. MAX_INDEX_NAME_LENGTH */
{
  int rc;

  ENTER ();
  rc = intern_db_rename_index (db_handle, table_name, old_index_name, new_index_name);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_delete_index (int    db_handle,
                            string table_name,
                            string index_name)
{
  int rc;

  ENTER ();
  rc = intern_db_delete_index (db_handle, table_name, index_name);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_query_table_name (int  db_handle,
                                int  table_nr,
                                out string(MAX_TABLE_NAME_LENGTH) table_name)
{
  int rc;

  ENTER ();
  rc = intern_db_query_table_name (db_handle, table_nr, out table_name);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_query_table_definition (int                  db_handle,
                                      string               table_name,
                                      out TABLE_DEFINITION table_definition)
{
  int rc;

  ENTER ();
  rc = intern_db_query_table_definition (db_handle, table_name, out table_definition);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_query_index_name (int    db_handle,
                                string table_name,
                                int    index_nr,
                                out string(MAX_INDEX_NAME_LENGTH) index_name)
{
  int rc;

  ENTER ();
  rc = intern_db_query_index_name (db_handle, table_name, index_nr, out index_name);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_query_index_definition (int                  db_handle,
                                      string               table_name,
                                      string               index_name,
                                      out INDEX_DEFINITION index_definition)
{
  int rc;

  ENTER ();
  rc = intern_db_query_index_definition (db_handle, table_name, index_name, out index_definition);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_open_table (int              db_handle,
                          string           table_name,
                          TABLE_DEFINITION table_definition)
{
  int rc;

  ENTER ();
  #begin unsafe
  rc = intern_db_open_table (db_handle, table_name, &table_definition);
  #end unsafe  
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_close_table (int table_handle)
{
  int rc;

  ENTER ();
  rc = intern_db_close_table (table_handle);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_insert (int table_handle, byte[] record)
{
  int rc;

  ENTER ();
  rc = intern_db_insert (table_handle, record);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_delete (int table_handle, byte[] record, string index_name)
{
  int rc;

  ENTER ();
  rc = intern_db_delete (table_handle, record, index_name);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_update (int table_handle, byte[] record, string index_name)
{
  int rc;

  ENTER ();
  rc = intern_db_update (table_handle, record, index_name);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_retrieve (int table_handle, ref byte[] record, uint2 retrieve_mode, string index_name)
{
  int rc;

  ENTER ();
  rc = intern_db_retrieve (table_handle, ref record, retrieve_mode, index_name);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_get_write_transaction_count (int db_handle, out long count)
{
  int rc;
  
  ENTER ();
  rc = intern_db_get_write_transaction_count (db_handle, out count);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_defragment (string filename)
{
  int rc;
  
  ENTER ();
  rc = intern_db_defragment (filename);
  LEAVE ();

  return rc;
}

/**********************************************************************/

public int db_backup_database (int db_handle, string output_filename, USER_CALLBACK user_callback = null)
{
  return backup.intern_db_backup_database (db_handle, output_filename, user_callback);
}

/**********************************************************************/

public long db_get_database_approximate_size (int db_handle)
{
  return backup.db_get_database_approximate_size (db_handle);
}

/**********************************************************************/

/* returns nb of threads waiting at a db_command, or inside a transaction */
/* minimum 1 inside a transaction */

public int db_nb_threads_waiting ()
{
  return dbport . __g_nb_threads_waiting;
}

/**********************************************************************/
