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

/* Approximation of 2^() (very close inverse of silk_lin2log()) */
/* Convert input to a linear scale    */

public
opus_int32 silk_log2lin( 
          opus_int32            inLog_Q7            /* I  input on log scale                                            */
)
{
    opus_int32 _out, frac_Q7;

    if( inLog_Q7 < 0 ) {
        return 0;
    }

    _out = ((opus_int32)((opus_uint32)(1)<<(uint)(((inLog_Q7)>>(7)))));
    frac_Q7 = inLog_Q7 & 0x7F;
    if( inLog_Q7 < 2048 ) {
        /* Piece-wise parabolic approximation */
        _out = (((_out)) + ((((((_out) * (((frac_Q7) + ((((((opus_int32)((opus_int16)(frac_Q7)) * (opus_int32)((opus_int16)(128 - frac_Q7)))) >> 16) * (opus_int32)((opus_int16)(-174))) + ((((((opus_int32)((opus_int16)(frac_Q7)) * (opus_int32)((opus_int16)(128 - frac_Q7)))) & 0x0000FFFF) * (opus_int32)((opus_int16)(-174))) >> 16)))))))>>((7)))));
    } else {
        /* Piece-wise parabolic approximation */
        _out = (((_out)) + (((((_out)>>(7))) * (((frac_Q7) + ((((((opus_int32)((opus_int16)(frac_Q7)) * (opus_int32)((opus_int16)(128 - frac_Q7)))) >> 16) * (opus_int32)((opus_int16)(-174))) + ((((((opus_int32)((opus_int16)(frac_Q7)) * (opus_int32)((opus_int16)(128 - frac_Q7)))) & 0x0000FFFF) * (opus_int32)((opus_int16)(-174))) >> 16)))))));
    }
    return _out;
}
#end unsafe
