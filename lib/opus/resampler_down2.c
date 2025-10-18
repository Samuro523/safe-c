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
use resampler_rom;

/* Downsample by a factor 2 */
public
void silk_resampler_down2(
    opus_int32                  *S,                 /* I/O  State vector [ 2 ]                                          */
    opus_int16                  *_out,               /* O    Output signal [ len ]                                       */
          opus_int16            *in,                /* I    Input signal [ floor(len/2) ]                               */
    opus_int32                  inLen               /* I    Number of input samples                                     */
)
{
    opus_int32 k, len2 = ((inLen)>>(1));
    opus_int32 in32, out32, Y, X;

    ;
    ;

    /* Internal variables and state are in Q10 format */
    for( k = 0; k < len2; k++ ) {
        /* Convert to Q10 */
        in32 = ((opus_int32)((opus_uint32)((opus_int32)in[ 2 * k ])<<(10)));

        /* All-pass section for even input sample */
        Y      = ((in32) - (S[ 0 ]));
        X      = ((Y) + ((((Y) >> 16) * (opus_int32)((opus_int16)(silk_resampler_down2_1))) + ((((Y) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_resampler_down2_1))) >> 16)));
        out32  = ((S[ 0 ]) + (X));
        S[ 0 ] = ((in32) + (X));

        /* Convert to Q10 */
        in32 = ((opus_int32)((opus_uint32)((opus_int32)in[ 2 * k + 1 ])<<(10)));

        /* All-pass section for odd input sample, and add to output of previous section */
        Y      = ((in32) - (S[ 1 ]));
        X      = ((((Y) >> 16) * (opus_int32)((opus_int16)(silk_resampler_down2_0))) + ((((Y) & 0x0000FFFF) * (opus_int32)((opus_int16)(silk_resampler_down2_0))) >> 16));
        out32  = ((out32) + (S[ 1 ]));
        out32  = ((out32) + (X));
        S[ 1 ] = ((in32) + (X));

        /* Add, convert back to int16 and store to output */
        _out[ k ] = (opus_int16)((((11) == 1 ? ((out32) >> 1) + ((out32) & 1) : (((out32) >> ((11) - 1)) + 1) >> 1)) > 0x7FFF ? 0x7FFF : ((((11) == 1 ? ((out32) >> 1) + ((out32) & 1) : (((out32) >> ((11) - 1)) + 1) >> 1)) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (((11) == 1 ? ((out32) >> 1) + ((out32) & 1) : (((out32) >> ((11) - 1)) + 1) >> 1))));
    }
}

#end unsafe
