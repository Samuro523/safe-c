
/* base64.h : encoding/decoding in base64 */

//--------------------------------------------------------------------------

/* Base 64 encoding converts 1-to-3 binary bytes into 4 ASCII characters */
/* assertion: for buf_out[], the caller must allocate at least (buf_in'length+2)/3*4 chars */
/* buf_out'length will always be a multiple of 4 */
/* input blocks can only be cut at offsets multiple of 3 */
/* output blocks can only be cut at offsets multiple of 4 */

void encode_block_base64 (    byte[] buf_in,    /* binary data */
                          out char[] buf_out,   /* displayable ascii characters */
                          out int    buf_out_length);

//--------------------------------------------------------------------------

/* Base 64 decoding converts 4 ASCII characters into upto 3 binary bytes */
/* assertion: buf_in'length must be a multiple of 4 */
/* assertion: for buf_out'length, the caller must allocate at least (buf_in'length+3)/4*3 bytes */
/* input blocks can only be cut at offsets multiple of 4 */
/* output blocks can only be cut at offsets multiple of 3 */
/* returns 0 if OK, -1 if format error */

int decode_block_base64 (    char[] buf_in,     /* displayable ascii characters */
                         out byte[] buf_out,    /* binary data */
                         out int    buf_out_length);

//--------------------------------------------------------------------------
