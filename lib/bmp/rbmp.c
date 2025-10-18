
/*************************************/
/* bmp.c : BMP (or DIB) image reader */
/*************************************/

/* notes:
  . tested     : 1, 4, 8, 24 bits.
  . not tested : 16 and 32 bits with bitmasks !
*/

use ../stream;
use ../image;          // for error codes

#define debug false

/********************************************************************/

const uint BI_RGB       = 0;
const uint BI_RLE8      = 1;
const uint BI_RLE4      = 2;
const uint BI_BITFIELDS = 3;

packed struct RGBQUAD
{
  byte b;
  byte g;
  byte r;
  byte reserved;
}

/********************************************************************/

typedef byte[]^ PLINE;

struct BMP
{
  uint  width;
  uint  height;
  byte  reverse;         /* 0 = lines are top-down, */
                         /* 1 = lines are bottom-up */

  uint2 bits_per_pixel;  /* can be 1, 4, 8, 16, 24 or 32 */
  uint  compression;     /* BI_RGB, BI_RLE4, BI_RLE8, BI_BITFIELDS */

  RGBQUAD color_map[256];  /* only used if bits_per_pixel <= 8 */

  uint  mask[3];         /* 3 RGB BI_BITFIELDS masks (16 or 32) */
  uint  rshift[3];       /* right shift */
  uint  land[3];         /* logical and */
  uint  lshift[3];       /* left shift */

  PLINE[]^ line;         /* indirect access to lines */
  uint  line_size;       /* size of a line[] */

  uint  write_y;         /* index of next line to write */
  int   write_dy;        /* increment to next write line index */
  uint  write_count_y;   /* nb of lines left to write */

  uint  read_y;          /* index of next line to read */

  byte[]^ buffer;         /* contains output RGB image line */
  uint    buffer_index;    /* index of next pending byte */
  uint    pending_count;   /* nb pixels left to send */

  byte[]^ in_buffer;      /* 4K input buffer for RLE4 & RLE8 */
  uint    in_buffer_index; /* next byte to be read */
  uint    in_buffer_rest;  /* nb bytes remaining in buffer at index */
}

/********************************************************************/

public int close_bmp (ref BMP bmp)
{
  ref BMP t = bmp;
  uint i;

  if (t.line != null)
  {
    for (i=0; i<t.height; i++)
      free t.line^[i];
    free t.line;
  }

  free t.buffer;
  free t.in_buffer;

  clear t;

  return 0;
}

/**************************************************************************/

public void get_bmp_size (BMP      bmp,
                          out uint width,
                          out uint height)
{
  width  = bmp.width;
  height = bmp.height;
}

/**************************************************************************/

int write_uncompressed (ref BMP t, ref READ_STREAM stream)
{
  int rc;

  if (t.write_count_y == 0)   /* all is done */
    return 0;

  if (t.line^[t.write_y] == null)   /* line not allocated */
  {
    t.line^[t.write_y] = new byte [t.line_size];
  }


  /* read entire next line */

  rc = read (ref stream, out t.line^[t.write_y]^);
  if (rc != (int)t.line_size)
    return IMG_FILE_ERROR;

  t.write_y += (uint)t.write_dy;
  t.write_count_y--;

  return 0;
}

/**************************************************************************/

int read_in_buffer (ref BMP t, out byte[] data, ref READ_STREAM stream)
{
  uint len, j, size;
  int  rc;

  clear data;

  size = data'size;
  j = 0;

  for (;;)
  {
    /* fill data with remaining bytes in 'in_buffer' */
    if (size > t.in_buffer_rest)
      len = t.in_buffer_rest;
    else
      len = size;

    data[j:len] = t.in_buffer^[t.in_buffer_index:len];

    j                  += len;
    size               -= len;
    t.in_buffer_index += len;
    t.in_buffer_rest  -= len;

    if (size == 0)
      return 0;


    /* assertion : t.in_buffer_rest == 0 */

    /* load in_buffer with additional data (at least 1 byte) */

    rc = read (ref stream, out t.in_buffer^);
    if (rc < 0)
      return IMG_FILE_ERROR;

    if (rc < 1)
      return BMP_UNEXPECTED_EOF;

    t.in_buffer_index = 0;
    t.in_buffer_rest  = (uint)rc;
  }
}

/**************************************************************************/

int write_rle4 (ref BMP t, ref READ_STREAM stream)
{
  int  rc;
  uint x;
  byte by[2];

  if (t.write_count_y == 0)   /* all is done */
    return 0;

  if (t.line^[t.write_y] == null)   /* line not allocated */
  {
    t.line^[t.write_y] = new byte [t.line_size];
  }

  x = 0;                       /* attention: x is multiplied by 2 ! */

  while (x < t.width)
  {
    ref byte[] target = t.line^[t.write_y]^;

    rc = read_in_buffer (ref t, out by, ref stream);
    if (rc < 0)
      return rc;

    if (by[0] != 0)           /* repeat count */
    {
      int  count;
      byte c1, c2;

      count = by[0];

      if (x + (uint)count > t.width)
        return BMP_FORMAT_ERROR;

      c1 = (byte)((by[1] >> 4) & 15);
      c2 = (byte)(by[1] & 15);

      for (;;)
      {
        if ((x & 1) == 0)             /* fill high-order bit */
          target[x>>1] = (byte)(c1 << 4);
        else                          /* fill low-order bit */
          target[x>>1] |= c1;

        x++;
        count--;
        if (count == 0)
          break;

        if ((x & 1) == 0)             /* fill high-order bit */
          target[x>>1] = (byte)(c2 << 4);
        else                          /* fill low-order bit */
          target[x>>1] |= c2;

        x++;
        count--;
        if (count == 0)
          break;
      }
    }
    else   /* first byte is zero */
    {
      int  count;
      byte c1, c2;

      count = by[1];

      if (count > 2)     /* copy 'count' nybbles directly */
      {
        if (x + (uint)count > t.width)
          return BMP_FORMAT_ERROR;

        for (;;)
        {
          rc = read_in_buffer (ref t, out by, ref stream);
          if (rc < 0)
            return rc;

          c1 = (byte)((by[0] >> 4) & 15);
          c2 = (byte)(by[0] & 15);

          if ((x & 1) == 0)             /* fill high-order bit */
            target[x>>1] = (byte)(c1 << 4);
          else                          /* fill low-order bit */
            target[x>>1] |= c1;

          x++;
          count--;
          if (count == 0)
            break;

          if ((x & 1) == 0)             /* fill high-order bit */
            target[x>>1] = (byte)(c2 << 4);
          else                          /* fill low-order bit */
            target[x>>1] |= c2;

          x++;
          count--;
          if (count == 0)
            break;

          c1 = (byte)((by[1] >> 4) & 15);
          c2 = (byte)(by[1] & 15);

          if ((x & 1) == 0)             /* fill high-order bit */
            target[x>>1] = (byte)(c1 << 4);
          else                          /* fill low-order bit */
            target[x>>1] |= c1;

          x++;
          count--;
          if (count == 0)
            break;

          if ((x & 1) == 0)             /* fill high-order bit */
            target[x>>1] = (byte)(c2 << 4);
          else                          /* fill low-order bit */
            target[x>>1] |= c2;

          x++;
          count--;
          if (count == 0)
            break;
        }
      }
      else if (count == 0)    /* end-of-line */
      {
        if (x != 0)
          return BMP_FORMAT_ERROR;
      }
      else if (count == 1)   /* end-of-bitmap */
      {
        return BMP_FORMAT_ERROR;
      }
      else   /* 2 : delta */
      {
        return BMP_DELTA_UNSUPPORTED;
      }
    }
  }

  t.write_y += (uint)t.write_dy;
  t.write_count_y--;

  return 0;
}

/**************************************************************************/

int write_rle8 (ref BMP t, ref READ_STREAM stream)
{
  int  rc;
  uint x;
  byte by[2];

  if (t.write_count_y == 0)   /* all is done */
    return 0;

  if (t.line^[t.write_y] == null)   /* line not allocated */
  {
    t.line^[t.write_y] = new byte [t.line_size];
  }


  x = 0;

  while (x < t.width)
  {
    ref byte[] target = t.line^[t.write_y]^;

    rc = read_in_buffer (ref t, out by, ref stream);
    if (rc < 0)
      return rc;

    if (by[0] != 0)
    {
      if (by[0] + x > t.width)
        return BMP_FORMAT_ERROR;

      target[x:by[0]] = {all=> by[1]};
      x += by[0];
    }
    else   /* first byte is zero */
    {
      if (by[1] > 2)
      {
        /* copy by[0] bytes directly */
        if (by[1] + x > t.width)
          return BMP_FORMAT_ERROR;

        rc = read_in_buffer (ref t, out target[x:by[1]], ref stream);
        if (rc < 0)
          return rc;

        x += by[1];

        if ((by[1] & 1) != 0)   /* odd value : read 1 extra byte */
        {
          rc = read_in_buffer (ref t, out by[0], ref stream);
          if (rc < 0)
            return rc;
        }
      }
      else if (by[1] == 0)    /* end-of-line */
      {
        if (x != 0)
          return BMP_FORMAT_ERROR;
      }
      else if (by[1] == 1)   /* end-of-bitmap */
      {
        return BMP_FORMAT_ERROR;
      }
      else   /* 2 : delta */
      {
        return BMP_DELTA_UNSUPPORTED;
      }
    }
  }

  t.write_y += (uint)t.write_dy;
  t.write_count_y--;

  return 0;
}

/**************************************************************************/

public int read_bmp (ref BMP bmp, out byte[] buffer, ref READ_STREAM stream)
{
  ref BMP t = bmp;
  uint nb_pixels, j;
  int  rc;

  clear buffer;

  if ((buffer'size & 3) != 0)
    return IMG_ILLEGAL_BUFFER_SIZE;    /* size is not multiple of 4 */

  nb_pixels = (buffer'size >> 2);  /* nb of pixels to fetch from image. */
                                   /* (this counter decreases til zero) */

  j = 0;  // index into buffer

  for (;;)
  {
    if (t.pending_count > 0)   /* pixels left to send */
    {
      uint len;

      /* compute nb of pixels to copy */
      if (nb_pixels > t.pending_count)
        len = t.pending_count;
      else
        len = nb_pixels;

      /* write the pixels */
      buffer[j:4*len] = t.buffer^[t.buffer_index:4*len];

      nb_pixels       -= len;
      t.pending_count -= len;
      j               += 4*len;
      t.buffer_index  += 4*len;
    }


    if (nb_pixels == 0)    /* output buffer is full */
      return 0;


    /* assertion : t.pending_count == 0L */


    /* read additional data into output buffer (at most nb_pixels) */

    if (t.read_y == t.height)
      return IMG_END_OF_IMAGE;


    /* read a full line into the indirect buffer line[] */

    if      (t.compression == BI_RGB)  rc = write_uncompressed (ref t, ref stream);
    else if (t.compression == BI_RLE4) rc = write_rle4         (ref t, ref stream);
    else if (t.compression == BI_RLE8) rc = write_rle8         (ref t, ref stream);
    else                               return BMP_INTERN_ERROR;

    if (rc < 0)
      return rc;


    /* if a line is available, convert it to output buffer */

    if (t.pending_count == 0 &&
        t.line^[t.read_y] != null &&
        t.read_y != t.write_y)
    {
      {
        ref byte[] psource = t.line^[t.read_y]^;
        ref byte[] ptarget = t.buffer^;
        ref byte[] pmap    = t.color_map ' byte;

        uint count, lcolor, r, g, b, isource, itarget;
        byte bit, color, bits_right;

        isource = 0;
        itarget = 0;
        count   = t.width;

        switch (t.bits_per_pixel)
        {
          case 1:
            bit = 128;
            while (count-- > 0)
            {
              color = (byte)((psource[isource] & bit) != 0);

              ptarget[itarget:4] = pmap[color<<2:4];
              itarget += 4;

              bit >>= 1;
              if (bit == 0)
              {
                bit = 128;
                isource++;
              }
            }
            break;

          case 4:
            bits_right = 4;
            while (count-- > 0)
            {
              color = (byte)((psource[isource] >> bits_right) & 15);

              ptarget[itarget:4] = pmap[color<<2:4];
              itarget += 4;

              if (bits_right == 4)
                bits_right = 0;
              else
              {
                bits_right = 4;
                isource++;
              }
            }
            break;

          case 8:
            while (count-- > 0)
            {
              color = psource[isource++];

              ptarget[itarget:4] = pmap[color<<2:4];
              itarget += 4;
            }
            break;

          case 16:
            while (count-- > 0)
            {
              lcolor = psource[isource] + (psource[isource+1] << 8);

              r = lcolor & t.mask[2];
              g = lcolor & t.mask[1];
              b = lcolor & t.mask[0];

              ptarget[itarget+0] = (byte)(((r >> t.rshift[2]) & t.land[2])
                                           << t.lshift[2]);
              ptarget[itarget+1] = (byte)(((g >> t.rshift[1]) & t.land[1])
                                           << t.lshift[1]);
              ptarget[itarget+2] = (byte)(((b >> t.rshift[0]) & t.land[0])
                                           << t.lshift[0]);
              ptarget[itarget+3] = 255;

              isource += 2;
              itarget += 4;
            }
            break;

          case 24:
            while (count-- > 0)
            {
              ptarget[itarget+0] = psource[isource+2];     /* convert BGR to RGBS */
              ptarget[itarget+1] = psource[isource+1];
              ptarget[itarget+2] = psource[isource+0];
              ptarget[itarget+3] = 255;
              isource += 3;
              itarget += 4;
            }
            break;

          case 32:
            while (count-- > 0)
            {
              lcolor = psource[isource+0]
                     + (psource[isource+1] << 8)
                     + (psource[isource+2] << 16)
                     + (psource[isource+3] << 24);

              r = lcolor & t.mask[2];
              g = lcolor & t.mask[1];
              b = lcolor & t.mask[0];

              ptarget[itarget+0] = (byte)(((r >> t.rshift[2]) & t.land[2])
                                       << t.lshift[2]);
              ptarget[itarget+1] = (byte)(((g >> t.rshift[1]) & t.land[1])
                                       << t.lshift[1]);
              ptarget[itarget+2] = (byte)(((b >> t.rshift[0]) & t.land[0])
                                       << t.lshift[0]);
              ptarget[itarget+3] = 255;

              isource += 4;
              itarget += 4;
            }
            break;

          default:
            return BMP_INTERN_ERROR;
        }

        t.buffer_index = 0;
        t.pending_count = t.width;
      }

      /* we can now free the line */
      free t.line^[t.read_y];
      t.line^[t.read_y] = null;
      t.read_y++;
    }
  }
}

/**************************************************************************/

public int open_bmp (out BMP bmp, ref READ_STREAM stream)
{
  ref BMP t = bmp;

  int   rc;
  byte  header[14];
  uint  bit_offset;
  int   temp;
  long  pos;
  byte  info[20], by[4];
  uint  i, qlen;

  clear t;

#if debug
  trace ("read 14 bytes\n");
#endif

  /* read 14 first bytes to identify BMP file format */

  rc = read (ref stream, out header);
  if (rc != (int)header'size || header[0] != (byte)'B' || header[1] != (byte)'M' ||
                                header[6] != 0 || header[7] != 0)
  {
#if debug
  trace ("not BMP file\n");
#endif
    (void)close_bmp (ref bmp);
    return IMG_NOT_RECOGNIZED;
  }


  /* compute offset of bitmap within file */

  bit_offset = (header[10]      )
             + (header[11] << 8 )
             + (header[12] << 16)
             + (header[13] << 24);


#if debug
  trace ("read bitmapinfoheader\n");
#endif

  /* read bitmapinfoheader */

  pos = lseekr (ref stream, 0L, SEEK_CUR);    /* save current position */

  rc = read (ref stream, out info);
  if (rc != (int)info'size)
  {
    (void)close_bmp (ref bmp);
    return IMG_FILE_ERROR;
  }

  pos += (info[0]      )
       + (info[1] << 8 )
       + (info[2] << 16)
       + (info[3] << 24);

  t.width = (info[4]      )
          + (info[5] << 8 )
          + (info[6] << 16)
          + (info[7] << 24);

  temp = (int)((info[8]       )
             + (info[9]  << 8 )
             + (info[10] << 16)
             + (info[11] << 24));

  if (temp < 0)  /* negative : lines are top-down */
  {
    t.reverse = 0;
    t.height = (uint)(-temp);
  }
  else           /* positive : lines are bottom-up */
  {
    t.reverse = 1;
    t.height = (uint)(temp);
  }

#if debug
  trace ("size          : %u x %u\n", t.width, t.height);
  trace ("reverse       : %u\n", t.reverse);
#endif

  if (info[12] != 1 || info[13] != 0)   /* only 1 plane supported */
  {
#if debug
  trace ("only 1 plane supported !\n");
#endif
    (void)close_bmp (ref bmp);
    return BMP_NOT_SUPPORTED;
  }

  t.bits_per_pixel = (uint2)(info[14] + (info[15] << 8));

#if debug
  trace ("bits per pixel: %u\n", t.bits_per_pixel);
#endif

  if (t.bits_per_pixel !=  1 &&
      t.bits_per_pixel !=  4 &&
      t.bits_per_pixel !=  8 &&
      t.bits_per_pixel != 16 &&
      t.bits_per_pixel != 24 &&
      t.bits_per_pixel != 32)
  {
    (void)close_bmp (ref bmp);
    return BMP_NOT_SUPPORTED;
  }

  t.compression = (info[16]      )
                + (info[17] << 8 )
                + (info[18] << 16)
                + (info[19] << 24);

#if debug
  trace ("compression   : %u\n", t.compression);
#endif

  if (t.compression != BI_RGB  &&
      t.compression != BI_RLE4 &&
      t.compression != BI_RLE8 &&
      t.compression != BI_BITFIELDS)
  {
    (void)close_bmp (ref bmp);
    return BMP_NOT_SUPPORTED;
  }


  /* load the color map */

  if (lseekr (ref stream, pos, SEEK_SET) < 0)
  {
    (void)close_bmp (ref bmp);
    return IMG_FILE_ERROR;
  }

  if (t.bits_per_pixel <= 8)
  {
    qlen = (1 << t.bits_per_pixel);

    rc = read (ref stream, out t.color_map[0:qlen]);
    if (rc != (int)(qlen * RGBQUAD'size))
    {
      (void)close_bmp (ref bmp);
      return IMG_FILE_ERROR;
    }

    /* reverse R and B elements of color_map + set the S element to 255 */

    for (i=0; i<(uint)(1 << t.bits_per_pixel); i++)
    {
      byte tmp;
      tmp                     = t.color_map[i].r;
      t.color_map[i].r        = t.color_map[i].b;
      t.color_map[i].b        = tmp;
      t.color_map[i].reserved = 255;

#if debug
  trace ("color %ld : %u %u %u\n", i, t.color_map[i].b,
                                      t.color_map[i].g,
                                      t.color_map[i].r);
#endif
    }
  }
  else if (t.bits_per_pixel == 16 || t.bits_per_pixel == 32)
  {
    if (t.compression != BI_RGB && t.compression != BI_BITFIELDS)
    {
      (void)close_bmp (ref bmp);
      return BMP_NOT_SUPPORTED;
    }

    /* load 3 LONGS with the bitmasks to be applied on RGB colors. */

    for (i=0; i<3; i++)
    {
      if (t.compression == BI_BITFIELDS)
      {
        rc = read (ref stream, out by);
        if (rc != 4)
        {
          (void)close_bmp (ref bmp);
          return IMG_FILE_ERROR;
        }
        t.mask[i] = by[0] + (by[1] << 8) + (by[2] << 16) + (by[3] << 24);
      }
      else
      {
        const uint model_16[3] = {0x001F, 0x03E0, 0x7C00};
        const uint model_32[3] = {0x0000FF, 0x00FF00, 0xFF0000};

        if (t.bits_per_pixel == 16)
          t.mask[i] = model_16[i];
        else
          t.mask[i] = model_32[i];
      }


      /* compute rshift[] and land[] values */

      {
        uint value;

        value = t.mask[i];
        if (value == 0)
        {
          (void)close_bmp (ref bmp);
          return BMP_BITMASK_ERROR;
        }

        t.rshift[i] = 0;
        while ((value & 1) == 0)
        {
          t.rshift[i]++;
          value >>= 1;
        }

        while (value > 255)   /* will not fit into 1 RGB byte */
        {
          t.rshift[i]++;
          value >>= 1;
        }

        t.land[i] = value;

        /* shift left to fill range 0 to 255 */
        t.lshift[i] = 0;
        if (value != 0)
        {
          while ((value & 0x80) == 0)
          {
            t.lshift[i]++;
            value <<= 1;
          }
        }
#if debug
  trace ("bitmask %u : %08x (rshift=%u, and=%08u, lshift=%u)\n",
         i, t.mask[i], t.rshift[i], t.land[i], t.lshift[i]);
#endif
      }
    }
  }


  /* reserve indirect access to lines */

  t.line = new PLINE [4 * t.height];

  t.line_size = ((t.width * t.bits_per_pixel + 31) & (uint'max - 31)) >> 3;

#if debug
  trace ("line_size = %u\n", t.line_size);
#endif


  if (t.reverse != 0)
  {
    t.write_y  = t.height - 1;
    t.write_dy = -1;
  }
  else
  {
    t.write_y  = 0;
    t.write_dy = +1;
  }

  t.write_count_y = t.height;
  t.read_y        = 0;


  /* allocate an entire image line in RGBS format */

  t.buffer = new byte [4 * t.width];
  t.buffer_index  = 0;
  t.pending_count = 0;


  /* reserve input buffer for BI_RLE4 and BI_RLE8 formats */

  t.in_buffer = new byte [4096];
  t.in_buffer_index = 0;
  t.in_buffer_rest  = 0;


  /* we're ready to load the bitmap now */

  if (lseekr (ref stream, bit_offset, SEEK_SET) < 0)
  {
    (void)close_bmp (ref bmp);
    return IMG_FILE_ERROR;
  }

  return 0;
}

/********************************************************************/
