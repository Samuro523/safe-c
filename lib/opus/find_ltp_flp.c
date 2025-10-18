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

use corrmatrix_flp, energy_flp, regularize_correlations_flp, solve_ls_flp;
use residual_energy_flp, scale_vector_flp, sigproc_flp;

public
void silk_find_LTP_FLP(
    float                      b*, // [ 4 * 5 ],      /* O    LTP coefs                                   */
    float                      WLTP*, //[ 4 * 5 * 5 ], /* O    Weight for LTP quantization       */
    float                      *LTPredCodGain,                     /* O    LTP coding gain                             */
          float                r_lpc*,                            /* I    LPC residual                                */
          int                  lag*,  // [  4 ],               /* I    LTP lags                                    */
          float                Wght*, // [ 4 ],               /* I    Weights                                     */
          int                  subfr_length,                       /* I    Subframe length                             */
          int                  nb_subfr,                           /* I    number of subframes                         */
          int                  mem_offset                          /* I    Number of samples in LTP memory             */
)
{
    int   i, k;
    float b_ptr*, temp, WLTP_ptr*;
    float LPC_res_nrg, LPC_LTP_res_nrg;
    float d[ 4 ], m, g, delta_b[ 5 ];
    float w[ 4 ], nrg[ 4 ], regu;
    float Rr[ 5 ], rr[ 4 ];
    float *r_ptr, lag_ptr;

	clear rr, nrg, d, w, delta_b;
	
    b_ptr    = b;
    WLTP_ptr = WLTP;
    r_ptr    = &r_lpc[ mem_offset ];
    for( k = 0; k < nb_subfr; k++ ) {
        lag_ptr = r_ptr - ( lag[ k ] + 5 / 2 );

        silk_corrMatrix_FLP( lag_ptr, subfr_length, 5, WLTP_ptr );
        silk_corrVector_FLP( lag_ptr, r_ptr, subfr_length, 5, &Rr );

        rr[ k ] = ( float )silk_energy_FLP( r_ptr, subfr_length );
        regu = 1.0f + rr[ k ] +
            *(WLTP_ptr + ((0)*(5)+(0))) +
            *(WLTP_ptr + ((5-1)*(5)+(5-1)));
        regu *= 0.05f / 3.0;
        silk_regularize_correlations_FLP( WLTP_ptr, &rr[ k ], regu, 5 );
        silk_solve_LDL_FLP( WLTP_ptr, 5, &Rr, b_ptr );

        /* Calculate residual energy */
        nrg[ k ] = silk_residual_energy_covar_FLP( b_ptr, WLTP_ptr, &Rr, rr[ k ], 5 );

        temp = Wght[ k ] / ( nrg[ k ] * Wght[ k ] + 0.01f * (float)subfr_length );
        silk_scale_vector_FLP( WLTP_ptr, temp, 5 * 5 );
        w[ k ] = *(WLTP_ptr + ((5 / 2)*(5)+(5 / 2)));

        r_ptr    += subfr_length;
        b_ptr    += 5;
        WLTP_ptr += 5 * 5;
    }

    /* Compute LTP coding gain */
    if( LTPredCodGain != null) {
        LPC_LTP_res_nrg = 1.0e-6f;
        LPC_res_nrg     = 0.0f;
        for( k = 0; k < nb_subfr; k++ ) {
            LPC_res_nrg     += rr[  k ] * Wght[ k ];
            LPC_LTP_res_nrg += nrg[ k ] * Wght[ k ];
        }

        ;
        *LTPredCodGain = 3.0f * silk_log2( LPC_res_nrg / LPC_LTP_res_nrg );
    }

    /* Smoothing */
    /* d = sum( B, 1 ); */
    b_ptr = b;
    for( k = 0; k < nb_subfr; k++ ) {
        d[ k ] = 0.0;
        for( i = 0; i < 5; i++ ) {
            d[ k ] += b_ptr[ i ];
        }
        b_ptr += 5;
    }
    /* m = ( w * d' ) / ( sum( w ) + 1e-3 ); */
    temp = 1.0e-3f;
    for( k = 0; k < nb_subfr; k++ ) {
        temp += w[ k ];
    }
    m = 0.0;
    for( k = 0; k < nb_subfr; k++ ) {
        m += d[ k ] * w[ k ];
    }
    m = m / temp;

    b_ptr = b;
    for( k = 0; k < nb_subfr; k++ ) {
        g = 0.1f / ( 0.1f + w[ k ] ) * ( m - d[ k ] );
        temp = 0.0;
        for( i = 0; i < 5; i++ ) {
            delta_b[ i ] = (((b_ptr[ i ]) > (0.1f)) ? (b_ptr[ i ]) : (0.1f));
            temp += delta_b[ i ];
        }
        temp = g / temp;
        for( i = 0; i < 5; i++ ) {
            b_ptr[ i ] = b_ptr[ i ] + delta_b[ i ] * temp;
        }
        b_ptr += 5;
    }
}
#end unsafe
