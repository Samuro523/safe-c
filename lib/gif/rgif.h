
/* rgif.h : GIF image reader */

use ../stream;
use ../image;    // for IMAGE_OPTIONS, IMAGE_ATTRIBUTES

/**************************************************************************/

struct GIF;

/**************************************************************************/

/* open a GIF image file */

int open_gif (out GIF gif, ref READ_STREAM stream);

/**************************************************************************/

/* returns width and height of open gif image */

void get_gif_size (GIF      gif,
                   out uint width,
                   out uint height);

/**************************************************************************/

/* returns the image pixels as 'RGBs' (4 bytes).          */
/* The bytes 'RGB' indicate the amount of RED,GREEN,BLUE  */
/* of the pixel; the byte 's' is reserved for future use. */
/* 'buffer'size' must be a multiple of 4.                 */
/* Image pixels are read left to right, line by line.     */
/* returns 0 if OK, or a negative error code.             */

int read_gif (ref GIF gif, out byte[] buffer, ref READ_STREAM stream);

/**************************************************************************/

void get_gif_attributes (GIF                  gif,
                         out IMAGE_ATTRIBUTES attr);

/**************************************************************************/

/* returns 0 if next image follows, +1 if no image follows, -1 if error */

int next_gif (ref GIF gif, ref READ_STREAM stream);

/**************************************************************************/

int close_gif (ref GIF gif);

/**************************************************************************/

const int GIF_BAD_CODE_SIZE        = (-12);
const int GIF_UNEXPECTED_EOF       = (-13);  /* unexpected code (end-of-image) */
const int GIF_TOO_MANY_PIXELS      = (-14);  /* corrupted file */
const int GIF_CORRUPTED_FILE       = (-15);
const int GIF_BAD_FORMAT           = (-16);  /* no pixels found */
const int GIF_INTERN_ERROR         = (-17);  /* should never happen */
const int GIF_IMAGE_OUTSIDE_SCREEN = (-18);  /* bad width/height offset or size */
const int GIF_END_OF_RASTER_DATA   = (-19);
const int GIF_TRAILER              = (-20);  /* end of file reached */

/**************************************************************************/
