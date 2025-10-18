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

use opus_types, lin2log, log2lin, sigproc_fix;


/* Gain scalar quantization with hysteresis, uniform on log scale */
public
void silk_gains_quant(
    int1                   ind*,  // [ 4 ],            /* O    gain indices                                */
    opus_int32                  gain_Q16*,  // [ 4 ],       /* I/O  gains (quantized out)                       */
    int1                   *prev_ind,                      /* I/O  last index in previous frame                */
          int              conditional,                    /* I    first gain is delta coded if 1              */
          int              nb_subfr                        /* I    number of subframes                         */
)
{
    int k, double_step_size_threshold;

    for( k = 0; k < nb_subfr; k++ ) {
        /* Convert to log scale, scale, floor() */
        ind[ k ] = (int1)(((((( ( 65536 * ( 64 - 1 ) ) / ( ( ( 88 - 2 ) * 128 ) / 6 ) )) >> 16) * (opus_int32)((opus_int16)(silk_lin2log( gain_Q16[ k ] ) - ( ( 2 * 128 ) / 6 + 16 * 128 )))) + ((((( ( 65536 * ( 64 - 1 ) ) / ( ( ( 88 - 2 ) * 128 ) / 6 ) )) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_lin2log( gain_Q16[ k ] ) - ( ( 2 * 128 ) / 6 + 16 * 128 )))) >> 16)));

        /* Round towards previous quantized gain (hysteresis) */
        if( ind[ k ] < *prev_ind ) {
            ind[ k ]++;
        }
        ind[ k ] = (int1)(((0) > (64 - 1) ? ((ind[ k ]) > (0) ? (0) : ((ind[ k ]) < (64 - 1) ? (64 - 1) : (ind[ k ]))) : ((ind[ k ]) > (64 - 1) ? (64 - 1) : ((ind[ k ]) < (0) ? (0) : (ind[ k ])))));

        /* Compute delta indices and limit */
        if( k == 0 && conditional == 0 ) {
            /* Full index */
            ind[ k ] = (int1)(((*prev_ind + -4) > (64 - 1) ? ((ind[ k ]) > (*prev_ind + -4) ? (*prev_ind + -4) : ((ind[ k ]) < (64 - 1) ? (64 - 1) : (ind[ k ]))) : ((ind[ k ]) > (64 - 1) ? (64 - 1) : ((ind[ k ]) < (*prev_ind + -4) ? (*prev_ind + -4) : (ind[ k ])))));
            *prev_ind = ind[ k ];
        } else {
            /* Delta index */
            ind[ k ] = (int1)(ind[ k ] - *prev_ind);

            /* Double the quantization step size for large gain increases, so that the max gain level can be reached */
            double_step_size_threshold = 2 * 36 - 64 + *prev_ind;
            if( ind[ k ] > double_step_size_threshold ) {
                ind[ k ] = (int1)(double_step_size_threshold + ((ind[ k ] - double_step_size_threshold + 1)>>(1)));
            }

            ind[ k ] = (int1)(((-4) > (36) ? ((ind[ k ]) > (-4) ? (-4) : ((ind[ k ]) < (36) ? (36) : (ind[ k ]))) : ((ind[ k ]) > (36) ? (36) : ((ind[ k ]) < (-4) ? (-4) : (ind[ k ])))));

            /* Accumulate deltas */
            if( ind[ k ] > double_step_size_threshold ) {
                *prev_ind += (int1)(((opus_int32)((opus_uint32)(ind[ k ])<<(1))) - double_step_size_threshold);
            } else {
                *prev_ind += ind[ k ];
            }

            /* Shift to make non-negative */
            ind[ k ] -= -4;
        }

        /* Scale and convert to linear scale */
        gain_Q16[ k ] = silk_log2lin( silk_min_32( ((((( ( 65536 * ( ( ( 88 - 2 ) * 128 ) / 6 ) ) / ( 64 - 1 ) )) >> 16) * (opus_int32)((opus_int16)(*prev_ind))) + ((((( ( 65536 * ( ( ( 88 - 2 ) * 128 ) / 6 ) ) / ( 64 - 1 ) )) & 0x0000FFFF) * (opus_int32)((opus_int16)(*prev_ind))) >> 16)) + ( ( 2 * 128 ) / 6 + 16 * 128 ), 3967 ) ); /* 3967 = 31 in Q7 */
    }
}

/* Gains scalar dequantization, uniform on log scale */
public
void silk_gains_dequant(
    opus_int32                  gain_Q16*,  // [ 4 ],       /* O    quantized gains                             */
          int1             ind*,    // [ 4 ],            /* I    gain indices                                */
    int1                   *prev_ind,                      /* I/O  last index in previous frame                */
          int              conditional,                    /* I    first gain is delta coded if 1              */
          int              nb_subfr                        /* I    number of subframes                          */
)
{
    int   k, ind_tmp, double_step_size_threshold;

    for( k = 0; k < nb_subfr; k++ ) {
        if( k == 0 && conditional == 0 ) {
            /* Gain index is not allowed to go down more than 16 steps (~21.8 dB) */
            *prev_ind = (int1)silk_max_int( ind[ k ], *prev_ind - 16 );
        } else {
            /* Delta index */
            ind_tmp = ind[ k ] + -4;

            /* Accumulate deltas */
            double_step_size_threshold = 2 * 36 - 64 + *prev_ind;
            if( ind_tmp > double_step_size_threshold ) {
                *prev_ind += (int1)(((opus_int32)((opus_uint32)(ind_tmp)<<(1))) - double_step_size_threshold);
            } else {
                *prev_ind += (int1)ind_tmp;
            }
        }
        *prev_ind = (int1)(((0) > (64 - 1) ? ((*prev_ind) > (0) ? (0) : ((*prev_ind) < (64 - 1) ? (64 - 1) : (*prev_ind))) : ((*prev_ind) > (64 - 1) ? (64 - 1) : ((*prev_ind) < (0) ? (0) : (*prev_ind)))));

        /* Scale and convert to linear scale */
        gain_Q16[ k ] = silk_log2lin( silk_min_32( ((((( ( 65536 * ( ( ( 88 - 2 ) * 128 ) / 6 ) ) / ( 64 - 1 ) )) >> 16) * (opus_int32)((opus_int16)(*prev_ind))) + ((((( ( 65536 * ( ( ( 88 - 2 ) * 128 ) / 6 ) ) / ( 64 - 1 ) )) & 0x0000FFFF) * (opus_int32)((opus_int16)(*prev_ind))) >> 16)) + ( ( 2 * 128 ) / 6 + 16 * 128 ), 3967 ) ); /* 3967 = 31 in Q7 */
    }
}

/* Compute unique identifier of gain indices vector */
public
opus_int32 silk_gains_ID(                                       /* O    returns unique identifier of gains          */
          int1             ind*,  // [ 4 ],            /* I    gain indices                                */
          int              nb_subfr                        /* I    number of subframes                         */
)
{
    int   k;
    opus_int32 gainsID;

    gainsID = 0;
    for( k = 0; k < nb_subfr; k++ ) {
        gainsID = (((ind[ k ])) + (((opus_int32)((opus_uint32)((gainsID))<<((8))))));
    }

    return gainsID;
}
#end unsafe
