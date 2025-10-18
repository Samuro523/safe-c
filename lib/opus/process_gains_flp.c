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
use structs_flp;
use sigproc_flp, gain_quant, tables;
use os_support;


/* Processing of gains */
public
void silk_process_gains_FLP(
    silk_encoder_state_FLP          *psEnc,                             /* I/O  Encoder state FLP                           */
    silk_encoder_control_FLP        *psEncCtrl,                         /* I/O  Encoder control FLP                         */
    int                        condCoding                          /* I    The type of conditional coding to use       */
){
    silk_shape_state_FLP *psShapeSt = &psEnc->sShape;
    int     k;
    opus_int32   pGains_Q16[ 4 ];
    float   s, InvMaxSqrVal, gain, quant_offset;

    /* Gain reduction when LTP coding gain is high */
    if( psEnc->sCmn.indices.signalType == 2 ) {
        s = 1.0f - 0.5f * silk_sigmoid( 0.25f * ( psEncCtrl->LTPredCodGain - 12.0f ) );
        for( k = 0; k < psEnc->sCmn.nb_subfr; k++ ) {
            psEncCtrl->Gains[ k ] *= s;
        }
    }

    /* Limit the quantized signal */
    InvMaxSqrVal = ( float )( pow( 2.0f, 0.33f * ( 21.0f - (float)psEnc->sCmn.SNR_dB_Q7 * ( 1.0 / 128.0f ) ) ) / (float)psEnc->sCmn.subfr_length );

    for( k = 0; k < psEnc->sCmn.nb_subfr; k++ ) {
        /* Soft limit on ratio residual energy and squared gains */
        gain = psEncCtrl->Gains[ k ];
        gain = ( float )sqrt( gain * gain + psEncCtrl->ResNrg[ k ] * InvMaxSqrVal );
        psEncCtrl->Gains[ k ] = (((gain) < (32767.0f)) ? (gain) : (32767.0f));
    }

    /* Prepare gains for noise shaping quantization */
    for( k = 0; k < psEnc->sCmn.nb_subfr; k++ ) {
        pGains_Q16[ k ] = (opus_int32)( psEncCtrl->Gains[ k ] * 65536.0f );
    }

    /* Save unquantized gains and gain Index */
    memcpy((byte*)&(psEncCtrl->GainsUnq_Q16), (byte*)&(pGains_Q16), (psEnc->sCmn.nb_subfr * ((int)(opus_int32 ' size  ))));
    psEncCtrl->lastGainIndexPrev = psShapeSt->LastGainIndex;

    /* Quantize gains */
    silk_gains_quant( &psEnc->sCmn.indices.GainsIndices, &pGains_Q16,
            &psShapeSt->LastGainIndex, (int)(condCoding == 2), psEnc->sCmn.nb_subfr );

    /* Overwrite unquantized gains with quantized gains and convert back to Q0 from Q16 */
    for( k = 0; k < psEnc->sCmn.nb_subfr; k++ ) {
        psEncCtrl->Gains[ k ] = (float)pGains_Q16[ k ] / 65536.0f;
    }

    /* Set quantizer offset for voiced signals. Larger offset when LTP coding gain is low or tilt is high (ie low-pass) */
    if( psEnc->sCmn.indices.signalType == 2 ) {
        if( psEncCtrl->LTPredCodGain + (float)psEnc->sCmn.input_tilt_Q15 * ( 1.0f / 32768.0f ) > 1.0f ) {
            psEnc->sCmn.indices.quantOffsetType = 0;
        } else {
            psEnc->sCmn.indices.quantOffsetType = 1;
        }
    }

    /* Quantizer boundary adjustment */
    quant_offset = (float)silk_Quantization_Offsets_Q10[ psEnc->sCmn.indices.signalType >> 1 ][ psEnc->sCmn.indices.quantOffsetType ] / 1024.0f;
    psEncCtrl->Lambda = 1.2f
                      + -0.05f * (float)psEnc->sCmn.nStatesDelayedDecision
                      + -0.2f        * (float)psEnc->sCmn.speech_activity_Q8 * ( 1.0f /  256.0f )
                      + -0.1f     * psEncCtrl->input_quality
                      + -0.2f    * psEncCtrl->coding_quality
                      + 0.8f      * quant_offset;

    ;
    ;
}
#end unsafe
