/*
 * jdhuff.h
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains declarations for Huffman entropy decoding routines
 * that are shared between the sequential decoder (jdhuff.c) and the
 * progressive decoder (jdphuff.c).  No other modules need to see these.
 */

use jpeglib, jmorecfg;


#begin unsafe


/* Derived data constructed for each Huffman table */

const int HUFF_LOOKAHEAD = 8;	/* # of bits of lookahead */

struct d_derived_tbl
{
  /* Basic tables: (element [0] of each array is unused) */
  INT32 maxcode[18];		/* largest code of length k (-1 if none) */
  /* (maxcode[17] is a sentinel to ensure jpeg_huff_decode terminates) */
  INT32 valoffset[17];		/* huffval[] offset for codes of length k */
  /* valoffset[k] = huffval[] index of 1st symbol of code length k, less
   * the smallest code of length k; so given a code of length k, the
   * corresponding symbol is huffval[code + valoffset[k]]
   */

  /* Link to public Huffman table (needed only in jpeg_huff_decode) */
  JHUFF_TBL *pub;

  /* Lookahead tables: indexed by the next HUFF_LOOKAHEAD bits of
   * the input data stream.  If the next Huffman code is no more
   * than HUFF_LOOKAHEAD bits long, we can obtain its length and
   * the corresponding symbol directly from these tables.
   */
  int look_nbits[1<<HUFF_LOOKAHEAD]; /* # bits, or 0 if too long */
  UINT8 look_sym[1<<HUFF_LOOKAHEAD]; /* symbol, or unused */
}


/*
 * Fetching the next N bits from the input stream is a time-critical operation
 * for the Huffman decoders.  We implement it with a combination of inline
 * macros and out-of-line subroutines.  Note that N (the number of bits
 * demanded at one time) never exceeds 15 for JPEG use.
 *
 * We read source bytes into get_buffer and dole out bits as needed.
 * If get_buffer already contains enough bits, they are fetched in-line
 * by the macros CHECK_BIT_BUFFER and GET_BITS.  When there aren't enough
 * bits, jpeg_fill_bit_buffer is called; it will attempt to fill get_buffer
 * as full as possible (not just to the number of bits needed; this
 * prefetching reduces the overhead cost of calling jpeg_fill_bit_buffer).
 * Note that jpeg_fill_bit_buffer may return FALSE to indicate suspension.
 * On TRUE return, jpeg_fill_bit_buffer guarantees that get_buffer contains
 * at least the requested number of bits --- dummy zeroes are inserted if
 * necessary.
 */

typedef INT32 bit_buf_type;	/* type of bit-extraction buffer */

const int BIT_BUF_SIZE = 32;	/* size of buffer in bits */

/* If long is > 32 bits on your machine, and shifting/masking longs is
 * reasonably fast, making bit_buf_type be long and setting BIT_BUF_SIZE
 * appropriately should be a win.  Unfortunately we can't define the size
 * with something like  #define BIT_BUF_SIZE (sizeof(bit_buf_type)*8)
 * because not all machines measure sizeof in 8-bit bytes.
 */

struct bitread_perm_state {		/* Bitreading state saved across MCUs */
  bit_buf_type get_buffer;	/* current bit-extraction buffer */
  int bits_left;		/* # of unused bits in it */
}

struct bitread_working_state {		/* Bitreading working state within an MCU */
  /* Current data source location */
  /* We need a copy, rather than munging the original, in case of suspension */
  JOCTET * next_input_byte; /* => next byte to read from source */
  size_t bytes_in_buffer;	/* # of bytes remaining in source buffer */
  /* Bit input buffer --- note these values are kept in register variables,
   * not in this struct, inside the inner loops.
   */
  bit_buf_type get_buffer;	/* current bit-extraction buffer */
  int bits_left;		/* # of unused bits in it */
  /* Pointer needed by jpeg_fill_bit_buffer. */
  j_decompress_ptr cinfo;	/* back link to decompress master record */
}


int
jpeg_make_d_derived_tbl (j_decompress_ptr cinfo, boolean isDC, int tblno,
			 d_derived_tbl ** pdtbl);


/*
 * These macros provide the in-line portion of bit fetching.
 * Use CHECK_BIT_BUFFER to ensure there are N bits in get_buffer
 * before using GET_BITS, PEEK_BITS, or DROP_BITS.
 * The variables get_buffer and bits_left are assumed to be locals,
 * but the state struct might not be (jpeg_huff_decode needs this).
 *	CHECK_BIT_BUFFER(state,n,action);
 *		Ensure there are N bits in get_buffer; if suspend, take action.
 *      val = GET_BITS(n);
 *		Fetch next N bits.
 *      val = PEEK_BITS(n);
 *		Fetch next N bits without removing them from the buffer.
 *	DROP_BITS(n);
 *		Discard next N bits.
 * The value N should be a simple variable, not an expression, because it
 * is evaluated multiple times.
 */

int GET_BITS (int nbits, int get_buffer, ref int bits_left);

int PEEK_BITS (int nbits, int get_buffer, int bits_left);


/* Load up the bit buffer to a depth of at least nbits */
boolean
jpeg_fill_bit_buffer (bitread_working_state * state,
		      bit_buf_type get_buffer, int bits_left,
		      int nbits);


/*
 * Code for extracting next Huffman-coded symbol from input bit stream.
 * Again, this is time-critical and we make the main paths be macros.
 *
 * We use a lookahead table to process codes of up to HUFF_LOOKAHEAD bits
 * without looping.  Usually, more than 95% of the Huffman codes will be 8
 * or fewer bits long.  The few overlength codes are handled with a loop,
 * which need not be inline code.
 *
 * Notes about the HUFF_DECODE macro:
 * 1. Near the end of the data segment, we may fail to get enough bits
 *    for a lookahead.  In that case, we do it the hard way.
 * 2. If the lookahead table contains no entry, the next code must be
 *    more than HUFF_LOOKAHEAD bits long.
 * 3. jpeg_huff_decode returns -1 if forced to suspend.
 */

#if 0

#define HUFF_DECODE(s,state,htbl,failaction,slowlabel) \
{
  int nb, look;
  bool bfast;

  bfast = true;

  if (bits_left < HUFF_LOOKAHEAD)
  {
    if (!jpeg_fill_bit_buffer(&state,get_buffer,bits_left, 0))
    {
$$$$$$$$$$  failaction;
    }

    get_buffer = state.get_buffer;
    bits_left = state.bits_left;

    if (bits_left < HUFF_LOOKAHEAD)
    {
      bfast = false;
    }
  }

  if (bfast)
  {
    look = PEEK_BITS(HUFF_LOOKAHEAD, get_buffer, bits_left);

    nb = $htbl$->look_nbits[look];
    if (nb != 0)
    {
      bits_left -= nb;
      s = $htbl$->look_sym[look];
    }
    else
    {
      nb = HUFF_LOOKAHEAD+1;

      s = jpeg_huff_decode (&state, get_buffer, bits_left, $htbl$, nb);
      if (s < 0)
      {
$$$$$$$$$$        failaction;
      }

      get_buffer = state.get_buffer;
      bits_left = state.bits_left;
    }
  }
  else   // slow
  {
    nb = 1;
    s = jpeg_huff_decode (&state, get_buffer, bits_left, $htbl$, nb);
    if (s < 0)
    {
$$$$$$$$$$      failaction;
    }

    get_buffer = state.get_buffer;
    bits_left = state.bits_left;
  }

}
#endif

/* Out-of-line case for Huffman code fetching */
int jpeg_huff_decode
  (bitread_working_state * state, bit_buf_type get_buffer, int bits_left, d_derived_tbl * htbl, int min_bits);

void
jinit_huff_decoder (j_decompress_ptr cinfo);

#end unsafe
