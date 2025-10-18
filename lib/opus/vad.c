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

use opus_types, structs, os_support;
use sigproc_fix, ana_filt_bank_1, lin2log, inlines, sigm_q15;

/**********************************/
/* Initialization of the Silk VAD */
/**********************************/
public
int silk_VAD_Init(                                         /* O    Return value, 0 if success                  */
    silk_VAD_state              *psSilk_VAD                     /* I/O  Pointer to Silk VAD state                   */
)
{
    int b, ret = 0;

    /* reset state memory */
    memset((char*)(psSilk_VAD), (0), (((int)(silk_VAD_state ' size  ))));

    /* init noise levels */
    /* Initialize array with approx pink noise levels (psd proportional to inverse of frequency) */
    for( b = 0; b < 4; b++ ) {
        psSilk_VAD->NoiseLevelBias[ b ] = silk_max_32( ((opus_int32)((50) / (b + 1))), 1 );
    }

    /* Initialize state */
    for( b = 0; b < 4; b++ ) {
        psSilk_VAD->NL[ b ]     = ((100) * (psSilk_VAD->NoiseLevelBias[ b ]));
        psSilk_VAD->inv_NL[ b ] = ((opus_int32)((0x7FFFFFFF) / (psSilk_VAD->NL[ b ])));
    }
    psSilk_VAD->counter = 15;

    /* init smoothed energy-to-noise ratio*/
    for( b = 0; b < 4; b++ ) {
        psSilk_VAD->NrgRatioSmth_Q8[ b ] = 100 * 256;       /* 100 * 256 --> 20 dB SNR */
    }

    return( ret );
}


/***************************************/
/* Get the speech activity level in Q8 */
/***************************************/
public
int silk_VAD_GetSA_Q8(                                     /* O    Return value, 0 if success                  */
    silk_encoder_state          *psEncC,                        /* I/O  Encoder state                               */
          opus_int16            pIn*   // []                           /* I    PCM input                                   */
)
{
    int   SA_Q15, pSNR_dB_Q7, input_tilt;
    int   decimated_framelength, dec_subframe_length, dec_subframe_offset, SNR_Q7, i, b, s;
    opus_int32 sumSquared, smooth_coef_Q16;
    opus_int16 HPstateTmp;
    opus_int16 X[ 4 ][ ( ( 5 * 4 ) * 16 ) / 2 ];
    opus_int32 Xnrg[ 4 ];
    opus_int32 NrgToNoiseRatio_Q8[ 4 ];
    opus_int32 speech_nrg, x_tmp;
    int   ret = 0;
    silk_VAD_state *psSilk_VAD = &psEncC->sVAD;

	clear X, Xnrg, sumSquared, NrgToNoiseRatio_Q8;
	
    /* Safety checks */
    ;
    ;
    ;
    ;

    /***********************/
    /* Filter and Decimate */
    /***********************/
    /* 0-8 kHz to 0-4 kHz and 4-8 kHz */
    silk_ana_filt_bank_1( pIn,          &psSilk_VAD->AnaState[  0 ], &X[ 0 ][ 0 ], &X[ 3 ][ 0 ], psEncC->frame_length );

    /* 0-4 kHz to 0-2 kHz and 2-4 kHz */
    silk_ana_filt_bank_1( &X[ 0 ][ 0 ], &psSilk_VAD->AnaState1[ 0 ], &X[ 0 ][ 0 ], &X[ 2 ][ 0 ], ((psEncC->frame_length)>>(1)) );

    /* 0-2 kHz to 0-1 kHz and 1-2 kHz */
    silk_ana_filt_bank_1( &X[ 0 ][ 0 ], &psSilk_VAD->AnaState2[ 0 ], &X[ 0 ][ 0 ], &X[ 1 ][ 0 ], ((psEncC->frame_length)>>(2)) );

    /*********************************************/
    /* HP filter on lowest band (differentiator) */
    /*********************************************/
    decimated_framelength = ((psEncC->frame_length)>>(3));
    X[ 0 ][ decimated_framelength - 1 ] = (opus_int16)(((X[ 0 ][ decimated_framelength - 1 ])>>(1)));
    HPstateTmp = X[ 0 ][ decimated_framelength - 1 ];
    for( i = decimated_framelength - 1; i > 0; i-- ) {
        X[ 0 ][ i - 1 ]  = (opus_int16)(((X[ 0 ][ i - 1 ])>>(1)));
        X[ 0 ][ i ]     -= X[ 0 ][ i - 1 ];
    }
    X[ 0 ][ 0 ] -= psSilk_VAD->HPstate;
    psSilk_VAD->HPstate = HPstateTmp;

    /*************************************/
    /* Calculate the energy in each band */
    /*************************************/
    for( b = 0; b < 4; b++ ) {
        /* Find the decimated framelength in the non-uniformly divided bands */
        decimated_framelength = ((psEncC->frame_length)>>(silk_min_int( 4 - b, 4 - 1 )));

        /* Split length into subframe lengths */
        dec_subframe_length = ((decimated_framelength)>>(2));
        dec_subframe_offset = 0;

        /* Compute energy per sub-frame */
        /* initialize with summed energy of last subframe */
        Xnrg[ b ] = psSilk_VAD->XnrgSubfr[ b ];
        for( s = 0; s < ( 1 << 2 ); s++ ) {
            sumSquared = 0;
            for( i = 0; i < dec_subframe_length; i++ ) {
                /* The energy will be less than dec_subframe_length * ( silk_int16_MIN / 8 ) ^ 2.            */
                /* Therefore we can accumulate with no risk of overflow (unless dec_subframe_length > 128)  */
                x_tmp = ((X[ b ][ i + dec_subframe_offset ])>>(3));
                sumSquared = ((sumSquared) + ((opus_int32)((opus_int16)(x_tmp))) * (opus_int32)((opus_int16)(x_tmp)));

                /* Safety check */
                ;
            }

            /* Add/saturate summed energy of current subframe */
            if( s < ( 1 << 2 ) - 1 ) {
                Xnrg[ b ] = ((( (uint) ((Xnrg[ b ])+(sumSquared)) & 0x80000000)) != 0 ? 0x7FFFFFFF : ((Xnrg[ b ])+(sumSquared)));
            } else {
                /* Look-ahead subframe */
                Xnrg[ b ] = ((( (uint) ((Xnrg[ b ])+(((sumSquared)>>(1)))) & 0x80000000)) != 0 ? 0x7FFFFFFF : ((Xnrg[ b ])+(((sumSquared)>>(1)))));
            }

            dec_subframe_offset += dec_subframe_length;
        }
        psSilk_VAD->XnrgSubfr[ b ] = sumSquared;
    }

    /********************/
    /* Noise estimation */
    /********************/
    silk_VAD_GetNoiseLevels( &Xnrg[ 0 ], psSilk_VAD );

    /***********************************************/
    /* Signal-plus-noise to noise ratio estimation */
    /***********************************************/
    sumSquared = 0;
    input_tilt = 0;
    for( b = 0; b < 4; b++ ) {
        speech_nrg = Xnrg[ b ] - psSilk_VAD->NL[ b ];
        if( speech_nrg > 0 ) {
            /* Divide, with sufficient resolution */
            if( ( (uint)Xnrg[ b ] & 0xFF800000 ) == 0 ) {
                NrgToNoiseRatio_Q8[ b ] = ((opus_int32)((((opus_int32)((opus_uint32)(Xnrg[ b ])<<(8)))) / (psSilk_VAD->NL[ b ] + 1)));
            } else {
                NrgToNoiseRatio_Q8[ b ] = ((opus_int32)((Xnrg[ b ]) / (((psSilk_VAD->NL[ b ])>>(8)) + 1)));
            }

            /* Convert to log domain */
            SNR_Q7 = silk_lin2log( NrgToNoiseRatio_Q8[ b ] ) - 8 * 128;

            /* Sum-of-squares */
            sumSquared = ((sumSquared) + ((opus_int32)((opus_int16)(SNR_Q7))) * (opus_int32)((opus_int16)(SNR_Q7)));          /* Q14 */

            /* Tilt measure */
            if( speech_nrg < ( (opus_int32)1 << 20 ) ) {
                /* Scale down SNR value for small subband speech energies */
                SNR_Q7 = ((((((opus_int32)((opus_uint32)(silk_SQRT_APPROX( speech_nrg ))<<(6)))) >> 16) * (opus_int32)((opus_int16)(SNR_Q7))) + ((((((opus_int32)((opus_uint32)(silk_SQRT_APPROX( speech_nrg ))<<(6)))) & 0x0000FFFF) * (opus_int32)((opus_int16)(SNR_Q7))) >> 16));
            }
            input_tilt = ((input_tilt) + ((((tiltWeights[ b ]) >> 16) * (opus_int32)((opus_int16)(SNR_Q7))) + ((((tiltWeights[ b ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(SNR_Q7))) >> 16)));
        } else {
            NrgToNoiseRatio_Q8[ b ] = 256;
        }
    }

    /* Mean-of-squares */
    sumSquared = ((opus_int32)((sumSquared) / (4))); /* Q14 */

    /* Root-mean-square approximation, scale to dBs, and write to output pointer */
    pSNR_dB_Q7 = (opus_int16)( 3 * silk_SQRT_APPROX( sumSquared ) ); /* Q7 */

    /*********************************/
    /* Speech Probability Estimation */
    /*********************************/
    SA_Q15 = silk_sigm_Q15( ((((45000) >> 16) * (opus_int32)((opus_int16)(pSNR_dB_Q7))) + ((((45000) & 0x0000FFFF) * (opus_int32)((opus_int16)(pSNR_dB_Q7))) >> 16)) - 128 );

    /**************************/
    /* Frequency Tilt Measure */
    /**************************/
    psEncC->input_tilt_Q15 = ((opus_int32)((opus_uint32)(silk_sigm_Q15( input_tilt ) - 16384)<<(1)));

    /**************************************************/
    /* Scale the sigmoid output based on power levels */
    /**************************************************/
    speech_nrg = 0;
    for( b = 0; b < 4; b++ ) {
        /* Accumulate signal-without-noise energies, higher frequency bands have more weight */
        speech_nrg += ( b + 1 ) * ((Xnrg[ b ] - psSilk_VAD->NL[ b ])>>(4));
    }

    /* Power scaling */
    if( speech_nrg <= 0 ) {
        SA_Q15 = ((SA_Q15)>>(1));
    } else if( speech_nrg < 32768 ) {
        if( psEncC->frame_length == 10 * psEncC->fs_kHz ) {
            speech_nrg = (((opus_int32)((opus_uint32)(((((((opus_int32)-0x80000000))>>((16)))) > (((0x7FFFFFFF)>>((16)))) ? (((speech_nrg)) > (((((opus_int32)-0x80000000))>>((16)))) ? (((((opus_int32)-0x80000000))>>((16)))) : (((speech_nrg)) < (((0x7FFFFFFF)>>((16)))) ? (((0x7FFFFFFF)>>((16)))) : ((speech_nrg)))) : (((speech_nrg)) > (((0x7FFFFFFF)>>((16)))) ? (((0x7FFFFFFF)>>((16)))) : (((speech_nrg)) < (((((opus_int32)-0x80000000))>>((16)))) ? (((((opus_int32)-0x80000000))>>((16)))) : ((speech_nrg))))))<<((16)))));
        } else {
            speech_nrg = (((opus_int32)((opus_uint32)(((((((opus_int32)-0x80000000))>>((15)))) > (((0x7FFFFFFF)>>((15)))) ? (((speech_nrg)) > (((((opus_int32)-0x80000000))>>((15)))) ? (((((opus_int32)-0x80000000))>>((15)))) : (((speech_nrg)) < (((0x7FFFFFFF)>>((15)))) ? (((0x7FFFFFFF)>>((15)))) : ((speech_nrg)))) : (((speech_nrg)) > (((0x7FFFFFFF)>>((15)))) ? (((0x7FFFFFFF)>>((15)))) : (((speech_nrg)) < (((((opus_int32)-0x80000000))>>((15)))) ? (((((opus_int32)-0x80000000))>>((15)))) : ((speech_nrg))))))<<((15)))));
        }

        /* square-root */
        speech_nrg = silk_SQRT_APPROX( speech_nrg );
        SA_Q15 = ((((32768 + speech_nrg) >> 16) * (opus_int32)((opus_int16)(SA_Q15))) + ((((32768 + speech_nrg) & 0x0000FFFF) * (opus_int32)((opus_int16)(SA_Q15))) >> 16));
    }

    /* Copy the resulting speech activity in Q8 */
    psEncC->speech_activity_Q8 = silk_min_int( ((SA_Q15)>>(7)), 0xFF );

    /***********************************/
    /* Energy Level and SNR estimation */
    /***********************************/
    /* Smoothing coefficient */
    smooth_coef_Q16 = ((((4096) >> 16) * (opus_int32)((opus_int16)((((((opus_int32)SA_Q15) >> 16) * (opus_int32)((opus_int16)(SA_Q15))) + (((((opus_int32)SA_Q15) & 0x0000FFFF) * (opus_int32)((opus_int16)(SA_Q15))) >> 16))))) + ((((4096) & 0x0000FFFF) * (opus_int32)((opus_int16)((((((opus_int32)SA_Q15) >> 16) * (opus_int32)((opus_int16)(SA_Q15))) + (((((opus_int32)SA_Q15) & 0x0000FFFF) * (opus_int32)((opus_int16)(SA_Q15))) >> 16))))) >> 16));

    if( psEncC->frame_length == 10 * psEncC->fs_kHz ) {
        smooth_coef_Q16 >>= 1;
    }

    for( b = 0; b < 4; b++ ) {
        /* compute smoothed energy-to-noise ratio per band */
        psSilk_VAD->NrgRatioSmth_Q8[ b ] = 
((psSilk_VAD->NrgRatioSmth_Q8[ b ]) + ((((NrgToNoiseRatio_Q8[ b ] - psSilk_VAD->NrgRatioSmth_Q8[ b ]) >> 16) * (opus_int32)((opus_int16)(smooth_coef_Q16))) + ((((NrgToNoiseRatio_Q8[ b ] - psSilk_VAD->NrgRatioSmth_Q8[ b ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(smooth_coef_Q16))) >> 16)));

        /* signal to noise ratio in dB per band */
        SNR_Q7 = 3 * ( silk_lin2log( psSilk_VAD->NrgRatioSmth_Q8[b] ) - 8 * 128 );
        /* quality = sigmoid( 0.25 * ( SNR_dB - 16 ) ); */
        psEncC->input_quality_bands_Q15[ b ] = silk_sigm_Q15( ((SNR_Q7 - 16 * 128)>>(4)) );
    }

    return( ret );
}

/**************************/
/* Noise level estimation */
/**************************/
public
void silk_VAD_GetNoiseLevels(
          opus_int32            pX*,   // [ 4 ],  /* I    subband energies                            */
    silk_VAD_state              *psSilk_VAD         /* I/O  Pointer to Silk VAD state                   */
)
{
    int   k;
    opus_int32 nl, nrg, inv_nrg;
    int   coef, min_coef;

    /* Initially faster smoothing */
    if( psSilk_VAD->counter < 1000 ) { /* 1000 = 20 sec */
        min_coef = ((opus_int32)((0x7FFF) / (((psSilk_VAD->counter)>>(4)) + 1)));
    } else {
        min_coef = 0;
    }

    for( k = 0; k < 4; k++ ) {
        /* Get old noise level estimate for current band */
        nl = psSilk_VAD->NL[ k ];
        ;

        /* Add bias */
        nrg = ((( (uint)((pX[ k ])+(psSilk_VAD->NoiseLevelBias[ k ])) & 0x80000000)) != 0 ? 0x7FFFFFFF : ((pX[ k ])+(psSilk_VAD->NoiseLevelBias[ k ])));
        ;

        /* Invert energies */
        inv_nrg = ((opus_int32)((0x7FFFFFFF) / (nrg)));
        ;

        /* Less update when subband energy is high */
        if( nrg > ((opus_int32)((opus_uint32)(nl)<<(3))) ) {
            coef = 1024 >> 3;
        } else if( nrg < nl ) {
            coef = 1024;
        } else {
            coef = ((((((((((((inv_nrg)) >> 16) * (opus_int32)((opus_int16)((nl)))) + (((((inv_nrg)) & 0x0000FFFF) * (opus_int32)((opus_int16)((nl)))) >> 16)))) + ((((inv_nrg)) * (((16) == 1 ? (((nl)) >> 1) + (((nl)) & 1) : ((((nl)) >> ((16) - 1)) + 1) >> 1)))))) >> 16) * (opus_int32)((opus_int16)(1024 << 1))) + ((((((((((((inv_nrg)) >> 16) * (opus_int32)((opus_int16)((nl)))) + (((((inv_nrg)) & 0x0000FFFF) * (opus_int32)((opus_int16)((nl)))) >> 16)))) + ((((inv_nrg)) * (((16) == 1 ? (((nl)) >> 1) + (((nl)) & 1) : ((((nl)) >> ((16) - 1)) + 1) >> 1)))))) & 0x0000FFFF) * (opus_int32)((opus_int16)(1024 << 1))) >> 16));
        }

        /* Initially faster smoothing */
        coef = silk_max_int( coef, min_coef );

        /* Smooth inverse energies */
        psSilk_VAD->inv_NL[ k ] = ((psSilk_VAD->inv_NL[ k ]) + ((((inv_nrg - psSilk_VAD->inv_NL[ k ]) >> 16) * (opus_int32)((opus_int16)(coef))) + ((((inv_nrg - psSilk_VAD->inv_NL[ k ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(coef))) >> 16)));
        ;

        /* Compute noise level by inverting again */
        nl = ((opus_int32)((0x7FFFFFFF) / (psSilk_VAD->inv_NL[ k ])));
        ;

        /* Limit noise levels (guarantee 7 bits of head room) */
        nl = (((nl) < (0x00FFFFFF)) ? (nl) : (0x00FFFFFF));

        /* Store as part of state */
        psSilk_VAD->NL[ k ] = nl;
    }

    /* Increment frame counter */
    psSilk_VAD->counter++;
}
#end unsafe
