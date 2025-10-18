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
use opus_types, structs_flp, find_ltp_flp, wrappers_flp, ltp_scale_ctrl_flp;
use ltp_analysis_filter_flp, scale_copy_vector_flp, os_support, find_lpc_flp, residual_energy_flp;


/* Find LPC and LTP coefficients */
public
void silk_find_pred_coefs_FLP(
    silk_encoder_state_FLP          *psEnc,                             /* I/O  Encoder state FLP                           */
    silk_encoder_control_FLP        *psEncCtrl,                         /* I/O  Encoder control FLP                         */
          float                res_pitch*,                        /* I    Residual from pitch analysis                */
          float                x*,                                /* I    Speech signal                               */
    int                        condCoding                          /* I    The type of conditional coding to use       */
)
{
    int         i;
    float       WLTP[ 4 * 5 * 5 ];
    float       invGains[ 4 ], Wght[ 4 ];
    opus_int16       NLSF_Q15[ 16 ];
          float *x_ptr;
    float       x_pre_ptr*, LPC_in_pre[ 4 * 16 + ( ( 5 * 4 ) * 16 ) ];
    float       minInvGain;

	clear invGains;
	
    /* Weighting for weighted least squares */
    for( i = 0; i < psEnc->sCmn.nb_subfr; i++ ) {
        ;
        invGains[ i ] = 1.0f / psEncCtrl->Gains[ i ];
        Wght[ i ]     = invGains[ i ] * invGains[ i ];
    }

    if( psEnc->sCmn.indices.signalType == 2 ) {
        /**********/
        /* VOICED */
        /**********/
        ;

        /* LTP analysis */
        silk_find_LTP_FLP( &psEncCtrl->LTPCoef, &WLTP, &psEncCtrl->LTPredCodGain, res_pitch,
            &psEncCtrl->pitchL, &Wght, psEnc->sCmn.subfr_length, psEnc->sCmn.nb_subfr, psEnc->sCmn.ltp_mem_length );

        /* Quantize LTP gain parameters */
        silk_quant_LTP_gains_FLP( &psEncCtrl->LTPCoef, &psEnc->sCmn.indices.LTPIndex, &psEnc->sCmn.indices.PERIndex,
            &WLTP, psEnc->sCmn.mu_LTP_Q9, psEnc->sCmn.LTPQuantLowComplexity, psEnc->sCmn.nb_subfr );

        /* Control LTP scaling */
        silk_LTP_scale_ctrl_FLP( psEnc, psEncCtrl, condCoding );

        /* Create LTP residual */
        silk_LTP_analysis_filter_FLP( &LPC_in_pre, x - psEnc->sCmn.predictLPCOrder, &psEncCtrl->LTPCoef,
            &psEncCtrl->pitchL, &invGains, psEnc->sCmn.subfr_length, psEnc->sCmn.nb_subfr, psEnc->sCmn.predictLPCOrder );
    } else {
        /************/
        /* UNVOICED */
        /************/
        /* Create signal with prepended subframes, scaled by inverse gains */
        x_ptr     = x - psEnc->sCmn.predictLPCOrder;
        x_pre_ptr = &LPC_in_pre;
        for( i = 0; i < psEnc->sCmn.nb_subfr; i++ ) {
            silk_scale_copy_vector_FLP( x_pre_ptr, x_ptr, invGains[ i ],
                psEnc->sCmn.subfr_length + psEnc->sCmn.predictLPCOrder );
            x_pre_ptr += psEnc->sCmn.subfr_length + psEnc->sCmn.predictLPCOrder;
            x_ptr     += psEnc->sCmn.subfr_length;
        }
        memset((char*)&(psEncCtrl->LTPCoef), (0), (psEnc->sCmn.nb_subfr * 5 * ((int)(float ' size  ))));
        psEncCtrl->LTPredCodGain = 0.0f;
    }

    /* Limit on total predictive coding gain */
    if( psEnc->sCmn.first_frame_after_reset != 0 ) {
        minInvGain = 1.0f / 1.0e2f;
    } else {        
        minInvGain = (float)pow( 2.0, psEncCtrl->LTPredCodGain / 3.0 ) /  1.0e4f;
        minInvGain /= 0.25f + 0.75f * psEncCtrl->coding_quality;
    }

    /* LPC_in_pre contains the LTP-filtered input for voiced, and the unfiltered input for unvoiced */
    silk_find_LPC_FLP( &psEnc->sCmn, &NLSF_Q15, &LPC_in_pre, minInvGain );

    /* Quantize LSFs */
    silk_process_NLSFs_FLP( &psEnc->sCmn, &psEncCtrl->PredCoef, &NLSF_Q15, &psEnc->sCmn.prev_NLSFq_Q15 );

    /* Calculate residual energy using quantized LPC coefficients */
    silk_residual_energy_FLP( &psEncCtrl->ResNrg, &LPC_in_pre, &psEncCtrl->PredCoef, &psEncCtrl->Gains,
        psEnc->sCmn.subfr_length, psEnc->sCmn.nb_subfr, psEnc->sCmn.predictLPCOrder );

    /* Copy to prediction struct for use in next frame for interpolation */
    memcpy((byte*)&(psEnc->sCmn.prev_NLSFq_Q15), (byte*)&(NLSF_Q15), (((int)(psEnc->sCmn.prev_NLSFq_Q15 ' size  ))));
}

#end unsafe
