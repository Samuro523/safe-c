
/* zip.h : packing/unpacking using Deflate+Huffman Algorithm */

use calendar;

//-------------------------------------------------------------------------
// 0) standard zip container
//-------------------------------------------------------------------------

struct ZIP_CONTAINER;

int zip_create_container  (out ZIP_CONTAINER zip, string zip_filename);
int wzip_create_container (out ZIP_CONTAINER zip, wstring zip_filename);

// store or deflate method
// file: stores content of file.
// name: filename to store in zip directory (must be relative, not starting with /, and contain no drive)
int zip_add_file          (ref ZIP_CONTAINER zip, string file,  string name, bool always_store = false);
int wzip_add_file         (ref ZIP_CONTAINER zip, wstring file, wstring name, bool always_store = false);

int zip_close_container   (ref ZIP_CONTAINER zip);

//----------

struct UNZIP_CONTAINER;

int unzip_open_container (out UNZIP_CONTAINER uzip, wstring zip_filename);
int unzip_number_of_files (UNZIP_CONTAINER uzip);

// index : 0 .. unzip_number_of_files()-1
int unzip_get_file_size (ref UNZIP_CONTAINER uzip, int index, out long size);
int unzip_get_file_datetime (ref UNZIP_CONTAINER uzip, int index, out DATE_TIME datetime);
int unzip_get_filename (ref UNZIP_CONTAINER uzip, int index, out wstring(260) filename);

// 0 = none(store), 8 = deflate, 9 = deflate64
int unzip_get_compression_method (ref UNZIP_CONTAINER uzip, int index, out int compression_method);

// supports store, deflate & deflate64
int unzip_extract_file (ref UNZIP_CONTAINER uzip, int index, wstring filename);

int unzip_close_container (ref UNZIP_CONTAINER uzip);


//-------------------------------------------------------------------------
// 1) non-standard format to compress/uncompress a memory block
//-------------------------------------------------------------------------

// compressed block can become at most 1 byte larger than source block
void compress_block (    byte[] block_in,
                     out byte[] block_out,
                     out int    block_out_size);


// block_out_size becomes zero in case of bad compression format
void decompress_block (    byte[] block_in,
                       out byte[] block_out,
                       out int    block_out_size);


// returns null if decompression failed
byte[]^ decompress_and_allocate_block (byte[] block_in);


//-------------------------------------------------------------------------
// 2) non-standard format to compress/uncompress a file
//-------------------------------------------------------------------------

// returns 0 if OK, -1 if i/o or format error.
int compress_file   (string source, string target);
int decompress_file (string source, string target);


//-------------------------------------------------------------------------
// 3) low-level routines for standard deflate algorithm with callback functions
//-------------------------------------------------------------------------

const int UNPACK_MAX_READ_AHEAD = 32768;

//--------------------------------------------------------------------------

struct READ_AHEAD
{
  uint size;      /* nb bytes in 'buffer' */
  byte buffer[UNPACK_MAX_READ_AHEAD];
}

//--------------------------------------------------------------------------

/* 'user_info' can be used to pass data to the user's i/o functions */

generic <USER_INFO>
package PACK

  //--------------------------------------------------------------------

  /* user-provided read function.                 */
  /* must return the number of bytes transferred, */
  /* zero in case of end-of-data,                 */
  /* or a negative value in case of error.        */

  typedef int IO_READ (ref USER_INFO user_info,
                       out byte[]    buffer);

  //--------------------------------------------------------------------

  /* user-provided write function.                */
  /* must return the number of bytes transferred, */
  /* or a negative value in case of error.        */

  typedef int IO_WRITE (ref USER_INFO user_info,
                            byte[]    buffer);

  //--------------------------------------------------------------------

  int pack (ref USER_INFO user_info, IO_READ user_read, IO_WRITE user_write);

  //--------------------------------------------------------------------

end PACK;

//--------------------------------------------------------------------

/* pack error codes: */
const int PK_READ_ERROR  = (-1);       /* error in user_read */
const int PK_WRITE_ERROR = (-2);       /* error in user_write */
const int PK_STACK_ERROR = (-3);       /* out of stack space */
const int PK_INTERN_1    = (-4);       /* intern error (should never occur) */

//--------------------------------------------------------------------------


//--------------------------------------------------------------------------

/* 'user_info' can be used to pass data to the user's i/o functions */

generic <USER_INFO>
package UNPACK

  //--------------------------------------------------------------------

  /* user-provided read function.                 */
  /* must return the number of bytes transferred, */
  /* zero in case of end-of-data,                 */
  /* or a negative value in case of error.        */

  typedef int IO_READ (ref USER_INFO user_info,
                       out byte[]    buffer);

  //--------------------------------------------------------------------

  /* user-provided write function.                */
  /* must return the number of bytes transferred, */
  /* or a negative value in case of error.        */

  typedef int IO_WRITE (ref USER_INFO user_info,
                            byte[]    buffer);

  //--------------------------------------------------------------------

  /* note: unpack() reads data in large blocks and will probably read  */
  /*       past the compression part. The data read but not used by    */
  /*       unpack will be returned in the structure 'extra'.           */

  int unpack (ref USER_INFO  user_info,
                  IO_READ    user_read,
                  IO_WRITE   user_write,
              out READ_AHEAD extra,
                  bool       deflate_64 = false);  // false=deflate, true=deflate64 algorithm

  //--------------------------------------------------------------------

end UNPACK;

//--------------------------------------------------------------------

/* unpack error codes: */
const int UNPK_READ_ERROR      =  (-1);   /* error in user_read */
const int UNPK_WRITE_ERROR     =  (-2);   /* error in user_write */
const int UNPK_STACK_ERROR     =  (-3);   /* out of stack space */
const int UNPK_BAD_BTYPE       =  (-4);   /* bad field BTYPE */
const int UNPK_BAD_NLEN        =  (-5);   /* bad field NLEN */
const int UNPK_HUFF_OVERFLOW   =  (-6);   /* bad format */
const int UNPK_HUFF_BAD_STRUCT =  (-7);   /* bad format */
const int UNPK_HUFF_BAD_CODE   =  (-8);   /* bad format */
const int UNPK_NO_PREV_CODE    =  (-9);   /* bad format */
const int UNPK_TOO_MANY        = (-10);   /* bad format */
const int UNPK_BAD_CODE        = (-11);   /* bad format */

const int UNPK_CLOSE_FAILED           = (-12);
const int UNPK_READ_ENTRY_FAILED      = (-13);
const int UNPK_SEEK_READ_FAILED       = (-14);
const int UNPK_NOT_ZIP                = (-15);  // not zip file
const int UNPK_TOO_MANY_ITEMS         = (-16);
const int UNPK_BAD_INDEX              = (-17);
const int UNPK_CREATE_FILE_FAILED     = (-18);
const int UNPK_SEEK_READ_WRITE_FAILED = (-19);
const int UNPK_UNSUPPORTED_COMPRESSION_METHOD = (-20);
const int UNPK_BAD_CRC                = (-21);
const int UNPK_BAD_FILESIZE           = (-22);

//--------------------------------------------------------------------
