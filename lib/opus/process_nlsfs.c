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

use opus_types, structs, nlsf_vq_weights_laroia, interpolate, nlsf_encode, nlsf2a, os_support;

/* Limit, stabilize, convert and quantize NLSFs */
public
void silk_process_NLSFs(
    silk_encoder_state          *psEncC,                            /* I/O  Encoder state                               */
    opus_int16                  PredCoef_Q12*[16], // [ 16 ], /* O    Prediction coefficients                     */
    opus_int16                  pNLSF_Q15*,  // [         16 ], /* I/O  Normalized LSFs (quant out) (0 - (2^15-1))  */
          opus_int16            prev_NLSFq_Q15*  // [    16 ]  /* I    Previous Normalized LSFs (0 - (2^15-1))     */
)
{
    int     i, doInterpolate;
    int     NLSF_mu_Q20;
    opus_int32   i_sqr_Q15;
    opus_int16   pNLSF0_temp_Q15[ 16 ];
    opus_int16   pNLSFW_QW[ 16 ];
    opus_int16   pNLSFW0_temp_QW[ 16 ];

    ;
    ;
    ;

    /***********************/
    /* Calculate mu values */
    /***********************/
    /* NLSF_mu  = 0.003 - 0.0015 * psEnc->speech_activity; */
    NLSF_mu_Q20 = ((((opus_int32)((0.003) * (float)((int8)1 << (20)) + 0.5))) + ((((((opus_int32)((-0.001) * (float)((int8)1 << (28)) + 0.5))) >> 16) * (opus_int32)((opus_int16)(psEncC->speech_activity_Q8))) + ((((((opus_int32)((-0.001) * (float)((int8)1 << (28)) + 0.5))) & 0x0000FFFF) * (opus_int32)((opus_int16)(psEncC->speech_activity_Q8))) >> 16)));
    if( psEncC->nb_subfr == 2 ) {
        /* Multiply by 1.5 for 10 ms packets */
        NLSF_mu_Q20 = ((NLSF_mu_Q20) + (((NLSF_mu_Q20))>>((1))));
    }

    ;
    ;

    /* Calculate NLSF weights */
    silk_NLSF_VQ_weights_laroia( &pNLSFW_QW, pNLSF_Q15, psEncC->predictLPCOrder );

    /* Update NLSF weights for interpolated NLSFs */
    doInterpolate = (int)(( psEncC->useInterpolatedNLSFs == 1 ) && ( psEncC->indices.NLSFInterpCoef_Q2 < 4 ));
    if( doInterpolate != 0 ) {
        /* Calculate the interpolated NLSF vector for the first half */
        silk_interpolate( &pNLSF0_temp_Q15, prev_NLSFq_Q15, pNLSF_Q15,
            psEncC->indices.NLSFInterpCoef_Q2, psEncC->predictLPCOrder );

        /* Calculate first half NLSF weights for the interpolated NLSFs */
        silk_NLSF_VQ_weights_laroia( &pNLSFW0_temp_QW, &pNLSF0_temp_Q15, psEncC->predictLPCOrder );

        /* Update NLSF weights with contribution from first half */
        i_sqr_Q15 = ((opus_int32)((opus_uint32)(((opus_int32)((opus_int16)(psEncC->indices.NLSFInterpCoef_Q2)) * (opus_int32)((opus_int16)(psEncC->indices.NLSFInterpCoef_Q2))))<<(11)));
        for( i = 0; i < psEncC->predictLPCOrder; i++ ) {
            pNLSFW_QW[ i ] = (opus_int16)(((((pNLSFW_QW[ i ])>>(1))) + (((((opus_int32)pNLSFW0_temp_QW[ i ]) >> 16) * (opus_int32)((opus_int16)(i_sqr_Q15))) + (((((opus_int32)pNLSFW0_temp_QW[ i ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(i_sqr_Q15))) >> 16))));
            ;
        }
    }

    silk_NLSF_encode( &psEncC->indices.NLSFIndices, pNLSF_Q15, psEncC->psNLSF_CB, &pNLSFW_QW,
        NLSF_mu_Q20, psEncC->NLSF_MSVQ_Survivors, psEncC->indices.signalType );

    /* Convert quantized NLSFs back to LPC coefficients */
    silk_NLSF2A( &PredCoef_Q12[ 1 ], pNLSF_Q15, psEncC->predictLPCOrder );

    if( doInterpolate != 0 ) {
        /* Calculate the interpolated, quantized LSF vector for the first half */
        silk_interpolate( &pNLSF0_temp_Q15, prev_NLSFq_Q15, pNLSF_Q15,
            psEncC->indices.NLSFInterpCoef_Q2, psEncC->predictLPCOrder );

        /* Convert back to LPC coefficients */
        silk_NLSF2A( &PredCoef_Q12[ 0 ], &pNLSF0_temp_Q15, psEncC->predictLPCOrder );

    } else {
        /* Copy LPC coefficients for first half from second half */
        memcpy((byte*)&(PredCoef_Q12[ 0 ]), (byte*)&(PredCoef_Q12[ 1 ]), (psEncC->predictLPCOrder * ((int)(opus_int16 ' size  ))));
    }
}
#end unsafe
