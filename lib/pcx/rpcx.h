
/* rpcx.h : PCX image reader */

/**************************************************************************/

use ../stream;

struct PCX;

/**************************************************************************/

/* open a PCX image file */

int open_pcx (out PCX pcx, ref READ_STREAM stream);

/**************************************************************************/

/* returns width and height of open pcx image */

void get_pcx_size (PCX      pcx,
                   out uint width,
                   out uint height);

/**************************************************************************/

/* returns the image pixels as 'RGBs' (4 bytes).          */
/* The bytes 'RGB' indicate the amount of RED,GREEN,BLUE  */
/* of the pixel; the byte 's' is reserved for future use. */
/* 'buffer'size' must be a multiple of 4.                 */
/* Image pixels are read left to right, line by line.     */
/* returns 0 if OK, or a negative error code.             */

int read_pcx (ref PCX pcx, out byte[] buffer, ref READ_STREAM stream);

/**************************************************************************/

int close_pcx (ref PCX pcx);

/**************************************************************************/

const int PCX_UNSUPPORTED_ENCODING  = (-10);   /* only RLE encoding */
const int PCX_PLANES_NOT_SUPPORTED  = (-11);   /* only 1 or >3 planes supported */
const int PCX_BITS_NOT_SUPPORTED    = (-12);   /* only 1 or 8 bits/pixel supported */
const int PCX_UNSUPPORTED_PALETTE   = (-13);   /* palette not found */
const int PCX_UNEXPECTED_EOF        = (-14);
const int PCX_FORMAT_ERROR          = (-15);   /* decompression error */

/**************************************************************************/
