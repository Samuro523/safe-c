
/* bmp2.h : BMP image writer */

use ../stream;

struct BMP;

/*************************************************************************/

/* create a 24-bit BMP image file */

int create_bmp (out BMP          bmp,
                ref WRITE_STREAM stream,
                    uint         width,
                    uint         height);

/**************************************************************************/

/* returns width and height of image */

void get_bmp_size2 (BMP      bmp,
                    out uint width,
                    out uint height);

/**************************************************************************/

/* provide the image pixels as 'RGBs' (4 bytes).          */
/* The bytes 'RGB' indicate the amount of RED,GREEN,BLUE  */
/* of the pixel; the byte 's' is reserved for future use. */
/* 'buffer'size' must be a multiple of 4.                 */
/* Image pixels are written left to right, line by line.  */
/* returns 0 if OK, or a negative error code.             */

int write_bmp (ref BMP bmp, byte[] buffer, ref WRITE_STREAM stream);

/**************************************************************************/

int close_bmp2 (ref BMP bmp);

/**************************************************************************/

const int BMP2_OUT_OF_SPACE           =  (-1);
const int BMP2_CANNOT_CREATE          =  (-2);
const int BMP2_IO_ERROR               =  (-3);
const int BMP2_ILLEGAL_WIDTH          =  (-4);
const int BMP2_ILLEGAL_HEIGHT         =  (-5);
const int BMP2_NOT_OPEN               =  (-7);
const int BMP2_ILLEGAL_BUFFER_SIZE    =  (-8);
const int BMP2_TOO_MUCH_DATA          =  (-9);
const int BMP2_SEEK_ERROR             = (-10);
const int BMP2_INCOMPLETE             = (-11);

/*************************************************************************/
