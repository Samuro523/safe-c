
/* rtiff.h : TIF image reader */

use ../stream;
use ../image;   // for IMAGE_ATTRIBUTES

/**************************************************************************/

struct TIFF;

/**************************************************************************/

/* open a TIFF image file */

int open_tiff (out TIFF tiff, ref READ_STREAM stream);

/**************************************************************************/

/* returns width and height of open tiff image */

void get_tiff_size (TIFF     tiff,
                    out uint width,
                    out uint height);

/**************************************************************************/

void get_tiff_attributes (TIFF                 tiff,
                          out IMAGE_ATTRIBUTES attr);

/**************************************************************************/

/* returns the image pixels as 'RGBs' (4 bytes).          */
/* The bytes 'RGB' indicate the amount of RED,GREEN,BLUE  */
/* of the pixel; the byte 's' is reserved for future use. */
/* 'buffer'size' must be a multiple of 4.                 */
/* Image pixels are read left to right, line by line.     */
/* returns 0 if OK, or a negative error code.             */

int read_tiff (ref TIFF tiff, out byte[] buffer, ref READ_STREAM stream);

/**************************************************************************/

int close_tiff (ref TIFF tiff);

/**************************************************************************/

const int TIFF_FORMAT_ERROR     = (-10); /* corrupted file or unsupported code */
const int UNSUPPORTED_OPTION_1  = (-11); /* TAG bits per sample has not type WORD */
const int UNSUPPORTED_OPTION_2  = (-12); /* bits per sample neither len 1 nor 3 */
const int UNSUPPORTED_OPTION_3  = (-13); /* TAG compression is not WORD */
const int UNSUPPORTED_OPTION_4  = (-14); /* TAG photom. interpretation not WORD */
const int UNSUPPORTED_OPTION_5  = (-15); /* TAG fill_order is not WORD */
const int UNSUPPORTED_OPTION_6  = (-16); /* TAG strip offset is neither WORD/LONG */
const int UNSUPPORTED_OPTION_7  = (-17); /* TAG strip offset given twice */
const int UNSUPPORTED_OPTION_8  = (-18); /* TAG strip offset : too many strips */
const int UNSUPPORTED_OPTION_9  = (-19); /* TAG samples per pixel has not type WORD */
const int UNSUPPORTED_OPTION_10 = (-20); /* TAG samples per pixel neither 1 nor 3 */
const int UNSUPPORTED_OPTION_11 = (-21); /* TAG rows per strip neither WORD nor LONG */
const int UNSUPPORTED_OPTION_12 = (-22); /* TAG strip byte count neither WORD nor LONG */
const int UNSUPPORTED_OPTION_13 = (-23); /* TAG strip byte count given twice */
const int UNSUPPORTED_OPTION_14 = (-24); /* TAG strip byte count : bad nb strips */
const int UNSUPPORTED_OPTION_15 = (-25); /* TAG planar configuration is not WORD */
const int UNSUPPORTED_OPTION_16 = (-26); /* TAG group3options not WORD */
const int UNSUPPORTED_OPTION_17 = (-27); /* TAG group4options not WORD */
const int UNSUPPORTED_OPTION_18 = (-28); /* TAG lzw predictor is not WORD */
const int UNSUPPORTED_OPTION_19 = (-29); /* TAG color map is not WORD */
const int UNSUPPORTED_OPTION_20 = (-30); /* TAG color map given twice */
const int UNSUPPORTED_OPTION_21 = (-31); /* TAG color map len is not Mult.3 */
const int UNSUPPORTED_OPTION_22 = (-32); /* TAG color map len is too large */
const int UNSUPPORTED_OPTION_23 = (-33); /* image is too wide for G4-FAX decompr. */
const int UNSUPPORTED_OPTION_24 = (-34); /* important TAG missing */
const int UNSUPPORTED_OPTION_25 = (-35); /* support only black/white */
const int UNSUPPORTED_OPTION_26 = (-36); /* no support for planar config != 1 */
const int UNSUPPORTED_OPTION_27 = (-37); /* support only G2/G3/G4-FAX compression */
const int UNSUPPORTED_OPTION_28 = (-38); /* unsupported compression option */
const int UNSUPPORTED_OPTION_29 = (-39); /* unsupported fill_order (neither 1 nor 2) */

/**************************************************************************/
