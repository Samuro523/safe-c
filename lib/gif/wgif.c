
/* wgif.c : GIF image writer */

use ../stream;
use ../image;     // for error codes

/*************************************************************************/

const uint2 NO_PREFIX = 65535;
const uint2 NIL       =     0;

struct STRING
{
  uint2 prefix;
  char  character;
  uint2 head;      /* index of first child, or NIL    */
  uint2 next;      /* linked list of children, or NIL */
}

/********************************************************************/

struct GIF
{
  short         status;           /* usually -1, set to 0 if the file is complete */
  bool          quantize_colors;
  byte          transparent_RGB[3];

  /* gif header data */
  uint          image_x_size;      /* 0 to 65535                  */
  uint          image_y_size;      /* 0 to 65535                  */
  byte          bits_per_pixel;    /* 1 to 8                      */
  uint          max_colors;        /* 2 to 256                    */
  byte          color_map[256*3];  /* (3 * max_colors) bytes      */
  uint          valid_entries;     /* #valid entries in color_map */
  long          table_offset;      /* offset of color_map in file */
  bool          use_transparency;  /* false = normal, true = use 4th byte */

  /* write codes */
  byte          initial_code_size; /* must have size 1 ! */
  uint          code_size;
  byte          cout[256];         /* output buffer */
  byte          count;             /* nb valid bytes in outc[] */
  byte          current_byte;      /* byte being built, bit by bit */
  uint          nb_bits;           /* nb of bits written into current byte */

  /* read data */
  uint           size;             /* nb bytes read until now */
  uint           max_size;         /* total nb bytes of image */

  /* compression data */
  STRING[]^      table;            /* 4096 entries allocated via new */
  uint           nb_entries;       /* nb of valid table entries */
  uint           code_clear_table;
  uint           code_end_of_information;
  uint2          prefix;
}

/*************************************************************************/

int write_block (ref WRITE_STREAM stream, byte[] buffer)
{
  if (write (ref stream, buffer) != buffer'length)
    return IMG_FILE_ERROR;
  return 0;
}

/********************************************************************/

int gif_signature (ref WRITE_STREAM stream)
{
  return write_block (ref stream, "GIF89a");
}

/********************************************************************/

int gif_screen_descriptor (ref GIF info, ref WRITE_STREAM stream)
{
  byte buf[7];
  int  rc;

  clear buf;

  buf[0] = (byte)(info.image_x_size & 255);
  buf[1] = (byte)(info.image_x_size >> 8);
  buf[2] = (byte)(info.image_y_size & 255);
  buf[3] = (byte)(info.image_y_size >> 8);
  buf[4] = (byte)(128 | ((info.bits_per_pixel-1) << 4) | (info.bits_per_pixel-1));
  buf[5] = 0;
  buf[6] = 0;

  rc = write_block (ref stream, buf);
  if (rc < 0)
    return rc;

  info.max_colors = (1 << (uint)info.bits_per_pixel);

  /* save offset of colormap within file */
  info.table_offset = lseekw (ref stream, 0L, SEEK_CUR);
  if (info.table_offset < 0)
    return GIF2_SEEK_ERROR;

  /* write dummy colormap */
  rc = write_block (ref stream, info.color_map[0:3*info.max_colors]);
  if (rc < 0)
    return rc;

  info.valid_entries = 0;

  if (info.use_transparency)    /* color 0 = transparent (white) */
  {
    info.color_map[0] = info.transparent_RGB[0];
    info.color_map[1] = info.transparent_RGB[1];
    info.color_map[2] = info.transparent_RGB[2];
    info.valid_entries++;
  }

  return 0;
}

/********************************************************************/

int gif_graphic_control_extension (GIF info, ref WRITE_STREAM stream)
{
  byte buf[8];

  if (!info.use_transparency)
    return 0;

  clear buf;

  buf[0] = 0x21;   /* extension block */
  buf[1] = 0xF9;   /* graphic control extension */
  buf[2] = 4;      /* 4 bytes */
  buf[3] = (byte)info.use_transparency;
  buf[4] = 0;   /* delay */
  buf[5] = 0;   /* delay */
  buf[6] = 0;   /* transparent color (index 0) */
  buf[7] = 0;   /* end of block (zero length) */

  return write_block (ref stream, buf);
}

/********************************************************************/

int gif_image_descriptor (GIF info, ref WRITE_STREAM stream)
{
  byte buf[10];

  clear buf;

  buf[0] = 0x2C;
  buf[5] = (byte)(info.image_x_size & 255);
  buf[6] = (byte)(info.image_x_size >> 8);
  buf[7] = (byte)(info.image_y_size & 255);
  buf[8] = (byte)(info.image_y_size >> 8);

  return write_block (ref stream, buf);
}

/********************************************************************/

int write_out_buffer (ref GIF info, ref WRITE_STREAM stream)
{
  int rc;

  /* write block size */
  rc = write_block (ref stream, info.count);
  if (rc < 0)
    return rc;

  /* write block */
  rc = write_block (ref stream, info.cout[0:info.count]);
  if (rc < 0)
    return rc;

  info.count = 0;

  return 0;
}

/*************************************************************************/

int flush_codes (ref GIF info, ref WRITE_STREAM stream)
{
  int rc;

  if (info.nb_bits > 0)     /* some bits to write */
  {
    info.cout[info.count++] = info.current_byte;
    info.nb_bits = 0;
    info.current_byte = 0;
  }

  if (info.count == 255)
  {
    rc = write_out_buffer (ref info, ref stream);
    if (rc < 0)
      return rc;
  }

  return 0;
}

/*************************************************************************/

int write_raster_code (ref GIF info, uint code, ref WRITE_STREAM stream)
{
  uint max_bits, i, bit;
  int  rc;

  max_bits = info.code_size;

  for (i=0; i<max_bits; i++)
  {
    bit = (uint)((code & (1 << i)) != 0);

    info.current_byte |= (byte)(bit << info.nb_bits);
    info.nb_bits++;

    if (info.nb_bits == 8)   /* current byte is full */
    {
      rc = flush_codes (ref info, ref stream);
      if (rc < 0)
        return rc;
    }
  }

  return 0;
}

/********************************************************************/

int init_compression (ref GIF info, ref WRITE_STREAM stream)
{
  int  rc;
  uint two_power_code_size, i;


  /* initialize code_size */

  info.initial_code_size = info.bits_per_pixel;
  if (info.initial_code_size == 1)    /* special case */
    info.initial_code_size = 2;

  rc = write_block (ref stream, info.initial_code_size);
  if (rc < 0)
    return rc;


  /* initialize write codes */

  info.count        = 0;
  info.nb_bits      = 0;
  info.current_byte = 0;
  info.code_size    = 1 + (uint)info.initial_code_size;


  /* initialize read data */

  info.size      = 0;
  info.max_size  = (uint)info.image_x_size * (uint)info.image_y_size;


  /* allocate string table */

  info.table = new STRING [4096];


  /* initialize first (2 ** initial_code_size + 2) entries of string table */

  two_power_code_size = (uint)(1 << info.initial_code_size);
  for (i=0; i<two_power_code_size; i++)
  {
    info.table^[i].prefix    = NO_PREFIX;
    info.table^[i].character = (char)i;
    info.table^[i].head      = NIL;
    info.table^[i].next      = NIL;
  }
  info.nb_entries = two_power_code_size;


  /* initialize special codes */

  info.code_clear_table        = info.nb_entries++;
  info.code_end_of_information = info.nb_entries++;


  info.prefix = NO_PREFIX;

  rc = write_raster_code (ref info, info.code_clear_table, ref stream);
  if (rc < 0)
    return rc;

  return 0;
}

/********************************************************************/

public int create_gif (out GIF gif,
                       ref WRITE_STREAM stream,
                       uint2   width,
                       uint2   height,
                       uint2   max_colors,
                       uint2   use_transparency,
                       byte[3] transparent_RGB)
{
  int   rc, bits_per_pixel;
  uint2 colors;

  clear gif;

  if (width < 1)
    return GIF2_ILLEGAL_WIDTH;

  if (height < 1)
    return GIF2_ILLEGAL_HEIGHT;

  if (max_colors == 1 || max_colors > 256)
    return GIF2_ILLEGAL_MAX_COLORS;

  colors = max_colors;

  gif.image_x_size = (uint)width;
  gif.image_y_size = (uint)height;

  if (colors == 0)
    bits_per_pixel = 8;   // quantize colors
  else
  {
    if (use_transparency != 0 && colors == 2)
      colors++;
    for (bits_per_pixel=1; bits_per_pixel<8; bits_per_pixel++)
    {
      if (((uint)(1 << bits_per_pixel)) >= colors)
        break;
    }
  }

  gif.transparent_RGB = transparent_RGB;
  gif.quantize_colors  = (colors == 0);
  gif.use_transparency = (use_transparency != 0);
  gif.bits_per_pixel   = (byte)bits_per_pixel;

  gif.status = -1;      /* default : file is incomplete */

  /* write the header */
  rc = gif_signature (ref stream);
  if (rc < 0)
  {
    (void)close_gif2 (ref gif);
    return rc;
  }

  rc = gif_screen_descriptor (ref gif, ref stream);
  if (rc < 0)
  {
    (void)close_gif2 (ref gif);
    return rc;
  }

  rc = gif_graphic_control_extension (gif, ref stream);
  if (rc < 0)
  {
    (void)close_gif2 (ref gif);
    return rc;
  }

  rc = gif_image_descriptor (gif, ref stream);
  if (rc < 0)
  {
    (void)close_gif2 (ref gif);
    return rc;
  }

  rc = init_compression (ref gif, ref stream);
  if (rc < 0)
  {
    (void)close_gif2 (ref gif);
    return rc;
  }

  return 0;
}

/*************************************************************************/

int diff (int x, int y)
{
  if (x > y)
    return x - y;

  return y - x;
}

/*************************************************************************/

public int write_gif (ref GIF gif, byte[] buffer, ref WRITE_STREAM stream)
{
  uint  nb_pixels, i;
  int   rc, col, j, first;

  if ((buffer'size & 3L) != 0)
    return GIF2_ILLEGAL_BUFFER_SIZE;    /* size is not multiple of 4 */

  nb_pixels = (buffer'size >> 2);

  if (gif.size + nb_pixels > gif.max_size)
    return GIF2_TOO_MUCH_DATA;

  gif.size += nb_pixels;

  first = (int)gif.use_transparency;

  for (i=0; i<nb_pixels; i++)
  {
    ref byte[4] pixel = buffer[i*4:4];


    /* select matching color 'col' */

    if (gif.use_transparency && pixel[3] < 255)
      col = 0;
    else
    {
      if (gif.quantize_colors)
      {
        for (col=first; col<(int)gif.valid_entries; col++)
        {
          if (30*diff (gif.color_map[3*col+0], pixel[0]) +
              59*diff (gif.color_map[3*col+1], pixel[1]) +
              11*diff (gif.color_map[3*col+2], pixel[2]) <= 1000)
            break;
        }
      }
      else   /* exact color matching */
      {
        for (col=first; col<(int)gif.valid_entries; col++)
        {
          if (gif.color_map[3*col+0] == pixel[0] &&
              gif.color_map[3*col+1] == pixel[1] &&
              gif.color_map[3*col+2] == pixel[2])
            break;
        }
      }

      if (col >= (int)gif.valid_entries)
      {
        if (gif.valid_entries >= gif.max_colors)
          col = (int)gif.max_colors - 1;
        else
        {
          gif.color_map[3*gif.valid_entries+0] = pixel[0];
          gif.color_map[3*gif.valid_entries+1] = pixel[1];
          gif.color_map[3*gif.valid_entries+2] = pixel[2];
          gif.valid_entries++;
        }
      }
    }


    /* encode color 'col' */

    if (gif.prefix == NO_PREFIX)    /* is always present in table */
    {
      gif.prefix = (uint2)col;
    }
    else     /* prefix is non-null */
    {
      uint p;

      /* search (gif.prefix,col) in table */

      p = gif.table^[gif.prefix].head;
      while (p != NIL)
      {
        if (gif.table^[p].character == (char)col)   /* found ! */
          break;
        p = gif.table^[p].next;
      }

      if (p != NIL)    /* (prefix,col) was found in string table ! */
      {
        gif.prefix = (uint2)p;
      }
      else     /* not found in table */
      {
        ref STRING s = gif.table^[gif.nb_entries];

        /* 1ø) add couple (prefix, col) to string table */

        s.prefix    = gif.prefix;
        s.character = (char)col;
        s.head      = NIL;
        s.next      = gif.table^[gif.prefix].head;

        gif.table^[gif.prefix].head = (uint2)gif.nb_entries;

        gif.nb_entries++;



        /* 2ø) output code of the prefix */

        rc = write_raster_code (ref gif, gif.prefix, ref stream);
        if (rc < 0)
          return rc;

        gif.prefix = (uint2)col;

        if (gif.nb_entries > (uint)(1 << gif.code_size))
        {
          if (gif.code_size < 12)
            gif.code_size++;
        }

        if (gif.nb_entries == 4096)
        {
          rc = write_raster_code (ref gif, gif.code_clear_table, ref stream);
          if (rc < 0)
            return rc;

          gif.code_size = 1 + (uint)gif.initial_code_size;
          gif.nb_entries = gif.code_end_of_information + 1;

          for (j=0; j<(int)gif.nb_entries; j++)
          {
            gif.table^[j].head = NIL;
            gif.table^[j].next = NIL;
          }
        }
      }
    }
  }

  if (gif.size == gif.max_size &&    /* no more pixels */
      nb_pixels > 0)                 /* not an null size call */
  {
    byte terminator = 0x3B;

    if (gif.prefix != NO_PREFIX)
    {
      rc = write_raster_code (ref gif, gif.prefix, ref stream);
      if (rc < 0)
        return rc;
    }

    rc = write_raster_code (ref gif, gif.code_end_of_information, ref stream);
    if (rc < 0)
      return rc;

    rc = flush_codes (ref gif, ref stream);
    if (rc < 0)
      return rc;

    if (gif.count > 0)   /* some bytes remaining to be sent */
    {
      rc = write_out_buffer (ref gif, ref stream);
      if (rc < 0)
        return rc;
    }

    /* write empty data block */
    rc = write_out_buffer (ref gif, ref stream);
    if (rc < 0)
      return rc;

    /* write terminator byte */
    rc = write_block (ref stream, terminator);
    if (rc < 0)
      return rc;

    /* write color map */
    if (lseekw (ref stream, gif.table_offset, SEEK_SET) < 0)
      return GIF2_SEEK_ERROR;

    rc = write_block (ref stream, gif.color_map[0:3*gif.max_colors]);
    if (rc < 0)
      return rc;

    /* set file status to OK */
    gif.status = 0;
  }

  return 0;
}

/*************************************************************************/

public void get_gif_size2 (GIF      gif,
                           out uint width,
                           out uint height)
{
  width  = gif.image_x_size;
  height = gif.image_y_size;
}

/**************************************************************************/

public int close_gif2 (ref GIF gif)
{
  int retcode;

  if (gif.status < 0)
    retcode = GIF2_INCOMPLETE;
  else
    retcode = 0;

  free gif.table;

  clear gif;

  return retcode;
}

/*************************************************************************/
