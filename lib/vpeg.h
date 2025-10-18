
// vpeg.h : compression/decompression for mostly static video streams

use image;

//-------------------------------------------------------------------------------------

typedef short[8*8] DCT_MATRIX;

struct DCT_PLANE
{
  uint          nb_cols;
  uint          nb_lines;
  DCT_MATRIX[]^ dct;       // nb_cols * nb_lines matrices
}

struct DCT_IMAGE
{
  uint       width, height;   // image dimensions in pixels
  int        quality;         // 0 .. 100 */
  DCT_PLANE^ component[3];    // Y, Cr, Cb
}

//-------------------------------------------------------------------------------------

const int HUFF_BUFFER_SIZE = 32768;    // size is informative only

struct HUFF_CHUNK
{
  int          count;    // nb occupied bytes in buffer
  byte[]^      buffer;
  HUFF_CHUNK^  next;
}

//-------------------------------------------------------------------------------------

// 1) COMPRESSION

int convert_rgbs_image_to_dct_image (    IMAGE_INFO image,
                                     out DCT_IMAGE  dct);

void quantize_dct_image (    int       quality,   // 0 .. 100
                         ref DCT_IMAGE dct);


// 'dct' must be freed after call.
// 'cache' : cleared by caller at the start of a stream,
//           will be allocated by function,
//           kept by caller between calls,
//           must be freed by caller at end of session.

int huff_encode_dct_image (    DCT_IMAGE   dct,                  // source
                           ref DCT_IMAGE   cache,                // last sent dct
                               uint        luminosity_threshold, // 64
                               uint        chromatic_threshold,  // 32
                           out HUFF_CHUNK^ pbuffers);

void free_dct_image (ref DCT_IMAGE dct);

void free_huff_buffer_list (ref HUFF_CHUNK^ list);

//-------------------------------------------------------------------------------------

// 2) DECOMPRESSION

// 'buffers' must be freed after call.
// 'cache' : cleared by caller at the start of a stream,
//           will be allocated by function,
//           kept by caller between calls,
//           freed by caller at end of session.
//
// a copy of 'cache' must be taken after the call and be passed to unquantize and conversion to rgb.

int huff_decode_dct_image (    HUFF_CHUNK^ buffers,
                           ref DCT_IMAGE   cache,     // last received dct
                           out int         percent_change);


int copy_dct_image (    DCT_IMAGE previous_dct,
                    out DCT_IMAGE dct);

void unquantize_dct_image (ref DCT_IMAGE dct);

int convert_dct_image_to_rgbs_image (    DCT_IMAGE  dct,
                                     out IMAGE_INFO image);

//-------------------------------------------------------------------------------------
