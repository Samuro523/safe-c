
/* wjpg.c : jpeg compression */

use ../stream;
use ../arithm;
use jpeglib, jmorecfg, jcomapi, jcapimin, jcparam, jcapistd, jerror, jdatadst;

#begin unsafe

/**************************************************************************/

struct my_error_mgr
{
  jpeg_error_mgr pub;             /* "public" fields */
}

typedef my_error_mgr *my_error_ptr;

typedef uint LONG;

/**************************************************************************/

struct JPG
{
  jpeg_compress_struct   cinfo;
  my_error_mgr           jerr;
  byte[]^                buffer;      /* Input row buffer */
  LONG                   buffer_size;  /* total size */
  LONG                   write_index;  /* write index into buffer */
}

/**************************************************************************/

public int close_jpg2 (ref JPG jpg)
{
  JPG  *t = &jpg;
  int  retcode;

  retcode = 0;

  if (t->cinfo.next_scanline < t->cinfo.image_height)
    retcode = JPG_IO_END_OF_IMAGE;

  jpeg_destroy ((j_common_ptr)&t->cinfo);

  free t->buffer;

  clear jpg;

  return retcode;
}

/**************************************************************************/

public void get_jpg_size2 (JPG      jpg,
                           out uint width,
                           out uint height)
{
  width  = jpg.cinfo.image_width;
  height = jpg.cinfo.image_height;
}

/**************************************************************************/

public 
int create_jpg (out JPG          jpg,
                ref WRITE_STREAM stream,
                    ushort       width,
                    ushort       height,
                    ushort       quality,       /* 0..100 : 0=WORSE, 100=BEST */
                    bool         output_grey,   /* false = color, true = grey */
                    bool         output_alpha,  /* true = save also alpha channel, this creates a non-standard jpeg ! */
                    uint         x_resolution,  /* 0 = undefined */
                    uint         y_resolution)  /* 0 = undefined */
{
  JPG *t = &jpg;
  int rc;

  clear jpg;

  if (quality > 100)
    return JPG_BAD_PARAMETER;

  t->cinfo.err = jpeg_std_error (&t->jerr.pub);

  rc = jpeg_CreateCompress (&t->cinfo, JPEG_LIB_VERSION, jpeg_compress_struct'size);
  if (rc != 0)
  {
    (void)close_jpg2 (ref jpg);
    return JPG_LIBRARY_ERROR;
  }

  jpeg_stdio_dest (&t->cinfo, ref stream);

  t->cinfo.image_width  = width;
  t->cinfo.image_height = height;

  if (output_alpha)   /* save also alpha channel, this creates a non-standard jpeg ! */
  {
    t->cinfo.input_components = 4;
    t->cinfo.in_color_space   = JCS_RGBA;
  }
  else
  {
    t->cinfo.input_components = output_grey ? 1 : 3;
    t->cinfo.in_color_space   = output_grey ? JCS_GRAYSCALE : JCS_RGB;
  }

  jpeg_set_defaults (&t->cinfo);
  jpeg_set_quality  (&t->cinfo, quality, TRUE);

  if ((x_resolution | y_resolution) != 0)
  {
    t->cinfo.density_unit = 1;    /* pixels per inch */
    t->cinfo.X_density = (ushort)x_resolution;
    t->cinfo.Y_density = (ushort)y_resolution;
  }

  rc = jpeg_start_compress (&t->cinfo, TRUE);
  if (rc != 0)
  {
    (void)close_jpg2 (ref jpg);
    return JPG_LIBRARY_ERROR;
  }

  t->buffer_size = width * (uint)t->cinfo.input_components;
  t->buffer = new byte[t->buffer_size];

  return 0;
}

/**************************************************************************/

public int write_jpg (ref JPG jpg, byte[] buffer)
{
  JPG *      t = &jpg;
  LONG       nb_pixels;
  JSAMPROW   table[1];
  byte       *pbuffer = &buffer;
  int        rc;

  if ((buffer'size & 3) != 0)
    return JPG_ILLEGAL_BUFFER_SIZE;    /* size is not multiple of 4 */

  nb_pixels = (buffer'size >> 2);  /* nb of pixels to store. */
                                   /* (this counter decreases til zero) */

  while (nb_pixels > 0)
  {
    /* copy pixels from buffer[] into row buffer until a row is full */

    if (t->cinfo.input_components == 1)   /* create grayscale image */
    {
      while (nb_pixels > 0 && t->write_index < t->buffer_size)
      {
        t->buffer^[t->write_index++] =
             (byte)((30*(short)pbuffer[0]
                   + 59*(short)pbuffer[1]
                   + 11*(short)pbuffer[2]) / 100);
        pbuffer += 4;
        nb_pixels--;
      }
    }
    else if (t->cinfo.input_components == 3)   /* create color image */
    {
      while (nb_pixels > 0 && t->write_index < t->buffer_size)
      {
        t->buffer^[t->write_index++] = *pbuffer++;
        t->buffer^[t->write_index++] = *pbuffer++;
        t->buffer^[t->write_index++] = *pbuffer++;
        pbuffer++;                 /* skip 's' component */
        nb_pixels--;
      }
    }
    else if (t->cinfo.input_components == 4)  /* create color image with alpha channel */
    {
      uint len = umin (nb_pixels, (t->buffer_size - t->write_index) >> 2);
      uint size = len << 2;
      t->buffer^[t->write_index:size] = pbuffer[0:size];
      t->write_index += size;
      pbuffer += size;
      nb_pixels -= len;
    }

    if (t->write_index == t->buffer_size)   /* row is full */
    {
      t->write_index = 0;

      if (t->cinfo.next_scanline == t->cinfo.image_height)
      {
        return JPG_TOO_MUCH_DATA;
      }

      table[0] = &t->buffer^;
      if (jpeg_write_scanlines (&t->cinfo, &table, 1) != 1)
      {
        return JPG_LIBRARY_ERROR;
      }

      if (t->cinfo.next_scanline == t->cinfo.image_height)
      {
        rc = jpeg_finish_compress (&t->cinfo);
        if (rc != 0)
        {
          return JPG_LIBRARY_ERROR;
        }

        if (nb_pixels != 0)
        {
          return JPG_TOO_MUCH_DATA;
        }
      }
    }
  }

  return 0;
}

/**************************************************************************/

#end unsafe
