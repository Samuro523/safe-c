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
use resampler_rom, resampler_structs;


/* Upsample by a factor 2, high quality */
/* Uses 2nd order allpass filters for the 2x upsampling, followed by a      */
/* notch filter just above Nyquist.                                         */
public void silk_resampler_private_up2_HQ(
    opus_int32                      *S,             /* I/O  Resampler state [ 6 ]       */
    opus_int16                      *_out,           /* O    Output signal [ 2 * len ]   */
          opus_int16                *in,            /* I    Input signal [ len ]        */
    opus_int32                      len             /* I    Number of input samples     */
)
{
    opus_int32 k;
    opus_int32 in32, out32_1, out32_2, Y, X;

    ;
    ;
    ;
    ;
    ;
    ;

    /* Internal variables and state are in Q10 format */
    for( k = 0; k < len; k++ ) {
        /* Convert to Q10 */
        in32 = ((opus_int32)((opus_uint32)((opus_int32)in[ k ])<<(10)));

        /* First all-pass section for even output sample */
        Y       = ((in32) - (S[ 0 ]));
        X       = ((((Y) >> 16) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_0[ 0 ]))) + ((((Y) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_0[ 0 ]))) >> 16));
        out32_1 = ((S[ 0 ]) + (X));
        S[ 0 ]  = ((in32) + (X));

        /* Second all-pass section for even output sample */
        Y       = ((out32_1) - (S[ 1 ]));
        X       = ((((Y) >> 16) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_0[ 1 ]))) + ((((Y) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_0[ 1 ]))) >> 16));
        out32_2 = ((S[ 1 ]) + (X));
        S[ 1 ]  = ((out32_1) + (X));

        /* Third all-pass section for even output sample */
        Y       = ((out32_2) - (S[ 2 ]));
        X       = ((Y) + ((((Y) >> 16) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_0[ 2 ]))) + ((((Y) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_0[ 2 ]))) >> 16)));
        out32_1 = ((S[ 2 ]) + (X));
        S[ 2 ]  = ((out32_2) + (X));

        /* Apply gain in Q15, convert back to int16 and store to output */
        _out[ 2 * k ] = (opus_int16)((((10) == 1 ? ((out32_1) >> 1) + ((out32_1) & 1) : (((out32_1) >> ((10) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((10) == 1 ? ((out32_1) >> 1) + ((out32_1) & 1) : (((out32_1) >> ((10) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((10) == 1 ? ((out32_1) >> 1) + ((out32_1) & 1) : (((out32_1) >> ((10) - 1)) + 1) >> 1))));

        /* First all-pass section for odd output sample */
        Y       = ((in32) - (S[ 3 ]));
        X       = ((((Y) >> 16) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_1[ 0 ]))) + ((((Y) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_1[ 0 ]))) >> 16));
        out32_1 = ((S[ 3 ]) + (X));
        S[ 3 ]  = ((in32) + (X));

        /* Second all-pass section for odd output sample */
        Y       = ((out32_1) - (S[ 4 ]));
        X       = ((((Y) >> 16) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_1[ 1 ]))) + ((((Y) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_1[ 1 ]))) >> 16));
        out32_2 = ((S[ 4 ]) + (X));
        S[ 4 ]  = ((out32_1) + (X));

        /* Third all-pass section for odd output sample */
        Y       = ((out32_2) - (S[ 5 ]));
        X       = ((Y) + ((((Y) >> 16) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_1[ 2 ]))) + ((((Y) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_resampler_up2_hq_1[ 2 ]))) >> 16)));
        out32_1 = ((S[ 5 ]) + (X));
        S[ 5 ]  = ((out32_2) + (X));

        /* Apply gain in Q15, convert back to int16 and store to output */
        _out[ 2 * k + 1 ] = (opus_int16)((((10) == 1 ? ((out32_1) >> 1) + ((out32_1) & 1) : (((out32_1) >> ((10) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((10) == 1 ? ((out32_1) >> 1) + ((out32_1) & 1) : (((out32_1) >> ((10) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((10) == 1 ? ((out32_1) >> 1) + ((out32_1) & 1) : (((out32_1) >> ((10) - 1)) + 1) >> 1))));
    }
}

public void silk_resampler_private_up2_HQ_wrapper(
    byte                            *SS,            /* I/O  Resampler state (unused)    */
    opus_int16                      *_out,           /* O    Output signal [ 2 * len ]   */
    opus_int16                      *in,            /* I    Input signal [ len ]        */
    opus_int32                      len             /* I    Number of input samples     */
)
{
    silk_resampler_state_struct *S = (silk_resampler_state_struct *)SS;
    silk_resampler_private_up2_HQ( &S->sIIR, _out, in, len );
}
#end unsafe
