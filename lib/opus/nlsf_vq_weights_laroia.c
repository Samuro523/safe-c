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
use sigproc_fix;

/*
R. Laroia, N. Phamdo and N. Farvardin, "Robust and Efficient Quantization of Speech LSP
Parameters Using Structured Vector Quantization", Proc. IEEE Int. Conf. Acoust., Speech,
Signal Processing, pp. 641-644, 1991.
*/

/* Laroia low complexity NLSF weights */
public void silk_NLSF_VQ_weights_laroia(
    opus_int16                  *pNLSFW_Q_OUT,      /* O     Pointer to input vector weights [D]                        */
          opus_int16            *pNLSF_Q15,         /* I     Pointer to input vector         [D]                        */
          int              D                   /* I     Input vector dimension (even)                              */
)
{
    int   k;
    opus_int32 tmp1_int, tmp2_int;

    ;
    ;

    /* First value */
    tmp1_int = silk_max_int( pNLSF_Q15[ 0 ], 1 );
    tmp1_int = ((opus_int32)(((opus_int32)1 << ( 15 + 2 )) / (tmp1_int)));
    tmp2_int = silk_max_int( pNLSF_Q15[ 1 ] - pNLSF_Q15[ 0 ], 1 );
    tmp2_int = ((opus_int32)(((opus_int32)1 << ( 15 + 2 )) / (tmp2_int)));
    pNLSFW_Q_OUT[ 0 ] = (opus_int16)silk_min_int( tmp1_int + tmp2_int, 0x7FFF );
    ;

    /* Main loop */
    for( k = 1; k < D - 1; k += 2 ) {
        tmp1_int = silk_max_int( pNLSF_Q15[ k + 1 ] - pNLSF_Q15[ k ], 1 );
        tmp1_int = ((opus_int32)(((opus_int32)1 << ( 15 + 2 )) / (tmp1_int)));
        pNLSFW_Q_OUT[ k ] = (opus_int16)silk_min_int( tmp1_int + tmp2_int, 0x7FFF );
        ;

        tmp2_int = silk_max_int( pNLSF_Q15[ k + 2 ] - pNLSF_Q15[ k + 1 ], 1 );
        tmp2_int = ((opus_int32)(((opus_int32)1 << ( 15 + 2 )) / (tmp2_int)));
        pNLSFW_Q_OUT[ k + 1 ] = (opus_int16)silk_min_int( tmp1_int + tmp2_int, 0x7FFF );
        ;
    }

    /* Last value */
    tmp1_int = silk_max_int( ( 1 << 15 ) - pNLSF_Q15[ D - 1 ], 1 );
    tmp1_int = ((opus_int32)(((opus_int32)1 << ( 15 + 2 )) / (tmp1_int)));
    pNLSFW_Q_OUT[ D - 1 ] = (opus_int16)silk_min_int( tmp1_int + tmp2_int, 0x7FFF );
    ;
}
#end unsafe
