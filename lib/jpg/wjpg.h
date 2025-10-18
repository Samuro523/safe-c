
/* wjpg.h : JPG image writer */

/**************************************************************************/

use ../stream;

struct JPG;

/**************************************************************************/

/* create a JPG image file */
/* quality : 0 = small file, 100 = large file.        */
/* output_grey : 0 = output color, 1 = output grey.   */

int create_jpg (out JPG          jpg,
                ref WRITE_STREAM stream,
                    ushort       width,
                    ushort       height,
                    ushort       quality,       /* 0..100 : 0=WORSE, 100=BEST */
                    bool         output_grey,   /* false = color, true = grey */
                    bool         output_alpha,  /* true = save also alpha channel, this creates a non-standard jpeg ! */
                    uint         x_resolution,  /* 0 = undefined */
                    uint         y_resolution); /* 0 = undefined */

/**************************************************************************/

/* returns width and height of open jpg image */

void get_jpg_size2 (JPG      jpg,
                    out uint width,
                    out uint height);

/**************************************************************************/

/* provide the image pixels as 'RGBs' (4 bytes).          */
/* The bytes 'RGB' indicate the amount of RED,GREEN,BLUE  */
/* of the pixel; the byte 's' is reserved for future use. */
/* 'buffer'size' must be a multiple of 4.                 */
/* Image pixels are written left to right, line by line.  */
/* returns 0 if OK, or a negative error code.             */

int write_jpg (ref JPG jpg, byte[] buffer);

/**************************************************************************/

int close_jpg2 (ref JPG jpg);

/**************************************************************************/

const int JPG_FILE_NOT_FOUND      = (-1);   /* cannot open/create file */

const int JPG_IO_ERROR            = (-3);   /* file read/write error or corrupted */

const int JPG_ILLEGAL_BUFFER_SIZE = (-5);   /* size must be multiple of 4 */
const int JPG_IO_END_OF_IMAGE     = (-6);   /* no more pixel data to read */
const int JPG_NOT_OPEN            = (-7);
const int JPG_BAD_PARAMETER       = (-8);   /* bad color or quality parameter */
const int JPG_TOO_MUCH_DATA       = (-9);   /* too many bytes provided to write_jpg() */
const int JPG_LIBRARY_ERROR       = (-10);
const int JPG_SHORT_FILE          = (-11);
const int JPG_BAD_COMPONENTS      = (-12);

/**************************************************************************/
