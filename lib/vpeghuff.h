
/* vpeghuff.h : huffman encoding/decoding */

use vpeg;

//--------------------------------------------------------------------------

struct HUF_STATS
{
  int dc_counts[16];   /* DC table : count # of occurences of each symbol */
  int ac_counts[256];  /* AC table */
  int nb_bits;         /* nb extra bits generated (in addition to the dc/ac codes) */
}

//--------------------------------------------------------------------------

int gather_dct_coef_statistics (    DCT_MATRIX coef,
                                ref HUF_STATS  stats);

int gather_statistics_zero_matrices (    int       zero_matrices,
                                     ref HUF_STATS stats);

//--------------------------------------------------------------------------

struct HUFF_TABLE     /* contents of a JPEG DHT marker */
{
  byte nb_symbols_having_nb_bits[16+1]; /* bits[k] = # of symbols with codes of length k bits; bits[0] is unused */
  byte symbol[256];                     /* the symbols, in order of increasing code length */
                                        /* nb_of_symbols = sum of all bits[] values */
}

//--------------------------------------------------------------------------

void create_huff_table (    int        counts[],   // length is 16 or 256
                        out HUFF_TABLE huff);

//--------------------------------------------------------------------------

struct HUFF_CODE
{
  byte   nb_bits; /* nb bits for each symbol (1..16, or 0 if symbol not used) */
  ushort code;    /* 1-to-16 bit sequence for each symbol */
}

struct HUFF_ENCODING_TABLE
{
  HUFF_CODE code[256];   /* order per symbol */
}

//--------------------------------------------------------------------------

/* check huff validity + create huff encoding table */

int create_huff_encoding_table (    HUFF_TABLE          huff,
                                    int                 max_symbols,  /* 16 for DC, 256 for AC */
                                out HUFF_ENCODING_TABLE table);

//--------------------------------------------------------------------------

struct HUFF_OUTPUT
{
  HUFF_CHUNK^  head;               /* will contain output data in linked buffer chain */
  HUFF_CHUNK^  tail;
  uint         bit_buffer;         /* bit shift accumulation buffer */
  int          nb_bits;            /* # bits in bit_buffer */
}

//--------------------------------------------------------------------------

void huff_init_output (out HUFF_OUTPUT hout);

/* assertion: size in 1..16 */
int huff_emit_bits (ref HUFF_OUTPUT hout, uint code, int size);

int send_huff_table (ref HUFF_OUTPUT  hout,
                         HUFF_TABLE   table,
                         int          max_symbols);   /* (16 or 256) */

int huff_encode_dct_coef (ref HUFF_OUTPUT         hout,
                              DCT_MATRIX          coef,
                              HUFF_ENCODING_TABLE dc,
                              HUFF_ENCODING_TABLE ac);

int huff_encode_zero_matrices (ref HUFF_OUTPUT         hout,
                                   int                 zero_matrices,
                                   HUFF_ENCODING_TABLE dc);

int flush_bits (ref HUFF_OUTPUT hout);

//--------------------------------------------------------------------------

struct HUFF_INPUT
{
  HUFF_CHUNK^   head;               /* will contain input data in linked buffer chain */
  HUFF_CHUNK^   current;            /* pointer to current buffer */
  int           index;              /* index into current buffer position */
  uint          bit_buffer;         /* bit shift accumulation buffer */
  int           nb_bits;            /* # bits in bit_buffer */
}

//--------------------------------------------------------------------------

void huff_init_input (out HUFF_INPUT in, HUFF_CHUNK^ buffers);

/* returns next input byte, or 0 on eof */
int huff_receive_bits (ref HUFF_INPUT in, int nb_bits);

int receive_huff_table (ref HUFF_INPUT   in,
                        out HUFF_TABLE   table,
                            int          max_symbols);   /* (16 or 256) */

//--------------------------------------------------------------------------

struct HUFF_DECODING_TABLE
{
  short      lookup[256];  /* 'input byte' to first 'code' starting with this byte (-1 = none) */
  HUFF_CODE  code[256];    /* ordered per code */
  byte       symbol[256];  /* code index to symbol */
  int        first_9bit;   /* index of first 9bit code */
  int        nb_symbols;
}

//--------------------------------------------------------------------------

/* check huff validity + create huff decoding table */

int create_huff_decoding_table (    HUFF_TABLE          huff,
                                    int                 max_symbols,  /* 16 for DC, 256 for AC */
                                out HUFF_DECODING_TABLE table);

//--------------------------------------------------------------------------

/* decode a single block's worth of coefficients */

int huff_decode_dct_coef (ref HUFF_INPUT          in,
                          out DCT_MATRIX          coef,
                              HUFF_DECODING_TABLE dc,
                              HUFF_DECODING_TABLE ac,
                          out int                 skip_zero_matrices);

//--------------------------------------------------------------------------
