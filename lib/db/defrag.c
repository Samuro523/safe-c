
// defrag.c : defragment database file

use ../files, ../tracing, ../strings;
use ../db, transact;

/*************************************************************************/

int copy_db_objects (int db1, int db2)
{
  int              table_id, index_id, rc, rc2;
  int              table_handle1, table_handle2;
  char             table_name[MAX_TABLE_NAME_LENGTH];
  char             index_name[MAX_INDEX_NAME_LENGTH];
  char             first_index_name[MAX_INDEX_NAME_LENGTH];
  char             record[65536];
  uint             count;
  TABLE_DEFINITION table_definition;
  INDEX_DEFINITION index_definition;

  for (table_id=0; ; table_id++)
  {
    rc = db_query_table_name (db1, table_id, out table_name);
    if (rc == E_INDEX_RANGE)
      break;
    if (rc < 0)
    {
      trace ("error: copy_db_objects() : db_query_table_name() returned %d\n", rc);
      return -1;
    }

    trace ("info: TABLE %s\n", table_name);

    rc = db_query_table_definition (db1, table_name, out table_definition);
    if (rc < 0)
    {
      trace ("error: copy_db_objects() : db_query_table_definition() returned %d\n", rc);
      return -1;
    }

    rc = db_create_table (db2, table_name, table_definition);
    if (rc < 0)
    {
      trace ("error: copy_db_objects() : db_create_table() returned %d\n", rc);
      return -1;
    }


    /* create first index */

    rc = db_query_index_name (db1, table_name, 0, out index_name);
    if (rc == E_INDEX_RANGE)
    {
      trace ("warning: copy_db_objects() : no index 0\n");
    }
    else if (rc < 0)
    {
      trace ("error: copy_db_objects() : db_query_index_name(0) returned %d\n", rc);
      return -1;
    }
    else  /* OK */
    {
      strcpy (out first_index_name, index_name);

      rc = db_query_index_definition (db1, table_name, index_name, out index_definition);
      if (rc < 0)
      {
        trace ("error: copy_db_objects() : db_query_index_definition(0) returned %d\n", rc);
        return -1;
      }

      rc = db_create_index (db2, table_name, index_name, index_definition);
      if (rc < 0)
      {
        trace ("error: copy_db_objects() : db_create_index(0) returned %d\n", rc);
        return -1;
      }

      table_handle1 = db_open_table (db1, table_name, table_definition);
      if (table_handle1 < 0)
      {
        trace ("error: copy_db_objects() : db_open_table(1) returned %d\n", rc);
        return -1;
      }

      table_handle2 = db_open_table (db2, table_name, table_definition);
      if (table_handle2 < 0)
      {
        db_close_table (table_handle1);
        trace ("error: copy_db_objects() : db_open_table(2) returned %d\n", rc);
        return -1;
      }

      count = 0;

      clear record;

      rc = db_retrieve (table_handle1, ref record[0:table_definition.record_size], DB_FIRST, first_index_name);

      while (rc == 0)
      {
        rc2 = db_insert (table_handle2, record[0:table_definition.record_size]);
        if (rc2 < 0)
        {
          db_close_table (table_handle1);
          db_close_table (table_handle2);
          trace ("error: copy_db_objects() : db_insert() returned %d\n", rc2);
          return -1;
        }

        count++;

        rc = db_retrieve (table_handle1, ref record[0:table_definition.record_size], DB_LARGER, first_index_name);
      }

      if (rc != E_KEY_NOT_FOUND)
      {
        db_close_table (table_handle1);
        db_close_table (table_handle2);
        trace ("error: copy_db_objects() : db_retrieve() returned %d\n", rc);
        return -1;
      }

      rc = db_close_table (table_handle1);
      if (rc < 0)
      {
        db_close_table (table_handle2);
        trace ("error: copy_db_objects() : db_close_table(1) returned %d\n", rc);
        return -1;
      }

      rc = db_close_table (table_handle2);
      if (rc < 0)
      {
        trace ("error: copy_db_objects() : db_close_table(2) returned %d\n", rc);
        return -1;
      }


      /* create remaining indexes (starting at index 1) */

      for (index_id=1; ; index_id++)
      {
        rc = db_query_index_name (db1, table_name, index_id, out index_name);
        if (rc == E_INDEX_RANGE)
          break;
        if (rc < 0)
        {
          trace ("error: copy_db_objects() : db_query_index_name() returned %d\n", rc);
          return -1;
        }

        rc = db_query_index_definition (db1, table_name, index_name, out index_definition);
        if (rc < 0)
        {
          trace ("error: copy_db_objects() : db_query_index_definition() returned %d\n", rc);
          return -1;
        }

        rc = db_create_index (db2, table_name, index_name, index_definition);
        if (rc < 0)
        {
          trace ("error: copy_db_objects() : db_create_index() returned %d\n", rc);
          return -1;
        }
      }

      trace ("info: %u records copied\n", count);
    }
  }

  return 0;
}

/*************************************************************************/

int do_copy (int db1, int db2)
{
  int rc;

  if (db_set_cache_size (db1, 512) < 0 ||
      db_set_cache_size (db2, 512) < 0)
  {
    trace ("error: db_set_cache_size() failed\n");
    return -1;
  }

  rc = db_begin_transaction (db1);
  if (rc < 0)
  {
    trace ("error: db_defragment() : db_begin_transaction(1) returned %d\n", rc);
    return -1;
  }

  rc = db_begin_transaction (db2);
  if (rc < 0)
  {
    trace ("error: db_defragment() : db_begin_transaction(2) returned %d\n", rc);
    return -1;
  }

  rc = copy_db_objects (db1, db2);
  if (rc < 0)
  {
    trace ("error: db_defragment() : copy_db_objects() failed\n");
    return -1;
  }

  rc = db_end_transaction (db1);
  if (rc < 0)
  {
    trace ("error: db_defragment() : db_end_transaction(1) returned %d\n", rc);
    return -1;
  }

  rc = db_end_transaction (db2);
  if (rc < 0)
  {
    trace ("error: db_defragment() : db_end_transaction(2) returned %d\n", rc);
    return -1;
  }

  return 0;
}

/*************************************************************************/

public
int intern_db_defragment (string filename)
{
  string^ target_filename;
  int     db1, db2, rc;

  trace ("\n");
  trace ("info: begin db_defragment('%s')\n", filename);

  target_filename = new string (filename ' length + 10);
  sprintf (out target_filename^, "%s%s", filename, ".DFRG");

  db1 = intern_db_open_database (filename, open_exclusive => true, open_also_logfile => false);
  if (db1 < 0)
  {
    trace ("error: db_defragment() : intern_db_open_database(%s) returned %d\n", filename, db1);
    free target_filename;
    return -1;
  }

  rc = db_create_database (target_filename^);
  if (rc < 0)
  {
    db_close_database (db1);
    trace ("error: db_defragment() : db_create_database(%s) returned %d\n", target_filename^, rc);
    free target_filename;
    return -1;
  }

  db2 = db_open_database (target_filename^);
  if (db2 < 0)
  {
    db_close_database (db1);
    delete_file (target_filename^);
    trace ("error: db_defragment() : db_open_database(%s) returned %d\n", target_filename^, db2);
    free target_filename;
    return -1;
  }

  if (do_copy (db1, db2) < 0)
  {
    db_close_database (db1);
    db_close_database (db2);
    delete_file (target_filename^);
    free target_filename;
    return -1;
  }

  rc = db_close_database (db1);
  if (rc < 0)
  {
    db_close_database (db2);
    delete_file (target_filename^);
    trace ("error: db_defragment() : db_close_transaction(1) returned %d\n", rc);
    free target_filename;
    return -1;
  }

  rc = db_close_database (db2);
  if (rc < 0)
  {
    delete_file (target_filename^);
    trace ("error: db_defragment() : db_close_transaction(2) returned %d\n", rc);
    free target_filename;
    return -1;
  }

  /* now, we must replace source by target database */

  if (db_delete_database (filename) < 0)
  {
    delete_file (target_filename^);
    trace ("error: db_defragment() : could not delete source database\n");
    free target_filename;
    return -1;
  }

  for (;;)
  {
    rc = files.move_file (target_filename^, filename);
    if (rc == 0)
      break;

    trace ("error: db_defragment() : could not rename(%s,%s) (error %d)\n", target_filename^, filename, rc);

    sleep 60;
  }

  free target_filename;

  trace ("info: end db_defragment(%s) : ok\n", filename);
  trace ("\n");

  return 0;
}

/*************************************************************************/
