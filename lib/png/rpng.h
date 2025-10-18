
/* rpng.h : PNG image reader */

/**************************************************************************/

use ../stream;
use ../image;

/**************************************************************************/

struct PNG;

/**************************************************************************/

/* open a PNG image file */

int open_png (out PNG png, ref READ_STREAM stream);

/**************************************************************************/

/* returns width and height of open png image */

void get_png_size (PNG      png,
                   out uint width,
                   out uint height);

/**************************************************************************/

/* returns the image pixels as 'RGBs' (4 bytes).          */
/* The bytes 'RGB' indicate the amount of RED,GREEN,BLUE  */
/* of the pixel; the byte 's' is reserved for future use. */
/* 'buffer'size' must be a multiple of 4.                 */
/* Image pixels are read left to right, line by line.     */
/* returns 0 if OK, or a negative error code.             */

int read_png (ref PNG png, out byte[] buffer);

/**************************************************************************/

void get_png_attributes (PNG                  png,
                         out IMAGE_ATTRIBUTES attr);

/**************************************************************************/

int close_png (ref PNG png);

/**************************************************************************/

const int PNG_BAD_HEADER          = (-12);
const int PNG_BAD_DIMENSIONS      = (-13);
const int PNG_BAD_CHUNK_CRC       = (-14);
const int PNG_LONG_CHUNK          = (-15);  /* chunk exceeds 256 MB */
const int PNG_BAD_PALETTE_LENGTH  = (-16);
const int PNG_MISSING_PALETTE     = (-17);
const int PNG_BAD_TRNS_LENGTH     = (-18);
const int PNG_BAD_BKGD_LENGTH     = (-19);
const int PNG_BAD_BKGD_VALUE      = (-20);
const int PNG_BAD_CHUNK_ORDER     = (-21);
const int PNG_NOT_DEFLATE         = (-22);
const int PNG_BAD_ADLER_CRC       = (-23);
const int PNG_MISSING_DATA        = (-24);
const int PNG_TOO_MUCH_DATA       = (-25);
const int PNG_CORRUPT             = (-26);
const int PNG_TOO_MUCH_DATA2      = (-27);
const int PNG_TOO_MUCH_DATA3      = (-28);
const int PNG_BAD_FCHECK          = (-29);
const int PNG_TOO_MUCH_DATA4      = (-30);

/**************************************************************************/
