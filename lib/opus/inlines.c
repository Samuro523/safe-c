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

use opus_types, macros, sigproc_fix;

/*! \file silk_Inlines.h
 *  \brief silk_Inlines.h defines inline signal processing functions.
 */

/* count leading zeros of opus_int64 */
public
opus_int32 silk_CLZ64( int8 in )
{
    opus_int32 in_upper;

    in_upper = (opus_int32)((in)>>(32));
    if (in_upper == 0) {
        /* Search in the lower 32 bits */
        return 32 + silk_CLZ32( (opus_int32) in );
    } else {
        /* Search in the upper 32 bits */
        return silk_CLZ32( in_upper );
    }
}

/* get number of leading zeros and fractional part (the bits right after the leading one */
public  void silk_CLZ_FRAC(
    opus_int32 in,            /* I  input                               */
    opus_int32 *lz,           /* O  number of leading zeros             */
    opus_int32 *frac_Q7       /* O  the 7 bits right after the leading one */
)
{
    opus_int32 lzeros = silk_CLZ32(in);

    * lz = lzeros;
    * frac_Q7 = silk_ROR32(in, 24 - lzeros) & 0x7f;
}

/* Approximation of square root                                          */
/* Accuracy: < +/- 10%  for output values > 15                           */
/*           < +/- 2.5% for output values > 120                          */
public  opus_int32 silk_SQRT_APPROX( opus_int32 x )
{
    opus_int32 y, lz, frac_Q7;

    if( x <= 0 ) {
        return 0;
    }

    silk_CLZ_FRAC(x, &lz, &frac_Q7);

    if(( lz & 1 ) != 0) {
        y = 32768;
    } else {
        y = 46214;        /* 46214 = sqrt(2) * 32768 */
    }

    /* get scaling right */
    y >>= ((lz)>>(1));

    /* increment using fractional part of input */
    y = ((y) + ((((y) >> 16) * (opus_int32)((opus_int16)(((opus_int32)((opus_int16)(213)) * (opus_int32)((opus_int16)(frac_Q7)))))) + ((((y) & 0x0000FFFF) * (opus_int32)((opus_int16)(((opus_int32)((opus_int16)(213)) * (opus_int32)((opus_int16)(frac_Q7)))))) >> 16)));

    return y;
}

/* Divide two int32 values and return result as int32 in a given Q-domain */
public  opus_int32 silk_DIV32_varQ(   /* O    returns a good approximation of "(a32 << Qres) / b32" */
          opus_int32     a32,               /* I    numerator (Q0)                  */
          opus_int32     b32,               /* I    denominator (Q0)                */
    int       Qres               /* I    Q-domain of result (>= 0)       */
)
{
    int   a_headrm, b_headrm, lshift;
    opus_int32 b32_inv, a32_nrm, b32_nrm, result;

    ;
    ;

    /* Compute number of bits head room and normalize inputs */
    a_headrm = silk_CLZ32( (((a32) > 0) ? (a32) : -(a32)) ) - 1;
    a32_nrm = ((opus_int32)((opus_uint32)(a32)<<(uint)(a_headrm)));                                       /* Q: a_headrm                  */
    b_headrm = silk_CLZ32( (((b32) > 0) ? (b32) : -(b32)) ) - 1;
    b32_nrm = ((opus_int32)((opus_uint32)(b32)<<(uint)(b_headrm)));                                       /* Q: b_headrm                  */

    /* Inverse of b32, with 14 bits of precision */
    b32_inv = ((opus_int32)((0x7FFFFFFF >> 2) / (((b32_nrm)>>(16)))));   /* Q: 29 + 16 - b_headrm        */

    /* First approximation */
    result = ((((a32_nrm) >> 16) * (opus_int32)((opus_int16)(b32_inv))) + ((((a32_nrm) & 0x0000FFFF) * (opus_int32)((opus_int16)(b32_inv))) >> 16));                                     /* Q: 29 + a_headrm - b_headrm  */

    /* Compute residual by subtracting product of denominator and first approximation */
    /* It's OK to overflow because the final value of a32_nrm should always be small */
    a32_nrm = ((opus_int32)((opus_uint32)(a32_nrm) - (opus_uint32)(((opus_int32)((opus_uint32)((opus_int32)((((int8)((b32_nrm)) * ((result))))>>(32))) << (3))))));  /* Q: a_headrm   */

    /* Refinement */
    result = ((result) + ((((a32_nrm) >> 16) * (opus_int32)((opus_int16)(b32_inv))) + ((((a32_nrm) & 0x0000FFFF) * (opus_int32)((opus_int16)(b32_inv))) >> 16)));                             /* Q: 29 + a_headrm - b_headrm  */

    /* Convert to Qres domain */
    lshift = 29 + a_headrm - b_headrm - Qres;
    if( lshift < 0 ) {
        return (((opus_int32)((opus_uint32)(((((((opus_int32)(-0x80000000)))>>((-lshift)))) > (((0x7FFFFFFF)>>((-lshift)))) ? (((result)) > (((((opus_int32)(-0x80000000)))>>((-lshift)))) ? (((((opus_int32)(-0x80000000)))>>((-lshift)))) : (((result)) < (((0x7FFFFFFF)>>((-lshift)))) ? (((0x7FFFFFFF)>>((-lshift)))) : ((result)))) : (((result)) > (((0x7FFFFFFF)>>((-lshift)))) ? (((0x7FFFFFFF)>>((-lshift)))) : (((result)) < (((((opus_int32)(-0x80000000)))>>((-lshift)))) ? (((((opus_int32)(-0x80000000)))>>((-lshift)))) : ((result))))))<<(uint)((-lshift)))));
    } else {
        if( lshift < 32){
            return ((result)>>(lshift));
        } else {
            /* Avoid undefined result */
            return 0;
        }
    }
}

/* Invert int32 value and return result as int32 in a given Q-domain */
public  opus_int32 silk_INVERSE32_varQ(   /* O    returns a good approximation of "(1 << Qres) / b32" */
          opus_int32     b32,                   /* I    denominator (Q0)                */
          int       Qres                   /* I    Q-domain of result (> 0)        */
)
{
    int   b_headrm, lshift;
    opus_int32 b32_inv, b32_nrm, err_Q32, result;

    ;
    ;

    /* Compute number of bits head room and normalize input */
    b_headrm = silk_CLZ32( (((b32) > 0) ? (b32) : -(b32)) ) - 1;
    b32_nrm = ((opus_int32)((opus_uint32)(b32)<<(uint)(b_headrm)));                                       /* Q: b_headrm                */

    /* Inverse of b32, with 14 bits of precision */
    b32_inv = ((opus_int32)((0x7FFFFFFF >> 2) / (((b32_nrm)>>(16)))));   /* Q: 29 + 16 - b_headrm    */

    /* First approximation */
    result = ((opus_int32)((opus_uint32)(b32_inv)<<(16)));                                          /* Q: 61 - b_headrm            */

    /* Compute residual by subtracting product of denominator and first approximation from one */
    err_Q32 = ((opus_int32)((opus_uint32)(((opus_int32)1<<29) - ((((b32_nrm) >> 16) * (opus_int32)((opus_int16)(b32_inv))) + ((((b32_nrm) & 0x0000FFFF) * (opus_int32)((opus_int16)(b32_inv))) >> 16)))<<(3)));        /* Q32                        */

    /* Refinement */
    result = ((((((result)) + (((((err_Q32)) >> 16) * (opus_int32)((opus_int16)((b32_inv)))) + (((((err_Q32)) & 0x0000FFFF) * (opus_int32)((opus_int16)((b32_inv)))) >> 16))))) + ((((err_Q32)) * (((16) == 1 ? (((b32_inv)) >> 1) + (((b32_inv)) & 1) : ((((b32_inv)) >> ((16) - 1)) + 1) >> 1)))));                             /* Q: 61 - b_headrm            */

    /* Convert to Qres domain */
    lshift = 61 - b_headrm - Qres;
    if( lshift <= 0 ) {
        return (((opus_int32)((opus_uint32)(((((((opus_int32)(-0x80000000)))>>((-lshift)))) > (((0x7FFFFFFF)>>((-lshift)))) ? (((result)) > (((((opus_int32)(-0x80000000)))>>((-lshift)))) ? (((((opus_int32)(-0x80000000)))>>((-lshift)))) : (((result)) < (((0x7FFFFFFF)>>((-lshift)))) ? (((0x7FFFFFFF)>>((-lshift)))) : ((result)))) : (((result)) > (((0x7FFFFFFF)>>((-lshift)))) ? (((0x7FFFFFFF)>>((-lshift)))) : (((result)) < (((((opus_int32)(-0x80000000)))>>((-lshift)))) ? (((((opus_int32)(-0x80000000)))>>((-lshift)))) : ((result))))))<<(uint)((-lshift)))));
    } else {
        if( lshift < 32){
            return ((result)>>(lshift));
        }else{
            /* Avoid undefined result */
            return 0;
        }
    }
}

#end unsafe
