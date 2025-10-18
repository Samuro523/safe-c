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

use lpc_analysis_filter_flp, energy_flp;

/* Residual energy: nrg = wxx - 2 * wXx * c + c' * wXX * c */
public
float silk_residual_energy_covar_FLP(                              /* O    Weighted residual energy                    */
          float                *c,                                 /* I    Filter coefficients                         */
    float                      *wXX,                               /* I/O  Weighted correlation matrix, reg. out       */
          float                *wXx,                               /* I    Weighted correlation vector                 */
          float                wxx,                                /* I    Weighted correlation value                  */
          int                  D                                   /* I    Dimension                                   */
)
{
    int   i, j, k;
    float tmp, nrg = 0.0f, regularization;

    /* Safety checks */
    ;

    regularization = 1.0e-8f * ( wXX[ 0 ] + wXX[ D * D - 1 ] );
    for( k = 0; k < 10; k++ ) {
        nrg = wxx;

        tmp = 0.0f;
        for( i = 0; i < D; i++ ) {
            tmp += wXx[ i ] * c[ i ];
        }
        nrg -= 2.0f * tmp;

        /* compute c' * wXX * c, assuming wXX is symmetric */
        for( i = 0; i < D; i++ ) {
            tmp = 0.0f;
            for( j = i + 1; j < D; j++ ) {
                tmp += *(wXX + ((i)+(D)*(j))) * c[ j ];
            }
            nrg += c[ i ] * ( 2.0f * tmp + *(wXX + ((i)+(D)*(i))) * c[ i ] );
        }
        if( nrg > 0.0 ) {
            break;
        } else {
            /* Add white noise */
            for( i = 0; i < D; i++ ) {
                *(wXX + ((i)+(D)*(i))) +=  regularization;
            }
            /* Increase noise for next run */
            regularization *= 2.0f;
        }
    }
    if( k == 10 ) {
        ;
        nrg = 1.0f;
    }

    return nrg;
}

/* Calculates residual energies of input subframes where all subframes have LPC_order   */
/* of preceeding samples                                                                */
public
void silk_residual_energy_FLP(
    float                      nrgs*, //[ 4 ],               /* O    Residual energy per subframe                */
          float                x*,                                /* I    Input signal                                */
    float                      a*[ 16 ],            /* I    AR coefs for each frame half                */
          float                gains*,                            /* I    Quantization gains                          */
          int                  subfr_length,                       /* I    Subframe length                             */
          int                  nb_subfr,                           /* I    number of subframes                         */
          int                  LPC_order                           /* I    LPC order                                   */
)
{
    int     shift;
    float   LPC_res_ptr*, LPC_res[ ( ( ( 5 * 4 ) * 16 ) + 4 * 16 ) / 2 ];

    LPC_res_ptr = &LPC_res + LPC_order;
    shift = LPC_order + subfr_length;

    /* Filter input to create the LPC residual for each frame half, and measure subframe energies */
    silk_LPC_analysis_filter_FLP( &LPC_res, &a[ 0 ], x + 0 * shift, 2 * shift, LPC_order );
    nrgs[ 0 ] = ( float )( gains[ 0 ] * gains[ 0 ] * silk_energy_FLP( LPC_res_ptr + 0 * shift, subfr_length ) );
    nrgs[ 1 ] = ( float )( gains[ 1 ] * gains[ 1 ] * silk_energy_FLP( LPC_res_ptr + 1 * shift, subfr_length ) );

    if( nb_subfr == 4 ) {
        silk_LPC_analysis_filter_FLP( &LPC_res, &a[ 1 ], x + 2 * shift, 2 * shift, LPC_order );
        nrgs[ 2 ] = ( float )( gains[ 2 ] * gains[ 2 ] * silk_energy_FLP( LPC_res_ptr + 0 * shift, subfr_length ) );
        nrgs[ 3 ] = ( float )( gains[ 3 ] * gains[ 3 ] * silk_energy_FLP( LPC_res_ptr + 1 * shift, subfr_length ) );
    }
}
#end unsafe
