
// image.h : image transformation.

// part 1 : image compression/decompression
//          between jpg/gif/bmp/png/tif and RGBA memory format.
// part 2 : zoom, resize, stretch, rotation.

//--------------------------------------------------------------------------

// Part 1 : image compresssion/decompression

/* supported file formats for reading :

   .BMP : 1, 4, 8, 16, 24 or 32-bit images.
   .DIB : (same as .BMP)
   .GIF : all options, including animated gifs.
   .JPG : all options.
   .PCX : 1&8 bits/pixel with 16 and 256 colors palette as well as RGB format.
   .PNG : all options including translucid images.
   .TIF : only black & white with FAX-G2/G3/G4 compression.

   - open_image
   - get_image_size
   - get_image_attributes
   - set_image_options
   - read_image
   - next_image
   - close_image

 * supported file formats for writing :

   .BMP : 24-bit images.
   .GIF : only 1 image (no animated gifs), with optional color quantization.
   .JPG : with parametrable compression.
   .PNG : 24-bit or 32-bit.
   .TIF : only black & white with FAX-G4 compression.

   - create_image
   - get_image_size
   - write_image
   - close_image
*/

//--------------------------------------------------------------------------

struct IMAGE;

//--------------------------------------------------------------------------

/* open an image file.                                   */
/* returns 0 if OK or a negative error code (see below). */

int open_image  (out IMAGE image, string  filename);
int wopen_image (out IMAGE image, wstring filename);

//--------------------------------------------------------------------------

/* close an image (coming from open_image or create_image) */
/* returns 0 if OK, or a negative error code.              */

int close_image (ref IMAGE image);

//--------------------------------------------------------------------------

/* returns width and height of image */

void get_image_size (    IMAGE image,
                     out uint  width,
                     out uint  height);

//--------------------------------------------------------------------------

struct IMAGE_ATTRIBUTES
{
  /* for all formats: */
  byte uses_transparency;   /* indicates how the Alpha byte (4th byte of RGBA) is used */
                            /* 0 = A is not used (always 0) */
                            /* 1 = A can have two values : (255=opaque) or (0=transparent) */
                            /* 2 = A indicates a transparency factor from 0 to 255 */
  /* for FORMAT_GIF : */
  bool is_gif;              /* true = is format GIF (fields below are filled), false = is not GIF */

  /* large screen surrounding all GIF images */
  int  screen_width;       /* 0 to 65535 */
  int  screen_height;      /* 0 to 65535 */

  byte color[3];           /* GIF screen background color (RGB values) */

  /* next GIF image is to be displayed in the screen at given offsets */
  int  offset_x;           /* 0 to 65535 */
  int  offset_y;           /* 0 to 65535 */
  int  image_width;        /* 0 to 65535 */
  int  image_height;       /* 0 to 65535 */
  int  max_colors;         /* 2 to 256 */

  int  hit_key;            /* 0 or 1=user must press key before next GIF image */
  int  disposal_method;    /* 0 .. 7  (0,1=none; 2=set background; 3=restore previous) */
  int  delay;              /* 1/100 secs to wait before next GIF image, or 0 if not used */
  int  repeat_count;       /* nb times to repeat all images, 0=infinite */

  /* for FORMAT_JPEG and FORMAT_TIFF : */
  uint x_resolution;       /* resolution in dots per inch (DPI) */
  uint y_resolution;       /* (0 = undefined) */
}

//--------------------------------------------------------------------------

/* get attributes of image (of next image if animated GIF) */
/* should be called before read_image */

void get_image_attributes (IMAGE image, out IMAGE_ATTRIBUTES attr);

//--------------------------------------------------------------------------

/* retrieves the image pixels as 'RGBA' (4 bytes).                       */
/* The bytes 'RGB' indicate the amount of (Red,Green,Blue) of the pixel. */
/* The byte 'A' (alpha) is the transparency (255=opaque, 0=transparent). */
/* 'buffer'size' must be a multiple of 4.                                */
/* Image pixels are read left to right, line by line.                    */
/* returns 0 if OK, or a negative error code.                            */

int read_image (IMAGE image, out byte[] buffer);

//--------------------------------------------------------------------------

/* prepare to read next image */
/* (to be called after read_image() for animated GIFs) */
/* returns 0 if next image follows, +1 if no image follows, -1 if error */

int next_image (IMAGE image);

//--------------------------------------------------------------------------

enum IMAGE_FILE_FORMAT {FORMAT_BMP, FORMAT_JPEG, FORMAT_GIF, FORMAT_PNG, FORMAT_TIFF};

struct IMAGE_CREATION_PARAMETERS
{
  IMAGE_FILE_FORMAT format; /* FORMAT_BMP, FORMAT_JPEG, FORMAT_GIF, FORMAT_PNG or FORMAT_TIFF */

  /* for FORMAT_JPEG : */
  ushort  quality;      /* 0..100 : 0=small file, 100=large file. */
  bool    output_grey;  /* false = output color, true = output grey (ignored if output_alpha) */
  bool    output_alpha; /* false = no alpha, true = save also alpha channel which creates a non-standard jpeg ! */

  /* for FORMAT_GIF or FORMAT_PNG : */
  ushort  max_colors;   /* 0      : full rgb color (DEFAULT)       */
                        /* 2      : black & white                  */
                        /* 3..256 : color drawing / logo / cartoon */

  ushort  use_transparency; /* 0 = don't save 4th byte */
                            /* 1 = save 4th byte (0=transparent or 255=opaque) */
                            /* 2 = save 4th byte (0..255 = transparency factor) (supported by png only) */

  byte transparent_RGB[3];  /* color of pixels with transparency 0 when loading back the image with .original_rgb == 1 */

  /* for FORMAT_JPEG or FORMAT_TIFF : */
  uint  x_resolution;  /* in dots per inch (DPI) (0 = undefined) */
  uint  y_resolution;

  /* additional fields might appear here in future releases */
}

//--------------------------------------------------------------------------

/* create an image file.                                 */
/* returns 0 if OK or a negative error code (see below). */

int create_image (out IMAGE                     image,
                      string                    filename,
                      uint                      width,
                      uint                      height,
                      IMAGE_CREATION_PARAMETERS parameters);

int wcreate_image (out IMAGE                     image,
                       wstring                   filename,
                       uint                      width,
                       uint                      height,
                       IMAGE_CREATION_PARAMETERS parameters);

//--------------------------------------------------------------------------

/* provide the image pixels as 'RGBA' (4 bytes).                         */
/* The bytes 'RGB' indicate the amount of (Red,Green,Blue) of the pixel. */
/* The byte 'A' (alpha) indicates the transparency factor.               */
/* RGB values must NOT be premultiplied by A before the call.            */
/* 'buffer'size' must be a multiple of 4.                                */
/* Image pixels are written left to right, line by line.                 */
/* returns 0 if OK, or a negative error code.                            */

int write_image (IMAGE image, byte[] buffer);

//--------------------------------------------------------------------------

const int IMG_FILE_NOT_FOUND       = (-1);   /* cannot open/create file */
const int IMG_NOT_RECOGNIZED       = (-2);   /* bad image format */
const int IMG_FILE_ERROR           = (-3);   /* file read/write error or corrupted */
const int IMG_OUT_OF_SPACE         = (-4);   /* out of memory */
const int IMG_ILLEGAL_BUFFER_SIZE  = (-5);   /* size must be multiple of 4 */
const int IMG_END_OF_IMAGE         = (-6);   /* no more pixel data to read */
const int IMG_NOT_OPEN             = (-7);   /* image was not opened or created */
/* or other image format/option-specific errors */

//--------------------------------------------------------------------------

/* multiply all original RGB Components by Alpha */
/* uses the formula : C' = (C+1) * A / 256 */
void multiply_rgb_by_alpha (ref byte[] buffer);

/**************************************************************************/

// Part 2 : RGBA bitmap manipulations

struct IMAGE_INFO
{
  byte[]^ pixel;   /* 4*width*height bytes (RGBA format) */
  uint    width;
  uint    height;
}

struct CLIP_INFO
{
  int  offset_x;
  int  offset_y;
  uint size_x;
  uint size_y;
}

//--------------------------------------------------------------------------

/* allocate memory and load image from file */

int load_image  (out IMAGE_INFO info, string  filename);
int wload_image (out IMAGE_INFO info, wstring filename);

//--------------------------------------------------------------------------

/* save image to file (the memory is not deallocated) */

int save_image (IMAGE_INFO                info,
                string                    filename, // output file
                IMAGE_CREATION_PARAMETERS param);   // output parameters

int wsave_image (IMAGE_INFO                info,
                 wstring                   filename, // output file
                 IMAGE_CREATION_PARAMETERS param);   // output parameters

//--------------------------------------------------------------------------

/* deallocate memory */

void free_image (ref IMAGE_INFO info);

//--------------------------------------------------------------------------

// decompress image in memory.

int decompress_image (byte[] compressed_image, out IMAGE_INFO info);

//--------------------------------------------------------------------------

// compress image in memory.
// returns 0 if OK, a negative value in case of error.
// compressed_image is null in case of error.
// info is not deallocated by the function.

int compress_image (    IMAGE_INFO                info,
                        IMAGE_CREATION_PARAMETERS param,             // output parameters
                    out byte[]^                   compressed_image); // allocated by function

//--------------------------------------------------------------------------

// compressed_image must not be freed while image is used.
int open_image_from_memory (out IMAGE image, byte[] compressed_image);

//--------------------------------------------------------------------------

int create_image_to_memory (out IMAGE                     image,
                                uint                      width,
                                uint                      height,
                                IMAGE_CREATION_PARAMETERS param);

// returns null if can't save or close.
byte[]^ close_and_return_memory (ref IMAGE image);

//--------------------------------------------------------------------------

// create a new image by duplicating the memory

int copy_image (IMAGE_INFO source, out IMAGE_INFO target);

//--------------------------------------------------------------------------

// extract rectangle of source image and create new target image with it

int copy_image_rectangle (    IMAGE_INFO source_image,
                              CLIP_INFO  source_clip,
                          out IMAGE_INFO target_image);

//--------------------------------------------------------------------------

// replace clipped rectangle in target with new source image

int replace_image_rectangle (IMAGE_INFO source_image,
                             CLIP_INFO  source_clip,
                             IMAGE_INFO target_image,
                             CLIP_INFO  target_clip);

//--------------------------------------------------------------------------

/* reduce 'width' or 'height' in order to keep the aspect/ratio of the image */

int bestfit_size (    IMAGE_INFO info,
                  ref uint       width,      /* in out ! */
                  ref uint       height);    /* in out ! */

//--------------------------------------------------------------------------

/* resize image to fit into the new dimensions */
/* the result image is stored in 'result' or 'info' is changed directly */

int resize_image (ref IMAGE_INFO info,
                      uint       width,
                      uint       height,
                      uint       border_color = 0xFF000000,  // opaque black
                      bool       bestfit = true,
                      bool       use_linear_colors = true, // convert from rgb to linear space, resize, then convert back.
                      bool       high_quality = true);

int resize_image2 (    IMAGE_INFO info,
                       uint       width,
                       uint       height,
                       uint       border_color = 0xFF000000,  // opaque black
                       bool       bestfit = true,
                   out IMAGE_INFO result,
                       bool       use_linear_colors = true, // convert from rgb to linear space, resize, then convert back.
                       bool       high_quality = true);

//--------------------------------------------------------------------------

/* zoom image on user click and create new image for screen */

int zoom_image
     (    IMAGE_INFO source,       /* source image */

          uint       click_x,      /* zoom x-center on screen */
          uint       click_y,      /* zoom y-center on screen */
          int        zoom,         /* in percent (100 .. 6_500_000) */
                                   /* use 100 for first call of new image ! */
          uint       width,        /* target screen width  */
          uint       height,       /* target screen height */
      ref CLIP_INFO  source_clip,  /* clear for first call, value must be kept between calls */
      ref CLIP_INFO  target_clip,  /* clear for first call, value must be kept between calls */
          uint       border_color, /* =0 */
      out IMAGE_INFO target,      /* result image for screen */
          bool       use_linear_colors = true, // convert from rgb to linear space, resize, then convert back.
          bool       high_quality = true);

//--------------------------------------------------------------------------

/* angle : 0, 90, 180, 270 (in clock order) */
/* rotation changes the dimensions of the image. */
/* the result image is stored in 'result' or 'info' is changed directly */

int rotate_image (ref IMAGE_INFO info,
                      int        angle,
                      uint       border_color = 0xFF000000,  // opaque black
                      bool       use_linear_colors = true); // convert from rgb to linear space, resize, then convert back.

int rotate_image2 (    IMAGE_INFO info,
                       int        angle,
                       uint       border_color = 0xFF000000,  // opaque black
                   out IMAGE_INFO result,
                       bool       use_linear_colors = true); // convert from rgb to linear space, resize, then convert back.

//--------------------------------------------------------------------------

/* compute a bestfit_target_clip from an original target_clip */
/* so that the later stretching operation keeps the original  */
/* image proportions. The bestfit_target_clip is usually      */
/* smaller than the original target_clip : it is centered     */
/* within the original target_clip and surrounded by vertical */
/* or horizontal borders.                                     */
/* 'target_clip' and 'bestfit_target_clip' are allowed to     */
/* denote the same variable.                                  */
/* returns 0 if OK, -1 in case of out-of-range parameters.    */

int compute_bestfit_target_clip (    CLIP_INFO source_clip,
                                     CLIP_INFO target_clip,
                                 out CLIP_INFO bestfit_target_clip);

//--------------------------------------------------------------------------

/* convert target screen coordinates (x,y) back */
/* into source image coordinates (out_x,out_y). */

void reverse_clip_coordinates (    uint      x,
                                   uint      y,
                                   CLIP_INFO source_clip,
                                   CLIP_INFO target_clip,
                               out uint      out_x,
                               out uint      out_y);

//--------------------------------------------------------------------------

/* perform a zoom by modifying the source and target clips so that */
/* the later stretching operation keeps the original image         */
/* proportions and uses the maximum target space.                  */
/* 'center_x', 'center_y' : center of zoom in source coordinates.  */
/* 'zoom_factor'          : 100 = x1, 200 = x2, ...                */
/* returns 0 if OK, -1 in case of out-of-range parameters.         */

int compute_zoom_clip_info
        (    uint      center_x,      /* within source                 */
             uint      center_y,      /* within source                 */
             int       zoom_factor,   /* in percent (100 .. 6_500_000) */
         ref CLIP_INFO source_clip,   /* in out                        */
         ref CLIP_INFO target_clip);  /* in out                        */

//--------------------------------------------------------------------------

/* copy source clip to target clip, possibly losing image  */
/* proportions if the source_clip and target_clip do not   */
/* have the same x/y ratios.                               */

/* The entire source image is read (source.width x source.height),     */
/* but only the clipped output image is written (target_clip.size_x x  */
/* target_clip.size_y).                                                */

/* returns 0 if OK, -1 in case of out-of-range parameters */

int stretch_image (IMAGE_INFO source,
                   CLIP_INFO  source_clip,
                   IMAGE_INFO target,
                   CLIP_INFO  target_clip,
                   bool       use_linear_colors = true, // convert from rgb to linear space, resize, then convert back.
                   bool       high_quality      = true); // true = high quality, false = very fast

//--------------------------------------------------------------------------

generic <USER_DATA>
package STRETCH

  /* function model */
  /* must return 0 if OK, != 0 in case of error */

  typedef int IO_READ_IMAGE_LINE (ref USER_DATA user_data,
                                      uint      x,         /* columns start at 0 */
                                      uint      y,         /* lines start at 0 */
                                  out byte[]    buffer);

  typedef int IO_WRITE_IMAGE_LINE (ref USER_DATA user_data,
                                       uint      x,         /* columns start at 0 */
                                       uint      y,         /* lines start at 0 */
                                       byte[]    buffer);

  /* same as stretch_image, but this function ignores the source->pixel and  */
  /* target->pixel fields. Instead, the source image lines are read          */
  /* (function read) and the target image lines are written (function write) */
  /* dynamically during the function call. This method never requires to     */
  /* hold the full image in memory.                                          */
  /* The entire source image is read (source.width x source.height),         */
  /* but only the clipped output image is written (target_clip.size_x x      */
  /* target_clip.size_y).                                                    */
  /* returns 0 if OK, -1 in case of out-of-range parameters,                 */
  /* -3 if any IO operation fails.                                           */

  int stretch_image2 (    IMAGE_INFO          source,
                          CLIP_INFO           source_clip,
                          IMAGE_INFO          target,
                          CLIP_INFO           target_clip,
                      ref USER_DATA           user_data,
                          IO_READ_IMAGE_LINE  read,
                          IO_WRITE_IMAGE_LINE write);

end STRETCH;

//--------------------------------------------------------------------------

// return value is in range 0 .. 65535, it must be >> 8 to obtain a byte
uint2 rgb_to_linear_color (byte c);

// c is in range 0 .. 65535
byte linear_to_rgb_color (uint2 c);

//--------------------------------------------------------------------------
