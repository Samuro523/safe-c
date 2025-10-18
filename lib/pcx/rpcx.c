
/*****************************/
/* rpcx.c : PCX image reader */
/*****************************/

use ../stream;
use ../image;   // for error codes

/********************************************************************/

#define debug false

/********************************************************************/

struct PCX
{
  uint   width;
  uint   height;

  byte   bits_per_pixel;

  byte   nb_planes;
  uint   bytes_per_scan;
  uint   bytes_per_line;
  byte   palette[256*3];

  uint   y;                  /* current line being read */

  byte   in_buffer[4096];    /* 4K input buffer */
  uint   in_buffer_index;    /* next byte to read in in_buffer */
  uint   in_buffer_rest;     /* remaining bytes in in_buffer */

  byte[]^ out_buffer;        /* size : t.bytes_per_line */
  byte    overflow_byte;     /* overflow byte from previous line */
  uint    overflow_count;

  uint    pending_count;
  uint    buffer_index;
  byte[]^ buffer;            /* size : 4 * t.width */
}

/********************************************************************/

packed struct PCX_HEADER
{
  byte manufacturer;
  byte version;
  byte encoding;
  byte bits_per_pixel;
  byte xminl, xminh;
  byte yminl, yminh;
  byte xmaxl, xmaxh;
  byte ymaxl, ymaxh;
  byte hresl, hresh;
  byte vresl, vresh;
  byte palette16[48];
  byte reserved;
  byte nb_planes;
  byte bytes_per_scan_low, bytes_per_scan_high;
  byte palette_type_low, palette_type_high;
}

/**************************************************************************/

public int close_pcx (ref PCX pcx)
{
  ref PCX t = pcx;

  free t.out_buffer;
  free t.buffer;

  clear t;

  return 0;
}

/**************************************************************************/

public void get_pcx_size (PCX      pcx,
                          out uint width,
                          out uint height)
{
  width  = pcx.width;
  height = pcx.height;
}

/**************************************************************************/

int read_in_buffer (ref PCX t, out byte c, ref READ_STREAM stream)
{
  int rc;

  c = 0;

  for (;;)
  {
    if (t.in_buffer_rest >= 1)
    {
      c = t.in_buffer[t.in_buffer_index++];
      t.in_buffer_rest--;
      return 0;
    }

    /* assertion : t.in_buffer_rest == 0 */

    /* load in_buffer with additional data (at least 1 byte) */

    rc = read (ref stream, out t.in_buffer);
    if (rc < 0)
      return IMG_FILE_ERROR;

    if (rc < 1)
      return PCX_UNEXPECTED_EOF;

    t.in_buffer_index = 0;
    t.in_buffer_rest  = (uint)rc;
  }
}

/**************************************************************************/

int decompress_line (ref PCX t, ref READ_STREAM stream)
{
  uint   count, x;
  byte   c;
  uint   nb;
  uint   iout, ired, igreen, iblue, icolor;
  byte   col;
  int    rc;


#if debug
  trace ("decompress line %lu\n", t.y);
#endif

  /* decompress a line */

  count = 0;

  if (t.overflow_count > 0)      /* overflow byte from previous line */
  {
    nb = t.overflow_count;
    if (nb > t.bytes_per_line)
      nb = (uint)t.bytes_per_line;

    t.out_buffer^[0:nb] = {all => t.overflow_byte};
    count            = nb;
    t.overflow_count -= nb;
  }

  while (count < t.bytes_per_line)
  {
    rc = read_in_buffer (ref t, out c, ref stream);
    if (rc < 0)
      return rc;

    if (c >= 0xC0)     /* 2 highest bits are set */
    {
      nb = c & 0x3F;

      rc = read_in_buffer (ref t, out c, ref stream);
      if (rc < 0)
        return rc;

#if debug
  if (t.y < 10)
    trace ("ofs %u : byte %u (x %u)\n", count, c, nb);
#endif

      if (nb > t.bytes_per_line - count)
      {
#if debug
  if (t.y < 10)
    trace ("save %u bytes for next line\n", (uint)(nb - (t.bytes_per_line - count)));
#endif
        /* save overflow data */
        t.overflow_byte  = c;
        t.overflow_count = (uint)(nb - (t.bytes_per_line - count));
        nb -= t.overflow_count;
      }

      t.out_buffer^[count:nb] = {all => c};
      count += nb;
    }
    else
    {
#if debug
  if (t.y < 10)
    trace ("ofs %lu : byte %d\n", count, c);
#endif
      t.out_buffer^[count++] = c;
    }
  }


  /* transcode now the line into RGBS */


  if (t.nb_planes >= 3)       /* 3 planes : R G B must be combined */
  {
    iout   = 0;   // index into t.buffer^;

    ired   = 0;                 // index into t.out_buffer^;
    igreen = t.bytes_per_scan;
    iblue  = t.bytes_per_scan * 2;

    for (x=0; x<t.width; x++)
    {
      t.buffer^[iout:4] = {t.out_buffer^[ired++],
                           t.out_buffer^[igreen++],
                           t.out_buffer^[iblue++],
                           255};
      iout += 4;
    }
  }
  else   /* 1 plane : use color palette */
  {
    iout = 0;   // index into t.buffer^;

    if (t.bits_per_pixel == 1)
    {
      for (x=0; x<t.width; x++)
      {
        col = (byte)((t.out_buffer^[x>>3] & (1 << (7 - (x & 7)))) != 0);
        t.buffer^[iout:3] = t.palette[3*col:3];
        t.buffer^[iout+3] = 255;
        iout += 4;
      }
    }
    else          /* 3 bits per pixel */
    {
      icolor = 0;  // index into t.out_buffer^;

      for (x=0; x<t.width; x++)
      {
        t.buffer^[iout:3] = t.palette[3 * (t.out_buffer^[icolor++]) : 3];
        t.buffer^[iout+3] = 255;
        iout += 4;
      }
    }
  }

  t.buffer_index  = 0;
  t.pending_count = t.width;

  t.y++;

  return 0;
}

/**************************************************************************/

public int read_pcx (ref PCX pcx, out byte[] buffer, ref READ_STREAM stream)
{
  ref  PCX t = pcx;
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

    /* assertion : t.pending_count == 0 */

    /* read additional data into buffer */

    if (t.y == t.height)
      return IMG_END_OF_IMAGE;

    rc = decompress_line (ref t, ref stream);
    if (rc < 0)
      return rc;
  }
}

/**************************************************************************/

public int open_pcx (out PCX pcx, ref READ_STREAM stream)
{
  ref PCX t = pcx;
  int        rc;
  PCX_HEADER header;
  byte       flag;

  clear t;

  rc = read (ref stream, out header);
  if (rc != (int)header'size)
  {
    close_pcx (ref pcx);
    return IMG_FILE_ERROR;
  }

  t.bits_per_pixel = header.bits_per_pixel;

  t.width = (header.xmaxl + ((uint2)header.xmaxh << 8))
           - (header.xminl + ((uint2)header.xminh << 8))
           + 1;

  t.height = (header.ymaxl + ((uint2)header.ymaxh << 8))
            - (header.yminl + ((uint2)header.yminh << 8))
            + 1;

#if debug
  trace ("version        : %u\n", header.version);
  trace ("encoding       : %u\n", header.encoding);
  trace ("bits_per_pixel : %u\n", t.bits_per_pixel);
  trace ("size           : %u x %u\n", t.width, t.height);
#endif

  t.nb_planes = header.nb_planes;

  t.bytes_per_scan = header.bytes_per_scan_low
                    + ((uint2)header.bytes_per_scan_high << 8);

  t.bytes_per_line = t.nb_planes * t.bytes_per_scan;

#if debug
  trace ("nb planes      : %u\n",  t.nb_planes);
  trace ("bytes_per_scan : %u\n", t.bytes_per_scan);
  trace ("bytes_per_line : %u\n", t.bytes_per_line);
#endif

  if (header.encoding != 1)
  {
    close_pcx (ref pcx);
    return PCX_UNSUPPORTED_ENCODING;
  }

  if (t.nb_planes != 1 && t.nb_planes < 3)
  {
    close_pcx (ref pcx);
    return PCX_PLANES_NOT_SUPPORTED;
  }

  if (t.nb_planes == 1)           /* 1 plane : there is a palette */
  {
    if (t.bits_per_pixel != 1 && t.bits_per_pixel != 8)
    {
      close_pcx (ref pcx);
      return PCX_BITS_NOT_SUPPORTED;
    }

    if (t.bits_per_pixel <= 4 || header.version < 5)  /* 16-color palette */
    {
      t.palette[0:16*3] = header.palette16;
    }
    else
    {
      /* load 256*3 bytes palette at EOF */
      if (lseekr (ref stream, -769L, SEEK_END) < 0)
      {
        close_pcx (ref pcx);
        return IMG_FILE_ERROR;
      }

      /* check for presence of code 12 */

      rc = read (ref stream, out flag);
      if (rc != 1)
      {
        close_pcx (ref pcx);
        return IMG_FILE_ERROR;
      }

      if (flag != 12)
      {
        close_pcx (ref pcx);
        return PCX_UNSUPPORTED_PALETTE;
      }


      /* load the palette */

      rc = read (ref stream, out t.palette);
      if (rc != (int)t.palette'size)
      {
        (void)close_pcx (ref pcx);
        return IMG_FILE_ERROR;
      }
    }

#if debug
    {
      int i;
      for (i=0; i<(1 << t.bits_per_pixel); i++)
        trace ("color %d : %u %u %u\n",
               i, t.palette[i*3], t.palette[i*3+1], t.palette[i*3+2]);
    }
#endif
  }
  else                                 /* 3 planes or more */
  {
    if (t.bits_per_pixel != 8)
    {
      (void)close_pcx (ref pcx);
      return PCX_BITS_NOT_SUPPORTED;
    }
  }


  t.y = 0;

  if (lseekr (ref stream, 128L, SEEK_SET) < 0)
  {
    (void)close_pcx (ref pcx);
    return IMG_FILE_ERROR;
  }

  t.in_buffer_index = 0;
  t.in_buffer_rest  = 0;

  t.out_buffer = new byte [t.bytes_per_line];

  t.overflow_count = 0;

  t.buffer = new byte [4 * t.width];

  return 0;
}

/********************************************************************/
