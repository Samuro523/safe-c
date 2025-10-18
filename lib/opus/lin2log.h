

use opus_types;

/* Approximation of 128 * log2() (very close inverse of silk_log2lin()) */
/* Convert input to a log scale    */

opus_int32 silk_lin2log(
          opus_int32            inLin               /* I  input in linear scale                                         */
);
