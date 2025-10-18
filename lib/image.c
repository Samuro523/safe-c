
// image.c

use stream, files;
use gif/rgif, gif/wgif;
use tiff/rtiff, tiff/wtiff;
use bmp/rbmp, bmp/wbmp;
use png/rpng, png/wpng;
use pcx/rpcx;
use jpg/rjpg, jpg/wjpg;
use math;
use memory;

/**************************************************************************/

enum IMAGE_KIND {KIND_RGIF, KIND_WGIF, KIND_RTIFF, KIND_WTIFF, KIND_RBMP, KIND_WBMP,
                 KIND_RJPG, KIND_WJPG, KIND_RPNG, KIND_WPNG, KIND_RPCX};

struct IMG (IMAGE_KIND kind)
{
  switch (kind)
  {
    case KIND_RGIF:
      rgif.GIF rgif;

    case KIND_WGIF:
      wgif.GIF wgif;

    case KIND_RTIFF:
      rtiff.TIFF rtiff;

    case KIND_WTIFF:
      wtiff.TIFF wtiff;

    case KIND_RBMP:
      rbmp.BMP rbmp;

    case KIND_WBMP:
      wbmp.BMP wbmp;

    case KIND_RJPG:
      rjpg.JPG rjpg;

    case KIND_WJPG:
      wjpg.JPG wjpg;

    case KIND_RPNG:
      rpng.PNG rpng;

    case KIND_WPNG:
      wpng.PNG wpng;

    case KIND_RPCX:
      rpcx.PCX rpcx;
  }
}

/**************************************************************************/

struct IMAGE
{
  READ_STREAM  rstream;
  WRITE_STREAM wstream;
  IMG^         i;     // null means unused
}

/**************************************************************************/

byte[]^ convert_alpha;   /* allocates 64K */

#begin unsafe
uint2 *g_to_linear_space;  // 256 entries  0.5 K
byte *g_to_rgbs_space;     // 64 K (65536 entries)
#end unsafe

/**************************************************************************/

bool is_write (IMAGE_KIND k)
{
  return (k == KIND_WGIF || k == KIND_WTIFF || k == KIND_WBMP || k == KIND_WJPG || k == KIND_WPNG);
}

/**************************************************************************/

/* multiply all RGB components by A */

public void multiply_rgb_by_alpha (ref byte[] buffer)
{
  uint    i, j;
  byte[]^ conv;

  if (convert_alpha == null)
  {
    conv = new byte[256 * 256];    /* 64K */

    if (conv == null)
      return;

    for (i=0; i<256; i++)
    {
      for (j=0; j<256; j++)
      {
        conv^[(i<<8)+j] = (byte)(((j+1) * i) >> 8);
      }
    }

    convert_alpha = conv;
  }

#begin unsafe
  {
    byte       *pbuf   = &buffer;
    uint       size    = buffer'size;
    ref byte[] convert = convert_alpha^;

    for (i=0; i<size; i+=4)
    {
      byte *ptr = &convert[pbuf[3] << 8];
      pbuf[0] = ptr[pbuf[0]];
      pbuf[1] = ptr[pbuf[1]];
      pbuf[2] = ptr[pbuf[2]];
      pbuf += 4;
    }
  }
#end unsafe
}

/**************************************************************************/

int open_img (ref IMAGE image)
{
  byte header[3];
  int  rc;
  IMG^ img;

  if (stream.read (ref image.rstream, out header) != 3)
    return IMG_NOT_RECOGNIZED;

  assert stream.lseekr (ref image.rstream, 0L, stream.SEEK_SET) == 0;

  if (header[0] == 0xFF)  // reads 4096 bytes, checks for FF D8
  {
    img = new IMG (KIND_RJPG);
    rc = open_jpg (out img^.rjpg, ref image.rstream);
  }
  else if (header[0] == 137)  // 8 bytes {137, 80, 78, 71, 13, 10, 26, 10}
  {
    img = new IMG (KIND_RPNG);
    rc = open_png (out img^.rpng, ref image.rstream);
  }
  else if (header[0] == 0x42)   // 14 bytes starting with "BM"
  {
    img = new IMG (KIND_RBMP);
    rc = open_bmp (out img^.rbmp, ref image.rstream);
  }
  else if (header[0] == 0x47)  // 6 bytes starting with "GIF"
  {
    img = new IMG (KIND_RGIF);
    rc = open_gif (out img^.rgif, ref image.rstream);
  }
  else if (header[0] == 0x49 || header[0] == 0x4D)  // 8 bytes header starting with 0x4949 or 0x4D4D
  {
    img = new IMG (KIND_RTIFF);
    rc = open_tiff (out img^.rtiff, ref image.rstream);
  }
  else if (header[2] == 1) // third byte must have value 1, or not supported.
  {
    img = new IMG (KIND_RPCX);
    rc = open_pcx (out img^.rpcx, ref image.rstream);
  }
  else
  {
    return IMG_NOT_RECOGNIZED;
  }

  if (rc == 0)
  {
    image.i = img;
    return 0;
  }

  free img;
  return rc;
}

/**************************************************************************/

void wcstrcpy (out wstring dest, string src)
{
  int i;

  clear dest;

  for (i=0; i<src'length && src[i] != nul; i++)
    dest[i] = (wchar)(uint)src[i];
}

//----------------------------------------------------------------------------

public int wopen_image (out IMAGE image, wstring filename)
{
  int rc;

  clear image;

  rc = wopen_file_stream (out image.rstream, filename);
  if (rc < 0)
    return rc;

  rc = open_img (ref image);
  if (rc < 0)
  {
    rclose (ref image.rstream);
    clear image;
    return rc;
  }

  return 0;
}

public int open_image (out IMAGE image, string filename)
{
  wchar[MAX_FILENAME_LENGTH]  wfilename;
  wcstrcpy (out wfilename, filename);
  return wopen_image (out image, wfilename);
}

/**************************************************************************/

public void get_image_size (IMAGE    image,
                            out uint width,
                            out uint height)
{
  if (!is_ropen(image.rstream))
    abort;

  switch (image.i^.kind)
  {
    case KIND_RGIF:
      get_gif_size (image.i^.rgif, out width, out height);
      break;

    case KIND_WGIF:
      get_gif_size2 (image.i^.wgif, out width, out height);
      break;

    case KIND_RTIFF:
      get_tiff_size (image.i^.rtiff, out width, out height);
      break;

    case KIND_WTIFF:
      get_tiff_size2 (image.i^.wtiff, out width, out height);
      break;

    case KIND_RBMP:
      get_bmp_size (image.i^.rbmp, out width, out height);
      break;

    case KIND_WBMP:
      get_bmp_size2 (image.i^.wbmp, out width, out height);
      break;

    case KIND_RJPG:
      get_jpg_size (image.i^.rjpg, out width, out height);
      break;

    case KIND_WJPG:
      get_jpg_size2 (image.i^.wjpg, out width, out height);
      break;

    case KIND_RPNG:
      get_png_size (image.i^.rpng, out width, out height);
      break;

    case KIND_WPNG:
      get_png_size2 (image.i^.wpng, out width, out height);
      break;

    case KIND_RPCX:
      get_pcx_size (image.i^.rpcx, out width, out height);
      break;

    default:
      abort;
  }
}

/**************************************************************************/

public void get_image_attributes (IMAGE image, out IMAGE_ATTRIBUTES attr)
{
  if (!is_ropen(image.rstream))
    abort;

  switch (image.i^.kind)
  {
    case KIND_RGIF:
      get_gif_attributes (image.i^.rgif, out attr);
      break;

    case KIND_RTIFF:
      get_tiff_attributes (image.i^.rtiff, out attr);
      break;

    case KIND_RJPG:
      get_jpg_attributes (image.i^.rjpg, out attr);
      break;

    case KIND_RPNG:
      get_png_attributes (image.i^.rpng, out attr);
      break;

    default:
      clear attr;    /* routine not provided for this image type */
      break;
  }
}

/**************************************************************************/

public int next_image (IMAGE image)
{
  if (!is_ropen(image.rstream))
    return IMG_NOT_OPEN;

#begin unsafe
  switch (image.i^.kind)
  {
    case KIND_RGIF:
      return next_gif (ref image.i^.rgif, ref *(&image.rstream));

    default:
      return +1;    // no next image
  }
#end unsafe
}

/**************************************************************************/

public int read_image (IMAGE image, out byte[] buffer)
{
  if (!is_ropen(image.rstream))
  {
    clear buffer;
    return IMG_NOT_OPEN;
  }

#begin unsafe
  switch (image.i^.kind)
  {
    case KIND_RGIF:
      return read_gif (ref image.i^.rgif, out buffer, ref *(&image.rstream));

    case KIND_RTIFF:
      return read_tiff (ref image.i^.rtiff, out buffer, ref *(&image.rstream));

    case KIND_RBMP:
      return read_bmp (ref image.i^.rbmp, out buffer, ref *(&image.rstream));

    case KIND_RJPG:
      return read_jpg (ref image.i^.rjpg, out buffer);

    case KIND_RPNG:
      return read_png (ref image.i^.rpng, out buffer);

    case KIND_RPCX:
      return read_pcx (ref image.i^.rpcx, out buffer, ref *(&image.rstream));

    default:
      abort;
  }
#end unsafe

  return 0;
}

/**************************************************************************/

public int write_image (IMAGE image, byte[] buffer)
{
  if (!is_wopen(image.wstream))
    return IMG_NOT_OPEN;

#begin unsafe
  switch (image.i^.kind)
  {
    case KIND_WGIF:
      return write_gif (ref image.i^.wgif, buffer, ref *(&image.wstream));

    case KIND_WTIFF:
      return write_tiff (ref image.i^.wtiff, buffer, ref *(&image.wstream));

    case KIND_WBMP:
      return write_bmp (ref image.i^.wbmp, buffer, ref *(&image.wstream));

    case KIND_WJPG:
      return write_jpg (ref image.i^.wjpg, buffer);

    case KIND_WPNG:
      return write_png (ref image.i^.wpng, buffer);

    default:
      abort;
  }
#end unsafe
}

/**************************************************************************/

int close_img (ref IMAGE image, out byte[]^ compressed, int initial_rc)
{
  int rc;

  compressed = null;

  if (is_write (image.i^.kind))
  {
    if (!is_wopen(image.wstream))
      return IMG_NOT_OPEN;
  }
  else
  {
    if (!is_ropen(image.rstream))
      return IMG_NOT_OPEN;
  }

  switch (image.i^.kind)
  {
    case KIND_RGIF:
      rc = close_gif (ref image.i^.rgif);
      break;

    case KIND_WGIF:
      rc = close_gif2 (ref image.i^.wgif);
      break;

    case KIND_RTIFF:
      rc = close_tiff (ref image.i^.rtiff);
      break;

    case KIND_WTIFF:
      rc = close_tiff2 (ref image.i^.wtiff, ref image.wstream);
      break;

    case KIND_RBMP:
      rc = close_bmp (ref image.i^.rbmp);
      break;

    case KIND_WBMP:
      rc = close_bmp2 (ref image.i^.wbmp);
      break;

    case KIND_RJPG:
      rc = close_jpg (ref image.i^.rjpg);
      break;

    case KIND_WJPG:
      rc = close_jpg2 (ref image.i^.wjpg);
      break;

    case KIND_RPNG:
      rc = close_png (ref image.i^.rpng);
      break;

    case KIND_WPNG:
      rc = close_png2 (ref image.i^.wpng);
      break;

    case KIND_RPCX:
      rc = close_pcx (ref image.i^.rpcx);
      break;

    default:
      abort;
  }

  if (rc == 0)
    rc = initial_rc;

  if (is_write (image.i^.kind))
  {
    compressed = wclose_and_get_memory_stream (ref image.wstream, ref rc);
  }
  else
  {
    rclose (ref image.rstream);
  }

  free image.i;
  clear image;
  return rc;
}

/**************************************************************************/

public int close_image (ref IMAGE image)
{
  int     rc;
  byte[]^ compressed;

  rc = close_img (ref image, out compressed, 0);

  assert compressed == null;   // both read and file stream return null

  return rc;
}

/**************************************************************************/

int create_img (ref IMAGE                     image,
                    uint                      width,
                    uint                      height,
                    IMAGE_CREATION_PARAMETERS parameters)
{
  int  rc;
  IMG^ img;

  switch (parameters.format)
  {
    case FORMAT_BMP :
      img = new IMG (KIND_WBMP);
      rc = create_bmp (out img^.wbmp, ref image.wstream, (uint2)width, (uint2)height);
      break;

    case FORMAT_JPEG :
      img = new IMG (KIND_WJPG);
      rc = create_jpg (out img^.wjpg, ref image.wstream,
                       (uint2)width, (uint2)height,
                       parameters.quality, parameters.output_grey, parameters.output_alpha,
                       parameters.x_resolution, parameters.y_resolution);
      break;

    case FORMAT_GIF :
      img = new IMG (KIND_WGIF);
      rc = create_gif (out img^.wgif, ref image.wstream,
                       (uint2)width, (uint2)height,
                       parameters.max_colors, parameters.use_transparency,
                       parameters.transparent_RGB);
      break;

    case FORMAT_PNG :
      img = new IMG (KIND_WPNG);
      rc = create_png (out img^.wpng, ref image.wstream,
                       (uint2)width, (uint2)height,
                       parameters.max_colors, parameters.use_transparency,
                       parameters.transparent_RGB);
      break;

    case FORMAT_TIFF :
      img = new IMG (KIND_WTIFF);
      rc = create_tiff (out img^.wtiff, ref image.wstream,
                        (uint2)width, (uint2)height,
                        parameters.x_resolution,
                        parameters.y_resolution);
      break;

    default:
      return IMG_NOT_RECOGNIZED;
  }

  if (rc == 0)
  {
    image.i = img;
    return 0;
  }

  free img;
  return rc;
}

/**************************************************************************/

public int wcreate_image (out IMAGE                     image,
                              wstring                   filename,
                              uint                      width,
                              uint                      height,
                              IMAGE_CREATION_PARAMETERS parameters)
{
  int rc;

  clear image;

  rc = wcreate_file_stream (out image.wstream, filename);
  if (rc < 0)
    return rc;

  rc = create_img (ref image, width, height, parameters);
  if (rc < 0)
  {
    free wclose_and_get_memory_stream (ref image.wstream, ref rc);
    clear image;
    return rc;
  }

  return 0;
}


public int create_image (out IMAGE                     image,
                             string                    filename,
                             uint                      width,
                             uint                      height,
                             IMAGE_CREATION_PARAMETERS parameters)
{
  wchar[MAX_FILENAME_LENGTH]  wfilename;
  wcstrcpy (out wfilename, filename);
  return wcreate_image (out image, wfilename, width, height, parameters);
}

/**************************************************************************/

/* allocate memory and load image from file */

public int wload_image (out IMAGE_INFO info, wstring filename)
{
  int   rc;
  IMAGE image;

  clear info;

  rc = wopen_image (out image, filename);
  if (rc < 0)
  {
//    trace ("error: load_image() : open_image(%s) returned %d\n", filename, rc);
    return rc;
  }

  get_image_size (image, out info.width, out info.height);

  info.pixel = new byte[4 * info.width * info.height];

  rc = read_image (image, out info.pixel^);
  if (rc < 0)
  {
    free info.pixel;
    clear info;
    close_image (ref image);
//    trace ("error: load_image() : read_image(%s) returned %d\n", filename, rc);
    return rc;
  }

  rc = close_image (ref image);
  if (rc < 0)
  {
    free info.pixel;
    clear info;
//    trace ("error: load_image() : close_image(%s) returned %d\n", filename, rc);
    return rc;
  }

  return 0;
}


public int load_image (out IMAGE_INFO info, string filename)
{
  wchar[MAX_FILENAME_LENGTH]  wfilename;
  wcstrcpy (out wfilename, filename);
  return wload_image (out info, wfilename);
}

/**************************************************************************/

public int wsave_image (IMAGE_INFO                info,
                        wstring                   filename, /* output file */
                        IMAGE_CREATION_PARAMETERS param)    /* output parameters */
{
  int   rc;
  IMAGE image;

  rc = wcreate_image (out image, filename, info.width, info.height, param);
  if (rc < 0)
  {
//    trace ("error: save_image() : create_image(%s) returned %d\n", filename, rc);
    return rc;
  }

  rc = write_image (image, info.pixel^);
  if (rc < 0)
  {
    close_image (ref image);
//    trace ("error: save_image() : write_image(%s) returned %d\n", filename, rc);
    return rc;
  }

  rc = close_image (ref image);
  if (rc < 0)
  {
//    trace ("error: save_image() : close_image(%s) returned %d\n", filename, rc);
    return rc;
  }

  return 0;
}


public int save_image (IMAGE_INFO                info,
                       string                    filename, /* output file */
                       IMAGE_CREATION_PARAMETERS param)    /* output parameters */
{
  wchar[MAX_FILENAME_LENGTH]  wfilename;
  wcstrcpy (out wfilename, filename);
  return wsave_image (info, wfilename, param);
}

/**************************************************************************/

/* deallocate memory */

public void free_image (ref IMAGE_INFO info)
{
  free info.pixel;
  clear info;
}

/**************************************************************************/

public int decompress_image (byte[] compressed_image, out IMAGE_INFO info)
{
  int   rc;
  IMAGE image;

  clear image, info;

  open_memory_stream (out image.rstream, compressed_image);

  rc = open_img (ref image);
  if (rc < 0)
  {
    rclose (ref image.rstream);
    return rc;
  }

  get_image_size (image, out info.width, out info.height);

  info.pixel = new byte[4 * info.width * info.height];

  rc = read_image (image, out info.pixel^);
  if (rc < 0)
  {
    free info.pixel;
    clear info;
    close_image (ref image);
    return rc;
  }

  rc = close_image (ref image);
  if (rc < 0)
  {
    free info.pixel;
    clear info;
    return rc;
  }

  return 0;
}

/**************************************************************************/

// compressed_image must not be freed while image is used.

public int open_image_from_memory (out IMAGE image, byte[] compressed_image)
{
  int rc;

  clear image;

  open_memory_stream (out image.rstream, compressed_image);

  rc = open_img (ref image);
  if (rc < 0)
    rclose (ref image.rstream);

  return rc;
}

/**************************************************************************/

public int create_image_to_memory (out IMAGE                     image,
                                       uint                      width,
                                       uint                      height,
                                       IMAGE_CREATION_PARAMETERS param)
{
  int rc;

  clear image;

  create_memory_stream (out image.wstream);

  rc = create_img (ref image, width, height, param);
  if (rc < 0)
    free wclose_and_get_memory_stream (ref image.wstream, ref rc);

  return rc;
}

/**************************************************************************/

public byte[]^ close_and_return_memory (ref IMAGE image)
{
  int     rc;
  byte[]^ compressed_image;

  rc = close_img (ref image, out compressed_image, 0);
  if (rc < 0)
  {
    free compressed_image;
    compressed_image = null;
  }

  return compressed_image;
}

/**************************************************************************/

// compress image in memory.
// returns 0 if OK, a negative value in case of error.
// compressed_image is null in case of error.
// info is not deallocated by the function.

public
int compress_image (    IMAGE_INFO                info,
                        IMAGE_CREATION_PARAMETERS param,             // output parameters
                    out byte[]^                   compressed_image)  // allocated by function
{
  IMAGE image;
  int   rc;

  clear image, compressed_image;

  create_memory_stream (out image.wstream);

  rc = create_img (ref image, info.width, info.height, param);
  if (rc < 0)
  {
    free wclose_and_get_memory_stream (ref image.wstream, ref rc);
    return rc;
  }

  rc = write_image (image, info.pixel^);
  if (rc < 0)
  {
    close_img (ref image, out compressed_image, rc);
    return rc;
  }

  rc = close_img (ref image, out compressed_image, 0);
  if (rc < 0)
    return rc;

  return 0;
}

/**************************************************************************/

/* create a new image by duplicating the memory */

public int copy_image (IMAGE_INFO source, out IMAGE_INFO target)
{
  uint size;

  size = 4 * source.width * source.height;

  assert (size == (uint)source.pixel^'length);

  target = { pixel  => new byte[] ' (source.pixel^),
             width  => source.width,
             height => source.height };

  return 0;
}

/**************************************************************************/

public int compute_bestfit_target_clip (CLIP_INFO     source_clip,
                                        CLIP_INFO     target_clip,
                                        out CLIP_INFO bestfit_target_clip)
{
  CLIP_INFO target_clip0;
  double    fx, fy;
  uint      temp;

  target_clip0 = target_clip;

  clear bestfit_target_clip;


  /* check all parameters */

  if (source_clip.size_x == 0 || source_clip.size_y == 0 ||
      target_clip0.size_x == 0 || target_clip0.size_y == 0)
  {
    return -1;
  }


  /* default : use same clipping */
  bestfit_target_clip = target_clip0;

  fx = (double)target_clip0.size_x / (double)source_clip.size_x;
  fy = (double)target_clip0.size_y / (double)source_clip.size_y;

  if (fx < fy)
  {
    temp = (uint)((double)target_clip0.size_y * fx / fy);
    if (temp == 0)
      temp = 1;

    bestfit_target_clip.offset_y += (int)((target_clip0.size_y - temp) >> 1);
    bestfit_target_clip.size_y = temp;
  }
  else if (fx > fy)
  {
    temp = (uint)((double)target_clip0.size_x * fy / fx);
    if (temp == 0)
      temp = 1;

    bestfit_target_clip.offset_x += (int)((target_clip0.size_x - temp) >> 1);
    bestfit_target_clip.size_x = temp;
  }

  return 0;
}

/***************************************************************/

/* reduce 'width' or 'height' in order to keep the aspect/ratio of the image */

public int bestfit_size (    IMAGE_INFO info,
                         ref uint       width,      /* in out ! */
                         ref uint       height)     /* in out ! */
{
  CLIP_INFO  source_clip, target_clip;

  source_clip = { offset_x => 0,
                  offset_y => 0,
                  size_x   => info.width,
                  size_y   => info.height };

  target_clip = { offset_x => 0,
                  offset_y => 0,
                  size_x   => width,
                  size_y   => height };

  if (compute_bestfit_target_clip (source_clip, target_clip, out target_clip) < 0)
    return -1;

  width  = target_clip.size_x;
  height = target_clip.size_y;

  return 0;
}

/**************************************************************************/

void build_linear_rgbs_conversion_tables ()
{
#begin unsafe
  if (g_to_rgbs_space != null)  // was done
    return;

  if (g_to_linear_space != null)  // is busy
  {
    while (g_to_rgbs_space == null)
      sleep 0.1;
    return;
  }

  {
    int i;

    g_to_linear_space = (uint2*)malloc(2*256);  // 256 entries  0.5 K

    for (i=0; i<256; i++)
    {
      double hi = pow(((double)i + 0.7) / (double)255, 2.2);
      g_to_linear_space[i] = (uint2)(hi * 65536.0);
    }
    g_to_linear_space[255] = 65535;
  }

  {
    int  i;
    byte *to_rgbs_space = malloc (64*1024);       // 64 K (65536 entries)

    for (i=0; i<256*256; i++)
      to_rgbs_space[i] = (byte)(pow((double)i / (double)(256*256), 1.0/2.2) * 255.0);
    to_rgbs_space[256*256-1] = 255;

    g_to_rgbs_space = to_rgbs_space;
  }
#end unsafe
}

/**************************************************************************/

// return value is in range 0 .. 65535, it must be >> 8 to obtain a byte

public uint2 rgb_to_linear_color (byte c)
{
#begin unsafe
  if (g_to_rgbs_space == null)
    build_linear_rgbs_conversion_tables ();
  return g_to_linear_space[c];
#end unsafe
}

/**************************************************************************/

// c is in range 0 .. 65535

public byte linear_to_rgb_color (uint2 c)
{
#begin unsafe
  if (g_to_rgbs_space == null)
    build_linear_rgbs_conversion_tables ();
  return g_to_rgbs_space[c];
#end unsafe
}

/**************************************************************************/

public int stretch_image (IMAGE_INFO source,
                          CLIP_INFO  source_clip,
                          IMAGE_INFO target,
                          CLIP_INFO  target_clip,
                          bool       use_linear_colors = true,
                          bool       high_quality      = true)  // true = high quality, false = very fast
{
#begin unsafe

  // check all parameters

  if (source_clip.size_x == 0 ||
      source_clip.size_x > source.width ||
      source_clip.offset_x < 0 ||
      source_clip.offset_x > (int)(source.width - source_clip.size_x) ||

      source_clip.size_y == 0 ||
      source_clip.size_y > source.height ||
      source_clip.offset_y < 0 ||
      source_clip.offset_y > (int)(source.height - source_clip.size_y) ||

      target_clip.size_x == 0 ||
      target_clip.size_x > target.width ||
      target_clip.offset_x < 0 ||
      target_clip.offset_x > (int)(target.width - target_clip.size_x) ||

      target_clip.size_y == 0 ||
      target_clip.size_y > target.height ||
      target_clip.offset_y < 0 ||
      target_clip.offset_y > (int)(target.height - target_clip.size_y))
  {
    return -1;
  }


  if (source_clip.size_x == target_clip.size_x &&   // no stretching, just a plain copy
      source_clip.size_y == target_clip.size_y)
  {
    uint* source_base = &((uint*)&source.pixel^)[source_clip.offset_x + (int)source_clip.offset_y * (int)source.width];
    uint* target_base = &((uint*)&target.pixel^)[target_clip.offset_x + (int)target_clip.offset_y * (int)target.width];

    if (source_clip.size_x == source.width && target_clip.size_x == target.width)  // we can copy all in one copy
    {
      int count = (int)source_clip.size_x * (int)source_clip.size_y;
      target_base[0 : count] = source_base[0 : count];
    }
    else   // we copy row per row
    {
      int line_len = (int)source_clip.size_x;
      int source_len = (int)source.width;
      int target_len = (int)target.width;
      int source_offset = 0;
      int target_offset = 0;
      int y;
      for (y=0; y<(int)target_clip.size_y; y++)
      {
        target_base[target_offset:line_len] = source_base[source_offset:line_len];
        target_offset += target_len;
        source_offset += source_len;
      }
    }

    return 0;
  }

  if (!high_quality)   // fast (without linear colors)
  {

    {
      const int SCALING_POWER = 1;
      if (source_clip.size_x == target_clip.size_x << (uint)SCALING_POWER &&   // shrink 1/(1<<SCALING_POWER)
          source_clip.size_y == target_clip.size_y << (uint)SCALING_POWER)
      {
        uint* source_base = &((uint*)&source.pixel^)[source_clip.offset_x + (int)source_clip.offset_y * (int)source.width];
        uint* target_base = &((uint*)&target.pixel^)[target_clip.offset_x + (int)target_clip.offset_y * (int)target.width];
        int y, x, x2, x9;
        x9 = (int)target_clip.size_x;
        for (y=0; y<(int)target_clip.size_y; y++)
        {
          for (x=0,x2=0; x<x9; x++,x2+=(1 << SCALING_POWER))
            target_base[x] = source_base[x2];
          target_base += (int)target.width;
          source_base += (int)source.width << SCALING_POWER;
        }
        return 0;
      }
    }

    {
      const int SCALING_POWER = 2;
      if (source_clip.size_x == target_clip.size_x << (uint)SCALING_POWER &&   // shrink 1/(1<<SCALING_POWER)
          source_clip.size_y == target_clip.size_y << (uint)SCALING_POWER)
      {
        uint* source_base = &((uint*)&source.pixel^)[source_clip.offset_x + (int)source_clip.offset_y * (int)source.width];
        uint* target_base = &((uint*)&target.pixel^)[target_clip.offset_x + (int)target_clip.offset_y * (int)target.width];
        int y, x, x2, x9;
        x9 = (int)target_clip.size_x;
        for (y=0; y<(int)target_clip.size_y; y++)
        {
          for (x=0,x2=0; x<x9; x++,x2+=(1 << SCALING_POWER))
            target_base[x] = source_base[x2];
          target_base += (int)target.width;
          source_base += (int)source.width << SCALING_POWER;
        }
        return 0;
      }
    }

    {
      const int SCALING_POWER = 3;
      if (source_clip.size_x == target_clip.size_x << (uint)SCALING_POWER &&   // shrink 1/(1<<SCALING_POWER)
          source_clip.size_y == target_clip.size_y << (uint)SCALING_POWER)
      {
        uint* source_base = &((uint*)&source.pixel^)[source_clip.offset_x + (int)source_clip.offset_y * (int)source.width];
        uint* target_base = &((uint*)&target.pixel^)[target_clip.offset_x + (int)target_clip.offset_y * (int)target.width];
        int y, x, x2, x9;
        x9 = (int)target_clip.size_x;
        for (y=0; y<(int)target_clip.size_y; y++)
        {
          for (x=0,x2=0; x<x9; x++,x2+=(1 << SCALING_POWER))
            target_base[x] = source_base[x2];
          target_base += (int)target.width;
          source_base += (int)source.width << SCALING_POWER;
        }
        return 0;
      }
    }

    {
      const int SCALING_POWER = 4;
      if (source_clip.size_x == target_clip.size_x << (uint)SCALING_POWER &&   // shrink 1/(1<<SCALING_POWER)
          source_clip.size_y == target_clip.size_y << (uint)SCALING_POWER)
      {
        uint* source_base = &((uint*)&source.pixel^)[source_clip.offset_x + (int)source_clip.offset_y * (int)source.width];
        uint* target_base = &((uint*)&target.pixel^)[target_clip.offset_x + (int)target_clip.offset_y * (int)target.width];
        int y, x, x2, x9;
        x9 = (int)target_clip.size_x;
        for (y=0; y<(int)target_clip.size_y; y++)
        {
          for (x=0,x2=0; x<x9; x++,x2+=(1 << SCALING_POWER))
            target_base[x] = source_base[x2];
          target_base += (int)target.width;
          source_base += (int)source.width << SCALING_POWER;
        }
        return 0;
      }
    }


    // low quality stretch

    {
      uint[]^  ptab_x, ptab_y;


      /* allocate conversion tables */

      ptab_x = new uint [target_clip.size_x];
      ptab_y = new uint [target_clip.size_y];


      {
        ref uint[] tab_x = ptab_x^;
        ref uint[] tab_y = ptab_y^;

        uint       i, i9, increment, limit, sum, x, y, x9, y9;


        /* fill conversion tables : "tab_x[target_x] -> source_x" */

        x         = 0;
        sum       = 0;
        increment = source_clip.size_x;
        limit     = target_clip.size_x;
        i9        = limit;

        for (i=0; i<i9; i++)
        {
          tab_x[i] = x;
          sum += increment;
          while (sum >= limit)
          {
            sum -= limit;
            x++;
          }
        }

        y         = 0;
        sum       = 0;
        increment = source_clip.size_y;
        limit     = target_clip.size_y;
        i9        = limit;

        for (i=0; i<i9; i++)
        {
          tab_y[i] = y;
          sum += increment;
          while (sum >= limit)
          {
            sum -= limit;
            y++;
          }
        }


        /* copy, line by line */

        {
          uint* base_source, base_target, psource, ptarget;
          uint* ptarget_y;
          uint* ptrtab_y, ptrtab_x;

          base_source = &((uint *)&source.pixel^)[(uint)source_clip.offset_x + (uint)source_clip.offset_y * source.width];
          base_target = &((uint *)&target.pixel^)[(uint)target_clip.offset_x + (uint)target_clip.offset_y * target.width];

 //         psource = base_source;
 //         ptarget = base_target;

          x9        = target_clip.size_x;
          y9        = target_clip.size_y;
          ptarget_y = base_target;

          ptrtab_y = &tab_y[0];

          for (y=0; y<y9; y++)
          {
            ptrtab_x = &tab_x[0];

            ptarget = ptarget_y;
            psource = base_source + (source.width * ptrtab_y[0]);

            for (x=0; x<x9; x++)
            {
              *ptarget++ = psource[ptrtab_x[0]];
              ptrtab_x++;

            }  // for (x

            ptarget_y += target.width;
            ptrtab_y++;

          }  // for (y
        }
      }

      // free all

      free ptab_x;
      free ptab_y;
    }

  }
  else   // high quality
  {

    if (use_linear_colors)
      build_linear_rgbs_conversion_tables ();

    {
      const uint LSHIFTS = 3;    // 1 = bad quality, 4 = best quality
      uint[]^    ptab_x, ptab_y;


      /* allocate conversion tables */

      ptab_x = new uint [(target_clip.size_x << LSHIFTS)];
      ptab_y = new uint [(target_clip.size_y << LSHIFTS)];


      {
        ref uint[] tab_x = ptab_x^;
        ref uint[] tab_y = ptab_y^;

        uint       i, i9, increment, limit, sum, x, y, x9, y9;


        /* fill conversion tables : "tab_x[target_x << LSHIFTS] -> source_x" */

        x         = 0;
        sum       = 0;
        increment = source_clip.size_x;
        limit     = target_clip.size_x;
        i9        = limit << LSHIFTS;

        for (i=0; i<i9; i++)
        {
          tab_x[i] = x >> LSHIFTS;
          sum += increment;
          while (sum >= limit)
          {
            sum -= limit;
            x++;
          }
        }

        y         = 0;
        sum       = 0;
        increment = source_clip.size_y;
        limit     = target_clip.size_y;
        i9        = limit << LSHIFTS;

        for (i=0; i<i9; i++)
        {
          tab_y[i] = y >> LSHIFTS;
          sum += increment;
          while (sum >= limit)
          {
            sum -= limit;
            y++;
          }
        }


        /* copy, line by line */

        {
          uint* base_source, base_target, psource, ptarget;
          uint* ptarget_y;
          uint* ptrtab_y, ptrtab_x;

          base_source = &((uint *)&source.pixel^)[(uint)source_clip.offset_x + (uint)source_clip.offset_y * source.width];
          base_target = &((uint *)&target.pixel^)[(uint)target_clip.offset_x + (uint)target_clip.offset_y * target.width];

 //         psource = base_source;
 //         ptarget = base_target;

          /* compute average color of each new pixel */

          x9        = target_clip.size_x;
          y9        = target_clip.size_y;
          ptarget_y = base_target;

          ptrtab_y = &tab_y[0];

          for (y=0; y<y9; y++)
          {
            ptrtab_x = &tab_x[0];

            ptarget = ptarget_y;

            for (x=0; x<x9; x++)
            {
              uint yy, xx;
              uint r, g, b, s;

              r = 0;
              g = 0;
              b = 0;
              s = 0;

              for (yy=0; yy<(1<<LSHIFTS); yy++)
              {
                psource = base_source + (source.width * ptrtab_y[yy]);

                for (xx=0; xx<(1<<LSHIFTS); xx++)
                {
                  byte *p = (byte *)&psource[ptrtab_x[xx]];

                  if (use_linear_colors)
                  {
                    r += g_to_linear_space [p[0]];
                    g += g_to_linear_space [p[1]];
                    b += g_to_linear_space [p[2]];
                  }
                  else
                  {
                    r += p[0];
                    g += p[1];
                    b += p[2];
                  }

                  s += p[3];

                }  // for (xx
              }  // for (yy

              if (use_linear_colors)
              {
                *ptarget++ =  g_to_rgbs_space[r >> (2*LSHIFTS)]
                           + (g_to_rgbs_space[g >> (2*LSHIFTS)] << 8)
                           + (g_to_rgbs_space[b >> (2*LSHIFTS)] << 16)
                           + ((s >> (2*LSHIFTS)) << 24);
              }
              else
              {
                *ptarget++ =  (r >> (2*LSHIFTS))
                           + ((g >> (2*LSHIFTS)) << 8)
                           + ((b >> (2*LSHIFTS)) << 16)
                           + ((s >> (2*LSHIFTS)) << 24);
              }

              ptrtab_x += (1<<LSHIFTS);

            }  // for (x

            ptarget_y += target.width;
            ptrtab_y += (1<<LSHIFTS);

          }  // for (y
        }
      }

      /* free all */

      free ptab_x;
      free ptab_y;
    }
  }

  return 0;

#end unsafe
}

/**************************************************************************/

package body STRETCH

  public int stretch_image2 (IMAGE_INFO          source,
                             CLIP_INFO           source_clip,
                             IMAGE_INFO          target,
                             CLIP_INFO           target_clip,
                             ref USER_DATA       user_data,
                             IO_READ_IMAGE_LINE  read,
                             IO_WRITE_IMAGE_LINE write)
  {

#begin unsafe

    uint[]^  ptab_x, ptab_y;
    uint*    tab_x, tab_y;
    uint     i, increment, limit, sum, times, x, y, yy, x9, y9;
    uint*    psource, ptarget;
    uint[]^  in_buffer, out_buffer;
    int      rc;


    /* check all parameters */

    if (source_clip.size_x == 0 ||
        source_clip.size_x > source.width ||
        source_clip.offset_x < 0 ||
        source_clip.offset_x > (int)(source.width - source_clip.size_x) ||

        source_clip.size_y == 0 ||
        source_clip.size_y > source.height ||
        source_clip.offset_y < 0 ||
        source_clip.offset_y > (int)(source.height - source_clip.size_y) ||

        target_clip.size_x == 0 ||
        target_clip.size_x > target.width ||
        target_clip.offset_x < 0 ||
        target_clip.offset_x > (int)(target.width - target_clip.size_x) ||

        target_clip.size_y == 0 ||
        target_clip.size_y > target.height ||
        target_clip.offset_y < 0 ||
        target_clip.offset_y > (int)(target.height - target_clip.size_y))
    {
      return -1;
    }


    /* allocate conversion tables */

    ptab_x = new uint [target_clip.size_x + 1];
    ptab_y = new uint [target_clip.size_y + 1];

    tab_x = &ptab_x^;
    tab_y = &ptab_y^;


    /* allocate in_buffer, out_buffer */

    in_buffer = new uint[source.width];
    out_buffer = new uint[target_clip.size_x];


    /* fill conversion tables */

    x         = 0;
    sum       = 0;
    increment = source_clip.size_x;
    limit     = target_clip.size_x;

    if ((increment >> 3) <= limit)     /* enlarge or shrink max 8 times */
    {
      for (i=0; i<limit; i++)
      {
        tab_x[i] = x;
        sum += increment;

        while (sum >= limit)
        {
          sum -= limit;
          x++;
        }
      }
    }
    else           /* increment / 8 > limit : shrink more than 8 times */
    {
      for (i=0; i<limit; i++)
      {
        tab_x[i] = x;
        sum += increment;

        times = sum / limit;

        sum -= times * limit;
        x   += times;
      }
    }


    y         = 0;
    sum       = 0;
    increment = source_clip.size_y;
    limit     = target_clip.size_y;

    if ((increment >> 3) <= limit)     /* enlarge or shrink max 8 times */
    {
      for (i=0; i<limit; i++)
      {
        tab_y[i] = y;
        sum += increment;
        while (sum >= limit)
        {
          sum -= limit;
          y++;
        }
      }
    }
    else           /* increment / 8 > limit : shrink more than 8 times */
    {
      for (i=0; i<limit; i++)
      {
        tab_y[i] = y;
        sum += increment;

        times = sum / limit;

        sum -= times * limit;
        y   += times;
      }
    }


    /* skip source lines above offset_y */

    yy = 0;
    while (yy < (uint)source_clip.offset_y)
    {
      rc = read (ref user_data, 0, yy, out in_buffer^'byte);
      if (rc != 0)
      {
        free (ptab_x);
        free (ptab_y);
        free (in_buffer);
        free (out_buffer);
        return -3;
      }
      yy++;
    }


    /* copy, line by line */

    y9 = target_clip.size_y;
    for (y=0; y<y9; y++)
    {
      if (tab_y[y] >= yy - (uint)source_clip.offset_y)
      {
        /* load new line in 'in_buffer' */
        while (tab_y[y] >= yy - (uint)source_clip.offset_y)
        {
          rc = read (ref user_data, 0, yy, out in_buffer^'byte);
          if (rc != 0)
          {
            free (ptab_x);
            free (ptab_y);
            free (in_buffer);
            free (out_buffer);
            return -3;
          }
          yy++;
        }

        /* stretch 'in_buffer' into 'out_buffer' */
        psource = &in_buffer^[source_clip.offset_x];
        ptarget = &out_buffer^;
        x9 = target_clip.size_x;
        for (x=0; x<x9; x++)
          *ptarget++ = psource[tab_x[x]];
      }


      /* output */
      rc = write (ref user_data, (uint)target_clip.offset_x, y, out_buffer^'byte);
      if (rc != 0)
      {
        free (ptab_x);
        free (ptab_y);
        free (in_buffer);
        free (out_buffer);
        return -3;
      }
    }


    /* skip source lines til end of image */

    while (yy < source.height)
    {
      rc = read (ref user_data, 0, yy, out in_buffer^'byte);
      if (rc != 0)
      {
        free (ptab_x);
        free (ptab_y);
        free (in_buffer);
        free (out_buffer);
        return -3;
      }
      yy++;
    }


    /* free all */

    free (ptab_x);
    free (ptab_y);
    free (in_buffer);
    free (out_buffer);

    return 0;

#end unsafe

  }

end STRETCH;

/**************************************************************************/

public int resize_image2 (    IMAGE_INFO info,
                              uint       width,
                              uint       height,
                              uint       border_color = 0xFF000000,  // opaque black
                              bool       bestfit = true,
                          out IMAGE_INFO result,
                              bool       use_linear_colors = true,
                              bool       high_quality = true)
{
  IMAGE_INFO source, target;
  CLIP_INFO  source_clip, target_clip;
  byte[]^    buffer2;
  uint       size, i;

  source = info;

  target = {pixel  => null,
            width  => width,
            height => height };

  source_clip = { offset_x => 0,
                  offset_y => 0,
                  size_x   => source.width,
                  size_y   => source.height };

  target_clip = { offset_x => 0,
                  offset_y => 0,
                  size_x   => target.width,
                  size_y   => target.height };

  if (bestfit)
  {
    if (compute_bestfit_target_clip (source_clip, target_clip, out target_clip) < 0)
    {
      clear result;
  //    trace ("error: resize_image() : compute_bestfit_target_clip() failed\n");
      return -1;
    }
  }

  /* allocate target buffer */

  size = 4 * target.width * target.height;

  buffer2 = new byte[size];

  target.pixel = buffer2;

  if (target_clip.size_x != target.width ||
      target_clip.size_y != target.height)
  {
    for (i=0; i<size; i+=4)
      buffer2^[i:4] = border_color'byte;
  }

  if (stretch_image (source, source_clip, target, target_clip, use_linear_colors, high_quality => high_quality) < 0)
  {
    free buffer2;
    clear result;
//    trace ("error: resize_image() : stretch_image() failed\n");
    return -1;
  }

  result = { pixel  => buffer2,
             width  => target.width,
             height => target.height };

  return 0;
}

/**************************************************************************/

public int resize_image (ref IMAGE_INFO info,
                             uint       width,
                             uint       height,
                             uint       border_color = 0xFF000000,  // opaque black
                             bool       bestfit = true,
                             bool       use_linear_colors = true,
                             bool       high_quality = true)
{
  int        rc;
  IMAGE_INFO result;

  rc = resize_image2 (info, width, height, border_color, bestfit, out result, use_linear_colors, high_quality => high_quality);
  if (rc != 0)
    return rc;

  free (info.pixel);

  info = result;

  return 0;
}

/**************************************************************************/

public void reverse_clip_coordinates (uint      x,
                                      uint      y,
                                      CLIP_INFO source_clip,
                                      CLIP_INFO target_clip,
                                      out uint  out_x,
                                      out uint  out_y)
{
  uint   x0, y0;
  double center_x, center_y;

  x0 = x;
  y0 = y;

  if ((int)x0 < 0)
    x0 = 0;

  if ((int)y0 < 0)
    y0 = 0;

  if (x0 < (uint)target_clip.offset_x)
    x0 = (uint)target_clip.offset_x;

  if (y0 < (uint)target_clip.offset_y)
    y0 = (uint)target_clip.offset_y;

  if (x0 > (uint)target_clip.offset_x + target_clip.size_x)
    x0 = (uint)target_clip.offset_x + target_clip.size_x;

  if (y0 > (uint)target_clip.offset_y + target_clip.size_y)
    y0 = (uint)target_clip.offset_y + target_clip.size_y;

  x0 -= (uint)target_clip.offset_x;
  y0 -= (uint)target_clip.offset_y;

  if (target_clip.size_x > 0)
    center_x = (double)x / (double)target_clip.size_x;      /* range 0 .. 1 */
  else
    center_x = 0.0;

  if (target_clip.size_y > 0)
    center_y = (double)y / (double)target_clip.size_y;      /* range 0 .. 1 */
  else
    center_y = 0.0;

  x0 = (uint)source_clip.offset_x + (uint)(center_x * (double)source_clip.size_x);
  y0 = (uint)source_clip.offset_y + (uint)(center_y * (double)source_clip.size_y);

  if (x0 < (uint)source_clip.offset_x)
    x0 = (uint)source_clip.offset_x;

  if (y0 < (uint)source_clip.offset_y)
    y0 = (uint)source_clip.offset_y;

  if (x0 >= (uint)source_clip.offset_x + source_clip.size_x)
    x0 = (uint)source_clip.offset_x + source_clip.size_x - 1;

  if (y0 >= (uint)source_clip.offset_y + source_clip.size_y)
    y0 = (uint)source_clip.offset_y + source_clip.size_y - 1;

  out_x = x0;
  out_y = y0;
}

/**************************************************************************/

public int compute_zoom_clip_info
        (uint          center_x,      /* within source                 */
         uint          center_y,      /* within source                 */
         int           zoom_factor,   /* in percent (100 .. 6_500_000) */
         ref CLIP_INFO source_clip,   /* in out                        */
         ref CLIP_INFO target_clip)   /* in out                        */
{
  uint zoom_size_x, zoom_size_y;
  int  zoom_min_x, zoom_max_x, zoom_min_y, zoom_max_y;

  if (source_clip.size_x == 0 || source_clip.size_y == 0 ||
      target_clip.size_x == 0 || target_clip.size_y == 0 ||
      zoom_factor < 100       || zoom_factor > 6500000)
    return -1;

  if (target_clip.size_x * source_clip.size_y
      < target_clip.size_y * source_clip.size_x)
  {
    /* full image is wider than high (image is horizontal bar) */
    zoom_size_x = source_clip.size_x * 100 / (uint)zoom_factor;
    zoom_size_y = zoom_size_x * target_clip.size_y / target_clip.size_x;
  }
  else            /* black border left and right */
  {
    /* full image is higher than wide (image is vertical bar) */
    zoom_size_y = source_clip.size_y * 100 / (uint)zoom_factor;
    zoom_size_x = zoom_size_y * target_clip.size_x / target_clip.size_y;
  }

  if (zoom_size_x == 0)
    zoom_size_x = 1;

  if (zoom_size_y == 0)
    zoom_size_y = 1;

  zoom_min_x = (int)center_x - (int)(zoom_size_x / 2);
  zoom_max_x = zoom_min_x + (int)zoom_size_x;

  zoom_min_y = (int)center_y - (int)(zoom_size_y / 2);
  zoom_max_y = zoom_min_y + (int)zoom_size_y;

  if (zoom_min_x < source_clip.offset_x)
  {
    zoom_max_x += source_clip.offset_x - zoom_min_x;
    zoom_min_x = source_clip.offset_x;
  }

  if (zoom_min_y < source_clip.offset_y)
  {
    zoom_max_y += source_clip.offset_y - zoom_min_y;
    zoom_min_y = source_clip.offset_y;
  }

  if (zoom_max_x > (source_clip.offset_x+(int)source_clip.size_x))
  {
    zoom_min_x -= zoom_max_x - (source_clip.offset_x+(int)source_clip.size_x);
    zoom_max_x = (source_clip.offset_x+(int)source_clip.size_x);
  }

  if (zoom_max_y > (source_clip.offset_y+(int)source_clip.size_y))
  {
    zoom_min_y -= zoom_max_y - (source_clip.offset_y+(int)source_clip.size_y);
    zoom_max_y = (source_clip.offset_y+(int)source_clip.size_y);
  }

  if (zoom_min_x < source_clip.offset_x)
    zoom_min_x = source_clip.offset_x;

  if (zoom_min_y < source_clip.offset_y)
    zoom_min_y = source_clip.offset_y;

  source_clip.size_x = (uint)(zoom_max_x - zoom_min_x);
  source_clip.size_y = (uint)(zoom_max_y - zoom_min_y);

  source_clip.offset_x = zoom_min_x;
  source_clip.offset_y = zoom_min_y;

  return compute_bestfit_target_clip (source_clip, target_clip, out target_clip);
}

/**************************************************************************/

/* zoom image on user click and create new image for screen */

public int zoom_image
     (IMAGE_INFO     source,       /* source image */
      uint           click_x,      /* zoom x-center on screen */
      uint           click_y,      /* zoom y-center on screen */
      int            zoom,         /* in percent (100 .. 6_500_000) */
                                   /* use 100 for first call of new image ! */
      uint           width,        /* target screen width  */
      uint           height,       /* target screen height */
      ref CLIP_INFO  source_clip,  /* clear for first call, value must be kept between calls */
      ref CLIP_INFO  target_clip,  /* clear for first call, value must be kept between calls */
      uint           border_color, /* =0 */
      out IMAGE_INFO target,       /* result image for screen */
      bool       use_linear_colors = true,
      bool       high_quality = true)
{
  uint image_click_x, image_click_y, size, i;

  reverse_clip_coordinates (click_x,
                            click_y,
                            source_clip,
                            target_clip,
                            out image_click_x,
                            out image_click_y);

  clear target;

  target.width  = width;
  target.height = height;

  source_clip = { offset_x => 0,
                  offset_y => 0,
                  size_x   => source.width,
                  size_y   => source.height };

  target_clip = { offset_x => 0,
                  offset_y => 0,
                  size_x   => target.width,
                  size_y   => target.height };

  if (compute_zoom_clip_info (image_click_x, image_click_y,
                              zoom,
                              ref source_clip, ref target_clip) < 0)
  {
//    trace ("error: zoom_image() : compute_zoom_clip_info() failed\n");
    return -1;
  }

  size = 4 * target.width * target.height;

  target.pixel = new byte [size];


  if (target_clip.size_x != target.width ||
      target_clip.size_y != target.height)
  {
    for (i=0; i<size; i+=4)
      target.pixel^[i:4] = border_color'byte;
  }

  if (stretch_image (source, source_clip, target, target_clip, use_linear_colors => use_linear_colors, high_quality => high_quality) < 0)
  {
    free (target.pixel);
//    trace ("error: zoom_image() : stretch_image() failed\n", size);
    return -1;
  }

  return 0;
}

/**************************************************************************/

public int rotate_image2 (IMAGE_INFO     info,
                          int            angle,
                          uint           border_color = 0xFF000000,
                          out IMAGE_INFO result,
                          bool           use_linear_colors = true)
{
#begin unsafe

  clear result;

  switch (angle)
  {
    case 0:
    case 360:
      return copy_image (info, out result);

    case 90:
      {
        uint*   source, target;
        uint    width, height, size, x, y, ofs1, ofs2;
        byte[]^ ptarget;

        source = (uint *)&info.pixel^;
        width  = info.width;
        height = info.height;

        size   = 4 * width * height;
        ptarget = new byte [size];
        target = (uint *)&ptarget^;

        for (y=0; y<width; y++)
        {
          ofs1 = y*height;
          ofs2 = (height-1)*width + y;
          for (x=0; x<height; x++)
          {
            target[ofs1++] = source[ofs2];
            ofs2 -= width;
          }
        }

        result = { pixel  => ptarget,
                   width  => height,  // exchange width and height
                   height => width };
      }
      break;

    case 180:
      {
        uint*   source, target;
        uint    i, j, nb_pixels, size;
        byte[]^ ptarget;

        source      = (uint *)&info.pixel^;
        nb_pixels   = info.width * info.height;
        j           = nb_pixels - 1;

        size = 4 * nb_pixels;
        ptarget = new byte [size];
        target = (uint *)&ptarget^;

        for (i=0; i<nb_pixels; i++,j--)
          target[j] = source[i];

        result = { pixel  => ptarget,
                   width  => info.width,
                   height => info.height };
      }
      break;

    case 270:
      {
        uint*   source, target;
        uint    size, x, y, ofs1, ofs2;
        uint    width, height;
        byte[]^ ptarget;

        source = (uint *)&info.pixel^;
        width  = info.width;
        height = info.height;

        size   = 4 * width * height;
        ptarget = new byte [size];
        target = (uint *)&ptarget^;

        for (y=0; y<width; y++)
        {
          ofs1 = y*height;
          ofs2 = width-y-1;
          for (x=0; x<height; x++)
          {
            target[ofs1++] = source[ofs2];
            ofs2 += width;
          }
        }

        result = { pixel  => ptarget,
                   width  => height,  // exchange width and height
                   height => width };
      }
      break;

    default:
      {
        double  a, sina, cosa;
        byte*   source, target;
        uint    size2, r, g, b, s, x, y, r0, g0, b0, s0, width15, height15;
        uint    width, height, ofs, ofs2, width2, height2;
        uint    limit_width2, limit_height2;
        int     half_width, half_height, x2, y2, lsina, lcosa, x20;
        byte[]^ ptarget;

        if (use_linear_colors)
          build_linear_rgbs_conversion_tables ();

        if (use_linear_colors)
        {
          r0 = g_to_linear_space[border_color & 255];
          g0 = g_to_linear_space[(border_color >> 8) & 255];
          b0 = g_to_linear_space[(border_color >> 16) & 255];
        }
        else
        {
          r0 = border_color & 255;
          g0 = (border_color >> 8) & 255;
          b0 = (border_color >> 16) & 255;
        }

        s0 = (border_color >> 24) & 255;

        source = &info.pixel^;
        width  = info.width;
        height = info.height;

        a = - (double)angle * 3.1415927 / 180.0;
        sina = sin(a);
        cosa = cos(a);

        width2  = (uint)((double)width * fabs(cosa) + (double)height * fabs(sina));
        height2 = (uint)((double)width * fabs(sina) + (double)height * fabs(cosa));

        lsina = (int)(sina * 32768.0);
        lcosa = (int)(cosa * 32768.0);

        size2 = 4 * width2 * height2;
        ptarget = new byte [size2];
        target = (byte *)&ptarget^;

        ofs2 = 0;

        limit_height2 = height2 - height2 / 2;
        limit_width2  = width2  - width2  / 2;

        half_width  = (int)width  / 2;
        half_height = (int)height / 2;

        width15  = (width << 15);
        height15 = (height << 15);

        for (y2=-((int)height2/2); y2<(int)limit_height2; y2++)
        {
          x20 = -((int)width2/2);

          x = (uint)((x20 * lcosa) - (y2 * lsina) + (half_width  << 15));
          y = (uint)((x20 * lsina) + (y2 * lcosa) + (half_height << 15));

          for (x2=x20; x2<(int)limit_width2; x2++)
          {
            r = 0;
            g = 0;
            b = 0;
            s = 0;

// format:
// x = (x2 * lcosa) - (y2 * lsina) + ...
// y = (x2 * lsina) + (y2 * lcosa) + ...


            /* X2, Y2 */

            if (x < width15 && y < height15)
            {
              ofs = (((x >> 15) + width * (y >> 15)) << 2);

              if (use_linear_colors)
              {
                r += g_to_linear_space[source[ofs+0]];
                g += g_to_linear_space[source[ofs+1]];
                b += g_to_linear_space[source[ofs+2]];
              }
              else
              {
                r += source[ofs+0];
                g += source[ofs+1];
                b += source[ofs+2];
              }

              s += source[ofs+3];
            }
            else
            {
              r += r0;
              g += g0;
              b += b0;
              s += s0;
            }


            /* X2, Y2+1 */

            x -= (uint)lsina;
            y += (uint)lcosa;

            if (x < width15 && y < height15)
            {
              ofs = (((x >> 15) + width * (y >> 15)) << 2);

              if (use_linear_colors)
              {
                r += g_to_linear_space[source[ofs+0]];
                g += g_to_linear_space[source[ofs+1]];
                b += g_to_linear_space[source[ofs+2]];
              }
              else
              {
                r += source[ofs+0];
                g += source[ofs+1];
                b += source[ofs+2];
              }

              s += source[ofs+3];
            }
            else
            {
              r += r0;
              g += g0;
              b += b0;
              s += s0;
            }


            /* X2+1, Y2+1 */

            x += (uint)lcosa;
            y += (uint)lsina;

            if (x < width15 && y < height15)
            {
              ofs = (((x >> 15) + width * (y >> 15)) << 2);

              if (use_linear_colors)
              {
                r += g_to_linear_space[source[ofs+0]];
                g += g_to_linear_space[source[ofs+1]];
                b += g_to_linear_space[source[ofs+2]];
              }
              else
              {
                r += source[ofs+0];
                g += source[ofs+1];
                b += source[ofs+2];
              }

              s += source[ofs+3];
            }
            else
            {
              r += r0;
              g += g0;
              b += b0;
              s += s0;
            }


            /* X2+1, Y2 */

            x += (uint)lsina;
            y -= (uint)lcosa;

            if (x < width15 && y < height15)
            {
              ofs = (((x >> 15) + width * (y >> 15)) << 2);

              if (use_linear_colors)
              {
                r += g_to_linear_space[source[ofs+0]];
                g += g_to_linear_space[source[ofs+1]];
                b += g_to_linear_space[source[ofs+2]];
              }
              else
              {
                r += source[ofs+0];
                g += source[ofs+1];
                b += source[ofs+2];
              }

              s += source[ofs+3];
            }
            else
            {
              r += r0;
              g += g0;
              b += b0;
              s += s0;
            }

            if (use_linear_colors)
            {
              target[ofs2+0] = g_to_rgbs_space[r >> 2];
              target[ofs2+1] = g_to_rgbs_space[g >> 2];
              target[ofs2+2] = g_to_rgbs_space[b >> 2];
            }
            else
            {
              target[ofs2+0] = (byte)(r >> 2);
              target[ofs2+1] = (byte)(g >> 2);
              target[ofs2+2] = (byte)(b >> 2);
            }

            target[ofs2+3] = (byte)(s >> 2);

            ofs2 += 4;
          }
        }

        result = { pixel  => ptarget,
                   width  => width2,
                   height => height2 };
      }
      break;
  }

  return 0;

#end unsafe
}

/**************************************************************************/

public int rotate_image (ref IMAGE_INFO info,
                         int            angle,
                         uint           border_color = 0xFF000000,
                         bool           use_linear_colors = true)
{
  int        rc;
  IMAGE_INFO result;

  rc = rotate_image2 (info, angle, border_color, out result, use_linear_colors);
  if (rc != 0)
    return rc;

  free (info.pixel);
  info = result;

  return 0;
}

/**************************************************************************/

// extract rectangle of source image and create new target image with it

public int copy_image_rectangle (    IMAGE_INFO source_image,
                                     CLIP_INFO  source_clip,
                                 out IMAGE_INFO target_image)
{
  uint size, y, ofs1, ofs2, len;

  if (source_clip.size_x   > source_image.width ||
      source_clip.offset_x < 0 ||
      source_clip.offset_x > (int)(source_image.width - source_clip.size_x) ||
      source_clip.size_y   > source_image.height ||
      source_clip.offset_y < 0 ||
      source_clip.offset_y > (int)(source_image.height - source_clip.size_y))
  {
    clear target_image;
    return -1;
  }

  size = 4 * source_clip.size_x * source_clip.size_y;

  target_image = { pixel  => new byte[size],
                   width  => source_clip.size_x,
                   height => source_clip.size_y };

  ofs1 = 0;
  ofs2 = 4 * ((uint)source_clip.offset_y * source_image.width + (uint)source_clip.offset_x);
  len  = 4 * source_clip.size_x;

  for (y=0; y<source_clip.size_y; y++)
  {
    target_image.pixel^[ofs1:len] = source_image.pixel^[ofs2:len];
    ofs1 += (4 * source_clip.size_x);
    ofs2 += (4 * source_image.width);
  }

  return 0;
}

/**************************************************************************/

// replace clipped rectangle in target with new source image.
// target_clip can be partially or fully outside image range.

public int replace_image_rectangle (IMAGE_INFO source_image,
                                    CLIP_INFO  source_clip,
                                    IMAGE_INFO target_image,
                                    CLIP_INFO  target_clip)
{
  uint y, ofs1, ofs2, len, stride1, stride2;
  int  tofs_x, tofs_y, tsize_x, tsize_y, sofs_x, sofs_y;

  if (source_image.pixel^'size != 4 * source_image.width * source_image.height ||
      target_image.pixel^'size != 4 * target_image.width * target_image.height)
  {
    return -1;
  }

  if (target_clip.size_x != source_clip.size_x ||
      target_clip.size_y != source_clip.size_y)
  {
    return -1;
  }

  if (source_clip.size_x   > source_image.width ||
      source_clip.offset_x < 0 ||
      source_clip.offset_x > (int)(source_image.width - source_clip.size_x) ||
      source_clip.size_y   > source_image.height ||
      source_clip.offset_y < 0 ||
      source_clip.offset_y > (int)(source_image.height - source_clip.size_y))
  {
    return -1;
  }

  if (target_clip.offset_x + (int)target_clip.size_x <= 0 ||
      target_clip.offset_y + (int)target_clip.size_y <= 0 ||
      target_clip.offset_x >= (int)target_image.width ||
      target_clip.offset_y >= (int)target_image.height)
  {
    return 0;   // target clip is fully outside target image : nothing to do
  }


  if (target_clip.offset_x >= 0)  // in range
  {
    sofs_x  = source_clip.offset_x;
    tofs_x  = target_clip.offset_x;
    tsize_x = (int)target_clip.size_x;
  }
  else    // x cut at start
  {
    sofs_x  = source_clip.offset_x - target_clip.offset_x;
    tofs_x  = 0;
    tsize_x = (int)target_clip.size_x + target_clip.offset_x;
  }

  if (tofs_x + tsize_x > (int)target_image.width)   // x cut at end
  {
    tsize_x = (int)target_image.width - tofs_x;
  }


  if (target_clip.offset_y >= 0)  // in range
  {
    sofs_y  = source_clip.offset_y;
    tofs_y  = target_clip.offset_y;
    tsize_y = (int)target_clip.size_y;
  }
  else    // y cut at start
  {
    sofs_y  = source_clip.offset_y - target_clip.offset_y;
    tofs_y = 0;
    tsize_y = (int)target_clip.size_y + target_clip.offset_y;
  }

  if (tofs_y + tsize_y > (int)target_image.height)   // y cut at end
  {
    tsize_y = (int)target_image.height - tofs_y;
  }


  ofs1 = 4 * ((uint)sofs_y * source_image.width + (uint)sofs_x);
  ofs2 = 4 * ((uint)tofs_y * target_image.width + (uint)tofs_x);
  len  = 4 * (uint)tsize_x;
  stride1 = 4 * source_image.width;
  stride2 = 4 * target_image.width;

  for (y=0; y<(uint)tsize_y; y++)
  {
    target_image.pixel^[ofs2:len] = source_image.pixel^[ofs1:len];
    ofs1 += stride1;
    ofs2 += stride2;
  }

  return 0;
}

/**************************************************************************/
