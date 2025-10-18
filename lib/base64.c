
/* base64.c */

/******************************************************************************************************/

/*
encoding
 0 .. 25 -> 65 ..  90
26 .. 51 -> 97 .. 122
52 .. 61 -> 48 .. 57
62       -> 43
63       -> 47

decoding
43        -> 62
47        -> 63
48 .. 57  -> 52 .. 61
65 .. 90  ->  0 .. 25
97 .. 122 -> 26 .. 51

*/

/******************************************************************************************************/

/* Base 64 encoding converts 1-to-3 binary bytes into 4 ASCII characters */
/* assertion: in_len in 1..3 (last block might be shorter than 3 bytes) */
/* output length is always 4 bytes */

void encode_base64 (byte[]      buf_in,    /* length 1 .. 3 */
                    out char[4] buf_out)
{
  const string cod64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  buf_out = {cod64[buf_in[0] >> 2],
             cod64[((buf_in[0] & 0x03) << 4) | (buf_in'length > 1 ? ((buf_in[1] & 0xf0) >> 4) : 0)],
             buf_in'length > 1 ? cod64[((buf_in[1] & 0x0f) << 2) | (buf_in'length > 2 ? ((buf_in[2] & 0xc0) >> 6) : 0)] : '=',
             buf_in'length > 2 ? cod64[buf_in[2] & 0x3f] : '='};
}

/******************************************************************************************************/

/* Base 64 decoding converts 4 ASCII characters into 3 binary bytes */
/* returns 0 if OK, -1 in case of illegal input characters */

int decode_base64 (char[4]     buf_in,
                   out byte[3] buf_out,
                   out int     out_len)   /* in 1..3 (0 if error) */
{
  byte t[4];
  int  len;

  /* 255 means illegal value, 64 means '=' */
  const byte dc64[123-43] =
             {62, 255, 255, 255, 63,                               /* + /  */
              52, 53, 54, 55, 56, 57, 58, 59, 60, 61,              /* 0..9 */
              255, 255, 255, 64, 255, 255, 255,
              0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,        /* A .. Z */
              14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
              255, 255, 255, 255, 255, 255,
              26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,  /* a .. z */
              39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

  t = {(byte)((byte)buf_in[0] >= 43 && (byte)buf_in[0] <= 122 ? dc64[(byte)buf_in[0]-43] : 255),
       (byte)((byte)buf_in[1] >= 43 && (byte)buf_in[1] <= 122 ? dc64[(byte)buf_in[1]-43] : 255),
       (byte)((byte)buf_in[2] >= 43 && (byte)buf_in[2] <= 122 ? dc64[(byte)buf_in[2]-43] : 255),
       (byte)((byte)buf_in[3] >= 43 && (byte)buf_in[3] <= 122 ? dc64[(byte)buf_in[3]-43] : 255)};

  clear buf_out, out_len;

  if (t[0] > 63 || t[1] > 63 || t[2] > 64 || t[3] > 64)       /* out of range ascii characters */
    return -1;

  if (buf_in[2] == '=' && buf_in[3] != '=')    /* if third character is '=', fourth character must also be '=' */
    return -1;

  buf_out[0] = (byte)(t[0] << 2 | t[1] >> 4);
  len = 1;

  if (buf_in[2] != '=')
  {
    buf_out[1] = (byte)(t[1] << 4 | (t[2] & 63) >> 2);
    len = 2;

    if (buf_in[3] != '=')
    {
      buf_out[2] = (byte)(((t[2] << 6) & 0xc0) | (t[3] & 63));
      len = 3;
    }
  }

  out_len = len;
  return 0;
}

/******************************************************************************************************/

/* Base 64 encoding converts 1-to-3 binary bytes into 4 ASCII characters */
/* assertion: for buf_out[], the caller must allocate at least (buf_in'length+2)/3*4 chars */
/* buf_out'length will always be a multiple of 4 */
/* input blocks can only be cut at offsets multiple of 3 */
/* output blocks can only be cut at offsets multiple of 4 */

public void encode_block_base64 (byte[]     buf_in,    /* binary data */
                                 out char[] buf_out,   /* displayable ascii characters */
                                 out int    buf_out_length)
{
  int i, j, len;

  clear buf_out;

  for (i=0,j=0; i+3<buf_in'length; i+=3,j+=4)
    encode_base64 (buf_in[i:3], out buf_out[j:4]);

  len = buf_in'length - i;
  if (len > 0)
  {
    encode_base64 (buf_in[i:len], out buf_out[j:4]);
    j += 4;
  }

  buf_out_length = j;
}

/******************************************************************************************************/

/* Base 64 decoding converts 4 ASCII characters into upto 3 binary bytes */
/* assertion: buf_in'length must be a multiple of 4 */
/* assertion: for buf_out'length, the caller must allocate at least (buf_in'length+3)/4*3 bytes */
/* input blocks can only be cut at offsets multiple of 4 */
/* output blocks can only be cut at offsets multiple of 3 */
/* returns 0 if OK, -1 if format error */

public int decode_block_base64 (char[]     buf_in,     /* displayable ascii characters */
                                out byte[] buf_out,    /* binary data */
                                out int    buf_out_length)
{
  int i, j, len;

  assert ((buf_in'length & 3) == 0);   /* check that buf_in'length is a multiple of 4 */

  clear buf_out, buf_out_length;

  for (i=0,j=0; i+4<buf_in'length; i+=4,j+=3)
  {
    if (decode_base64 (buf_in[i:4], out buf_out[j:3], out len) < 0 || len != 3)
      return -1;
  }

  if (i < buf_in'length)
  {
    if (decode_base64 (buf_in[i:4], out buf_out[j:3], out len) < 0)
      return -1;
    j += len;
  }

  buf_out_length = j;
  return 0;
}

/******************************************************************************************************/
