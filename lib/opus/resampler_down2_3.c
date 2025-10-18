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
use os_support, resampler_private_ar2, resampler_rom;

/* Downsample by a factor 2/3, low quality */
public
void silk_resampler_down2_3(
    opus_int32                  *S,                 /* I/O  State vector [ 6 ]                                          */
    opus_int16                  *_out,               /* O    Output signal [ floor(2*inLen/3) ]                          */
          opus_int16            *in,                /* I    Input signal [ inLen ]                                      */
    opus_int32                  inLen               /* I    Number of input samples                                     */
)
{
    opus_int32 nSamplesIn, counter, res_Q6;
    opus_int32 buf[ ( 10 * 48 ) + 4 ];
    opus_int32 *buf_ptr;

    /* Copy buffered samples to start of buffer */
    memcpy((byte*)(&buf), (byte*)(S), (4 * ((int)(opus_int32 ' size  ))));

    /* Iterate over blocks of frameSizeIn input samples */
    while( true ) {
        nSamplesIn = (((inLen) < (( 10 * 48 ))) ? (inLen) : (( 10 * 48 )));

        /* Second-order AR filter (output in Q8) */
        silk_resampler_private_AR2( &S[ 4 ], &buf[ 4 ], in,
            &silk_Resampler_2_3_COEFS_LQ, nSamplesIn );

        /* Interpolate filtered signal */
        buf_ptr = &buf;
        counter = nSamplesIn;
        while( counter > 2 ) {
            /* Inner product */
            res_Q6 = ((((buf_ptr[ 0 ]) >> 16) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 2 ]))) + ((((buf_ptr[ 0 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 2 ]))) >> 16));
            res_Q6 = ((res_Q6) + ((((buf_ptr[ 1 ]) >> 16) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 3 ]))) + ((((buf_ptr[ 1 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 3 ]))) >> 16)));
            res_Q6 = ((res_Q6) + ((((buf_ptr[ 2 ]) >> 16) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 5 ]))) + ((((buf_ptr[ 2 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 5 ]))) >> 16)));
            res_Q6 = ((res_Q6) + ((((buf_ptr[ 3 ]) >> 16) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 4 ]))) + ((((buf_ptr[ 3 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 4 ]))) >> 16)));

            /* Scale down, saturate and store in output array */
            *(*&_out)++ = (opus_int16)((((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1))));

            res_Q6 = ((((buf_ptr[ 1 ]) >> 16) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 4 ]))) + ((((buf_ptr[ 1 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 4 ]))) >> 16));
            res_Q6 = ((res_Q6) + ((((buf_ptr[ 2 ]) >> 16) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 5 ]))) + ((((buf_ptr[ 2 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 5 ]))) >> 16)));
            res_Q6 = ((res_Q6) + ((((buf_ptr[ 3 ]) >> 16) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 3 ]))) + ((((buf_ptr[ 3 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 3 ]))) >> 16)));
            res_Q6 = ((res_Q6) + ((((buf_ptr[ 4 ]) >> 16) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 2 ]))) + ((((buf_ptr[ 4 ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_Resampler_2_3_COEFS_LQ[ 2 ]))) >> 16)));

            /* Scale down, saturate and store in output array */
            *(*&_out)++ = (opus_int16)((((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((6) == 1 ? ((res_Q6) >> 1) + ((res_Q6) & 1) : (((res_Q6) >> ((6) - 1)) + 1) >> 1))));

            buf_ptr += 3;
            counter -= 3;
        }

        *&in += nSamplesIn;
        *&inLen -= nSamplesIn;

        if( inLen > 0 ) {
            /* More iterations to do; copy last part of filtered signal to beginning of buffer */
            memcpy((byte*)&(buf), (byte*)(&buf[ nSamplesIn ]), (4 * ((int)(opus_int32 ' size  ))));
        } else {
            break;
        }
    }

    /* Copy last part of filtered signal to the state for the next call */
    memcpy((byte*)(S), (byte*)(&buf[ nSamplesIn ]), (4 * ((int)(opus_int32 ' size  ))));
}
#end unsafe
