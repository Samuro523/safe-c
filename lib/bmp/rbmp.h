
/* rbmp.h : BMP image reader */

use ../stream;

/**************************************************************************/

struct BMP;

/**************************************************************************/

/* open a BMP image file */

int open_bmp (out BMP bmp, ref READ_STREAM stream);

/**************************************************************************/

/* returns width and height of open bmp image */

void get_bmp_size (BMP      bmp,
                   out uint width,
                   out uint height);

/**************************************************************************/

/* returns the image pixels as 'RGBs' (4 bytes).          */
/* The bytes 'RGB' indicate the amount of RED,GREEN,BLUE  */
/* of the pixel; the byte 's' is reserved for future use. */
/* 'buffer'size' must be a multiple of 4.                 */
/* Image pixels are read left to right, line by line.     */
/* returns 0 if OK, or a negative error code.             */

int read_bmp (ref BMP bmp, out byte[] buffer, ref READ_STREAM stream);

/**************************************************************************/

int close_bmp (ref BMP bmp);

/**************************************************************************/

const int BMP_NOT_SUPPORTED     = (-10);
const int BMP_INTERN_ERROR      = (-11);
const int BMP_BITMASK_ERROR     = (-12);
const int BMP_UNEXPECTED_EOF    = (-13);
const int BMP_FORMAT_ERROR      = (-14);
const int BMP_DELTA_UNSUPPORTED = (-15);  /* delta option is not supported */

/**************************************************************************/
