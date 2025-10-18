
/* wtiff.h : TIFF image writer */

/*************************************************************************/

use ../stream;

struct TIFF;

/*************************************************************************/

/* create a TIFF image file */

int create_tiff (out TIFF         tiff,
                 ref WRITE_STREAM stream,
                     uint2        width,
                     uint2        height,
                     uint         x_resolution,  /* 0 = undefined */
                     uint         y_resolution);

/**************************************************************************/

/* returns width and height of image */

void get_tiff_size2 (TIFF     tiff,
                     out uint width,
                     out uint height);

/**************************************************************************/

/* provide the image pixels as 'RGBs' (4 bytes).          */
/* The bytes 'RGB' indicate the amount of RED,GREEN,BLUE  */
/* of the pixel; the byte 's' is reserved for future use. */
/* 'buffer'size' must be a multiple of 4.                 */
/* Image pixels are written left to right, line by line.  */
/* returns 0 if OK, or a negative error code.             */

int write_tiff (ref TIFF tiff, byte[] buffer, ref WRITE_STREAM stream);

/**************************************************************************/

int close_tiff2 (ref TIFF tiff, ref WRITE_STREAM stream);

/**************************************************************************/

const int TIFF_ILLEGAL_WIDTH         = (-30);
const int TIFF_ILLEGAL_HEIGHT        = (-31);
const int TIFF_CANNOT_CREATE         = (-32);
const int TIFF_INCOMPLETE            = (-33);
const int TIFF_TOO_MUCH_DATA         = (-34);
const int TIFF_CLOSE_ERROR           = (-35);
const int TIFF_WRITE_ERROR           = (-36);
const int TIFF_SEEK_ERROR            = (-37);
const int TIFF_INTERN_ERROR          = (-38);

/**************************************************************************/
