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
use structs, structs_flp, vad, entcode;
use lp_variable_cutoff, sigproc_flp, find_pitch_lags_flp;
use noise_shape_analysis_flp, find_pred_coefs_flp, process_gains_flp;
use prefilter_flp, gain_quant, wrappers_flp;
use encode_indices, encode_pulses, log2lin;
use sigproc_fix;

public
void silk_encode_do_VAD_FLP(
    silk_encoder_state_FLP          *psEnc                              /* I/O  Encoder state FLP                           */
)
{
    /****************************/
    /* Voice Activity Detection */
    /****************************/
    silk_VAD_GetSA_Q8( &psEnc->sCmn, &psEnc->sCmn.inputBuf + 1 );

    /**************************************************/
    /* Convert speech activity into VAD and DTX flags */
    /**************************************************/
    if( psEnc->sCmn.speech_activity_Q8 < ((opus_int32)((0.05f) * (float)((int8)1 << (8)) + 0.5)) ) {
        psEnc->sCmn.indices.signalType = 0;
        psEnc->sCmn.noSpeechCounter++;
        if( psEnc->sCmn.noSpeechCounter < 10 ) {
            psEnc->sCmn.inDTX = 0;
        } else if( psEnc->sCmn.noSpeechCounter > 20 + 10 ) {
            psEnc->sCmn.noSpeechCounter = 10;
            psEnc->sCmn.inDTX           = 0;
        }
        psEnc->sCmn.VAD_flags[ psEnc->sCmn.nFramesEncoded ] = 0;
    } else {
        psEnc->sCmn.noSpeechCounter    = 0;
        psEnc->sCmn.inDTX              = 0;
        psEnc->sCmn.indices.signalType = 1;
        psEnc->sCmn.VAD_flags[ psEnc->sCmn.nFramesEncoded ] = 1;
    }
}

/****************/
/* Encode frame */
/****************/
public
int silk_encode_frame_FLP(
    silk_encoder_state_FLP          *psEnc,                             /* I/O  Encoder state FLP                           */
    opus_int32                      *pnBytesOut,                        /* O    Number of payload bytes;                    */
    ec_enc                          *psRangeEnc,                        /* I/O  compressor data structure                   */
    int                        condCoding,                         /* I    The type of conditional coding to use       */
    int                        maxBits,                            /* I    If > 0: maximum number of output bits       */
    int                        useCBR                              /* I    Flag to force      ant-bitrate operation    */
)
{
    silk_encoder_control_FLP sEncCtrl;
    int     i, iter, maxIter, found_upper, found_lower, ret = 0;
    float * x_frame, res_pitch_frame;
    float   xfw[ ( ( 5 * 4 ) * 16 ) ];
    float   res_pitch[ 2 * ( ( 5 * 4 ) * 16 ) + ( 2 * 16 ) ];
    ec_enc       sRangeEnc_copy, sRangeEnc_copy2;
    silk_nsq_state sNSQ_copy, sNSQ_copy2;
    opus_int32   seed_copy, nBits, nBits_lower, nBits_upper, gainMult_lower, gainMult_upper;
    opus_int32   gainsID, gainsID_lower, gainsID_upper;
    opus_int16   gainMult_Q8;
    opus_int16   ec_prevLagIndex_copy;
    int     ec_prevSignalType_copy;
    int1    LastGainIndex_copy2;
    opus_int32   pGains_Q16[ 4 ];
    byte   ec_buf_copy[ 1275 ];

	clear sEncCtrl;
	
    /* This is totally unnecessary but many compilers (including gcc) are too dumb to realise it */
    clear LastGainIndex_copy2, nBits_lower, nBits_upper, gainMult_lower, gainMult_upper;

    psEnc->sCmn.indices.Seed = (int1)(psEnc->sCmn.frameCounter++ & 3);

    /**************************************************************/
    /* Set up Input Pointers, and insert frame in input buffer    */
    /**************************************************************/
    /* pointers aligned with start of frame to encode */
    x_frame         = &psEnc->x_buf + psEnc->sCmn.ltp_mem_length;    /* start of frame to encode */
    res_pitch_frame = &res_pitch    + psEnc->sCmn.ltp_mem_length;    /* start of pitch LPC residual frame */

    /***************************************/
    /* Ensure smooth bandwidth transitions */
    /***************************************/
    silk_LP_variable_cutoff( &psEnc->sCmn.sLP, &psEnc->sCmn.inputBuf + 1, psEnc->sCmn.frame_length );

    /*******************************************/
    /* Copy new frame to front of input buffer */
    /*******************************************/
    silk_short2float_array( x_frame + 5 * psEnc->sCmn.fs_kHz, &psEnc->sCmn.inputBuf + 1, psEnc->sCmn.frame_length );

    /* Add tiny signal to avoid high CPU load from denormalized floating point numbers */
    for( i = 0; i < 8; i++ ) {
        x_frame[ 5 * psEnc->sCmn.fs_kHz + i * ( psEnc->sCmn.frame_length >> 3 ) ] += (float)( 1 - ( i & 2 ) ) * 1.0e-6f;
    }

    if( psEnc->sCmn.prefillFlag == 0 ) {
        /*****************************************/
        /* Find pitch lags, initial LPC analysis */
        /*****************************************/
        silk_find_pitch_lags_FLP( psEnc, &sEncCtrl, &res_pitch, x_frame );

        /************************/
        /* Noise shape analysis */
        /************************/
        silk_noise_shape_analysis_FLP( psEnc, &sEncCtrl, res_pitch_frame, x_frame );

        /***************************************************/
        /* Find linear prediction coefficients (LPC + LTP) */
        /***************************************************/
        silk_find_pred_coefs_FLP( psEnc, &sEncCtrl, &res_pitch, x_frame, condCoding );

        /****************************************/
        /* Process gains                        */
        /****************************************/
        silk_process_gains_FLP( psEnc, &sEncCtrl, condCoding );

        /*****************************************/
        /* Prefiltering for noise shaper         */
        /*****************************************/
        silk_prefilter_FLP( psEnc, &sEncCtrl, &xfw, x_frame );

        /****************************************/
        /* Low Bitrate Redundant Encoding       */
        /****************************************/
        silk_LBRR_encode_FLP( psEnc, &sEncCtrl, &xfw, condCoding );

        /* Loop over quantizer and entroy coding to control bitrate */
        maxIter = 6;
        gainMult_Q8 = (opus_int16)(((opus_int32)((1.0) * (float)((int8)1 << (8)) + 0.5)));
        found_lower = 0;
        found_upper = 0;
        gainsID = silk_gains_ID( &psEnc->sCmn.indices.GainsIndices, psEnc->sCmn.nb_subfr );
        gainsID_lower = -1;
        gainsID_upper = -1;
        /* Copy part of the input state */
        memcpy((byte*)(&sRangeEnc_copy), (byte*)(psRangeEnc), (((int)(ec_enc ' size  ))));
        memcpy((byte*)(&sNSQ_copy), (byte*)(&psEnc->sCmn.sNSQ), (((int)(silk_nsq_state ' size  ))));
        seed_copy = psEnc->sCmn.indices.Seed;
        ec_prevLagIndex_copy = psEnc->sCmn.ec_prevLagIndex;
        ec_prevSignalType_copy = psEnc->sCmn.ec_prevSignalType;
        for( iter = 0; ; iter++ ) {
            if( gainsID == gainsID_lower ) {
                nBits = nBits_lower;
            } else if( gainsID == gainsID_upper ) {
                nBits = nBits_upper;
            } else {
                /* Restore part of the input state */
                if( iter > 0 ) {
                    memcpy((byte*)(psRangeEnc), (byte*)(&sRangeEnc_copy), (((int)(ec_enc ' size  ))));
                    memcpy((byte*)&(psEnc->sCmn.sNSQ), (byte*)&(sNSQ_copy), (((int)(silk_nsq_state ' size  ))));
                    psEnc->sCmn.indices.Seed = (int1)seed_copy;
                    psEnc->sCmn.ec_prevLagIndex = ec_prevLagIndex_copy;
                    psEnc->sCmn.ec_prevSignalType = ec_prevSignalType_copy;
                }

                /*****************************************/
                /* Noise shaping quantization            */
                /*****************************************/
                silk_NSQ_wrapper_FLP( psEnc, &sEncCtrl, &psEnc->sCmn.indices, &psEnc->sCmn.sNSQ, &psEnc->sCmn.pulses, &xfw );

                /****************************************/
                /* Encode Parameters                    */
                /****************************************/
                silk_encode_indices( &psEnc->sCmn, psRangeEnc, psEnc->sCmn.nFramesEncoded, 0, condCoding );

                /****************************************/
                /* Encode Excitation Signal             */
                /****************************************/
                silk_encode_pulses( psRangeEnc, psEnc->sCmn.indices.signalType, psEnc->sCmn.indices.quantOffsetType,
                      &psEnc->sCmn.pulses, psEnc->sCmn.frame_length );

                nBits = ec_tell( psRangeEnc );

                if( useCBR == 0 && iter == 0 && nBits <= maxBits ) {
                    break;
                }
            }

            if( iter == maxIter ) {
                if( found_lower!=0 && ( gainsID == gainsID_lower || nBits > maxBits ) ) {
                    /* Restore output state from earlier iteration that did meet the bitrate budget */
                    memcpy((byte*)(psRangeEnc), (byte*)(&sRangeEnc_copy2), (((int)(ec_enc ' size  ))));
                    ;
                    memcpy((byte*)(psRangeEnc->buf), (byte*)(&ec_buf_copy), (int)(sRangeEnc_copy2.offs));
                    memcpy((byte*)(&psEnc->sCmn.sNSQ), (byte*)(&sNSQ_copy2), (((int)(silk_nsq_state ' size  ))));
                    psEnc->sShape.LastGainIndex = LastGainIndex_copy2;
                }
                break;
            }

            if( nBits > maxBits ) {
                if( found_lower == 0 && iter >= 2 ) {
                    /* Adjust the quantizer's rate/distortion tradeoff and discard previous "upper" results */
                    sEncCtrl.Lambda *= 1.5f;
                    found_upper = 0;
                    gainsID_upper = -1;
                } else {
                    found_upper = 1;
                    nBits_upper = nBits;
                    gainMult_upper = gainMult_Q8;
                    gainsID_upper = gainsID;
                }
            } else if( nBits < maxBits - 5 ) {
                found_lower = 1;
                nBits_lower = nBits;
                gainMult_lower = gainMult_Q8;
                if( gainsID != gainsID_lower ) {
                    gainsID_lower = gainsID;
                    /* Copy part of the output state */
                    memcpy((byte*)(&sRangeEnc_copy2), (byte*)(psRangeEnc), (((int)(ec_enc ' size  ))));
                    ;
                    memcpy((byte*)&(ec_buf_copy), (byte*)(psRangeEnc->buf), (int)(psRangeEnc->offs));
                    memcpy((byte*)(&sNSQ_copy2), (byte*)(&psEnc->sCmn.sNSQ), (((int)(silk_nsq_state ' size  ))));
                    LastGainIndex_copy2 = psEnc->sShape.LastGainIndex;
                }
            } else {
                /* Within 5 bits of budget: close enough */
                break;
            }

            if( ( found_lower & found_upper ) == 0 ) {
                /* Adjust gain according to high-rate rate/distortion curve */
                opus_int32 gain_factor_Q16;
                gain_factor_Q16 = silk_log2lin( ((opus_int32)((opus_uint32)(nBits - maxBits)<<(7))) / psEnc->sCmn.frame_length + ((opus_int32)((16.0) * (float)((int8)1 << (7)) + 0.5)) );
                gain_factor_Q16 = silk_min_32( gain_factor_Q16, ((opus_int32)((2.0) * (float)((int8)1 << (16)) + 0.5)) );
                if( nBits > maxBits ) {
                    gain_factor_Q16 = silk_max_32( gain_factor_Q16, ((opus_int32)((1.3) * (float)((int8)1 << (16)) + 0.5)) );
                }
                gainMult_Q8 = (opus_int16)(((((gain_factor_Q16) >> 16) * (opus_int32)((opus_int16)(gainMult_Q8))) + ((((gain_factor_Q16) & 0x0000FFFF) * (opus_int32)((opus_int16)(gainMult_Q8))) >> 16)));
            } else {
                /* Adjust gain by interpolating */
                gainMult_Q8 = (opus_int16)(gainMult_lower + ( ( gainMult_upper - gainMult_lower ) * ( maxBits - nBits_lower ) ) / ( nBits_upper - nBits_lower ));
                /* New gain multplier must be between 25% and 75% of old range (note that gainMult_upper < gainMult_lower) */
                if( gainMult_Q8 > (((gainMult_lower)) + ((((gainMult_upper - gainMult_lower))>>((2))))) ) {
                    gainMult_Q8 = (opus_int16)((((gainMult_lower)) + ((((gainMult_upper - gainMult_lower))>>((2))))));
                } else
                if( gainMult_Q8 < (((gainMult_upper)) - ((((gainMult_upper - gainMult_lower))>>((2))))) ) {
                    gainMult_Q8 = (opus_int16)((((gainMult_upper)) - ((((gainMult_upper - gainMult_lower))>>((2))))));
                }
            }

            for( i = 0; i < psEnc->sCmn.nb_subfr; i++ ) {
                pGains_Q16[ i ] = (((opus_int32)((opus_uint32)(((((((opus_int32)-0x80000000))>>((8)))) > (((0x7FFFFFFF)>>((8)))) ? (((((((sEncCtrl.GainsUnq_Q16[ i ]) >> 16) * (opus_int32)((opus_int16)(gainMult_Q8))) + ((((sEncCtrl.GainsUnq_Q16[ i ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(gainMult_Q8))) >> 16)))) > (((((opus_int32)-0x80000000))>>((8)))) ? (((((opus_int32)-0x80000000))>>((8)))) : (((((((sEncCtrl.GainsUnq_Q16[ i ]) >> 16) * (opus_int32)((opus_int16)(gainMult_Q8))) + ((((sEncCtrl.GainsUnq_Q16[ i ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(gainMult_Q8))) >> 16)))) < (((0x7FFFFFFF)>>((8)))) ? (((0x7FFFFFFF)>>((8)))) : ((((((sEncCtrl.GainsUnq_Q16[ i ]) >> 16) * (opus_int32)((opus_int16)(gainMult_Q8))) + ((((sEncCtrl.GainsUnq_Q16[ i ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(gainMult_Q8))) >> 16)))))) : (((((((sEncCtrl.GainsUnq_Q16[ i ]) >> 16) * (opus_int32)((opus_int16)(gainMult_Q8))) + ((((sEncCtrl.GainsUnq_Q16[ i ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(gainMult_Q8))) >> 16)))) > (((0x7FFFFFFF)>>((8)))) ? (((0x7FFFFFFF)>>((8)))) : (((((((sEncCtrl.GainsUnq_Q16[ i ]) >> 16) * (opus_int32)((opus_int16)(gainMult_Q8))) + ((((sEncCtrl.GainsUnq_Q16[ i ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(gainMult_Q8))) >> 16)))) < (((((opus_int32)-0x80000000))>>((8)))) ? (((((opus_int32)-0x80000000))>>((8)))) : ((((((sEncCtrl.GainsUnq_Q16[ i ]) >> 16) * (opus_int32)((opus_int16)(gainMult_Q8))) + ((((sEncCtrl.GainsUnq_Q16[ i ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(gainMult_Q8))) >> 16))))))))<<((8)))));
            }

            /* Quantize gains */
            psEnc->sShape.LastGainIndex = sEncCtrl.lastGainIndexPrev;
            silk_gains_quant( &psEnc->sCmn.indices.GainsIndices, &pGains_Q16,
                  &psEnc->sShape.LastGainIndex, (int)(condCoding == 2), psEnc->sCmn.nb_subfr );

            /* Unique identifier of gains vector */
            gainsID = silk_gains_ID( &psEnc->sCmn.indices.GainsIndices, psEnc->sCmn.nb_subfr );

            /* Overwrite unquantized gains with quantized gains and convert back to Q0 from Q16 */
            for( i = 0; i < psEnc->sCmn.nb_subfr; i++ ) {
                sEncCtrl.Gains[ i ] = (float)pGains_Q16[ i ] / 65536.0f;
            }
        }
    }

    /* Update input buffer */
    
memmove((byte*)(&psEnc->x_buf), (byte*)(&psEnc->x_buf[ psEnc->sCmn.frame_length ]), (( psEnc->sCmn.ltp_mem_length + 5 * psEnc->sCmn.fs_kHz ) * ((int)(float ' size  ))));

    /* Parameters needed for next frame */
    psEnc->sCmn.prevLag        = sEncCtrl.pitchL[ psEnc->sCmn.nb_subfr - 1 ];
    psEnc->sCmn.prevSignalType = psEnc->sCmn.indices.signalType;

    /* Exit without entropy coding */
    if( psEnc->sCmn.prefillFlag != 0) {
        /* No payload */
        *pnBytesOut = 0;
        return ret;
    }

    /****************************************/
    /* Finalize payload                     */
    /****************************************/
    psEnc->sCmn.first_frame_after_reset = 0;
    /* Payload size */
    *pnBytesOut = ((ec_tell( psRangeEnc ) + 7)>>(3));

    return ret;
}


/* Low-Bitrate Redundancy (LBRR) encoding. Reuse all parameters but encode excitation at lower bitrate  */
public
void silk_LBRR_encode_FLP(
    silk_encoder_state_FLP          *psEnc,                             /* I/O  Encoder state FLP                           */
    silk_encoder_control_FLP        *psEncCtrl,                         /* I/O  Encoder control FLP                         */
          float                xfw*,                              /* I    Input signal                                */
    int                        condCoding                          /* I    The type of conditional coding used so far for this frame */
)
{
    int     k;
    opus_int32   Gains_Q16[ 4 ];
    float   TempGains[ 4 ];
    SideInfoIndices *psIndices_LBRR = &psEnc->sCmn.indices_LBRR[ psEnc->sCmn.nFramesEncoded ];
    silk_nsq_state sNSQ_LBRR;

    /*******************************************/
    /* Control use of inband LBRR              */
    /*******************************************/
    if( psEnc->sCmn.LBRR_enabled != 0 && psEnc->sCmn.speech_activity_Q8 > ((opus_int32)((0.3f) * (float)((int8)1 << (8)) + 0.5)) ) {
        psEnc->sCmn.LBRR_flags[ psEnc->sCmn.nFramesEncoded ] = 1;

        /* Copy noise shaping quantizer state and quantization indices from regular encoding */
        memcpy((byte*)(&sNSQ_LBRR), (byte*)(&psEnc->sCmn.sNSQ), (((int)(silk_nsq_state ' size  ))));
        memcpy((byte*)(psIndices_LBRR), (byte*)(&psEnc->sCmn.indices), (((int)(SideInfoIndices ' size  ))));

        /* Save original gains */
        memcpy((byte*)&(TempGains), (byte*)&(psEncCtrl->Gains), (psEnc->sCmn.nb_subfr * ((int)(float ' size  ))));

        if( psEnc->sCmn.nFramesEncoded == 0 || psEnc->sCmn.LBRR_flags[ psEnc->sCmn.nFramesEncoded - 1 ] == 0 ) {
            /* First frame in packet or previous frame not LBRR coded */
            psEnc->sCmn.LBRRprevLastGainIndex = psEnc->sShape.LastGainIndex;

            /* Increase Gains to get target LBRR rate */
            psIndices_LBRR->GainsIndices[ 0 ] += (int1)psEnc->sCmn.LBRR_GainIncreases;
            psIndices_LBRR->GainsIndices[ 0 ] = (int1)silk_min_int( psIndices_LBRR->GainsIndices[ 0 ], 64 - 1 );
        }

        /* Decode to get gains in sync with decoder */
        silk_gains_dequant( &Gains_Q16, &psIndices_LBRR->GainsIndices,
            &psEnc->sCmn.LBRRprevLastGainIndex, (int)(condCoding == 2), psEnc->sCmn.nb_subfr );

        /* Overwrite unquantized gains with quantized gains and convert back to Q0 from Q16 */
        for( k = 0; k <  psEnc->sCmn.nb_subfr; k++ ) {
            psEncCtrl->Gains[ k ] = (float)Gains_Q16[ k ] * ( 1.0f / 65536.0f );
        }

        /*****************************************/
        /* Noise shaping quantization            */
        /*****************************************/
        silk_NSQ_wrapper_FLP( psEnc, psEncCtrl, psIndices_LBRR, &sNSQ_LBRR,
            &psEnc->sCmn.pulses_LBRR[ psEnc->sCmn.nFramesEncoded ], xfw );

        /* Restore original gains */
        memcpy((byte*)&(psEncCtrl->Gains), (byte*)&(TempGains), (psEnc->sCmn.nb_subfr * ((int)(float ' size  ))));
    }
}
#end unsafe
