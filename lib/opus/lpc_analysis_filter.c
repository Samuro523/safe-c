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

/*******************************************/
/* LPC analysis filter                     */
/* NB! State is kept internally and the    */
/* filter always starts with zero state    */
/* first d output samples are set to zero  */
/*******************************************/

public
void silk_LPC_analysis_filter(
    opus_int16                  *_out,               /* O    Output signal                                               */
          opus_int16            *in,                /* I    Input signal                                                */
          opus_int16            *B,                 /* I    MA prediction coefficients, Q12 [order]                     */
          opus_int32            len,                /* I    Signal length                                               */
          opus_int32            d                   /* I    Filter order                                                */
)
{
    int         ix, j;
    opus_int32       out32_Q12, out32;
          opus_int16 *in_ptr;

    ;
    ;
    ;

    for( ix = d; ix < len; ix++ ) {
        in_ptr = &in[ ix - 1 ];

        out32_Q12 = ((opus_int32)((opus_int16)(in_ptr[ 0 ])) * (opus_int32)((opus_int16)(B[ 0 ])));
        /* Allowing wrap around so that two wraps can cancel each other. The rare
           cases where the result wraps around can only be triggered by invalid streams*/
        out32_Q12 = (((opus_int32)((opus_uint32)((out32_Q12)) + (opus_uint32)(((opus_int32)((opus_int16)(in_ptr[ -1 ]))) * (opus_int32)((opus_int16)(B[ 1 ]))))));
        out32_Q12 = (((opus_int32)((opus_uint32)((out32_Q12)) + (opus_uint32)(((opus_int32)((opus_int16)(in_ptr[ -2 ]))) * (opus_int32)((opus_int16)(B[ 2 ]))))));
        out32_Q12 = (((opus_int32)((opus_uint32)((out32_Q12)) + (opus_uint32)(((opus_int32)((opus_int16)(in_ptr[ -3 ]))) * (opus_int32)((opus_int16)(B[ 3 ]))))));
        out32_Q12 = (((opus_int32)((opus_uint32)((out32_Q12)) + (opus_uint32)(((opus_int32)((opus_int16)(in_ptr[ -4 ]))) * (opus_int32)((opus_int16)(B[ 4 ]))))));
        out32_Q12 = (((opus_int32)((opus_uint32)((out32_Q12)) + (opus_uint32)(((opus_int32)((opus_int16)(in_ptr[ -5 ]))) * (opus_int32)((opus_int16)(B[ 5 ]))))));
        for( j = 6; j < d; j += 2 ) {
            out32_Q12 = (((opus_int32)((opus_uint32)((out32_Q12)) + (opus_uint32)(((opus_int32)((opus_int16)(in_ptr[ -j ]))) * (opus_int32)((opus_int16)(B[ j ]))))));
            out32_Q12 = (((opus_int32)((opus_uint32)((out32_Q12)) + (opus_uint32)(((opus_int32)((opus_int16)(in_ptr[ -j - 1 ]))) * (opus_int32)((opus_int16)(B[ j + 1 ]))))));
        }

        /* Subtract prediction */
        out32_Q12 = ((opus_int32)((opus_uint32)(((opus_int32)((opus_uint32)((opus_int32)in_ptr[ 1 ])<<(12)))) - (opus_uint32)(out32_Q12)));

        /* Scale to Q0 */
        out32 = ((12) == 1 ? ((out32_Q12) >> 1) + ((out32_Q12) & 1) : (((out32_Q12) >> ((12) - 1)) + 1) >> 1);

        /* Saturate output */
        _out[ ix ] = (opus_int16)((out32) > 0x7FFF ? 0x7FFF : ((out32) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : (out32)));
    }

    /* Set first d output samples to zero */
    memset((char*)(_out), (0), (d * ((int)(opus_int16 ' size  ))));
}
#end unsafe
