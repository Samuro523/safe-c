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

use opus_types, structs, os_support, stereo_find_predictor, inlines;
use stereo_quant_pred, sigproc_fix;

/* Convert Left/Right stereo signal to adaptive Mid/Side representation */
public
void silk_stereo_LR_to_MS(
    stereo_enc_state            *state,                         /* I/O  State                                       */
    opus_int16                  x1*,                           /* I/O  Left input signal, becomes mid signal       */
    opus_int16                  x2*,                           /* I/O  Right input signal, becomes side signal     */
    int1                   ix*[ 3 ],                   /* O    Quantization indices                        */
    int1                   *mid_only_flag,                 /* O    Flag: only mid signal coded                 */
    opus_int32                  mid_side_rates_bps*,           /* O    Bitrates for mid and side signals           */
    opus_int32                  total_rate_bps,                 /* I    Total bitrate                               */
    int                    prev_speech_act_Q8,             /* I    Speech activity level in previous frame     */
    int                    toMono,                         /* I    Last frame before a stereo->mono transition */
    int                    fs_kHz,                         /* I    Sample rate (kHz)                           */
    int                    frame_length                    /* I    Number of samples                           */
)
{
    int   n, is10msFrame, denom_Q16, delta0_Q13, delta1_Q13;
    opus_int32 sum, diff, smooth_coef_Q16, pred_Q13[ 2 ], pred0_Q13, pred1_Q13;
    opus_int32 LP_ratio_Q14, HP_ratio_Q14, frac_Q16, frac_3_Q16, min_mid_rate_bps, width_Q14, w_Q24, deltaw_Q24;
    opus_int16 side[ ( ( 5 * 4 ) * 16 ) + 2 ];
    opus_int16 LP_mid[  ( ( 5 * 4 ) * 16 ) ], HP_mid[  ( ( 5 * 4 ) * 16 ) ];
    opus_int16 LP_side[ ( ( 5 * 4 ) * 16 ) ], HP_side[ ( ( 5 * 4 ) * 16 ) ];
    opus_int16 *mid = &x1[ -2 ];

	clear pred_Q13;
	
    /* Convert to basic mid/side signals */
    for( n = 0; n < frame_length + 2; n++ ) {
        sum  = x1[ n - 2 ] + (opus_int32)x2[ n - 2 ];
        diff = x1[ n - 2 ] - (opus_int32)x2[ n - 2 ];
        mid[  n ] = (opus_int16)((1) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((1) - 1)) + 1) >> 1);
        side[ n ] = (opus_int16)((((1) == 1 ? ((diff) >> 1) + ((diff) & 1) : (((diff) >> ((1) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((1) == 1 ? ((diff) >> 1) + ((diff) & 1) : (((diff) >> ((1) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((1) == 1 ? ((diff) >> 1) + ((diff) & 1) : (((diff) >> ((1) - 1)) + 1) >> 1))));
    }

    /* Buffering */
    memcpy((byte*)(mid), (byte*)&(state->sMid), (2 * ((int)(opus_int16 ' size  ))));
    memcpy((byte*)&(side), (byte*)&(state->sSide), (2 * ((int)(opus_int16 ' size  ))));
    memcpy((byte*)&(state->sMid), (byte*)(&mid[ frame_length ]), (2 * ((int)(opus_int16 ' size  ))));
    memcpy((byte*)&(state->sSide), (byte*)(&side[ frame_length ]), (2 * ((int)(opus_int16 ' size  ))));

    /* LP and HP filter mid signal */
    for( n = 0; n < frame_length; n++ ) {
        sum = ((2) == 1 ? ((((mid[ n ] + mid[ n + 2 ]) + ((opus_int32)((opus_uint32)((mid[ n + 1 ]))<<((1)))))) >> 1) + ((((mid[ n ] + mid[ n + 2 ]) + ((opus_int32)((opus_uint32)((mid[ n + 1 ]))<<((1)))))) & 1) : (((((mid[ n ] + mid[ n + 2 ]) + ((opus_int32)((opus_uint32)((mid[ n + 1 ]))<<((1)))))) >> ((2) - 1)) + 1) >> 1);
        LP_mid[ n ] = (opus_int16)sum;
        HP_mid[ n ] = (opus_int16)(mid[ n + 1 ] - sum);
    }

    /* LP and HP filter side signal */
    for( n = 0; n < frame_length; n++ ) {
        sum = ((2) == 1 ? ((((side[ n ] + side[ n + 2 ]) + ((opus_int32)((opus_uint32)((side[ n + 1 ]))<<((1)))))) >> 1) + ((((side[ n ] + side[ n + 2 ]) + ((opus_int32)((opus_uint32)((side[ n + 1 ]))<<((1)))))) & 1) : (((((side[ n ] + side[ n + 2 ]) + ((opus_int32)((opus_uint32)((side[ n + 1 ]))<<((1)))))) >> ((2) - 1)) + 1) >> 1);
        LP_side[ n ] = (opus_int16)sum;
        HP_side[ n ] = (opus_int16)(side[ n + 1 ] - sum);
    }

    /* Find energies and predictors */
    is10msFrame = (int)(frame_length == 10 * fs_kHz);
    smooth_coef_Q16 = is10msFrame!=0 ?
        ((opus_int32)((0.01 / 2.0) * (float)((int8)1 << (16)) + 0.5)) :
        ((opus_int32)((0.01) * (float)((int8)1 << (16)) + 0.5));
    smooth_coef_Q16 = ((((((opus_int32)((opus_int16)(prev_speech_act_Q8)) * (opus_int32)((opus_int16)(prev_speech_act_Q8)))) >> 16) * (opus_int32)((opus_int16)(smooth_coef_Q16))) + ((((((opus_int32)((opus_int16)(prev_speech_act_Q8)) * (opus_int32)((opus_int16)(prev_speech_act_Q8)))) & 0x0000FFFF) * (opus_int32)((opus_int16)(smooth_coef_Q16))) >> 16));

    pred_Q13[ 0 ] = silk_stereo_find_predictor( &LP_ratio_Q14, &LP_mid, &LP_side, &state->mid_side_amp_Q0[ 0 ], frame_length, smooth_coef_Q16 );
    pred_Q13[ 1 ] = silk_stereo_find_predictor( &HP_ratio_Q14, &HP_mid, &HP_side, &state->mid_side_amp_Q0[ 2 ], frame_length, smooth_coef_Q16 );
    /* Ratio of the norms of residual and mid signals */
    frac_Q16 = ((HP_ratio_Q14) + ((opus_int32)((opus_int16)(LP_ratio_Q14))) * (opus_int32)((opus_int16)(3)));
    frac_Q16 = (((frac_Q16) < (((opus_int32)((1.0) * (float)((int8)1 << (16)) + 0.5)))) ? (frac_Q16) : (((opus_int32)((1.0) * (float)((int8)1 << (16)) + 0.5))));

    /* Determine bitrate distribution between mid and side, and possibly reduce stereo width */
    *&total_rate_bps -= is10msFrame != 0 ? 1200 : 600;      /* Subtract approximate bitrate for coding stereo parameters */
    if( total_rate_bps < 1 ) {
        *&total_rate_bps = 1;
    }
    min_mid_rate_bps = ((2000) + ((opus_int32)((opus_int16)(fs_kHz))) * (opus_int32)((opus_int16)(900)));
    ;
    /* Default bitrate distribution: 8 parts for Mid and (5+3*frac) parts for Side. so: mid_rate = ( 8 / ( 13 + 3 * frac ) ) * total_ rate */
    frac_3_Q16 = ((3) * (frac_Q16));
    mid_side_rates_bps[ 0 ] = silk_DIV32_varQ( total_rate_bps, ((opus_int32)((8.0 + 5.0) * (float)((int8)1 << (16)) + 0.5)) + frac_3_Q16, 16+3 );
    /* If Mid bitrate below minimum, reduce stereo width */
    if( mid_side_rates_bps[ 0 ] < min_mid_rate_bps ) {
        mid_side_rates_bps[ 0 ] = min_mid_rate_bps;
        mid_side_rates_bps[ 1 ] = total_rate_bps - mid_side_rates_bps[ 0 ];
        /* width = 4 * ( 2 * side_rate - min_rate ) / ( ( 1 + 3 * frac ) * min_rate ) */
        width_Q14 = silk_DIV32_varQ( ((opus_int32)((opus_uint32)(mid_side_rates_bps[ 1 ])<<(1))) - min_mid_rate_bps,
            ((((((opus_int32)((1.0) * (float)((int8)1 << (16)) + 0.5)) + frac_3_Q16) >> 16) * (opus_int32)((opus_int16)(min_mid_rate_bps))) + ((((((opus_int32)((1.0) * (float)((int8)1 << (16)) + 0.5)) + frac_3_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(min_mid_rate_bps))) >> 16)), 14+2 );
        width_Q14 = ((0) > (((opus_int32)((1.0) * (float)((int8)1 << (14)) + 0.5))) ? ((width_Q14) > (0) ? (0) : ((width_Q14) < (((opus_int32)((1.0) * (float)((int8)1 << (14)) + 0.5))) ? (((opus_int32)((1.0) * (float)((int8)1 << (14)) + 0.5))) : (width_Q14))) : ((width_Q14) > (((opus_int32)((1.0) * (float)((int8)1 << (14)) + 0.5))) ? (((opus_int32)((1.0) * (float)((int8)1 << (14)) + 0.5))) : ((width_Q14) < (0) ? (0) : (width_Q14))));
    } else {
        mid_side_rates_bps[ 1 ] = total_rate_bps - mid_side_rates_bps[ 0 ];
        width_Q14 = ((opus_int32)((1.0) * (float)((int8)1 << (14)) + 0.5));
    }

    /* Smoother */
    state->smth_width_Q14 = (opus_int16)((state->smth_width_Q14) + ((((width_Q14 - state->smth_width_Q14) >> 16) * (opus_int32)((opus_int16)(smooth_coef_Q16))) + ((((width_Q14 - state->smth_width_Q14) & 0x0000FFFF) * (opus_int32)((opus_int16)(smooth_coef_Q16))) >> 16)));

    /* At very low bitrates or for inputs that are nearly amplitude panned, switch to panned-mono coding */
    *mid_only_flag = 0;
    if( toMono != 0 ) {
        /* Last frame before stereo->mono transition; collapse stereo width */
        width_Q14 = 0;
        pred_Q13[ 0 ] = 0;
        pred_Q13[ 1 ] = 0;
        silk_stereo_quant_pred( &pred_Q13, ix );
    } else if( state->width_prev_Q14 == 0 &&
        ( 8 * total_rate_bps < 13 * min_mid_rate_bps || ((((frac_Q16) >> 16) * (opus_int32)((opus_int16)(state->smth_width_Q14))) + ((((frac_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(state->smth_width_Q14))) >> 16)) < ((opus_int32)((0.05) * (float)((int8)1 << (14)) + 0.5)) ) )
    {
        /* Code as panned-mono; previous frame already had zero width */
        /* Scale down and quantize predictors */
        pred_Q13[ 0 ] = ((((opus_int32)((opus_int16)(state->smth_width_Q14)) * (opus_int32)((opus_int16)(pred_Q13[ 0 ]))))>>(14));
        pred_Q13[ 1 ] = ((((opus_int32)((opus_int16)(state->smth_width_Q14)) * (opus_int32)((opus_int16)(pred_Q13[ 1 ]))))>>(14));
        silk_stereo_quant_pred( &pred_Q13, ix );
        /* Collapse stereo width */
        width_Q14 = 0;
        pred_Q13[ 0 ] = 0;
        pred_Q13[ 1 ] = 0;
        mid_side_rates_bps[ 0 ] = total_rate_bps;
        mid_side_rates_bps[ 1 ] = 0;
        *mid_only_flag = 1;
    } else if( state->width_prev_Q14 != 0 &&
        ( 8 * total_rate_bps < 11 * min_mid_rate_bps || ((((frac_Q16) >> 16) * (opus_int32)((opus_int16)(state->smth_width_Q14))) + ((((frac_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(state->smth_width_Q14))) >> 16)) < ((opus_int32)((0.02) * (float)((int8)1 << (14)) + 0.5)) ) )
    {
        /* Transition to zero-width stereo */
        /* Scale down and quantize predictors */
        pred_Q13[ 0 ] = ((((opus_int32)((opus_int16)(state->smth_width_Q14)) * (opus_int32)((opus_int16)(pred_Q13[ 0 ]))))>>(14));
        pred_Q13[ 1 ] = ((((opus_int32)((opus_int16)(state->smth_width_Q14)) * (opus_int32)((opus_int16)(pred_Q13[ 1 ]))))>>(14));
        silk_stereo_quant_pred( &pred_Q13, ix );
        /* Collapse stereo width */
        width_Q14 = 0;
        pred_Q13[ 0 ] = 0;
        pred_Q13[ 1 ] = 0;
    } else if( state->smth_width_Q14 > ((opus_int32)((0.95) * (float)((int8)1 << (14)) + 0.5)) ) {
        /* Full-width stereo coding */
        silk_stereo_quant_pred( &pred_Q13, ix );
        width_Q14 = ((opus_int32)((1.0) * (float)((int8)1 << (14)) + 0.5));
    } else {
        /* Reduced-width stereo coding; scale down and quantize predictors */
        pred_Q13[ 0 ] = ((((opus_int32)((opus_int16)(state->smth_width_Q14)) * (opus_int32)((opus_int16)(pred_Q13[ 0 ]))))>>(14));
        pred_Q13[ 1 ] = ((((opus_int32)((opus_int16)(state->smth_width_Q14)) * (opus_int32)((opus_int16)(pred_Q13[ 1 ]))))>>(14));
        silk_stereo_quant_pred( &pred_Q13, ix );
        width_Q14 = state->smth_width_Q14;
    }

    /* Make sure to keep on encoding until the tapered output has been transmitted */
    if( *mid_only_flag == 1 ) {
        state->silent_side_len += (opus_int16)(frame_length - 8 * fs_kHz);
        if( state->silent_side_len < 5 * fs_kHz ) {
            *mid_only_flag = 0;
        } else {
            /* Limit to avoid wrapping around */
            state->silent_side_len = 10000;
        }
    } else {
        state->silent_side_len = 0;
    }

    if( *mid_only_flag == 0 && mid_side_rates_bps[ 1 ] < 1 ) {
        mid_side_rates_bps[ 1 ] = 1;
        mid_side_rates_bps[ 0 ] = silk_max_int( 1, total_rate_bps - mid_side_rates_bps[ 1 ]);
    }

    /* Interpolate predictors and subtract prediction from side channel */
    pred0_Q13  = -state->pred_prev_Q13[ 0 ];
    pred1_Q13  = -state->pred_prev_Q13[ 1 ];
    w_Q24      =  ((opus_int32)((opus_uint32)(state->width_prev_Q14)<<(10)));
    denom_Q16  = ((opus_int32)(((opus_int32)1 << 16) / (8 * fs_kHz)));
    delta0_Q13 = -((16) == 1 ? ((((opus_int32)((opus_int16)(pred_Q13[ 0 ] - state->pred_prev_Q13[ 0 ])) * (opus_int32)((opus_int16)(denom_Q16)))) >> 1) + ((((opus_int32)((opus_int16)(pred_Q13[ 0 ] - state->pred_prev_Q13[ 0 ])) * (opus_int32)((opus_int16)(denom_Q16)))) & 1) : (((((opus_int32)((opus_int16)(pred_Q13[ 0 ] - state->pred_prev_Q13[ 0 ])) * (opus_int32)((opus_int16)(denom_Q16)))) >> ((16) - 1)) + 1) >> 1);
    delta1_Q13 = -((16) == 1 ? ((((opus_int32)((opus_int16)(pred_Q13[ 1 ] - state->pred_prev_Q13[ 1 ])) * (opus_int32)((opus_int16)(denom_Q16)))) >> 1) + ((((opus_int32)((opus_int16)(pred_Q13[ 1 ] - state->pred_prev_Q13[ 1 ])) * (opus_int32)((opus_int16)(denom_Q16)))) & 1) : (((((opus_int32)((opus_int16)(pred_Q13[ 1 ] - state->pred_prev_Q13[ 1 ])) * (opus_int32)((opus_int16)(denom_Q16)))) >> ((16) - 1)) + 1) >> 1);
    deltaw_Q24 =  ((opus_int32)((opus_uint32)(((((width_Q14 - state->width_prev_Q14) >> 16) * (opus_int32)((opus_int16)(denom_Q16))) + ((((width_Q14 - state->width_prev_Q14) & 0x0000FFFF) * (opus_int32)((opus_int16)(denom_Q16))) >> 16)))<<(10)));
    for( n = 0; n < 8 * fs_kHz; n++ ) {
        pred0_Q13 += delta0_Q13;
        pred1_Q13 += delta1_Q13;
        w_Q24   += deltaw_Q24;
        sum = ((opus_int32)((opus_uint32)(((mid[ n ] + mid[ n + 2 ]) + ((opus_int32)((opus_uint32)((mid[ n + 1 ]))<<((1))))))<<(9)));    /* Q11 */
        sum = ((((((w_Q24) >> 16) * (opus_int32)((opus_int16)(side[ n + 1 ]))) + ((((w_Q24) & 0x0000FFFF) * (opus_int32)((opus_int16)(side[ n + 1 ]))) >> 16))) + ((((sum) >> 16) * (opus_int32)((opus_int16)(pred0_Q13))) + ((((sum) & 0x0000FFFF) * (opus_int32)((opus_int16)(pred0_Q13))) >> 16)));               /* Q8  */
        sum = ((sum) + ((((((opus_int32)((opus_uint32)((opus_int32)mid[ n + 1 ])<<(11)))) >> 16) * (opus_int32)((opus_int16)(pred1_Q13))) + ((((((opus_int32)((opus_uint32)((opus_int32)mid[ n + 1 ])<<(11)))) & 0x0000FFFF) * (opus_int32)((opus_int16)(pred1_Q13))) >> 16)));       /* Q8  */
        x2[ n - 1 ] = (opus_int16)((((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1))));
    }

    pred0_Q13 = -pred_Q13[ 0 ];
    pred1_Q13 = -pred_Q13[ 1 ];
    w_Q24     =  ((opus_int32)((opus_uint32)(width_Q14)<<(10)));
    for( n = 8 * fs_kHz; n < frame_length; n++ ) {
        sum = ((opus_int32)((opus_uint32)(((mid[ n ] + mid[ n + 2 ]) + ((opus_int32)((opus_uint32)((mid[ n + 1 ]))<<((1))))))<<(9)));    /* Q11 */
        sum = ((((((w_Q24) >> 16) * (opus_int32)((opus_int16)(side[ n + 1 ]))) + ((((w_Q24) & 0x0000FFFF) * (opus_int32)((opus_int16)(side[ n + 1 ]))) >> 16))) + ((((sum) >> 16) * (opus_int32)((opus_int16)(pred0_Q13))) + ((((sum) & 0x0000FFFF) * (opus_int32)((opus_int16)(pred0_Q13))) >> 16)));               /* Q8  */
        sum = ((sum) + ((((((opus_int32)((opus_uint32)((opus_int32)mid[ n + 1 ])<<(11)))) >> 16) * (opus_int32)((opus_int16)(pred1_Q13))) + ((((((opus_int32)((opus_uint32)((opus_int32)mid[ n + 1 ])<<(11)))) & 0x0000FFFF) * (opus_int32)((opus_int16)(pred1_Q13))) >> 16)));       /* Q8  */
        x2[ n - 1 ] = (opus_int16)((((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1))));
    }
    state->pred_prev_Q13[ 0 ] = (opus_int16)pred_Q13[ 0 ];
    state->pred_prev_Q13[ 1 ] = (opus_int16)pred_Q13[ 1 ];
    state->width_prev_Q14     = (opus_int16)width_Q14;
}
#end unsafe
