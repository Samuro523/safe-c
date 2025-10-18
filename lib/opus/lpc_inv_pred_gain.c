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
use macros, inlines;

/* Compute inverse of LPC prediction gain, and                          */
/* test if LPC coefficients are stable (all poles within unit circle)   */
public opus_int32 LPC_inverse_pred_gain_QA(                 /* O   Returns inverse prediction gain in energy domain, Q30    */
    opus_int32           A_QA*[ 16 ],   /* I   Prediction coefficients                                  */
          int       order                              /* I   Prediction order                                         */
)
{
    int   k, n, mult2Q;
    opus_int32 invGain_Q30, rc_Q31, rc_mult1_Q30, rc_mult2, tmp_QA;
    opus_int32 *Aold_QA, Anew_QA;

    Anew_QA = &A_QA[ order & 1 ];

    invGain_Q30 = (opus_int32)1 << 30;
    for( k = order - 1; k > 0; k-- ) {
        /* Check for stability */
        if( ( Anew_QA[ k ] > ((opus_int32)((0.99975) * (float)((int8)1 << (24)) + 0.5)) ) || ( Anew_QA[ k ] < -((opus_int32)((0.99975) * (float)((int8)1 << (24)) + 0.5)) ) ) {
            return 0;
        }

        /* Set RC equal to negated AR coef */
        rc_Q31 = -((opus_int32)((opus_uint32)(Anew_QA[ k ])<<(31 - 24)));

        /* rc_mult1_Q30 range: [ 1 : 2^30 ] */
        rc_mult1_Q30 = ( (opus_int32)1 << 30 ) - (opus_int32)((((int8)((rc_Q31)) * ((rc_Q31))))>>(32));
        ;                   /* reduce A_LIMIT if fails */
        ;

        /* rc_mult2 range: [ 2^30 : silk_int32_MAX ] */
        mult2Q = 32 - silk_CLZ32( (((rc_mult1_Q30) > 0) ? (rc_mult1_Q30) : -(rc_mult1_Q30)) );
        rc_mult2 = silk_INVERSE32_varQ( rc_mult1_Q30, mult2Q + 30 );

        /* Update inverse gain */
        /* invGain_Q30 range: [ 0 : 2^30 ] */
        invGain_Q30 = ((opus_int32)((opus_uint32)((opus_int32)((((int8)((invGain_Q30)) * ((rc_mult1_Q30))))>>(32)))<<(2)));
        ;
        ;

        /* Swap pointers */
        Aold_QA = Anew_QA;
        Anew_QA = &A_QA[ k & 1 ];

        /* Update AR coefficient */
        for( n = 0; n < k; n++ ) {
            tmp_QA = Aold_QA[ n ] - ((opus_int32)(((31) == 1 ? ((((int8)(Aold_QA[ k - n - 1 ]) * (rc_Q31))) >> 1) + ((((int8)(Aold_QA[ k - n - 1 ]) * (rc_Q31))) & 1) : (((((int8)(Aold_QA[ k - n - 1 ]) * (rc_Q31))) >> ((31) - 1)) + 1) >> 1)));
            Anew_QA[ n ] = ((opus_int32)(((mult2Q) == 1 ? ((((int8)(tmp_QA) * (rc_mult2))) >> 1) + ((((int8)(tmp_QA) * (rc_mult2))) & 1) : (((((int8)(tmp_QA) * (rc_mult2))) >> ((mult2Q) - 1)) + 1) >> 1)));
        }
    }

    /* Check for stability */
    if( ( Anew_QA[ 0 ] > ((opus_int32)((0.99975) * (float)((int8)1 << (24)) + 0.5)) ) || ( Anew_QA[ 0 ] < -((opus_int32)((0.99975) * (float)((int8)1 << (24)) + 0.5)) ) ) {
        return 0;
    }

    /* Set RC equal to negated AR coef */
    rc_Q31 = -((opus_int32)((opus_uint32)(Anew_QA[ 0 ])<<(31 - 24)));

    /* Range: [ 1 : 2^30 ] */
    rc_mult1_Q30 = ( (opus_int32)1 << 30 ) - (opus_int32)((((int8)((rc_Q31)) * ((rc_Q31))))>>(32));

    /* Update inverse gain */
    /* Range: [ 0 : 2^30 ] */
    invGain_Q30 = ((opus_int32)((opus_uint32)((opus_int32)((((int8)((invGain_Q30)) * ((rc_mult1_Q30))))>>(32)))<<(2)));
    ;
    ;

    return invGain_Q30;
}

/* For input in Q12 domain */
public
opus_int32 silk_LPC_inverse_pred_gain(              /* O   Returns inverse prediction gain in energy domain, Q30        */
          opus_int16            *A_Q12,             /* I   Prediction coefficients, Q12 [order]                         */
          int              order               /* I   Prediction order                                             */
)
{
    int   k;
    opus_int32 Atmp_QA[ 2 ][ 16 ];
    opus_int32 *Anew_QA;
    opus_int32 DC_resp = 0;

	clear Atmp_QA;
	
    Anew_QA = &Atmp_QA[ order & 1 ];

    /* Increase Q domain of the AR coefficients */
    for( k = 0; k < order; k++ ) {
        DC_resp += (opus_int32)A_Q12[ k ];
        Anew_QA[ k ] = ((opus_int32)((opus_uint32)((opus_int32)A_Q12[ k ])<<(24 - 12)));
    }
    /* If the DC is unstable, we don't even need to do the full calculations */
    if( DC_resp >= 4096 ) {
        return 0;
    }
    return LPC_inverse_pred_gain_QA( &Atmp_QA, order );
}

#end unsafe
