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

use opus_types, resampler_structs, resampler_rom;
use resampler_private_up2_hq, resampler_private_iir_fir;
use resampler_private_down_fir;
use os_support;

/*
 * Matrix of resampling methods used:
 *                                 Fs_out (kHz)
 *                        8      12     16     24     48
 *
 *               8        C      UF     U      UF     UF
 *              12        AF     C      UF     U      UF
 * Fs_in (kHz)  16        D      AF     C      UF     UF
 *              24        AF     D      AF     C      U
 *              48        AF     AF     AF     D      C
 *
 * C   -> Copy (no resampling)
 * D   -> Allpass-based 2x downsampling
 * U   -> Allpass-based 2x upsampling
 * UF  -> Allpass-based 2x upsampling followed by FIR interpolation
 * AF  -> AR2 filter followed by FIR interpolation
 */



/* Simple way to make [8000, 12000, 16000, 24000, 48000] to [0, 1, 2, 3, 4] */

/* Initialize/reset the resampler state for a given pair of input/output sampling rates */
public
int silk_resampler_init(
    silk_resampler_state_struct *S,                 /* I/O  Resampler state                                             */
    opus_int32                  Fs_Hz_in,           /* I    Input sampling rate (Hz)                                    */
    opus_int32                  Fs_Hz_out,          /* I    Output sampling rate (Hz)                                   */
    int                    forEnc              /* I    If 1: encoder; if 0: decoder                                */
)
{
    int up2x;

    /* Clear state */
    memset((char*)(S), (0), (((int)(silk_resampler_state_struct ' size  ))));

    /* Input checking */
    if( forEnc!=0 ) {
        if( ( Fs_Hz_in  != 8000 && Fs_Hz_in  != 12000 && Fs_Hz_in  != 16000 && Fs_Hz_in  != 24000 && Fs_Hz_in  != 48000 ) ||
            ( Fs_Hz_out != 8000 && Fs_Hz_out != 12000 && Fs_Hz_out != 16000 ) ) {
            ;
            return -1;
        }
        S->inputDelay = delay_matrix_enc[ ( ( ( ((Fs_Hz_in)>>12) - (int)((Fs_Hz_in)>16000) ) >> (int)((Fs_Hz_in)>24000) ) - 1 ) ][ ( ( ( ((Fs_Hz_out)>>12) - (int)((Fs_Hz_out)>16000) ) >> (int)((Fs_Hz_out)>24000) ) - 1 ) ];
    } else {
        if( ( Fs_Hz_in  != 8000 && Fs_Hz_in  != 12000 && Fs_Hz_in  != 16000 ) ||
            ( Fs_Hz_out != 8000 && Fs_Hz_out != 12000 && Fs_Hz_out != 16000 && Fs_Hz_out != 24000 && Fs_Hz_out != 48000 ) ) {
            ;
            return -1;
        }
        S->inputDelay = delay_matrix_dec[ ( ( ( ((Fs_Hz_in)>>12) - (int)((Fs_Hz_in)>16000) ) >> (int)((Fs_Hz_in)>24000) ) - 1 ) ][ ( ( ( ((Fs_Hz_out)>>12) - (int)((Fs_Hz_out)>16000) ) >> (int)((Fs_Hz_out)>24000) ) - 1 ) ];
    }

    S->Fs_in_kHz  = ((opus_int32)((Fs_Hz_in) / (1000)));
    S->Fs_out_kHz = ((opus_int32)((Fs_Hz_out) / (1000)));

    /* Number of samples processed per batch */
    S->batchSize = S->Fs_in_kHz * 10;

    /* Find resampler with the right sampling ratio */
    up2x = 0;
    if( Fs_Hz_out > Fs_Hz_in ) {
        /* Upsample */
        if( Fs_Hz_out == ((Fs_Hz_in) * (2)) ) {                            /* Fs_out : Fs_in = 2 : 1 */
            /* Special case: directly use 2x upsampler */
            S->resampler_function = (1);
        } else {
            /* Default resampler */
            S->resampler_function = (2);
            up2x = 1;
        }
    } else if ( Fs_Hz_out < Fs_Hz_in ) {
        /* Downsample */
         S->resampler_function = (3);
        if( ((Fs_Hz_out) * (4)) == ((Fs_Hz_in) * (3)) ) {             /* Fs_out : Fs_in = 3 : 4 */
            S->FIR_Fracs = 3;
            S->FIR_Order = 18;
            S->Coefs = &silk_Resampler_3_4_COEFS;
        } else if( ((Fs_Hz_out) * (3)) == ((Fs_Hz_in) * (2)) ) {      /* Fs_out : Fs_in = 2 : 3 */
            S->FIR_Fracs = 2;
            S->FIR_Order = 18;
            S->Coefs = &silk_Resampler_2_3_COEFS;
        } else if( ((Fs_Hz_out) * (2)) == Fs_Hz_in ) {                     /* Fs_out : Fs_in = 1 : 2 */
            S->FIR_Fracs = 1;
            S->FIR_Order = 24;
            S->Coefs = &silk_Resampler_1_2_COEFS;
        } else if( ((Fs_Hz_out) * (3)) == Fs_Hz_in ) {                     /* Fs_out : Fs_in = 1 : 3 */
            S->FIR_Fracs = 1;
            S->FIR_Order = 36;
            S->Coefs = &silk_Resampler_1_3_COEFS;
        } else if( ((Fs_Hz_out) * (4)) == Fs_Hz_in ) {                     /* Fs_out : Fs_in = 1 : 4 */
            S->FIR_Fracs = 1;
            S->FIR_Order = 36;
            S->Coefs = &silk_Resampler_1_4_COEFS;
        } else if( ((Fs_Hz_out) * (6)) == Fs_Hz_in ) {                     /* Fs_out : Fs_in = 1 : 6 */
            S->FIR_Fracs = 1;
            S->FIR_Order = 36;
            S->Coefs = &silk_Resampler_1_6_COEFS;
        } else {
            /* None available */
            ;
            return -1;
        }
    } else {
        /* Input and output sampling rates are equal: copy */
        S->resampler_function = (0);
    }

    /* Ratio of input/output samples */
    S->invRatio_Q16 = ((opus_int32)((opus_uint32)(((opus_int32)((((opus_int32)((opus_uint32)(Fs_Hz_in)<<(uint)(14 + up2x)))) / (Fs_Hz_out))))<<(2)));
    /* Make sure the ratio is rounded up */
    while( ((((((((S->invRatio_Q16)) >> 16) * (opus_int32)((opus_int16)((Fs_Hz_out)))) + (((((S->invRatio_Q16)) & 0x0000FFFF) * (opus_int32)((opus_int16)((Fs_Hz_out)))) >> 16)))) + ((((S->invRatio_Q16)) * (((16) == 1 ? (((Fs_Hz_out)) >> 1) + (((Fs_Hz_out)) & 1) : ((((Fs_Hz_out)) >> ((16) - 1)) + 1) >> 1))))) < ((opus_int32)((opus_uint32)(Fs_Hz_in)<<(uint)(up2x))) ) {
        S->invRatio_Q16++;
    }

    return 0;
}

/* Resampler: convert from one sampling rate to another */
/* Input and output sampling rate are at most 48000 Hz  */
public int silk_resampler(
    silk_resampler_state_struct *S,                 /* I/O  Resampler state                                             */
    opus_int16                  _out*,              /* O    Output signal                                               */
          opus_int16            in*,               /* I    Input signal                                                */
    opus_int32                  inLen               /* I    Number of input samples                                     */
)
{
    int nSamples;

    /* Need at least 1 ms of input data */
    ;
    /* Delay can't exceed the 1 ms of buffering */
    ;

    nSamples = S->Fs_in_kHz - S->inputDelay;

    /* Copy to delay buffer */
    memcpy((byte*)(&S->delayBuf[ S->inputDelay ]), (byte*)(in), (nSamples * ((int)(opus_int16 ' size  ))));

    switch( S->resampler_function ) {
        case (1):
            silk_resampler_private_up2_HQ_wrapper((byte*)S, _out, &S->delayBuf, S->Fs_in_kHz );
            silk_resampler_private_up2_HQ_wrapper((byte*)S, &_out[ S->Fs_out_kHz ], &in[ nSamples ], inLen - S->Fs_in_kHz );
            break;
        case (2):
            silk_resampler_private_IIR_FIR((byte*) S, _out, &S->delayBuf, S->Fs_in_kHz );
            silk_resampler_private_IIR_FIR((byte*) S, &_out[ S->Fs_out_kHz ], &in[ nSamples ], inLen - S->Fs_in_kHz );
            break;
        case (3):
            silk_resampler_private_down_FIR((byte*) S, _out, &S->delayBuf, S->Fs_in_kHz );
            silk_resampler_private_down_FIR((byte*) S, &_out[ S->Fs_out_kHz ], &in[ nSamples ], inLen - S->Fs_in_kHz );
            break;
        default:
            memcpy((byte*)(_out), (byte*)&(S->delayBuf), (S->Fs_in_kHz * ((int)(opus_int16 ' size  ))));
            memcpy((byte*)(&_out[ S->Fs_out_kHz ]), (byte*)(&in[ nSamples ]), (( inLen - S->Fs_in_kHz ) * ((int)(opus_int16 ' size  ))));
			break;
    }

    /* Copy to delay buffer */
    memcpy((byte*)&(S->delayBuf), (byte*)(&in[ inLen - S->inputDelay ]), (S->inputDelay * ((int)(opus_int16 ' size  ))));

    return 0;
}
#end unsafe
