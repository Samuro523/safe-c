
// dbport.h : database portability layer

use config, dbstruct;

/**************************************************************************/

/* the three functions below return either a non-negative file handle     */
/* (for success), or a negative value (for failure). The negative value   */
/* corresponds to -errno.                                                 */

/**************************************************************************/

/* create new file in exclusive mode */
int __db_create_exclusive (string filename);

/* open existing file in exclusive mode */
int __db_open_exclusive (string filename);

/* open existing file in share mode */
int __db_open_share (string filename);

/**************************************************************************/

/* all functions below return either 0 (for success), or a negative value */
/* (for failure). The negative value corresponds to -errno.               */

/**************************************************************************/

/* close file */
int __db_close (int fd);

/**************************************************************************/

/* delete file */
int __db_unlink (string filename);

/**************************************************************************/

/* read (returns E_FILE_UNUSABLE if reaching the end of the file) */
int __db_read (int fd, LINK block_nr, out byte[DB_BLOCK_SIZE] buffer);

/* write (returns E_NOSPC if the disk is full) */
int __db_write (int fd, LINK block_nr, byte[DB_BLOCK_SIZE] buffer);

typedef void CRYPT (LINK block_nr, ref byte[512] block);
CRYPT crypt;

/**************************************************************************/

int __db_seek_log (int fd, long offset);

/* read (returns E_FILE_UNUSABLE if reaching the end of the file) */
int __db_read_log (int fd, out byte[] buffer);

/* write (returns E_NOSPC if the disk is full) */
int __db_write_log (int fd, byte[] buffer);

/**************************************************************************/

/* if needed, extend file to reach size (returns E_NOSPC if disk is full) */
int __db_extend_file (int fd, LINK nb_blocks);

/**************************************************************************/

/* flush all sectors of the file to disk */
int __db_flush_file (int fd);

/**************************************************************************/

/* lock block in exclusive mode */
int __db_lock_exclusive (out LOCKING_INFO info,
                             int          fd,
                             LINK         block_nr);

/* lock block in shared mode */
int __db_lock_share (out LOCKING_INFO info,
                         int          fd,
                         LINK         block_nr);

/* unlock block */
int __db_unlock (LOCKING_INFO info);

/**************************************************************************/

int __g_nb_threads_waiting;   // nb of threads waiting to enter or inside critical section

void ENTER ();  // enter_critical_section
void LEAVE ();  // leave_critical_section

/* the same thread can call __db_enter_critical_section() several times */
/* without locking itself. It must call an equivalent number of times   */
/* __db_leave_critical_section() in order to unlock other threads.      */

/**************************************************************************/
