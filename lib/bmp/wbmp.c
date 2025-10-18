
/* bmp2.c : BMP image writer */

use ../stream;

/********************************************************************/

struct BMP
{
  short     status;            /* usually -1, set to 0 if the file is complete */

  /* bmp header data */
  uint      image_x_size;
  uint      image_y_size;

  /* read data */

  uint      size_per_line;    /* (x_size * 3) rounded up to multiple of 4 */
  uint      max_size;         /* total nb bytes of image */

  byte[]^   pout;             /* output buffer (size_per_line bytes) */
  uint      iout;

  uint      pixels_written_on_this_line;
  uint      pixels_written;   /* total pixels written until now */
  uint      total_pixels;     /* width x height */
  long      seek_pos;
}

/*************************************************************************/

packed struct HEADER
{
  char   bm[2];           /* "BM" */
  uint4  file_size;
  uint4  reserved;
  uint4  offset_to_data;  /* sizeof(HEADER) */

  uint4  biSize;          /* = 40 */
  uint4  biWidth;
  uint4  biHeight;
  uint2  biPlanes;        /* = 1 */
  uint2  biBitCount;      /* = 24 (bits/pixel) */
  uint4  biCompression;   /* = 0 (no compression) */
  uint4  biSizeImage;     /* = size of image, with each line rounded up to M4 */
  uint4  biXPelsPerMeter; /* = 0 */
  uint4  biYPelsPerMeter; /* = 0 */
  uint4  biClrUsed;       /* = 0 */
  uint4  biClrImportant;  /* = 0 */
}

/*************************************************************************/

/* round up to a multiple of 4 */

uint align4 (uint x)
{
  return (((x) + 3) / 4) * 4;
}

/*************************************************************************/

int write_block (ref WRITE_STREAM stream, byte[] buffer)
{
  if (write (ref stream, buffer) != buffer'length)
    return BMP2_IO_ERROR;
  return 0;
}

/********************************************************************/

int write_bmp_header (BMP t, ref WRITE_STREAM stream)
{
  HEADER h;

  clear h;
  h.bm = "BM";
  h.file_size       = h'size + t.max_size;
  h.offset_to_data  = h'size;
  h.biSize          = 40;
  h.biWidth         = t.image_x_size;
  h.biHeight        = t.image_y_size;
  h.biPlanes        = 1;
  h.biBitCount      = 24;
  h.biCompression   = 0;
  h.biSizeImage     = t.max_size;

  return write_block (ref stream, h);
}

/*************************************************************************/

public int create_bmp (out BMP bmp,
                       ref WRITE_STREAM stream,
                       uint    width,
                       uint    height)
{
  ref BMP t = bmp;
  int  rc;

  clear t;

  if (width == 0)
    return BMP2_ILLEGAL_WIDTH;

  if (height == 0 || width > (uint'max/8) / height)
    return BMP2_ILLEGAL_HEIGHT;

  t.image_x_size   = width;
  t.image_y_size   = height;
  t.size_per_line  = align4 (width * 3);
  t.max_size       = t.size_per_line * height;
  t.total_pixels   = width * height;
  t.seek_pos       = HEADER'size + t.max_size - t.size_per_line;

  t.status = -1;      /* default : file is incomplete */

  /* write the header */
  rc = write_bmp_header (t, ref stream);
  if (rc < 0)
  {
    (void)close_bmp2 (ref bmp);
    return rc;
  }

  t.pout = new byte [t.size_per_line];
  t.iout = 0;

  return 0;
}

/*************************************************************************/

public int write_bmp (ref BMP bmp, byte[] buffer, ref WRITE_STREAM stream)
{
  ref BMP t = bmp;
  uint4  nb_pixels, rest_pixels, i;
  int    rc, j;

  if ((buffer'size & 3) != 0)
    return BMP2_ILLEGAL_BUFFER_SIZE;    /* size is not multiple of 4 */

  nb_pixels = (buffer'size >> 2);

  if (t.pixels_written + nb_pixels > t.total_pixels)
    return BMP2_TOO_MUCH_DATA;

  j = 0;   // index into buffer

  while (nb_pixels > 0)
  {
    /* compute rest pixels to write for this line */
    rest_pixels = t.image_x_size - t.pixels_written_on_this_line;
    if (rest_pixels > nb_pixels)
      rest_pixels = nb_pixels;

    for (i=0; i<rest_pixels; i++)
    {
      t.pout^[t.iout:3] = { buffer[j+2], buffer[j+1], buffer[j]};
      t.iout += 3;
      j += 4;
    }

    t.pixels_written_on_this_line += rest_pixels;
    t.pixels_written += rest_pixels;

    if (t.pixels_written_on_this_line == t.image_x_size)  /* line complete */
    {
      t.pixels_written_on_this_line = 0;
      t.iout = 0;

      if (lseekw (ref stream, t.seek_pos, SEEK_SET) < 0)
        return BMP2_SEEK_ERROR;
      t.seek_pos -= t.size_per_line;

      rc = write_block (ref stream, t.pout^);
      if (rc < 0)
        return rc;

      if (t.pixels_written == t.total_pixels)
      {
        /* set file status to OK */
        t.status = 0;
      }
    }

    nb_pixels -= rest_pixels;
  }

  return 0;
}

/*************************************************************************/

public void get_bmp_size2 (BMP      bmp,
                           out uint width,
                           out uint height)
{
  width  = bmp.image_x_size;
  height = bmp.image_y_size;
}

/**************************************************************************/

public int close_bmp2 (ref BMP bmp)
{
  ref BMP t = bmp;
  int     retcode;

  if (t.status < 0)
    retcode = BMP2_INCOMPLETE;
  else
    retcode = 0;

  free t.pout;

  clear t;

  return retcode;
}

/*************************************************************************/
