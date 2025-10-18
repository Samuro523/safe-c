
// dbport.c : database portability layer

use ../crc, ../files, ../thread;
use ../db, config, dbstruct;
use ../exception, ../strings;

/**************************************************************************/

#if 1  // WIN32

SHARED_OBJECT so;
SHARED_OBJECT so_lseek;

/**************************************************************************/

void report_corrupted_block_in_crash_report (long block_nr, byte[] buffer)
{
  int  i, x;
  char str[64*2];

  log_in_crash_report ("corrupted db block %d\n", block_nr);

  clear str;
  for (i=0; i<(int)DB_BLOCK_SIZE; i+=64)
  {
    for (x=0; x<64; x++)
      sprintf (out str[x*2:2], "%02x", buffer[i+x]);
    log_in_crash_report ("%s\n", str);
  }
  log_in_crash_report ("------\n");
}

/**************************************************************************/

public void ENTER ()  // __db_enter_critical_section ();
{
  InterlockedAdd (ref __g_nb_threads_waiting, delta => +1);
  enter_shared_object (ref so);
}

/**************************************************************************/

public void LEAVE ()  // __db_leave_critical_section ();
{
  InterlockedAdd (ref __g_nb_threads_waiting, delta => -1);
  leave_shared_object (ref so);
}

/**************************************************************************/

// create new file in exclusive mode

public int __db_create_exclusive (string filename)
{
  return files.create (filename,
                       access          => READ+WRITE,
                       share           => 0,       // 0 = no one else can open/delete this file
                       error_if_exists => false,
                       delete_on_close => false,
                       organization    => RANDOM);
}

/**************************************************************************/

// open existing file in exclusive mode

public int __db_open_exclusive (string filename)
{
  const int ERROR_SHARING_VIOLATION = -32;    // locking error (for windows)
  int rc, i;

  for (i=0; i<=8; i++)
  {
    rc = files.open (filename,
                     access               => READ+WRITE,
                     share                => 0,       // 0 = no one else can open/delete this file
                     create_if_not_exists => false,
                     delete_on_close      => false,
                     organization         => RANDOM);

    if (rc != ERROR_SHARING_VIOLATION)       /* valid handle or error other than share violation */
      return rc;

    sleep 0.25;   /* wait until other process releases the file */
  }

  return ERROR_SHARING_VIOLATION;
}

/**************************************************************************/

// open existing file in share mode

public int __db_open_share (string filename)
{
  const int ERROR_SHARING_VIOLATION = -32;
  int rc, i;

  for (i=0; i<=8; i++)
  {
    rc = files.open (filename,
                     access               => READ+WRITE,
                     share                => READ+WRITE,  // open allowed, delete not allowed
                     create_if_not_exists => false,
                     delete_on_close      => false,
                     organization         => RANDOM);

    if (rc != ERROR_SHARING_VIOLATION)       /* valid handle or error other than share violation */
      return rc;

    sleep 0.25;   /* wait until other process releases the file */
  }

  return ERROR_SHARING_VIOLATION;
}

/**************************************************************************/

public int __db_close (int fd)
{
  return close (fd);
}

/**************************************************************************/

public int __db_unlink (string filename)
{
  return delete_file (filename);
}

/**************************************************************************/

public int __db_read (int fd, LINK block_nr, out byte[DB_BLOCK_SIZE] buffer)
{
  long pos, ret;
  int  rc;
  uint adler;

  pos = block_nr << DB_BLOCK_SIZE_SHIFTS;


  // this should not be necessary, however,
  // since a user got blocks written at wrong position in file,
  // we want to exclude the possibility that it comes from this layer.
  // this is now mandatory since backup thread and main thread can call this function at same time.
  enter_shared_object (ref so_lseek);

  ret = lseek (fd, pos);
  if (ret != pos)
  {
    leave_shared_object (ref so_lseek);
    clear buffer;
    rc = (int)ret;
    if (rc >= 0)
      rc = -1;
    return rc;
  }

  rc = read (fd, out buffer);

  leave_shared_object (ref so_lseek);


  if (rc < 0)
    return rc;

  if (rc != (int)buffer'size)
    return E_FILE_UNUSABLE;

  if (crypt != null)
    crypt (block_nr, ref buffer);

  adler = 1;
  update_adler (ref adler, block_nr);
  update_adler (ref adler, buffer[0 : buffer'size - 4]);
#begin unsafe

  if (adler != *(uint*)(&buffer[buffer'size - 4]))
  {
    enter_shared_object (ref so_lseek);

    ret = lseek (fd, pos);
    if (ret != pos)
    {
      leave_shared_object (ref so_lseek);
      clear buffer;
      rc = (int)ret;
      if (rc >= 0)
        rc = -1;
      return rc;
    }

    rc = read (fd, out buffer);

    leave_shared_object (ref so_lseek);

    if (rc == (int)buffer'size)
      report_corrupted_block_in_crash_report (block_nr, buffer);

    return E_CORRUPTED_BLOCK;
  }
#end unsafe

  return 0;
}

/**************************************************************************/

// returns E_NOSPC if disk is full

public int __db_write (int fd, LINK block_nr, byte[DB_BLOCK_SIZE] buffer)
{
  long pos, ret;
  int  rc;
  uint adler;
  byte[DB_BLOCK_SIZE] buffer2 = buffer;

  adler = 1;
  update_adler (ref adler, block_nr);
  update_adler (ref adler, buffer2[0 : buffer2'size - 4]);
  buffer2[buffer2'size - 4 : 4] = adler'byte;

  if (crypt != null)
    crypt (block_nr, ref buffer2);


  pos = block_nr << DB_BLOCK_SIZE_SHIFTS;


  enter_shared_object (ref so_lseek);

  ret = lseek (fd, pos);
  if (ret != pos)
  {
    leave_shared_object (ref so_lseek);
    rc = (int)ret;
    if (rc >= 0)
      rc = -1;
    return rc;
  }

  rc = write (fd, buffer2);

  leave_shared_object (ref so_lseek);


  if (rc < 0)
    return rc;

  if (rc != (int)buffer2'size)
    return E_NOSPC;

  return 0;
}

/**************************************************************************/

// if needed, extend file to reach size (returns E_NOSPC if disk is full)

public int __db_extend_file (int fd, LINK nb_blocks)
{
  long size, blocks1, blocks2;

  size = filesize (fd);
  if (size < 0)
    return (int)size;

  blocks1 = size >> DB_BLOCK_SIZE_SHIFTS;

  // round up to a multiple of 512 blocks (256 KBytes)
  blocks2 = (nb_blocks & (LINK'max - 511)) + 512;
  if (blocks2 <= 0)   // overflow
    return E_NOSPC;

  if (blocks1 < blocks2)
  {
    byte[DB_BLOCK_SIZE] buffer;   // 512 bytes
    LINK b;

    clear buffer;

    for (b=blocks1; b<blocks2; b++)
    {
      int rc = __db_write (fd, b, buffer);
      if (rc < 0)
        return rc;
    }
  }

  return 0;
}

/**************************************************************************/

public int __db_seek_log (int fd, long offset)
{
  long ret = lseek (fd, offset);
  if (ret < 0)
    return (int)ret;

  return 0;
}

/**************************************************************************/

public int __db_read_log (int fd, out byte[] buffer)
{
  int rc;

  rc = read (fd, out buffer);
  if (rc < 0)
    return rc;

  if (rc != (int)buffer'size)
    return E_FILE_UNUSABLE;

  return 0;
}

/**************************************************************************/

public int __db_write_log (int fd, byte[] buffer)
{
  int rc;

  rc = write (fd, buffer);
  if (rc < 0)
    return rc;

  if (rc != (int)buffer'size)
    return E_NOSPC;

  return 0;
}

/**************************************************************************/

/* flush all sectors of the file to disk */

public int __db_flush_file (int fd)
{
  return flush (fd);
}

/**************************************************************************/

/* lock a block exclusively (with 120 seconds delay)         */
/* Algorithm : lock all bytes of the block                   */

/* warning: Windows 32-bit locking prevents any read/write   */
/*          access to the locked portion of the file !       */

public int __db_lock_exclusive (out LOCKING_INFO  info,
                                int               fd,
                                LINK              block_nr)
{
  const int ERROR_LOCK_VIOLATION = -33;
  TIMER timer;
  int   rc;

  info = {fd     => fd,
          offset => block_nr << DB_BLOCK_SIZE_SHIFTS,
          size   => DB_BLOCK_SIZE};

  set_timer (out timer, 120);  // 120 seconds

  for (;;)
  {
    rc = lock_file (fd, EXCLUSIVE,
                    offset   => info.offset,
                    nb_bytes => info.size);

    if (rc != ERROR_LOCK_VIOLATION || timer_elapsed (timer))
      return rc;

    sleep 0.125;
  }
}

/**************************************************************************/

/* lock a block in share mode (with 120 seconds delay).          */
/* Algorithm : lock first byte of the block, then try to lock    */
/*             any other byte of the block with no-wait, finally */
/*             unlock the first byte.                            */

/* warning: Windows 32-bit locking prevents any read/write       */
/*          access to the locked portion of the file !           */

public int __db_lock_share (out LOCKING_INFO  info,
                            int               fd,
                            LINK              block_nr)
{
  const int    ERROR_LOCK_VIOLATION = -33;
  LOCKING_INFO my_info;
  TIMER        timer;
  int          rc;
  uint         ofs;

  my_info = {fd     => fd,
             offset => block_nr << DB_BLOCK_SIZE_SHIFTS,
             size   => 1};

  set_timer (out timer, 120);  // 120 seconds

  for (;;)
  {
    rc = lock_file (fd, EXCLUSIVE,
                    offset   => my_info.offset,
                    nb_bytes => my_info.size);
    if (rc == 0)
      break;

    if (rc != ERROR_LOCK_VIOLATION || timer_elapsed (timer))
    {
      clear info;
      return rc;
    }

    sleep 0.125;
  }


  info = {fd     => fd,
          offset => block_nr << DB_BLOCK_SIZE_SHIFTS,
          size   => 1};

  for (ofs=1; ofs<DB_BLOCK_SIZE; ofs++)
  {
    info.offset++;

    rc = lock_file (fd, EXCLUSIVE,
                    offset   => info.offset,
                    nb_bytes => info.size);
    if (rc == 0)
      break;

    if (rc != ERROR_LOCK_VIOLATION)
    {
      unlock_file (fd, EXCLUSIVE,
                   offset   => my_info.offset,
                   nb_bytes => my_info.size);
      clear info;
      return rc;
    }
  }

  rc = unlock_file (fd, EXCLUSIVE,
                    offset   => my_info.offset,
                    nb_bytes => my_info.size);
  if (rc < 0)
  {
    clear info;
    return rc;
  }

  return 0;
}

/**************************************************************************/

/* unlock block */

public int __db_unlock (LOCKING_INFO info)
{
  int rc;

  rc = unlock_file (handle   => info.fd,
                    EXCLUSIVE,
                    offset   => info.offset,
                    nb_bytes => info.size);
  if (rc < 0)
    return rc;

  return 0;
}

/**************************************************************************/

#endif   /* _WIN32 */



#if 0  // unix

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/locking.h>

/**************************************************************************/

/* create new file in exclusive mode */

static int __db_create_exclusive (char *filename)
{
  int          rc, fd;
  struct flock f;

  rc = _open (filename,                   /* open or create      */
              (int)(O_RDWR | O_CREAT),    /* (no truncate now !) */
              S_IREAD | S_IWRITE);
  if (rc < 0)
    return neg(errno);
  fd = rc;

  f.l_type   = F_WRLCK;
  f.l_whence = SEEK_SET;
  f.l_start  = 1;   /* offset = 1 */
  f.l_len    = 1;   /* size   = 1 */

  if (fcntl (fd, F_SETLK, &f) == -1)
  {
    if (errno == EDEADLK || errno == EACCES || errno == EAGAIN)
    {
      close (fd);
      return E_ACCES;
    }
    else
    {
      rc = errno;
      close (fd);
      return neg(rc);
    }
  }


  /* the file was opened correctly and we're the only user */

  /* truncate it to size 0 */

  rc = _open (filename, (int)(O_RDWR | O_TRUNC), S_IREAD | S_IWRITE);
  if (rc < 0)
  {
    rc = errno;
    close (fd);
    return neg(rc);
  }
  close (rc);   /* close duplicate handle */

  return fd;
}

/**************************************************************************/

/* open existing file in exclusive mode */

static int __db_open_exclusive (char *filename)
{
  int          rc, fd;
  struct flock f;

  rc = _open (filename, (int)(O_RDWR), S_IREAD | S_IWRITE);
  if (rc < 0)
    return neg(errno);
  fd = rc;

  f.l_type   = F_WRLCK;
  f.l_whence = SEEK_SET;
  f.l_start  = 1;   /* offset = 1 */
  f.l_len    = 1;   /* size   = 1 */

  if (fcntl (fd, F_SETLK, &f) == -1)
  {
    if (errno == EDEADLK || errno == EACCES || errno == EAGAIN)
    {
      close (fd);
      return E_ACCES;
    }
    else
    {
      rc = errno;
      close (fd);
      return neg(rc);
    }
  }


  /* the file was opened correctly and we're the only user */

  return fd;
}

/**************************************************************************/

/* open existing file in share mode */

static int __db_open_share (char *filename)
{
  int          rc, fd;
  struct flock f;

  rc = _open (filename, (int)(O_RDWR), S_IREAD | S_IWRITE);
  if (rc < 0)
    return neg(errno);
  fd = rc;

  f.l_type   = F_RDLCK;
  f.l_whence = SEEK_SET;
  f.l_start  = 1;   /* offset = 1 */
  f.l_len    = 1;   /* size   = 1 */

  if (fcntl (fd, F_SETLK, &f) == -1)
  {
    if (errno == EDEADLK || errno == EACCES || errno == EAGAIN)
    {
      close (fd);
      return E_ACCES;
    }
    else
    {
      rc = errno;
      close (fd);
      return neg(rc);
    }
  }

  return fd;
}

/**************************************************************************/

static int __db_close (int fd)
{
  if (_close (fd) < 0)
    return neg(errno);
  return 0;
}

/**************************************************************************/

static int __db_unlink (char *filename)
{
  if (_unlink (filename) < 0)
    return neg(errno);
  return 0;
}

/**************************************************************************/

static int __db_read (int fd, unsigned long block_nr, void *buffer, unsigned int size)
{
  unsigned int rc;

#if DEBUG_PORT
  trace ("%d: seek to block %lu (block size = %d)\n", fd, block_nr, block_size);
#endif

  if (block_nr > (unsigned long)2147483647L / (unsigned long)block_size)
    return E_NOSPC;

  if (_lseek (fd, block_nr << DB_BLOCK_SIZE_SHIFTS, SEEK_SET) == -1L)
    return neg(errno);

#if DEBUG_PORT
  trace ("%d: read %u bytes\n", fd, size);
#endif

  rc = _read (fd, buffer, size);

  if (rc != size)
  {
    if (rc == (unsigned int)-1)
      return neg(errno);
    return E_FILE_UNUSABLE;
  }

  return 0;
}

/**************************************************************************/

static int __db_write (int fd, unsigned long block_nr,, void *buffer, unsigned int size)
{
  unsigned int rc;

#if DEBUG_PORT
  trace ("%d: seek to block %lu (block size = %d)\n", fd, block_nr, block_size);
#endif

  if (block_nr > (unsigned long)2147483647L / (unsigned long)block_size)
    return E_NOSPC;

  if (_lseek (fd, block_nr << DB_BLOCK_SIZE_SHIFTS, SEEK_SET) == -1L)
    return neg(errno);

#if DEBUG_PORT
  trace ("%d: write %u bytes\n", fd, size);
#endif

  rc = _write (fd, buffer, size);

  if (rc != size)
  {
    if (rc == (unsigned int)-1)
      return neg(errno);
    return E_NOSPC;
  }

  return 0;
}

/**************************************************************************/

/* if needed, extend file to reach size (returns E_NOSPC if disk is full) */

static int __db_extend_file (int fd, unsigned long nb_blocks)
{
  unsigned long size;
  long          old_pos, old_size;
  int           rc;
  char          page[1024];
  unsigned int  length;

  if (nb_blocks > (unsigned long)2147483647L / (unsigned long)DB_BLOCK_SIZE)
    return E_NOSPC;

  size = nb_blocks << DB_BLOCK_SIZE_SHIFTS;

  old_pos = _lseek (fd, 0L, SEEK_END);   /* seek to EOF & get current pos */
  if (old_pos == -1L)
    return neg(errno);

  old_size = _lseek (fd, old_pos, SEEK_SET);  /* seek to old pos & get size */
  if (old_size == -1L)
    return neg(errno);

  if (size > (unsigned long)old_size)    /* file must be extended */
  {
#if DEBUG_PORT
  trace ("%d: extend file to %lu bytes\n", fd, size);
#endif

    memset (page, 0x00, sizeof(page));

    if (_lseek (fd, size-1L, SEEK_SET) == -1L)   /* seek to new EOF - 1 */
      return neg(errno);

    rc = _write (fd, page, 1);     /* write 1 byte */
    if (rc < 0)
      return neg(errno);
    if (rc != 1)
      return E_NOSPC;

    if (_lseek (fd, old_size, SEEK_SET) == -1L)   /* seek to old size */
      return neg(errno);

    while ((unsigned long)old_size < size)
    {
      if (size - (unsigned long)old_size >= sizeof(page))
        length = sizeof(page);
      else
        length = (unsigned int)(size - (unsigned long)old_size);

      rc = _write (fd, page, length);     /* write 'length' zero bytes */
      if (rc < 0)
        return neg(errno);
      if (rc != (int)length)
        return E_NOSPC;

      old_size += length;
    }

    if (_lseek (fd, old_pos, SEEK_SET) == -1L)   /* seek to old pos */
      return neg(errno);
  }

  return 0;
}

/**************************************************************************/

/* flush all sectors of the file to disk */

static int __db_flush_file (int fd)
{
#if DEBUG_PORT
  trace ("%d: flush\n", fd);
#endif

  if (_fsync (fd) < 0)
    return neg(errno);
  return 0;
}

/**************************************************************************/

/* lock block exclusively (with 120 seconds delay) */
/* Algorithm : lock all bytes of the block */

static int __db_lock_exclusive (LOCKING_INFO  *info,
                                int           fd,
                                unsigned long block_nr)
{
  TIMER        timer;
  struct flock f;

  set_timer (&timer, 120);

  info->fd     = fd;
  info->offset = block_nr << DB_BLOCK_SIZE_SHIFTS;
  info->size   = DB_BLOCK_SIZE;

  for (;;)
  {
    f.l_type   = F_WRLCK;
    f.l_whence = SEEK_SET;
    f.l_start  = info->offset;
    f.l_len    = DB_BLOCK_SIZE;

    if (fcntl (fd, F_SETLK, &f) == 0)
      break;

    if (errno == EDEADLK || errno == EACCES || errno == EAGAIN)
    {
      if (timer_elapsed (timer))
        return E_ACCES;
      sleep (0);
    }
    else
    {
      return neg(errno);
    }
  }

  return 0;
}

/**************************************************************************/

/* lock block in share mode (with 120 seconds delay) */

static int __db_lock_share (LOCKING_INFO  *info,
                            int           fd,
                            unsigned long block_nr)
{
  TIMER        timer;
  struct flock f;

  set_timer (&timer, 120);

  info->fd     = fd;
  info->offset = block_nr << DB_BLOCK_SIZE_SHIFTS;
  info->size   = DB_BLOCK_SIZE;

  for (;;)
  {
    f.l_type   = F_RDLCK;
    f.l_whence = SEEK_SET;
    f.l_start  = info->offset;
    f.l_len    = DB_BLOCK_SIZE;

    if (fcntl (fd, F_SETLK, &f) == 0)
      break;

    if (errno == EDEADLK || errno == EACCES || errno == EAGAIN)
    {
      if (timer_elapsed (timer))
        return E_ACCES;
      sleep (0);
    }
    else
    {
      return neg(errno);
    }
  }

  return 0;
}

/**************************************************************************/

/* unlock block */

static int __db_unlock (LOCKING_INFO *info)
{
  struct flock f;

  f.l_type   = F_UNLCK;
  f.l_whence = SEEK_SET;
  f.l_start  = info->offset;
  f.l_len    = info->size;

  if (fcntl (info->fd, F_SETLK, &f) == 0)
    return 0;

  if (errno == EDEADLK || errno == EACCES || errno == EAGAIN)
  {
    return E_ACCES;
  }
  else
  {
    return neg(errno);
  }
}

/**************************************************************************/

static void __db_enter_critical_section (void)
{
  $  /* not implemented for unix */
}

/**************************************************************************/

static void __db_leave_critical_section (void)
{
  $  /* not implemented for unix */
}

/**************************************************************************/

#endif /* unix */
