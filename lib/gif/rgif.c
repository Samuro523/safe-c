
/*****************************/
/* rgif.c : GIF image reader */
/*****************************/

/* limitations:
   . reads only the first image of a file (ignore screen data)
   . x/y ratio parameter is not supported.
*/

use ../stream, ../strings;
use ../image;    // for error codes

/********************************************************************/

const uint NO_PREFIX = 65535;

struct STRING
{
  char first;      /* first character of string */
  uint prefix;
  char character;
}

/********************************************************************/

const ushort MAGIC = 0x28BA;

typedef byte[]^ PLINE;

struct GIF
{
  /* gif header data */
  uint  screen_x_size;             /* 0 to 65535 */
  uint  screen_y_size;             /* 0 to 65535 */

  uint  offset_x;                  /* 0 to 65535 */
  uint  offset_y;                  /* 0 to 65535 */
  uint  image_x_size;              /* 0 to 65535 */
  uint  image_y_size;              /* 0 to 65535 */

  uint  global_max_colors;         /* 2 to 256   */
  byte  global_color_map[256*3];   /* (3 * max_colors) bytes */

  uint  local_max_colors;          /* 2 to 256   */
  byte  local_color_map[256*3];    /* (3 * max_colors) bytes */

  byte  background_color[3];       /* RGB values */

  int   transparent_color_index;   /* 256 = none */

  byte  user_input_flag;           /* 0 or 1=user input expected */
  byte  disposal_method;           /* 0 .. 7  (2=restore to background, 3=restore to previous) */
  int   delay;                     /* 1/100 secs or 0 if not used */
  int   repeat_count;              /* nb times to repeat all image, 0=infinite */
  bool  use_interlace_mode;

  /* read data */
  byte  in[256];             /* input buffer */
  uint  index;
  uint  count;
  byte  current_byte;
  uint  bits_left;           /* nb of bits remaining in current byte */
  byte  initial_code_size;
  uint  code_size;

  /* write data */
  uint  size;        /* nb bytes written until now */
  uint  max_size;    /* total nb bytes of image */
  uint2 pass;        /* 1..4 for interlaced display (0 = normal) */
  uint  y_multiple;  /* 8, 4 or 2 (for interlaced mode only) */

  PLINE[]^ line_buf;    /* indirect output buffer allocated */
                        /* for interlace mode : first dimension has image_y_size pointers. */

  uint    y;              /* writing line (for interlace mode)     */
  uint    x;              /* writing column (for interlace mode)   */
  uint    out_col;        /* output col (for interlace mode)       */
  uint    out_line;       /* output line (for interlace mode)      */
  byte[]^ buffer;         /* 4096-byte output buffer allocated via */
                          /* malloc().                             */
  uint    buffer_index;   /* index of next pending pixel */
  uint    pending_count;  /* nb of pixels in buffer that remain */
                          /* to be sent.                        */

  /* decompressor data */
  STRING[]^  table;       /* 4096 entries allocated */
  uint       nb_entries;  /* nb of valid table entries */
  uint       code_clear_table;
  uint       code_end_of_information;
  bool       first_time;  /* true if we just did a clear table */
  uint       old_code;    /* previous code */
}

/********************************************************************/

int read_block (ref READ_STREAM stream, out byte[] buffer)
{
  if (read (ref stream, out buffer) != buffer'length)
    return IMG_FILE_ERROR;
  return 0;
}

/********************************************************************/

int gif_signature (ref READ_STREAM stream)
{
  char sign[6];
  int  rc;

  rc = read_block (ref stream, out sign);
  if (rc < 0)
    return rc;

  if (memcmp (sign[0:3], "GIF") != 0)
    return IMG_NOT_RECOGNIZED;

  return 0;
}

/********************************************************************/

int gif_screen_descriptor (ref GIF info, ref READ_STREAM stream)
{
  byte buf[7], col;
  int  rc, i;
  byte background_color_index;
  uint bits_per_pixel;

  rc = read_block (ref stream, out buf);
  if (rc < 0)
    return rc;

  info.screen_x_size = buf[0] | (buf[1] << 8);
  info.screen_y_size = buf[2] | (buf[3] << 8);

  bits_per_pixel = 1 + (buf[4] & 7);
  info.global_max_colors = (1 << bits_per_pixel);

  if ((buf[4] & 128) != 0)     /* global color map is present */
  {
    /* read global colormap from file */
    rc = read_block (ref stream, out info.global_color_map[0 : 3*info.global_max_colors]);
    if (rc < 0)
      return rc;
  }
  else    /* no global colormap : build default one with grey tones */
  {
    for (i=0; i<(int)info.global_max_colors; i++)
    {
      col = (byte)(i * 255 / ((int)info.global_max_colors - 1));
      info.global_color_map[i*3]   = col;
      info.global_color_map[i*3+1] = col;
      info.global_color_map[i*3+2] = col;
    }
  }

  /* store background color */
  background_color_index = buf[5];
  info.background_color = info.global_color_map[3*background_color_index:3];

  return 0;
}

/********************************************************************/

int get_delimiter (ref READ_STREAM stream, out byte delimiter)
{
  int rc;

  for (;;)
  {
    rc = read_block (ref stream, out delimiter);
    if (rc < 0)
      return rc;

    if (delimiter == 0x2C || delimiter == 0x3B || delimiter == 0x21)
      break;
  }

  return 0;
}

/********************************************************************/

int process_extension_block (ref GIF info, ref READ_STREAM stream)
{
  int  rc;
  byte code, count;
  byte buffer[256];

  rc = read_block (ref stream, out code);
  if (rc < 0)
    return rc;

  for (;;)
  {
    rc = read_block (ref stream, out count);
    if (rc < 0)
      return rc;

    if (count == 0)
      return 0;

    clear buffer;
    rc = read_block (ref stream, out buffer[0:count]);
    if (rc < 0)
      return rc;

    if (code == 0xF9 && count == 4)   /* graphic control extension */
    {
      if ((buffer[0] & 1) != 0)   /* transparent color is specified */
        info.transparent_color_index = buffer[3];

      info.disposal_method = (byte)((buffer[0] >> 2) &  7);
      info.user_input_flag = (byte)((buffer[0] >> 1) &  1);
      info.delay = (int)(buffer[1] | (buffer[2] << 8));
    }

    if (code == 0xFF && count == 11)   /* netscape control extension */
    {
      if (strnicmp (buffer, "NETSCAPE", 8) == 0 &&
          buffer[11] == 3 && buffer[12] == 1)
      {
        info.repeat_count = (int)(buffer[13] | (buffer[14] << 8));
      }
    }
  }
}

/********************************************************************/

int read_image_descriptor (ref GIF info, ref READ_STREAM stream)
{
  byte buf[9];
  int  rc;
  uint bits_per_pixel;

  rc = read_block (ref stream, out buf);
  if (rc < 0)
    return rc;

  info.offset_x     = buf[0] | (buf[1] << 8);
  info.offset_y     = buf[2] | (buf[3] << 8);
  info.image_x_size = buf[4] | (buf[5] << 8);
  info.image_y_size = buf[6] | (buf[7] << 8);

  if (info.offset_x >= info.screen_x_size ||
      info.offset_y >= info.screen_y_size ||
      info.offset_x + info.image_x_size > info.screen_x_size ||
      info.offset_y + info.image_y_size > info.screen_y_size)
    return GIF_IMAGE_OUTSIDE_SCREEN;

  if ((buf[8] & 128) != 0)   /* local color map present */
  {
    bits_per_pixel = 1 + (buf[8] & 7);
    info.local_max_colors = (1 << bits_per_pixel);
    rc = read_block (ref stream, out info.local_color_map[0 : 3*info.local_max_colors]);
    if (rc < 0)
      return rc;
  }
  else   /* use global color map */
  {
    info.local_color_map[0:3*info.global_max_colors] = info.global_color_map[0:3*info.global_max_colors];
    info.local_max_colors = info.global_max_colors;
  }

  info.use_interlace_mode = (buf[8] & 64) != 0;

  return 0;
}

/********************************************************************/

int handle_image_sequence (ref GIF info, ref READ_STREAM stream)
{
  int  rc;
  byte delimiter;

  info.transparent_color_index = 256;

  for (;;)
  {
    rc = get_delimiter (ref stream, out delimiter);
    if (rc < 0)
      return rc;

    switch (delimiter)
    {
      case 0x21:
        rc = process_extension_block (ref info, ref stream);
        if (rc < 0)
          return rc;
        break;

      case 0x2C:
        rc = read_image_descriptor (ref info, ref stream);
        if (rc < 0)
          return rc;
        return 0;      /* DONE ! */

      case 0x3B:
        return GIF_TRAILER;    /* early end-of-file (no image data) */

      default:
        return GIF_BAD_FORMAT;
    }
  }
}

/********************************************************************/

int init_decompressor (ref GIF info, ref READ_STREAM stream)
{
  int  rc;
  uint two_power_code_size, i;


  /* initialize read data */

  rc = read_block (ref stream, out info.initial_code_size);
  if (rc < 0)
    return rc;

  if (info.initial_code_size < 2 || info.initial_code_size > 11)
    return GIF_BAD_CODE_SIZE;

  info.index     = 0;
  info.count     = 0;
  info.bits_left = 0;
  info.code_size = 1 + (uint)info.initial_code_size;


  /* initialize write data */

  info.size      = 0;
  info.max_size  = (uint)info.image_x_size * (uint)info.image_y_size;

  if (info.use_interlace_mode)
    info.pass = 1;                 /* start with pass 1 */
  else
    info.pass = 0;                 /* normal (non-interlace) mode */
  info.y_multiple = 8;


  /* allocate string table */

  info.table = new STRING [4096];


  /* initialize first (2 ** initial_code_size + 2) entries of string table */

  two_power_code_size = (uint)(1 << (uint)info.initial_code_size);
  for (i=0; i<two_power_code_size; i++)
  {
    info.table^[i].first     = (char)i;
    info.table^[i].prefix    = NO_PREFIX;
    info.table^[i].character = (char)i;
  }
  info.nb_entries = two_power_code_size;

  /* initialize special codes */
  info.code_clear_table        = info.nb_entries++;
  info.code_end_of_information = info.nb_entries++;

  /* init first_time flag */
  info.first_time = true;

  /* allocate indirect output buffer for interlace mode */
  if (info.use_interlace_mode)
  {
    info.line_buf = new PLINE [info.image_y_size];   /* indirect */

    /* set output variables to zero */
    info.out_col  = 0;
    info.out_line = 0;
    info.x        = 0;
    info.y        = 0;
  }

  /* allocate output buffer */
  info.buffer = new byte[4096];
  info.buffer_index  = 0;
  info.pending_count = 0;

  return 0;
}

/********************************************************************/

void end_decompressor (ref GIF t)
{
  uint i;

  free t.buffer;
  t.buffer = null;

  if (t.line_buf != null)
  {
    for (i=0; i<t.image_y_size; i++)
      free t.line_buf^[i];
    free t.line_buf;
    t.line_buf = null;
  }

  free t.table;
  t.table = null;
}

/********************************************************************/

int read_raster_code (ref GIF info, out uint code, ref READ_STREAM stream)
{
  uint cout, nb_bits, bit, i;
  int  rc;
  byte count;

  code = 0;

  cout = 0;
  nb_bits = info.code_size;

  for (i=0; i<nb_bits; i++)
  {
    if (info.bits_left == 0)
    {
      if (info.index == info.count)
      {
        rc = read_block (ref stream, out count);   /* read block size */
        if (rc < 0)
          return rc;

        if (count == 0)         /* no data bytes to read */
          return GIF_END_OF_RASTER_DATA;

        rc = read_block (ref stream, out info.in[0:count]); /* read next block */
        if (rc < 0)
          return rc;

        info.index = 0;
        info.count = count;
      }

      info.current_byte = info.in[info.index++];
      info.bits_left = 8;
    }

    bit = (info.current_byte & 1);
    info.current_byte >>= 1;
    info.bits_left--;

    cout |= ((uint)bit) << i;
  }

  code = cout;

  return 0;
}

/********************************************************************/

/* assertion: the output buffer 'buffer' is empty and will be filled */
/*            with at most 4096 bytes (0 bytes in interlace mode).   */

int write_string (ref GIF info, STRING str)
{
  uint len, i, y, q, s;
  byte buffer[4096];


  /* build sequence to write in 'p', length 'len' */

  q   = buffer'size;
  len = 0;

  clear buffer;

  /* store in reverse order */

  buffer[--q] = (byte)str.character;
  len++;
  s = str.prefix;

  if (s != NO_PREFIX)
  {
    for (;;)   /* store in reverse order */
    {
      buffer[--q] = (byte)info.table^[s].character;
      len++;
      s = info.table^[s].prefix;
      if (s == NO_PREFIX)
        break;
    }
  }

  info.size += len;
  if (info.size > info.max_size)   /* too many pixels */
    return GIF_TOO_MANY_PIXELS;


  if (info.use_interlace_mode)
  {
    /* store character sequence in appropriate indirect line */
    for (i=0; i<len; i++)
    {
      if (info.x >= info.image_x_size)   /* scan line is done */
      {
        info.x = 0;    /* restart at x=0 */

        /* select next interlace line to be drawn */
        y = (uint)info.y + (uint)info.y_multiple;

        while (y >= (uint)info.image_y_size)
        {
          switch (info.pass)
          {
            case 1:
              y = 4;
              break;

            case 2:
              y = 2;
              info.y_multiple = 4;
              break;

            case 3:
              y = 1;
              info.y_multiple = 2;
              break;

            default:          /* should never happen */
              return GIF_INTERN_ERROR;   /* too many pixels */
          }
          info.pass++;
        }

        info.y = (uint)y;
      }

      if (info.line_buf^[info.y] == null)
      {
        /* we need to allocate a buffer for this line */
        info.line_buf^[info.y] = new byte [info.image_x_size];
      }

      info.line_buf^[info.y]^[info.x++] = buffer[q+i];
    }

    info.buffer_index  = 0;
    info.pending_count = 0;
  }
  else    /* simple non-interlace mode */
  {
    /* simply store the character sequence in output buffer */
    info.buffer^[0:len] = buffer[q:len];
    info.buffer_index  = 0;
    info.pending_count = len;
  }

  return 0;
}

/********************************************************************/

/* for interlace mode :                              */
/* copy some bytes from indirect in_buf into buffer. */
/* assertion: buffer is initially empty.             */

void fill_buffer (ref GIF info)
{
  uint last, count;


  /* let's now fill the output buffer, if possible */

  if (info.out_line >= info.image_y_size ||
      info.line_buf^[info.out_line] == null)
  {
    info.buffer_index  = 0;
    info.pending_count = 0;    /* no bytes available */
    return;
  }


  /* compute nb of bytes that can be copied */

  if (info.out_line == info.y)   /* line is being written */
    last = info.x;
  else
    last = info.image_x_size;

  count = last - info.out_col;

  if (count > 4096)
    count = 4096;


  /* copy the bytes */

  info.buffer^[0:count] = info.line_buf^[info.out_line]^[info.out_col:count];

  info.buffer_index  = 0;
  info.pending_count = count;

  info.out_col += count;
  if (info.out_col == info.image_x_size)     /* end of line */
  {
    /* we can free this line */
    free info.line_buf^[info.out_line];
    info.line_buf^[info.out_line] = null;

    /* prepare for next line */
    info.out_col = 0;
    info.out_line++;
  }
}

/********************************************************************/

public int close_gif (ref GIF gif)
{
  uint i;

  free gif.buffer;

  if (gif.line_buf != null)
  {
    for (i=0; i<gif.image_y_size; i++)
      free gif.line_buf^[i];
    free gif.line_buf;
  }

  free gif.table;

  clear gif;

  return 0;
}

/**************************************************************************/

public void get_gif_size (GIF      gif,
                          out uint width,
                          out uint height)
{
  width  = gif.image_x_size;
  height = gif.image_y_size;
}

/**************************************************************************/

public void get_gif_attributes (GIF                  gif,
                                out IMAGE_ATTRIBUTES attr)
{
  clear attr;

  attr.uses_transparency = (byte)(gif.transparent_color_index != 256);

  attr.is_gif = true;

  attr.screen_width  = (int)gif.screen_x_size;
  attr.screen_height = (int)gif.screen_y_size;
  attr.color         = gif.background_color;
  attr.offset_x      = (int)gif.offset_x;
  attr.offset_y      = (int)gif.offset_y;
  attr.image_width   = (int)gif.image_x_size;
  attr.image_height  = (int)gif.image_y_size;
  attr.max_colors    = (int)gif.local_max_colors;
  attr.hit_key       = gif.user_input_flag;
  attr.disposal_method = gif.disposal_method;
  attr.delay         = gif.delay;
  attr.repeat_count  = gif.repeat_count;
}

/**************************************************************************/

public int read_gif (ref GIF gif, out byte[] buffer, ref READ_STREAM stream)
{
  uint    nb_pixels;
  int     rc, j;
  uint    code;

  clear buffer;

  if ((buffer'size & 3) != 0)
    return IMG_ILLEGAL_BUFFER_SIZE;    /* size is not multiple of 4 */

  nb_pixels = (buffer'size >> 2);  /* nb of pixels to fetch from image. */
                                   /* (this counter decreases til zero) */

  j = 0;

  for (;;)
  {
    if (gif.pending_count > 0)   /* pixels left to send */
    {
      uint len, i;
      uint color;

      /* compute nb of pixels to copy */
      if (nb_pixels > gif.pending_count)
        len = gif.pending_count;
      else
        len = nb_pixels;

      /* write the pixels */
      for (i=0; i<len; i++)
      {
        color = gif.buffer^[gif.buffer_index++];

        if (gif.transparent_color_index == (int)color)   // transparent
        {
          buffer[j:3] = gif.local_color_map[color+color+color:3];
          buffer[j+3] = 0;  // transparent
        }
        else
        {
          buffer[j:3] = gif.local_color_map[color+color+color:3];
          buffer[j+3] = 0xFF;  // opaque
        }

        j += 4;
      }

      nb_pixels        -= len;
      gif.pending_count -= len;
    }


    if (nb_pixels == 0)    /* output buffer is full */
      return 0;


    /* read additional data into output buffer (at most nb_pixels) */

    if (gif.use_interlace_mode)
      fill_buffer (ref gif);

    if (gif.pending_count == 0)
    {
      rc = read_raster_code (ref gif, out code, ref stream);        /* read next code */
      if (rc < 0)
        return rc;

      if (code == gif.code_clear_table)
      {
        gif.nb_entries = gif.code_end_of_information + 1;

        /* reset code size to initial value */
        gif.code_size = 1 + (uint)gif.initial_code_size;

        /* reset first_time flag */
        gif.first_time = true;
      }
      else if (code == gif.code_end_of_information)
      {
        return IMG_END_OF_IMAGE;
      }
      else
      {
        if (code < gif.nb_entries)    /* code exists in table */
        {
          rc = write_string (ref gif, gif.table^[code]);
          if (rc < 0)
            return rc;

          if ((!gif.first_time) && (gif.nb_entries < 4096))
          {
            ref STRING p = gif.table^[gif.nb_entries++];

            p.first     = gif.table^[gif.old_code].first;
            p.prefix    = gif.old_code;
            p.character = gif.table^[code].first;
          }

          gif.first_time = false;
        }
        else if (code == gif.nb_entries)    /* new entry in table */
        {
          ref STRING p = gif.table^[gif.nb_entries++];

          p.first     = gif.table^[gif.old_code].first;
          p.prefix    = gif.old_code;
          p.character = gif.table^[gif.old_code].first;

          rc = write_string (ref gif, p);
          if (rc < 0)
            return rc;
        }
        else    /* illegal code */
        {
          return GIF_CORRUPTED_FILE;
        }

        gif.old_code = code;

        /* we must extend the string table with a new code */
        if (gif.nb_entries == (uint)(1 << gif.code_size))
        {
          if (gif.code_size < 12)  /* we must increment the code_size */
            gif.code_size++;
        }
      }
    }
  }
}

/**************************************************************************/

/* returns 0 if next image follows, +1 if no image follows, -1 if error */

public int next_gif (ref GIF gif, ref READ_STREAM stream)
{
  uint  code;
  int   rc;

  /* swallow rest of codes */
  for (;;)
  {
    rc = read_raster_code (ref gif, out code, ref stream);
    if (rc != 0)
      break;
  }

  _unused code;

  end_decompressor (ref gif);

  if (rc != GIF_END_OF_RASTER_DATA)
    return rc;

  rc = handle_image_sequence (ref gif, ref stream);
  if (rc < 0)
  {
    if (rc == GIF_TRAILER)
      return +1;    /* no more images */
    return rc;
  }

  rc = init_decompressor (ref gif, ref stream);
  if (rc < 0)
    return rc;

  return 0;
}

/**************************************************************************/

public int open_gif (out GIF gif, ref READ_STREAM stream)
{
  int rc;

  clear gif;

  /* read the header */
  rc = gif_signature (ref stream);
  if (rc < 0)
  {
    close_gif (ref gif);
    return rc;
  }

  rc = gif_screen_descriptor (ref gif, ref stream);
  if (rc < 0)
  {
    close_gif (ref gif);
    return rc;
  }

  rc = handle_image_sequence (ref gif, ref stream);
  if (rc < 0)
  {
    close_gif (ref gif);
    return rc;
  }

  rc = init_decompressor (ref gif, ref stream);
  if (rc < 0)
  {
    close_gif (ref gif);
    return rc;
  }

  return 0;
}

/********************************************************************/
