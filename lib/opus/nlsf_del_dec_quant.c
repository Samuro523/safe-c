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

use opus_types, os_support;

/* Delayed-decision quantizer for NLSF residuals */
public
opus_int32 silk_NLSF_del_dec_quant(                             /* O    Returns RD value in Q25                     */
    int1                   indices*,                      /* O    Quantization indices [ order ]              */
          opus_int16            x_Q10*,                        /* I    Input [ order ]                             */
          opus_int16            w_Q5*,                         /* I    Weights [ order ]                           */
          byte            pred_coef_Q8*,                 /* I    Backward predictor coefs [ order ]          */
          opus_int16            ec_ix*,                        /* I    Indices to entropy coding tables [ order ]  */
          byte            ec_rates_Q5*,                  /* I    Rates []                                    */
          int              quant_step_size_Q16,            /* I    Quantization step size                      */
          opus_int16            inv_quant_step_size_Q6,         /* I    Inverse quantization step size              */
          opus_int32            mu_Q20,                         /* I    R/D tradeoff                                */
          opus_int16            order                           /* I    Number of input values                      */
)
{
    int         i, j, nStates, ind_tmp, ind_min_max, ind_max_min, in_Q10, res_Q10;
    int         pred_Q10, diff_Q10, out0_Q10, out1_Q10, rate0_Q5, rate1_Q5;
    opus_int32       RD_tmp_Q25, min_Q25, min_max_Q25, max_min_Q25, pred_coef_Q16;
    int         ind_sort[         ( 1 << 2 ) ];
    int1        ind[              ( 1 << 2 ) ][ 16 ];
    opus_int16       prev_out_Q10[ 2 * ( 1 << 2 ) ];
    opus_int32       RD_Q25[       2 * ( 1 << 2 ) ];
    opus_int32       RD_min_Q25[       ( 1 << 2 ) ];
    opus_int32       RD_max_Q25[       ( 1 << 2 ) ];
          byte *rates_Q5;

    ;     /* must be power of two */

	clear prev_out_Q10, RD_Q25, ind, RD_min_Q25, RD_max_Q25, ind_sort;
	
    nStates = 1;
    RD_Q25[ 0 ] = 0;
    prev_out_Q10[ 0 ] = 0;
    for( i = order - 1; ; i-- ) {
        rates_Q5 = &ec_rates_Q5[ ec_ix[ i ] ];
        pred_coef_Q16 = ((opus_int32)((opus_uint32)((opus_int32)pred_coef_Q8[ i ])<<(8)));
        in_Q10 = x_Q10[ i ];
        for( j = 0; j < nStates; j++ ) {
            pred_Q10 = ((((pred_coef_Q16) >> 16) * (opus_int32)((opus_int16)(prev_out_Q10[ j ]))) + ((((pred_coef_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(prev_out_Q10[ j ]))) >> 16));
            res_Q10  = ((in_Q10) - (pred_Q10));
            ind_tmp  = (((((opus_int32)inv_quant_step_size_Q6) >> 16) * (opus_int32)((opus_int16)(res_Q10))) + (((((opus_int32)inv_quant_step_size_Q6) & 0x0000FFFF) * (opus_int32)((opus_int16)(res_Q10))) >> 16));
            ind_tmp  = ((-10) > (10-1) ? ((ind_tmp) > (-10) ? (-10) : ((ind_tmp) < (10-1) ? (10-1) : (ind_tmp))) : ((ind_tmp) > (10-1) ? (10-1) : ((ind_tmp) < (-10) ? (-10) : (ind_tmp))));
            ind[ j ][ i ] = (int1)ind_tmp;

            /* compute outputs for ind_tmp and ind_tmp + 1 */
            out0_Q10 = ((opus_int32)((opus_uint32)(ind_tmp)<<(10)));
            out1_Q10 = ((out0_Q10) + (1024));
            if( ind_tmp > 0 ) {
                out0_Q10 = ((out0_Q10) - (((opus_int32)((0.1) * (float)((int8)1 << (10)) + 0.5))));
                out1_Q10 = ((out1_Q10) - (((opus_int32)((0.1) * (float)((int8)1 << (10)) + 0.5))));
            } else if( ind_tmp == 0 ) {
                out1_Q10 = ((out1_Q10) - (((opus_int32)((0.1) * (float)((int8)1 << (10)) + 0.5))));
            } else if( ind_tmp == -1 ) {
                out0_Q10 = ((out0_Q10) + (((opus_int32)((0.1) * (float)((int8)1 << (10)) + 0.5))));
            } else {
                out0_Q10 = ((out0_Q10) + (((opus_int32)((0.1) * (float)((int8)1 << (10)) + 0.5))));
                out1_Q10 = ((out1_Q10) + (((opus_int32)((0.1) * (float)((int8)1 << (10)) + 0.5))));
            }
            out0_Q10  = (((((opus_int32)out0_Q10) >> 16) * (opus_int32)((opus_int16)(quant_step_size_Q16))) + (((((opus_int32)out0_Q10) & 0x0000FFFF) * (opus_int32)((opus_int16)(quant_step_size_Q16))) >> 16));
            out1_Q10  = (((((opus_int32)out1_Q10) >> 16) * (opus_int32)((opus_int16)(quant_step_size_Q16))) + (((((opus_int32)out1_Q10) & 0x0000FFFF) * (opus_int32)((opus_int16)(quant_step_size_Q16))) >> 16));
            out0_Q10  = ((out0_Q10) + (pred_Q10));
            out1_Q10  = ((out1_Q10) + (pred_Q10));
            prev_out_Q10[ j           ] = (opus_int16)out0_Q10;
            prev_out_Q10[ j + nStates ] = (opus_int16)out1_Q10;

            /* compute RD for ind_tmp and ind_tmp + 1 */
            if( ind_tmp + 1 >= 4 ) {
                if( ind_tmp + 1 == 4 ) {
                    rate0_Q5 = rates_Q5[ ind_tmp + 4 ];
                    rate1_Q5 = 280;
                } else {
                    rate0_Q5 = ((280 - 43 * 4) + ((opus_int32)((opus_int16)(43))) * (opus_int32)((opus_int16)(ind_tmp)));
                    rate1_Q5 = ((rate0_Q5) + (43));
                }
            } else if( ind_tmp <= -4 ) {
                if( ind_tmp == -4 ) {
                    rate0_Q5 = 280;
                    rate1_Q5 = rates_Q5[ ind_tmp + 1 + 4 ];
                } else {
                    rate0_Q5 = ((280 - 43 * 4) + ((opus_int32)((opus_int16)(-43))) * (opus_int32)((opus_int16)(ind_tmp)));
                    rate1_Q5 = ((rate0_Q5) - (43));
                }
            } else {
                rate0_Q5 = rates_Q5[ ind_tmp +     4 ];
                rate1_Q5 = rates_Q5[ ind_tmp + 1 + 4 ];
            }
            RD_tmp_Q25            = RD_Q25[ j ];
            diff_Q10              = ((in_Q10) - (out0_Q10));
            RD_Q25[ j ]           = (((((RD_tmp_Q25)) + (((((opus_int32)((opus_int16)(diff_Q10)) * (opus_int32)((opus_int16)(diff_Q10)))) * (w_Q5[ i ]))))) + ((opus_int32)((opus_int16)(mu_Q20))) * (opus_int32)((opus_int16)(rate0_Q5)));
            diff_Q10              = ((in_Q10) - (out1_Q10));
            RD_Q25[ j + nStates ] = (((((RD_tmp_Q25)) + (((((opus_int32)((opus_int16)(diff_Q10)) * (opus_int32)((opus_int16)(diff_Q10)))) * (w_Q5[ i ]))))) + ((opus_int32)((opus_int16)(mu_Q20))) * (opus_int32)((opus_int16)(rate1_Q5)));
        }

        if( nStates < ( 1 << 2 ) ) {
            /* double number of states and copy */
            for( j = 0; j < nStates; j++ ) {
                ind[ j + nStates ][ i ] = (int1)(ind[ j ][ i ] + 1);
            }
            nStates = ((opus_int32)((opus_uint32)(nStates)<<(1)));
            for( j = nStates; j < ( 1 << 2 ); j++ ) {
                ind[ j ][ i ] = ind[ j - nStates ][ i ];
            }
        } else if( i > 0 ) {
            /* sort lower and upper half of RD_Q25, pairwise */
            for( j = 0; j < ( 1 << 2 ); j++ ) {
                if( RD_Q25[ j ] > RD_Q25[ j + ( 1 << 2 ) ] ) {
                    RD_max_Q25[ j ]                         = RD_Q25[ j ];
                    RD_min_Q25[ j ]                         = RD_Q25[ j + ( 1 << 2 ) ];
                    RD_Q25[ j ]                             = RD_min_Q25[ j ];
                    RD_Q25[ j + ( 1 << 2 ) ] = RD_max_Q25[ j ];
                    /* swap prev_out values */
                    out0_Q10 = prev_out_Q10[ j ];
                    prev_out_Q10[ j ] = prev_out_Q10[ j + ( 1 << 2 ) ];
                    prev_out_Q10[ j + ( 1 << 2 ) ] = (opus_int16)out0_Q10;
                    ind_sort[ j ] = j + ( 1 << 2 );
                } else {
                    RD_min_Q25[ j ] = RD_Q25[ j ];
                    RD_max_Q25[ j ] = RD_Q25[ j + ( 1 << 2 ) ];
                    ind_sort[ j ] = j;
                }
            }
            /* compare the highest RD values of the winning half with the lowest one in the losing half, and copy if necessary */
            /* afterwards ind_sort[] will contain the indices of the NLSF_QUANT_DEL_DEC_STATES winning RD values */
            while( true ) {
                min_max_Q25 = 0x7FFFFFFF;
                max_min_Q25 = 0;
                ind_min_max = 0;
                ind_max_min = 0;
                for( j = 0; j < ( 1 << 2 ); j++ ) {
                    if( min_max_Q25 > RD_max_Q25[ j ] ) {
                        min_max_Q25 = RD_max_Q25[ j ];
                        ind_min_max = j;
                    }
                    if( max_min_Q25 < RD_min_Q25[ j ] ) {
                        max_min_Q25 = RD_min_Q25[ j ];
                        ind_max_min = j;
                    }
                }
                if( min_max_Q25 >= max_min_Q25 ) {
                    break;
                }
                /* copy ind_min_max to ind_max_min */
                ind_sort[     ind_max_min ] = ind_sort[     ind_min_max ] ^ ( 1 << 2 );
                RD_Q25[       ind_max_min ] = RD_Q25[       ind_min_max + ( 1 << 2 ) ];
                prev_out_Q10[ ind_max_min ] = prev_out_Q10[ ind_min_max + ( 1 << 2 ) ];
                RD_min_Q25[   ind_max_min ] = 0;
                RD_max_Q25[   ind_min_max ] = 0x7FFFFFFF;
                memcpy((byte*)&(ind[ ind_max_min ]), (byte*)&(ind[ ind_min_max ]), (16 * ((int)(int1 ' size  ))));
            }
            /* increment index if it comes from the upper half */
            for( j = 0; j < ( 1 << 2 ); j++ ) {
                ind[ j ][ i ] += (int1)((ind_sort[ j ])>>(2));
            }
        } else {  /* i == 0 */
            break;
        }
    }

    /* last sample: find winner, copy indices and return RD value */
    ind_tmp = 0;
    min_Q25 = 0x7FFFFFFF;
    for( j = 0; j < 2 * ( 1 << 2 ); j++ ) {
        if( min_Q25 > RD_Q25[ j ] ) {
            min_Q25 = RD_Q25[ j ];
            ind_tmp = j;
        }
    }
    for( j = 0; j < order; j++ ) {
        indices[ j ] = ind[ ind_tmp & ( ( 1 << 2 ) - 1 ) ][ j ];
        ;
        ;
    }
    indices[ 0 ] += (int1)((ind_tmp)>>(2));
    ;
    ;
    return min_Q25;
}
#end unsafe
