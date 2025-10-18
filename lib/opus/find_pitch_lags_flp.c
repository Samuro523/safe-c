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

use structs_flp, apply_sine_window_flp, autocorrelation_flp;
use os_support, schur_flp, k2a_flp, bwexpander_flp, lpc_analysis_filter_flp;
use pitch_analysis_core_flp;

public
void silk_find_pitch_lags_FLP(
    silk_encoder_state_FLP          *psEnc,                             /* I/O  Encoder state FLP                           */
    silk_encoder_control_FLP        *psEncCtrl,                         /* I/O  Encoder control FLP                         */
    float                      res*,                              /* O    Residual                                    */
    float                x*                                 /* I    Speech signal                               */
)
{
    int   buf_len;
    float thrhld, res_nrg;
          float *x_buf_ptr, x_buf;
    float auto_corr[ 16 + 1 ];
    float A[         16 ];
    float refl_coef[ 16 ];
    float Wsig[      ( ( 20 + (2 << 1) ) * 16 ) ];
    float *Wsig_ptr;

    /******************************************/
    /* Set up buffer lengths etc based on Fs  */
    /******************************************/
    buf_len = psEnc->sCmn.la_pitch + psEnc->sCmn.frame_length + psEnc->sCmn.ltp_mem_length;

    /* Safty check */
    ;

    x_buf = x - psEnc->sCmn.ltp_mem_length;

    /******************************************/
    /* Estimate LPC AR coeficients            */
    /******************************************/

    /* Calculate windowed signal */

    /* First LA_LTP samples */
    x_buf_ptr = x_buf + buf_len - psEnc->sCmn.pitch_LPC_win_length;
    Wsig_ptr  = &Wsig;
    silk_apply_sine_window_FLP( Wsig_ptr, x_buf_ptr, 1, psEnc->sCmn.la_pitch );

    /* Middle non-windowed samples */
    Wsig_ptr  += psEnc->sCmn.la_pitch;
    x_buf_ptr += psEnc->sCmn.la_pitch;
    memcpy((byte*)(Wsig_ptr), (byte*)(x_buf_ptr), (( psEnc->sCmn.pitch_LPC_win_length - ( psEnc->sCmn.la_pitch << 1 ) ) * ((int)(float ' size  ))));

    /* Last LA_LTP samples */
    Wsig_ptr  += psEnc->sCmn.pitch_LPC_win_length - ( psEnc->sCmn.la_pitch << 1 );
    x_buf_ptr += psEnc->sCmn.pitch_LPC_win_length - ( psEnc->sCmn.la_pitch << 1 );
    silk_apply_sine_window_FLP( Wsig_ptr, x_buf_ptr, 2, psEnc->sCmn.la_pitch );

    /* Calculate autocorrelation sequence */
    silk_autocorrelation_FLP( &auto_corr, &Wsig, psEnc->sCmn.pitch_LPC_win_length, psEnc->sCmn.pitchEstimationLPCOrder + 1 );

    /* Add white noise, as a fraction of the energy */
    auto_corr[ 0 ] += auto_corr[ 0 ] * 1.0e-3f + 1.0;

    /* Calculate the reflection coefficients using Schur */
    res_nrg = silk_schur_FLP( &refl_coef, &auto_corr, psEnc->sCmn.pitchEstimationLPCOrder );

    /* Prediction gain */
    psEncCtrl->predGain = auto_corr[ 0 ] / (((res_nrg) > (1.0f)) ? (res_nrg) : (1.0f));

    /* Convert reflection coefficients to prediction coefficients */
    silk_k2a_FLP( &A, &refl_coef, psEnc->sCmn.pitchEstimationLPCOrder );

    /* Bandwidth expansion */
    silk_bwexpander_FLP( &A, psEnc->sCmn.pitchEstimationLPCOrder, 0.99f );

    /*****************************************/
    /* LPC analysis filtering                */
    /*****************************************/
    silk_LPC_analysis_filter_FLP( res, &A, x_buf, buf_len, psEnc->sCmn.pitchEstimationLPCOrder );

    if( psEnc->sCmn.indices.signalType != 0 && psEnc->sCmn.first_frame_after_reset == 0 ) {
        /* Threshold for pitch estimator */
        thrhld  = 0.6f;
        thrhld -= 0.004f * (float)psEnc->sCmn.pitchEstimationLPCOrder;
        thrhld -= 0.1f   * (float)psEnc->sCmn.speech_activity_Q8 * ( 1.0f /  256.0f );
        thrhld -= 0.15f  * (float)(psEnc->sCmn.prevSignalType >> 1);
        thrhld -= 0.1f   * (float)psEnc->sCmn.input_tilt_Q15 * ( 1.0f / 32768.0f );

        /*****************************************/
        /* Call Pitch estimator                  */
        /*****************************************/
        if( silk_pitch_analysis_core_FLP( res, &psEncCtrl->pitchL, &psEnc->sCmn.indices.lagIndex,
            &psEnc->sCmn.indices.contourIndex, &psEnc->LTPCorr, psEnc->sCmn.prevLag, (float)psEnc->sCmn.pitchEstimationThreshold_Q16 / 65536.0f,
            thrhld, psEnc->sCmn.fs_kHz, psEnc->sCmn.pitchEstimationComplexity, psEnc->sCmn.nb_subfr ) == 0 )
        {
            psEnc->sCmn.indices.signalType = 2;
        } else {
            psEnc->sCmn.indices.signalType = 1;
        }
    } else {
        memset((char*)&(psEncCtrl->pitchL), (0), (((int)((psEncCtrl->pitchL) ' size  ))));
        psEnc->sCmn.indices.lagIndex = 0;
        psEnc->sCmn.indices.contourIndex = 0;
        psEnc->LTPCorr = 0.0;
    }
}
#end unsafe
