
// files.h : text and binary files, directories and disk volumes.

//----------------------------------------------------------------------------
// Text Files : functions for reading or writing buffered text files.
//----------------------------------------------------------------------------

struct FILE;  // the same FILE must not be used by several threads at the same time.

enum ENCODING {ANSI, UTF8, UTF16, UTF16BE};

//----------------------------------------------------------------------------

// open a text file for reading
// returns 0 if OK, a negative value if error.

int fopen  (out FILE file, string  filename);
int wfopen (out FILE file, wstring filename);

//----------------------------------------------------------------------------

// creating a text file for writing.
// returns 0 if OK, a negative value if error.

int fcreate  (out FILE file, string  filename, ENCODING encoding);
int wfcreate (out FILE file, wstring filename, ENCODING encoding);

//----------------------------------------------------------------------------

// open or create a file for writing at the end.
// returns 0 if OK, a negative value if error.

int fappend  (out FILE file, string  filename, ENCODING encoding);
int wfappend (out FILE file, wstring filename, ENCODING encoding);

//----------------------------------------------------------------------------

const int FEOF = +1;  // return value that indicates end of file

//----------------------------------------------------------------------------

// read single char from stream.
// returns 0 if OK, FEOF(+1) if end-of-file, a negative value if read error.
// a runtime error occurs if file is invalid.

int fgetc (ref FILE file, out char c);
int wfgetc (ref FILE file, out wchar c);

//----------------------------------------------------------------------------

// write single char to stream.
// returns 0 if OK, a negative value if error.
// a runtime error occurs if file is invalid.

int fputc (ref FILE file, char c);
int wfputc (ref FILE file, wchar c);

//----------------------------------------------------------------------------

// read a string from stream, until \n is reached or until the buffer is full.
// The '\n' is included in buffer if read.
// returns 0 if OK, FEOF(+1) if end-of-file, a negative value if read error.
// a runtime error occurs if file is invalid.

int fgets (ref FILE file, out string buffer);
int wfgets (ref FILE file, out wstring buffer);

//----------------------------------------------------------------------------

// write a string to the stream.
// returns 0 if OK, a negative value if error.
// a runtime error occurs if file is invalid.

int fputs (ref FILE file, string buffer);
int wfputs (ref FILE file, wstring buffer);

//----------------------------------------------------------------------------

// read a formatted string from stream.
// returns:
// .  0 if OK
// . +1 if end-of-file.
// . -1 if a read-error occured during GET.
// . -2 if end-of-stream occured before all arguments received a value;
// . -3 if end-of-format string occured before all arguments received a value;
// . -4 if a syntax error occured in the stream;
// . -5 if an overflow occured while reading a numeric value or storing it in a numeric argument.
// a runtime error occurs when file or format are invalid.

int fscanf (ref FILE file, string format, out object[] arg);
int wfscanf (ref FILE file, wstring format, out object[] arg);

//----------------------------------------------------------------------------

// write a formatted string to the stream.
// returns 0 if OK, a negative value if error.
// a runtime error occurs when file or format are invalid.

int fprintf (ref FILE file, string format, object[] arg);
int wfprintf (ref FILE file, wstring format, object[] arg);

//----------------------------------------------------------------------------

// returns true if end-of-file reached, false otherwise.
// a runtime error occurs if file is invalid.

bool feof (ref FILE file);

//----------------------------------------------------------------------------

// flush all written data to disk.
// returns 0 if OK, a negative value if error.
// a runtime error occurs if file is invalid.

int fflush (ref FILE file);

//----------------------------------------------------------------------------

// flush all written data to disk and closes the file.
// returns 0 if OK, a negative value if error.
// a runtime error occurs if file is invalid.

int fclose (ref FILE file);

//----------------------------------------------------------------------------

// returns binary file handle.
// a runtime error occurs if file is invalid.

int fhandle (ref FILE file);

//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Binary Files : functions that operate on low-level binary files.
//----------------------------------------------------------------------------

const int MAX_FILENAME_LENGTH = 260;

// access and share constants (can be combined with |).
const uint READ   = 1;
const uint WRITE  = 2;
const uint DELETE = 4;  // for share : other fd can delete the file or set delete_on_close

// caching hint (improves performance)
enum ORGANIZATION {UNDEFINED, SEQUENTIAL, RANDOM};

//----------------------------------------------------------------------------

// create a new file (deletes any existing file of the same name)
// on android : parameter 'share' simply creates a shared lock if share != 0,
//              and an exclusive lock if share == 0.
//              so the file cannot be opened again.
// on android : parameter 'delete_on_close' erases the filename right away,
//              so the file cannot be opened again.

// returns a file handle, or a negative error code.
int create  (string       filename,
             uint         access          = WRITE,
             uint         share           = READ,   // 0 = no one else can open this file
             bool         error_if_exists = false,
             bool         delete_on_close = false,
             ORGANIZATION organization    = UNDEFINED);

int wcreate (wstring      filename,
             uint         access          = WRITE,
             uint         share           = READ,
             bool         error_if_exists = false,
             bool         delete_on_close = false,
             ORGANIZATION organization    = UNDEFINED);

// open an existing file.
// returns a file handle, or a negative error code.
int open  (string       filename,
           uint         access               = READ,
           uint         share                = READ,
           bool         create_if_not_exists = false,
           bool         delete_on_close      = false,
           ORGANIZATION organization         = UNDEFINED);

int wopen (wstring      filename,
           uint         access               = READ,
           uint         share                = READ,
           bool         create_if_not_exists = false,
           bool         delete_on_close      = false,
           ORGANIZATION organization         = UNDEFINED);

// test if file exists
bool exists  (string  filename);
bool wexists (wstring filename);

//----------------------------------------------------------------------------

// returns nb of bytes read (>=0), or a negative error code.
int read (int handle, out byte[] buffer);

// returns nb of bytes written(>=0), or a negative error code.
int write (int handle, byte[] buffer);

//----------------------------------------------------------------------------

enum SEEK_MODE {
  SEEK_SET,  // absolute position
  SEEK_CUR,  // relative from current position
  SEEK_END,  // relative from end of file
};

// set file position.
// returns current file position, or a negative error code.
long lseek (int handle, long offset, SEEK_MODE mode = SEEK_SET);

// returns file size, or a negative error code.
long filesize (int handle);

//----------------------------------------------------------------------------

// file must be open in read access
// get time of last write access, convert with calendar.clock_to_datetime()
void get_ftime (int handle, out long time);

// file must be open in write access
// set time of last write access, convert with calendar.datetime_to_clock()
void set_ftime (int handle, long time);

//----------------------------------------------------------------------------

enum LOCK {
  SHARED,    // all processes, including the caller, can only read the locked file portion.
  EXCLUSIVE, // only the caller can access the locked file portion, other processes can neither read nor write.
};

// returns 0 if OK, or a negative error code.
// Windows: locks apply per handle, Android: locks apply per process.
int lock_file   (int handle, LOCK lock, long offset, long nb_bytes);
int unlock_file (int handle, LOCK lock, long offset, long nb_bytes);

//----------------------------------------------------------------------------

// forces a write of all file buffers to physical disk (for commiting a database transaction)
int flush (int handle);

//----------------------------------------------------------------------------

// returns 0 if OK, or a negative error code.
int close (int handle);

//----------------------------------------------------------------------------

#if WINDOWS   // exists on Windows only
// returns 0 if OK, or a negative error code.
int get_full_filename  (int handle, out string (MAX_FILENAME_LENGTH) filename);
int wget_full_filename (int handle, out wstring(MAX_FILENAME_LENGTH) filename);
#endif

//----------------------------------------------------------------------------

// rename or move file to a different directory but on the same disk.
// the target must not already exist.
// returns 0 if OK, or a negative error code.
int move_file  (string  source, string  target);
int wmove_file (wstring source, wstring target);

// returns 0 if OK, or a negative error code.
int copy_file  (string  source, string  target, bool overwrite = true);
int wcopy_file (wstring source, wstring target, bool overwrite = true);

// returns 0 if OK, or a negative error code.
int delete_file  (string  filename);
int wdelete_file (wstring filename);

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

void split_pathname (    string pathname,
                     out string device,
                     out string directory,  // starts with / or \ if absolute path
                     out string filename);   // does not contain any / or \

void wsplit_pathname (    wstring pathname,
                      out wstring device,
                      out wstring directory,  // starts with / or \ if absolute path
                      out wstring filename);   // does not contain any / or \

//----------------------------------------------------------------------------

// compute a filename from :
// - a current directory,
// - an absolute or relative filename.
// normalizes the filename.
// note: source and result filenames can denote the same buffer.
// returns 0 if OK, -1 if result_filename'length is too small.

int expand_pathname (    string current_directory,
                         string source_filename,
                     out string result_filename);

int wexpand_pathname (    wstring current_directory,
                          wstring source_filename,
                      out wstring result_filename);

//----------------------------------------------------------------------------

// replace "\" by "/" except first one after \\computer
// replace "//" by "/"
// normalize ".", "..", "...", .. path components.

void normalize_pathname  (ref string  pathname);
void wnormalize_pathname (ref wstring pathname);

//----------------------------------------------------------------------------

// test if a name matches a pattern containing ? and * characters.

bool name_matches_pattern (string name,
                           string pattern,
                           char   replace_one  = '?',
                           char   replace_many = '*');

bool wname_matches_pattern (wstring name,
                            wstring pattern,
                            wchar   replace_one  = L'?',
                            wchar   replace_many = L'*');

//----------------------------------------------------------------------------

// show a dialog box allowing the user to select a filename
// warning: this can change the current disk and current directory.

// for mask use :
// "images=*.jpg;*.bmp;*.gif\n" +
// "sounds=*.wav;*.mp3\n"

int select_filename (    string  dialog_title,
                         string  mask,
                         bool    saving,     // false=load, true=save
                     out char    filename[MAX_FILENAME_LENGTH],
                         string  default_filename = "",
                         long    hwnd = 0);  // parent window handle

int wselect_filename (    wstring dialog_title,
                          wstring mask,
                          bool    saving,     // false=load, true=save
                      out wchar   filename[MAX_FILENAME_LENGTH],
                          wstring default_filename = L"",
                          long    hwnd = 0);  // parent window handle

//----------------------------------------------------------------------------
// Directories
//----------------------------------------------------------------------------

// file type :
const uint2 TYPE_REGULAR_FILE = 1;
const uint2 TYPE_DIRECTORY    = 2;

struct _FILE_INFO;   // private structure

struct FILE_INFO
{
  char                name[MAX_FILENAME_LENGTH];
  wchar               wname[MAX_FILENAME_LENGTH];
  uint2               type;       // TYPE_REGULAR_FILE, TYPE_DIRECTORY, or others.
  long                time;       // time of last write access, convert with calendar.clock_to_datetime()
  long                size;       // file size in bytes
  _FILE_INFO          intern;     // private structure
}

const int NO_MORE_FILES = -1;

//----------------------------------------------------------------------------

// returns 0 if OK, or a negative error code if the file or folder was not found.
int get_file_information  (string  filename, out FILE_INFO info);
int wget_file_information (wstring filename, out FILE_INFO info);

//----------------------------------------------------------------------------

// returns 0 if OK, NO_MORE_FILES if directory is empty, or a negative error code.
// for filter, you can specify masks like "*.txt" to retrieve only filenames ending with .txt
int open_directory  (out FILE_INFO info, string  directory_name, string filter = "");
int wopen_directory (out FILE_INFO info, wstring directory_name, wstring filter = L"");

// returns 0 if OK, NO_MORE_FILES if no more files, or a negative error code.
int read_directory  (ref FILE_INFO info);

// returns 0 if OK, or a negative error code.
int close_directory (ref FILE_INFO info);

//----------------------------------------------------------------------------

// returns 0 if OK, or a negative error code.
int create_directory  (string  directory_name);
int wcreate_directory (wstring directory_name);

// returns 0 if OK, or a negative error code.
int remove_directory  (string  directory_name);
int wremove_directory (wstring directory_name);

void get_current_directory  (out char  directory_name[MAX_FILENAME_LENGTH]);
void wget_current_directory (out wchar directory_name[MAX_FILENAME_LENGTH]);

// returns 0 if OK, or a negative error code.
int set_current_directory  (string  directory_name);
int wset_current_directory (wstring directory_name);


//----------------------------------------------------------------------------
// Disk Volumes
//----------------------------------------------------------------------------

long free_disk_space  (string  directory_name);   // any directory on the disk
long wfree_disk_space (wstring directory_name);   // any directory on the disk

//----------------------------------------------------------------------------

const int E_FILE_NOT_FOUND      = -2;
const int E_PATH_NOT_FOUND      = -3;
const int E_TOO_MANY_OPEN_FILES = -4;
const int E_ACCESS_DENIED       = -5;
const int E_INVALID_HANDLE      = -6;
const int E_FILE_IN_USE         = -32;
const int E_FILE_LOCKED         = -33;

//----------------------------------------------------------------------------
