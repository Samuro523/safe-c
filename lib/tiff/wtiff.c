
/* wtiff.c : TIFF image writer (format TIFF G4 FAX, black/white images) */

use ../stream;
use ../image;   // for error codes

use runcoding;

/*************************************************************************/

struct TIFF
{
  short   status;            /* usually -1, set to 0 if the file is complete */

  uint2   image_x_size;      /* 1 to 65535                  */
  uint2   image_y_size;      /* 1 to 65535                  */

  uint4   size;              /* nb pixels written so far    */
  uint4   max_size;          /* image_x_size * image_y_size */

  uint2   x;                 /* x-coord. of current pixel   */
  byte    col;               /* current color (0 or 1)      */

  uint2[]^ run1;             /* x-offset table of changing pixels */
  uint2[]^ run0;             /* same for previous line      */
  uint2    run_index;        /* nb entries in table run[]   */

  byte     mask;             /* 128, 64, ..., 1 (next bit to write) */
  byte     out_buffer[4096];
  uint2    out_index;        /* current index within output buffer */

  uint     x_resolution;     /* resolution in dots per inch (DPI) */
  uint     y_resolution;
}

/*************************************************************************/

packed struct ENTRY
{
  uint2 tag;
  uint2 type;
  uint4 length;
  uint4 value;
}

const uint2 MAX_ENTRIES  = 12;

packed struct TAG_TABLE
{
  uint2 nb_entries;
  ENTRY entry[MAX_ENTRIES];
}

const uint2 TYPE_WORD     = 3;
const uint2 TYPE_LONG     = 4;
const uint2 TYPE_RATIONAL = 5;

/*************************************************************************/

int write_char (byte c, ref WRITE_STREAM stream)
{
  if (write (ref stream, c) != 1)
    return TIFF_WRITE_ERROR;
  return 0;
}

/*************************************************************************/

int write_word (uint2 w, ref WRITE_STREAM stream)
{
  int rc;
  rc = write_char ((byte)w, ref stream);
  if (rc < 0)
    return rc;
  return write_char ((byte)(w >> 8), ref stream);
}

/*************************************************************************/

int write_long (uint4 l, ref WRITE_STREAM stream)
{
  int   rc, i;
  uint4 u = l;
  for (i=0; i<4; i++)
  {
    rc = write_char ((byte)u, ref stream);
    if (rc < 0)
      return rc;
    u >>= 8;
  }
  return 0;
}

/*************************************************************************/

int write_tag_table (TAG_TABLE table, ref WRITE_STREAM stream)
{
  int rc, i;

  rc = write_word (table.nb_entries, ref stream);
  if (rc < 0)
    return rc;

  for (i=0; i<(int)table.nb_entries; i++)
  {
    rc = write_word (table.entry[i].tag, ref stream);
    if (rc < 0)
      return rc;

    rc = write_word (table.entry[i].type, ref stream);
    if (rc < 0)
      return rc;

    rc = write_long (table.entry[i].length, ref stream);
    if (rc < 0)
      return rc;

    if (table.entry[i].type == TYPE_WORD)
    {
      rc = write_word ((uint2)table.entry[i].value, ref stream);
      if (rc < 0)
        return rc;
      rc = write_word (0, ref stream);
      if (rc < 0)
        return rc;
    }
    else if (table.entry[i].type == TYPE_LONG ||
             table.entry[i].type == TYPE_RATIONAL)
    {
      rc = write_long (table.entry[i].value, ref stream);
      if (rc < 0)
        return rc;
    }
  }

  return write_long (0, ref stream);
}

/*************************************************************************/

int write_string (ref TIFF t, string s, ref WRITE_STREAM stream)
{
  int i = 0;
  while (i < s'length && s[i] != nul)
  {
    if (s[i] == '1')
      t.out_buffer[t.out_index] |= t.mask;

    if (t.mask > 1)
      t.mask >>= 1;
    else
    {
      t.mask = 128;
      t.out_index++;
      if (t.out_index == t.out_buffer'size)  /* output buffer full */
      {
        if (write (ref stream, t.out_buffer[0:t.out_index]) != (int)t.out_index)
          return TIFF_WRITE_ERROR;

        clear t.out_buffer;
        t.out_index = 0;
      }
    }

    i++;
  }

  return 0;
}

/*************************************************************************/

int flush_output_buffer (ref TIFF t, ref WRITE_STREAM stream)
{
  if (t.mask != 128)   /* some bits of this byte already written */
    t.out_index++;

  if (write (ref stream, t.out_buffer[0:t.out_index]) != (int)t.out_index)
    return TIFF_WRITE_ERROR;

  return 0;
}

/*************************************************************************/

int write_white_length (ref TIFF t, int length, ref WRITE_STREAM stream)
{
  int rc;
  int len = length;

  while (len > 2560)
  {
    rc = write_string (ref t, white_run2[39], ref stream);
    if (rc < 0)
      return rc;
    len -= 2560;
  }

  if (len >= 64)
  {
    rc = write_string (ref t, white_run2[(len>>6) - 1], ref stream);
    if (rc < 0)
      return rc;
    len &= 63;
  }

  return write_string (ref t, white_run[len], ref stream);
}

/*************************************************************************/

int write_black_length (ref TIFF t, int length, ref WRITE_STREAM stream)
{
  int rc;
  int len = length;

  while (len > 2560)
  {
    rc = write_string (ref t, black_run2[39], ref stream);
    if (rc < 0)
      return rc;
    len -= 2560;
  }

  if (len >= 64)
  {
    rc = write_string (ref t, black_run2[(len>>6) - 1], ref stream);
    if (rc < 0)
      return rc;
    len &= 63;
  }

  return write_string (ref t, black_run[len], ref stream);
}

/*************************************************************************/

public int create_tiff (out TIFF tiff,
                        ref WRITE_STREAM stream,
                        uint2    width,
                        uint2    height,
                        uint     x_resolution,
                        uint     y_resolution)
{
  const byte tiff_header[8] = {0x49, 0x49, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00};
  ref TIFF t = tiff;

  clear t;

  if (width < 1)
    return TIFF_ILLEGAL_WIDTH;

  if (height < 1)
    return TIFF_ILLEGAL_HEIGHT;

  t.image_x_size = width;
  t.image_y_size = height;

  t.x_resolution = x_resolution;
  t.y_resolution = y_resolution;

  t.max_size = (uint)width * (uint)height;

  t.status = -1;      /* default : file is incomplete */

  t.run1 = new uint2 [width+3];
  t.run0 = new uint2 [width+3];

  /* init run0[] with 3 sentinel codes */
  t.run0^[0] = width;
  t.run0^[1] = width;
  t.run0^[2] = width;

  t.mask = 128;

  /* write the header */
  if (write (ref stream, tiff_header) != tiff_header'length)
  {
    close_tiff2 (ref tiff, ref stream);
    return TIFF_WRITE_ERROR;
  }

  return 0;
}

/*************************************************************************/

public int close_tiff2 (ref TIFF tiff, ref WRITE_STREAM stream)
{
  ref TIFF t = tiff;
  int      retcode, i;
  long     x_offset_resolution = 0, y_offset_resolution = 0;

  if (t.size == t.max_size)    /* image is complete */
  {
    int       rc;
    long      table_ofs;
    TAG_TABLE table;

    rc = write_string (ref t, "000000000001000000000001", ref stream);    /* EOFB */
    if (rc < 0)
      return rc;

    rc = flush_output_buffer (ref t, ref stream);
    if (rc < 0)
      return rc;


    /* write RATIONAL (x_resolution,1) */

    if (t.x_resolution > 0)
    {
      x_offset_resolution = lseekw (ref stream, 0L, SEEK_CUR);  /* save current file position */
      if (x_offset_resolution < 0)
        return TIFF_SEEK_ERROR;

      rc = write_long (t.x_resolution, ref stream);
      if (rc < 0)
        return rc;

      rc = write_long (1, ref stream);
      if (rc < 0)
        return rc;
    }


    /* write RATIONAL (y_resolution,1) */

    if (t.y_resolution > 0)
    {
      y_offset_resolution = lseekw (ref stream, 0L, SEEK_CUR);  /* save current file position */
      if (y_offset_resolution < 0)
        return TIFF_SEEK_ERROR;

      rc = write_long (t.y_resolution, ref stream);
      if (rc < 0)
        return rc;

      rc = write_long (1, ref stream);
      if (rc < 0)
        return rc;
    }


    /* write table */

    table_ofs = lseekw (ref stream, 0L, SEEK_CUR);  /* save current file position */
    if (table_ofs < 0)
      return TIFF_SEEK_ERROR;

    clear table;
    i = 0;

    {
      ref ENTRY e = table.entry[i];
      e.tag    = 0x100;     /* width */
      e.type   = TYPE_WORD;
      e.length = 1;
      e.value  = t.image_x_size;
      i++;
    }

    {
      ref ENTRY e = table.entry[i];
      e.tag    = 0x101;     /* height */
      e.type   = TYPE_WORD;
      e.length = 1;
      e.value  = t.image_y_size;
      i++;
    }

    {
      ref ENTRY e = table.entry[i];
      e.tag    = 0x103;     /* compression */
      e.type   = TYPE_WORD;
      e.length = 1;
      e.value  = 4;         /* TIFF G4 FAX */
      i++;
    }

    {
      ref ENTRY e = table.entry[i];
      e.tag    = 0x106;     /* photometric interpretation */
      e.type   = TYPE_WORD;
      e.length = 1;
      e.value  = 0;
      i++;
    }

    {
      ref ENTRY e = table.entry[i];
      e.tag    = 0x10A;     /* fill order */
      e.type   = TYPE_WORD;
      e.length = 1;
      e.value  = 1;         /* 1 = high order bit first */
      i++;
    }

    {
      ref ENTRY e = table.entry[i];
      e.tag    = 0x111;     /* strip offset */
      e.type   = TYPE_LONG;
      e.length = 1;
      e.value  = 8;         /* image data is just after 8-byte header */
      i++;
    }

    {
      ref ENTRY e = table.entry[i];
      e.tag    = 0x116;     /* rows per strip */
      e.type   = TYPE_WORD;
      e.length = 1;
      e.value  = t.image_y_size;  /* image height (single strip) */
      i++;
    }

    {
      ref ENTRY e = table.entry[i];
      e.tag    = 0x117;     /* strip byte count */
      e.type   = TYPE_LONG;
      e.length = 1;
      e.value  = (uint)(table_ofs - 8);  /* size of image data */
      i++;
    }

    if (t.x_resolution > 0)
    {
      ref ENTRY e = table.entry[i];
      e.tag    = 0x11A;     /* X-Resolution */
      e.type   = TYPE_RATIONAL;
      e.length = 1;
      e.value  = (uint)x_offset_resolution;
      i++;
    }

    if (t.y_resolution > 0)
    {
      ref ENTRY e = table.entry[i];
      e.tag    = 0x11B;     /* Y-Resolution */
      e.type   = TYPE_RATIONAL;
      e.length = 1;
      e.value  = (uint)y_offset_resolution;
      i++;
    }

    table.nb_entries = (uint2)i;

    if (table.nb_entries > MAX_ENTRIES)
      return TIFF_INTERN_ERROR;

    rc = write_tag_table (table, ref stream);
    if (rc < 0)
      return rc;

    /* complete header */

    if (lseekw (ref stream, 4L, SEEK_SET) != 4)
      return TIFF_SEEK_ERROR;

    if (write_long ((uint)table_ofs, ref stream) < 0)
      return TIFF_WRITE_ERROR;

    /* set file status to OK */
    t.status = 0;
  }

  if (t.status < 0)
    retcode = TIFF_INCOMPLETE;
  else
    retcode = 0;

  free t.run1;
  free t.run0;

  clear t;

  return retcode;
}

/*************************************************************************/

public void get_tiff_size2 (TIFF     tiff,
                            out uint width,
                            out uint height)
{
  width  = tiff.image_x_size;
  height = tiff.image_y_size;
}

/**************************************************************************/

public int write_tiff (ref TIFF tiff, byte[] buffer, ref WRITE_STREAM stream)
{
  ref TIFF t = tiff;
  uint     nb_pixels, i;
  int      col, j;
  uint2[]^ temp;

  if ((buffer'size & 3) != 0)
    return IMG_ILLEGAL_BUFFER_SIZE;    /* size is not multiple of 4 */

  nb_pixels = (buffer'size >> 2);

  if (nb_pixels > t.max_size - t.size)
    return TIFF_TOO_MUCH_DATA;

  t.size += nb_pixels;

  j = 0;   // index into buffer

  for (i=0; i<nb_pixels; i++)
  {
    ref byte[4] pixel = buffer[j:4];

    /* convert RGB to (0 = white, 1 = black)   (R*32 + G*64 + B*16) */
    col = (int)(((((((pixel[1]) << 1) + pixel[0]) << 1) + pixel[2]) << 4)
                < 14280);

    if (col != (int)t.col)     /* black <. white color change */
    {
      t.run1^[t.run_index++] = t.x;   /* store x-offset in run1[] */
                                      /* first entry = first black pixel */
      t.col = (byte)col;              /* new color */
    }

    t.x++;
    if (t.x == t.image_x_size)  /* line is complete */
    {
      int rc, x0, a1, b1;

      /* append 3 sentinel codes */
      t.run1^[t.run_index++] = t.image_x_size;
      t.run1^[t.run_index++] = t.image_x_size;
      t.run1^[t.run_index++] = t.image_x_size;

      /* encode run1[] using CCITT T6 standard */

      x0 = -1;
      a1 = 0;   /* index into run1[]  */
      b1 = 0;   /* index into run0[] */

      while (x0 < (int)t.image_x_size)
      {
        /* compute a1 : next changing element larger than x0 */
        while ((int)t.run1^[a1] <= x0)
          a1++;

        /* compute b1 : next changing element larger than x0 on run0[] */
        /*              of same color than a1                          */
        if (b1 > 0)   /* go 1 index backwards to avoid overflow */
          b1--;
        if (b1 > 0)   /* go 1 index backwards to avoid overflow */
          b1--;
        while ((int)t.run0^[b1] <= x0)
          b1++;
        if (((a1 ^ b1) & 1) != 0)   /* a1 and b1 have different color */
          b1++;

        if (t.run0^[b1+1] < t.run1^[a1])   /* use PASS MODE */
        {
          rc = write_string (ref t, "0001", ref stream);    /* PASS */
          if (rc < 0)
            return rc;

          x0 = t.run0^[b1+1];
        }
        else if ((int)t.run1^[a1] - (int)t.run0^[b1] >= -3 &&
                 (int)t.run1^[a1] - (int)t.run0^[b1] <= +3)    /* VERTICAL */
        {
          const string code[7] =
             {"0000010", "000010", "010", "1", "011", "000011", "0000011"};

          rc = write_string (ref t, code[3 + (int)t.run1^[a1] - (int)t.run0^[b1]], ref stream);
          if (rc < 0)
            return rc;

          x0 = t.run1^[a1];
        }
        else   /* HORIZONTAL */
        {
          rc = write_string (ref t, "001", ref stream);    /* horizontal code */
          if (rc < 0)
            return rc;

          if ((a1 & 1) != 0)    /* we must code a black then a white run */
          {
            rc = write_black_length (ref t, (uint2)(t.run1^[a1] - (uint2)x0), ref stream);
            if (rc < 0)
              return rc;
            x0 = t.run1^[a1];
            a1++;

            rc = write_white_length (ref t, (uint2)(t.run1^[a1] - (uint2)x0), ref stream);
            if (rc < 0)
              return rc;
            x0 = t.run1^[a1];
            a1++;
          }
          else     /* we must code a white then a black run */
          {
            if (x0 < 0)
              x0 = 0;

            rc = write_white_length (ref t, (uint2)(t.run1^[a1] - (uint2)x0), ref stream);
            if (rc < 0)
              return rc;
            x0 = t.run1^[a1];
            a1++;

            rc = write_black_length (ref t, (uint2)(t.run1^[a1] - (uint2)x0), ref stream);
            if (rc < 0)
              return rc;
            x0 = t.run1^[a1];
            a1++;
          }
        }
      }


      /* re-initialize for next line */

      t.x         = 0;
      t.col       = 0;      /* current color : white */
      t.run_index = 0;

      temp   = t.run1;      /* exchange run1[] and run0[] */
      t.run1 = t.run0;
      t.run0 = temp;
    }

    j += 4;
  }

  return 0;
}

/*************************************************************************/
