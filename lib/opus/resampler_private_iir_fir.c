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
use resampler_rom, resampler_structs;
use resampler_private_up2_hq;

opus_int16 *silk_resampler_private_IIR_FIR_INTERPOL(
    opus_int16  *_out,
    opus_int16  *buf,
    opus_int32  max_index_Q16,
    opus_int32  index_increment_Q16
)
{
    opus_int32 index_Q16, res_Q15;
    opus_int16 *buf_ptr;
    opus_int32 table_index;

    /* Interpolate upsampled signal and store in output array */
    for( index_Q16 = 0; index_Q16 < max_index_Q16; index_Q16 += index_increment_Q16 ) {
        table_index = ((((index_Q16 & 0xFFFF) >> 16) * (opus_int32)((opus_int16)(12))) + ((((index_Q16 & 0xFFFF) & 0x0000FFFF) * (opus_int32)((opus_int16)(12))) >> 16));
        buf_ptr = &buf[ index_Q16 >> 16 ];

        res_Q15 = ((opus_int32)((opus_int16)(buf_ptr[ 0 ])) * (opus_int32)((opus_int16)(silk_resampler_frac_FIR_12[ table_index ][ 0 ])));
        res_Q15 = ((res_Q15) + ((opus_int32)((opus_int16)(buf_ptr[ 1 ]))) * (opus_int32)((opus_int16)(silk_resampler_frac_FIR_12[ table_index ][ 1 ])));
        res_Q15 = ((res_Q15) + ((opus_int32)((opus_int16)(buf_ptr[ 2 ]))) * (opus_int32)((opus_int16)(silk_resampler_frac_FIR_12[ table_index ][ 2 ])));
        res_Q15 = ((res_Q15) + ((opus_int32)((opus_int16)(buf_ptr[ 3 ]))) * (opus_int32)((opus_int16)(silk_resampler_frac_FIR_12[ table_index ][ 3 ])));
        res_Q15 = ((res_Q15) + ((opus_int32)((opus_int16)(buf_ptr[ 4 ]))) * (opus_int32)((opus_int16)(silk_resampler_frac_FIR_12[ 11 - table_index ][ 3 ])));
        res_Q15 = ((res_Q15) + ((opus_int32)((opus_int16)(buf_ptr[ 5 ]))) * (opus_int32)((opus_int16)(silk_resampler_frac_FIR_12[ 11 - table_index ][ 2 ])));
        res_Q15 = ((res_Q15) + ((opus_int32)((opus_int16)(buf_ptr[ 6 ]))) * (opus_int32)((opus_int16)(silk_resampler_frac_FIR_12[ 11 - table_index ][ 1 ])));
        res_Q15 = ((res_Q15) + ((opus_int32)((opus_int16)(buf_ptr[ 7 ]))) * (opus_int32)((opus_int16)(silk_resampler_frac_FIR_12[ 11 - table_index ][ 0 ])));
        *(*&_out)++ = (opus_int16)((((15) == 1 ? ((res_Q15) >> 1) + ((res_Q15) & 1) : (((res_Q15) >> ((15) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((15) == 1 ? ((res_Q15) >> 1) + ((res_Q15) & 1) : (((res_Q15) >> ((15) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((15) == 1 ? ((res_Q15) >> 1) + ((res_Q15) & 1) : (((res_Q15) >> ((15) - 1)) + 1) >> 1))));
    }
    return _out;
}
/* Upsample using a combination of allpass-based 2x upsampling and FIR interpolation */
public
void silk_resampler_private_IIR_FIR(
    byte                            *SS,            /* I/O  Resampler state             */
    opus_int16                      _out*,          /* O    Output signal               */
          opus_int16                in*,           /* I    Input signal                */
    opus_int32                      inLen           /* I    Number of input samples     */
)
{
    silk_resampler_state_struct *S = (silk_resampler_state_struct *)SS;
    opus_int32 nSamplesIn;
    opus_int32 max_index_Q16, index_increment_Q16;
    opus_int16 buf[ ( 10 * 48 ) + 8 ];

    /* Copy buffered samples to start of buffer */
    memcpy((byte*)&(buf), (byte*)&(S->sFIR), (8 * ((int)(opus_int32 ' size  ))));

    /* Iterate over blocks of frameSizeIn input samples */
    index_increment_Q16 = S->invRatio_Q16;
    while( true ) {
        nSamplesIn = (((inLen) < (S->batchSize)) ? (inLen) : (S->batchSize));

        /* Upsample 2x */
        silk_resampler_private_up2_HQ( &S->sIIR, &buf[ 8 ], in, nSamplesIn );

        max_index_Q16 = ((opus_int32)((opus_uint32)(nSamplesIn)<<(16 + 1)));         /* + 1 because 2x upsampling */
        *&_out = silk_resampler_private_IIR_FIR_INTERPOL( _out, &buf, max_index_Q16, index_increment_Q16 );
        *&in += nSamplesIn;
        *&inLen -= nSamplesIn;

        if( inLen > 0 ) {
            /* More iterations to do; copy last part of filtered signal to beginning of buffer */
            memcpy((byte*)&(buf), (byte*)(&buf[ nSamplesIn << 1 ]), (8 * ((int)(opus_int32 ' size  ))));
        } else {
            break;
        }
    }

    /* Copy last part of filtered signal to the state for the next call */
    memcpy((byte*)&(S->sFIR), (byte*)(&buf[ nSamplesIn << 1 ]), (8 * ((int)(opus_int32 ' size  ))));
}

#end unsafe
