
// backup.c

use ../set, ../db, ../files, ../thread;
use config, dbstruct, fs, transact, dbport;

/**********************************************************************/
#begin unsafe
/**********************************************************************/

void free_backup_info (ref BACKUP_INFO^ bi)
{
  destroy_shared_object (ref bi^.so);

  if (bi^.output_fd != -1)
    __db_close (bi^.output_fd);

  SET_close (ref bi^.interval_to_backup);

  free bi^.page0;
  free bi^.buffers;

  clear bi^;
  free bi;
  bi = null;
}

//---------------------------------------------------------------------------------------------------

long db_get_database_approximate_nb_blocks (int db_handle)
{
  int        rc;
  DB_INFO*   p;
  DB_HEADER  db_header;

  ENTER ();

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
  {
    LEAVE ();
    return rc;
  }

  if (p->inside_explicit_transaction ||  // not allowed inside a transaction
      p->pbackup != null)                // backup already busy
  {
    LEAVE ();
    return E_NESTED_TRANS;
  }


  /* lock header */

  rc = lock_header (ref *p);
  if (rc < 0)
  {
    LEAVE ();
    return rc;
  }


  rc = __db_read (p->db, 0, out db_header);
  if (rc < 0)
  {
    unlock_header (ref *p);
    LEAVE ();
    return rc;
  }

  /* unlock header */

  unlock_header (ref *p);


  LEAVE ();


  return db_header.free_space_link;
}

//---------------------------------------------------------------------------------------------------

public long db_get_database_approximate_size (int db_handle)
{
  long nb_blocks = db_get_database_approximate_nb_blocks (db_handle);

  if (nb_blocks < 0)
    return nb_blocks;

  return nb_blocks * DB_BLOCK_SIZE;
}

//---------------------------------------------------------------------------------------------------

// this should run on another thread than the database queries, as it takes long.

public int intern_db_backup_database (int db_handle, string output_filename, USER_CALLBACK user_callback = null)
{
  int          fd_out;
  int          rc;
  DB_INFO*     p;
  long         nb_blocks;
  BACKUP_INFO^ bi;
  bool         done;
  float        factor;

  nb_blocks = db_get_database_approximate_nb_blocks (db_handle);
  if (nb_blocks < 0)
    return (int)nb_blocks;

  fd_out = __db_create_exclusive (output_filename);
  if (fd_out < 0)
    return fd_out;

  // this can take long, especially on slow backup device
  // mandatory to avoid long delays in transactions below.
  rc = __db_extend_file (fd_out, nb_blocks);
  if (rc < 0)
  {
    __db_close (fd_out);
    delete_file (output_filename);
    return rc;
  }


  ENTER ();   // there is no other thread with a transaction in progress.

  rc = check_db_handle (db_handle, out p);
  if (rc < 0)
  {
    __db_close (fd_out);
    delete_file (output_filename);
    LEAVE ();
    return rc;
  }

  if (p->inside_explicit_transaction ||  // not allowed inside a transaction
      p->pbackup != null)                // backup already busy
  {
    __db_close (fd_out);
    delete_file (output_filename);
    LEAVE ();
    return E_NESTED_TRANS;
  }


  // lock header (there is no other process with a transaction in progress)

  rc = lock_header (ref *p);
  if (rc < 0)
  {
    __db_close (fd_out);
    delete_file (output_filename);
    LEAVE ();
    return rc;
  }


  // prepare backup info

  bi = new BACKUP_INFO;
  bi^.output_fd = fd_out;
  SET_create (out bi^.interval_to_backup);
  bi^.page0   = new byte[DB_BLOCK_SIZE];
  bi^.buffers = new byte[DB_BLOCK_SIZE];

  rc = __db_read (p->db, 0, out bi^.page0^);
  if (rc < 0)
  {
    free_backup_info (ref bi);
    unlock_header (ref *p);
    delete_file (output_filename);
    LEAVE ();
    return rc;
  }

  p->db_header'byte = bi^.page0^;
  nb_blocks = p->db_header.free_space_link;
  SET_insert_interval (ref bi^.interval_to_backup, first => 1, last => nb_blocks-1);

/*
  rc = __db_extend_file (fd_out, nb_blocks);
  if (rc < 0)
  {
    free_backup_info (ref bi);
    LEAVE ();
    return rc;
  }
*/

  p->pbackup = bi;

  LEAVE ();   // from this point, other threads can do transactions, 
              // but not other processes since header was locked.



  factor = 100.0 / (float)nb_blocks;

  done = false;
  while (!done && bi^.error == 0)
  {
    long low, high, ofs;

    enter_shared_object (ref bi^.so);

    if (SET_search_any_interval (bi^.interval_to_backup, low => 0, out low, out high))
    {
      ofs = low;

      for (;;)
      {
        // backup [ofs]
        rc = __db_read (p->db, ofs, out bi^.buffers^);
        if (rc < 0)
        {
          bi^.error = rc;
          break;
        }
        else
        {
          rc = __db_write (fd_out, ofs, bi^.buffers^);
          if (rc < 0)
          {
            bi^.error = rc;
            break;
          }
        }

        if (bi^.waiting)
          break;

        if (ofs == high)
          break;

        ofs++;
      }

      assert SET_delete_interval (ref bi^.interval_to_backup, low, ofs);
    }
    else
    {
      done = true;
      low = nb_blocks;
    }

    leave_shared_object (ref bi^.so);

    if (bi^.waiting)    // some transaction is waiting to write blocks,
      sleep 0.01;       // give it some undisturbed time slice

    if (user_callback != null)
      user_callback (db_handle, percent_done => factor * (float)low);
  }



  ENTER ();
  p->pbackup = null;   // must be protected in critical section, we cannot free this suddenly during a transaction write
  unlock_header (ref *p);
  LEAVE ();


  if (bi^.error < 0)
  {
    rc = bi^.error;
    free_backup_info (ref bi);
    delete_file (output_filename);
    return rc;
  }

  rc = __db_write (fd_out, 0, bi^.page0^);
  if (rc < 0)
  {
    free_backup_info (ref bi);
    delete_file (output_filename);
    return rc;
  }

  rc = __db_flush_file (fd_out);
  if (rc < 0)
  {
    free_backup_info (ref bi);
    delete_file (output_filename);
    return rc;
  }

  rc = __db_close (fd_out);
  if (rc < 0)
  {
    free_backup_info (ref bi);
    delete_file (output_filename);
    return rc;
  }

  bi^.output_fd = -1;
  free_backup_info (ref bi);

  return 0;
}

//---------------------------------------------------------------------------------------------------

// called before each __db_write() in database file when backup_info is not null,
// to pre-save the corresponding block in the backup before overwriting it.

public void backup_block (ref BACKUP_INFO bi, int db, long block_nr)
{
  int rc;

  if (bi.error < 0)  // previous error occured
    return;

  bi.waiting = true;      // signals the backup thread we need to write a block NOW !

  enter_shared_object (ref bi.so);

  bi.waiting = false;

  if (SET_item_found (bi.interval_to_backup, item => block_nr))
  {
    rc = __db_read (db, block_nr, out bi.buffers^);
    if (rc < 0)
    {
      bi.error = rc;
    }
    else
    {
      rc = __db_write (bi.output_fd, block_nr, bi.buffers^);
      if (rc < 0)
      {
        bi.error = rc;
      }
    }

    assert SET_delete_item (ref bi.interval_to_backup, block_nr);
  }

  leave_shared_object (ref bi.so);
}

/**********************************************************************/
#end unsafe
/**********************************************************************/
