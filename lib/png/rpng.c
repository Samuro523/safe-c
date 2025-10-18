
/****************************/
/* png.c : PNG image reader */
/****************************/

use ../stream, ../strings, ../crc, ../zip;
use ../image;

#define debug   0

#if debug
  use tracing;
#endif

/********************************************************************/
#begin unsafe
/********************************************************************/

const int PNG_FILE_NOT_FOUND      = IMG_FILE_NOT_FOUND;       /* -1 : cannot open file */
const int NOT_PNG_FILE            = IMG_NOT_RECOGNIZED;       /* -2 : illegal PNG header */
const int PNG_IO_ERROR            = IMG_FILE_ERROR;           /* -3 : file read error or corrupted file */
const int PNG_OUT_OF_SPACE        = IMG_OUT_OF_SPACE;         /* -4 : out of memory */
const int PNG_ILLEGAL_BUFFER_SIZE = IMG_ILLEGAL_BUFFER_SIZE;  /* -5 : size must be M4 */
const int PNG_END_OF_IMAGE        = IMG_END_OF_IMAGE;         /* -6 : no more pixel data to read */
const int PNG_NOT_OPEN            = IMG_NOT_OPEN;             /* -7 : NULL 'png' parameter */

/********************************************************************/

packed struct RGB_TRIPLET
{
  byte[3] col;
}

/********************************************************************/

struct PNG
{
  READ_STREAM*  pstream;

  /* png header data */
  uint  width;        /* 0 = illegal */
  uint  height;       /* 0 = illegal */
  byte  bit_depth;    /* (1, 2, 4, 8 or 16) (1, 2, 4 only allowed for colour_type 0 & 3) */
  byte  colour_type;  /* (0, 2, 3, 4 or 6) */
  byte  interlace;    /* (0=no interlace, 1=Adam7 interlace) */

#if 0
  PNG image type     Colour type 
  ==============     ===========
  Greyscale               0       grey                  Transparency info: tRNS with single pixel value
  Truecolour              2       red, green, blue      Transparency info: tRNS with single pixel value
  Indexed-colour          3       palette_index         Transparency info: tRNS with alpha table 
  Greyscale with alpha    4       grey, alpha
  Truecolour with alpha   6       red, green, blue, alpha

=============  ========  ==========
types           depth     tested ?
=============  ========  ==========
  0  (grey)       1
  0  (grey)       2
  0  (grey)       4         OK
  0  (grey)       8         OK
  0  (grey)      16
=============  ========  ==========
  2  (rgb)        8         OK
  2  (rgb)       16         OK
=============  ========  ==========
  3  (index)      1
  3  (index)      2
  3  (index)      4         OK
  3  (index)      8         OK
=============  ========  ==========
  4  (grey+a)     8
  4  (grey+a)    16
=============  ========  ==========
  6  (rgb+a)      8         OK
  6  (rgb+a)     16         OK
=============  ========  ==========
#endif

  bool     init_done;    /* everydone was done before reading pixel data */

  /* in chunk */
  byte[]^  chunk;       /* current chunk (allocated on heap) */
  uint     chunk_index;  /* index of next chunk data byte to read */
  uint     chunk_size;   /* index after final chunk data byte to read */
  bool     eof;          /* true=IEND chunk reached */

  /* palette, transparency and background */
  int             nb_palette_colors;   /* 0 to 255 */
  RGB_TRIPLET[]^  palette;             /* palette table (for colour_type 3) */
  byte[]^         alpha_palette;       /* null if not provided (all entries have value 255) */

  bool            transparent_color_filled;
  ushort[3]       transparent_color;           /* RGB (each component has 16 bits) (all 3 same for grey) */

  byte[3]         background_color;            /* RGB values (default = black) */


  /* filter */
  uint            filter_ofs;          /* byte offset to reach previous pixel's byte (minimum 1) */

  /* output */
  byte[]^     out_buffer;              /* OUT_BUFFER_SIZE bytes allocated */
  uint        out_buffer_length;

  byte[]^     previous_scanline;

  byte[]^     pout;           /* full output image (4 * width * height) in RGBS */
  uint        image_size;     /* (= 4*width*height) */
  uint        send_index;     /* 0 .. 4*width*height */
  uint        adler_crc;
  int         pass, last_pass;
  uint        scanline_nb_bytes;
  uint        y;
}

/********************************************************************/

package UNPACK_PNG_DATA = new UNPACK (USER_INFO => PNG);

/********************************************************************/

const byte samples_per_pixel[7] = { /* 0: grey        */ 1,
                                    /* 1: unused      */ 1,
                                    /* 2: color       */ 3,
                                    /* 3: index       */ 1,
                                    /* 4: grey+alpha  */ 2,
                                    /* 5: unused      */ 1,
                                    /* 6: color+alpha */ 4 };

/********************************************************************/

packed struct CHUNK0
{
  uint length;
  byte type[4];
}

packed struct CHUNK9
{
  uint crc;
}

/********************************************************************/

int read_block (ref READ_STREAM stream, out byte[] buffer)
{
  if (read (ref stream, out buffer) != buffer'length)
  {
#if debug
    trace ("error: read_block (%d bytes) failed\n", buffer'length);
#endif
    return PNG_IO_ERROR;
  }
  return 0;
}

/********************************************************************/

int png_signature (ref PNG info)
{
  const byte png_sign[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  byte sign[8];
  int  rc;

  rc = read_block (ref *info.pstream, out sign);
  if (rc < 0)
    return rc;

  if (memcmp (sign, png_sign) != 0)
  {
#if debug
    trace ("error: png signature mismatch\n");
#endif
    return NOT_PNG_FILE;
  }

#if debug
  trace ("info: png signature ok\n");
#endif

  return 0;
}

/********************************************************************/

void swap_short (ref ushort pn)
{
  byte c[2];
  c = pn'byte;
  c = {c[1], c[0]};
  pn'byte = c;
}

/********************************************************************/

void swap_long (ref uint pn)
{
  byte c[4];
  c = pn'byte;
  c = {c[3], c[2], c[1], c[0]};
  pn'byte = c;
}

/********************************************************************/

/* load next chunk in info^.chunk */

int load_chunk (ref PNG info)
{
  int     rc;
  CHUNK0  chunk0;
  CHUNK9  chunk9;
  uint    crc;

  rc = read_block (ref *info.pstream, out chunk0);
  if (rc < 0)
    return rc;

  crc = 0;
  update_crc (ref crc, chunk0.type);

  swap_long (ref chunk0.length);

#if debug
  trace ("info: loading chunk of %d data bytes type %.4s\n", chunk0.length, chunk0.type);
#endif

  if (chunk0.length > 256*1024*1024)   /* more than 256 MB */
    return PNG_LONG_CHUNK;

  free (info.chunk);
  info.chunk = new byte[8 + chunk0.length];   /* crc not included */

  info.chunk^[0:8] = chunk0'byte;

  rc = read_block (ref *info.pstream, out info.chunk^[8:chunk0.length]);
  if (rc < 0)
    return rc;

#if debug
//  trace_block (info.chunk^[8:chunk0.length]);
#endif

  info.chunk_index = 8;
  info.chunk_size = 8 + chunk0.length;

  update_crc (ref crc, info.chunk^[8:chunk0.length]);

  rc = read_block (ref *info.pstream, out chunk9);
  if (rc < 0)
    return rc;

  swap_long (ref chunk9.crc);

  if (crc != chunk9.crc)
  {
#if debug
  trace ("info: chunk has bad crc\n");
#endif
    return PNG_BAD_CHUNK_CRC;
  }

  return 0;
}

/********************************************************************/

uint min (uint a, uint b)
{
  if (a < b)
    return a;
  return b;
}

/********************************************************************/

int abs (int n)
{
  if (n < 0)
    return -n;
  return n;
}

/********************************************************************/

int read_data_chunk_bytes (ref PNG    info,
                           out byte[] buffer)
{
  uint len, bytes_read, ofs;
  int  rc;

  clear buffer;

#if debug
  trace ("info: unpack reads %d from idat chunk\n", buffer'size);
#endif

  bytes_read = 0;
  ofs = 0;

  while (bytes_read < buffer'size)
  {
    if (info.eof)
      break;

    len = min (buffer'size - bytes_read, info.chunk_size - info.chunk_index);
    buffer[ofs:len] = info.chunk^[info.chunk_index:len];

    ofs += len;
    info.chunk_index += len;
    bytes_read += len;

    if (bytes_read < buffer'size)    /* we must read further chunks */
    {
      for (;;)
      {
        CHUNK0 c0;

        rc = load_chunk (ref info);
        if (rc < 0)
          return rc;

        c0'byte = info.chunk^[0:8];

        if (memcmp (c0.type, "IDAT") == 0)   /* data */
          break;

        if (memcmp (c0.type, "IEND") == 0)   /* end */
        {
          info.eof = true;
          break;
        }
      }
    }
  }

#if debug
  if (bytes_read != buffer'size)
    trace ("info: no more bytes to read from idat chunk\n");
#endif

  return (int)bytes_read;
}

/********************************************************************/

package IHDR

  packed struct IHDR_CHUNK
  {
    uint length;
    byte type[4];
    uint width;        /* 0 = illegal */
    uint height;       /* 0 = illegal */
    byte bit_depth;    /* (1, 2, 4, 8 or 16) (1, 2, 4 only allowed for colour_type 0 & 3) */
    byte colour_type;  /* (0, 2, 3, 4 or 6) */
    byte compression;  /* must be 0 */
    byte filter;       /* must be 0 */
    byte interlace;    /* (0=no interlace, 1=Adam7 interlace) */
  }

  const uint MAX_WIDTH  = 8191;
  const uint MAX_HEIGHT = 8191;

  const uint OUT_BUFFER_SIZE = (1 + 4 * MAX_WIDTH);    /* max size of a scanline */

end IHDR;

/********************************************************************/

int png_header (ref PNG info)
{
  int rc;

  rc = load_chunk (ref info);
  if (rc < 0)
    return rc;

  {
    CHUNK0 c0;

    c0'byte = info.chunk^[0:8];

    if (c0.length != 13 || memcmp (c0.type, "IHDR") != 0)
      return PNG_BAD_HEADER;
  }

  {
    IHDR_CHUNK pihdr;

    pihdr'byte = info.chunk^[0:pihdr'size];

    swap_long (ref pihdr.width);
    swap_long (ref pihdr.height);

#if debug
    {
      const string colour_type_str[7] = {"grey", "?", "r+g+b", "palette", "grey+alpha", "?", "r+g+b+alpha"};

      trace ("width       %u\n", pihdr.width);
      trace ("height      %u\n", pihdr.height);
      trace ("bit depth   %u\n", pihdr.bit_depth);

      trace ("colour_type %u (%s)\n", pihdr.colour_type, pihdr.colour_type >= 0 && pihdr.colour_type < 7 ? colour_type_str[pihdr.colour_type] : "");
      trace ("interlace   %u\n", pihdr.interlace);
    }
#endif

    if (pihdr.width == 0        || pihdr.height == 0 ||        /* not empty dimensions */
        pihdr.width > MAX_WIDTH || pihdr.height > MAX_HEIGHT)  /* max 256 MB image */
      return PNG_BAD_DIMENSIONS;

    if (pihdr.bit_depth != 1 && pihdr.bit_depth != 2 && pihdr.bit_depth != 4 &&
        pihdr.bit_depth != 8 && pihdr.bit_depth != 16)
      return PNG_BAD_HEADER;

    if (pihdr.colour_type != 0 && pihdr.colour_type != 2 && pihdr.colour_type != 3 &&
        pihdr.colour_type != 4 && pihdr.colour_type != 6)
      return PNG_BAD_HEADER;

    /* bit_depths 1, 2, 4 only allowed for colour_type 0 & 3 */
    if (pihdr.colour_type != 0 && pihdr.colour_type != 3)
    {
      if (pihdr.bit_depth < 8)
        return PNG_BAD_HEADER;
    }

    if (pihdr.colour_type == 3)
    {
      if (pihdr.bit_depth > 8)
        return PNG_BAD_HEADER;
    }

    if (pihdr.compression != 0 || pihdr.filter != 0)
      return PNG_BAD_HEADER;

    if (pihdr.interlace > 1)
      return PNG_BAD_HEADER;

    info.width       = pihdr.width;
    info.height      = pihdr.height;
    info.bit_depth   = pihdr.bit_depth;
    info.colour_type = pihdr.colour_type;
    info.interlace   = pihdr.interlace;

    if (info.interlace != 0)      /* adam7 */
    {
      info.pass       = 0;
      info.last_pass  = 6;
    }
    else   /* no interlace */
    {
      info.pass       = 7;
      info.last_pass  = 7;
    }

    info.image_size = 4 * info.width * info.height;

    info.filter_ofs = (uint)((samples_per_pixel[info.colour_type] * info.bit_depth) >> 3);
    if (info.filter_ofs == 0)
      info.filter_ofs = 1;

    clear info.background_color;
  }

  return 0;
}

/********************************************************************/

int secondary_chunks (ref PNG info)
{
  int    rc;
  uint   length;
  CHUNK0 ph;

  for (;;)
  {
    rc = load_chunk (ref info);
    if (rc < 0)
      return rc;

    ph'byte = info.chunk^[0:8];

    length = ph.length;

    if (memcmp (ph.type, "PLTE") == 0)   /* palette */
    {
      if (info.palette != null)
        return PNG_BAD_CHUNK_ORDER;

      if (length < 3 || (length % 3) != 0 || length/3 > (uint)(1 << info.bit_depth))
        return PNG_BAD_PALETTE_LENGTH;

      info.nb_palette_colors = (int)(length / 3);

      info.palette = new RGB_TRIPLET[info.nb_palette_colors];

      info.palette^'byte = info.chunk^[8:length];

#if debug
      trace ("palette (%d bytes):\n", length);
//      trace_block (info.palette^'byte);
#endif

    }
    else if (memcmp (ph.type, "tRNS") == 0)   /* transparent */
    {
      if (info.colour_type == 0)   /* grey */
      {
        if (length != 2)
          return PNG_BAD_TRNS_LENGTH;

        info.transparent_color[0]'byte = info.chunk^[8:2];

        swap_short (ref info.transparent_color[0]);

        info.transparent_color[1] = info.transparent_color[0];
        info.transparent_color[2] = info.transparent_color[0];

        info.transparent_color_filled = true;

#if debug
      trace ("transparent_color : %u\n", info.transparent_color[0]);
#endif
      }
      else if (info.colour_type == 2)   /* color */
      {
        if (length != 6)
          return PNG_BAD_TRNS_LENGTH;

        info.transparent_color'byte = info.chunk^[8:6];

        swap_short (ref info.transparent_color[0]);
        swap_short (ref info.transparent_color[1]);
        swap_short (ref info.transparent_color[2]);

        info.transparent_color_filled = true;

#if debug
      trace ("transparent_color : (%u,%u,%u)\n", info.transparent_color[0], info.transparent_color[1], info.transparent_color[2]);
#endif
      }
      else if (info.colour_type == 3)   /* index */
      {
        if (info.palette == null)
          return PNG_BAD_CHUNK_ORDER;

        if (length > (uint)info.nb_palette_colors)
          return PNG_BAD_TRNS_LENGTH;

        info.alpha_palette = new byte [info.nb_palette_colors];

        info.alpha_palette^ = {all => 0xFF};
        info.alpha_palette^[0:length] = info.chunk^[8:length];

#if debug
      trace ("alpha-palette (%d bytes):\n", length);
//      trace_block (info.alpha_palette^);
#endif
      }
    }
    else if (memcmp (ph.type, "bKGD") == 0)   /* background */
    {
      if (info.colour_type == 0 || info.colour_type == 4)   /* grey */
      {
        ushort col;

        if (length != 2)
          return PNG_BAD_BKGD_LENGTH;

        col'byte = info.chunk^[8:2];

        swap_short (ref col);

        info.background_color[0] = (byte)col;
        info.background_color[1] = (byte)col;
        info.background_color[2] = (byte)col;

#if debug
      trace ("background_color : %u\n", col);
#endif
      }
      else if (info.colour_type == 2 || info.colour_type == 6)   /* color */
      {
        ushort col[3];

        if (length != 6)
          return PNG_BAD_BKGD_LENGTH;

        col'byte = info.chunk^[8:6];

        swap_short (ref col[0]);
        swap_short (ref col[1]);
        swap_short (ref col[2]);

        info.background_color[0] = (byte)col[0];
        info.background_color[1] = (byte)col[1];
        info.background_color[2] = (byte)col[2];

#if debug
      trace ("background_color : (%u,%u,%u)\n", col[0], col[1], col[2]);
#endif
      }
      else if (info.colour_type == 3)   /* index */
      {
        byte index;

        if (length != 1)
          return PNG_BAD_BKGD_LENGTH;

        index'byte = info.chunk^[8:1];

        if (info.palette == null || index >= (uint)info.nb_palette_colors)
          return PNG_BAD_BKGD_VALUE;

        info.background_color[0] = (byte)info.palette^[index].col[0];
        info.background_color[1] = (byte)info.palette^[index].col[1];
        info.background_color[2] = (byte)info.palette^[index].col[2];

#if debug
        trace ("background_color : (%u,%u,%u)\n", info.background_color[0], info.background_color[1], info.background_color[2]);
#endif
      }
    }
    else if (memcmp (ph.type, "IDAT") == 0)   /* data */
    {
      if (info.colour_type == 3 && info.palette == null)
        return PNG_MISSING_PALETTE;
      return 0;   /* ready to read pixel data */
    }
    else if (memcmp (ph.type, "IEND") == 0)   /* end */
    {
      return PNG_MISSING_DATA;
    }
  }
}

/********************************************************************/

int zlib_header (ref PNG info)
{
  int  rc;
  byte head[2];

  rc = read_data_chunk_bytes (ref info, out head);
  if (rc != 2)
    return PNG_MISSING_DATA;

#if debug
  trace ("zlib bytes : (cm=%u, cinfo=%u, fcheck=%u, fdict=%u, flevel=%u)\n",
         head[0] & 15, head[0] >> 4, head[1] & 31, (head[1]>>5) & 1, head[1]>>6);
#endif

  if ((head[0] & 15) != 8)  /* not deflate */
    return PNG_NOT_DEFLATE;

  if (((((head[0]) << 8) + head[1]) % 31) != 0)
  {
#if debug
    trace ("error: fcheck has bad value\n");
#endif
    return PNG_BAD_FCHECK;
  }

  return 0;
}

/********************************************************************/

public int close_png (ref PNG png)
{
  /* free all */
  free (png.chunk);
  free (png.palette);
  free (png.alpha_palette);
  free (png.pout);
  free (png.out_buffer);
  free (png.previous_scanline);

  clear png;

  return 0;
}

/**************************************************************************/

public void get_png_size (PNG      png,
                          out uint width,
                          out uint height)
{
  width  = png.width;
  height = png.height;
}

/**************************************************************************/

package PASS

  /* pass 0 to 6, or 7 to 7 */

  const uint starting_y[8]    = { 0, 0, 4, 0, 2, 0, 1, 0 };
  const uint starting_x[8]    = { 0, 4, 0, 2, 0, 1, 0, 0 };
  const uint y_increment[8]   = { 8, 8, 8, 4, 4, 2, 2, 1 };
  const uint x_increment[8]   = { 8, 8, 4, 4, 2, 2, 1, 1 };
  const uint x_inc_rshifts[8] = { 3, 3, 2, 2, 1, 1, 0, 0 };

end PASS;

/**************************************************************************/

void compute_scanline_size (ref PNG t)
{
  uint nb_pixels;

  for (;;)
  {
    nb_pixels = (t.width - 1 - starting_x[t.pass] + x_increment[t.pass]) >> x_inc_rshifts[t.pass];
    if (nb_pixels > 0)
      break;
    t.pass++;   /* if no pixels, go directly to next pass */
  }

  t.scanline_nb_bytes = 1 + (((nb_pixels * samples_per_pixel[t.colour_type] * t.bit_depth) + 7) >> 3);
}

/**************************************************************************/

/* assert:  t.pass <= t.last_pass */

void next_scanline (ref PNG t)
{
  t.y += y_increment[t.pass];
  if (t.y >= t.height)
  {
    t.pass++;
    if (t.pass <= t.last_pass)
    {
      t.y = starting_y[t.pass];
      compute_scanline_size (ref t);
      clear t.previous_scanline^;   /* clear scanline */
    }
  }
}

/**************************************************************************/

package PALETTE
  const int pal1[2] = {0, 255};
  const int pal2[4] = {0, 85, 170, 255};
  const int pal4[16] = {17*0, 17*1, 17*2, 17*3, 17*4, 17*5, 17*6, 17*7,
                        17*8, 17*9, 17*10, 17*11, 17*12, 17*13, 17*14, 17*15};
end PALETTE;

/**************************************************************************/

void store_scanline (byte[] scanline, ref PNG t)
{
  uint   i, index, x, begin_x, end_x, inc_x;
  ushort c, col[4];

  ref byte[] ptr = t.pout^[4 * t.width * t.y : 4 * t.width];

  i = 0;

  begin_x = starting_x[t.pass];
  end_x   = t.width;
  inc_x   = x_increment[t.pass];

  switch (t.colour_type)
  {
    case 0:   /* grey */         /* Transparency info: tRNS with single pixel value */
      for (x=begin_x; x<end_x; x+=inc_x)
      {
        if (t.bit_depth == 1)
          c = (ushort)((scanline[i>>3] >> (7-(i & 7))) & 1);
        else if (t.bit_depth == 2)
          c = (ushort)((scanline[i>>2] >> (6-2*(i & 3))) & 3);
        else if (t.bit_depth == 4)
          c = (ushort)((scanline[i>>1] >> (4-4*(i & 1))) & 15);
        else if (t.bit_depth == 8)
          c = (ushort)(scanline[i]);
        else   /* bit_depth == 16 */
        {
          c = (ushort)(((scanline[i]) << 8) + scanline[i+1]);
          i++;
        }

        i++;

        col = {c, c, c, 255};   /* opaque */

        if (t.transparent_color_filled &&
            col[0] == t.transparent_color[0])
        {
          col[3] = 0;   /* transparent */
        }

        if (t.bit_depth == 1)   /* 0 .. 1 */
        {
          col[0] = (ushort)pal1[col[0]];
          col[1] = col[0];
          col[2] = col[0];
        }
        else if (t.bit_depth == 2)   /* 0 .. 3 */
        {
          col[0] = (ushort)pal2[col[0]];
          col[1] = col[0];
          col[2] = col[0];
        }
        else if (t.bit_depth == 4)   /* 0 .. 15 */
        {
          col[0] = (ushort)pal4[col[0]];
          col[1] = col[0];
          col[2] = col[0];
        }
        else if (t.bit_depth == 16)
        {
          col[0] >>= 8;
          col[1] >>= 8;
          col[2] >>= 8;
        }

        /* keep original color */
        if (col[3] == 0)
        {
          col[0] = t.background_color[0];
          col[1] = t.background_color[1];
          col[2] = t.background_color[2];
        }

        ptr[4*x+0] = (byte)col[0];
        ptr[4*x+1] = (byte)col[1];
        ptr[4*x+2] = (byte)col[2];
        ptr[4*x+3] = (byte)col[3];
      }
      break;

    case 2:   /* Truecolour (r,g,b) */                /* Transparency info: tRNS with single pixel value */
      for (x=begin_x; x<end_x; x+=inc_x)
      {
        if (t.bit_depth == 8)    /* (8 or 16) */
        {
          col = {scanline[i], scanline[i+1], scanline[i+2], 255};   /* opaque */
          i += 3;
        }
        else
        {
          col = {(ushort)(((scanline[i]) << 8) + scanline[i+1]),
                 (ushort)(((scanline[i+2]) << 8) + scanline[i+3]),
                 (ushort)(((scanline[i+4]) << 8) + scanline[i+5]),
                 0xFFFF};
          i += 6;
        }

        if (t.transparent_color_filled &&
            col[0] == t.transparent_color[0] &&
            col[1] == t.transparent_color[1] &&
            col[2] == t.transparent_color[2])
        {
          col[3] = 0;   /* transparent */
        }

        if (t.bit_depth == 16)
        {
          col[0] >>= 8;
          col[1] >>= 8;
          col[2] >>= 8;
          col[3] >>= 8;
        }

        /* keep original color */
        if (col[3] == 0)
        {
          col[0] = t.background_color[0];
          col[1] = t.background_color[1];
          col[2] = t.background_color[2];
        }

        ptr[4*x+0] = (byte)col[0];
        ptr[4*x+1] = (byte)col[1];
        ptr[4*x+2] = (byte)col[2];
        ptr[4*x+3] = (byte)col[3];
      }
      break;

    case 3:   /* Indexed-colour (palette_index) */    /* Transparency info: tRNS with alpha table */
      for (x=begin_x; x<end_x; x+=inc_x)
      {
        if (t.bit_depth == 1)
          index = (scanline[i>>3] >> (7-(i & 7))) & 1;
        else if (t.bit_depth == 2)
          index = (scanline[i>>2] >> (6-2*(i & 3))) & 3;
        else if (t.bit_depth == 4)
          index = (scanline[i>>1] >> (4-4*(i & 1))) & 15;
        else
          index = scanline[i];

        i++;

        if (index >= (uint)t.nb_palette_colors)
          index = 0;

        col = {t.palette^[index].col[0],
               t.palette^[index].col[1],
               t.palette^[index].col[2],
               (ushort)(t.alpha_palette != null ? t.alpha_palette^[index] : 255)};

        /* keep original color */
        if (col[3] == 0)
        {
          col[0] = t.background_color[0];
          col[1] = t.background_color[1];
          col[2] = t.background_color[2];
        }

        ptr[4*x+0] = (byte)col[0];
        ptr[4*x+1] = (byte)col[1];
        ptr[4*x+2] = (byte)col[2];
        ptr[4*x+3] = (byte)col[3];
      }
      break;

    case 4:   /* Greyscale with alpha (grey, alpha) */
      for (x=begin_x; x<end_x; x+=inc_x)
      {
        if (t.bit_depth == 8)
        {
          col = {scanline[i], scanline[i], scanline[i], scanline[i+1]};
          i += 2;
        }
        else   /* bit_depth == 16 */
        {
          col = {(ushort)(((scanline[i]) << 8) + scanline[i+1]),
                 (ushort)(((scanline[i]) << 8) + scanline[i+1]),
                 (ushort)(((scanline[i]) << 8) + scanline[i+1]),
                 (ushort)(((scanline[i+2]) << 8) + scanline[i+3])};
          i += 4;
        }

        if (t.bit_depth == 16)
        {
          col[0] >>= 8;
          col[1] >>= 8;
          col[2] >>= 8;
          col[3] >>= 8;
        }

        /* keep original color */
        if (col[3] == 0)
        {
          col[0] = t.background_color[0];
          col[1] = t.background_color[1];
          col[2] = t.background_color[2];
        }

        ptr[4*x+0] = (byte)col[0];
        ptr[4*x+1] = (byte)col[1];
        ptr[4*x+2] = (byte)col[2];
        ptr[4*x+3] = (byte)col[3];
      }
      break;

    case 6:   /* Truecolour with alpha (r,g,b,alpha) */
      for (x=begin_x; x<end_x; x+=inc_x)
      {
        if (t.bit_depth == 8)    /* (8 or 16) */
        {
          col = {scanline[i], scanline[i+1], scanline[i+2], scanline[i+3]};
          i += 4;
        }
        else
        {
          col = {(ushort)(((scanline[i]) << 8) + scanline[i+1]),
                 (ushort)(((scanline[i+2]) << 8) + scanline[i+3]),
                 (ushort)(((scanline[i+4]) << 8) + scanline[i+5]),
                 (ushort)(((scanline[i+6]) << 8) + scanline[i+7])};
          i += 8;
        }

        if (t.bit_depth == 16)
        {
          col[0] >>= 8;
          col[1] >>= 8;
          col[2] >>= 8;
          col[3] >>= 8;
        }

        /* keep original color */
        if (col[3] == 0)
        {
          col[0] = t.background_color[0];
          col[1] = t.background_color[1];
          col[2] = t.background_color[2];
        }

        ptr[4*x+0] = (byte)col[0];
        ptr[4*x+1] = (byte)col[1];
        ptr[4*x+2] = (byte)col[2];
        ptr[4*x+3] = (byte)col[3];
      }
      break;

    default:
      break;
  }
}

/**************************************************************************/

void filter_scanline (byte       filter,
                      byte[]     previous_scanline,
                      ref byte[] scanline,
                      uint       nb_bytes,
                      uint       filter_ofs)  /* byte offset to reach next pixel, minimum 1 */
{
  uint i;

  switch (filter)
  {
    case 0:    /* no filter */
      break;

    case 1:    /* (previous)  :  Recon(x) = Filt(x) + Recon(a) */
      for (i=filter_ofs; i<nb_bytes; i++)
        scanline[i] += scanline[i-filter_ofs];
      break;

    case 2:    /* (up)  :   Recon(x) = Filt(x) + Recon(b) */
      for (i=0; i<nb_bytes; i++)
        scanline[i] += previous_scanline[i];
      break;

    case 3:    /* Recon(x) = Filt(x) + floor((Recon(a) + Recon(b)) / 2) */
      for (i=0; i<filter_ofs; i++)
        scanline[i] += (byte)(previous_scanline[i] >> 1);
      for (i=filter_ofs; i<nb_bytes; i++)
        scanline[i] += (byte)((scanline[i-filter_ofs] + previous_scanline[i]) >> 1);
      break;
 
    case 4:    /* Recon(x) = Filt(x) + PaethPredictor(Recon(a), Recon(b), Recon(c)) */
      for (i=0; i<filter_ofs; i++)
      {
        int p, pa, pb, pc;

        p = (int)previous_scanline[i];

        pa = abs(p);
        pb = abs(p - (int)previous_scanline[i]);
        pc = abs(p);

        if ((pa <= pb) && (pa <= pc))
          p = 0;
        else if (pb <= pc)
          p = previous_scanline[i];
        else
          p = 0;

        scanline[i] += (byte)p;
      }

      for (i=filter_ofs; i<nb_bytes; i++)
      {
        int p, pa, pb, pc;

        p = (int)scanline[i-filter_ofs] + (int)previous_scanline[i] - (int)previous_scanline[i-filter_ofs];

        pa = abs(p - (int)scanline[i-filter_ofs]);
        pb = abs(p - (int)previous_scanline[i]);
        pc = abs(p - (int)previous_scanline[i-filter_ofs]);

        if ((pa <= pb) && (pa <= pc))
          p = scanline[i-filter_ofs];
        else if (pb <= pc)
          p = previous_scanline[i];
        else
          p = previous_scanline[i-filter_ofs];

        scanline[i] += (byte)p;
      }
      break;

   default:
#if debug
      trace ("illegal filter %d\n", filter);
#endif
      break;
  }
}

/**************************************************************************/

/* outputs scanline bytes in 32K chunks */

int write_scanline_bytes (ref PNG t,
                          byte[]  buffer)
{
  uint bytes_written, len, ofs;

  update_adler (ref t.adler_crc, buffer);

#if debug
//  trace ("write raw scanlines:\n");
//  trace_block (buffer);
#endif

  bytes_written = 0;

  while (bytes_written < buffer'size)
  {
    if (t.out_buffer_length < OUT_BUFFER_SIZE)   /* fill out_buffer with next bytes from decompression */
    {
      len = min (OUT_BUFFER_SIZE - t.out_buffer_length, buffer'size - bytes_written);

      t.out_buffer^[t.out_buffer_length:len] = buffer[bytes_written:len];
      t.out_buffer_length += len;
      bytes_written += len;
    }

    /* write as much scanlines as possible from out_buffer to pout */

    ofs = 0;
    while (ofs + t.scanline_nb_bytes <= t.out_buffer_length)
    {
      if (t.pass > t.last_pass)   /* too much data */
        return -1;

#if debug
//  trace ("pass %d line %d : %d bytes  filter %d\n", t.pass, t.y, t.scanline_nb_bytes, t.out_buffer[ofs]);
//  trace ("before filter:\n");
//  trace_block (t.out_buffer^[ofs+1:t.scanline_nb_bytes-1]);
#endif

      filter_scanline (filter            => t.out_buffer^[ofs],
                       previous_scanline => t.previous_scanline^[1:t.scanline_nb_bytes-1],
                       ref scanline      => t.out_buffer^[ofs+1:t.scanline_nb_bytes-1],
                       nb_bytes          => t.scanline_nb_bytes-1,
                       filter_ofs        => t.filter_ofs);

      t.previous_scanline^[0:t.scanline_nb_bytes] = t.out_buffer^[ofs:t.scanline_nb_bytes];

#if debug
//  trace ("after filter:\n");
//  trace_block (t.out_buffer^[ofs+1:t.scanline_nb_bytes-1]);
#endif

      store_scanline (t.out_buffer^[ofs+1:t.scanline_nb_bytes-1], ref t);

      ofs += t.scanline_nb_bytes;
      next_scanline (ref t);
    }

    t.out_buffer^[0:t.out_buffer_length - ofs] = t.out_buffer^[ofs:t.out_buffer_length - ofs];
    t.out_buffer_length -= ofs;
  }

  return (int)bytes_written;
}

/**************************************************************************/

public void get_png_attributes (PNG                  png,
                                out IMAGE_ATTRIBUTES attr)
{
  clear attr;

  if (((png.colour_type == 3) && (png.alpha_palette != null))
      || (png.colour_type == 4)
      || (png.colour_type == 6))
  {
    attr.uses_transparency = 2;
  }
  else if (png.transparent_color_filled)
  {
    attr.uses_transparency = 1;
  }
}

/**************************************************************************/

public int read_png (ref PNG png, out byte[] buffer)
{
  clear buffer;

  if ((buffer'size & 3) != 0)
    return PNG_ILLEGAL_BUFFER_SIZE;    /* size is not multiple of 4 */


  if (!png.init_done)   // image not yet decompressed into png.pout^
  {
    free png.pout;
    png.pout = new byte [png.image_size];

    free png.out_buffer;
    png.out_buffer = new byte [OUT_BUFFER_SIZE];

    free png.previous_scanline;
    png.previous_scanline = new byte [1 + (((png.width * samples_per_pixel[png.colour_type] * png.bit_depth) + 7) >> 3)];

    compute_scanline_size (ref png);

    {
      READ_AHEAD read_ahead;
      int        rc;
      uint       crc;

      png.adler_crc = 1;

      rc = unpack (ref png, read_data_chunk_bytes, write_scanline_bytes, out read_ahead);
      if (rc < 0)
      {
#if debug
        trace ("error: unpack() returned error %d\n", rc);
#endif
        if (rc == PK_READ_ERROR)
          return PNG_IO_ERROR;
        else if (rc == PK_WRITE_ERROR)
          return PNG_TOO_MUCH_DATA;
        else
          return PNG_CORRUPT;   /* unpack error */
      }

      if (read_ahead.size < 8)  // make sure we tried to read past the 4-byte trailer
      {
        rc = read_data_chunk_bytes (ref png, out read_ahead.buffer[read_ahead.size:8-read_ahead.size]);
        if (rc < 0)
          return PNG_IO_ERROR;
        read_ahead.size += (uint)rc;
      }

      if (read_ahead.size < 4)
        return PNG_MISSING_DATA;
      else if (read_ahead.size > 4)
        return PNG_TOO_MUCH_DATA4;
      
      crc'byte = read_ahead.buffer[0:4]'byte;
      swap_long (ref crc);
      if (crc != png.adler_crc)
      {
#if debug
        trace ("error: adler crc mismatch\n");
#endif
        return PNG_BAD_ADLER_CRC;
      }
    }

    if (png.out_buffer_length > 0)
    {
#if debug
      trace ("error: extra pixel bytes\n");
#endif
      return PNG_TOO_MUCH_DATA2;
    }

    if (png.pass <= png.last_pass)
    {
#if debug
      trace ("error: missing pixel bytes\n");
#endif
      return PNG_TOO_MUCH_DATA3;
    }

    png.init_done = true;
  }

  if (png.image_size - png.send_index < buffer'size)
    return PNG_END_OF_IMAGE;

  buffer = png.pout^[png.send_index:buffer'size];

  png.send_index += buffer'size;

  return 0;
}

/**************************************************************************/

public int open_png (out PNG png, ref READ_STREAM stream)
{
  int rc;

  clear png;

  png.pstream = &stream;
  
  /* read the header */
  rc = png_signature (ref png);
  if (rc < 0)
  {
    close_png (ref png);
    return rc;
  }

  rc = png_header (ref png);
  if (rc < 0)
  {
    close_png (ref png);
    return rc;
  }

  rc = secondary_chunks (ref png);
  if (rc < 0)
  {
    close_png (ref png);
    return rc;
  }

  rc = zlib_header (ref png);
  if (rc < 0)
  {
    close_png (ref png);
    return rc;
  }

  return 0;
}

/********************************************************************/
#end unsafe
/********************************************************************/
