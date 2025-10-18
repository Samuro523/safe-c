#begin unsafe
/***********************************************************************
Copyright (c) 2006-2011, Skype Limited. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the 
names of specific contributors, may be used to endorse or promote
products derived from this software without specific prior written
permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

use opus_types;

/*! \file silk_Inlines.h
 *  \brief silk_Inlines.h defines inline signal processing functions.
 */

/* count leading zeros of opus_int64 */
opus_int32 silk_CLZ64( int8 in );

/* get number of leading zeros and fractional part (the bits right after the leading one */
void silk_CLZ_FRAC(
    opus_int32 in,            /* I  input                               */
    opus_int32 *lz,           /* O  number of leading zeros             */
    opus_int32 *frac_Q7       /* O  the 7 bits right after the leading one */
);

/* Approximation of square root                                          */
/* Accuracy: < +/- 10%  for output values > 15                           */
/*           < +/- 2.5% for output values > 120                          */
opus_int32 silk_SQRT_APPROX( opus_int32 x );

/* Divide two int32 values and return result as int32 in a given Q-domain */
opus_int32 silk_DIV32_varQ(   /* O    returns a good approximation of "(a32 << Qres) / b32" */
          opus_int32     a32,               /* I    numerator (Q0)                  */
          opus_int32     b32,               /* I    denominator (Q0)                */
    int       Qres               /* I    Q-domain of result (>= 0)       */
);

/* Invert int32 value and return result as int32 in a given Q-domain */
opus_int32 silk_INVERSE32_varQ(   /* O    returns a good approximation of "(1 << Qres) / b32" */
          opus_int32     b32,                   /* I    denominator (Q0)                */
          int       Qres                   /* I    Q-domain of result (> 0)        */
);

#end unsafe
