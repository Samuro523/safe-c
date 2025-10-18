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

use ../math;
use opus_types;


/********************************************************************/
/*                                MACROS                            */
/********************************************************************/

/* sigmoid function */
public float silk_sigmoid( float x )
{
    return (float)(1.0 / (1.0 + exp(-x)));
}

/* floating-point to integer conversion (rounding) */
public opus_int32 silk_float2int( float x )
{
    return (opus_int32)((int)(floor(0.5+x)));
}

/* floating-point to integer conversion (rounding) */
public void silk_float2short_array(
    opus_int16       *_out,
          float *in,
    opus_int32       length
)
{
    opus_int32 k;
    for( k = length - 1; k >= 0; k-- ) {
        _out[k] = (opus_int16)((((opus_int32)((int)(floor(0.5+in[k])))) > 0x7FFF ? 0x7FFF : (((opus_int32)((int)(floor(0.5+in[k])))) < ((opus_int16)-0x8000) ? ((opus_int16)-0x8000) : ((opus_int32)((int)(floor(0.5+in[k])))))));
    }
}

/* integer to floating-point conversion */
public void silk_short2float_array(
    float       *_out,
          opus_int16 *in,
    opus_int32       length
)
{
    opus_int32 k;
    for( k = length - 1; k >= 0; k-- ) {
        _out[k] = (float)in[k];
    }
}

/* using log2() helps the fixed-point conversion */
public float silk_log2( double x )
{
    return ( float )( 3.32192809488736 * log10( x ) );
}

#end unsafe
