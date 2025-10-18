
/* wgif.h : GIF image writer */

/*************************************************************************/

use ../stream;

struct GIF;

/**************************************************************************/

/* create a GIF image file */
/* the user must reserve a variable of type 'void *' */
/* and pass its address to the parameter 'gif' : it  */
/* will be filled with a pointer to intern data.     */

/* 'max_colors' =  2      : black & white,                  */
/*                 3..256 : color drawing / logo / cartoon, */
/*                 0      : webcam / real-world photo.      */

/* 'use_transparency' = 0 : normal,                                 */
/*                      1 : use 4th byte (0=opaque,255=transparent) */

/* 'transparent_RGB' : color used as transparency when loading the gif */

int create_gif (out GIF          gif,
                ref WRITE_STREAM stream,
                    uint2        width,
                    uint2        height,
                    uint2        max_colors,
                    uint2        use_transparency,
                    byte[3]      transparent_RGB);

/**************************************************************************/

/* returns width and height of image */

void get_gif_size2 (GIF      gif,
                    out uint width,
                    out uint height);

/**************************************************************************/

/* provide the image pixels as 'RGBs' (4 bytes).          */
/* The bytes 'RGB' indicate the amount of RED,GREEN,BLUE  */
/* of the pixel; the byte 's' is reserved for future use. */
/* 'buffer'size' must be a multiple of 4.                 */
/* Image pixels are written left to right, line by line.  */
/* returns 0 if OK, or a negative error code.             */

int write_gif (ref GIF gif, byte[] buffer, ref WRITE_STREAM stream);

/**************************************************************************/

int close_gif2 (ref GIF gif);

/**************************************************************************/

const int GIF2_OUT_OF_SPACE             =  (-1);
const int GIF2_CANNOT_CREATE            =  (-2);
const int GIF2_IO_ERROR                 =  (-3);
const int GIF2_ILLEGAL_WIDTH            =  (-4);
const int GIF2_ILLEGAL_HEIGHT           =  (-5);
const int GIF2_ILLEGAL_BITS_PER_PIXEL   =  (-6);
const int GIF2_NOT_OPEN                 =  (-7);
const int GIF2_ILLEGAL_BUFFER_SIZE      =  (-8);
const int GIF2_TOO_MUCH_DATA            =  (-9);
const int GIF2_SEEK_ERROR               = (-10);
const int GIF2_INCOMPLETE               = (-11);
const int GIF2_ILLEGAL_MAX_COLORS       = (-12);

/*************************************************************************/
