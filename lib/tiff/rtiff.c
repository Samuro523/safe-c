
/*****************************/
/* tif.c : TIFF image reader */
/*****************************/

/*
 - supports:
   . G2/G3/G4-FAX format
   . black/white

 - does not support :
   . uncompressed, LZW and PackBits compression
   . grey, color.
*/

/* not tested : swap bytes (try on unix) */

/**************************************************************************/

use ../stream;
use ../image, fsm1, fsm2;

/**************************************************************************/

union VALUE
{
  uint4 l;
  uint2 w[2];
}

packed struct ENTRY
{
  uint2 tag;
  uint2 typ;
  uint4 len;
  VALUE value;
}

/**************************************************************************/

struct TIFF
{
  bool     swap_bytes;            /* true = swap all uint2 and uint bytes */

  uint2    nb_entries;
  ENTRY[]^ entry;                 /* current ifd */

  uint4    width;                 /* in pixels */
  uint4    height;                /* in pixels */
  uint2    samples_per_pixel;     /* 1 for BW/grey/palette or 3 for RGB */
  uint2    bits_per_sample[3];    /* all 3 values in range 1 .. 8 */

  uint2    compression;           /* 1=none, 2=FAX-G3, 3=FAX-G3, 4=FAX-G4, 5=LZW, 32773=PackBits */
  uint4    fax_g3_options;        /* 32 bits (must = 0) */
  uint4    fax_g4_options;        /* 32 bits (must = 0) */
  uint2    lzw_options;           /* LZW predictor (must = 1) */
  uint2    fill_order;            /* 1 = high to low bit, 2 = low to high bit */

  uint2    photometric;           /* 0 : (1=black,0=white), 1 : reverse */
                                  /* (not used for color images)        */

  uint2    planar_configuration;  /* 1 : 1 plane "RGBRGBRGB"...          */
                                  /* 2 : 1 plane R, 1 plane G, 1 plane B */
                                  /* only used if samples_per_pixel != 1 */

  uint4    max_lines_per_strip;   /* max lines contained in a strip */
  uint2    nb_strips;
  uint[]^  strip_offset;
  uint[]^  strip_count;

  uint2[]^ color_map;
  uint2    nb_entries_in_map;    /* only if color_map non-NULL */

  /* for reading operations: */
  uint4   pending_count;         /* nb of pixels left to read from last call */
  byte    pending_pixel[4];      /* value of RGBS pixel left to read */

  /* for FAX-G3/G4 decompression: */
  bool    end_of_line;           /* true = end of line reached */
  uint2   strip_index;           /* index of current strip in buffer */
  byte[]^ buffer;                /* decompression buffer */
  uint2   buffer_size;           /* max allocated buffer size */
  uint2   buffer_len;            /* nb of active bytes in buffer */
  uint2   buffer_index;          /* index of current byte being read */
  byte    buffer_byte;           /* value of current byte */
  byte    buffer_bit;            /* current bit mask (128,64,32,16,8,4,2,1) */
  uint4   strip_line_index;      /* nb of lines already read for this strip */
  uint4   strip_line_count;      /* max lines to read for this strip */
  uint4   line_pixel_index;      /* nb of pixels already read for this line */
  uint2   fsm_state;             /* current state of finite state machine */

  /* additional fields for FAX-G4 decompression: */
  uint2    previous_index;       /* used for searching the next b1 */
  uint2[]^ previous;             /* offset sequence of changing elements */
                                 /* (bit 15 indicates the color transition) */
  uint2    current_count;        /* nb of active uint2s in current[] */
  uint2[]^ current;              /* offset sequence of changing elements */

  uint2    fsm2_state;           /* current state of finite state machine 2 */
  uint2    fsm2_op;              /* current operation of fsm2 (or 0 if none) */
  uint2    g4_color;             /* current color (0 or 1) */
  int      g4_a0;                /* see algorithm (this type is signed) */

  uint4    x_resolution;         /* in dot per inch */
  uint4    y_resolution;         /* in dot per inch */
}

/**************************************************************************/

const byte TYP_BYTE             =  1;
const byte TYP_ASCII            =  2;
const byte TYPE_WORD            =  3;
const byte TYPE_LONG            =  4;
const byte TYP_RATIONAL         =  5;
const byte TYPE_SIGNED_BYTE     =  6;
const byte TYPE_UNDEFINED       =  7;
const byte TYPE_SHORT           =  8;
const byte TYPE_SIGNED_uint4    =  9;
const byte TYPE_SIGNED_RATIONAL = 10;
const byte TYPE_FLOAT           = 11;
const byte TYPE_DOUBLE          = 12;

const byte type_size[13] =
  { 0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8 };

/**************************************************************************/

void SWAP_uint2 (TIFF t, ref uint2 w)
{
  if (t.swap_bytes)
    w = (uint2)((w >> 8) | (w << 8));
}

/**************************************************************************/

void SWAP_uint4 (TIFF t, ref uint4 l)
{
  byte b1, b2, b3, b4;

  if (t.swap_bytes)
  {
    b1 = (byte)(l >> 24);
    b2 = (byte)(l >> 16);
    b3 = (byte)(l >> 8);
    b4 = (byte)(l);
    l = b1 + (b2 << 8) + (b3 << 16) + (b4 << 24);
  }
}

/**************************************************************************/

/* when returning 0, the current file position denotes the first IFD */

int read_header (ref TIFF t, ref READ_STREAM stream)
{
  packed struct HEADER
  {
    uint2 order;
    uint2 magic;
    uint4 ifd;
  }

  HEADER header;

  union U
  {
    uint2 w;
    byte  b[2];
  }
  U u;

  if (read (ref stream, out header) != (int)header'size)
    return IMG_FILE_ERROR;

  clear u;
  u.w = 0x0201;
  if (header.order == 0x4949)       /* file has INTEL order */
    t.swap_bytes = (u.b[0] == 2);
  else if (header.order == 0x4D4D)  /* file has MOTOROLA order */
    t.swap_bytes = (u.b[0] == 1);
  else
    return IMG_NOT_RECOGNIZED;

  SWAP_uint2 (t, ref header.magic);
  SWAP_uint4 (t, ref header.ifd);

  if (header.magic != 0x002A)
    return IMG_NOT_RECOGNIZED;

  if (lseekr (ref stream, header.ifd, SEEK_SET) < 0)
    return IMG_FILE_ERROR;

  return 0;
}

/**************************************************************************/

/* on OK return, the function has allocated memory for t.entry */

int read_ifd (ref TIFF t, ref READ_STREAM stream)
{
  /* read nb of entries in IFD */

  if (read (ref stream, out t.nb_entries) != 2)
    return IMG_FILE_ERROR;

  SWAP_uint2 (t, ref t.nb_entries);

  if (t.nb_entries > 8192 / 12)
    return IMG_NOT_RECOGNIZED;


  /* allocate and read entry table */

  t.entry = new ENTRY [t.nb_entries];

  if (read (ref stream, out t.entry^) != (int)t.entry^'size)
  {
    free  t.entry;
    return IMG_FILE_ERROR;
  }

  return 0;
}

/**************************************************************************/

void set_default_options (ref TIFF t)
{
/* t.width                = 0; */
/* t.height               = 0; */
   t.samples_per_pixel    = 1;
   t.bits_per_sample[0]   = 1;
   t.bits_per_sample[1]   = 1;
   t.bits_per_sample[2]   = 1;
   t.compression          = 1;
/* t.fax_g3_options       = 0; */
/* t.fax_g4_options       = 0; */
   t.lzw_options          = 1;
   t.fill_order           = 1;
   t.photometric          = 1;
   t.planar_configuration = 1;
   t.max_lines_per_strip  = 0xffffffff;
/* t.nb_strips            = 0; */
/* t.strip_offset         = null;  */
/* t.strip_count          = null;  */
/* t.color_map            = null;  */
/* t.nb_entries_in_map    = 0; */
/* t.end_of_line          = 0; */
/* t.strip_index          = 0; */
/* t.buffer               = null; */
/* t.buffer_size          = 0; */
/* t.buffer_len           = 0; */
/* t.buffer_index         = 0; */
/* t.buffer_byte          = 0; */
/* t.buffer_bit           = 0; */
/* t.strip_line_index     = 0; */
/* t.strip_line_count     = 0; */
/* t.line_pixel_index     = 0; */
/* t.fsm_state            = 0; */
/* t.pending_action       = 0; */
/* t.pending_bytes        = 0; */
/* t.previous_index       = 0;    */
/* t.previous             = null; */
/* t.current_count        = 0;    */
/* t.current              = null; */
/* t.fsm2_state           = 0;    */
/* t.g4_color             = 0;    */
/* t.g4_a0                = 0;    */
}

/**************************************************************************/

int analyze_ifd (ref TIFF t, ref READ_STREAM stream)
{
  uint2 i;
  uint4 value_size;
  byte  typ;

  for (i=0; i<t.nb_entries; i++)
  {
    ref ENTRY e = t.entry^[i];

    SWAP_uint2 (t, ref e.tag);
    SWAP_uint2 (t, ref e.typ);

    if (e.typ < 1 || e.typ > 12)   /* unknown type : ignore TAG */
      continue;

    typ = (byte)e.typ;

    SWAP_uint4 (t, ref e.len);

    value_size = type_size[typ] * e.len;

    if (value_size > 4 ||                   /* it is a pointer,        */
        (value_size == 4 && e.len == 1))    /* or a single 4-byte type */
    {
      SWAP_uint4 (t, ref e.value.l);
    }
    else if (type_size[typ] == 2)   /* 1 or 2 x 2-byte type */
    {
      SWAP_uint2 (t, ref e.value.w[0]);
      SWAP_uint2 (t, ref e.value.w[1]);
    }

    switch (e.tag)
    {
      case 0x100:   /* image Width */
        if (typ == TYPE_LONG)
          t.width = e.value.l;
        else if (typ == TYPE_WORD)
          t.width = e.value.w[0];
        break;

      case 0x101:   /* image Height */
        if (typ == TYPE_LONG)
          t.height = e.value.l;
        else if (typ == TYPE_WORD)
          t.height = e.value.w[0];
        break;

      case 0x102:   /* bits per sample */
        if (typ != TYPE_WORD)
          return UNSUPPORTED_OPTION_1;

        if (e.len == 1)             /* 1 sample per pixel */
          t.bits_per_sample[0] = e.value.w[0];
        else if (e.len == 3)        /* 3 samples per pixel (RGB) */
        {
          /* read the word table in the file */
          if (lseekr (ref stream, e.value.l, SEEK_SET) < 0)
            return IMG_FILE_ERROR;

          if (read (ref stream, out t.bits_per_sample) != (int)t.bits_per_sample'size)
            return IMG_FILE_ERROR;

          SWAP_uint2 (t, ref t.bits_per_sample[0]);
          SWAP_uint2 (t, ref t.bits_per_sample[1]);
          SWAP_uint2 (t, ref t.bits_per_sample[2]);
        }
        else
        {
          return UNSUPPORTED_OPTION_2;
        }
        break;

      case 0x103:   /* compression */
        if (typ != TYPE_WORD)
          return UNSUPPORTED_OPTION_3;
        t.compression = e.value.w[0];
        break;

      case 0x106:   /* photometric interpretation */
        if (typ != TYPE_WORD)
          return UNSUPPORTED_OPTION_4;
        t.photometric = e.value.w[0];
        break;

      case 0x10A:   /* fill order */
        if (typ != TYPE_WORD)
          return UNSUPPORTED_OPTION_5;
        t.fill_order = e.value.w[0];
        break;

      case 0x111:   /* strip offsets */
        if (typ != TYPE_WORD && typ != TYPE_LONG)
          return UNSUPPORTED_OPTION_6;

        if (t.strip_offset != null)
          return UNSUPPORTED_OPTION_7;      /* already allocated */

        if (e.len >= 16*1024)     /* more than 16383 strips */
          return UNSUPPORTED_OPTION_8;

        t.nb_strips = (uint2)e.len;
        t.strip_offset = new uint [t.nb_strips];

        if (value_size <= 4)    /* immediate value in e.value */
        {
          if (typ == TYPE_LONG)
            t.strip_offset^[0] = e.value.l;
          else                              /* 1 or 2 uint2's */
          {
            t.strip_offset^[0] = e.value.w[0];
            if (t.nb_strips == 2)
              t.strip_offset^[1] = e.value.w[1];
          }
        }
        else    /* pointer to strip offset table */
        {
          /* read the strip offset table */
          if (lseekr (ref stream, e.value.l, SEEK_SET) < 0)
            return IMG_FILE_ERROR;

          if (typ == TYPE_WORD)
          {
            uint2 val, ofs;

            /* load & convert all entries to uint4 */
            for (ofs=0; ofs<t.nb_strips; ofs++)
            {
              if (read (ref stream, out val) != 2)
                return IMG_FILE_ERROR;

              SWAP_uint2 (t, ref val);
            
              t.strip_offset^[ofs] = val;
            }
          }
          else   /* TYPE_LONG */
          {
            uint2 ofs;

            if (read (ref stream, out t.strip_offset^) != (int)t.strip_offset^'size)
              return IMG_FILE_ERROR;

            /* convert all entries */
            for (ofs=0; ofs<t.nb_strips; ofs++)
              SWAP_uint4 (t, ref t.strip_offset^[ofs]);
          }
        }
        break;

      case 0x115:   /* samples per pixel */
        if (typ != TYPE_WORD)
          return UNSUPPORTED_OPTION_9;

        t.samples_per_pixel = e.value.w[0];
        if (t.samples_per_pixel != 1 && t.samples_per_pixel != 3)
          return UNSUPPORTED_OPTION_10;
        break;

      case 0x116:   /* rows per strip */
        if (typ != TYPE_WORD && typ != TYPE_LONG)
          return UNSUPPORTED_OPTION_11;

        if (typ == TYPE_WORD)
          t.max_lines_per_strip = e.value.w[0];
        else
          t.max_lines_per_strip = e.value.l;
        break;

      case 0x117:   /* strip byte count */
        if (typ != TYPE_WORD && typ != TYPE_LONG)
          return UNSUPPORTED_OPTION_12;

        if (t.strip_count != null)
          return UNSUPPORTED_OPTION_13;       /* already allocated */

        if (e.len != (uint4)t.nb_strips)  /* mismatch */
          return UNSUPPORTED_OPTION_14;

        t.strip_count = new uint [t.nb_strips];

        if (value_size <= 4)    /* immediate value in e.value */
        {
          if (typ == TYPE_LONG)
            t.strip_count^[0] = e.value.l;
          else                              /* 1 or 2 uint2s */
          {
            t.strip_count^[0] = e.value.w[0];
            if (t.nb_strips == 2)
              t.strip_count^[1] = e.value.w[1];
          }
        }
        else    /* pointer to strip offset table */
        {
          /* read the strip offset table */
          if (lseekr (ref stream, e.value.l, SEEK_SET) < 0)
            return IMG_FILE_ERROR;

          if (typ == TYPE_WORD)
          {
            uint2 val, ofs;

            /* load & convert all entries to uint4 */
            for (ofs=0; ofs<t.nb_strips; ofs++)
            {
              if (read (ref stream, out val) != 2)
                return IMG_FILE_ERROR;

              SWAP_uint2 (t, ref val);
            
              t.strip_count^[ofs] = val;
            }
          }
          else   /* TYPE_LONG */
          {
            uint2 ofs;

            if (read (ref stream, out t.strip_count^) != (int)t.strip_count^'size)
              return IMG_FILE_ERROR;

            /* convert all entries */
            for (ofs=0; ofs<t.nb_strips; ofs++)
              SWAP_uint4 (t, ref t.strip_count^[ofs]);
          }
        }
        break;

      case 0x11A:   /* X-resolution */
        if (typ == TYP_RATIONAL && e.len == 1)
        {
          uint den;

          if (lseekr (ref stream, e.value.l, SEEK_SET) < 0)
            return IMG_FILE_ERROR;

          if (read (ref stream, out t.x_resolution) != 4)
            return IMG_FILE_ERROR;

          SWAP_uint4 (t, ref t.x_resolution);

          if (read (ref stream, out den) != 4)
            return IMG_FILE_ERROR;

          SWAP_uint4 (t, ref den);

          if (den > 1)
            t.x_resolution /= den;
        }
        break;

      case 0x11B:   /* Y-resolution */
        if (typ == TYP_RATIONAL && e.len == 1)
        {
          uint den;

          if (lseekr (ref stream, e.value.l, SEEK_SET) < 0)
            return IMG_FILE_ERROR;

          if (read (ref stream, out t.y_resolution) != 4)
            return IMG_FILE_ERROR;

          SWAP_uint4 (t, ref t.y_resolution);

          if (read (ref stream, out den) != 4)
            return IMG_FILE_ERROR;

          SWAP_uint4 (t, ref den);

          if (den > 1)
            t.y_resolution /= den;
        }
        break;

      case 0x11C:   /* planar configuration */
        if (typ != TYPE_WORD)
          return UNSUPPORTED_OPTION_15;
        t.planar_configuration = e.value.w[0];
        break;

      case 0x124:   /* group 3 options */
        if (typ != TYPE_LONG)
          return UNSUPPORTED_OPTION_16;
        t.fax_g3_options = e.value.l;
        break;

      case 0x125:   /* group 4 options */
        if (typ != TYPE_LONG)
          return UNSUPPORTED_OPTION_17;
        t.fax_g4_options = e.value.l;
        break;

      case 0x13D:   /* predictor (LZW options) */
        if (typ != TYPE_WORD)
          return UNSUPPORTED_OPTION_18;
        t.lzw_options = e.value.w[0];
        break;

      case 0x140:   /* color map */
        if (typ != TYPE_WORD)
          return UNSUPPORTED_OPTION_19;

        if (t.color_map != null)
          return UNSUPPORTED_OPTION_20;      /* already allocated */

        if ((e.len % 3) != 0 || e.len < 3)
          return UNSUPPORTED_OPTION_21;

        if (e.len >= 16*1024)    /* more than 16384 entries */
          return UNSUPPORTED_OPTION_22;

        t.nb_entries_in_map = (uint2)e.len;

        t.color_map = new uint2 [t.nb_entries_in_map];

        /* read the color table */
        if (lseekr (ref stream, e.value.l, SEEK_SET) < 0)
          return IMG_FILE_ERROR;

        if (read (ref stream, out t.color_map^) != (int)t.color_map^'size)
          return IMG_FILE_ERROR;

        /* convert all entries */
        {
          uint2 idx;
          for (idx=0; idx<t.nb_entries_in_map; idx++)
            SWAP_uint2 (t, ref t.color_map^[idx]);
        }
        break;

      default:
        break;
    }
  }

  return 0;
}

/**************************************************************************/

public int close_tiff (ref TIFF tiff)
{
  free tiff.entry;
  free tiff.color_map;
  free tiff.strip_offset;
  free tiff.strip_count;
  free tiff.previous;
  free tiff.current;
  free tiff.buffer;

  clear tiff;
  return 0;
}

/**************************************************************************/

int init_fax_decompression (ref TIFF t)
{
  uint2 i;
  uint4 size, len;

  t.end_of_line = true;         /* pretend we reached the end of the line */

  /* t.strip_index = 0; */

  /* compute largest strip */
  if (t.strip_count == null)
    size = 8192;                 /* use default buffer size : 8K */
  else
  {
    size = 16;                   /* compute size of largest strip */
    for (i=0; i<t.nb_strips; i++)
    {
      if (t.strip_count^[i] > size)
        size = t.strip_count^[i];
    }

    if (size > 32*1024)   /* max 32K */
      size = 32*1024;
  }

  t.buffer = new byte [size];
  t.buffer_size = (uint2)size;

  /* t.buffer_len   = 0; */
  /* t.buffer_index = 0; */
  /* t.buffer_byte  = 0; */
  /* t.buffer_bit   = 0; */
  /* t.strip_line_index = 0; */
  /* t.strip_line_count = 0; */
  /* t.line_pixel_index = 0; */

  /* t.fsm_state        = 0; */     /* start WHITE */
  /* t.pending_count    = 0; */

  if (t.compression == 4)    /* FAX-G4 compression */
  {
    /* we need to allocate two buffers to store the */
    /* (offsets, color) of changing elements.       */
    /* previous contains the previous image line.   */

    /* t.previous_index = 0; */

    len = (t.width + 3);

    if (len >= 32767)                /* ! bit 15 of previous/current ! */
      return UNSUPPORTED_OPTION_23;  /* ! is used to store the color ! */

    t.previous = new uint2 [len];

    /* t.current_count = 0; */

    t.current = new uint2 [len];

    /* t.g4_color = 0; */
    /* t.g4_a0    = 0; */
  }

  return 0;
}

/**************************************************************************/

public void get_tiff_size (TIFF     tiff,
                           out uint width,
                           out uint height)
{
  width  = tiff.width;
  height = tiff.height;
}

/**************************************************************************/

public int read_tiff (ref TIFF tiff, out byte[] buffer, ref READ_STREAM stream)
{
  ref   TIFF t = tiff;
  uint4 nb_pixels, j;

  clear buffer;

  if ((buffer'size & 3) != 0)
    return IMG_ILLEGAL_BUFFER_SIZE;    /* size is not multiple of 4 */

  nb_pixels = (buffer'size >> 2);  /* nb of pixels to fetch from image. */
                                   /* (this counter decreases til zero) */

  j = 0;   // write index into buffer[]

  if (t.pending_count > 0)   /* pixels left from last call */
  {
    uint4 len, i;

    /* compute nb of pixels to copy */
    if (nb_pixels > t.pending_count)
      len = t.pending_count;
    else
      len = nb_pixels;


    /* write the pixels */

    i = len;
    while (i-- > 0)
    {
      buffer[j:4] = t.pending_pixel;
      j += 4;
    }

    nb_pixels       -= len;
    t.pending_count -= len;
  }


  if (nb_pixels == 0)    /* output buffer is full */
    return 0;


  /* read additional data into output buffer (at most nb_pixels) */

  for (;;)
  {
    byte  bit;
    uint2 op;


    /* check if the output reached a line boundary : */
    /* we must then switch to a new input line.      */

    if (t.end_of_line)         /* we reached the end of the output line */
    {
      /* begin a new line */

      t.end_of_line = false;

//      trace ("start new line %u within strip %u\n", t.strip_line_index, t.strip_index);

      t.line_pixel_index = 0;

      if (t.compression == 2)  /* FAX-G2 new line operations */
      {
        t.buffer_bit = 0;  /* throw away remaining bits of input byte */
        t.fsm_state  = 0;  /* restart with WHITE code */
      }

      if (t.compression == 4)  /* FAX-G4 new line operations */
      {
        uint2[]^ temp;


        /* add 3 terminators on 'current' array */

        t.current^[t.current_count++] = (uint2)(t.width | 0x0000);
        t.current^[t.current_count++] = (uint2)(t.width | 0x8000);
        t.current^[t.current_count++] = (uint2)(t.width | 0x0000);


        /* swap 'current' and 'previous' arrays */

        temp       = t.current;
        t.current  = t.previous;
        t.previous = temp;

        t.previous_index = 0;  /* search index into 'previous' array */
        t.current_count  = 0;  /* start building an empty 'current' array */

        t.fsm2_state = 0;   /* restart finite state machine 2 at start */
        t.fsm2_op    = 0;   /* no operation currently going on */
        t.g4_color   = 0;   /* restart with color WHITE */
        t.g4_a0      = -1;  /* start at imaginary element before first col */
      }

      if (t.strip_line_index == t.strip_line_count) /* strip is exhausted */
      {
        int   rc;
        uint2 len;
        uint4 nb_lines;


        /* load a new strip into buffer */

        if (t.strip_index >= t.nb_strips)   /* no more strips to load */
          return IMG_END_OF_IMAGE;

        if (lseekr (ref stream, t.strip_offset^[t.strip_index], SEEK_SET) < 0)
          return IMG_FILE_ERROR;

        if (t.strip_count != null &&
            t.strip_count^[t.strip_index] <= t.buffer_size)
        {
          len = (uint2)t.strip_count^[t.strip_index];
        }
        else
        {
          len = t.buffer_size;
        }

        rc = read (ref stream, out t.buffer^[0:len]);
        if (rc <= 0)
          return IMG_FILE_ERROR;

        t.buffer_len   = (uint2)rc;
        t.buffer_index = 0;

//        trace ("read strip of %u bytes\n", len);


        /* compute the strip line index and count */

        nb_lines = t.max_lines_per_strip;
        if (nb_lines > t.height)
          nb_lines = t.height;        /* max nb lines per strip */

        if (t.strip_index + 1 == t.nb_strips)   /* last strip */
          nb_lines = t.height - (t.nb_strips - 1) * nb_lines;

        t.strip_line_index = 0;
        t.strip_line_count = nb_lines;

//        trace ("strip has %ld lines\n", t.strip_line_count);

        t.buffer_bit = 0;    /* start with first bit */
        t.fsm_state  = 0;    /* start with WHITE code */

        if (t.compression == 4)  /* FAX-G4 : prepare previous white line */
        {
          t.previous^[0] = (uint2)(t.width | 0x0000);   /* white */
          t.previous^[1] = (uint2)(t.width | 0x8000);   /* black */
          t.previous^[2] = (uint2)(t.width | 0x0000);   /* white */
        }

        t.strip_index++;
      }

      t.strip_line_index++;
    }


    /* make sure 't.buffer_byte' is valid */

    if (t.buffer_bit == 0)    /* the current byte is exhausted ! */
    {
      /* we need to fetch a new compressed byte */

      if (t.buffer_index >= t.buffer_len)   /* buffer is exhausted */
      {
        int rc;

//        trace ("read extended buffer\n");

        /* we need to read an extend */
        rc = read (ref stream, out t.buffer^);
        if (rc <= 0)
          return IMG_FILE_ERROR;

        t.buffer_len = (uint2)rc;
        t.buffer_index = 0;
      }

      t.buffer_byte = t.buffer^[t.buffer_index++];
      if (t.fill_order == 1)
        t.buffer_bit = 128;
      else
        t.buffer_bit = 1;
    }


    /* get a bit from compressed image */

    bit = (byte)((t.buffer_byte & t.buffer_bit) != 0);

    if (t.fill_order == 1)
      t.buffer_bit >>= 1;
    else
      t.buffer_bit <<= 1;

    if (t.compression < 4)    /* G2-FAX and G3-FAX */
    {
      if (bit != 0)
        t.fsm_state = action[t.fsm_state].one;
      else
        t.fsm_state = action[t.fsm_state].zero;

//      trace ("state : %d\n", t.fsm_state);

      op = action[t.fsm_state].operation;
      if (op != 0)
      {
        uint2 count, len, i;
        byte  col, pixels[4];


        /* compute color of pixels */

        if (op == OP_ERROR)
          return TIFF_FORMAT_ERROR;

        if (op == PUT_WHITE)   /* send ones */
          col = 0xFF;
        else                   /* send zeroes */
          col = 0;

        if (t.photometric == 1)
          col = (byte)(0xFF ^ col);

        pixels = {col, col, col, 255};


        /* compute nb of pixels to copy */

        count = action[t.fsm_state].length;

        /* predict later line overflow */
        if (t.line_pixel_index + count > t.width)
          return TIFF_FORMAT_ERROR;

        t.line_pixel_index += count;     /* cannot cause line overflow */

        if (count <= 63)   /* it's a terminating code */
        {
          /* check if this terminates the line */
          if (t.line_pixel_index == t.width)       /* yes */
            t.end_of_line = true;
        }


        /* compute active length to send */

        if (nb_pixels > count)
          len = count;
        else
          len = (uint2)nb_pixels;

//        trace ("put %d bytes color %d\n", len, col);


        /* write the pixels */

        i = len;
        while (i-- > 0)
        {
          buffer[j:4] = pixels;
          j += 4;
        }

        nb_pixels -= len;
        count     -= len;

        if (count > 0)                   /* some pending pixels to send */
        {
          t.pending_count = count;
          t.pending_pixel = pixels;
        }

        if (nb_pixels == 0)    /* output buffer is full */
          return 0;
      }
    }
    else       /* G4-FAX : CCITT 2D algorithm */
    {
      if (t.fsm2_op == 0)     /* no operation currently going on */
      {
        if (bit != 0)
          t.fsm2_state = action2[t.fsm2_state].one;
        else
          t.fsm2_state = action2[t.fsm2_state].zero;

//      trace ("state2 : %d\n", t.fsm2_state);

        op = action2[t.fsm2_state].operation;
        if (op != 0)
        {
          switch (op)
          {
            case OPER_PASS:
//            trace ("Pass Mode.\n");

              {
                uint2 count, len, i, new_value;
                int   i_previous;
                byte  col, pixels[4];

                /* find offset b2 : must be > a0 and have */
                /* a different color than t.g4_color,    */
                /* + take next offset.                    */

                /* 1) advance t.previous_index til (value > a0) */
                for (;;)
                {
                  if ((int)(t.previous^[t.previous_index] & 32767) > t.g4_a0)
                    break;
                  t.previous_index++;
                }

                i_previous = t.previous_index;

                /* if same color, take next one to have different color */
                if ((uint2)((t.previous^[i_previous] & 32768) != 0) == t.g4_color)
                  i_previous++;

//                trace("b1=%d - ", (t.previous^[i_previous] & 32767);

                /* take next one to have same color */
                i_previous++;

                /* store it */
                t.g4_a0 = (int)(t.previous^[i_previous] & 32767);

//                trace("a0=b2=%d\n", (uint2)t.g4_a0);

                /* compute color of pixels */

                if (t.g4_color != 0)   /* black */
                  col = 0;
                else                   /* white */
                  col = 0xFF;

                if (t.photometric == 1)
                  col = (byte)(0xFF ^ col);

                pixels = {col, col, col, 255};


                /* compute nb of pixels to copy */

                count = (uint2)((uint2)t.g4_a0 - (uint2)t.line_pixel_index);

                if (count > 0)  /* non-empty sequence */
                {
                  /* store begin of sequence's offset in current[] */
                  if (t.current_count == 0 &&   /* first time */
                      t.g4_color == 0)          /* WHITE */
                    /* don't store it */;
                  else if (t.current_count > 0 &&  /* previous exists */
                         (byte)((t.current^[t.current_count-1] & 32768) != 0)
                                 == t.g4_color)         /* with same color */
                    /* don't store it */;
                  else
                  {
                    new_value = (uint2)t.line_pixel_index;

//                    trace ("TAB %d (g4_color=%d)\n", new_value, t.g4_color);

                    if (t.g4_color != 0)
                      new_value += 0x8000;
                    t.current^[t.current_count++] = new_value;
                  }
                }

                /* predict later line overflow */
                if (t.line_pixel_index + count > t.width)
                {
//                  trace ("line overflow in PASS\n");
                  return TIFF_FORMAT_ERROR;
                }

                t.line_pixel_index += count; /* cannot cause line overflow */

                /* check if this terminates the line */
                if (t.line_pixel_index == t.width)       /* yes */
                  t.end_of_line = true;


                /* compute active length to send */

                if (nb_pixels > count)
                  len = count;
                else
                  len = (uint2)nb_pixels;

//                trace ("PASS: put %d bytes color %d\n", len, col);


                /* write the pixels */

                i = len;
                while (i-- > 0)
                {
                  buffer[j:4] = pixels;
                  j += 4;
                }

                nb_pixels -= len;
                count     -= len;

                if (count > 0)                 /* some pending pixels to send */
                {
                  t.pending_count = count;
                  t.pending_pixel = pixels;
                }

                t.fsm2_state = 0;      /* restart in state 0 */

                /* ! do not switch current color ! */

                if (nb_pixels == 0)    /* output buffer is full */
                  return 0;
              }
              break;

            case OPER_HORIZ:
//            trace ("Horizontal Mode.\n");

              /* switch to fsm1 */
              t.fsm2_op = 1;    /* first part : read M(a0a1) */

              if (t.g4_color != 0)    /* black */
                t.fsm_state = (uint2)START_BLACK;
              else
                t.fsm_state = (uint2)START_WHITE;
              break;

            case OPER_VERT:
//            trace ("Vertical Mode.\n");
              {
                uint2 new_value, count, len, i;
                int   i_previous;
                byte  col, pixels[4];

                /* find offset b1 : must be > a0 and have */
                /* a different color than t.g4_color,     */

                /* 1) advance t.previous_index til (value > a0) */
                for (;;)
                {
                  if ((int)(t.previous^[t.previous_index] & 32767) > t.g4_a0)
                    break;
                  t.previous_index++;
                }

                i_previous = t.previous_index;

                /* if same color, take next one to have different color */
                if ((uint2)((t.previous^[i_previous] & 32768) != 0) == t.g4_color)
                  i_previous++;

//                trace("b1=%d - ", t.previous^[i_previous] & 32767);

                t.g4_a0 = (int)((int)(t.previous^[i_previous] & 32767) + action2[t.fsm2_state].distance);

//              trace("a0=%d\n", (uint2)t.g4_a0);


                /* compute color of pixels */

                if (t.g4_color != 0)   /* black */
                  col = 0;
                else                   /* white */
                  col = 0xFF;

                if (t.photometric == 1)
                  col = (byte)(0xFF ^ col);

                pixels = {col, col, col, 255};


                /* compute nb of pixels to copy */

                count = (uint2)((uint2)t.g4_a0 - (uint2)t.line_pixel_index);

                if (count > 0)  /* non-empty sequence */
                {
                  /* store begin of sequence's offset in current[] */
                  if (t.current_count == 0 &&   /* first time */
                      t.g4_color == 0)          /* WHITE */
                    /* don't store it */;
                  else if (t.current_count > 0 &&  /* previous exists */
                           (byte)((t.current^[t.current_count-1] & 32768) != 0)
                             == t.g4_color)         /* with same color */
                    /* don't store it */;
                  else
                  {
                    new_value = (uint2)t.line_pixel_index;
//                    trace ("TAB %d (g4_color=%d)\n", new_value, t.g4_color);

                    if (t.g4_color != 0)
                      new_value += 0x8000;
                    t.current^[t.current_count++] = new_value;
                  }
                }

                /* predict later line overflow */
                if (t.line_pixel_index + count > t.width)
                {
//                  trace ("line overflow in VERTICAL\n");
                  return TIFF_FORMAT_ERROR;
                }

                t.line_pixel_index += count; /* cannot cause line overflow */

                /* check if this terminates the line */
                if (t.line_pixel_index == t.width)       /* yes */
                  t.end_of_line = true;


                /* compute active length to send */

                if (nb_pixels > count)
                  len = count;
                else
                  len = (uint2)nb_pixels;

//                trace ("fast put %d bytes color %d\n", len, col);


                /* write the pixels */

                i = len;
                while (i-- > 0)
                {
                  buffer[j:4] = pixels;
                  j += 4;
                }

                nb_pixels -= len;
                count     -= len;

                if (count > 0)                /* some pending pixels to send */
                {
                  t.pending_count = count;
                  t.pending_pixel = pixels;
                }

                t.fsm2_state = 0;      /* restart in state 0 */
                t.g4_color ^= 1;       /* switch current color */

                if (nb_pixels == 0)    /* output buffer is full */
                  return 0;
              }
              break;

            default:
//            trace ("unsupported G4-FAX sequence\n");
              return TIFF_FORMAT_ERROR;
          }
        }
      }
      else   /* horizontal operation is currently in progress */
      {
        if (bit != 0)
          t.fsm_state = action[t.fsm_state].one;
        else
          t.fsm_state = action[t.fsm_state].zero;

//        trace ("state : %d\n", t.fsm_state);

        op = action[t.fsm_state].operation;
        if (op != 0)
        {
          uint2 count, len, i, new_value;
          byte  col, pixels[4];


          /* compute color of pixels */

          if (op == OP_ERROR)
            return TIFF_FORMAT_ERROR;

          if (t.g4_color != 0) /* black */
            col = 0;
          else                 /* white */
            col = 0xFF;

          if (t.photometric == 1)
            col = (byte)(0xFF ^ col);

          pixels = {col, col, col, 255};


          /* compute nb of pixels to copy */

          count = action[t.fsm_state].length;

          if (count > 0)  /* non-empty sequence */
          {
            /* store begin of sequence's offset in current[] */
            if (t.current_count == 0 &&   /* first time */
                t.g4_color == 0)          /* WHITE */
              /* don't store it */;
            else if (t.current_count > 0 &&  /* previous exists */
                     (byte)((t.current^[t.current_count-1] & 32768) != 0)
                        == t.g4_color)         /* with same color */
              /* don't store it */;
            else
            {
              new_value = (uint2)t.line_pixel_index;
//              trace ("TAB %d (g4_color=%d)\n", new_value, t.g4_color);

              if (t.g4_color != 0)
                new_value += 0x8000;
              t.current^[t.current_count++] = new_value;
            }
          }

          /* predict later line overflow */
          if (t.line_pixel_index + count > t.width)
            return TIFF_FORMAT_ERROR;

          t.line_pixel_index += count;     /* cannot cause line overflow */

          if (count <= 63)   /* it's a terminating code */
          {
            if (t.fsm2_op == 1)
            {
              t.fsm2_op = 2;         /* do this twice */
            }
            else
            {
              t.fsm2_op    = 0;      /* end special mode */
              t.fsm2_state = 0;      /* restart fsm2 in state 0 */

              /* store current position in a0 */
              t.g4_a0 = (int)t.line_pixel_index;

              /* check if this terminates the line */
              if (t.line_pixel_index == t.width)       /* yes */
                t.end_of_line = true;
            }

            t.g4_color ^= 1;       /* switch current color */
          }


          /* compute active length to send */

          if (nb_pixels > count)
            len = count;
          else
            len = (uint2)nb_pixels;

//          trace ("horiz put %d bytes color %d\n", len, col);


          /* write the pixels */

          i = len;
          while (i-- > 0)
          {
            buffer[j:4] = pixels;
            j += 4;
          }

          nb_pixels -= len;
          count     -= len;

          if (count > 0)                   /* some pending pixels to send */
          {
            t.pending_count = count;
            t.pending_pixel = pixels;
          }

          if (nb_pixels == 0)    /* output buffer is full */
            return 0;
        }
      }
    }
  }
}

/**************************************************************************/

public void get_tiff_attributes (TIFF                 tiff,
                                 out IMAGE_ATTRIBUTES attr)
{
  clear attr;
  attr.x_resolution = tiff.x_resolution;
  attr.y_resolution = tiff.y_resolution;
}

/**************************************************************************/

public int open_tiff (out TIFF tiff, ref READ_STREAM stream)
{
  ref TIFF t = tiff;
  int      rc;

  clear t;


  /* read the header */

  rc = read_header (ref t, ref stream);
  if (rc < 0)
  {
    close_tiff (ref tiff);
    return rc;
  }


  /* allocate memory in t.entry and read IFD */

  rc = read_ifd (ref t, ref stream);
  if (rc < 0)
  {
    close_tiff (ref tiff);
    return rc;
  }


  /* set default options */

  set_default_options (ref t);


  /* analyze ifd entries and allocate memory               */
  /* for t.strip_offset, t.strip_count and t.color_map. */

  rc = analyze_ifd (ref t, ref stream);
  if (rc < 0)
  {
    close_tiff (ref tiff);
    return rc;
  }


  /* free ifd */

  free t.entry;
  t.entry = null;


  /* check supported options */

  if (t.width == 0 || t.height == 0 || /* image height/width not provided */
      t.strip_offset == null        || /* missing offsets into bitmap */
      t.nb_strips == 0)                /* no strips */
  {
    rc = UNSUPPORTED_OPTION_24;   /* important TAG missing */
  }
  else if (t.samples_per_pixel != 1 ||  /* do not support RGB */
           t.bits_per_sample[0] != 1)   /* only black/white */
  {
    rc = UNSUPPORTED_OPTION_25;   /* support only black/white */
  }
  else if (t.planar_configuration != 1)  /* support only planar config == 1 */
  {
    rc = UNSUPPORTED_OPTION_26;   /* no support for planar_config != 1 */
  }
  else if (t.compression < 2 || t.compression > 4)  /* only FAX compression */
  {
    rc = UNSUPPORTED_OPTION_27;   /* support only G2/G3/G4-FAX compression */
  }
  else if ((t.compression == 3 && t.fax_g3_options != 0
                               && t.fax_g3_options != 4) ||  /* bit 2 */
           (t.compression == 4 && t.fax_g4_options != 0) ||
           (t.compression == 5 && t.lzw_options    != 1))
  {
    rc = UNSUPPORTED_OPTION_28;   /* unsupported compression option */
  }
  else if (t.fill_order != 1 && t.fill_order != 2)
  {
    rc = UNSUPPORTED_OPTION_29; /* unsupported fill_order (neither 1 nor 2) */
  }
  else
  {
    rc = 0;
  }

  if (rc < 0)
  {
    close_tiff (ref tiff);
    return rc;
  }

  if (t.compression >= 2 && t.compression <= 4)
  {
    rc = init_fax_decompression (ref t);
    if (rc < 0)
    {
      close_tiff (ref tiff);
      return rc;
    }
  }

  return 0;
}

/**************************************************************************/
