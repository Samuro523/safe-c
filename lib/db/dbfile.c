
// dbfile.c

use ../files, ../strings;

use ../db;
use config, fs, dbport, cache, open, transact;

//-------------------------------------------------------------------------------

LONG unique = 0;

//-------------------------------------------------------------------------------

/*

The following functions access directly the file structure without using the block cache.
To avoid header modifications being erased when a transaction completes,
we need to be sure that no transaction is currently in progress.

A transaction can be :

- warm -> flag .in_transaction is set in physical file, file is open in share mode.
- cold -> flag .in_transaction is set in physical file, rollback needed.

  The flag .in_transaction must also be tested for cold cases (when no transaction
  is currently open) because it could be an aborted transaction that requires
  a prior rollback, which will erase the header with a copy from the end of the
  rollback chain !

We cannot just test the header flag on the fly because other calls can change it too.
So:

1) be sure to enclose the call in ENTER() LEAVE(), which is done by the caller,
   so we're the only one accessing the physical file and we will wait indefinitively if not.

2) do a rollback if the flag is set.
   BUT this could rollback a transaction that is currently open ?
   We avoid this by opening the file in exclusive mode, so we're sure no transaction is open.

*/

//-------------------------------------------------------------------------------

public int intern_db_create_database (string database_filename)
{
  int       fd, i, rc;
  DB_HEADER db_header;

  fd = __db_create_exclusive (database_filename);
  if (fd < 0)
    return fd;

  clear db_header;

  db_header.db_magic = DB_MAGIC;

  db_header.free_space_link = 1;    // block 1

  db_header.current_transaction.signature = unique++;
  for (i=0; i<database_filename'length; i++)
    db_header.current_transaction.signature += (LONG)database_filename[i] << (LONG)(i & 15);

  rc = __db_write (fd         => fd,
                   block_nr   => 0,
                   buffer     => db_header);
  if (rc < 0)
  {
    (void)__db_close (fd);
    return rc;
  }

  rc = __db_extend_file (fd, db_header.free_space_link);
  if (rc < 0)
  {
    (void)__db_close (fd);
    return rc;
  }

  rc = __db_close (fd);
  if (rc < 0)
  {
    return rc;
  }

  return 0;
}

//-------------------------------------------------------------------------------

public int intern_db_set_logging_filename (string database_filename,
                                           string logging_filename,
                                           bool   create_logging_file)
{
  int            fd, log, rc;
  DB_HEADER      db_header;
  LOGGING_HEADER log_header;

  if (strlen (logging_filename) > MAX_LOGGING_FILENAME_LENGTH)
    return E_LOG_FN_TOO_LONG;

  fd = __db_open_exclusive (database_filename);
  if (fd < 0)
    return fd;

  rc = __db_read (fd, 0, out db_header);
  if (rc < 0)
  {
    (void)__db_close (fd);
    return rc;
  }

  if (db_header.db_magic != DB_MAGIC)
  {
    (void)__db_close (fd);
    return E_NOT_DB_FILE;
  }


  /* rollback any interrupted transaction */

  if (db_header.in_transaction)
  {
    rc = rollback_transaction (fd, ref db_header, null);
    if (rc < 0)
    {
      (void)__db_close (fd);
      return rc;
    }
  }


  if (create_logging_file)
  {
    log = __db_create_exclusive (logging_filename);
    if (log < 0)
    {
      (void)__db_close (fd);
      return log;
    }

    /* initialize logging file */
    clear log_header;

    log_header.log_magic    = LOG_MAGIC;

    log_header.first.trans = db_header.current_transaction;
    log_header.first.offset = log_header'size;

    log_header.butlast = log_header.first;
    log_header.last    = log_header.first;

    strncpy (out log_header.logging_filename,
             logging_filename,
             MAX_LOGGING_FILENAME_LENGTH);

    rc = __db_write_log (log, log_header);
    if (rc < 0)
    {
      (void)__db_close (fd);
      (void)__db_close (log);
      return rc;
    }

    rc = __db_close (log);
    if (rc < 0)
    {
      (void)__db_close (fd);
      return rc;
    }
  }


  /* initialize db file */

  strncpy (out db_header.logging_filename,
           logging_filename,
           MAX_LOGGING_FILENAME_LENGTH);

  rc = __db_write (fd         => fd,
                   block_nr   => 0,
                   buffer     => db_header);
  if (rc < 0)
  {
    (void)__db_close (fd);
    return rc;
  }

  rc = __db_close (fd);
  if (rc < 0)
    return rc;

  return 0;
}

//-------------------------------------------------------------------------------

public int intern_db_get_logging_filename (    string database_filename,
                                           out string(MAX_LOGGING_FILENAME_LENGTH) logging_filename)
{
  int          fd, rc;
  DB_HEADER    db_header;
  
  clear logging_filename;
  
  fd = __db_open_exclusive (database_filename);
  if (fd < 0)
    return fd;

  rc = __db_read (fd, 0, out db_header);
  if (rc < 0)
  {
    (void)__db_close (fd);
    return rc;
  }

  if (db_header.db_magic != DB_MAGIC)
  {
    (void)__db_close (fd);
    return E_NOT_DB_FILE;
  }


  /* rollback any interrupted transaction */

  if (db_header.in_transaction)
  {
    rc = rollback_transaction (fd, ref db_header, null);
    if (rc < 0)
    {
      (void)__db_close (fd);
      return rc;
    }
  }


  rc = __db_close (fd);
  if (rc < 0)
    return rc;


  strcpy (out logging_filename, db_header.logging_filename);

  return 0;
}

//-------------------------------------------------------------------------------

public int intern_db_delete_database (string database_filename)
{
  int       fd, rc;
  DB_HEADER db_header;

  fd = __db_open_exclusive (database_filename);
  if (fd < 0)
    return fd;

  rc = __db_read (    fd         => fd,
                      block_nr   => 0,
                  out buffer     => db_header);
  if (rc < 0)
  {
    (void)__db_close (fd);
    return rc;
  }

  if (db_header.db_magic != DB_MAGIC)
  {
    (void)__db_close (fd);
    return E_NOT_DB_FILE;
  }

  rc = __db_close (fd);
  if (rc < 0)
  {
    return rc;
  }

  if (db_header.logging_filename[0] != nul)
  {
    rc = __db_unlink (db_header.logging_filename);
    if (rc < 0)
      return rc;
  }

  rc = __db_unlink (database_filename);
  if (rc < 0)
    return rc;

  return 0;
}

//-------------------------------------------------------------------------------

int read_logging_data (int                log,
                       ref LOGGING_HEADER log_header,
                       out byte[]         buffer)
{
  int rc;

  if (log_header.first.offset + buffer'size > log_header.last.offset)
  {
    clear buffer;
    return E_TRUNCATED_LOGFILE;
  }

  rc = __db_seek_log (log, log_header.first.offset);
  if (rc < 0)
  {
    clear buffer;
    return rc;
  }

  rc = __db_read_log (log, out buffer);
  if (rc < 0)
    return rc;

  log_header.first.offset += buffer'size;

  return 0;
}

//-------------------------------------------------------------------------------

int read_logging_byte (int                log,
                       ref LOGGING_HEADER log_header,
                       out byte           b)
{
  return read_logging_data (log, ref log_header, out b);
}

//-------------------------------------------------------------------------------

int read_logging_word (int                log,
                       ref LOGGING_HEADER log_header,
                       out uint           w)
{
  byte c[2];
  int  rc;
  rc = read_logging_data (log, ref log_header, out c);
  w = (uint)(c[0] + (c[1] << 8));
  return rc;
}

//-------------------------------------------------------------------------------

/* starts recovery of the database using the logging file.           */
/* this requires exclusive file access.                              */
/* returns 0 if OK, or a negative error code.                        */

public int intern_db_recover (string database_filename, string logging_filename)
{
  int            log, fd, rc, db;
  LOGGING_HEADER log_header;
  DB_HEADER      db_header;

  log = __db_open_exclusive (logging_filename);
  if (log < 0)
    return log;

  rc = __db_read_log (log, out log_header);
  if (rc < 0)
  {
    (void)__db_close (log);
    return rc;
  }

  if (log_header.log_magic != LOG_MAGIC)
  {
    (void)__db_close (log);
    return E_NOT_LOG_FILE;
  }

  fd = __db_open_exclusive (database_filename);
  if (fd == files.E_FILE_NOT_FOUND)   // file not found
  {
    rc = db_create_database (database_filename);
    if (rc < 0)
    {
      (void)__db_close (log);
      return rc;
    }
    fd = __db_open_exclusive (database_filename);
  }
  if (fd < 0)
  {
    (void)__db_close (log);
    return fd;
  }

  rc = __db_read (fd, 0, out db_header);
  if (rc < 0)
  {
    (void)__db_close (fd);
    (void)__db_close (log);
    return rc;
  }

  if (db_header.db_magic != DB_MAGIC)
  {
    (void)__db_close (fd);
    (void)__db_close (log);
    return E_NOT_DB_FILE;
  }

  if (db_header.current_transaction.number != log_header.first.trans.number)
  {
    (void)__db_close (fd);
    (void)__db_close (log);
    return E_MISMATCHING_LOG_FILE;
  }

  if (log_header.first.trans.number != 0 &&
      db_header.current_transaction.signature != log_header.first.trans.signature)
  {
    (void)__db_close (fd);
    (void)__db_close (log);
    return E_MISMATCHING_LOG_FILE;
  }

  rc = __db_close (fd);
  if (rc < 0)
  {
    (void)__db_close (log);
    return rc;
  }

  db = intern_db_open_database (database_filename, open_exclusive => true, open_also_logfile => false);
  if (db < 0)
  {
    (void)__db_close (log);
    return db;
  }


  rc = db_begin_transaction (db);
  if (rc < 0)
  {
    (void)db_close_database (db);
    (void)__db_close (log);
    return rc;
  }


  /* sequentially read the logging file and execute all log records */

  while (log_header.first.offset != log_header.last.offset)
  {
    byte type;

    rc = read_logging_byte (log, ref log_header, out type);
    if (rc < 0) break;

    switch (type)
    {
      case LOGTYPE_DB_CREATE_TABLE:
        {
          BYTE             len;
          char             table_name[MAX_TABLE_NAME_LENGTH+1];
          TABLE_DEFINITION table_definition;
          uint             i;


          /* table_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear table_name;
          rc = read_logging_data (log, ref log_header, out table_name[0:len]);
          if (rc < 0) break;

          table_name[len] = nul;


          /* table_definition */

          clear table_definition;

          rc = read_logging_word (log, ref log_header, out table_definition.record_size);
          if (rc < 0) break;

          rc = read_logging_word (log, ref log_header, out table_definition.nb_fields);
          if (rc < 0) break;


          for (i=0; i<table_definition.nb_fields; i++)
          {
            rc = read_logging_word (log, ref log_header, out table_definition.field[i].offset);
            if (rc < 0) break;

            rc = read_logging_word (log, ref log_header, out table_definition.field[i].size);
            if (rc < 0) break;

            rc = read_logging_byte (log, ref log_header, out table_definition.field[i].type);
            if (rc < 0) break;

            rc = read_logging_byte (log, ref log_header, out len);
            if (rc < 0) break;

            rc = read_logging_data (log, ref log_header, out table_definition.field[i].name[0:len]);
            if (rc < 0) break;

            table_definition.field[i].name[len] = nul;
          }
          if (rc < 0) break;

          rc = db_create_table (db, table_name, table_definition);
          if (rc < 0) break;
        }
        break;

      case LOGTYPE_DB_RENAME_TABLE:
        {
          BYTE   len;
          char   old_table_name[MAX_TABLE_NAME_LENGTH + 1];
          char   new_table_name[MAX_TABLE_NAME_LENGTH + 1];


          /* old_table_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear old_table_name;
          rc = read_logging_data (log, ref log_header, out old_table_name[0:len]);
          if (rc < 0) break;

          old_table_name[len] = nul;


          /* new_table_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear new_table_name;
          rc = read_logging_data (log, ref log_header, out new_table_name[0:len]);
          if (rc < 0) break;

          new_table_name[len] = nul;

          rc = db_rename_table (db, old_table_name, new_table_name);
          if (rc < 0) break;
        }
        break;

      case LOGTYPE_DB_DELETE_TABLE:
        {
          BYTE  len;
          char  table_name[MAX_TABLE_NAME_LENGTH + 1];


          /* table_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear table_name;
          rc = read_logging_data (log, ref log_header, out table_name[0:len]);
          if (rc < 0) break;

          table_name[len] = nul;

          rc = db_delete_table (db, table_name);
          if (rc < 0) break;
        }
        break;

      case LOGTYPE_DB_CREATE_INDEX:
        {
          BYTE              len;
          char              table_name[MAX_TABLE_NAME_LENGTH + 1];
          char              index_name[MAX_INDEX_NAME_LENGTH + 1];
          INDEX_DEFINITION  index_definition;
          uint              i;


          /* table_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear table_name;
          rc = read_logging_data (log, ref log_header, out table_name[0:len]);
          if (rc < 0) break;

          table_name[len] = nul;


          /* index_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear index_name;
          rc = read_logging_data (log, ref log_header, out index_name[0:len]);
          if (rc < 0) break;

          index_name[len] = nul;


          /* index_definition */

          clear index_definition;
          rc = read_logging_byte (log, ref log_header, out index_definition.nb_parts);
          if (rc < 0) break;

          for (i=0; i<index_definition.nb_parts; i++)
          {
            rc = read_logging_byte (log, ref log_header, out len);
            if (rc < 0) break;

            rc = read_logging_data (log,
                                    ref log_header,
                                    out index_definition.part[i].name[0:len]);
            if (rc < 0) break;

            index_definition.part[i].name[len] = nul;
          }
          if (rc < 0) break;

          rc = db_create_index (db, table_name, index_name, index_definition);
          if (rc < 0) break;
        }
        break;

      case LOGTYPE_DB_RENAME_INDEX:
        {
          BYTE  len;
          char  table_name[MAX_TABLE_NAME_LENGTH + 1];
          char  old_index_name[MAX_INDEX_NAME_LENGTH + 1];
          char  new_index_name[MAX_INDEX_NAME_LENGTH + 1];


          /* table_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear table_name;
          rc = read_logging_data (log, ref log_header, out table_name[0:len]);
          if (rc < 0) break;

          table_name[len] = nul;


          /* old_index_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear old_index_name;
          rc = read_logging_data (log, ref log_header, out old_index_name[0:len]);
          if (rc < 0) break;

          old_index_name[len] = nul;


          /* new_index_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear new_index_name;
          rc = read_logging_data (log, ref log_header, out new_index_name[0:len]);
          if (rc < 0) break;

          new_index_name[len] = nul;


          rc = db_rename_index (db, table_name, old_index_name, new_index_name);
          if (rc < 0) break;
        }
        break;

      case LOGTYPE_DB_DELETE_INDEX:
        {
          BYTE  len;
          char  table_name[MAX_TABLE_NAME_LENGTH + 1];
          char  index_name[MAX_INDEX_NAME_LENGTH + 1];


          /* table_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear table_name;
          rc = read_logging_data (log, ref log_header, out table_name[0:len]);
          if (rc < 0) break;

          table_name[len] = nul;


          /* index_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear index_name;
          rc = read_logging_data (log, ref log_header, out index_name[0:len]);
          if (rc < 0) break;

          index_name[len] = nul;


          rc = db_delete_index (db, table_name, index_name);
          if (rc < 0) break;
        }
        break;

      case LOGTYPE_DB_INSERT:
        {
          BYTE     len;
          char     table_name[MAX_TABLE_NAME_LENGTH + 1];
          uint     record_size;
          byte[]^  record;
          int      table_handle;


          /* table_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear table_name;
          rc = read_logging_data (log, ref log_header, out table_name[0:len]);
          if (rc < 0) break;

          table_name[len] = nul;


          /* record */

          rc = read_logging_word (log, ref log_header, out record_size);
          if (rc < 0) break;

          record = new byte[record_size];

          rc = read_logging_data (log, ref log_header, out record^);
          if (rc < 0)
          {
            free record;
            break;
          }

          rc = intern_db_open_table (db, table_name, null);
          if (rc < 0)
          {
            free record;
            break;
          }
          table_handle = rc;

          rc = db_insert (table_handle, record^);
          if (rc < 0)
          {
            free record;
            break;
          }

          free record;

          rc = db_close_table (table_handle);
          if (rc < 0) break;
        }
        break;

      case LOGTYPE_DB_DELETE:
        {
          BYTE     len;
          char     table_name[MAX_TABLE_NAME_LENGTH + 1];
          char     index_name[MAX_INDEX_NAME_LENGTH + 1];
          uint     record_size;
          byte[]^  record;
          int      table_handle;


          /* table_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear table_name;
          rc = read_logging_data (log, ref log_header, out table_name[0:len]);
          if (rc < 0) break;

          table_name[len] = nul;


          /* index_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear index_name;
          rc = read_logging_data (log, ref log_header, out index_name[0:len]);
          if (rc < 0) break;

          index_name[len] = nul;


          /* record */

          rc = read_logging_word (log, ref log_header, out record_size);
          if (rc < 0) break;

          record = new byte[record_size];

          rc = read_logging_data (log, ref log_header, out record^);
          if (rc < 0)
          {
            free record;
            break;
          }

          rc = intern_db_open_table (db, table_name, null);
          if (rc < 0)
          {
            free record;
            break;
          }
          table_handle = rc;

          rc = db_delete (table_handle, record^, index_name);
          if (rc < 0)
          {
            free record;
            break;
          }

          free record;

          rc = db_close_table (table_handle);
          if (rc < 0) break;
        }
        break;

      case LOGTYPE_DB_UPDATE:
        {
          BYTE     len;
          char     table_name[MAX_TABLE_NAME_LENGTH + 1];
          char     index_name[MAX_INDEX_NAME_LENGTH + 1];
          uint     record_size;
          byte[]^  record;
          int      table_handle;


          /* table_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear table_name;
          rc = read_logging_data (log, ref log_header, out table_name[0:len]);
          if (rc < 0) break;

          table_name[len] = nul;


          /* index_name */

          rc = read_logging_byte (log, ref log_header, out len);
          if (rc < 0) break;

          clear index_name;
          rc = read_logging_data (log, ref log_header, out index_name[0:len]);
          if (rc < 0) break;

          index_name[len] = nul;


          /* record */

          rc = read_logging_word (log, ref log_header, out record_size);
          if (rc < 0) break;

          record = new byte[record_size];

          rc = read_logging_data (log, ref log_header, out record^);
          if (rc < 0)
          {
            free record;
            break;
          }

          rc = intern_db_open_table (db, table_name, null);
          if (rc < 0)
          {
            free record;
            break;
          }
          table_handle = rc;

          rc = db_update (table_handle, record^, index_name);
          if (rc < 0)
          {
            free record;
            break;
          }

          free record;

          rc = db_close_table (table_handle);
          if (rc < 0) break;
        }
        break;

      default:
        (void)db_close_database (db);
        (void)__db_close (log);
        return E_CORRUPT_LOGFILE;
    }


    if (rc < 0)
      break;
  }  // end while


  if (rc < 0)
  {
    (void)db_rollback_transaction (db);
    (void)db_close_database (db);
    (void)__db_close (log);
    return rc;
  }


  /* terminate transaction */

  rc = db_end_transaction (db);
  if (rc < 0)
  {
    (void)db_rollback_transaction (db);
    (void)db_close_database (db);
    (void)__db_close (log);
    return rc;
  }


  /* close all */

  rc = db_close_database (db);
  if (rc < 0)
  {
    (void)__db_close (log);
    return rc;
  }

  rc = __db_close (log);
  if (rc < 0)
    return rc;


  /* finally, copy some fields from logging to database file : */
  /* . last transaction number and signature                   */
  /* . logging filename                                        */

  fd = __db_open_exclusive (database_filename);
  if (fd < 0)
    return fd;

  rc = __db_read (fd, 0, out db_header);
  if (rc < 0)
  {
    (void)__db_close (fd);
    return rc;
  }

  db_header.current_transaction = log_header.last.trans;
  db_header.logging_filename    = log_header.logging_filename;

  rc = __db_write (fd, 0, db_header);
  if (rc < 0)
  {
    (void)__db_close (fd);
    return rc;
  }

  rc = __db_flush_file (fd);
  if (rc < 0)
  {
    (void)__db_close (fd);
    return rc;
  }

  rc = __db_close (fd);
  if (rc < 0)
    return rc;

  return 0;
}

//-------------------------------------------------------------------------------
