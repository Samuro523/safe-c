
// stream.h : intern stream layer used for image layer

//-------------------------------------------------------------------------

enum SEEK_MODE {
  SEEK_SET,  // absolute position
  SEEK_CUR,  // relative from current position
  SEEK_END,  // relative from end of file
};

//-------------------------------------------------------------------------

struct READ_STREAM;

int open_file_stream  (out READ_STREAM stream, string  filename);
int wopen_file_stream (out READ_STREAM stream, wstring filename);
void open_memory_stream (out READ_STREAM stream, byte[] memory);

bool is_ropen (READ_STREAM stream);

int read (ref READ_STREAM stream, out byte[] buffer);
long lseekr (ref READ_STREAM stream, long offset, SEEK_MODE mode = SEEK_SET);

void rclose (ref READ_STREAM stream);

//-------------------------------------------------------------------------

struct WRITE_STREAM;

int create_file_stream  (out WRITE_STREAM stream, string  filename);
int wcreate_file_stream (out WRITE_STREAM stream, wstring filename);
void create_memory_stream (out WRITE_STREAM stream);

bool is_wopen (WRITE_STREAM stream);

int write (ref WRITE_STREAM stream, byte[] buffer);
long lseekw (ref WRITE_STREAM stream, long offset, SEEK_MODE mode = SEEK_SET);

// returns null for file stream
byte[]^ wclose_and_get_memory_stream (ref WRITE_STREAM stream, ref int rc);

//-------------------------------------------------------------------------
