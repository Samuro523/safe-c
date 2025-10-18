
/* wpng.c : PNG image writer */

use ../stream;
use ../crc, ../strings, ../zip;

/*************************************************************************/
#begin unsafe
/*************************************************************************/

#define debug   0

const uint MAX_WIDTH  = 8191;
const uint MAX_HEIGHT = 8191;

/********************************************************************/

struct PNG
{
  WRITE_STREAM* pstream;
  
  short    status;         /* usually -1, set to 0 when the file is complete */

  uint     width;
  uint     height;

  byte     use_transparency;    /* 0, 1 or 2 */
  byte     transparent_RGB[3];  /* background color for fully transparent pixels */

  byte     colour_type;   /* (2, 3 or 6) */

#if 0
  PNG image type     Colour type 
  ==============     ===========
  Truecolour              2       red, green, blue
  Indexed-colour          3       palette_index         Transparency info: tRNS with alpha table 
  Truecolour with alpha   6       red, green, blue, alpha
#endif

  uint     image_size;        /* 4 * width * height */
  uint     image_offset;      /* bytes read-in from user so far */
  byte[]^  image;             /* image_size bytes allocated on heap or taken from caller's buffer */

  uint     max_colors;        /* 2 to 256 (advice from caller) */
  uint     actual_colors;     /* 2 to 256 */
  byte     color_map[256*3];  /* (3 * actual_colors) bytes */

  /* filter */
  uint     filter_ofs;        /* byte offset to reach previous pixel's byte (minimum 1) */

  /* input */
  byte[]^  previous_scanline;
  byte[]^  scanline;
  byte[]^  temp_scanline;
  uint     scanline_length;
  uint     scanline_index;
  uint     y;
  uint     adler_crc;

  /* output */
  bool     zlib_sent;
}

/*************************************************************************/

package P_PACK = new PACK (USER_INFO => PNG);

/*************************************************************************/

const byte png_sign[8] = {137, 80, 78, 71, 13, 10, 26, 10};

/********************************************************************/

int write_block (ref WRITE_STREAM stream, byte[] buffer)
{
  if (write (ref stream, buffer) != buffer'length)
    return PNG2_IO_ERROR;
  return 0;
}

/********************************************************************/

int png_signature (PNG info)
{
  return write_block (ref *info.pstream, png_sign);
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

package P_CHUNK

  packed struct CHUNK0
  {
    uint length;
    byte type[4];
  }

  packed struct CHUNK9
  {
    uint crc;
  }

end P_CHUNK;

/********************************************************************/

int save_chunk (PNG info, byte type[4], byte[] data, byte[] data2)
{
  CHUNK0   chunk0;
  CHUNK9   chunk9;
  uint     crc;
  int      rc;

#if debug
  trace ("info: save_chunk (type=%s, length=%d+%d)\n", type, data'length, data2'length);
#endif

  chunk0 = {length => (uint)(data'length + data2'length),
            type   => type };
  swap_long (ref chunk0.length);

  crc = 0;
  update_crc (ref crc, chunk0.type);
  update_crc (ref crc, data);
  update_crc (ref crc, data2);

  chunk9 = {crc => crc};
  swap_long (ref chunk9.crc);

  rc = write_block (ref *info.pstream, chunk0);
  if (rc < 0)
    return rc;

  rc = write_block (ref *info.pstream, data);
  if (rc < 0)
    return rc;

  rc = write_block (ref *info.pstream, data2);
  if (rc < 0)
    return rc;

  rc = write_block (ref *info.pstream, chunk9);
  if (rc < 0)
    return rc;

  return 0;
}

/********************************************************************/

package P2_CHUNK

  packed struct IHDR_CHUNK
  {
    uint width;
    uint height;
    byte bit_depth;
    byte colour_type;
    byte compression;  /* 0 */
    byte filter;       /* 0 */
    byte interlace;    /* 0 */
  }

end P2_CHUNK;

/********************************************************************/

int png_header (PNG t)
{
  IHDR_CHUNK h;

  clear h;
  h.width       = t.width;
  h.height      = t.height;
  h.bit_depth   = 8;
  h.colour_type = t.colour_type;

#if debug
  trace ("info: png_header (width=%u, height=%u, colour_type=%u\n", t.width, t.height, t.colour_type);
#endif

  swap_long (ref h.width);
  swap_long (ref h.height);

  return save_chunk (t, "IHDR", h, "");
}

/********************************************************************/

int secondary_chunks (PNG t)
{
  int rc;

  if (t.colour_type == 3)   /* palette "PLTE" (1 .. 256) x 3 bytes */
                            /* (index 0 = for transparent (optional) contains background color) */
  {
    rc = save_chunk (t, "PLTE", t.color_map[0:3*t.actual_colors], "");
    if (rc < 0)
      return rc;

    if (t.use_transparency > 0)
    {
      byte zero;

      zero = 0;

      /* "tRNS" 1 entry-alpha table  (1 byte value zero) */
      rc = save_chunk (t, "tRNS", zero, "");
      if (rc < 0)
        return rc;

      /* "bKGD" (1 byte value zero) */
      rc = save_chunk (t, "bKGD", zero, "");
      if (rc < 0)
        return rc;
    }
  }


  if (t.colour_type == 6)
  {
    ushort bk[3];

    /* "bKGD" (6 bytes) background color in 3 x short */
    bk = {(ushort)((t.transparent_RGB[0] << 8) + t.transparent_RGB[0]),
          (ushort)((t.transparent_RGB[1] << 8) + t.transparent_RGB[1]),
          (ushort)((t.transparent_RGB[2] << 8) + t.transparent_RGB[2])};

    swap_short (ref bk[0]);
    swap_short (ref bk[1]);
    swap_short (ref bk[2]);

    rc = save_chunk (t, "bKGD", bk, "");
    if (rc < 0)
      return rc;
  }

  return 0;
}

/********************************************************************/

int write_data_chunk_bytes (ref PNG t, byte[] buffer)
{
  const byte zlib_header[2] = {120, 156};

  int  rc;
  uint size0;

  size0 = 0;
  if (!t.zlib_sent)         /* first IDAT chunk */
  {
    size0 = zlib_header'size;
    t.zlib_sent = true;
  }

  rc = save_chunk (t, "IDAT", zlib_header[0:size0], buffer);
  if (rc < 0)
    return rc;

  return (int)buffer'size;
}

/********************************************************************/

int iend_chunk (ref PNG t)
{
  int rc;

  rc = save_chunk (t, "IEND", "", "");
  if (rc < 0)
    return rc;

  t.status = 0;    /* set file status to OK (can be closed without being deleted) */

  return 0;
}

/********************************************************************/

public
int create_png (out PNG          png,
                ref WRITE_STREAM stream,
                    uint         width,
                    uint         height,
                    ushort       max_colors,
                    ushort       use_transparency,
                    byte[3]      transparent_RGB)
{
  int rc;

  clear png;

  if (width < 1 || width > MAX_WIDTH)
    return PNG2_ILLEGAL_WIDTH;

  if (height < 1 || height > MAX_HEIGHT)
    return PNG2_ILLEGAL_HEIGHT;

  if (max_colors == 1 || max_colors > 256)
    return PNG2_ILLEGAL_MAX_COLORS;

  png.pstream = &stream;

  png.width  = width;
  png.height = height;

  png.image_size       = 4 * width * height;
  png.use_transparency = (byte)use_transparency;
  png.transparent_RGB  = transparent_RGB;
  png.max_colors       = max_colors;

#if debug
  trace ("info: width x height   = %u x %u\n", width, height);
  trace ("info: image_size       = %u\n", png.image_size);
  trace ("info: use_transparency = %u\n", use_transparency);
  trace ("info: max_colors       = %u\n", max_colors);
#endif

  png.status = -1;      /* default : file is incomplete */

  rc = png_signature (png);
  if (rc < 0)
  {
    close_png2 (ref png);
    return rc;
  }

  return 0;
}

/*************************************************************************/

public void get_png_size2 (PNG      png,
                           out uint width,
                           out uint height)
{
  width  = png.width;
  height = png.height;
}

/**************************************************************************/

public int close_png2 (ref PNG png)
{
  int retcode;

  free (png.image);
  free (png.scanline);
  free (png.previous_scanline);
  free (png.temp_scanline);

  if (png.status < 0)
    retcode = PNG2_INCOMPLETE;
  else
    retcode = 0;

  clear png;

  return retcode;
}

/*************************************************************************/

/* can reset .use_transparency to 0 if no transparent pixels are found */
/* returns 0 if OK, (+1) if palette table overflow */

int compute_palette (ref PNG t)
{
  int  first, col;
  bool transparent_pixels_found;
  uint i;

  transparent_pixels_found = false;
  t.actual_colors = 0;
  first = 0;

  for (i=0; i<t.image_size; i+=4)
  {
    if (t.use_transparency != 0 && t.image^[i+3] != 255)   /* transparent */
    {
      if (!transparent_pixels_found)
      {
        transparent_pixels_found = true;

        if (t.actual_colors >= 256)
          return +1;  /* overflow : abandon palette and switch to rgb color */

        t.color_map[3*t.actual_colors:3] = t.color_map[0:3];
        t.actual_colors++;
        t.color_map[0:3] = t.transparent_RGB;
        first = 1;
      }
    }
    else
    {
      for (col=first; col<(int)t.actual_colors; col++)
      {
        if (memcmp (t.color_map[3*col:3], t.image^[i:3]) == 0)
          break;
      }

      if (col >= (int)t.actual_colors)
      {
        if (t.actual_colors >= 256)
          return +1;  /* overflow : abandon palette and switch to rgb color */

        t.color_map[3*t.actual_colors+0:3] = t.image^[i+0:3];
        t.actual_colors++;
      }
    }
  }

  if (!transparent_pixels_found)
  {
    t.use_transparency = 0;
#if debug
    trace ("info: use_transparency reset to zero !\n");
#endif
  }

#if debug
  trace ("info: actual_colors = %u\n", t.actual_colors);
#endif

  return 0;
}

/*************************************************************************/

package SAMPLES

  const byte samples_per_pixel[7] = { /* 0: grey        */ 1,
                                      /* 1: unused      */ 1,
                                      /* 2: color       */ 3,
                                      /* 3: index       */ 1,
                                      /* 4: grey+alpha  */ 2,
                                      /* 5: unused      */ 1,
                                      /* 6: color+alpha */ 4 };

end SAMPLES;

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

void filter_scanline (byte       filter,
                      byte[]     previous_scanline,
                      byte[]     old_scanline,
                      ref byte[] new_scanline,
                      uint       nb_bytes,
                      uint       filter_ofs)  /* byte offset to reach next pixel, minimum 1 */
{
  uint i;

  switch (filter)
  {
    case 0:    /* no filter */
      new_scanline[0:nb_bytes] = old_scanline[0:nb_bytes];
      break;

    case 1:    /* (previous)  :  Recon(x) = Filt(x) - Recon(a) */
      new_scanline[0:filter_ofs] = old_scanline[0:filter_ofs];
      for (i=filter_ofs; i<nb_bytes; i++)
        new_scanline[i] = (byte)(old_scanline[i] - old_scanline[i-filter_ofs]);
      break;

    case 2:    /* (up)  :   Recon(x) = Filt(x) - Recon(b) */
      for (i=0; i<nb_bytes; i++)
        new_scanline[i] = (byte)(old_scanline[i] - previous_scanline[i]);
      break;

    case 3:    /* Recon(x) = Filt(x) - floor((Recon(a) + Recon(b)) / 2) */
      for (i=0; i<filter_ofs; i++)
        new_scanline[i] = (byte)(old_scanline[i] - (previous_scanline[i] >> 1));
      for (i=filter_ofs; i<nb_bytes; i++)
        new_scanline[i] = (byte)(old_scanline[i] - ((old_scanline[i-filter_ofs] + previous_scanline[i]) >> 1));
      break;

    case 4:    /* Recon(x) = Filt(x) - PaethPredictor(Recon(a), Recon(b), Recon(c)) */
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

        new_scanline[i] = (byte)((int)old_scanline[i] - p);
      }

      for (i=filter_ofs; i<nb_bytes; i++)
      {
        int p, pa, pb, pc;

        p = (int)old_scanline[i-filter_ofs] + (int)previous_scanline[i] - (int)previous_scanline[i-filter_ofs];

        pa = abs(p - (int)old_scanline[i-filter_ofs]);
        pb = abs(p - (int)previous_scanline[i]);
        pc = abs(p - (int)previous_scanline[i-filter_ofs]);

        if ((pa <= pb) && (pa <= pc))
          p = old_scanline[i-filter_ofs];
        else if (pb <= pc)
          p = previous_scanline[i];
        else
          p = previous_scanline[i-filter_ofs];

        new_scanline[i] = (byte)((int)old_scanline[i] - p);
      }
      break;

   default:
     abort;
  }
}

/**************************************************************************/

int filter_result (byte[] scanline)
{
  uint i;
  int  n, sum;

  sum = 0;

  for (i=0; i<scanline'size; i++)
  {
    n = (tiny)scanline[i];
    sum += abs(n);
  }

  return sum;
}

/**************************************************************************/

/* returns best filter (0..4) */

byte best_filter (ref byte[] previous_scanline,
                  ref byte[] scanline,
                  ref byte[] temp_scanline,
                  uint       nb_bytes,
                  uint       filter_ofs)   /* byte offset to reach next pixel, minimum 1 */
{
  int min_diff, diff, filter, best_filter;

  temp_scanline[0:nb_bytes] = scanline[0:nb_bytes];       /* save original scanline */

  min_diff = 2000000000;
  best_filter = -1;

  for (filter=0; filter<5; filter++)
  {
    filter_scanline ((byte)filter, previous_scanline, temp_scanline, ref scanline, nb_bytes, filter_ofs);
    diff = filter_result (scanline[0:nb_bytes]);
    if (diff < min_diff)
    {
      min_diff = diff;
      best_filter = filter;
    }
  }

#if debug
//  trace ("filter %d\n", best_filter);
#endif

  filter_scanline ((byte)best_filter, previous_scanline, temp_scanline, ref scanline, nb_bytes, filter_ofs);
  previous_scanline[0:nb_bytes] = temp_scanline[0:nb_bytes];

  return (byte)best_filter;
}

/**************************************************************************/

int read_scanline_bytes (ref PNG    t,
                         out byte[] buffer)
{
  uint bytes_read, len, i, first, col;

  clear buffer;

  bytes_read = 0;

  for (;;)
  {
    len = min (buffer'size - bytes_read, t.scanline_length - t.scanline_index);
    buffer[bytes_read:len] = t.scanline^[t.scanline_index:len];
    bytes_read += len;
    t.scanline_index += len;    

    if (bytes_read == buffer'size)   /* enough bytes read */
      break;

    if (t.scanline_length == t.scanline_index)   /* scanline exhausted, compute a new one */
    {
      if (t.y >= t.height)   /* bottom of image */
        break;

      /* convert depending on colour_type */

      switch (t.colour_type)
      {
        case 2:  /* rgb */
          for (i=0; i<t.width; i++)
            t.scanline^[1+i*3:3] = t.image^[4 * (t.width * t.y + i):3];
          break;

        case 3:  /* palette */
          first = (uint)((t.use_transparency != 0) ? 1 : 0);
          for (i=0; i<t.width; i++)
          {
            if (t.use_transparency > 0 && t.image^[4*(t.width * t.y + i) + 3] != 255)   /* transparent */
              col = 0;      /* transparent is color 0 */
            else
            {
              for (col=first; col<t.actual_colors; col++)
              {
                if (memcmp (t.color_map[3*col:3], t.image^[4*(t.width * t.y + i):3]) == 0)
                  break;
              }
            }
            t.scanline^[1+i] = (byte)col;
          }
          break;

        case 6:  /* rgbs */
          t.scanline^[1 : 4 * t.width] = t.image^[4 * t.width * t.y : 4 * t.width];
          break;

        default:
          abort;
      }

#if debug
//  trace ("\n");
//  trace ("info: line %d\n", t.y);
//  trace ("info: scanline before :\n");
//  trace_block (t.scanline, 32);
#endif

      t.scanline^[0] = best_filter (ref t.previous_scanline^[1:t.previous_scanline^'length-1],
                                    ref t.scanline^[1:t.scanline^'length-1],
                                    ref t.temp_scanline^[1:t.temp_scanline^'length-1],
                                    t.scanline_length-1,
                                    t.filter_ofs);

#if debug
//  trace ("info: scanline after : (filter=%u)\n", t.scanline[0]);
//  trace_block (t.scanline[0:32]);
#endif

      t.scanline_index = 0;

      t.y++;
    }
  }

  update_adler (ref t.adler_crc, buffer[0:bytes_read]);

#if debug
  trace ("info: read_scanline_bytes() returns %d bytes\n", bytes_read);
//  trace_block (buffer[0:min (size, 32)]);
#endif

  return (int)bytes_read;
}

/**************************************************************************/

public int write_png (ref PNG png, byte[] buffer)
{
  int rc;

  if ((buffer'size & 3) != 0)
    return PNG2_ILLEGAL_BUFFER_SIZE;    /* size is not multiple of 4 */

  if (png.image == null)
    png.image = new byte [png.image_size];

  if (buffer'size > png.image_size || png.image_offset + buffer'size > png.image_size)
    return PNG2_TOO_MUCH_DATA;

  png.image^[png.image_offset:buffer'size] = buffer;

  png.image_offset += buffer'size;

  if (png.image_offset < png.image_size)   /* further bytes expected */
    return 0;


  /* png.image is complete, let's proceed */


#if debug
  trace ("info: image read-in complete\n");
//  trace_block (png.image[0:min(png.image_size, 32)]);
#endif


  /* compute colour_type */

  if (png.max_colors > 0 &&           /* user wants palette */
      png.use_transparency <= 1 &&    /* and simple transparency is asked */
      compute_palette(ref png) == 0)  /* and there is no palette overflow */
  {
    png.colour_type = 3;  /* palette index with at most one transparent color */
  }
  else   /* full rgb */
  {
    if (png.use_transparency > 0)   /* 1=simple or 2=translucid */
    {
      png.colour_type = 6;   /* RGBA */
    }
    else    /* no transparency */
    {
      png.colour_type = 2;   /* RGB */
    }
  }

  rc = png_header (png);
  if (rc < 0)
    return rc;

  rc = secondary_chunks (png);
  if (rc < 0)
    return rc;

  png.filter_ofs = samples_per_pixel[png.colour_type];
  png.scanline_length = 1 + png.filter_ofs*png.width;
  png.scanline_index = png.scanline_length;

#if debug
  trace ("info: filter_ofs      = %u\n", png.filter_ofs);
  trace ("info: scanline_length = %u\n", png.scanline_length);
#endif

  free png.scanline;
  free png.previous_scanline;
  free png.temp_scanline;

  png.scanline          = new byte [png.scanline_length];
  png.previous_scanline = new byte [png.scanline_length];
  png.temp_scanline     = new byte [png.scanline_length];


  /* compress and write IDAT chunks */

  png.adler_crc = 1;

  rc = pack (ref png, read_scanline_bytes, write_data_chunk_bytes);
  if (rc < 0)
  {
    if (rc == PK_WRITE_ERROR)
      return PNG2_IO_ERROR;
    return PNG2_PACK_ERROR;
  }

  swap_long (ref png.adler_crc);

  rc = save_chunk (png, "IDAT", png.adler_crc, "");
  if (rc < 0)
    return rc;

  rc = iend_chunk (ref png);
  if (rc < 0)
    return rc;

  return 0;
}

/*************************************************************************/
#end unsafe
/*************************************************************************/
