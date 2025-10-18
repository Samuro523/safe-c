
/* wpng.h : PNG image writer */

/*************************************************************************/

use ../stream;

struct PNG;

/*************************************************************************/

/* create a PNG image file */

/* 'max_colors' =  0      : full rgb color (DEFAULT)        */
/*                 2      : black & white,                  */
/*                 3..256 : color drawing / logo / cartoon  */

/* 'use_transparency' = 0 : don't save 4th byte.                         */
/*                      1 : save 4th byte (0=opaque, 255=transparent)    */
/*                      2 : save 4th byte (0..255 = transparency factor) */

/* 'transparent_RGB' : color used for pixels of transparency 255 when loading back the png */

int create_png (out PNG          png,
                ref WRITE_STREAM stream,
                    uint         width,
                    uint         height,
                    ushort       max_colors,
                    ushort       use_transparency,
                    byte[3]      transparent_RGB);

/**************************************************************************/

/* returns width and height of image */

void get_png_size2 (PNG png,
                    out uint width,
                    out uint height);

/**************************************************************************/

/* provide the image pixels as 'RGBs' (4 bytes).          */
/* The bytes 'RGB' indicate the amount of RED,GREEN,BLUE  */
/* of the pixel; the byte 's' is reserved for future use. */
/* 'buffer'size' must be a multiple of 4.                 */
/* Image pixels are written left to right, line by line.  */
/* returns 0 if OK, or a negative error code.             */

int write_png (ref PNG png, byte[] buffer);

/**************************************************************************/

int close_png2 (ref PNG png);

/**************************************************************************/

const int PNG2_OUT_OF_SPACE             = (-1);
const int PNG2_CANNOT_CREATE            = (-2);
const int PNG2_IO_ERROR                 = (-3);
const int PNG2_ILLEGAL_WIDTH            = (-4);
const int PNG2_ILLEGAL_HEIGHT           = (-5);
const int PNG2_NOT_OPEN                 = (-7);
const int PNG2_ILLEGAL_BUFFER_SIZE      = (-8);
const int PNG2_TOO_MUCH_DATA            = (-9);
const int PNG2_INCOMPLETE              = (-11);
const int PNG2_ILLEGAL_MAX_COLORS      = (-12);
const int PNG2_PACK_ERROR              = (-13);

/*************************************************************************/
