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
use resampler_structs;

/************************************/
/* Noise shaping quantization state */
/************************************/
struct  silk_nsq_state
{
    opus_int16                  xq[           2 * ( ( 5 * 4 ) * 16 ) ]; /* Buffer for quantized output signal                             */
    opus_int32                  sLTP_shp_Q14[ 2 * ( ( 5 * 4 ) * 16 ) ];
    opus_int32                  sLPC_Q14[ ( 5 * 16 ) + 32 ];
    opus_int32                  sAR2_Q14[ 16 ];
    opus_int32                  sLF_AR_shp_Q14;
    int                    lagPrev;
    int                    sLTP_buf_idx;
    int                    sLTP_shp_buf_idx;
    opus_int32                  rand_seed;
    opus_int32                  prev_gain_Q16;
    int                    rewhite_flag;
}

/********************************/
/* VAD state                    */
/********************************/
struct  silk_VAD_state
{
    opus_int32                  AnaState[ 2 ];                  /* Analysis filterbank state: 0-8 kHz                                   */
    opus_int32                  AnaState1[ 2 ];                 /* Analysis filterbank state: 0-4 kHz                                   */
    opus_int32                  AnaState2[ 2 ];                 /* Analysis filterbank state: 0-2 kHz                                   */
    opus_int32                  XnrgSubfr[ 4 ];       /* Subframe energies                                                    */
    opus_int32                  NrgRatioSmth_Q8[ 4 ]; /* Smoothed energy level in each band                                   */
    opus_int16                  HPstate;                        /* State of differentiator in the lowest band                           */
    opus_int32                  NL[ 4 ];              /* Noise energy level in each band                                      */
    opus_int32                  inv_NL[ 4 ];          /* Inverse noise energy level in each band                              */
    opus_int32                  NoiseLevelBias[ 4 ];  /* Noise level estimator bias/offset                                    */
    opus_int32                  counter;                        /* Frame counter used in the initial phase                              */
}

/* Variable cut-off low-pass filter state */
struct  silk_LP_state
{
    opus_int32                   In_LP_State[ 2 ];           /* Low pass filter state */
    opus_int32                   transition_frame_no;        /* Counter which is mapped to a cut-off frequency */
    int                     mode;                       /* Operating mode, <0: switch down, >0: switch up; 0: do nothing           */
}

/* Structure containing NLSF codebook */
struct  silk_NLSF_CB_struct
{
          opus_int16             nVectors;
          opus_int16             order;
          opus_int16             quantStepSize_Q16;
          opus_int16             invQuantStepSize_Q6;
          byte         [512]     CB1_NLSF_Q8;
          byte          [64]     CB1_iCDF;
          byte          [30]     pred_Q8;
          byte         [256]     ec_sel;
          byte          [72]     ec_iCDF;
          byte          [72]     ec_Rates_Q5;
          opus_int16    [17]     deltaMin_Q15;
}

struct  stereo_enc_state
{
    opus_int16                   pred_prev_Q13[ 2 ];
    opus_int16                   sMid[ 2 ];
    opus_int16                   sSide[ 2 ];
    opus_int32                   mid_side_amp_Q0[ 4 ];
    opus_int16                   smth_width_Q14;
    opus_int16                   width_prev_Q14;
    opus_int16                   silent_side_len;
    int1                    predIx[ 3 ][ 2 ][ 3 ];
    int1                    mid_only_flags[ 3 ];
}

struct  stereo_dec_state
{
    opus_int16                   pred_prev_Q13[ 2 ];
    opus_int16                   sMid[ 2 ];
    opus_int16                   sSide[ 2 ];
}

struct  SideInfoIndices
{
    int1                    GainsIndices[ 4 ];
    int1                    LTPIndex[ 4 ];
    int1                    NLSFIndices[ 16 + 1 ];
    opus_int16                   lagIndex;
    int1                    contourIndex;
    int1                    signalType;
    int1                    quantOffsetType;
    int1                    NLSFInterpCoef_Q2;
    int1                    PERIndex;
    int1                    LTP_scaleIndex;
    int1                    Seed;
}

/********************************/
/* Encoder state                */
/********************************/
struct  silk_encoder_state
{
    opus_int32                   In_HP_State[ 2 ];                  /* High pass filter state                                           */
    opus_int32                   variable_HP_smth1_Q15;             /* State of first smoother                                          */
    opus_int32                   variable_HP_smth2_Q15;             /* State of second smoother                                         */
    silk_LP_state                sLP;                               /* Low pass filter state                                            */
    silk_VAD_state               sVAD;                              /* Voice activity detector state                                    */
    silk_nsq_state               sNSQ;                              /* Noise Shape Quantizer State                                      */
    opus_int16                   prev_NLSFq_Q15[ 16 ];   /* Previously quantized NLSF vector                                 */
    int                     speech_activity_Q8;                /* Speech activity                                                  */
    int                     allow_bandwidth_switch;            /* Flag indicating that switching of internal bandwidth is allowed  */
    int1                    LBRRprevLastGainIndex;
    int1                    prevSignalType;
    int                     prevLag;
    int                     pitch_LPC_win_length;
    int                     max_pitch_lag;                     /* Highest possible pitch lag (samples)                             */
    opus_int32                   API_fs_Hz;                         /* API sampling frequency (Hz)                                      */
    opus_int32                   prev_API_fs_Hz;                    /* Previous API sampling frequency (Hz)                             */
    int                     maxInternal_fs_Hz;                 /* Maximum internal sampling frequency (Hz)                         */
    int                     minInternal_fs_Hz;                 /* Minimum internal sampling frequency (Hz)                         */
    int                     desiredInternal_fs_Hz;             /* Soft request for internal sampling frequency (Hz)                */
    int                     fs_kHz;                            /* Internal sampling frequency (kHz)                                */
    int                     nb_subfr;                          /* Number of 5 ms subframes in a frame                              */
    int                     frame_length;                      /* Frame length (samples)                                           */
    int                     subfr_length;                      /* Subframe length (samples)                                        */
    int                     ltp_mem_length;                    /* Length of LTP memory                                             */
    int                     la_pitch;                          /* Look-ahead for pitch analysis (samples)                          */
    int                     la_shape;                          /* Look-ahead for noise shape analysis (samples)                    */
    int                     shapeWinLength;                    /* Window length for noise shape analysis (samples)                 */
    opus_int32                   TargetRate_bps;                    /* Target bitrate (bps)                                             */
    int                     PacketSize_ms;                     /* Number of milliseconds to put in each packet                     */
    int                     PacketLoss_perc;                   /* Packet loss rate measured by farend                              */
    opus_int32                   frameCounter;
    int                     Complexity;                        /* Complexity setting                                               */
    int                     nStatesDelayedDecision;            /* Number of states in delayed decision quantization                */
    int                     useInterpolatedNLSFs;              /* Flag for using NLSF interpolation                                */
    int                     shapingLPCOrder;                   /* Filter order for noise shaping filters                           */
    int                     predictLPCOrder;                   /* Filter order for prediction filters                              */
    int                     pitchEstimationComplexity;         /* Complexity level for pitch estimator                             */
    int                     pitchEstimationLPCOrder;           /* Whitening filter order for pitch estimator                       */
    opus_int32                   pitchEstimationThreshold_Q16;      /* Threshold for pitch estimator                                    */
    int                     LTPQuantLowComplexity;             /* Flag for low complexity LTP quantization                         */
    int                     mu_LTP_Q9;                         /* Rate-distortion tradeoff in LTP quantization                     */
    int                     NLSF_MSVQ_Survivors;               /* Number of survivors in NLSF MSVQ                                 */
    int                     first_frame_after_reset;           /* Flag for deactivating NLSF interpolation, pitch prediction       */
    int                     controlled_since_last_payload;     /* Flag for ensuring codec_control only runs once per packet        */
    int                     warping_Q16;                       /* Warping parameter for warped noise shaping                       */
    int                     useCBR;                            /* Flag to enable      ant bitrate                                  */
    int                     prefillFlag;                       /* Flag to indicate that only buffers are prefilled, no coding      */
          byte             *pitch_lag_low_bits_iCDF;          /* Pointer to iCDF table for low bits of pitch lag index            */
          byte             *pitch_contour_iCDF;               /* Pointer to iCDF table for pitch contour index                    */
          silk_NLSF_CB_struct    *psNLSF_CB;                        /* Pointer to NLSF codebook                                         */
    int                     input_quality_bands_Q15[ 4 ];
    int                     input_tilt_Q15;
    int                     SNR_dB_Q7;                         /* Quality setting                                                  */

    int1                    VAD_flags[ 3 ];
    int1                    LBRR_flag;
    int                     LBRR_flags[ 3 ];

    SideInfoIndices              indices;
    int1                    pulses[ ( ( 5 * 4 ) * 16 ) ];

    /* Input/output buffering */
    opus_int16                   inputBuf[ ( ( 5 * 4 ) * 16 ) + 2 ];  /* Buffer containing input signal                                   */
    int                     inputBufIx;
    int                     nFramesPerPacket;
    int                     nFramesEncoded;                    /* Number of frames analyzed in current packet                      */

    int                     nChannelsAPI;
    int                     nChannelsInternal;
    int                     channelNb;

    /* Parameters For LTP scaling Control */
    int                     frames_since_onset;

    /* Specifically for entropy coding */
    int                     ec_prevSignalType;
    opus_int16                   ec_prevLagIndex;

    silk_resampler_state_struct resampler_state;

    /* DTX */
    int                     useDTX;                            /* Flag to enable DTX                                               */
    int                     inDTX;                             /* Flag to signal DTX period                                        */
    int                     noSpeechCounter;                   /* Counts concecutive nonactive frames, used by DTX                 */

    /* Inband Low Bitrate Redundancy (LBRR) data */
    int                     useInBandFEC;                      /* Saves the API setting for query                                  */
    int                     LBRR_enabled;                      /* Depends on useInBandFRC, bitrate and packet loss rate            */
    int                     LBRR_GainIncreases;                /* Gains increment for coding LBRR frames                           */
    SideInfoIndices              indices_LBRR[ 3 ];
    int1                    pulses_LBRR[ 3 ][ ( ( 5 * 4 ) * 16 ) ];
}

/* Struct for Packet Loss Concealment */
struct  silk_PLC_struct
{
    opus_int32                  pitchL_Q8;                          /* Pitch lag to use for voiced concealment                          */
    opus_int16                  LTPCoef_Q14[ 5 ];           /* LTP coeficients to use for voiced concealment                    */
    opus_int16                  prevLPC_Q12[ 16 ];
    int                    last_frame_lost;                    /* Was previous frame lost                                          */
    opus_int32                  rand_seed;                          /* Seed for unvoiced signal generation                              */
    opus_int16                  randScale_Q14;                      /* Scaling of unvoiced random signal                                */
    opus_int32                  conc_energy;
    int                    conc_energy_shift;
    opus_int16                  prevLTP_scale_Q14;
    opus_int32                  prevGain_Q16[ 2 ];
    int                    fs_kHz;
    int                    nb_subfr;
    int                    subfr_length;
}

/* Struct for CNG */
struct  silk_CNG_struct
{
    opus_int32                  CNG_exc_buf_Q14[ ( ( 5 * 4 ) * 16 ) ];
    opus_int16                  CNG_smth_NLSF_Q15[ 16 ];
    opus_int32                  CNG_synth_state[ 16 ];
    opus_int32                  CNG_smth_Gain_Q16;
    opus_int32                  rand_seed;
    int                    fs_kHz;
}

/********************************/
/* Decoder state                */
/********************************/
struct  silk_decoder_state
{
    opus_int32                  prev_gain_Q16;
    opus_int32                  exc_Q14[ ( ( 5 * 4 ) * 16 ) ];
    opus_int32                  sLPC_Q14_buf[ 16 ];
    opus_int16                  outBuf[ ( ( 5 * 4 ) * 16 ) + 2 * ( 5 * 16 ) ];  /* Buffer for output signal                     */
    int                    lagPrev;                            /* Previous Lag                                                     */
    int1                   LastGainIndex;                      /* Previous gain index                                              */
    int                    fs_kHz;                             /* Sampling frequency in kHz                                        */
    opus_int32                  fs_API_hz;                          /* API sample frequency (Hz)                                        */
    int                    nb_subfr;                           /* Number of 5 ms subframes in a frame                              */
    int                    frame_length;                       /* Frame length (samples)                                           */
    int                    subfr_length;                       /* Subframe length (samples)                                        */
    int                    ltp_mem_length;                     /* Length of LTP memory                                             */
    int                    LPC_order;                          /* LPC order                                                        */
    opus_int16                  prevNLSF_Q15[ 16 ];      /* Used to interpolate LSFs                                         */
    int                    first_frame_after_reset;            /* Flag for deactivating NLSF interpolation                         */
          byte            *pitch_lag_low_bits_iCDF;           /* Pointer to iCDF table for low bits of pitch lag index            */
          byte            *pitch_contour_iCDF;                /* Pointer to iCDF table for pitch contour index                    */

    /* For buffering payload in case of more frames per packet */
    int                    nFramesDecoded;
    int                    nFramesPerPacket;

    /* Specifically for entropy coding */
    int                    ec_prevSignalType;
    opus_int16                  ec_prevLagIndex;

    int                    VAD_flags[ 3 ];
    int                    LBRR_flag;
    int                    LBRR_flags[ 3 ];

    silk_resampler_state_struct resampler_state;

          silk_NLSF_CB_struct   *psNLSF_CB;                         /* Pointer to NLSF codebook                                         */

    /* Quantization indices */
    SideInfoIndices             indices;

    /* CNG state */
    silk_CNG_struct             sCNG;

    /* Stuff used for PLC */
    int                    lossCnt;
    int                    prevSignalType;

    silk_PLC_struct sPLC;

}

/************************/
/* Decoder control      */
/************************/
struct  silk_decoder_control
{
    /* Prediction and coding parameters */
    int                    pitchL[ 4 ];
    opus_int32                  Gains_Q16[ 4 ];
    /* Holds interpolated and final coefficients, 4-byte aligned */
     opus_int16 PredCoef_Q12[ 2 ][ 16 ];
    opus_int16                  LTPCoef_Q14[ 5 * 4 ];
    int                    LTP_scale_Q14;
}

#end unsafe
