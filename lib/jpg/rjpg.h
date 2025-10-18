
/* jpg.h : JPG image reader */

/**************************************************************************/

use ../stream;
use ../image;    // for IMAGE_ATTRIBUTES

/**************************************************************************/

struct JPG;

/**************************************************************************/

/* open a JPG image file */

int open_jpg (out JPG jpg, ref READ_STREAM stream);

/**************************************************************************/

/* returns width and height of open jpg image */

void get_jpg_size (JPG      jpg,
                   out uint width,
                   out uint height);

/**************************************************************************/

/* returns the image pixels as 'RGBs' (4 bytes).          */
/* The bytes 'RGB' indicate the amount of RED,GREEN,BLUE  */
/* of the pixel; the byte 's' indicates the alpha channel.*/
/* 'buffer'size' must be a multiple of 4.                 */
/* Image pixels are read left to right, line by line.     */
/* returns 0 if OK, or a negative error code.             */

int read_jpg (ref JPG jpg, out byte[] buffer);

/**************************************************************************/

void get_jpg_attributes (JPG                  jpg,
                         out IMAGE_ATTRIBUTES attr);

/**************************************************************************/

int close_jpg (ref JPG jpg);

/**************************************************************************/

const int JPG_FILE_NOT_FOUND      = (-1);   /* cannot open/create file */
const int JPG_ILLEGAL_BUFFER_SIZE = (-5);   /* size must be multiple of 4 */
const int JPG_IO_END_OF_IMAGE     = (-6);   /* no more pixel data to read */
const int JPG_NOT_OPEN            = (-7);
const int JPG_BAD_PARAMETER       = (-8);   /* bad color or quality parameter */
const int JPG_TOO_MUCH_DATA       = (-9);   /* too many bytes provided to write_jpg() */
const int JPG_LIBRARY_ERROR       = (-10);
const int JPG_SHORT_FILE          = (-11);
const int JPG_BAD_COMPONENTS      = (-12);

/**************************************************************************/
