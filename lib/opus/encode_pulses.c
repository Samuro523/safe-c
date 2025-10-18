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
use entcode, tables, entenc, shell_coder, code_signs;

/*********************************************/
/* Encode quantization indices of excitation */
/*********************************************/

public
int combine_and_check(    /* return ok                           */
    int         *pulses_comb,           /* O                                   */
          int   *pulses_in,             /* I                                   */
    int         max_pulses,             /* I    max value for sum of pulses    */
    int         len                     /* I    number of output values        */
)
{
    int k, sum;

    for( k = 0; k < len; k++ ) {
        sum = pulses_in[ 2 * k ] + pulses_in[ 2 * k + 1 ];
        if( sum > max_pulses ) {
            return 1;
        }
        pulses_comb[ k ] = sum;
    }

    return 0;
}


/* Encode quantization indices of excitation */
public
void silk_encode_pulses(
    ec_enc                      *psRangeEnc,                    /* I/O  compressor data structure                   */
          int              signalType,                     /* I    Signal type                                 */
          int              quantOffsetType,                /* I    quantOffsetType                             */
    int1                   pulses*,                       /* I    quantization indices                        */
          int              frame_length                    /* I    Frame length                                */
)
{
    int   i, k, j, iter, bit, nLS, scale_down, RateLevelIndex = 0;
    opus_int32 abs_q, minSumBits_Q5, sumBits_Q5;
    int   abs_pulses[ ( ( 5 * 4 ) * 16 ) ];
    int   sum_pulses[ ( ( ( 5 * 4 ) * 16 ) / 16 ) ];
    int   nRshifts[   ( ( ( 5 * 4 ) * 16 ) / 16 ) ];
    int   pulses_comb[ 8 ];
    int   *abs_pulses_ptr;
          int1 *pulses_ptr;
          byte *cdf_ptr;
          byte *nBits_ptr;

    clear sum_pulses, nRshifts;
    memset((char*)&(pulses_comb), (0), (8 * ((int)(int ' size  )))); /* Fixing Valgrind reported problem*/

    /****************************/
    /* Prepare for shell coding */
    /****************************/
    /* Calculate number of shell blocks */
    ;
    iter = ((frame_length)>>(4));
    if( iter * 16 < frame_length ) {
        ; /* Make sure only happens for 10 ms @ 12 kHz */
        iter++;
        memset((char*)(&pulses[ frame_length ]), (0), (16 * ((int)(int1 ' size  ))));
    }

    /* Take the absolute value of the pulses */
    for( i = 0; i < iter * 16; i+=4 ) {
        abs_pulses[i+0] = ( int )(((pulses[ i + 0 ]) > 0) ? (pulses[ i + 0 ]) : -(pulses[ i + 0 ]));
        abs_pulses[i+1] = ( int )(((pulses[ i + 1 ]) > 0) ? (pulses[ i + 1 ]) : -(pulses[ i + 1 ]));
        abs_pulses[i+2] = ( int )(((pulses[ i + 2 ]) > 0) ? (pulses[ i + 2 ]) : -(pulses[ i + 2 ]));
        abs_pulses[i+3] = ( int )(((pulses[ i + 3 ]) > 0) ? (pulses[ i + 3 ]) : -(pulses[ i + 3 ]));
    }

    /* Calc sum pulses per shell code frame */
    abs_pulses_ptr = &abs_pulses;
    for( i = 0; i < iter; i++ ) {
        nRshifts[ i ] = 0;

        while( true ) {
            /* 1+1 -> 2 */
            scale_down = combine_and_check( &pulses_comb, abs_pulses_ptr, silk_max_pulses_table[ 0 ], 8 );
            /* 2+2 -> 4 */
            scale_down += combine_and_check( &pulses_comb, &pulses_comb, silk_max_pulses_table[ 1 ], 4 );
            /* 4+4 -> 8 */
            scale_down += combine_and_check( &pulses_comb, &pulses_comb, silk_max_pulses_table[ 2 ], 2 );
            /* 8+8 -> 16 */
            scale_down += combine_and_check( &sum_pulses[ i ], &pulses_comb, silk_max_pulses_table[ 3 ], 1 );

            if( scale_down != 0 ) {
                /* We need to downscale the quantization signal */
                nRshifts[ i ]++;
                for( k = 0; k < 16; k++ ) {
                    abs_pulses_ptr[ k ] = ((abs_pulses_ptr[ k ])>>(1));
                }
            } else {
                /* Jump out of while(1) loop and go to next shell coding frame */
                break;
            }
        }
        abs_pulses_ptr += 16;
    }

    /**************/
    /* Rate level */
    /**************/
    /* find rate level that leads to fewest bits for coding of pulses per block info */
    minSumBits_Q5 = 0x7FFFFFFF;
    for( k = 0; k < 10 - 1; k++ ) {
        nBits_ptr  = &silk_pulses_per_block_BITS_Q5[ k ];
        sumBits_Q5 = silk_rate_levels_BITS_Q5[ signalType >> 1 ][ k ];
        for( i = 0; i < iter; i++ ) {
            if( nRshifts[ i ] > 0 ) {
                sumBits_Q5 += nBits_ptr[ 16 + 1 ];
            } else {
                sumBits_Q5 += nBits_ptr[ sum_pulses[ i ] ];
            }
        }
        if( sumBits_Q5 < minSumBits_Q5 ) {
            minSumBits_Q5 = sumBits_Q5;
            RateLevelIndex = k;
        }
    }
    ec_enc_icdf( psRangeEnc, RateLevelIndex, &silk_rate_levels_iCDF[ signalType >> 1 ], 8 );

    /***************************************************/
    /* Sum-Weighted-Pulses Encoding                    */
    /***************************************************/
    cdf_ptr = &silk_pulses_per_block_iCDF[ RateLevelIndex ];
    for( i = 0; i < iter; i++ ) {
        if( nRshifts[ i ] == 0 ) {
            ec_enc_icdf( psRangeEnc, sum_pulses[ i ], cdf_ptr, 8 );
        } else {
            ec_enc_icdf( psRangeEnc, 16 + 1, cdf_ptr, 8 );
            for( k = 0; k < nRshifts[ i ] - 1; k++ ) {
                ec_enc_icdf( psRangeEnc, 16 + 1, &silk_pulses_per_block_iCDF[ 10 - 1 ], 8 );
            }
            ec_enc_icdf( psRangeEnc, sum_pulses[ i ], &silk_pulses_per_block_iCDF[ 10 - 1 ], 8 );
        }
    }

    /******************/
    /* Shell Encoding */
    /******************/
    for( i = 0; i < iter; i++ ) {
        if( sum_pulses[ i ] > 0 ) {
            silk_shell_encoder( psRangeEnc, &abs_pulses[ i * 16 ] );
        }
    }

    /****************/
    /* LSB Encoding */
    /****************/
    for( i = 0; i < iter; i++ ) {
        if( nRshifts[ i ] > 0 ) {
            pulses_ptr = &pulses[ i * 16 ];
            nLS = nRshifts[ i ] - 1;
            for( k = 0; k < 16; k++ ) {
                abs_q = (int1)(((pulses_ptr[ k ]) > 0) ? (pulses_ptr[ k ]) : -(pulses_ptr[ k ]));
                for( j = nLS; j > 0; j-- ) {
                    bit = ((abs_q)>>(j)) & 1;
                    ec_enc_icdf( psRangeEnc, bit, &silk_lsb_iCDF, 8 );
                }
                bit = abs_q & 1;
                ec_enc_icdf( psRangeEnc, bit, &silk_lsb_iCDF, 8 );
            }
        }
    }

    /****************/
    /* Encode signs */
    /****************/
    silk_encode_signs( psRangeEnc, pulses, frame_length, signalType, quantOffsetType, sum_pulses );
}
#end unsafe
