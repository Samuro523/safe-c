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
use os_support;

/* compute inverse of LPC prediction gain, and                          */
/* test if LPC coefficients are stable (all poles within unit circle)   */
/* this code is based on silk_a2k_FLP()                                 */
public
float silk_LPC_inverse_pred_gain_FLP(  /* O    return inverse prediction gain, energy domain               */
          float    *A,                 /* I    prediction coefficients [order]                             */
    opus_int32          order               /* I    prediction order                                            */
)
{
    int   k, n;
    double     invGain, rc, rc_mult1, rc_mult2;
    float Atmp[ 2 ][ 16 ];
    float *Aold, Anew;
    
	clear Atmp;
	
    Anew = &Atmp[ order & 1 ];
    memcpy((byte*)(Anew), (byte*)(A), (order * ((int)(float ' size  ))));

    invGain = 1.0;
    for( k = order - 1; k > 0; k-- ) {
        rc = -Anew[ k ];
        if( rc > 0.9999f || rc < -0.9999f ) {
            return 0.0f;
        }
        rc_mult1 = 1.0f - rc * rc;
        rc_mult2 = 1.0f / rc_mult1;
        invGain *= rc_mult1;
        /* swap pointers */
        Aold = Anew;
        Anew = &Atmp[ k & 1 ];
        for( n = 0; n < k; n++ ) {
            Anew[ n ] = (float)( ( Aold[ n ] - Aold[ k - n - 1 ] * rc ) * rc_mult2 );
        }
    }
    rc = -Anew[ 0 ];
    if( rc > 0.9999f || rc < -0.9999f ) {
        return 0.0f;
    }
    rc_mult1 = 1.0f - rc * rc;
    invGain *= rc_mult1;
    return (float)invGain;
}
#end unsafe
