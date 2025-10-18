
/* rjpg.c : jpeg decompression */

use ../stream;
use ../image;

use jpeglib, jmorecfg, jdapimin, jdapistd, jdatasrc, jerror;

#begin unsafe

/**************************************************************************/

struct my_error_mgr {
  jpeg_error_mgr pub;             /* "public" fields */
}

typedef my_error_mgr *my_error_ptr;

typedef uint LONG;

/**************************************************************************/

struct JPG
{
  jpeg_decompress_struct cinfo;
  my_error_mgr           jerr;
  byte[]^                buffer;      /* Output row buffer */
  LONG                   buffer_size;
  LONG                   buffer_index;
  LONG                   pending_count;
  uint                   x_resolution;
  uint                   y_resolution;
}

/**************************************************************************/

public int close_jpg (ref JPG jpg)
{
  JPG* t = &jpg;

  jpeg_destroy_decompress (&t->cinfo);
  free t->buffer;

  return 0;
}

/**************************************************************************/

public void get_jpg_size (JPG      jpg,
                          out uint width,
                          out uint height)
{
  width  = (uint)jpg.cinfo.output_width;
  height = (uint)jpg.cinfo.output_height;
}

/**************************************************************************/

public int read_jpg (ref JPG jpg, out byte[] buffer)
{
  JPG*       t = &jpg;
  LONG       nb_pixels;
  JSAMPROW   table[1];
  byte       *pbuffer = &buffer;

  if ((buffer'size & 3) != 0)
    return JPG_ILLEGAL_BUFFER_SIZE;    /* size is not multiple of 4 */

  nb_pixels = (buffer'size >> 2);  /* nb of pixels to fetch from image. */
                            /* (this counter decreases til zero) */

  for (;;)
  {
    if (t->pending_count > 0)   /* pixels left from last jpg library call */
    {
      LONG len, i;

      /* compute nb of pixels to copy */
      if (nb_pixels > t->pending_count)
        len = t->pending_count;
      else
        len = nb_pixels;

      /* write the pixels */
      if (t->cinfo.output_components == 1) /* black & white picture */
      {
        for (i=0; i<len; i++)
        {
          *pbuffer++ = (byte)t->buffer^[t->buffer_index];
          *pbuffer++ = (byte)t->buffer^[t->buffer_index];
          *pbuffer++ = (byte)t->buffer^[t->buffer_index++];
          *pbuffer++ = 255;
        }
      }
      else if (t->cinfo.output_components == 3)  /* color picture (3 components) */
      {
        ref LONG source_index = t->buffer_index;
        byte* psource = &t->buffer^[source_index];
        
        for (i=0; i<len; i++)
        {
          pbuffer[0:3] = psource[0:3];
          pbuffer += 3;
          psource += 3;
          *pbuffer++ = 255;
        }
        source_index += 3*len;
      }
      else if (t->cinfo.output_components == 4)  /* color picture with alpha channel (4 components) */
      {
        LONG size = 4*len;
        pbuffer[0:size] = t->buffer^[t->buffer_index:size];
        pbuffer += size;
        t->buffer_index += size;
      }

      nb_pixels        -= len;
      t->pending_count -= len;
    }


    if (nb_pixels == 0)    /* output buffer is full */
      return 0;


    /* read additional data into buffer */

    if (t->cinfo.output_scanline >= t->cinfo.output_height)
      return JPG_IO_END_OF_IMAGE;

    table[0] = &t->buffer^;
    if (jpeg_read_scanlines (&t->cinfo, &table, 1) != 1)
      return JPG_IO_END_OF_IMAGE;

    t->buffer_index  = 0;
    t->pending_count = t->cinfo.output_width;
  }
}

/**************************************************************************/

public void get_jpg_attributes (JPG                  jpg,
                                out IMAGE_ATTRIBUTES attr)
{
  clear attr;
  attr.x_resolution = jpg.x_resolution;
  attr.y_resolution = jpg.y_resolution;
}

/**************************************************************************/

public int open_jpg (out JPG jpg, ref READ_STREAM stream)
{
  JPG* t = &jpg;
  int  rc;

  clear jpg;
  
  t->cinfo.err = jpeg_std_error (&t->jerr.pub);

  rc = jpeg_CreateDecompress (&t->cinfo, JPEG_LIB_VERSION, jpeg_decompress_struct'size);
  if (rc != 0)
  {
    (void)close_jpg (ref jpg);
    return JPG_LIBRARY_ERROR;
  }

  jpeg_stdio_src (&t->cinfo, stream);

  if (jpeg_read_header (&t->cinfo, TRUE) != JPEG_HEADER_OK)
  {
    (void)close_jpg (ref jpg);
    return JPG_SHORT_FILE;
  }

  /* set parameters here ... */

  if (jpeg_start_decompress (&t->cinfo) == FALSE)
  {
    (void)close_jpg (ref jpg);
    return JPG_SHORT_FILE;
  }

  /* we have the image dimensions and colormap available now ... */

  if (t->cinfo.output_components != 1 && t->cinfo.output_components != 3 && t->cinfo.output_components != 4)
  {
    (void)close_jpg (ref jpg);
    return JPG_BAD_COMPONENTS;
  }

  t->buffer_size = (uint)t->cinfo.output_width * (uint)t->cinfo.output_components;
  t->buffer = new byte[t->buffer_size];

  if (t->cinfo.density_unit == 1)   /* pixels per inch */
  {
    t->x_resolution = t->cinfo.X_density;
    t->y_resolution = t->cinfo.Y_density;
  }

  return 0;
}

/**************************************************************************/

#end unsafe
