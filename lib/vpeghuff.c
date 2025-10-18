
// vpeghuff.c : huffman encoding/decoding

use vpeg;

/**************************************************************************************/

const int MAX_COEF_BITS = 10;

/**************************************************************************************/

/* jpeg_natural_order[i] is the natural-order position of the i'th element of zigzag order */
const int jpeg_natural_order[64+16] =
{ 0,  1,  8, 16,  9,  2,  3, 10,
 17, 24, 32, 25, 18, 11,  4,  5,
 12, 19, 26, 33, 40, 48, 41, 34,
 27, 20, 13,  6,  7, 14, 21, 28,
 35, 42, 49, 56, 57, 50, 43, 36,
 29, 22, 15, 23, 30, 37, 44, 51,
 58, 59, 52, 45, 38, 31, 39, 46,
 53, 60, 61, 54, 47, 55, 62, 63,
 63, 63, 63, 63, 63, 63, 63, 63, /* 16 extra entries for safety in decoder */
 63, 63, 63, 63, 63, 63, 63, 63
};

/**************************************************************************************/

/* - stats must be zeroed before the call, function adds to existing values */
/* - code must match with function to encode a block, below */
/* - returns 0 if OK or negative error if dct coef out of range */

/*
 coding
 ------
 dc symbols (-1024 .. 1023):  (delta in range -2047 .. 2047)
 code 0 .. 11 : nb bits

 ac symbols (-1024 .. 1023):  (delta in range -2047 .. 2047)
 code 0   : end of dct block
 code 1..11, 17..27, 33..43, ..., 241..251 : skip zero + symbol(1..11) nb_zeroes(0..15) x 16 + nb_bits(1..11)
 code 240 : 16 zero coefficients
*/

public int gather_dct_coef_statistics (    DCT_MATRIX coef,
                                       ref HUF_STATS  stats)
{
  int temp, nbits, k, r;


  /* encode the DC coefficient */

  temp = coef[0];
  if (temp < 0)
    temp = -temp;

  /* find the number of bits needed for the magnitude of the coefficient */
  nbits = 0;
  while (temp != 0)
  {
    nbits++;
    temp >>= 1;
  }

  /* check for out-of-range coefficient values */
  /* since we're encoding a difference, the range limit is twice as much */
  if (nbits > MAX_COEF_BITS+1)    /* more than 11 bits ? */
    return -1000;

  /* count the Huffman symbol for the number of bits */
  stats.dc_counts[nbits]++;         /* nbits in 0 .. 11, temp in 0 .. 2047 */

  /* count the bits themselves */
  stats.nb_bits += nbits;


  /* Encode the AC coefficients */

  r = 0;			/* r = run length of zeros */
  
  for (k=1; k<64; k++)
  {
    temp = coef[jpeg_natural_order[k]];
    if (temp == 0)
    {
      r++;   /* count nb of zero coefficients */
    }
    else
    {
      /* if run length >= 16, must emit special run-length-16 codes (0xF0) */
      while (r >= 16)
      {
        stats.ac_counts[0xF0]++;    // code 240
        r -= 16;
      }

      if (temp < 0)
	temp = -temp;

      /* find the number of bits needed for the magnitude of the coefficient */
      nbits = 1;                /* there must be at least one 1 bit since temp != 0 */
      for (;;)
      {
        temp >>= 1;
        if (temp == 0)
          break;
	nbits++;
      }

      /* check for out-of-range coefficient values */
      /* since we're encoding a difference, the range limit is twice as much */
      if (nbits > MAX_COEF_BITS+1)    /* more than 11 bits ? */
	return -1001;

      /* count Huffman symbol for run length / number of bits */
      stats.ac_counts[(r << 4) + nbits]++;     /* code (0..15) x 16 + (1..11) */

      /* count the bits themselves */
      stats.nb_bits += nbits;

      r = 0;
    }
  }

  /* if the last coef(s) were zero, emit an end-of-block code */
  if (r > 0)
    stats.ac_counts[0]++;      // code 0

  return 0;
}

/**************************************************************************************/

public int gather_statistics_zero_matrices (    int       zero_matrices,
                                            ref HUF_STATS stats)
{
  int temp, nbits;

  stats.dc_counts[15]++;      // SKIP code 15

  if (zero_matrices <= 0 || zero_matrices > 32767)   /* 1 to 15 bits */
    return -1002;

  /* find the number of bits needed for the magnitude of 'zero_matrices' */
  nbits = 0;
  temp = zero_matrices;
  while (temp != 0)
  {
    nbits++;
    temp >>= 1;
  }

  stats.dc_counts[nbits]++;

  /* count the bits themselves */
  stats.nb_bits += nbits;

  return 0;
}

/**************************************************************************************/

public void create_huff_table (    int        counts[],   // length is 16 or 256
                               out HUFF_TABLE huff)
{
  int  freq[256];
  int  codesize[256];    /* codesize[k] = code length of symbol k */
  int  others[256];      /* next symbol in current branch of tree */
  int  count, c1, c2;
  int  p, i, j, nb_freq;
  int  v;
  byte bits[32];        /* bits[k] = # of symbols with code length k */

  clear huff;

  clear freq;
  freq[0:counts'length] = counts;
  nb_freq = counts'length;

  clear codesize, others;

  for (i=0; i<nb_freq; i++)
    others[i] = -1;		/* init links to empty */

  /* force at least 2 non-zero frequencies */
  count = 0;
  for (i=0; i<nb_freq; i++)
    count += (int)(freq[i] > 0);
  i = 0;
  while (count < 2)
  {
    if (freq[i] == 0)
    {
      freq[i] = 1;
      count++;
    }
    i++;
  }

  for (;;)
  {
    /* find the smallest nonzero frequency, set c1 = its symbol */
    /* in case of ties, take the larger symbol number */
    c1 = -1;
    v = 2000000000;
    for (i=0; i<nb_freq; i++)
    {
      if (freq[i] != 0 && freq[i] <= v)
      {
	v = freq[i];
	c1 = i;
      }
    }

    /* find the next smallest nonzero frequency, set c2 = its symbol */
    /* in case of ties, take the larger symbol number */
    c2 = -1;
    v = 2000000000;
    for (i=0; i<nb_freq; i++)
    {
      if (freq[i] != 0 && freq[i] <= v && i != c1)
      {
	v = freq[i];
	c2 = i;
      }
    }

    /* done if we've merged everything into one frequency */
    if (c2 < 0)
      break;

    /* else merge the two counts/trees */
    freq[c1] += freq[c2];
    freq[c2] = 0;

    /* increment the codesize of everything in c1's tree branch */
    codesize[c1]++;
    while (others[c1] >= 0)
    {
      c1 = others[c1];
      codesize[c1]++;
    }

    others[c1] = c2;		/* chain c2 onto c1's tree branch */

    /* increment the codesize of everything in c2's tree branch */
    codesize[c2]++;
    while (others[c2] >= 0)
    {
      c2 = others[c2];
      codesize[c2]++;
    }
  }

  /* now count the number of symbols of each code length */
  clear bits;
  for (i=0; i<nb_freq; i++)
  {
    if (codesize[i] != 0)
      bits[codesize[i]]++;
  }


  /* convert all code lengths > 16 to shorter codes */

  for (i=31; i>16; i--)
  {
    while (bits[i] > 0)
    {
      j = i - 2;		/* find length of new prefix to be used */

      while (bits[j] == 0)
	j--;

      bits[i] -= 2;		/* remove two symbols */
      bits[i-1]++;		/* one goes in this length */
      bits[j+1] += 2;		/* two new symbols in this length */
      bits[j]--;		/* symbol of this length is now a prefix */
    }
  }

  /* return final symbol counts (only for lengths 0..16) */
  huff.nb_symbols_having_nb_bits = bits[0:16+1];

  /* return a list of the symbols sorted by code length */
  clear huff.symbol;
  p = 0;
  for (i=1; i<32; i++)
  {
    for (j=0; j<nb_freq; j++)
    {
      if (codesize[j] == i)
      {
        huff.symbol[p] = (byte)j;
	p++;
      }
    }
  }
}

/**************************************************************************************/

/* check huff table validity + create huff codes */

int create_huff_codes (    HUFF_TABLE  huff,
                           int         max_symbols,  /* 16 for DC, 256 for AC */
                       out HUFF_CODE   table[],
                       out int         pnb_symbols)
{
  int  p, i, j, l, nb_symbols, si;
  uint code;

  clear table, pnb_symbols;

  /* make table of Huffman code length for each symbol */

  p = 0;
  for (l=1; l<=16; l++)
  {
    i = (int) huff.nb_symbols_having_nb_bits[l];    /* entry 0 is unused */

    if (i < 0 || p + i > max_symbols)    /* protect against table overrun */
      return -1003;

    while (i-- != 0)
      table[p++].nb_bits = (byte)l;    /* assign nb bits to all these symbols */
  }
  nb_symbols = p;


  /* assign codes (=bit sequences) to all symbols */

  code = 0;
  p    = 0;
  si   = table[0].nb_bits;    /* nb bits of symbol 0 */

  while (p < nb_symbols)     /* max 16 iterations */
  {
    while (p < nb_symbols && (int)table[p].nb_bits == si)       /* assign all codes having 'si' bits */
    {
      table[p++].code = (ushort)code;    /* assign code (1-to-16 bit sequence) */
      code++;
    }

    if ((code-1) >= (uint)(1 << si))     /* last assigned code didn't fit into 'si' bits ! */
      return -1004;

    code <<= 1;
    si++;
  }


  /* check that all symbol are different and in range */

  for (i=0; i<nb_symbols; i++)
  {
    if ((int)huff.symbol[i] >= max_symbols)
      return -1005;

    for (j=i+1; j<nb_symbols; j++)
    {
      if (huff.symbol[i] == huff.symbol[j])
        return -1006;
    }
  }


  pnb_symbols = nb_symbols;

  return 0;
}

/**************************************************************************************/

/* create huff encoding table */

public int create_huff_encoding_table (    HUFF_TABLE          huff,
                                           int                 max_symbols,  /* 16 for DC, 256 for AC */
                                       out HUFF_ENCODING_TABLE table)
{
  int       rc, nb_symbols, p, i;
  HUFF_CODE codes[256];

  clear table;

  rc = create_huff_codes (huff, max_symbols, out codes, out nb_symbols);
  if (rc < 0)
    return rc;

  for (p=0; p<nb_symbols; p++)    /* order by symbol value */
  {
    i = huff.symbol[p];
    if (i < 0 || i >= max_symbols)
      return -1007;
    table.code[i] = codes[p];
  }

  return 0;
}

/**************************************************************************************/

public void huff_init_output (out HUFF_OUTPUT hout)
{
  clear hout;
}

/**************************************************************************************/

int emit_byte (ref HUFF_OUTPUT hout, int byt)
{
  HUFF_CHUNK^ p;

  if (hout.tail == null || hout.tail^.count == HUFF_BUFFER_SIZE)
  {
    p = new HUFF_CHUNK ' {count  => 0,
                          buffer => new byte[HUFF_BUFFER_SIZE],
                          next   => null};

    if (hout.head == null)
      hout.head = p;
    else
      hout.tail^.next = p;

    hout.tail = p;
  }

  hout.tail^.buffer^[hout.tail^.count++] = (byte)byt;

  return 0;
}

/**************************************************************************************/

/* output bits */

/* assertion: size in 1..16 */

public int huff_emit_bits (ref HUFF_OUTPUT hout, uint code, int size)
{
  int bits_left, rc;

  if (size < 1 || size > 16)    /* should not occur */
    return -1009;

// trace ("out: %5lu   (%2d bits)\n", (code & ((1L << size) - 1)), size);

  hout.bit_buffer = (hout.bit_buffer << (uint)size)         // make space
                  | (code & ((1 << (uint)size) - 1));       // insert valid bits of new code

  bits_left = hout.nb_bits + size;   // new number of bits in bit_buffer

  while (bits_left >= 8)     /* send bytes, MSB first */
  {
    bits_left -= 8;

    rc = emit_byte (ref hout, ((int)hout.bit_buffer >> bits_left));
    if (rc < 0)
      return rc;
  }

  hout.nb_bits = bits_left;

  return 0;
}

/**************************************************************************************/

public int flush_bits (ref HUFF_OUTPUT hout)
{
  int rc;

  if (hout.nb_bits >= 1)  /* between 1 and 7 bits left */
  {
    rc = emit_byte (ref hout, ((int)hout.bit_buffer << (8 - hout.nb_bits)));
    if (rc < 0)
      return rc;
  }

  hout.nb_bits = 0;
  hout.bit_buffer = 0;

  return 0;
}

/**************************************************************************************/

int huff_emit_code (ref HUFF_OUTPUT hout, HUFF_CODE code)
{
  return huff_emit_bits (ref hout, code.code, code.nb_bits);
}

/**************************************************************************************/

/* Encode a single block's worth of coefficients */
/* - we encode the raw DC value, not the delta */

public int huff_encode_dct_coef (ref HUFF_OUTPUT         hout,
                                     DCT_MATRIX          coef,
                                     HUFF_ENCODING_TABLE dc,
                                     HUFF_ENCODING_TABLE ac)
{
  int temp, temp2, nbits, k, r, i, rc;


  /* encode the DC coefficient */

  /*
     DC Code  Size            Additional Bits                           DC Value 
     =======  ====          ======================                     ==========
       00       0                   none                                   0
       01       1                   0 1                                  -1 1 
       02       2               00,01 10,11                           -3,-2 2,3 
       03       3     000,001,010,011 100,101,110,111           -7,-6,-5,-4 4,5,6,7 
       04       4       0000,...,0111 1000,...,1111              -15,...,-8 8,...,15 
       05       5          0 0000,... ...,1 1111                -31,...,-16 16,...,31 
       06       6         00 0000,... ...,11 1111               -63,...,-32 32,...,63 
       07       7        000 0000,... ...,111 1111             -127,...,-64 64,...,127 
       08       8       0000 0000,... ...,1111 1111           -255,...,-128 128,...,255 
       09       9     0 0000 0000,... ...,1 1111 1111         -511,...,-256 256,...,511 
       0A      10    00 0000 0000,... ...,11 1111 1111       -1023,...,-512 512,...,1023 
       0B      11   000 0000 0000,... ...,111 1111 1111     -2047,...,-1024 1024,...,2047 
  */

  temp = coef[0];
  temp2 = temp;

  if (temp < 0)
  {
    temp = -temp;   /* temp is abs value of input (0 .. 2047) */
    temp2--;        /* temp2 = bitwise complement of abs(input) */
  }

  /* find the number of bits needed for the magnitude of the coefficient */
  nbits = 0;
  while (temp != 0)
  {
    nbits++;
    temp >>= 1;
  }

  /* check for out-of-range coefficient values */
  /* since we're encoding a difference, the range limit is twice as much */
  if (nbits > MAX_COEF_BITS+1)
    return -1010;

  /* emit the Huffman-coded symbol for the number of bits */
  rc = huff_emit_code (ref hout, dc.code[nbits]);
  if (rc < 0)
    return rc;

  /* emit that number of bits of the value, if positive, */
  /* or the complement of its magnitude, if negative. */
  if (nbits != 0)        /* huff_emit_bits rejects calls with size 0 */
  {
    rc = huff_emit_bits (ref hout, (uint)temp2, nbits);
    if (rc < 0)
      return rc;
  }

  /* encode the AC coefficients */
  
  r = 0;			/* r = run length of zeros */
  
  for (k=1; k<64; k++)
  {
    temp = coef[jpeg_natural_order[k]];

    if (temp == 0)
    {
      r++;
    }
    else
    {
      /* if run length >= 16, must emit special run-length-16 codes (0xF0) */
      while (r >= 16)
      {
        rc = huff_emit_code (ref hout, ac.code[0xF0]);
        if (rc < 0)
	  return rc;
	r -= 16;
      }

      temp2 = temp;
      if (temp < 0)
      {
	temp = -temp;		/* temp is abs value of input */
	temp2--;
      }

      /* find the number of bits needed for the magnitude of the coefficient */
      nbits = 1;		/* there must be at least one 1 bit */
      for (;;)
      {
        temp >>= 1;
        if (temp == 0)
          break;
	nbits++;
      }

      /* check for out-of-range coefficient values */
      /* since we're encoding a difference, the range limit is twice as much */
      if (nbits > MAX_COEF_BITS+1)    /* more than 11 bits ? */
	return -1011;
      
      /* emit Huffman symbol for run length / number of bits */
      i = (r << 4) + nbits;
      rc = huff_emit_code (ref hout, ac.code[i]);
      if (rc < 0)
	return rc;

      /* Emit that number of bits of the value, if positive, */
      /* or the complement of its magnitude, if negative. */
      rc = huff_emit_bits (ref hout, (uint) temp2, nbits);
      if (rc < 0)
	return rc;

      r = 0;
    }
  }

  /* if the last coef(s) were zero, emit an end-of-block code */
  if (r > 0)
  {
    rc = huff_emit_code (ref hout, ac.code[0]);
    if (rc < 0)
      return rc;
  }

  return 0;
}

/**************************************************************************************/

public int huff_encode_zero_matrices (ref HUFF_OUTPUT         hout,
                                          int                 zero_matrices,  // 1 .. 1200 ?
                                          HUFF_ENCODING_TABLE dc)
{
  int temp, nbits, rc;

  if (zero_matrices <= 0 || zero_matrices > 32767)
  {
//    trace ("error: huff_encode_zero_matrices() failed\n");
    return -1012;
  }

  /* emit the Huffman-coded symbol for SKIP (=15) */
  rc = huff_emit_code (ref hout, dc.code[15]);
  if (rc < 0)
    return rc;


  /* find the number of bits needed for the magnitude of 'zero_matrices' */

  nbits = 0;
  temp = zero_matrices;
  while (temp != 0)
  {
    nbits++;
    temp >>= 1;
  }

  /* emit the Huffman-coded symbol for the number of bits */
  rc = huff_emit_code (ref hout, dc.code[nbits]);
  if (rc < 0)
    return rc;

  /* emit that number of bits of the value */
  rc = huff_emit_bits (ref hout, (uint)zero_matrices, nbits);
  if (rc < 0)
    return rc;

  return 0;
}

/**************************************************************************************/

public int send_huff_table (ref HUFF_OUTPUT  hout,
                                HUFF_TABLE   table,
                                int          max_symbols)   // (16 or 256)
{
  int size, i, count, rc;

  if (max_symbols <= 16)
    size = 4;
  else
    size = 8;

  count = 0;
  for (i=1; i<=16; i++)
  {
    rc = huff_emit_bits (ref hout, table.nb_symbols_having_nb_bits[i], size);
    if (rc < 0)
      return rc;
    count += table.nb_symbols_having_nb_bits[i];
  }

  for (i=0; i<count; i++)
  {
    rc = huff_emit_bits (ref hout, table.symbol[i], size);
    if (rc < 0)
      return rc;
  }

  return 0;
}

/**************************************************************************************/

public void huff_init_input (out HUFF_INPUT  in,
                                 HUFF_CHUNK^ buffers)
{
  clear in;
  in.head    = buffers;
  in.current = buffers;
}

/**************************************************************************************/

void load_16_bit (ref HUFF_INPUT in)
{
  while (in.nb_bits < 16)    // less than 16 bits available
  {
    for (;;)
    {
      if (in.current == null)
      {
        in.bit_buffer <<= (16 - (uint)in.nb_bits);
        in.nb_bits = 16;
        return;
      }

      if (in.index < in.current^.count)
        break;

      in.current = in.current^.next;
      in.index = 0;
    }

    /* load a byte into bit_buffer */

    in.bit_buffer <<= 8;
    in.nb_bits += 8;
    in.bit_buffer |= in.current^.buffer^[in.index++];
  }
}

/**************************************************************************************/

/* returns next input byte, or 0 on eof */

public int huff_receive_bits (ref HUFF_INPUT in, int nb_bits)
{
  int val;

  load_16_bit (ref in);

  val = ((int)in.bit_buffer >> (in.nb_bits - nb_bits));
  in.nb_bits -= nb_bits;
  in.bit_buffer &= (uint) ((1 << in.nb_bits) - 1);   // cleanup

  return val;
}

/**************************************************************************************/

// returns next input byte, or 0 on eof

int get_huff_code (ref HUFF_INPUT in, HUFF_DECODING_TABLE table)
{
  int byt, index;

  load_16_bit (ref in);

  /* peek a byte */
  byt = ((int)in.bit_buffer >> (in.nb_bits - 8));

  /* fast 8-bit lookup */
  index = table.lookup[byt];

  if (index != -1)
  {
    in.nb_bits -= table.code[index].nb_bits;
    in.bit_buffer &= (uint) ((1 << in.nb_bits) - 1);   // cleaup

    return table.symbol[index];
  }

  /* search in all codes */
  for (index=table.first_9bit; index<table.nb_symbols; index++)
  {
    if ((in.bit_buffer >> ((uint)in.nb_bits - table.code[index].nb_bits))
         == table.code[index].code)
    {
      in.nb_bits -= table.code[index].nb_bits;
      in.bit_buffer &= (uint) ((1 << in.nb_bits) - 1);   // cleaup
      return table.symbol[index];
    }
  }

  return 0;
}

/**************************************************************************************/

public int receive_huff_table (ref HUFF_INPUT   in,
                               out HUFF_TABLE   table,
                                   int          max_symbols)   /* (16 or 256) */
{
  int size, n, i, count;

  clear table;

  if (max_symbols <= 16)
    size = 4;
  else
    size = 8;

  count = 0;
  table.nb_symbols_having_nb_bits[0] = 0;
  for (i=1; i<=16; i++)
  {
    n = huff_receive_bits (ref in, size);
    table.nb_symbols_having_nb_bits[i] = (byte)n;
    count += n;
  }

  if (count > max_symbols)
    return -1013;

  for (i=0; i<count; i++)
  {
    n = huff_receive_bits (ref in, size);
    table.symbol[i] = (byte)n;
  }

  return 0;
}

/**************************************************************************************/

/* create huff decoding table */

public int create_huff_decoding_table (    HUFF_TABLE          huff,
                                           int                 max_symbols,  /* 16 for DC, 256 for AC */
                                       out HUFF_DECODING_TABLE table)
{
  int rc, nb_symbols, b, p, nb_bits, code;

  clear table;

  /* load code[] in bit sequence order */
  rc = create_huff_codes (huff, max_symbols, out table.code, out nb_symbols);
  if (rc < 0)
    return rc;

  /* copy symbols */
  table.symbol = huff.symbol;

  /* build lookup table */

  b = 0;

  for (p=0; p<nb_symbols; p++)
  {
    nb_bits = table.code[p].nb_bits;
    code    = table.code[p].code;

    if (nb_bits > 8)
      break;

    while (b < 256 && code < (b >> (8 - nb_bits)))
    {
      table.lookup[b] = -1;   /* -1 means invalid code */
      b++;
    }

    while (b < 256 && code == (b >> (8 - nb_bits)))
    {
      table.lookup[b] = (short)p;   /* index to code[] and symbol[] */
      b++;
    }
  }

  table.first_9bit = p;
  table.nb_symbols = nb_symbols;

  for (; b<256; b++)        /* set remaining codes to -1 */
    table.lookup[b] = -1;

  return 0;
}

/**************************************************************************************/

int EXTEND (int x, int s)
{
  return (x < (1<<(s-1)) ? x + (((-1)<<(s)) + 1) : (x));
}

/**************************************************************************************/

/* decode a single block of coefficients */

public int huff_decode_dct_coef (ref HUFF_INPUT          in,
                                 out DCT_MATRIX          coef,
                                     HUFF_DECODING_TABLE dc,
                                     HUFF_DECODING_TABLE ac,
                                 out int                 skip_zero_matrices)
{
  int nb_bits, s, k, r;


  clear coef, skip_zero_matrices;


  /* DC coefficient */

  nb_bits = get_huff_code (ref in, dc);

  if (nb_bits == 15)  /* special code SKIP */
  {
    nb_bits = get_huff_code (ref in, dc);    /* 1 .. 15 */
    if (nb_bits < 0 || nb_bits > 15)
      return -1014;

    s = huff_receive_bits (ref in, nb_bits);

    skip_zero_matrices = s;

    return 0;
  }

  skip_zero_matrices = 0;
  s = 0;  /* default (if nb_bits == 0) */

  if (nb_bits != 0)
  {
    if (nb_bits > MAX_COEF_BITS+1)
      return -1015;

    s = huff_receive_bits (ref in, nb_bits);
    s = EXTEND (s, nb_bits);
  }

  coef[0] = (short)s;


  /* AC coefficients */

  k = 1;
  while (k < 64)
  {
    s = get_huff_code (ref in, ac);

    r = s >> 4;          /* nb leading zeroes (0 .. 15) */
    nb_bits = s & 15;    /* size of AC code (1 .. 11, or 0 if EOB or SKIP) */

    if (nb_bits != 0)
    {
      if (nb_bits > MAX_COEF_BITS+1)
        return -1016;

      s = huff_receive_bits (ref in, nb_bits);
      s = EXTEND (s, nb_bits);

      k += r;   /* skip 'r' zero coefficients */

      if (k > 64)
        return -1017;

      /* Output coefficient in natural (dezigzagged) order.
       * Note: the extra entries in jpeg_natural_order[] will save us
       * if k >= DCTSIZE2, which could happen if the data is corrupted.
       */

      coef[jpeg_natural_order[k++]] = (short)s;
    }
    else   /* 0 (end of block) or 240 (16 zero coef) */
    {
      if (s == 240)   /* code 240 (produce 16 zero coef) */
      {
        k += 16;
        if (k > 64)
          return -1018;
      }
      else if (s == 0)    /* end of block */
      {
        break;
      }
      else
      {
        return -1019;
      }
    }
  }

  return 0;
}

/**************************************************************************************/
