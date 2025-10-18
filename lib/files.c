
// files.c : files, directories and disk volumes.

use strings, strformat;

#if WINDOWS
  use win/windows;
#endif

#if ANDROID
  use android/bionic;
  use utf;
#endif

//----------------------------------------------------------------------------

#begin unsafe

struct _FILE_INFO     // private part of FILE_INFO
{
#if WINDOWS
  HANDLE handle;
#endif

#if ANDROID
  byte*    ptr;
  string^  dir;
  wstring^ filter;
#endif
}

//----------------------------------------------------------------------------

struct FILE           // full type for text files
{
  int      h;             // windows handle (<=0 means closed file)
  int      error;         // error code from last fill_buffer or flush_buffer
  ENCODING coding;
  bool     is_write;      // false=read, true=write
  bool     last_chunk_read;
  bool     look_ahead_encoding_full;
  wchar    look_ahead_encoding;
  bool     look_ahead_crlf_full;
  wchar    look_ahead_crlf;
  bool     unget_full;
  wchar    unget_char;
  bool     put_char_full;
  wchar    put_char;
  int      len;           // index of buffer for reading from o.s. (not used for write)
  int      ofs;           // read or write index into buffer
  byte     buffer[16*1024];
}

const byte[] BOOM_UTF8    = {0xEF, 0xBB, 0xBF};
const byte[] BOOM_UTF16   = {0xFF, 0xFE};
const byte[] BOOM_UTF16BE = {0xFE, 0xFF};

//----------------------------------------------------------------------------

// returns current file size, or a negative error code.

#if WINDOWS
public long filesize (int handle)
{
  long size;
  if (GetFileSizeEx (handle, &size) == 0)
    return negative_last_windows_error();
  return size;
}
#endif

#if ANDROID
public long filesize (int handle)
{
  _stat s;
  if (bionic.fstat (handle, &s) != 0)
    return negative_errno();
  return s.st_size;
}
#endif

//----------------------------------------------------------------------------

// set file position.
// returns current file position, or a negative error code.

#if WINDOWS
public long lseek (int handle, long offset, SEEK_MODE mode = SEEK_SET)
{
  long pos;
  if (SetFilePointerEx (handle, offset, &pos, (DWORD)mode) == 0)
    return negative_last_windows_error();
  return pos;
}
#endif

#if ANDROID
public long lseek (int handle, long offset, SEEK_MODE mode = SEEK_SET)
{
  long pos = bionic.lseek (handle, offset, (int)mode);
  if (pos == -1)
    return negative_errno();
  return pos;
}
#endif

//----------------------------------------------------------------------------

// copy a string and append nul character

#if WINDOWS
void wstrcpyz (out wstring dest, wstring src)
{
  int i;

  clear dest;

  for (i=0; i<src'length && src[i] != Lnul; i++)
    dest[i] = src[i];

  dest[i] = Lnul;
}
#endif

//----------------------------------------------------------------------------

void wcstrcpy (out wstring dest, string src)
{
  int i;
  clear dest;
  for (i=0; i<src'length && src[i] != nul; i++)
    dest[i] = (wchar)(uint)src[i];
}

//----------------------------------------------------------------------------

#if WINDOWS
void cwstrcpy (out string dest, wstring src)
{
  int i;

  clear dest;

  for (i=0; i<src'length && src[i] != Lnul; i++)
  {
    uint c = (uint)src[i];
    if (c > 255)
      c = (uint)'?';
    dest[i] = (char)c;
  }
}
#endif

//----------------------------------------------------------------------------

#if WINDOWS
int win_open (wstring filename, uint access, uint share, uint dwCreationDisposition, bool delete_on_close, ORGANIZATION organization)
{
  wchar name[MAX_FILENAME_LENGTH+1];
  uint  mode, sh, attr;
  int   h;

  wstrcpyz (out name, filename);

  mode = 0;
  if ((access & READ) != 0)
    mode |= 0x80000000;
  if ((access & WRITE) != 0)
    mode |= 0x40000000;

  sh = 0;
  if ((share & READ) != 0)
    sh |= 1;       // FILE_SHARE_READ (other fd can read)
  if ((share & WRITE) != 0)
    sh |= 2;       // FILE_SHARE_WRITE (other fd can write)
  if ((share & DELETE) != 0)
    sh |= 4;       // FILE_SHARE_DELETE (other fd can delete)

  attr = FILE_ATTRIBUTE_NORMAL;
  if (organization == SEQUENTIAL)
    attr += FILE_FLAG_SEQUENTIAL_SCAN;
  else if (organization == RANDOM)
    attr += FILE_FLAG_RANDOM_ACCESS;

  if (delete_on_close)
    attr |= FILE_FLAG_DELETE_ON_CLOSE;

  h = (int)CreateFileW (&name, mode, sh, null, dwCreationDisposition, attr, 0);
  if (h == -1)   // INVALID_HANDLE_VALUE
    return negative_last_windows_error();

  return h;
}
#endif

#if ANDROID
// op: 0 = open, 1 = create, 2 = open/create_if_not_exist, 3 = create_but_must_not_exist
int android_open (char* filename, uint access, uint share, uint op, bool delete_on_close)
{
  int flags, fd, lk;

  if (op == 0)       // open
    flags = 0;
  else if (op == 1)  // create
    flags = O_CREAT | O_TRUNC;
  else if (op == 2)  // open/create_if_not_exist
    flags = O_CREAT;
  else if (op == 3)  // create_but_must_not_exist
    flags = O_CREAT | O_EXCL;
  else
    abort;

  flags |= O_CLOEXEC;  // close when exec a child process

  if ((access & (READ|WRITE)) == READ)
    flags |= O_RDONLY;
  else if ((access & (READ|WRITE)) == WRITE)
    flags |= O_WRONLY;
  else if ((access & (READ|WRITE)) == (READ|WRITE))
    flags |= O_RDWR;
  else
    abort;

  fd = bionic.open (pathname => filename,
                    flags    => flags & ~O_TRUNC,   // do not (damage) trunc file before applying share below
                    mode     => 504);  // octal 770 (rwx for owner and group, nothing for others)

  if (fd == -1)   // On error, -1 is returned and  errno is set to indicate the error.
    return negative_errno();

  if ((share & (READ|WRITE|DELETE)) == 0)  // other processes must not do anything
    lk = LOCK_EX;  // exclusive lock.  Only one process may hold an exclusive lock
  else                              // other processes can read/write
    lk = LOCK_SH;  // shared lock.  More than one process may hold a shared lock

  if (flock (fd, lk | LOCK_NB) < 0)   // LOCK_NB means non-blocking
  {
    int rc = negative_errno();
    close (fd);
    return rc;
  }

  if (op == 1)   // after advisory locking did work, we need to trunc it
  {
    int fd2 = bionic.open (pathname => filename,
                           flags    => flags,
                           mode     => 504);  // octal 770 (rwx for owner and group, nothing for others)
    if (fd2 < 0)
    {
      int rc = negative_errno();
      close (fd);
      return rc;
    }

    close (fd2);
  }

  if (delete_on_close)
    bionic.unlink (filename);

  return fd;
}
#endif

//----------------------------------------------------------------------------

// create a new file (deletes any existing file of the same name)
// returns a file handle, or a negative error code.
// warning: parameters 'share' and 'organization' are ignored on android.

#if WINDOWS
public
int wcreate (wstring      filename,
             uint         access          = WRITE,
             uint         share           = READ,
             bool         error_if_exists = false,
             bool         delete_on_close = false,
             ORGANIZATION organization    = UNDEFINED)
{
  const uint CREATE_NEW    = 1;
  const uint CREATE_ALWAYS = 2;
  return win_open (filename, access, share,
                  dwCreationDisposition => error_if_exists ? CREATE_NEW : CREATE_ALWAYS,
                  delete_on_close, organization);
}

public
int create  (string       filename,
             uint         access          = WRITE,
             uint         share           = READ,
             bool         error_if_exists = false,
             bool         delete_on_close = false,
             ORGANIZATION organization    = UNDEFINED)
{
  wchar name[MAX_FILENAME_LENGTH];
  wcstrcpy (out name, filename);
  return wcreate (name, access, share, error_if_exists, delete_on_close, organization);
}
#endif

#if ANDROID
public
int wcreate (wstring      filename,
             uint         access          = WRITE,
             uint         share           = READ,
             bool         error_if_exists = false,
             bool         delete_on_close = false,
             ORGANIZATION organization    = UNDEFINED)
{
  char name[3*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int  name_length;

  _unused organization;

  utf.utf16_to_utf8 (    filename,
                         wstrlen(filename),
                     out name,           // must be at least 3X larger than source !
                     out name_length);
  _unused name_length;

  return android_open (&name, access, share, op => error_if_exists ? 3 : 1, delete_on_close => delete_on_close);
}

public
int create  (string       filename,
             uint         access          = WRITE,
             uint         share           = READ,
             bool         error_if_exists = false,
             bool         delete_on_close = false,
             ORGANIZATION organization    = UNDEFINED)
{
  char name[2*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int  name_length;

  _unused organization;

  utf.ascii_to_utf8 (    filename,
                         strlen(filename),
                     out name,                    // must be at least 2X larger than source !
                     out name_length);
  _unused name_length;

  return android_open (&name, access, share, op => error_if_exists ? 3 : 1, delete_on_close => delete_on_close);
}
#endif

//----------------------------------------------------------------------------

// open an existing file.
// returns a file handle, or a negative error code.

#if WINDOWS
public
int wopen (wstring      filename,
           uint         access               = READ,
           uint         share                = READ,
           bool         create_if_not_exists = false,
           bool         delete_on_close      = false,
           ORGANIZATION organization         = UNDEFINED)
{
  const uint OPEN_EXISTING = 3;
  const uint OPEN_ALWAYS   = 4;
  return win_open (filename, access, share,
                   dwCreationDisposition => create_if_not_exists ?  OPEN_ALWAYS : OPEN_EXISTING,
                   delete_on_close, organization);
}

public
int open  (string       filename,
           uint         access               = READ,
           uint         share                = READ,
           bool         create_if_not_exists = false,
           bool         delete_on_close      = false,
           ORGANIZATION organization         = UNDEFINED)
{
  wchar name[MAX_FILENAME_LENGTH];
  wcstrcpy (out name, filename);
  return wopen (name, access, share, create_if_not_exists, delete_on_close, organization);
}
#endif  // WINDOWS

#if ANDROID
public
int wopen (wstring      filename,
           uint         access               = READ,
           uint         share                = READ,
           bool         create_if_not_exists = false,
           bool         delete_on_close      = false,
           ORGANIZATION organization         = UNDEFINED)
{
  char name[3*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int  name_length;

  _unused organization;

  utf.utf16_to_utf8 (    filename,
                         wstrlen(filename),
                     out name,           // must be at least 3X larger than source !
                     out name_length);
  _unused name_length;

  return android_open (&name, access, share, op => create_if_not_exists ? 2 : 0, delete_on_close => delete_on_close);
}

public
int open  (string       filename,
           uint         access               = READ,
           uint         share                = READ,
           bool         create_if_not_exists = false,
           bool         delete_on_close      = false,
           ORGANIZATION organization         = UNDEFINED)
{
  char name[2*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int  name_length;

  _unused organization;

  utf.ascii_to_utf8 (    filename,
                         strlen(filename),
                     out name,                    // must be at least 2X larger than source !
                     out name_length);
  _unused name_length;

  return android_open (&name, access, share, op => create_if_not_exists ? 2 : 0, delete_on_close => delete_on_close);
}
#endif

//----------------------------------------------------------------------------

// test if file exists

public bool wexists (wstring filename)
{
  int fd = wopen (filename);
  bool b = (fd >= 0);
  if (b)
    close (fd);
  return b;
}

public bool exists (string filename)
{
  int fd = open (filename);
  bool b = (fd >= 0);
  if (b)
    close (fd);
  return b;
}

//----------------------------------------------------------------------------

// returns nb of bytes written(>=0), or a negative error code.

#if WINDOWS
public int write (int handle, byte[] buffer)
{
  DWORD written;

  if (WriteFile (handle, &buffer, buffer'size, &written, null) == FALSE)
    return negative_last_windows_error();

  return (int)written;
}
#endif

#if ANDROID
public int write (int handle, byte[] buffer)
{
  int rc = bionic.write (handle, &buffer, buffer'length);
  if (rc == -1)
    return negative_errno();
  return rc;
}
#endif

//----------------------------------------------------------------------------

// returns nb of bytes read (>=0), or a negative error code.

#if WINDOWS
public int read (int handle, out byte[] buffer)
{
  DWORD read;

  if (ReadFile (handle, &buffer, buffer'size, &read, null) == FALSE)
    return negative_last_windows_error();

  return (int)read;
}
#endif

#if ANDROID
public int read (int handle, out byte[] buffer)
{
  int rc = bionic.read (handle, &buffer, buffer'length);
  if (rc == -1)
    return negative_errno();
  return rc;
}
#endif

//----------------------------------------------------------------------------

#if WINDOWS
public void get_ftime (int handle, out long time)
{
  if (GetFileTime (handle, null, null, (FILETIME *)&time) == FALSE)  // get time of last write access
    abort;
}
#endif

#if ANDROID
// clock in 10e-7 sec
long timespec_to_clock (timespec ts)
{
  return ts.tv_sec * 10_000_000L + ((ts.tv_nsec * 2748779070L) >> 38) + 11644473600L * 10_000_000;
}
#endif

#if ANDROID
public void get_ftime (int handle, out long time)
{
  _stat s;
  if (bionic.fstat (handle, &s) != 0)
    abort;

  time = timespec_to_clock (s.st_mtim);
}
#endif

//----------------------------------------------------------------------------

#if WINDOWS
public void set_ftime (int handle, long time)
{

  if (SetFileTime (handle, (FILETIME *)&time, (FILETIME *)&time, (FILETIME *)&time) == FALSE)
    abort;
}
#endif

#if ANDROID
public void set_ftime (int handle, long time)
{
  timespec tm[2];
  long     clock_nsec;

  clock_nsec = (time - 11644473600L * 10_000_000) * 100L;

  clear tm;
  tm[0].tv_sec = clock_nsec / 1_000_000_000;
  tm[0].tv_nsec = clock_nsec - 1_000_000_000 * tm[0].tv_sec;
  tm[1] = tm[0];

  assert bionic.futimens (handle, &tm) == 0;
}
#endif

//----------------------------------------------------------------------------

#if WINDOWS
public int lock_file (int handle, LOCK lock, long offset, long nb_bytes)
{
  OVERLAPPED o;

  clear o;
  o.Offset     = (uint)offset;
  o.OffsetHigh = (uint)(offset >> 32);

  if (LockFileEx (hFile                    => handle,
                  dwFlags                  => (uint)(1 + (lock == SHARED ? 0 : 2)),
                  dwReserved               => 0,
                  nNumberOfBytesToLockLow  => (uint)nb_bytes,
                  nNumberOfBytesToLockHigh => (uint)(nb_bytes >> 32),
                  lpOverlapped             => &o) == FALSE)
    return negative_last_windows_error();

  return 0;
}
#endif // WINDOWS

#if ANDROID
public int lock_file (int handle, LOCK lock, long offset, long nb_bytes)
{
  Flock fl, p*;
  long  arg;
  int   rc;

  clear fl;
  fl.l_type  = (int2)((lock == SHARED) ? F_RDLCK : F_WRLCK);
  fl.l_start = offset;
  fl.l_len   = nb_bytes;

  p = &fl;
  arg'byte = p'byte;

  rc = fcntl (handle, F_SETLK, arg);
  if (rc == -1)
    return negative_errno();
  return 0;
}
#endif // ANDROID

//----------------------------------------------------------------------------

#if WINDOWS
public int unlock_file (int handle, LOCK lock, long offset, long nb_bytes)
{
  OVERLAPPED o;

  _unused lock;

  clear o;
  o.Offset     = (uint)offset;
  o.OffsetHigh = (uint)(offset >> 32);

  if (UnlockFileEx (hFile                      => handle,
                    dwReserved                 => 0,
                    nNumberOfBytesToUnlockLow  => (uint)nb_bytes,
                    nNumberOfBytesToUnlockHigh => (uint)(nb_bytes >> 32),
                    lpOverlapped               => &o) == FALSE)
    return negative_last_windows_error();

  return 0;
}
#endif

#if ANDROID
public int unlock_file (int handle, LOCK lock, long offset, long nb_bytes)
{
  Flock fl, p*;
  long  arg;
  int   rc;

  _unused lock;

  clear fl;
  fl.l_type  = (int2)F_UNLCK;
  fl.l_start = offset;
  fl.l_len   = nb_bytes;

  p = &fl;
  arg'byte = p'byte;

  rc = fcntl (handle, F_SETLK, arg);
  if (rc == -1)
    return negative_errno();
  return 0;
}
#endif // ANDROID

//----------------------------------------------------------------------------

// forces a write of all file buffers to physical disk (for commiting a database transaction)

#if WINDOWS
public int flush (int handle)
{
  if (FlushFileBuffers(handle) == FALSE)
    return negative_last_windows_error();
  return 0;
}
#endif // WINDOWS

#if ANDROID
public int flush (int handle)
{
  int rc = bionic.fsync (handle);
  if (rc == -1)
    return negative_errno();
  return 0;
}
#endif // ANDROID

//----------------------------------------------------------------------------

#if WINDOWS
public int close (int handle)
{
  if (CloseHandle (handle) == FALSE)
    return negative_last_windows_error();
  return 0;
}
#endif // WINDOWS

#if ANDROID
public int close (int handle)
{
  int rc = bionic.close (handle);
  if (rc == -1)
    return negative_errno();
  return 0;
}
#endif // ANDROID

//----------------------------------------------------------------------------

// open a file for reading
// returns 0 if OK, a negative value if error.

public int wfopen (out FILE file, wstring filename)
{
  byte head[5];
  long pos, rc;

  clear file;
  file.h = wopen (filename,
                  access               => READ,
                  share                => READ,
                  create_if_not_exists => false,
                  delete_on_close      => false,
                  organization         => SEQUENTIAL);
  if (file.h <= 0)
    return file.h;

  clear head;
  read (file.h, out head);

  file.coding = ANSI;  // default
  pos = 0L;

  if (memcmp (head[0:BOOM_UTF8'length], BOOM_UTF8) == 0)
  {
    file.coding = UTF8;
    pos = BOOM_UTF8'length;
  }
  else if (memcmp (head[0:BOOM_UTF16'length], BOOM_UTF16) == 0)
  {
    file.coding = UTF16;
    pos = BOOM_UTF16'length;
  }
  else if (memcmp (head[0:BOOM_UTF16BE'length], BOOM_UTF16BE) == 0)
  {
    file.coding = UTF16BE;
    pos = BOOM_UTF16BE'length;
  }
  else if (head[0] > 0 && head[1] == 0)
  {
    file.coding = UTF16;
  }
  else if (head[0] == 0 && head[1] > 0)
  {
    file.coding = UTF16BE;
  }

  rc = lseek (file.h, pos, SEEK_SET);
  if (rc < 0)
  {
    close (file.h);
    file.h = 0;
    return (int)rc;
  }

  return 0;
}

public int fopen (out FILE file, string filename)
{
  wchar name[MAX_FILENAME_LENGTH];
  wcstrcpy (out name, filename);
  return wfopen (out file, name);
}

//----------------------------------------------------------------------------

bool write_boom (FILE file)
{
  bool b;
  switch (file.coding)
  {
    case ANSI:
      b = true;
      break;

    case UTF8:
      b = write (file.h, BOOM_UTF8) == BOOM_UTF8'length;
      break;

    case UTF16:
      b = write (file.h, BOOM_UTF16) == BOOM_UTF16'length;
      break;

    case UTF16BE:
      b = write (file.h, BOOM_UTF16BE) == BOOM_UTF16BE'length;
      break;

    default:
      b = false;
      break;
  }
  return b;
}

//----------------------------------------------------------------------------

// creating a file for writing.
// returns 0 if OK, a negative value if error.

public int wfcreate (out FILE file, wstring filename, ENCODING encoding)
{
  clear file;

  file.h = wcreate (filename,
                    access          => WRITE,
                    share           => READ,
                    error_if_exists => false,
                    delete_on_close => false,
                    organization    => SEQUENTIAL);

  file.is_write = true;
  file.coding = encoding;

  if (file.h <= 0)
    return file.h;

  if (!write_boom (file))
  {
    close (file.h);
    file.h = 0;
    return -1;
  }

  return 0;
}

public int fcreate (out FILE file, string filename, ENCODING encoding)
{
  wchar name[MAX_FILENAME_LENGTH];
  wcstrcpy (out name, filename);
  return wfcreate (out file, name, encoding);
}

//----------------------------------------------------------------------------

// open or create a file for writing at the end.
// returns 0 if OK, a negative value if error.

public int wfappend (out FILE file, wstring filename, ENCODING encoding)
{
  long rc;

  clear file;

  file.h = wopen (filename,
                  access               => WRITE,
                  share                => READ,
                  create_if_not_exists => true,
                  delete_on_close      => false,
                  organization         => SEQUENTIAL);

  file.is_write = true;
  file.coding = encoding;

  if (file.h <= 0)
    return file.h;

  rc = lseek (file.h, 0, SEEK_END);
  if (rc < 0)
  {
    close (file.h);
    file.h = 0;
    return (int)rc;
  }

  if (rc == 0)
  {
    if (!write_boom (file))
    {
      close (file.h);
      file.h = 0;
      return -1;
    }
  }

  return 0;
}

public int fappend (out FILE file, string filename, ENCODING encoding)
{
  wchar name[MAX_FILENAME_LENGTH];
  wcstrcpy (out name, filename);
  return wfappend (out file, name, encoding);
}

//----------------------------------------------------------------------------

void skip_char (char[] source, out int source_length)
{
  source_length = 1;
  while (source_length < source'length && ((byte)source[source_length] & 0b11000000) == 0b10000000)
    source_length++;
}

//----------------------------------------------------------------------------

void single_utf8_to_utf16 (    char[]   source,                // at least 1 char
                           out int      source_length,         // >=1
                           out wchar[2] target,
                           out int      target_length)         // 1 or 2
{
  const wchar replacement_char = L'?';
  int val;

  assert source'length >= 1;

  clear source_length, target, target_length;

  val = (int)source[0];

  if (val < 128)
  {
    source_length = 1;
  }
  else if ((val & 0xE0) == 0xC0 && source'length >= 2)
  {
    val = (((val & 31)) << 6) + ((int)source[1] & 63);

    if (val < 128 || ((int)source[1] & 192) != 128)    // illegal
    {
      val = (int)replacement_char;
      skip_char (source, out source_length);
    }
    else  // valid
    {
      source_length = 2;
    }
  }
  else if ((val & 0xF0) == 0xE0 && source'length >= 3)
  {
    val = (((val & 15)) << 12) + ((((int)source[1] & 63)) << 6) + ((int)source[2] & 63);

    if (val < 2048 ||    // illegal
        ((int)source[1] & 192) != 128 ||
        ((int)source[2] & 192) != 128 ||
        (val >= 0xD800 && val <= 0xDFFF) ||
        (val >= 0xFFFE && val <= 0xFFFF))
    {
      val = (int)replacement_char;
      skip_char (source, out source_length);
    }
    else  // valid
    {
      source_length = 3;
    }
  }
  else if ((val & 0xF8) == 0xF0 && source'length >= 4)
  {
    val = (((val & 7)) << 18)
        + ((((int)source[1] & 63)) << 12)
        + ((((int)source[2] & 63)) << 6)
        + ((int)source[3] & 63);

    if (val < 65536 || val > 0x10FFFF ||    // illegal
        ((int)source[1] & 192) != 128 ||
        ((int)source[2] & 192) != 128 ||
        ((int)source[3] & 192) != 128 ||
        ((int)val & 0xFFFF) >= 0xFFFE)
    {
      val = (int)replacement_char;
      skip_char (source, out source_length);
    }
    else  // valid
    {
      source_length = 4;
    }
  }
  else
  {
    val = (int)replacement_char;
    skip_char (source, out source_length);
  }


  if (val <= 0xFFFF)
  {
    target[target_length++] = (wchar)val;
  }
  else
  {
    val -= 0x10000;

    target[target_length++] = (wchar)(0xD800 + (val >> 10));
    target[target_length++] = (wchar)(0xDC00 + (val & 1023));
  }
}

//===========================================================================

int fetch_next_char (ref FILE file, out wchar c)
{
  int rc;

  if (file.look_ahead_encoding_full)
  {
    c = file.look_ahead_encoding;
    file.look_ahead_encoding_full = false;
    return 0;
  }

  clear c;

  if (file.error != 0)   // earlier error or FEOF
    return file.error;

  // try to make the next 4 bytes available for unicode conversion
  if (file.len - file.ofs < 4 && !file.last_chunk_read)
  {
    file.len -= file.ofs;
    file.buffer[0 : file.len] = file.buffer[file.ofs : file.len];
    file.ofs = 0;

    rc = read (file.h, out file.buffer[file.len : file.buffer'length - file.len]);
    if (rc < 0)
    {
      file.error = rc;
      return rc;
    }

    file.len += rc;
    if (file.len < file.buffer'length)  // could not fill all buffer
      file.last_chunk_read = true;
  }

  switch (file.coding)
  {
    case ANSI:
      if (file.len - file.ofs < 1)   // buffer exhausted
      {
        file.error = FEOF;
        return FEOF;
      }
      c = (wchar)file.buffer[file.ofs++];
      break;

    case UTF8:
      if (file.len - file.ofs < 1)   // buffer exhausted
      {
        file.error = FEOF;
        return FEOF;
      }

      {
        int      source_length;
        wchar[2] target;
        int      target_length;

        single_utf8_to_utf16 (    source        => file.buffer[file.ofs : file.len-file.ofs],
                              out source_length => source_length,
                              out target        => target,
                              out target_length => target_length);
        c = target[0];
        file.ofs += source_length;
        if (target_length == 2)
        {
          file.look_ahead_encoding = target[1];
          file.look_ahead_encoding_full = true;
        }
      }
      break;

    case UTF16:
      if (file.len - file.ofs < 2)   // buffer exhausted
      {
        file.error = FEOF;
        return FEOF;
      }
      c = (wchar)(file.buffer[file.ofs] + ((uint)file.buffer[file.ofs+1] << 8));
      file.ofs += 2;
      break;

    case UTF16BE:
      if (file.len - file.ofs < 2)   // buffer exhausted
      {
        file.error = FEOF;
        return FEOF;
      }
      c = (wchar)(file.buffer[file.ofs+1] + ((uint)file.buffer[file.ofs] << 8));
      file.ofs += 2;
      break;

    default:
      abort;
  }

  return 0;
}

//----------------------------------------------------------------------------

// get next char
// returns 0 if OK, FEOF(+1) if eof, a negative value if read error.

int wgetc (ref FILE file, out wchar c)
{
  int rc;

  if (file.unget_full)
  {
    c = file.unget_char;
    file.unget_full = false;
    return 0;
  }

  if (file.look_ahead_crlf_full)
  {
    c = file.look_ahead_crlf;
    file.unget_char = c;
    file.look_ahead_crlf_full = false;
    return 0;
  }

  rc = fetch_next_char (ref file, out c);
  if (rc != 0)
  {
    file.unget_char = c;
    return rc;    // FEOF or file error
  }

  // accept \r, \r\n, or \n
  if (c == L'\r')
  {
    if (fetch_next_char (ref file, out c) < 0)   // no further chars
    {
      c = L'\n';
      file.unget_char = c;
      return 0;                 // will return \n
    }

    if (c != L'\n')   // not \n -> unfetch char
    {
      file.look_ahead_crlf = c;
      file.look_ahead_crlf_full = true;
    }

    c = L'\n';        // will return \n
  }

  file.unget_char = c;
  return 0;
}

//----------------------------------------------------------------------------

// get next char
// returns 0 if OK, FEOF(+1) if eof, a negative value if read error.

int getc (ref FILE file, out char c)
{
  wchar wc;
  int   rc;

  rc = wgetc (ref file, out wc);
  if (rc != 0)
  {
    clear c;
    return rc;
  }

  c = wc <= (wchar)255 ? (char)(int)wc : '?';
  return 0;
}

//----------------------------------------------------------------------------

// returns 0 if OK, a negative value if error

int flush_buffer (ref FILE file)
{
  int rc;

  if (file.error != 0)
    return file.error;

  rc = write (file.h, file.buffer[0:file.ofs]);
  if (rc < 0)
  {
    file.error = rc;
    return file.error;
  }

  if (rc != file.ofs)  // could not write all
  {
    file.error = -1;
    return file.error;
  }

  file.ofs = 0;
  return 0;
}

//----------------------------------------------------------------------------

// convert a single character

void single_utf16_to_utf8 (    wchar[] source,
                           out int     source_length,    // 1 or 2
                           out char[]  target,           // at least 4 chars
                           out int     target_length)    // 1 to 4
{
  const wchar replacement_char = L'?';
  int val, val2;

  clear target, target_length;

  val = (int)source[0];

  if (val < 0xD800 || (val > 0xDFFF && val < 0xFFFE))
  {
    source_length = 1;
  }
  else if (val < 0xE000 && source'length >= 2)
  {
    val2 = (int)source[1];

    if (val >= 0xD800 && val <= 0xDBFF && val2 >= 0xDC00 && val2 <= 0xDFFF)
    {
      val = 0x10000 + ((val - 0xD800) << 10) + (val2 - 0xDC00);
      source_length = 2;
    }
    else
    {
      val = (int)replacement_char;
      source_length = 2;
    }
  }
  else  // illegal FFFE or FFFF
  {
    val = (int)replacement_char;
    source_length = 1;
  }

  if (val < (1<<7))
  {
    target[target_length++] = (char)val;
  }
  else if (val < (1<<11))
  {
    target[target_length++] = (char)(0b1100_0000 + (val >> 6));
    target[target_length++] = (char)(0b1000_0000 + (val & 0b11_1111));
  }
  else if (val <= 0xFFFF)
  {
    target[target_length++] = (char)(0b1110_0000 + ((val >> 12) & 0b1111));
    target[target_length++] = (char)(0b1000_0000 + ((val >> 6) & 0b11_1111));
    target[target_length++] = (char)(0b1000_0000 + (val & 0b11_1111));
  }
  else
  {
    target[target_length++] = (char)(0b1111_0000 + ((val >> 18) & 0b111));
    target[target_length++] = (char)(0b1000_0000 + ((val >> 12) & 0b11_1111));
    target[target_length++] = (char)(0b1000_0000 + ((val >> 6) & 0b11_1111));
    target[target_length++] = (char)(0b1000_0000 + (val & 0b11_1111));
  }
}

//===========================================================================

int store_next_char (ref FILE file, wchar c)
{
  if (file.buffer'length - file.ofs < 4)   // at least 4 bytes free in output buffer
  {
    int rc = flush_buffer (ref file);
    if (rc != 0)
      return rc;
  }

  switch (file.coding)
  {
    case ANSI:
      file.buffer[file.ofs++] = (byte)((int)c <= 255 ? c : L'?');
      break;

    case UTF8:
      if ((!file.put_char_full) && c >= (wchar)0xD800 && c < (wchar)0xE000)   // 2 wchar sequence
      {
        file.put_char = c;
        file.put_char_full = true;
        return 0;
      }

      {
        int source_length;
        int target_length;

        if (file.put_char_full)
        {
          single_utf16_to_utf8 (    source        => {file.put_char, c},
                                out source_length => source_length,    // 2
                                out target        => file.buffer[file.ofs : 4],
                                out target_length => target_length);

          assert source_length == 2;
          file.put_char_full = false;
        }
        else
        {
          single_utf16_to_utf8 (    source        => {c},
                                out source_length => source_length,    // 1
                                out target        => file.buffer[file.ofs : 4],
                                out target_length => target_length);

          assert source_length == 1;
        }

        file.ofs += target_length;
      }
      break;

    case UTF16:
      file.buffer[file.ofs++] = (byte)c;
      file.buffer[file.ofs++] = (byte)((uint)c >> 8);
      break;

    case UTF16BE:
      file.buffer[file.ofs++] = (byte)((uint)c >> 8);
      file.buffer[file.ofs++] = (byte)c;
      break;

    default:
      abort;
  }

  return 0;
}

//----------------------------------------------------------------------------

// returns 0 if OK, a negative value if error

int wputc (ref FILE file, wchar c)
{
  int rc;

  // expand \n to \r\n

  if (c == L'\n')    // first, write an \r
  {
    rc = store_next_char (ref file, L'\r');
    if (rc != 0)
      return rc;
  }

  rc = store_next_char (ref file, c);
  if (rc != 0)
    return rc;

  return 0;
}

//----------------------------------------------------------------------------

int putc (ref FILE file, char c)
{
  return wputc (ref file, (wchar)(int)c);
}

//----------------------------------------------------------------------------

// read single char from stream.
// returns 0 if OK, +1 if end-of-file, a negative value if read error.
// a runtime occurs when the handle is invalid.

public int fgetc (ref FILE file, out char c)
{
  assert (file.h > 0);

  if (file.is_write)   // bad file mode
  {
    clear c;
    return -1;
  }

  return getc (ref file, out c);
}

//----------------------------------------------------------------------------

public int wfgetc (ref FILE file, out wchar c)
{
  assert (file.h > 0);

  if (file.is_write)   // bad file mode
  {
    clear c;
    return -1;
  }

  return wgetc (ref file, out c);
}

//----------------------------------------------------------------------------

// write single char to stream, returns negative value if error
// a runtime occurs when the handle is invalid.

public int fputc (ref FILE file, char c)
{
  assert (file.h > 0);

  if (!file.is_write)   // bad file mode
    return -1;

  return putc (ref file, c);
}

//----------------------------------------------------------------------------

public int wfputc (ref FILE file, wchar c)
{
  assert (file.h > 0);

  if (!file.is_write)   // bad file mode
    return -1;

  return wputc (ref file, c);
}

//----------------------------------------------------------------------------

// reads a string from stream, until \n is reached or until the buffer is full
// returns 0 if OK, +1 if end-of-file, a negative value if read error.
// a runtime occurs when the handle is invalid.

public int fgets (ref FILE file, out string buffer)
{
  char c;
  int  rc, i;

  assert (file.h > 0);

  clear buffer;

  if (file.is_write)   // bad file mode
    return -1;

  if (file.error != 0)
    return file.error;

  for (i=0; i<buffer'length; i++)
  {
    // returns 0 if OK, FEOF(+1) if eof, a negative value if read error.
    rc = getc (ref file, out c);
    if (rc != 0)
      return rc;

    buffer[i] = c;

    if (c == '\n')
      break;
  }

  return 0;
}

//----------------------------------------------------------------------------

public int wfgets (ref FILE file, out wstring buffer)
{
  wchar c;
  int   rc, i;

  assert (file.h > 0);

  clear buffer;

  if (file.is_write)   // bad file mode
    return -1;

  if (file.error != 0)
    return file.error;

  for (i=0; i<buffer'length; i++)
  {
    // returns 0 if OK, FEOF(+1) if eof, a negative value if read error.
    rc = wgetc (ref file, out c);
    if (rc != 0)
      return rc;

    buffer[i] = c;

    if (c == L'\n')
      break;
  }

  return 0;
}

//----------------------------------------------------------------------------

// write a string to the stream, returns negative value if error
// a runtime occurs when the handle is invalid.

public int fputs (ref FILE file, string buffer)
{
  char c;
  int  rc, i;

  assert (file.h > 0);

  if (!file.is_write)   // bad file mode
    return -1;

  if (file.error != 0)
    return file.error;

  for (i=0; i<buffer'length; i++)
  {
    c = buffer[i];

    if (c == nul)
      break;

    rc = putc (ref file, c);
    if (rc != 0)
      return rc;
  }

  return 0;
}

//----------------------------------------------------------------------------

public int wfputs (ref FILE file, wstring buffer)
{
  wchar c;
  int   rc, i;

  assert (file.h > 0);

  if (!file.is_write)   // bad file mode
    return -1;

  if (file.error != 0)
    return file.error;

  for (i=0; i<buffer'length; i++)
  {
    c = buffer[i];

    if (c == Lnul)
      break;

    rc = wputc (ref file, c);
    if (rc != 0)
      return rc;
  }

  return 0;
}

//----------------------------------------------------------------------------

wstring^ wide (string s)
{
  int len = s'length;
  wstring^ t = new wstring (len);
  ref wstring tt = t^;
  int i;
  for (i=0; i<len; i++)
    tt[i] = (wchar)(uint)s[i];
  return t;
}

//----------------------------------------------------------------------------

package ReadFromFILE
  int Get (WGET_CONTEXT context, out wchar c);
  void UnGet (WGET_CONTEXT context);
end ReadFromFILE;

//----------------------------------------------------------------------------

package body ReadFromFILE

  // user function that must return 0 if OK, -1 if read error, -2 if end-of-stream.
  public int Get (WGET_CONTEXT context, out wchar c)
  {
    ref FILE file = *((FILE *)context);
    int rc;

    // returns 0 if OK, FEOF(+1) if eof, a negative value if read error.
    rc = wgetc (ref file, out c);
    if (rc != 0)
      return rc == +1 ? -2 : -1;

    return 0;
  }

  public void UnGet (WGET_CONTEXT context)
  {
    ref FILE file = *((FILE *)context);
    file.unget_full = true;
  }

end ReadFromFILE;

//----------------------------------------------------------------------------

// reads a formatted string from stream.
// returns:
// .  0 if OK
// . +1 if end-of-file.
// . -1 if a read-error occured during GET.
// . -2 if end-of-stream occured before all arguments received a value;
// . -3 if end-of-format string occured before all arguments received a value;
// . -4 if a syntax error occured in the stream;
// . -5 if an overflow occured while reading a numeric value or storing it in a numeric argument.
// a runtime occurs when the handle or the format string are invalid.

public int fscanf (ref FILE file, string format, out object[] arg)
{
  int rc;
  wstring^ wformat;

  assert (file.h > 0);

  if (file.is_write)   // bad file mode
    return -1;

  wformat = wide (format);
  rc = wscan_string ((short *)&file, ReadFromFILE.Get, ReadFromFILE.UnGet, wformat^, out arg);
  free wformat;

  return rc;
}

//----------------------------------------------------------------------------

public int wfscanf (ref FILE file, wstring format, out object[] arg)
{
  assert (file.h > 0);

  if (file.is_write)   // bad file mode
    return -1;

  return wscan_string ((short *)&file, ReadFromFILE.Get, ReadFromFILE.UnGet, format, out arg);
}

//----------------------------------------------------------------------------

package WriteToFILE
  int Put (WPUT_CONTEXT context, wstring buffer);
end WriteToFILE;

//----------------------------------------------------------------------------

package body WriteToFILE

  public int Put (WPUT_CONTEXT context, wstring buffer)
  {
    ref FILE file = *((FILE *)context);
    int   i, rc;
    wchar c;

    for (i=0; i<buffer'length; i++)
    {
      c = buffer[i];

      rc = wputc (ref file, c);
      if (rc != 0)
        return rc;
    }

    return 0;
  }

end WriteToFILE;

//----------------------------------------------------------------------------

// write a formatted string to the stream.
// returns 0 if OK, a negative value if error.
// a runtime occurs when file or format are invalid.

public int fprintf (ref FILE file, string format, object[] arg)
{
  wstring^ wformat;

  assert (file.h > 0);

  if (!file.is_write)   // bad file mode
    return -1;

  wformat = wide (format);
  assert (wformat_string ((short *)&file, WriteToFILE.Put, wformat^, arg) == 0);
  free wformat;

  return file.error;
}

//----------------------------------------------------------------------------

public int wfprintf (ref FILE file, wstring format, object[] arg)
{
  assert (file.h > 0);

  if (!file.is_write)   // bad file mode
    return -1;

  assert (wformat_string ((short *)&file, WriteToFILE.Put, format, arg) == 0);

  return file.error;
}

//----------------------------------------------------------------------------

// returns true if end-of-file reached, false otherwise.
// a runtime occurs when the handle is invalid.

public bool feof (ref FILE file)
{
  wchar c;
  int   rc;

  assert (file.h > 0);
  assert (!file.is_write);   // bad file mode

  // returns 0 if OK, FEOF(+1) if eof, a negative value if read error.
  rc = wgetc (ref file, out c);
  if (rc == 0)
  {
    file.unget_full = true;
    return false;
  }

  _unused c;
  return true;
}

//----------------------------------------------------------------------------

// flush all written data to disk, returns negative value if error
// a runtime occurs when the handle is invalid.

public int fflush (ref FILE file)
{
  assert (file.h > 0);

  if (!file.is_write)   // bad file mode
    return -1;

  flush_buffer (ref file);

  return file.error;
}

//----------------------------------------------------------------------------

// flush all written data to disk and closes the file.
// returns negative value if error
// a runtime occurs if file is invalid.

public int fclose (ref FILE file)
{
  int rc;

  assert (file.h > 0);

  if (file.is_write)
  {
    if (file.error == 0 && file.put_char_full)
      file.error = store_next_char (ref file, Lnul);

    if (file.error == 0)
      file.error = flush_buffer (ref file);
  }

  rc = close (file.h);

  file.h = 0;

  if (rc < 0)
    return rc;

  if (file.error < 0)   // error (don't return FEOF value +1)
    return file.error;

  return 0;
}

//----------------------------------------------------------------------------

// returns low-level file handle
// a runtime occurs if file is invalid.

public int fhandle (ref FILE file)
{
  assert (file.h > 0);
  return file.h;
}

//----------------------------------------------------------------------------

// rename or move file to a different directory but on the same disk.
// returns 0 if OK, or a negative error code.

#if WINDOWS
public int wmove_file (wstring source, wstring target)
{
  wchar oldf[MAX_FILENAME_LENGTH+1], newf[MAX_FILENAME_LENGTH+1];

  wstrcpyz (out oldf, source);
  wstrcpyz (out newf, target);

  if (MoveFileW (&oldf, &newf) == FALSE)
    return negative_last_windows_error();

  return 0;
}
#endif // WINDOWS

#if ANDROID
public int wmove_file (wstring source, wstring target)
{
  char name1[3*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int  name1_length;
  char name2[3*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int  name2_length;
  int  rc;
  _stat s;

  utf.utf16_to_utf8 (    source,
                         wstrlen(source),
                     out name1,           // must be at least 3X larger than source !
                     out name1_length);

  utf.utf16_to_utf8 (    target,
                         wstrlen(target),
                     out name2,           // must be at least 3X larger than source !
                     out name2_length);

  _unused name1_length;
  _unused name2_length;

  if (bionic.stat (&name2, &s) == 0)
    return -17;   // EEXISTS (target already exists)

  rc = bionic.rename (&name1, &name2);
  if (rc == -1)
    return negative_errno();

  return 0;
}
#endif // ANDROID

public int move_file (string source, string target)
{
  wchar[MAX_FILENAME_LENGTH] source2, target2;
  wcstrcpy (out source2, source);
  wcstrcpy (out target2, target);
  return wmove_file (source2, target2);
}

//----------------------------------------------------------------------------

// returns 0 if OK, or a negative error code.

#if WINDOWS
public int wcopy_file (wstring source, wstring target, bool overwrite = true)
{
  wchar[MAX_FILENAME_LENGTH+1] oldf, newf;

  wstrcpyz (out oldf, source);
  wstrcpyz (out newf, target);

  if (CopyFileW (&oldf, &newf, (BOOL)!overwrite) == FALSE)
    return negative_last_windows_error();

  return 0;
}
#endif // WINDOWS

#if ANDROID
public int wcopy_file (wstring source, wstring target, bool overwrite = true)
{
  int fd1, fd2;

  if (!overwrite && wexists(target))
    return -17;	// EEXIST : File exists

  fd1 = wopen (source);
  if (fd1 < 0)
    return fd1;

  fd2 = wcreate (target);
  if (fd2 < 0)
  {
    close (fd1);
    return fd2;
  }

  {
    byte buffer[1024*1024];
    int  rc1, rc2;

    for (;;)
    {
      rc1 = read (fd1, out buffer);
      if (rc1 <= 0)
      {
        close (fd1);
        close (fd2);
        return rc1;
      }

      rc2 = write (fd2, buffer[0 : rc1]);
      if (rc1 != rc2)
      {
        close (fd1);
        close (fd2);

        if (rc2 >= 0)  // could not write all
          rc2 = -5;    // i/o error

        return rc2;
      }
    }
  }
}
#endif // ANDROID


public int copy_file (string source, string target, bool overwrite = true)
{
  wchar[MAX_FILENAME_LENGTH] source2, target2;
  wcstrcpy (out source2, source);
  wcstrcpy (out target2, target);
  return wcopy_file (source2, target2, overwrite);
}

//----------------------------------------------------------------------------

// returns 0 if OK, or a negative error code.

#if WINDOWS
public int wdelete_file (wstring filename)
{
  wchar name[MAX_FILENAME_LENGTH+1];

  wstrcpyz (out name, filename);

  if (DeleteFileW(&name) == FALSE)
    return negative_last_windows_error();

  return 0;
}
#endif // WINDOWS

#if ANDROID
public int wdelete_file (wstring filename)
{
  char name[3*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int  name_length;
  int  rc;

  utf.utf16_to_utf8 (    filename,
                         wstrlen(filename),
                     out name,           // must be at least 3X larger than source !
                     out name_length);
  _unused name_length;

  rc = bionic.unlink (&name);
  if (rc == -1)
    return negative_errno();
  return 0;
}
#endif // ANDROID

public int delete_file (string filename)
{
  wchar[MAX_FILENAME_LENGTH] filename2;
  wcstrcpy (out filename2, filename);
  return wdelete_file (filename2);
}

//----------------------------------------------------------------------------

#if WINDOWS
void wconvert_file_info (WIN32_FIND_DATAW data, ref FILE_INFO info)
{
  info.wname = data.cFileName;
  cwstrcpy (out info.name, data.cFileName);

  if ((data.dwFileAttributes & 0x10) != 0)  // a directory
  {
    info.type = TYPE_DIRECTORY;
  }
  else
  {
    info.type = TYPE_REGULAR_FILE;
    info.size = data.nFileSizeLow;
    if (data.nFileSizeHigh != 0)
      info.size += (data.nFileSizeHigh << 32L);
  }

  info.time'byte = data.ftLastWriteTime'byte;
}
#endif

//----------------------------------------------------------------------------

// returns 0 if OK, or a negative error code if the file or folder was not found.

#if WINDOWS
public int wget_file_information (wstring filename, out FILE_INFO info)
{
  wchar            name[MAX_FILENAME_LENGTH+1];
  WIN32_FIND_DATAW data;

  clear info;
  wstrcpyz (out name, filename);

  if (GetFileAttributesExW (&name, 0, &data) == FALSE)
    return negative_last_windows_error();

  wstrncpy (out data.cFileName, filename, data.cFileName'length);
  wconvert_file_info (data, ref info);
  return 0;
}
#endif   // WINDOWS

#if ANDROID
public int wget_file_information (wstring filename, out FILE_INFO info)
{
  char  name[3*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int   name_length;
  int   rc;
  _stat s;

  clear info;

  utf.utf16_to_utf8 (    filename,
                         wstrlen(filename),
                     out name,           // must be at least 3X larger than source !
                     out name_length);
  _unused name_length;

  rc = bionic.stat (&name, &s);
  if (rc == -1)
    return negative_errno();

  if ((s.st_mode & S_IFREG) != 0)
    info.type = TYPE_REGULAR_FILE;
  else if ((s.st_mode & S_IFDIR) != 0)
    info.type = TYPE_DIRECTORY;

  info.time = timespec_to_clock (s.st_mtim);
  info.size = s.st_size;

  {
    char target[MAX_FILENAME_LENGTH];
    int  target_length;

    utf16_to_ascii (    filename,
                        wstrlen(filename),
                    out target,           // must be at least as long as source !
                    out target_length);   // will be same as source_length
    _unused target_length;
    strcpy (out info.name, target);
  }

  wstrcpy (out info.wname, filename);
  return 0;
}
#endif   // ANDROID

public int get_file_information (string filename, out FILE_INFO info)
{
  wchar[MAX_FILENAME_LENGTH] filename2;
  wcstrcpy (out filename2, filename);
  return wget_file_information (filename2, out info);
}

//----------------------------------------------------------------------------

bool is_dot_or_double_dot (string name)
{
  return strcmp (name, ".") == 0 || strcmp (name, "..") == 0;
}

//----------------------------------------------------------------------------

bool wis_dot_or_double_dot (wstring name)
{
  return wstrcmp (name, L".") == 0 || wstrcmp (name, L"..") == 0;
}

//----------------------------------------------------------------------------

// returns 0 if OK, NO_MORE_FILES if directory is empty, or a negative error code.

#if WINDOWS
public int wopen_directory (out FILE_INFO info, wstring directory_name, wstring filter = L"")
{
  wchar            name[MAX_FILENAME_LENGTH+1+4];
  wchar            last;
  HANDLE           h;
  WIN32_FIND_DATAW data;
  int              rc;

  clear info;

  // make sure filter has no / or \
  if (wstrchr (filter, L'/') != -1 || wstrchr (filter, L'\\') != -1)
    return -1;

  wstrcpyz (out name, directory_name);

  // make sure the name ends with / or \
  if (name[0] == Lnul)
    last = Lnul;
  else
    last = name[wstrlen(name)-1];

  if (last != L'/' && last != L'\\')
    wstrcat (ref name, L"/");

  // append the filter
  if (wstrlen(filter) == 0)
    wstrcat (ref name, L"*.*");
  else
    wstrcat (ref name, filter);

  // make sure there is a terminating nul
  assert wstrlen(name) < name'length;


  h = FindFirstFileW (&name, &data);

  if (h > 0)     // success (valid handle)
  {
    info.intern.handle = h;

    if (wis_dot_or_double_dot (data.cFileName))
      return read_directory (ref info);

    wconvert_file_info (data, ref info);
    return 0;
  }

  // error occured

  rc = negative_last_windows_error();
  if (rc == -2)              // ERROR_FILE_NOT_FOUND
    return NO_MORE_FILES;    // -1

  return rc;
}
#endif // WINDOWS

#if ANDROID
public int wopen_directory (out FILE_INFO info, wstring directory_name, wstring filter = L"")
{
  char  name[3*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int   name_length;
  byte* ptr;

  clear info;

  // make sure filter has no /
  if (wstrchr (filter, L'/') != -1)
    return -1;

  utf.utf16_to_utf8 (    directory_name,
                         wstrlen(directory_name),
                     out name,           // must be at least 3X larger than source !
                     out name_length);

  // make sure the name ends with /
  if (name_length == 0 || name[name_length-1] != '/')
    name[name_length++] = '/';

  // store the name for later, to build full filenames
  info.intern.dir = new string ' (name[0:name_length]);

  // add current folder
  name[name_length++] = '.';

  // make sure there is a terminating nul
  assert strlen(name) < name'length;

  // store the filter for later, or leave it null
  if (wstrlen(filter) > 0)
    info.intern.filter = new wstring ' (filter[0:wstrlen(filter)]);

  ptr = bionic.opendir (&name);
  if (ptr == null)
    return negative_errno();

  info.intern.ptr = ptr;

  return read_directory (ref info);
}
#endif // ANDROID


public int open_directory (out FILE_INFO info, string  directory_name, string filter = "")
{
  wchar[MAX_FILENAME_LENGTH] filename2;
  wchar[MAX_FILENAME_LENGTH] filter2;
  wcstrcpy (out filename2, directory_name);
  wcstrcpy (out filter2, filter);
  return wopen_directory (out info, filename2, filter2);
}

//----------------------------------------------------------------------------

// returns 0 if OK, NO_MORE_FILES if no more files, or a negative error code.

#if WINDOWS
public int read_directory (ref FILE_INFO info)
{
  HANDLE           h;
  WIN32_FIND_DATAW data;
  int              rc;

  h = info.intern.handle;  // save handle

  clear info;

  for (;;)
  {
    rc = FindNextFileW (h, &data);
    if (rc == 0)  // error
      break;
    if (!wis_dot_or_double_dot (data.cFileName))
      break;
  }

  if (rc != 0)  // success
  {
    wconvert_file_info (data, ref info);
    info.intern.handle = h;     // restore handle
    return 0;
  }

  info.intern.handle = h;     // restore handle

  // error occured

  rc = negative_last_windows_error();
  if (rc == -18)             // ERROR_NO_MORE_FILES
    return NO_MORE_FILES;    // -1

  return rc;
}
#endif  // WINDOWS

#if ANDROID
public int read_directory (ref FILE_INFO info)
{
  _FILE_INFO intern;
  dirent*    e;

  intern = info.intern;   // save intern info

  clear info;

  *__errno() = 0;

  for (;;)
  {
    e = readdir (intern.ptr);
    if (e == null)   // end of list
      break;
    if (is_dot_or_double_dot (e->d_name))
      continue;

    {
      wchar target[2*MAX_FILENAME_LENGTH];
      int   target_length;

      utf8_to_utf16 (    ((char*)&e->d_name)[0 : MAX_FILENAME_LENGTH],
                         strlen( ((char*)&e->d_name) [0 : MAX_FILENAME_LENGTH]),
                     out target,           // must be at least 2X larger than source !
                     out target_length);
      _unused target_length;
      wstrcpy (out info.wname, target);
    }

    {
      char target[MAX_FILENAME_LENGTH];
      int  target_length;

      utf8_to_ascii (    ((char*)&e->d_name)[0 : MAX_FILENAME_LENGTH],
                         strlen(((char*)&e->d_name)[0 : MAX_FILENAME_LENGTH]),
                     out target,           // must be at least as long as source !
                     out target_length);
      _unused target_length;
      strcpy (out info.name, target);
    }

    if (intern.filter == null || wname_matches_pattern (name => info.wname, pattern => intern.filter^))
      break;
  }

  info.intern = intern;   // restore intern info

  if (e == null)
  {
    int rc = *__errno();
    if (rc != 0)
      return negative_errno();
    return NO_MORE_FILES;    // -1
  }

  if ((e->d_type & DT_REG) != 0)
    info.type = TYPE_REGULAR_FILE;
  else if ((e->d_type & DT_DIR) != 0)
    info.type = TYPE_DIRECTORY;

  {
    char name[6*MAX_FILENAME_LENGTH+1];
    int   rc;
    _stat s;

    sprintf (out name, "%s%s", intern.dir^, ((char*)&e->d_name)[0 : MAX_FILENAME_LENGTH]);
    rc = bionic.stat (&name, &s);
    if (rc == -1)
      return negative_errno();

    info.time = timespec_to_clock (s.st_mtim);
    info.size = s.st_size;
  }

  return 0;
}
#endif  // ANDROID

//----------------------------------------------------------------------------

// returns 0 if OK, or a negative error code.

#if WINDOWS
public int close_directory (ref FILE_INFO info)
{
  HANDLE h;

  h = info.intern.handle;

  clear info;

  if (h == 0)
    return 0;

  if (FindClose (h) == FALSE)
    return negative_last_windows_error();

  return 0;
}
#endif  // WINDOWS

#if ANDROID
public int close_directory (ref FILE_INFO info)
{
  int rc;

  free info.intern.dir;
  free info.intern.filter;

  rc = closedir (info.intern.ptr);
  if (rc == -1)
    rc = negative_errno();

  clear info;

  return rc;
}
#endif  // ANDROID

//----------------------------------------------------------------------------

// returns 0 if OK, or a negative error code.

#if WINDOWS
public int wcreate_directory (wstring directory_name)
{
  wchar name[MAX_FILENAME_LENGTH+1];

  wstrcpyz (out name, directory_name);

  if (CreateDirectoryW (&name, null) == FALSE)
    return negative_last_windows_error();

  return 0;
}
#endif // WINDOWS

#if ANDROID
public int wcreate_directory (wstring directory_name)
{
  char name[3*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int  name_length;
  int  rc;

  utf.utf16_to_utf8 (    directory_name,
                         wstrlen(directory_name),
                     out name,           // must be at least 3X larger than source !
                     out name_length);
  _unused name_length;

  rc = bionic.mkdir (&name);
  if (rc == -1)
    return negative_errno();
  return 0;
}
#endif // ANDROID


public int create_directory (string directory_name)
{
  wchar[MAX_FILENAME_LENGTH] filename2;
  wcstrcpy (out filename2, directory_name);
  return wcreate_directory (filename2);
}

//----------------------------------------------------------------------------

// returns 0 if OK, or a negative error code.

#if WINDOWS
public int wremove_directory (wstring directory_name)
{
  wchar name[MAX_FILENAME_LENGTH+1];

  wstrcpyz (out name, directory_name);

  if (RemoveDirectoryW (&name) == FALSE)
    return negative_last_windows_error();

  return 0;
}
#endif // WINDOWS

#if ANDROID
public int wremove_directory (wstring directory_name)
{
  char name[3*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int  name_length;
  int  rc;

  utf.utf16_to_utf8 (    directory_name,
                         wstrlen(directory_name),
                     out name,           // must be at least 3X larger than source !
                     out name_length);
  _unused name_length;

  rc = bionic.rmdir (&name);
  if (rc == -1)
    return negative_errno();
  return 0;
}
#endif // ANDROID


public int remove_directory (string directory_name)
{
  wchar[MAX_FILENAME_LENGTH] filename2;
  wcstrcpy (out filename2, directory_name);
  return wremove_directory (filename2);
}

//----------------------------------------------------------------------------

#if WINDOWS
public void wget_current_directory (out wchar directory_name[MAX_FILENAME_LENGTH])
{
  GetCurrentDirectoryW (directory_name'size, &directory_name);
}
#endif  // WINDOWS

#if ANDROID
public void wget_current_directory (out wchar directory_name[MAX_FILENAME_LENGTH])
{
  char buf[MAX_FILENAME_LENGTH+1];

  clear buf;
  bionic.getcwd (&buf, buf'size);

  {
    wchar target[2*MAX_FILENAME_LENGTH];
    int   target_length;

    utf8_to_utf16 (    buf[0:MAX_FILENAME_LENGTH],
                       strlen(buf),
                   out target,           // must be at least 2X larger than source !
                   out target_length);

    wstrcpy (out directory_name, target[0 : target_length]);
  }
}
#endif  // ANDROID

#if WINDOWS
public void get_current_directory (out char directory_name[MAX_FILENAME_LENGTH])
{
  GetCurrentDirectoryA (directory_name'size, &directory_name);
}
#endif  // WINDOWS

#if ANDROID
public void get_current_directory (out char directory_name[MAX_FILENAME_LENGTH])
{
  char buf[MAX_FILENAME_LENGTH+1];

  clear buf;
  bionic.getcwd (&buf, buf'size);

  {
    char target[MAX_FILENAME_LENGTH+1];
    int  target_length;

    utf8_to_ascii (    buf,
                       strlen(buf),
                   out target,               // must be at least as long as source !
                   out target_length);

    strcpy (out directory_name, target[0 : target_length]);
  }
}
#endif  // ANDROID

//----------------------------------------------------------------------------

// returns 0 if OK, or a negative error code.

#if WINDOWS
public int wset_current_directory (wstring directory_name)
{
  wchar name[MAX_FILENAME_LENGTH+1];

  wstrcpyz (out name, directory_name);

  if (SetCurrentDirectoryW (&name) == FALSE)
    return negative_last_windows_error();

  return 0;
}
#endif  // WINDOWS

#if ANDROID
public int wset_current_directory (wstring directory_name)
{
  char name[3*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int  name_length;
  int  rc;

  utf.utf16_to_utf8 (    directory_name,
                         wstrlen(directory_name),
                     out name,           // must be at least 3X larger than source !
                     out name_length);
  _unused name_length;

  rc = bionic.chdir (&name);
  if (rc == -1)
    return negative_errno();
  return 0;
}
#endif  // ANDROID


public int set_current_directory (string directory_name)
{
  wchar[MAX_FILENAME_LENGTH] filename2;
  wcstrcpy (out filename2, directory_name);
  return wset_current_directory (filename2);
}

//----------------------------------------------------------------------------

#if WINDOWS
public long wfree_disk_space (wstring directory_name)  // any directory on the disk
{
  wchar         name[MAX_FILENAME_LENGTH+1];
  int           len;
  FILE_INFO     info;
  LARGE_INTEGER f;

  wstrcpyz (out name, directory_name);

  len = wstrlen(name);

  for (;;)
  {
    if (len == 0 || (wget_file_information (name, out info) == 0 && info.type == TYPE_DIRECTORY))
      break;
    name[--len] = Lnul;
    while (len > 0 && name[len-1] != L'/' && name[len-1] != L'\\')
      name[--len] = Lnul;
  }

  if (len == 0)
    name[0:2] = {L'/', Lnul};

  clear f;
  GetDiskFreeSpaceExW (&name, &f, null, null);

  return f.low + (f.high << 32L);
}
#endif  // WINDOWS

#if ANDROID
public long wfree_disk_space (wstring directory_name)  // any directory on the disk
{
  char name[3*MAX_FILENAME_LENGTH+1];   // long enough so a trailing zero will be there
  int  name_length;
  int  rc;
  _statvfs s;

  utf.utf16_to_utf8 (    directory_name,
                         wstrlen(directory_name),
                     out name,           // must be at least 3X larger than source !
                     out name_length);
  _unused name_length;

  rc = statvfs (&name, &s);
  if (rc == -1)
    return negative_errno();
  return s.f_bsize * s.f_bavail;
}
#endif // ANDROID

public long free_disk_space (string directory_name)  // any directory on the disk
{
  wchar[MAX_FILENAME_LENGTH] filename2;
  wcstrcpy (out filename2, directory_name);
  return wfree_disk_space (filename2);
}

//----------------------------------------------------------------------------

#end unsafe


//----------------------------------------------------------------------------

// split pathname in 3 parts
//
// pathname  ::= device  directory  filename
//
// device    ::= drive-letter: | \\computer | (empty)
// directory ::= [/|\] {name (/|\)}
// filename  ::= name [.ext]
//
// any part can be empty.

void intern_split_pathname (    string pathname,
                            out int    offset_directory,
                            out int    offset_filename,
                            out int    offset_end)
{
  int i, last, len;

  len = strlen(pathname);

  if (len >= 2 &&
      toupper(pathname[0]) >= 'A' && toupper(pathname[0]) <= 'Z' &&
      pathname[1] == ':')       // drive letter
  {
    i = 2;
  }
  else if (len >= 2 && pathname[0] == '\\' && pathname[1] == '\\')   // windows computer name
  {
    i = 2;
    while (i < len && pathname[i] != '\\' && pathname[i] != '/')
      i++;
  }
  else
  {
    i = 0;
  }

  last = len;
  while (last > i && pathname[last-1] != '/' && pathname[last-1] != '\\')
    last--;

  offset_directory = i;
  offset_filename  = last;
  offset_end       = len;
}

void wintern_split_pathname (    wstring pathname,
                             out int    offset_directory,
                             out int    offset_filename,
                             out int    offset_end)
{
  int i, last, len;

  len = wstrlen(pathname);

  if (len >= 2 &&
      wtoupper(pathname[0]) >= L'A' && wtoupper(pathname[0]) <= L'Z' &&
      pathname[1] == L':')       // drive letter
  {
    i = 2;
  }
  else if (len >= 2 && pathname[0] == L'\\' && pathname[1] == L'\\')   // windows computer name
  {
    i = 2;
    while (i < len && pathname[i] != L'\\' && pathname[i] != L'/')
      i++;
  }
  else
  {
    i = 0;
  }

  last = len;
  while (last > i && pathname[last-1] != L'/' && pathname[last-1] != L'\\')
    last--;

  offset_directory = i;
  offset_filename  = last;
  offset_end       = len;
}

//----------------------------------------------------------------------------

// split pathname in 3 parts
//
// pathname  ::= device  directory  filename
//
// device    ::= drive-letter: | \\computer | (empty)
// directory ::= [/|\] {name (/|\)}
// filename  ::= name [.ext]
//
// any part can be empty.
// the program halts if any out parameter is too short.

public void split_pathname (    string pathname,
                            out string device,
                            out string directory,  // starts with / or \ if absolute path
                            out string filename)   // does not contain any / or \
{
  int offset_directory, offset_filename, offset_end;

  intern_split_pathname (pathname, out offset_directory, out offset_filename, out offset_end);

  strcpy (out device,    pathname[0                : offset_directory]);
  strcpy (out directory, pathname[offset_directory : offset_filename - offset_directory]);
  strcpy (out filename,  pathname[offset_filename  : offset_end      - offset_filename]);
}

public void wsplit_pathname (    wstring pathname,
                             out wstring device,
                             out wstring directory,  // starts with / or \ if absolute path
                             out wstring filename)   // does not contain any / or \
{
  int offset_directory, offset_filename, offset_end;

  wintern_split_pathname (pathname, out offset_directory, out offset_filename, out offset_end);

  wstrcpy (out device,    pathname[0                : offset_directory]);
  wstrcpy (out directory, pathname[offset_directory : offset_filename - offset_directory]);
  wstrcpy (out filename,  pathname[offset_filename  : offset_end      - offset_filename]);
}

//----------------------------------------------------------------------------

/* filename  ::= [device] [pathname]                        */
/* device    ::= drive-letter: | \\computer | (empty)       */
/* pathname  ::= {[/|\] [name]}                             */
/*                                                          */
/* replace "\" by "/" except first one after \\computer     */
/* replace "//" by "/".                                     */
/* normalize ".", "..", "...", .. path components.          */
/*                                                          */
/* the device parameter is provided for read-only.          */

void normalize_device_pathname (    string device,
                                ref string pathname)
{
  int s, t;

  // replace all '\' by '/'

  for (s=0; s<pathname'length && pathname[s] != nul; s++)
  {
    if (pathname[s] == '\\')
      pathname[s] = '/';
  }


  // remove duplicate '/'

  for (s=0,t=0; s<pathname'length && pathname[s] != nul; )
  {
    if (pathname[s] == '/')
    {
      pathname[t++] = pathname[s++];
      while (s<pathname'length && pathname[s] == '/')    // swallow duplicate '/'
        s++;
    }
    else
    {
      pathname[t++] = pathname[s++];
    }
  }
  if (t < pathname'length)
    pathname[t] = nul;


  /* normalize ".", "..", "...", .. path components */

  {
    int  p, start;
    int  begin_relative_path, previous_component;
    int  len, count;
    bool only_dots;

    p = 0;

    if (pathname'length >= 1 && pathname[p] == '/')   // skip separator (in case of absolute path)
      p++;

    begin_relative_path = p;  // no change of the filename before this point

    for (;;)
    {
      start = p;      // start of path component

      // parse component name

      only_dots = true;
      while (p < pathname'length && pathname[p] != nul && pathname[p] != '/')
      {
        if (pathname[p] != '.')
          only_dots = false;
        p++;
      }

      len = p - start;

      if (len == 0)   // no more components
      {
        break;
      }
      else if (only_dots)    // "." or ".." or "...", ..
      {
        // we must swallow this component and the (len-1) previous ones

        previous_component = start;
        count = len;

        for (;;)
        {
          if (previous_component == begin_relative_path)
            break;

          previous_component--;

          if (pathname[previous_component] == '/')
          {
            count--;
            if (count == 0)
            {
              previous_component++;   // skip '/'
              break;
            }
          }
        }

        if (p == pathname'length || pathname[p] == nul)   // path ends with dot
        {
          pathname[previous_component] = '.';
          if (previous_component+1 < pathname'length)
            pathname[previous_component+1] = nul;
          break;
        }
        else              /* "/" */
        {
          pathname[previous_component: pathname'length - (p+1)] = pathname[p+1: pathname'length - (p+1)];
          if (previous_component < (p+1))
            pathname[previous_component + pathname'length - (p+1)] = nul;
          p = previous_component;
        }
      }
      else   // normal path component
      {
        if (p == pathname'length || pathname[p] == nul)
          break;
        p++;               // skip '/'
      }
    }
  }


  // restore '\' for a device that is a computer name

  if (device'length >= 1 && device[0] == '\\' && pathname'length >= 1 && pathname[0] == '/')
    pathname[0] = '\\';
}

void wnormalize_device_pathname (    wstring device,
                                 ref wstring pathname)
{
  int s, t;

  // replace all '\' by '/'

  for (s=0; s<pathname'length && pathname[s] != Lnul; s++)
  {
    if (pathname[s] == L'\\')
      pathname[s] = L'/';
  }


  // remove duplicate '/'

  for (s=0,t=0; s<pathname'length && pathname[s] != Lnul; )
  {
    if (pathname[s] == L'/')
    {
      pathname[t++] = pathname[s++];
      while (s<pathname'length && pathname[s] == L'/')    // swallow duplicate '/'
        s++;
    }
    else
    {
      pathname[t++] = pathname[s++];
    }
  }
  if (t < pathname'length)
    pathname[t] = Lnul;


  /* normalize ".", "..", "...", .. path components */

  {
    int  p, start;
    int  begin_relative_path, previous_component;
    int  len, count;
    bool only_dots;

    p = 0;

    if (pathname'length >= 1 && pathname[p] == L'/')   // skip separator (in case of absolute path)
      p++;

    begin_relative_path = p;  // no change of the filename before this point

    for (;;)
    {
      start = p;      // start of path component

      // parse component name

      only_dots = true;
      while (p < pathname'length && pathname[p] != Lnul && pathname[p] != L'/')
      {
        if (pathname[p] != L'.')
          only_dots = false;
        p++;
      }

      len = p - start;

      if (len == 0)   // no more components
      {
        break;
      }
      else if (only_dots)    // "." or ".." or "...", ..
      {
        // we must swallow this component and the (len-1) previous ones

        previous_component = start;
        count = len;

        for (;;)
        {
          if (previous_component == begin_relative_path)
            break;

          previous_component--;

          if (pathname[previous_component] == L'/')
          {
            count--;
            if (count == 0)
            {
              previous_component++;   // skip '/'
              break;
            }
          }
        }

        if (p == pathname'length || pathname[p] == Lnul)   // path ends with dot
        {
          pathname[previous_component] = L'.';
          if (previous_component+1 < pathname'length)
            pathname[previous_component+1] = Lnul;
          break;
        }
        else              /* "/" */
        {
          pathname[previous_component: pathname'length - (p+1)] = pathname[p+1: pathname'length - (p+1)];
          if (previous_component < (p+1))
            pathname[previous_component + pathname'length - (p+1)] = Lnul;
          p = previous_component;
        }
      }
      else   // normal path component
      {
        if (p == pathname'length || pathname[p] == Lnul)
          break;
        p++;               // skip '/'
      }
    }
  }


  // restore '\' for a device that is a computer name

  if (device'length >= 1 && device[0] == L'\\' && pathname'length >= 1 && pathname[0] == L'/')
    pathname[0] = L'\\';
}

//----------------------------------------------------------------------------

// compute a filename from :
// - a current directory,
// - an absolute or relative filename.
// normalizes the filename.
// note: source and result filenames can denote the same buffer.
// returns 0 if OK, -1 if result_filename'length is too small.

public int expand_pathname (    string current_directory,
                                string source_filename,
                            out string result_filename)
{
  int curdir_offset_directory, curdir_offset_filename, curdir_offset_end;
  int srcfile_offset_directory, srcfile_offset_filename, srcfile_offset_end;
  int rc;
  string^ result_device, result_pathname;

  intern_split_pathname (    current_directory,
                         out curdir_offset_directory,
                         out curdir_offset_filename,
                         out curdir_offset_end);

  intern_split_pathname (    source_filename,
                         out srcfile_offset_directory,
                         out srcfile_offset_filename,
                         out srcfile_offset_end);

  if (srcfile_offset_directory > 0)   // source_filename has a device
    result_device = new string ' (source_filename[0:srcfile_offset_directory]);   // source device
  else
    result_device = new string ' (current_directory[0:curdir_offset_directory]);  // current device

  // if source_filename starts with slash, or current directory is empty
  if ((srcfile_offset_filename - srcfile_offset_directory >= 1 &&
        (source_filename[srcfile_offset_directory] == '/' ||
         source_filename[srcfile_offset_directory] == '\\'))
     || curdir_offset_directory == curdir_offset_filename)
  {
    result_pathname = new string ' (source_filename[srcfile_offset_directory : srcfile_offset_end - srcfile_offset_directory]);
  }
  else  // use current directory's directory
  {
    result_pathname = new string (curdir_offset_end - curdir_offset_directory + 1
                                  + srcfile_offset_end - srcfile_offset_directory);

    sprintf (out result_pathname^, "%s/%s",
             current_directory[curdir_offset_directory : curdir_offset_end - curdir_offset_directory],
             source_filename  [srcfile_offset_directory : srcfile_offset_end - srcfile_offset_directory]);
  }

  normalize_device_pathname (result_device^, ref result_pathname^);

  if (strlen(result_device^) + strlen(result_pathname^) > result_filename'length)
  {
    clear result_filename;
    rc = -1;
  }
  else
  {
    sprintf (out result_filename, "%s%s", result_device^, result_pathname^);
    rc = 0;
  }

  free result_device;
  free result_pathname;

  return rc;
}


public int wexpand_pathname (    wstring current_directory,
                                 wstring source_filename,
                             out wstring result_filename)
{
  int curdir_offset_directory, curdir_offset_filename, curdir_offset_end;
  int srcfile_offset_directory, srcfile_offset_filename, srcfile_offset_end;
  int rc;
  wstring^ result_device, result_pathname;

  wintern_split_pathname (    current_directory,
                          out curdir_offset_directory,
                          out curdir_offset_filename,
                          out curdir_offset_end);

  wintern_split_pathname (    source_filename,
                          out srcfile_offset_directory,
                          out srcfile_offset_filename,
                          out srcfile_offset_end);

  if (srcfile_offset_directory > 0)   // source_filename has a device
    result_device = new wstring ' (source_filename[0:srcfile_offset_directory]);   // source device
  else
    result_device = new wstring ' (current_directory[0:curdir_offset_directory]);  // current device

  // if source_filename starts with slash, or current directory is empty
  if ((srcfile_offset_filename - srcfile_offset_directory >= 1 &&
        (source_filename[srcfile_offset_directory] == L'/' ||
         source_filename[srcfile_offset_directory] == L'\\'))
     || curdir_offset_directory == curdir_offset_filename)
  {
    result_pathname = new wstring ' (source_filename[srcfile_offset_directory : srcfile_offset_end - srcfile_offset_directory]);
  }
  else  // use current directory's directory
  {
    result_pathname = new wstring (curdir_offset_end - curdir_offset_directory + 1
                                  + srcfile_offset_end - srcfile_offset_directory);

    wsprintf (out result_pathname^, L"%S/%S",
             current_directory[curdir_offset_directory : curdir_offset_end - curdir_offset_directory],
             source_filename  [srcfile_offset_directory : srcfile_offset_end - srcfile_offset_directory]);
  }

  wnormalize_device_pathname (result_device^, ref result_pathname^);

  if (wstrlen(result_device^) + wstrlen(result_pathname^) > result_filename'length)
  {
    clear result_filename;
    rc = -1;
  }
  else
  {
    wsprintf (out result_filename, L"%S%S", result_device^, result_pathname^);
    rc = 0;
  }

  free result_device;
  free result_pathname;

  return rc;
}

//----------------------------------------------------------------------------

// replace "\" by "/" except first one after \\computer
// replace "//" by "/"
// normalize ".", "..", "...", .. path components.

public void normalize_pathname (ref string pathname)
{
  (void)expand_pathname ("", pathname, out pathname);
}

public void wnormalize_pathname (ref wstring pathname)
{
  (void)wexpand_pathname (L"", pathname, out pathname);
}

//----------------------------------------------------------------------------

bool _intern_name_match_pattern (string name,
                                 string pattern,
                                 char   replace_one,
                                 char   replace_many,
                                 int    name_first0,
                                 int    name_last0,
                                 int    pattern_first0,
                                 int    pattern_last0)
{
  int i;
  int name_first    = name_first0;
  int name_last     = name_last0;
  int pattern_first = pattern_first0;
  int pattern_last  = pattern_last0;

  while (name_first <= name_last && pattern_first <= pattern_last)
  {
    if ((pattern[pattern_first] == name[name_first] && pattern[pattern_first] != replace_many)
        || pattern[pattern_first] == replace_one)
    {
      // eliminate common first characters
      name_first++;
      pattern_first++;
    }
    else if ((pattern[pattern_last] == name[name_last] && pattern[pattern_last] != replace_many)
             || pattern[pattern_last] == replace_one)
    {
      // eliminate common last characters
      name_last--;
      pattern_last--;
    }
    else if (pattern[pattern_first] == replace_many)
    {
      // suppress all 'replace many' characters

      while (pattern_first <= pattern_last && pattern[pattern_first] == replace_many)
      {
        pattern_first++;
      }

      for (i=name_last+1; i>=name_first; i--)
      {
        if (_intern_name_match_pattern (name, pattern,
                                        replace_one, replace_many,
                                        i, name_last,
                                        pattern_first, pattern_last))
        {
          return true;
        }
      }

      return false;
    }
    else   // in all other cases there's no match
    {
      return false;
    }
  }

  // here, either the name or the pattern is empty, or both

  // suppress all 'replace many' characters

  while (pattern_first <= pattern_last && pattern[pattern_first] == replace_many)
  {
    pattern_first++;
  }

  return (name_first > name_last && pattern_first > pattern_last);
}

//----------------------------------------------------------------------------

bool _wintern_name_match_pattern (wstring name,
                                  wstring pattern,
                                  wchar   replace_one,
                                  wchar   replace_many,
                                  int     name_first0,
                                  int     name_last0,
                                  int     pattern_first0,
                                  int     pattern_last0)
{
  int i;
  int name_first    = name_first0;
  int name_last     = name_last0;
  int pattern_first = pattern_first0;
  int pattern_last  = pattern_last0;

  while (name_first <= name_last && pattern_first <= pattern_last)
  {
    if ((pattern[pattern_first] == name[name_first] && pattern[pattern_first] != replace_many)
        || pattern[pattern_first] == replace_one)
    {
      // eliminate common first characters
      name_first++;
      pattern_first++;
    }
    else if ((pattern[pattern_last] == name[name_last] && pattern[pattern_last] != replace_many)
             || pattern[pattern_last] == replace_one)
    {
      // eliminate common last characters
      name_last--;
      pattern_last--;
    }
    else if (pattern[pattern_first] == replace_many)
    {
      // suppress all 'replace many' characters

      while (pattern_first <= pattern_last && pattern[pattern_first] == replace_many)
      {
        pattern_first++;
      }

      for (i=name_last+1; i>=name_first; i--)
      {
        if (_wintern_name_match_pattern (name, pattern,
                                         replace_one, replace_many,
                                         i, name_last,
                                         pattern_first, pattern_last))
        {
          return true;
        }
      }

      return false;
    }
    else   // in all other cases there's no match
    {
      return false;
    }
  }

  // here, either the name or the pattern is empty, or both

  // suppress all 'replace many' characters

  while (pattern_first <= pattern_last && pattern[pattern_first] == replace_many)
  {
    pattern_first++;
  }

  return (name_first > name_last && pattern_first > pattern_last);
}

//----------------------------------------------------------------------------

// test if a name matches a pattern containing ? and * characters.

public bool name_matches_pattern (string name,
                                  string pattern,
                                  char   replace_one  = '?',
                                  char   replace_many = '*')
{
  return _intern_name_match_pattern (name, pattern,
                                     replace_one, replace_many,
                                     0, strlen(name)-1,
                                     0, strlen(pattern)-1);
}

public bool wname_matches_pattern (wstring name,
                                   wstring pattern,
                                   wchar   replace_one  = L'?',
                                   wchar   replace_many = L'*')
{
  return _wintern_name_match_pattern (name, pattern,
                                      replace_one, replace_many,
                                      0, wstrlen(name)-1,
                                      0, wstrlen(pattern)-1);
}

//----------------------------------------------------------------------------

#if WINDOWS
void convert_mask (    string mask,
                   out string filter)
{
  int len = strlen(mask);
  int i, j;

  clear filter;
  i = 0;
  j = 0;
  while (i < len)
  {
    while (mask[i] != '=')
      filter[j++] = mask[i++];
    i++;
    filter[j++] = nul;
    while (mask[i] != '\n')
      filter[j++] = mask[i++];
    i++;
    filter[j++] = nul;
  }
  filter[j++] = nul;
  filter[j++] = nul;
}

void wconvert_mask (    wstring mask,
                    out wstring filter)
{
  int len = wstrlen(mask);
  int i, j;

  clear filter;
  i = 0;
  j = 0;
  while (i < len)
  {
    while (mask[i] != L'=')
      filter[j++] = mask[i++];
    i++;
    filter[j++] = Lnul;
    while (mask[i] != L'\n')
      filter[j++] = mask[i++];
    i++;
    filter[j++] = Lnul;
  }
  filter[j++] = Lnul;
  filter[j++] = Lnul;
}
#endif //  WINDOWS

//----------------------------------------------------------------------------

public
int select_filename (    string  dialog_title,
                         string  mask,
                         bool    saving,     // false=load, true=save
                     out char    filename[MAX_FILENAME_LENGTH],
                         string  default_filename = "",
                         long    hwnd = 0)
{

#if WINDOWS

#begin unsafe
  char title[260], filter[500];
  OPENFILENAMEA o;

  sprintf (out title, "%.259s", dialog_title);
  convert_mask (mask, out filter);
  strncpy (out filename, default_filename, filename'length);

  clear o;
  o.lStructSize = o'size;
  o.hwndOwner   = (hwnd == 0) ? main_hWnd : (HWND)hwnd;
  o.lpstrFilter = &filter;
  o.lpstrFile   = &filename;
  o.nMaxFile    = filename'size;
  o.lpstrTitle  = &title;

  if (saving)
  {
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetSaveFileNameA (&o) == 0)
      return -1;
  }
  else
  {
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOTESTFILECREATE
            | OFN_READONLY      | OFN_HIDEREADONLY  | OFN_SHAREAWARE;

    if (GetOpenFileNameA (&o) == 0)
      return -1;
  }

  return 0;
#end unsafe

#elif ANDROID
  _unused dialog_title, mask, saving, default_filename, hwnd;
  clear filename;
  return -1;
  
#endif  
}


public
int wselect_filename (    wstring  dialog_title,
                          wstring  mask,
                          bool    saving,     // false=load, true=save
                      out wchar    filename[MAX_FILENAME_LENGTH],
                          wstring  default_filename = L"",
                          long    hwnd = 0)
{
#if WINDOWS
#begin unsafe
  wchar title[260], filter[500];
  OPENFILENAMEW o;

  wsprintf (out title, L"%.259S", dialog_title);
  wconvert_mask (mask, out filter);
  wstrncpy (out filename, default_filename, filename'length);

  clear o;
  o.lStructSize = o'size;
  o.hwndOwner   = (hwnd == 0) ? main_hWnd : (HWND)hwnd;
  o.lpstrFilter = &filter;
  o.lpstrFile   = &filename;
  o.nMaxFile    = filename'size;
  o.lpstrTitle  = &title;

  if (saving)
  {
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetSaveFileNameW (&o) == 0)
      return -1;
  }
  else
  {
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOTESTFILECREATE
            | OFN_READONLY      | OFN_HIDEREADONLY  | OFN_SHAREAWARE;

    if (GetOpenFileNameW (&o) == 0)
      return -1;
  }

  return 0;
#end unsafe

#elif ANDROID
  _unused dialog_title, mask, saving, default_filename, hwnd;
  clear filename;
  return -1;
  
#endif  
}

//----------------------------------------------------------------------------

// returns 0 if OK, or a negative error code.

#if WINDOWS
public int wget_full_filename (int handle, out wstring(MAX_FILENAME_LENGTH) filename)
{
#begin unsafe
  wstring(MAX_FILENAME_LENGTH+1) fn;
  uint len = GetFinalPathNameByHandleW ((HANDLE)handle, &fn, fn'size, dwFlags => 0);
  if (len == 0)
  {
    clear filename;
    return negative_last_windows_error();
  }

  wstrcpy (out filename, fn);
  return 0;
#end unsafe
}
#endif

#if WINDOWS
public int get_full_filename (int handle, out string(MAX_FILENAME_LENGTH) filename)
{
#begin unsafe
  string(MAX_FILENAME_LENGTH+1) fn;
  uint len = GetFinalPathNameByHandleA ((HANDLE)handle, &fn, fn'size, dwFlags => 0);
  if (len == 0)
  {
    clear filename;
    return negative_last_windows_error();
  }

  strcpy (out filename, fn);
  return 0;
#end unsafe
}
#endif

//----------------------------------------------------------------------------
