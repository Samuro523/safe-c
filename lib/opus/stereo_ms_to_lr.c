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

use opus_types, structs, os_support;

/* Convert adaptive Mid/Side representation to Left/Right stereo signal */
public
void silk_stereo_MS_to_LR(
    stereo_dec_state            *state,                         /* I/O  State                                       */
    opus_int16                  x1*,                           /* I/O  Left input signal, becomes mid signal       */
    opus_int16                  x2*,                           /* I/O  Right input signal, becomes side signal     */
    opus_int32            pred_Q13*,                     /* I    Predictors                                  */
    int                    fs_kHz,                         /* I    Samples rate (kHz)                          */
    int                    frame_length                    /* I    Number of samples                           */
)
{
    int   n, denom_Q16, delta0_Q13, delta1_Q13;
    opus_int32 sum, diff, pred0_Q13, pred1_Q13;

    /* Buffering */
    memcpy((byte*)(x1), (byte*)&(state->sMid), (2 * ((int)(opus_int16 ' size  ))));
    memcpy((byte*)(x2), (byte*)&(state->sSide), (2 * ((int)(opus_int16 ' size  ))));
    memcpy((byte*)&(state->sMid), (byte*)(&x1[ frame_length ]), (2 * ((int)(opus_int16 ' size  ))));
    memcpy((byte*)&(state->sSide), (byte*)(&x2[ frame_length ]), (2 * ((int)(opus_int16 ' size  ))));

    /* Interpolate predictors and add prediction to side channel */
    pred0_Q13  = state->pred_prev_Q13[ 0 ];
    pred1_Q13  = state->pred_prev_Q13[ 1 ];
    denom_Q16  = ((opus_int32)(((opus_int32)1 << 16) / (8 * fs_kHz)));
    delta0_Q13 = ((16) == 1 ? ((((opus_int32)((opus_int16)(pred_Q13[ 0 ] - state->pred_prev_Q13[ 0 ])) * (opus_int32)((opus_int16)(denom_Q16)))) >> 1) + ((((opus_int32)((opus_int16)(pred_Q13[ 0 ] - state->pred_prev_Q13[ 0 ])) * (opus_int32)((opus_int16)(denom_Q16)))) & 1) : (((((opus_int32)((opus_int16)(pred_Q13[ 0 ] - state->pred_prev_Q13[ 0 ])) * (opus_int32)((opus_int16)(denom_Q16)))) >> ((16) - 1)) + 1) >> 1);
    delta1_Q13 = ((16) == 1 ? ((((opus_int32)((opus_int16)(pred_Q13[ 1 ] - state->pred_prev_Q13[ 1 ])) * (opus_int32)((opus_int16)(denom_Q16)))) >> 1) + ((((opus_int32)((opus_int16)(pred_Q13[ 1 ] - state->pred_prev_Q13[ 1 ])) * (opus_int32)((opus_int16)(denom_Q16)))) & 1) : (((((opus_int32)((opus_int16)(pred_Q13[ 1 ] - state->pred_prev_Q13[ 1 ])) * (opus_int32)((opus_int16)(denom_Q16)))) >> ((16) - 1)) + 1) >> 1);
    for( n = 0; n < 8 * fs_kHz; n++ ) {
        pred0_Q13 += delta0_Q13;
        pred1_Q13 += delta1_Q13;
        sum = ((opus_int32)((opus_uint32)(((x1[ n ] + x1[ n + 2 ]) + ((opus_int32)((opus_uint32)((x1[ n + 1 ]))<<((1))))))<<(9)));       /* Q11 */
        sum = ((((opus_int32)((opus_uint32)((opus_int32)x2[ n + 1 ])<<(8)))) + ((((sum) >> 16) * (opus_int32)((opus_int16)(pred0_Q13))) + ((((sum) & 0x0000FFFF) * (opus_int32)((opus_int16)(pred0_Q13))) >> 16)));         /* Q8  */
        sum = ((sum) + ((((((opus_int32)((opus_uint32)((opus_int32)x1[ n + 1 ])<<(11)))) >> 16) * (opus_int32)((opus_int16)(pred1_Q13))) + ((((((opus_int32)((opus_uint32)((opus_int32)x1[ n + 1 ])<<(11)))) & 0x0000FFFF) * (opus_int32)((opus_int16)(pred1_Q13))) >> 16)));        /* Q8  */
        x2[ n + 1 ] = (opus_int16)((((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1))));
    }
    pred0_Q13 = pred_Q13[ 0 ];
    pred1_Q13 = pred_Q13[ 1 ];
    for( n = 8 * fs_kHz; n < frame_length; n++ ) {
        sum = ((opus_int32)((opus_uint32)(((x1[ n ] + x1[ n + 2 ]) + ((opus_int32)((opus_uint32)((x1[ n + 1 ]))<<((1))))))<<(9)));       /* Q11 */
        sum = ((((opus_int32)((opus_uint32)((opus_int32)x2[ n + 1 ])<<(8)))) + ((((sum) >> 16) * (opus_int32)((opus_int16)(pred0_Q13))) + ((((sum) & 0x0000FFFF) * (opus_int32)((opus_int16)(pred0_Q13))) >> 16)));         /* Q8  */
        sum = ((sum) + ((((((opus_int32)((opus_uint32)((opus_int32)x1[ n + 1 ])<<(11)))) >> 16) * (opus_int32)((opus_int16)(pred1_Q13))) + ((((((opus_int32)((opus_uint32)((opus_int32)x1[ n + 1 ])<<(11)))) & 0x0000FFFF) * (opus_int32)((opus_int16)(pred1_Q13))) >> 16)));        /* Q8  */
        x2[ n + 1 ] = (opus_int16)((((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((8) == 1 ? ((sum) >> 1) + ((sum) & 1) : (((sum) >> ((8) - 1)) + 1) >> 1))));
    }
    state->pred_prev_Q13[ 0 ] = (opus_int16)pred_Q13[ 0 ];
    state->pred_prev_Q13[ 1 ] = (opus_int16)pred_Q13[ 1 ];

    /* Convert to left/right signals */
    for( n = 0; n < frame_length; n++ ) {
        sum  = x1[ n + 1 ] + (opus_int32)x2[ n + 1 ];
        diff = x1[ n + 1 ] - (opus_int32)x2[ n + 1 ];
        x1[ n + 1 ] = (opus_int16)((sum) > 0x7FFF ? 0x7FFF : ((sum) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (sum)));
        x2[ n + 1 ] = (opus_int16)((diff) > 0x7FFF ? 0x7FFF : ((diff) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (diff)));
    }
}
#end unsafe
