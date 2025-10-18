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
use lpc_inv_pred_gain;
use tables, bwexpander_32;

/* conversion between prediction filter coefficients and LSFs   */
/* order should be even                                         */
/* a piecewise linear approximation maps LSF <-> cos(LSF)       */
/* therefore the result is not accurate LSFs, but the two       */
/* functions are accurate inverses of each other                */


/* helper function for NLSF2A(..) */
public
void silk_NLSF2A_find_poly(
    opus_int32    *_out,      /* O    intermediate polynomial, QA [dd+1]        */
    opus_int32    *cLSF,      /* I    vector of interleaved 2*cos(LSFs), QA [d] */
    int            dd         /* I    polynomial order (= 1/2 * filter order)   */
)
{
    int   k, n;
    opus_int32 ftmp;

    _out[0] = ((opus_int32)((opus_uint32)(1)<<(16)));
    _out[1] = -cLSF[0];
    for( k = 1; k < dd; k++ ) {
        ftmp = cLSF[2*k];            /* QA*/
        _out[k+1] = ((opus_int32)((opus_uint32)(_out[k-1])<<(1))) - (opus_int32)((16) == 1 ? ((((int8)(ftmp) * (_out[k]))) >> 1) + ((((int8)(ftmp) * (_out[k]))) & 1) : (((((int8)(ftmp) * (_out[k]))) >> ((16) - 1)) + 1) >> 1);
        for( n = k; n > 1; n-- ) {
            _out[n] += _out[n-2] - (opus_int32)((16) == 1 ? ((((int8)(ftmp) * (_out[n-1]))) >> 1) + ((((int8)(ftmp) * (_out[n-1]))) & 1) : (((((int8)(ftmp) * (_out[n-1]))) >> ((16) - 1)) + 1) >> 1);
        }
        _out[1] -= ftmp;
    }
}

/* compute whitening filter coefficients from normalized line spectral frequencies */
public
void silk_NLSF2A(
    opus_int16                  *a_Q12,             /* O    monic whitening filter coefficients in Q12,  [ d ]          */
          opus_int16            *NLSF,              /* I    normalized line spectral frequencies in Q15, [ d ]          */
          int              d                   /* I    filter order (should be even)                               */
)
{
    /* This ordering was found to maximize quality. It improves numerical accuracy of
       silk_NLSF2A_find_poly() compared to "standard" ordering. */
    const            byte      ordering16[16] = {
      0, 15, 8, 7, 4, 11, 12, 3, 2, 13, 10, 5, 6, 9, 14, 1
    };
    const            byte      ordering10[10] = {
      0, 9, 6, 3, 4, 5, 8, 1, 2, 7
    };
              byte      *ordering;
    int   k, i, dd;
    opus_int32 cos_LSF_QA[ 16 ];
    opus_int32 P[ 16 / 2 + 1 ], Q[ 16 / 2 + 1 ];
    opus_int32 Ptmp, Qtmp, f_int, f_frac, cos_val, delta;
    opus_int32 a32_QA1[ 16 ];
    opus_int32 maxabs, absval, idx=0, sc_Q16;

    clear cos_LSF_QA, a32_QA1;
    ;

    /* convert LSFs to 2*cos(LSF), using piecewise linear curve from table */
    ordering = d == 16 ? &ordering16 : &ordering10;
    for( k = 0; k < d; k++ ) {
        ;

        /* f_int on a scale 0-127 (rounded down) */
        f_int = ((NLSF[k])>>(15 - 7));

        /* f_frac, range: 0..255 */
        f_frac = NLSF[k] - ((opus_int32)((opus_uint32)(f_int)<<(15 - 7)));

        ;
        ;

        /* Read start and end value from table */
        cos_val = silk_LSFCosTab_FIX_Q12[ f_int ];                /* Q12 */
        delta   = silk_LSFCosTab_FIX_Q12[ f_int + 1 ] - cos_val;  /* Q12, with a range of 0..200 */

        /* Linear interpolation */
        cos_LSF_QA[ordering[k]] = ((20 - 16) == 1 ? ((((opus_int32)((opus_uint32)(cos_val)<<(8))) + ((delta) * (f_frac))) >> 1) + ((((opus_int32)((opus_uint32)(cos_val)<<(8))) + ((delta) * (f_frac))) & 1) : (((((opus_int32)((opus_uint32)(cos_val)<<(8))) + ((delta) * (f_frac))) >> ((20 - 16) - 1)) + 1) >> 1); /* QA */
    }

    dd = ((d)>>(1));

    /* generate even and odd polynomials using convolution */
    silk_NLSF2A_find_poly( &P, &cos_LSF_QA[ 0 ], dd );
    silk_NLSF2A_find_poly( &Q, &cos_LSF_QA[ 1 ], dd );

    /* convert even and odd polynomials to opus_int32 Q12 filter coefs */
    for( k = 0; k < dd; k++ ) {
        Ptmp = P[ k+1 ] + P[ k ];
        Qtmp = Q[ k+1 ] - Q[ k ];

        /* the Ptmp and Qtmp values at this stage need to fit in int32 */
        a32_QA1[ k ]     = -Qtmp - Ptmp;        /* QA+1 */
        a32_QA1[ d-k-1 ] =  Qtmp - Ptmp;        /* QA+1 */
    }

    /* Limit the maximum absolute value of the prediction coefficients, so that they'll fit in int16 */
    for( i = 0; i < 10; i++ ) {
        /* Find maximum absolute value and its index */
        maxabs = 0;
        for( k = 0; k < d; k++ ) {
            absval = (((a32_QA1[k]) > 0) ? (a32_QA1[k]) : -(a32_QA1[k]));
            if( absval > maxabs ) {
                maxabs = absval;
                idx    = k;
            }
        }
        maxabs = ((16 + 1 - 12) == 1 ? ((maxabs) >> 1) + ((maxabs) & 1) : (((maxabs) >> ((16 + 1 - 12) - 1)) + 1) >> 1);                                          /* QA+1 -> Q12 */

        if( maxabs > 0x7FFF ) {
            /* Reduce magnitude of prediction coefficients */
            maxabs = (((maxabs) < (163838)) ? (maxabs) : (163838));  /* ( silk_int32_MAX >> 14 ) + silk_int16_MAX = 163838 */
            sc_Q16 = ((opus_int32)((0.999) * (float)((int8)1 << (16)) + 0.5)) - 
((opus_int32)((((opus_int32)((opus_uint32)(maxabs - 0x7FFF)<<(14)))) / (((((maxabs) * (idx + 1)))>>(2)))));
            silk_bwexpander_32( &a32_QA1, d, sc_Q16 );
        } else {
            break;
        }
    }

    if( i == 10 ) {
        /* Reached the last iteration, clip the coefficients */
        for( k = 0; k < d; k++ ) {
            a_Q12[ k ] = (opus_int16)((((16 + 1 - 12) == 1 ? ((a32_QA1[ k ]) >> 1) + ((a32_QA1[ k ]) & 1) : (((a32_QA1[ k ]) >> ((16 + 1 - 12) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((16 + 1 - 12) == 1 ? ((a32_QA1[ k ]) >> 1) + ((a32_QA1[ k ]) & 1) : (((a32_QA1[ k ]) >> ((16 + 1 - 12) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((16 + 1 - 12) == 1 ? ((a32_QA1[ k ]) >> 1) + ((a32_QA1[ k ]) & 1) : (((a32_QA1[ k ]) >> ((16 + 1 - 12) - 1)) + 1) >> 1))));  /* QA+1 -> Q12 */
            a32_QA1[ k ] = ((opus_int32)((opus_uint32)((opus_int32)a_Q12[ k ])<<(16 + 1 - 12)));
        }
    } else {
        for( k = 0; k < d; k++ ) {
            a_Q12[ k ] = (opus_int16)((16 + 1 - 12) == 1 ? ((a32_QA1[ k ]) >> 1) + ((a32_QA1[ k ]) & 1) : (((a32_QA1[ k ]) >> ((16 + 1 - 12) - 1)) + 1) >> 1);                /* QA+1 -> Q12 */
        }
    }

    for( i = 0; i < 16; i++ ) {
        if( silk_LPC_inverse_pred_gain( a_Q12, d ) < ((opus_int32)((1.0 / 1.0e4f) * (float)((int8)1 << (30)) + 0.5)) ) {
            /* Prediction coefficients are (too close to) unstable; apply bandwidth expansion   */
            /* on the unscaled coefficients, convert to Q12 and measure again                   */
            silk_bwexpander_32( &a32_QA1, d, 65536 - ((opus_int32)((opus_uint32)(2)<<(uint)(i))) );
            for( k = 0; k < d; k++ ) {
                a_Q12[ k ] = (opus_int16)((16 + 1 - 12) == 1 ? ((a32_QA1[ k ]) >> 1) + ((a32_QA1[ k ]) & 1) : (((a32_QA1[ k ]) >> ((16 + 1 - 12) - 1)) + 1) >> 1);            /* QA+1 -> Q12 */
            }
        } else {
            break;
        }
    }
}

#end unsafe
