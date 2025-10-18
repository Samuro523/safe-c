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
use structs_flp, control, structs, control_audio_bandwidth;
use resampler, resampler_structs, sigproc_flp, tables, os_support, sigproc_fix;


/* Control encoder */

public
int silk_control_encoder(
    silk_encoder_state_FLP          *psEnc,                                 /* I/O  Pointer to Silk encoder state                                               */
    silk_EncControlStruct           *encControl,                            /* I    Control structure                                                           */
          opus_int32                TargetRate_bps,                         /* I    Target max bitrate (bps)                                                    */
          int                  allow_bw_switch,                        /* I    Flag to allow switching audio bandwidth                                     */
          int                  channelNb,                              /* I    Channel number                                                              */
          int                  force_fs_kHz
)
{
    int   fs_kHz, ret = 0;

    psEnc->sCmn.useDTX                 = encControl->useDTX;
    psEnc->sCmn.useCBR                 = encControl->useCBR;
    psEnc->sCmn.API_fs_Hz              = encControl->API_sampleRate;
    psEnc->sCmn.maxInternal_fs_Hz      = encControl->maxInternalSampleRate;
    psEnc->sCmn.minInternal_fs_Hz      = encControl->minInternalSampleRate;
    psEnc->sCmn.desiredInternal_fs_Hz  = encControl->desiredInternalSampleRate;
    psEnc->sCmn.useInBandFEC           = encControl->useInBandFEC;
    psEnc->sCmn.nChannelsAPI           = encControl->nChannelsAPI;
    psEnc->sCmn.nChannelsInternal      = encControl->nChannelsInternal;
    psEnc->sCmn.allow_bandwidth_switch = allow_bw_switch;
    psEnc->sCmn.channelNb              = channelNb;

    if( psEnc->sCmn.controlled_since_last_payload != 0 && psEnc->sCmn.prefillFlag == 0 ) {
        if( psEnc->sCmn.API_fs_Hz != psEnc->sCmn.prev_API_fs_Hz && psEnc->sCmn.fs_kHz > 0 ) {
            /* Change in API sampling rate in the middle of encoding a packet */
            ret += silk_setup_resamplers( psEnc, psEnc->sCmn.fs_kHz );
        }
        return ret;
    }

    /* Beyond this point we know that there are no previously coded frames in the payload buffer */

    /********************************************/
    /* Determine internal sampling rate         */
    /********************************************/
    fs_kHz = silk_control_audio_bandwidth( &psEnc->sCmn, encControl );
    if( force_fs_kHz != 0 ) {
       fs_kHz = force_fs_kHz;
    }
    /********************************************/
    /* Prepare resampler and buffered data      */
    /********************************************/
    ret += silk_setup_resamplers( psEnc, fs_kHz );

    /********************************************/
    /* Set internal sampling frequency          */
    /********************************************/
    ret += silk_setup_fs( psEnc, fs_kHz, encControl->payloadSize_ms );

    /********************************************/
    /* Set encoding complexity                  */
    /********************************************/
    ret += silk_setup_complexity( &psEnc->sCmn, encControl->complexity  );

    /********************************************/
    /* Set packet loss rate measured by farend  */
    /********************************************/
    psEnc->sCmn.PacketLoss_perc = encControl->packetLossPercentage;

    /********************************************/
    /* Set LBRR usage                           */
    /********************************************/
    ret += silk_setup_LBRR( &psEnc->sCmn, TargetRate_bps );

    psEnc->sCmn.controlled_since_last_payload = 1;

    return ret;
}

public
int silk_setup_resamplers(
    silk_encoder_state_FLP          *psEnc,             /* I/O                      */
    int                         fs_kHz              /* I                        */
)
{
    int   ret = 0;
    opus_int32 nSamples_temp;

    if( psEnc->sCmn.fs_kHz != fs_kHz || psEnc->sCmn.prev_API_fs_Hz != psEnc->sCmn.API_fs_Hz )
    {
        if( psEnc->sCmn.fs_kHz == 0 ) {
            /* Initialize the resampler for enc_API.c preparing resampling from API_fs_Hz to fs_kHz */
            ret += silk_resampler_init( &psEnc->sCmn.resampler_state, psEnc->sCmn.API_fs_Hz, fs_kHz * 1000, 1 );
        } else {
            /* Allocate worst case space for temporary upsampling, 8 to 48 kHz, so a factor 6 */
            opus_int16 x_buf_API_fs_Hz[ ( 2 * ( 5 * 4 ) + 5 ) * 48 ];
            silk_resampler_state_struct  temp_resampler_state;

            opus_int16 x_bufFIX[ 2 * ( ( 5 * 4 ) * 16 ) + ( 5 * 16 ) ];

            nSamples_temp = ((opus_int32)((opus_uint32)(psEnc->sCmn.frame_length)<<(1))) + 5 * psEnc->sCmn.fs_kHz;

            silk_float2short_array( &x_bufFIX, &psEnc->x_buf, nSamples_temp );

            /* Initialize resampler for temporary resampling of x_buf data to API_fs_Hz */
            ret += silk_resampler_init( &temp_resampler_state, ((opus_int32)((opus_int16)(psEnc->sCmn.fs_kHz)) * (opus_int32)((opus_int16)(1000))), psEnc->sCmn.API_fs_Hz, 0 );

            /* Temporary resampling of x_buf data to API_fs_Hz */
            ret += silk_resampler( &temp_resampler_state, &x_buf_API_fs_Hz, &x_bufFIX, nSamples_temp );

            /* Calculate number of samples that has been temporarily upsampled */
            nSamples_temp = ((opus_int32)((nSamples_temp * psEnc->sCmn.API_fs_Hz) / (((opus_int32)((opus_int16)(psEnc->sCmn.fs_kHz)) * (opus_int32)((opus_int16)(1000))))));

            /* Initialize the resampler for enc_API.c preparing resampling from API_fs_Hz to fs_kHz */
            ret += silk_resampler_init( &psEnc->sCmn.resampler_state, psEnc->sCmn.API_fs_Hz, ((opus_int32)((opus_int16)(fs_kHz)) * (opus_int32)((opus_int16)(1000))), 1 );

            /* Correct resampler state by resampling buffered data from API_fs_Hz to fs_kHz */
            ret += silk_resampler( &psEnc->sCmn.resampler_state, &x_bufFIX, &x_buf_API_fs_Hz, nSamples_temp );

            silk_short2float_array( &psEnc->x_buf, &x_bufFIX, ( 2 * ( 5 * 4 ) + 5 ) * fs_kHz );
        }
    }

    psEnc->sCmn.prev_API_fs_Hz = psEnc->sCmn.API_fs_Hz;

    return ret;
}

public
int silk_setup_fs(
    silk_encoder_state_FLP          *psEnc,             /* I/O                      */
    int                        fs_kHz,             /* I                        */
    int                        PacketSize_ms       /* I                        */
)
{
    int ret = 0;

    /* Set packet size */
    if( PacketSize_ms != psEnc->sCmn.PacketSize_ms ) {
        if( ( PacketSize_ms !=  10 ) &&
            ( PacketSize_ms !=  20 ) &&
            ( PacketSize_ms !=  40 ) &&
            ( PacketSize_ms !=  60 ) ) {
            ret = -103;
        }
        if( PacketSize_ms <= 10 ) {
            psEnc->sCmn.nFramesPerPacket = 1;
            psEnc->sCmn.nb_subfr = PacketSize_ms == 10 ? 2 : 1;
            psEnc->sCmn.frame_length = ((opus_int32)((opus_int16)(PacketSize_ms)) * (opus_int32)((opus_int16)(fs_kHz)));
            psEnc->sCmn.pitch_LPC_win_length = ((opus_int32)((opus_int16)(( 10 + (2 << 1) ))) * (opus_int32)((opus_int16)(fs_kHz)));
            if( psEnc->sCmn.fs_kHz == 8 ) {
                psEnc->sCmn.pitch_contour_iCDF = &silk_pitch_contour_10_ms_NB_iCDF;
            } else {
                psEnc->sCmn.pitch_contour_iCDF = &silk_pitch_contour_10_ms_iCDF;
            }
        } else {
            psEnc->sCmn.nFramesPerPacket = ((opus_int32)((PacketSize_ms) / (( 5 * 4 ))));
            psEnc->sCmn.nb_subfr = 4;
            psEnc->sCmn.frame_length = ((opus_int32)((opus_int16)(20)) * (opus_int32)((opus_int16)(fs_kHz)));
            psEnc->sCmn.pitch_LPC_win_length = ((opus_int32)((opus_int16)(( 20 + (2 << 1) ))) * (opus_int32)((opus_int16)(fs_kHz)));
            if( psEnc->sCmn.fs_kHz == 8 ) {
                psEnc->sCmn.pitch_contour_iCDF = &silk_pitch_contour_NB_iCDF;
            } else {
                psEnc->sCmn.pitch_contour_iCDF = &silk_pitch_contour_iCDF;
            }
        }
        psEnc->sCmn.PacketSize_ms  = PacketSize_ms;
        psEnc->sCmn.TargetRate_bps = 0;         /* trigger new SNR computation */
    }

    /* Set internal sampling frequency */
    ;
    ;
    if( psEnc->sCmn.fs_kHz != fs_kHz ) {
        /* reset part of the state */
        memset((char*)(&psEnc->sShape), (0), (((int)((psEnc->sShape) ' size  ))));
        memset((char*)(&psEnc->sPrefilt), (0), (((int)((psEnc->sPrefilt) ' size  ))));
        memset((char*)(&psEnc->sCmn.sNSQ), (0), (((int)((psEnc->sCmn.sNSQ) ' size  ))));
        memset((char*)&(psEnc->sCmn.prev_NLSFq_Q15), (0), (((int)((psEnc->sCmn.prev_NLSFq_Q15) ' size  ))));
        memset((char*)(&psEnc->sCmn.sLP.In_LP_State), (0), (((int)((psEnc->sCmn.sLP.In_LP_State) ' size  ))));
        psEnc->sCmn.inputBufIx                  = 0;
        psEnc->sCmn.nFramesEncoded              = 0;
        psEnc->sCmn.TargetRate_bps              = 0;     /* trigger new SNR computation */

        /* Initialize non-zero parameters */
        psEnc->sCmn.prevLag                     = 100;
        psEnc->sCmn.first_frame_after_reset     = 1;
        psEnc->sPrefilt.lagPrev                 = 100;
        psEnc->sShape.LastGainIndex             = 10;
        psEnc->sCmn.sNSQ.lagPrev                = 100;
        psEnc->sCmn.sNSQ.prev_gain_Q16          = 65536;
        psEnc->sCmn.prevSignalType              = 0;

        psEnc->sCmn.fs_kHz = fs_kHz;
        if( psEnc->sCmn.fs_kHz == 8 ) {
            if( psEnc->sCmn.nb_subfr == 4 ) {
                psEnc->sCmn.pitch_contour_iCDF = &silk_pitch_contour_NB_iCDF;
            } else {
                psEnc->sCmn.pitch_contour_iCDF = &silk_pitch_contour_10_ms_NB_iCDF;
            }
        } else {
            if( psEnc->sCmn.nb_subfr == 4 ) {
                psEnc->sCmn.pitch_contour_iCDF = &silk_pitch_contour_iCDF;
            } else {
                psEnc->sCmn.pitch_contour_iCDF = &silk_pitch_contour_10_ms_iCDF;
            }
        }
        if( psEnc->sCmn.fs_kHz == 8 || psEnc->sCmn.fs_kHz == 12 ) {
            psEnc->sCmn.predictLPCOrder = 10;
            psEnc->sCmn.psNLSF_CB  = &silk_NLSF_CB_NB_MB;
        } else {
            psEnc->sCmn.predictLPCOrder = 16;
            psEnc->sCmn.psNLSF_CB  = &silk_NLSF_CB_WB;
        }
        psEnc->sCmn.subfr_length   = 5 * fs_kHz;
        psEnc->sCmn.frame_length   = ((opus_int32)((opus_int16)(psEnc->sCmn.subfr_length)) * (opus_int32)((opus_int16)(psEnc->sCmn.nb_subfr)));
        psEnc->sCmn.ltp_mem_length = ((opus_int32)((opus_int16)(20)) * (opus_int32)((opus_int16)(fs_kHz)));
        psEnc->sCmn.la_pitch       = ((opus_int32)((opus_int16)(2)) * (opus_int32)((opus_int16)(fs_kHz)));
        psEnc->sCmn.max_pitch_lag  = ((opus_int32)((opus_int16)(18)) * (opus_int32)((opus_int16)(fs_kHz)));
        if( psEnc->sCmn.nb_subfr == 4 ) {
            psEnc->sCmn.pitch_LPC_win_length = ((opus_int32)((opus_int16)(( 20 + (2 << 1) ))) * (opus_int32)((opus_int16)(fs_kHz)));
        } else {
            psEnc->sCmn.pitch_LPC_win_length = ((opus_int32)((opus_int16)(( 10 + (2 << 1) ))) * (opus_int32)((opus_int16)(fs_kHz)));
        }
        if( psEnc->sCmn.fs_kHz == 16 ) {
            psEnc->sCmn.mu_LTP_Q9 = ((opus_int32)((0.02f) * (float)((int8)1 << (9)) + 0.5));
            psEnc->sCmn.pitch_lag_low_bits_iCDF = &silk_uniform8_iCDF;
        } else if( psEnc->sCmn.fs_kHz == 12 ) {
            psEnc->sCmn.mu_LTP_Q9 = ((opus_int32)((0.025f) * (float)((int8)1 << (9)) + 0.5));
            psEnc->sCmn.pitch_lag_low_bits_iCDF = &silk_uniform6_iCDF;
        } else {
            psEnc->sCmn.mu_LTP_Q9 = ((opus_int32)((0.03f) * (float)((int8)1 << (9)) + 0.5));
            psEnc->sCmn.pitch_lag_low_bits_iCDF = &silk_uniform4_iCDF;
        }
    }

    /* Check that settings are valid */
    ;

    return ret;
}

public
int silk_setup_complexity(
    silk_encoder_state              *psEncC,            /* I/O                      */
    int                        Complexity          /* I                        */
)
{
    int ret = 0;

    /* Set encoding complexity */
    ;
    if( Complexity < 2 ) {
        psEncC->pitchEstimationComplexity       = 0;
        psEncC->pitchEstimationThreshold_Q16    = ((opus_int32)((0.8) * (float)((int8)1 << (16)) + 0.5));
        psEncC->pitchEstimationLPCOrder         = 6;
        psEncC->shapingLPCOrder                 = 8;
        psEncC->la_shape                        = 3 * psEncC->fs_kHz;
        psEncC->nStatesDelayedDecision          = 1;
        psEncC->useInterpolatedNLSFs            = 0;
        psEncC->LTPQuantLowComplexity           = 1;
        psEncC->NLSF_MSVQ_Survivors             = 2;
        psEncC->warping_Q16                     = 0;
    } else if( Complexity < 4 ) {
        psEncC->pitchEstimationComplexity       = 1;
        psEncC->pitchEstimationThreshold_Q16    = ((opus_int32)((0.76) * (float)((int8)1 << (16)) + 0.5));
        psEncC->pitchEstimationLPCOrder         = 8;
        psEncC->shapingLPCOrder                 = 10;
        psEncC->la_shape                        = 5 * psEncC->fs_kHz;
        psEncC->nStatesDelayedDecision          = 1;
        psEncC->useInterpolatedNLSFs            = 0;
        psEncC->LTPQuantLowComplexity           = 0;
        psEncC->NLSF_MSVQ_Survivors             = 4;
        psEncC->warping_Q16                     = 0;
    } else if( Complexity < 6 ) {
        psEncC->pitchEstimationComplexity       = 1;
        psEncC->pitchEstimationThreshold_Q16    = ((opus_int32)((0.74) * (float)((int8)1 << (16)) + 0.5));
        psEncC->pitchEstimationLPCOrder         = 10;
        psEncC->shapingLPCOrder                 = 12;
        psEncC->la_shape                        = 5 * psEncC->fs_kHz;
        psEncC->nStatesDelayedDecision          = 2;
        psEncC->useInterpolatedNLSFs            = 1;
        psEncC->LTPQuantLowComplexity           = 0;
        psEncC->NLSF_MSVQ_Survivors             = 8;
        psEncC->warping_Q16                     = psEncC->fs_kHz * ((opus_int32)((0.015f) * (float)((int8)1 << (16)) + 0.5));
    } else if( Complexity < 8 ) {
        psEncC->pitchEstimationComplexity       = 1;
        psEncC->pitchEstimationThreshold_Q16    = ((opus_int32)((0.72) * (float)((int8)1 << (16)) + 0.5));
        psEncC->pitchEstimationLPCOrder         = 12;
        psEncC->shapingLPCOrder                 = 14;
        psEncC->la_shape                        = 5 * psEncC->fs_kHz;
        psEncC->nStatesDelayedDecision          = 3;
        psEncC->useInterpolatedNLSFs            = 1;
        psEncC->LTPQuantLowComplexity           = 0;
        psEncC->NLSF_MSVQ_Survivors             = 16;
        psEncC->warping_Q16                     = psEncC->fs_kHz * ((opus_int32)((0.015f) * (float)((int8)1 << (16)) + 0.5));
    } else {
        psEncC->pitchEstimationComplexity       = 2;
        psEncC->pitchEstimationThreshold_Q16    = ((opus_int32)((0.7) * (float)((int8)1 << (16)) + 0.5));
        psEncC->pitchEstimationLPCOrder         = 16;
        psEncC->shapingLPCOrder                 = 16;
        psEncC->la_shape                        = 5 * psEncC->fs_kHz;
        psEncC->nStatesDelayedDecision          = 4;
        psEncC->useInterpolatedNLSFs            = 1;
        psEncC->LTPQuantLowComplexity           = 0;
        psEncC->NLSF_MSVQ_Survivors             = 32;
        psEncC->warping_Q16                     = psEncC->fs_kHz * ((opus_int32)((0.015f) * (float)((int8)1 << (16)) + 0.5));
    }

    /* Do not allow higher pitch estimation LPC order than predict LPC order */
    psEncC->pitchEstimationLPCOrder = silk_min_int( psEncC->pitchEstimationLPCOrder, psEncC->predictLPCOrder );
    psEncC->shapeWinLength          = 5 * psEncC->fs_kHz + 2 * psEncC->la_shape;
    psEncC->Complexity              = Complexity;

    ;
    ;
    ;
    ;
    ;
    ;
    ;

    return ret;
}

public
int silk_setup_LBRR(
    silk_encoder_state          *psEncC,            /* I/O                      */
          opus_int32            TargetRate_bps      /* I                        */
)
{
    int   ret = 0;
    opus_int32 LBRR_rate_thres_bps;

    psEncC->LBRR_enabled = 0;
    if( psEncC->useInBandFEC != 0 && psEncC->PacketLoss_perc > 0 ) {
        if( psEncC->fs_kHz == 8 ) {
            LBRR_rate_thres_bps = 12000;
        } else if( psEncC->fs_kHz == 12 ) {
            LBRR_rate_thres_bps = 14000;
        } else {
            LBRR_rate_thres_bps = 16000;
        }
        LBRR_rate_thres_bps = ((((((LBRR_rate_thres_bps) * (125 - (((psEncC->PacketLoss_perc) < (25)) ? (psEncC->PacketLoss_perc) : (25))))) >> 16) * (opus_int32)((opus_int16)(((opus_int32)((0.01) * (float)((int8)1 << (16)) + 0.5))))) + ((((((LBRR_rate_thres_bps) * (125 - (((psEncC->PacketLoss_perc) < (25)) ? (psEncC->PacketLoss_perc) : (25))))) & 0x0000FFFF) * (opus_int32)((opus_int16)(((opus_int32)((0.01) * (float)((int8)1 << (16)) + 0.5))))) >> 16));

        if( TargetRate_bps > LBRR_rate_thres_bps ) {
            /* Set gain increase for coding LBRR excitation */
            psEncC->LBRR_enabled = 1;
            psEncC->LBRR_GainIncreases = silk_max_int( 7 - (((((opus_int32)psEncC->PacketLoss_perc) >> 16) * (opus_int32)((opus_int16)(((opus_int32)((0.4) * (float)((int8)1 << (16)) + 0.5))))) + (((((opus_int32)psEncC->PacketLoss_perc) & 0x0000FFFF) * (opus_int32)((opus_int16)(((opus_int32)((0.4) * (float)((int8)1 << (16)) + 0.5))))) >> 16)), 2 );
        }
    }

    return ret;
}
#end unsafe
