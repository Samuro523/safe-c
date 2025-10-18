
// transact.c

use ../strings, ../files;
use ../db, config, fs, dbstruct, cache, dbport, super;

/**********************************************************************/
#begin unsafe
/**********************************************************************/

/*
. thread-safe
. open db (and log file if 'open_also_logfile' set).
. check magic & version nr of db & log files.
. check that transaction nr of db and log files match.
*/

public int intern_db_open_database (string database_filename, bool open_exclusive, bool open_also_logfile)
{
  int            i, fd, rc, log;
  DB_HEADER      db_header;
  LOCKING_INFO   lock_info;
  LOGGING_HEADER log_header;

  /* find empty DB slot */
  for (i=0; i<MAX_OPEN_DATABASES; i++)
  {
    if (db_info[i] == null)    /* empty slot */
      break;
  }

  if (i >= MAX_OPEN_DATABASES)
    return E_TOO_MANY_OPEN_DB;

  /* try to open the database */
  if (open_exclusive)
    fd = __db_open_exclusive (database_filename);
  else
    fd = __db_open_share (database_filename);
  if (fd < 0)
    return fd;

  // lock header
  rc = __db_lock_exclusive (out lock_info,
                            fd,
                            0);     /* block nr */
  if (rc < 0)
  {
    (void)__db_close (fd);
    return rc;
  }

  rc = __db_read (fd, 0, out db_header);
  if (rc < 0)
  {
    (void)__db_unlock (lock_info);
    (void)__db_close (fd);
    return rc;
  }

  if (db_header.db_magic != DB_MAGIC)
  {
    (void)__db_unlock (lock_info);
    (void)__db_close (fd);
    return E_NOT_DB_FILE;
  }


  /* rollback any interrupted transaction */

  if (db_header.in_transaction)
  {
    rc = rollback_transaction (fd, ref db_header, null);
    if (rc < 0)
    {
      (void)__db_unlock (lock_info);
      (void)__db_close (fd);
      return rc;
    }
  }


  /* open the logging file and check that it matches */

  if (!open_also_logfile)             /* do not open the logging file */
  {
    log = -1;
  }
  else if (db_header.logging_filename[0] == nul)   /* no logging file */
  {
    log = -1;
  }
  else    /* there is a logging file */
  {
    if (open_exclusive)
      log = __db_open_exclusive (db_header.logging_filename);
    else
      log = __db_open_share (db_header.logging_filename);
    if (log < 0)
    {
      (void)__db_unlock (lock_info);
      (void)__db_close (fd);
      return log;
    }


    /* check that the transaction nr matches */

    rc = __db_read_log (log, out log_header);
    if (rc < 0)
    {
      (void)__db_unlock (lock_info);
      (void)__db_close (fd);
      (void)__db_close (log);
      return rc;
    }

    if (log_header.log_magic != LOG_MAGIC)
    {
      (void)__db_unlock (lock_info);
      (void)__db_close (fd);
      (void)__db_close (log);
      return E_NOT_LOG_FILE;
    }

    if (memcmp (log_header.butlast.trans, db_header.current_transaction) != 0 &&
        memcmp (log_header.last.trans, db_header.current_transaction) != 0)
    {
      (void)__db_unlock (lock_info);
      (void)__db_close (fd);
      (void)__db_close (log);
      return E_MISMATCHING_LOG_FILE;
    }
  }

  rc = __db_unlock (lock_info);
  if (rc < 0)
  {
    (void)__db_close (fd);
    (void)__db_close (log);   /* here, log can possibly equal -1 */
    return rc;
  }


  /* reserve and initialize slot */

  {
    DB_INFO^    p = new DB_INFO;
    ref DB_INFO r = p^;

    db_info[i] = p;

    r.db                          = fd;
    r.log                         = log;
    r.transaction_open            = false;
    r.inside_explicit_transaction = false;
    r.cache_size_limit            = 32;   /* default cache size : 32 MB */

    {
      char[MAX_FILENAME_LENGTH] dir, full;
      int                       len;
      get_current_directory (out dir);
      assert expand_pathname (dir, database_filename, out full) == 0;
      len = strlen(full);
      r.database_filename = new string ' (full[0:len]);
    }
  }

  return (i << 8);    /* higher 8 bits = slot nr, lower 8 bits == 0 */
}

/**********************************************************************/

public int check_db_handle (int db_handle, out DB_INFO* p)
{
  int      i;
  DB_INFO^ q;

  p = null;

  if ((db_handle & 255) != 0)           /* lower 8 bits not zero */
    return E_BAD_HANDLE;

  i = (db_handle >> 8);                 /* get db slot nr */
  if (i < 0 || i >= MAX_OPEN_DATABASES)
    return E_BAD_HANDLE;

  q = db_info[i];
  if (q == null)
    return E_BAD_HANDLE;

  p = &q^;

  return 0;
}

/**********************************************************************/

public int lock_header (ref DB_INFO p)
{
  if (p.header_lock_count == 0)   // not yet locked
  {
    int rc = __db_lock_exclusive (out p.header_lock,
                                      p.db,
                                      block_nr   => 0);
    if (rc < 0)
      return rc;
  }

  p.header_lock_count++;   // nb of times header is locked
  return 0;
}

/**********************************************************************/

public void unlock_header (ref DB_INFO p)
{
  if (p.header_lock_count == 1)   // last time locked
  {
    (void)__db_unlock (p.header_lock);
  }

  p.header_lock_count--;   // nb of times header is locked
}

/**********************************************************************/

/* initialize an implicit or explicit transaction : */

/*
. lock db header.
. read db header into variable db_header.
. rollback any interrupted transaction.
. read log header into variable log_header & initialize it.
. initialize cache.
. deallocate any rollback chain.
*/

int initialize_transaction (ref DB_INFO p)
{
  int rc;


  /* initialize global transaction error variable */

  p.transaction_error = 0;


  /* lock header */

  rc = lock_header (ref p);
  if (rc < 0)
    return rc;



  /* read db_header */

  rc = __db_read (p.db, 0, out p.db_header);
  if (rc < 0)
  {
    unlock_header (ref p);
    return rc;
  }

  if (p.db_header.db_magic != DB_MAGIC)
  {
    unlock_header (ref p);
    return E_NOT_DB_FILE;
  }


  /* rollback any interrupted transaction */

  if (p.db_header.in_transaction)
  {
    rc = rollback_transaction (p.db, ref p.db_header, p.pbackup);
    if (rc < 0)
    {
      unlock_header (ref p);
      return rc;
    }

    if (p.db_header.in_transaction)
    {
      unlock_header (ref p);
      return E_INTERN_26;
    }
  }


  /* deallocate any rollback chain (if previous committed transaction */
  /* was interrupted before the rollback chain could be deallocated)  */

  if (p.db_header.rollback_link != 0)
  {
    rc = deallocate_rollback_chain (ref p);
    if (rc < 0)
    {
      unlock_header (ref p);
      return rc;
    }
  }


  /* if there is a logging file, compute the current logging offset */

  if (p.log != -1)   /* there is a logging file */
  {
    rc = __db_seek_log (p.log, 0);
    if (rc < 0)
    {
      unlock_header (ref p);
      return rc;
    }


    rc = __db_read_log (p.log, out p.log_header);
    if (rc < 0)
    {
      unlock_header (ref p);
      return rc;
    }


    /* make sure that 'log_header.butlast' and 'log_header.last'   */
    /* contain the current transaction point.                      */
    /* 'log_header.last' can have been increased when closing this */
    /* transaction.                                                */

    if (memcmp (p.log_header.butlast.trans,  p.db_header.current_transaction) == 0)
    {
      p.log_header.last = p.log_header.butlast;    /* re-init last transaction point */
    }
    else if (memcmp (p.log_header.last.trans, p.db_header.current_transaction) == 0)
    {
      p.log_header.butlast = p.log_header.last;   /* shift 1 transaction forward */
    }
    else
    {
      unlock_header (ref p);
      return E_MISMATCHING_LOG_FILE;
    }
  }


  /* initialize 'p' fields for transaction */

  p.transaction_open        = true;
  p.initial_free_space_link = p.db_header.free_space_link;
  p.db_header_dirty         = false;
  p.initial_db_header       = p.db_header;   // for rollback


  clear p.cache;


  ENTER ();    /* enter again into critical section here */
               /* so that all other threads are locked ! */

  return 0;
}

/**********************************************************************/

/* pre-commit an implicit or explicit transaction : */

/*
. if any buffers or db_header were updated,
  . increment the current transaction nr + signature.
  . end the transaction in the logging file.
  . reset db_header.in_transaction to 0.
  . deallocate rollback chain.
*/

int precommit_transaction (ref DB_INFO p)
{
  int                 rc;
  bool                rollback_done;
  ROLLBACK_CHUNK_INFO rollback_chunk_info;


  rc = flush_cache_buffers (ref p,
                            final_flush => true,
                            out rollback_done,
                            out rollback_chunk_info);
  if (rc < 0)
    return rc;


  if (p.db_header_dirty || p.db_header.in_transaction)
  {
    /* this transaction was not purely read-only */

    rc = __db_flush_file (p.db);
    if (rc < 0)
      return rc;


    /* increment current transaction number */

    p.db_header.current_transaction.number++;
    p.db_header.current_transaction.signature += p.db_header.current_transaction.number;


    /* update logging file */

    if (p.log != -1)
    {
      rc = __db_flush_file (p.log);
      if (rc < 0)
        return rc;

      p.log_header.last.trans = p.db_header.current_transaction;

      rc = __db_seek_log (p.log, 0);
      if (rc < 0)
        return rc;

      rc = __db_write_log (p.log, p.log_header);
      if (rc < 0)
        return rc;

      rc = __db_flush_file (p.log);
      if (rc < 0)
        return rc;
    }

    p.db_header.in_transaction = false;   /* ending transaction here */

    /* we perform a little optimization here : deallocate the last */
    /* appended rollback chunk in case it is at end-of-file.       */

    if (rollback_done &&
        rollback_chunk_info.first + rollback_chunk_info.count
          == p.db_header.free_space_link)
    {
      p.db_header.rollback_link = rollback_chunk_info.next_rollback_link;
      p.db_header.free_space_link -= rollback_chunk_info.count;
    }

    rc = __db_write (p.db, 0, p.db_header);
    if (rc < 0)
      return rc;

    rc = __db_flush_file (p.db);
    if (rc < 0)
      return rc;

    if (p.db_header.rollback_link != 0)   /* non-empty rollback chain */
    {
      (void)deallocate_rollback_chain (ref p);   /* ignore any errors here */
    }
  }

  return 0;
}

/**********************************************************************/

/* pre-cancel an implicit or explicit transaction : */

/* this function can be called after an I/O or heap error, */
/* so we can't work with the cache (no deallocation !)     */

void precancel_transaction (ref DB_INFO p)
{
  if (p.db_header.in_transaction)
    (void)rollback_transaction (p.db, ref p.db_header, p.pbackup); /* ignore any errors */
}

/**********************************************************************/

/* terminate an implicit or explicit transaction : */

/*
. unlock header
. deallocate heap buffers of cache
*/

void terminate_transaction (ref DB_INFO p)
{
  /* unlock header */

  unlock_header (ref p);


  /* deallocate cache */

  deallocate_cache (ref p);


  p.transaction_open = false;

  LEAVE ();    /* leave again from critical section here ! */
}

/**********************************************************************/

void free_db_info (ref DB_INFO^ p)
{
  free p^.database_filename;
  free p;
  p = null;
}

/**********************************************************************/

public int intern_db_close_database (int db_handle)
{
  int      rc, i, j, k;
  DB_INFO* p;

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
    return rc;

  while (p->pbackup != null)      // backup busy
  {
    LEAVE ();
    sleep 0.25;
    ENTER ();
  }

  i = (db_handle >> 8);           /* get also db slot nr */

  if (p->transaction_open)
  {
    precancel_transaction (ref *p);
    terminate_transaction (ref *p);
  }

  for (j=0; j<MAX_OPEN_TABLES; j++)
  {
    if (p->table[j] != null)
    {
      (void)__db_unlock (p->table[j]^.locking_info);
      for (k=0; k<MAX_INDEXES_PER_TABLE; k++)
      {
        if (p->table[j]^.index[k] != null)
          free p->table[j]^.index[k];
      }
      free p->table[j];
      p->table[j] = null;
    }
  }

  rc = __db_close (p->db);
  if (rc < 0)
  {
    (void)__db_close (p->log);   /* handle can equal -1 */
    free_db_info (ref db_info[i]);
    return rc;
  }

  if (p->log != -1)
  {
    rc = __db_close (p->log);
    if (rc < 0)
    {
      free_db_info (ref db_info[i]);
      return rc;
    }
  }

  free_db_info (ref db_info[i]);
  return 0;
}

/**********************************************************************/

public int open_command (ref DB_INFO p)
{
  int rc;

  if (p.inside_explicit_transaction)   /* after db_begin_transaction() */
  {
    if (p.transaction_error != 0)         /* transaction is in error */
      return p.transaction_error;
  }
  else    /* single command */
  {
    rc = initialize_transaction (ref p);
    if (rc < 0)
      return rc;
  }

  return 0;
}

/**********************************************************************/

/* any logging records must be added before closing the command.    */

/* 'fatal_error_occured' must be set to 1 if the command could neither */
/* be completely done nor undone, for example due to a system error    */
/* (read error, disk full, out of memory), or if errors occured while  */
/* writing to the logging file. The transaction is then rolled back    */
/* and all subsequent commands within the transaction will fail.       */

/* there are 3 possible states :
   1) OK
   2) command returned a logical application error (duplicate index)
   3) a fatal system error occured and the db is now inconsistent
*/

public int close_command (ref DB_INFO p,
                          int         command_rc,
                          bool        fatal_error_occured)
{
  int rc;

  if (p.inside_explicit_transaction)   /* after db_begin_transaction() */
  {
    if (fatal_error_occured)   /* case 3 : fatal error */
    {
      assert (command_rc != 0);
      p.transaction_error = command_rc;   /* skip rest of transaction */
    }

    return command_rc;   /* return always return code for this command */
  }
  else    /* single command */
  {
    if (command_rc == 0)     /* OK */
    {
      rc = precommit_transaction (ref p);
      terminate_transaction (ref p);
      if (rc < 0)
        return rc;
      return 0;
    }
    else   /* logical or fatal error */
    {
      precancel_transaction (ref p);
      terminate_transaction (ref p);
      return command_rc;
    }
  }
}

/**********************************************************************/

public int intern_db_begin_transaction (int db_handle)
{
  int      rc;
  DB_INFO* p;

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
    return rc;

  if (p->inside_explicit_transaction)
    return E_NESTED_TRANS;

  rc = initialize_transaction (ref *p);
  if (rc < 0)
    return rc;

  p->inside_explicit_transaction = true;

  return 0;
}

/**********************************************************************/

public int intern_db_end_transaction (int db_handle)
{
  int      rc;
  DB_INFO* p;

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
    return rc;

  if (!p->inside_explicit_transaction)
    return E_NO_BEGIN_TRANS;

  p->inside_explicit_transaction = false;

  if (p->transaction_error == 0)
  {
    p->transaction_error = precommit_transaction (ref *p);
  }
  else   /* the entire transaction was cancelled */
  {
    precancel_transaction (ref *p);
  }

  terminate_transaction (ref *p);

  rc = p->transaction_error;

  return rc;
}

/**********************************************************************/

public int intern_db_rollback_transaction (int db_handle)
{
  int      rc;
  DB_INFO* p;

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
    return rc;

  if (!p->inside_explicit_transaction)
    return E_NO_BEGIN_TRANS;

  p->inside_explicit_transaction = false;

  precancel_transaction (ref *p);
  terminate_transaction (ref *p);

  return 0;
}

/**********************************************************************/
#end unsafe
/**********************************************************************/
