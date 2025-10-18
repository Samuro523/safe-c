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
use sigproc_flp;
use sigproc_fix;
use pitch_est_defines;
use os_support, resampler_down2, resampler_down2_3, inner_product_flp, energy_flp;
use sort_flp;

/*****************************************************************************
* Pitch analyser function
******************************************************************************/

/************************************************************/
/* CORE PITCH ANALYSIS FUNCTION                             */
/************************************************************/
public
int silk_pitch_analysis_core_FLP(      /* O    Voicing estimate: 0 voiced, 1 unvoiced                      */
          float    *frame,             /* I    Signal of length PE_FRAME_LENGTH_MS*Fs_kHz                  */
    int            *pitch_out,         /* O    Pitch lag values [nb_subfr]                                 */
    opus_int16          *lagIndex,          /* O    Lag Index                                                   */
    int1           *contourIndex,      /* O    Pitch contour Index                                         */
    float          *LTPCorr,           /* I/O  Normalized correlation; input: value from previous frame    */
    int            prevLag,            /* I    Last lag of previous frame; set to zero is unvoiced         */
          float    search_thres1,      /* I    First stage threshold for lag candidates 0 - 1              */
          float    search_thres2,      /* I    Final threshold for lag candidates 0 - 1                    */
          int      Fs_kHz,             /* I    sample frequency (kHz)                                      */
          int      complexity,         /* I    Complexity setting, 0-2, where 2 is highest                 */
          int      nb_subfr            /* I    Number of 5 ms subframes                                    */
)
{
    int   i, k, d, j;
    float frame_8kHz[  ( ( 4 * 5 ) + 4 * 5 ) * 8 ];
    float frame_4kHz[  ( ( 4 * 5 ) + 4 * 5 ) * 4 ];
    opus_int16 frame_8_FIX[ ( ( 4 * 5 ) + 4 * 5 ) * 8 ];
    opus_int16 frame_4_FIX[ ( ( 4 * 5 ) + 4 * 5 ) * 4 ];
    opus_int32 filt_state[ 6 ];
    float threshold, contour_bias;
    float C[ 4][ (( 18 * 16 ) >> 1) + 5 ];
    float CC[ 11 ];
    float* target_ptr, basis_ptr;
    double cross_corr, normalizer, energy, energy_tmp;
    int   d_srch[ 24 ];
    opus_int16 d_comp[ (( 18 * 16 ) >> 1) + 5 ];
    int   length_d_srch, length_d_comp;
    float Cmax, CCmax, CCmax_b, CCmax_new_b, CCmax_new;
    int   CBimax, CBimax_new, lag, start_lag, end_lag, lag_new;
    int   cbk_size;
    float lag_log2, prevLag_log2, delta_lag_log2_sqr;
    float energies_st3[ 4 ][ 34 ][ 5 ];
    float cross_corr_st3[ 4 ][ 34 ][ 5 ];
    int   lag_counter;
    int   frame_length, frame_length_8kHz, frame_length_4kHz;
    int   sf_length, sf_length_8kHz, sf_length_4kHz;
    int   min_lag, min_lag_8kHz, min_lag_4kHz;
    int   max_lag, max_lag_8kHz, max_lag_4kHz;
    int   nb_cbk_search;
          int1 *Lag_CB_ptr;

    clear d_comp, frame_8kHz, CC;
    /* Check for valid sampling frequency */
    ;

    /* Check for valid complexity setting */
    ;
    ;

    ;
    ;

    /* Set up frame lengths max / min lag for the sampling frequency */
    frame_length      = ( ( 4 * 5 ) + nb_subfr * 5 ) * Fs_kHz;
    frame_length_4kHz = ( ( 4 * 5 ) + nb_subfr * 5 ) * 4;
    frame_length_8kHz = ( ( 4 * 5 ) + nb_subfr * 5 ) * 8;
    sf_length         = 5 * Fs_kHz;
    sf_length_4kHz    = 5 * 4;
    sf_length_8kHz    = 5 * 8;
    min_lag           = 2 * Fs_kHz;
    min_lag_4kHz      = 2 * 4;
    min_lag_8kHz      = 2 * 8;
    max_lag           = 18 * Fs_kHz - 1;
    max_lag_4kHz      = 18 * 4;
    max_lag_8kHz      = 18 * 8 - 1;

    memset((char*)&(C), (0), (((int)(float ' size  )) * nb_subfr * ((( 18 * 16 ) >> 1) + 5)));

    /* Resample from input sampled at Fs_kHz to 8 kHz */
    if( Fs_kHz == 16 ) {
        /* Resample to 16 -> 8 khz */
        opus_int16 frame_16_FIX[ 16 * ( ( 4 * 5 ) + 4 * 5 ) ];
        silk_float2short_array( &frame_16_FIX, frame, frame_length );
        memset((char*)&(filt_state), (0), (2 * ((int)(opus_int32 ' size  ))));
        silk_resampler_down2( &filt_state, &frame_8_FIX, &frame_16_FIX, frame_length );
        silk_short2float_array( &frame_8kHz, &frame_8_FIX, frame_length_8kHz );
    } else if( Fs_kHz == 12 ) {
        /* Resample to 12 -> 8 khz */
        opus_int16 frame_12_FIX[ 12 * ( ( 4 * 5 ) + 4 * 5 ) ];
        silk_float2short_array( &frame_12_FIX, frame, frame_length );
        memset((char*)&(filt_state), (0), (6 * ((int)(opus_int32 ' size  ))));
        silk_resampler_down2_3( &filt_state, &frame_8_FIX, &frame_12_FIX, frame_length );
        silk_short2float_array( &frame_8kHz, &frame_8_FIX, frame_length_8kHz );
    } else {
        ;
        silk_float2short_array( &frame_8_FIX, frame, frame_length_8kHz );
    }

    /* Decimate again to 4 kHz */
    memset((char*)&(filt_state), (0), (2 * ((int)(opus_int32 ' size  ))));
    silk_resampler_down2( &filt_state, &frame_4_FIX, &frame_8_FIX, frame_length_8kHz );
    silk_short2float_array( &frame_4kHz, &frame_4_FIX, frame_length_4kHz );

    /* Low-pass filter */
    for( i = frame_length_4kHz - 1; i > 0; i-- ) {
        frame_4kHz[ i ] += frame_4kHz[ i - 1 ];
    }

    /******************************************************************************
    * FIRST STAGE, operating in 4 khz
    ******************************************************************************/
    target_ptr = &frame_4kHz[ ((opus_int32)((opus_uint32)(sf_length_4kHz)<<(2))) ];
    for( k = 0; k < nb_subfr >> 1; k++ ) {
        /* Check that we are within range of the array */
        ;
        ;

        basis_ptr = target_ptr - min_lag_4kHz;

        /* Check that we are within range of the array */
        ;
        ;

        /* Calculate first vector products before loop */
        cross_corr = silk_inner_product_FLP( target_ptr, basis_ptr, sf_length_8kHz );
        normalizer = silk_energy_FLP( basis_ptr, sf_length_8kHz ) + (float)sf_length_8kHz * 4000.0f;

        C[ 0 ][ min_lag_4kHz ] += (float)(cross_corr / sqrt(normalizer));

        /* From now on normalizer is computed recursively */
        for(d = min_lag_4kHz + 1; d <= max_lag_4kHz; d++) {
            basis_ptr--;

            /* Check that we are within range of the array */
            ;
            ;

            cross_corr = silk_inner_product_FLP(target_ptr, basis_ptr, sf_length_8kHz);

            /* Add contribution of new sample and remove contribution from oldest sample */
            normalizer +=
                basis_ptr[ 0 ] * (double)basis_ptr[ 0 ] -
                basis_ptr[ sf_length_8kHz ] * (double)basis_ptr[ sf_length_8kHz ];
            C[ 0 ][ d ] += (float)(cross_corr / sqrt( normalizer ));
        }
        /* Update target pointer */
        target_ptr += sf_length_8kHz;
    }

    /* Apply short-lag bias */
    for( i = max_lag_4kHz; i >= min_lag_4kHz; i-- ) {
        C[ 0 ][ i ] -= C[ 0 ][ i ] * (float)i / 4096.0f;
    }

    /* Sort */
    length_d_srch = 4 + 2 * complexity;
    ;
    silk_insertion_sort_decreasing_FLP( &C[ 0 ][ min_lag_4kHz ], &d_srch, max_lag_4kHz - min_lag_4kHz + 1, length_d_srch );

    /* Escape if correlation is very low already here */
    Cmax = C[ 0 ][ min_lag_4kHz ];
    target_ptr = &frame_4kHz[ ((opus_int32)((opus_int16)(sf_length_4kHz)) * (opus_int32)((opus_int16)(nb_subfr))) ];
    energy = 1000.0f;
    for( i = 0; i < ((opus_int32)((opus_uint32)(sf_length_4kHz)<<(2))); i++ ) {
        energy += target_ptr[i] * (double)target_ptr[i];
    }
    threshold = Cmax * Cmax;
    if( energy / 16.0f > threshold ) {
        memset((char*)(pitch_out), (0), (nb_subfr * ((int)(int ' size  ))));
        *LTPCorr      = 0.0f;
        *lagIndex     = 0;
        *contourIndex = 0;
        return 1;
    }

    threshold = search_thres1 * Cmax;
    for( i = 0; i < length_d_srch; i++ ) {
        /* Convert to 8 kHz indices for the sorted correlation that exceeds the threshold */
        if( C[ 0 ][ min_lag_4kHz + i ] > threshold ) {
            d_srch[ i ] = ((opus_int32)((opus_uint32)(d_srch[ i ] + min_lag_4kHz)<<(1)));
        } else {
            length_d_srch = i;
            break;
        }
    }
    ;

    for( i = min_lag_8kHz - 5; i < max_lag_8kHz + 5; i++ ) {
        d_comp[ i ] = 0;
    }
    for( i = 0; i < length_d_srch; i++ ) {
        d_comp[ d_srch[ i ] ] = 1;
    }

    /* Convolution */
    for( i = max_lag_8kHz + 3; i >= min_lag_8kHz; i-- ) {
        d_comp[ i ] += (opus_int16)(d_comp[ i - 1 ] + d_comp[ i - 2 ]);
    }

    length_d_srch = 0;
    for( i = min_lag_8kHz; i < max_lag_8kHz + 1; i++ ) {
        if( d_comp[ i + 1 ] > 0 ) {
            d_srch[ length_d_srch ] = i;
            length_d_srch++;
        }
    }

    /* Convolution */
    for( i = max_lag_8kHz + 3; i >= min_lag_8kHz; i-- ) {
        d_comp[ i ] += (opus_int16)(d_comp[ i - 1 ] + d_comp[ i - 2 ] + d_comp[ i - 3 ]);
    }

    length_d_comp = 0;
    for( i = min_lag_8kHz; i < max_lag_8kHz + 4; i++ ) {
        if( d_comp[ i ] > 0 ) {
            d_comp[ length_d_comp ] = (opus_int16)( i - 2 );
            length_d_comp++;
        }
    }

    /**********************************************************************************
    ** SECOND STAGE, operating at 8 kHz, on lag sections with high correlation
    *************************************************************************************/
    /*********************************************************************************
    * Find energy of each subframe projected onto its history, for a range of delays
    *********************************************************************************/
    memset((char*)&(C), (0), (4*((( 18 * 16 ) >> 1) + 5) * ((int)(float ' size  ))));

    if( Fs_kHz == 8 ) {
        target_ptr = &frame[ ( 4 * 5 ) * 8 ];
    } else {
        target_ptr = &frame_8kHz[ ( 4 * 5 ) * 8 ];
    }
    for( k = 0; k < nb_subfr; k++ ) {
        energy_tmp = silk_energy_FLP( target_ptr, sf_length_8kHz );
        for( j = 0; j < length_d_comp; j++ ) {
            d = d_comp[ j ];
            basis_ptr = target_ptr - d;
            cross_corr = silk_inner_product_FLP( basis_ptr, target_ptr, sf_length_8kHz );
            energy     = silk_energy_FLP( basis_ptr, sf_length_8kHz );
            if( cross_corr > 0.0f ) {
                C[ k ][ d ] = (float)(cross_corr * cross_corr / (energy * energy_tmp + 1.192092896e-07f));
            } else {
                C[ k ][ d ] = 0.0f;
            }
        }
        target_ptr += sf_length_8kHz;
    }

    /* search over lag range and lags codebook */
    /* scale factor for lag codebook, as a function of center lag */

    CCmax   = 0.0f; /* This value doesn't matter */
    CCmax_b = -1000.0f;

    CBimax = 0; /* To avoid returning undefined lag values */
    lag = -1;   /* To check if lag with strong enough correlation has been found */

    if( prevLag > 0 ) {
        if( Fs_kHz == 12 ) {
            *&prevLag = ((opus_int32)((opus_uint32)(prevLag)<<(1))) / 3;
        } else if( Fs_kHz == 16 ) {
            *&prevLag = ((prevLag)>>(1));
        }
        prevLag_log2 = silk_log2((float)prevLag);
    } else {
        prevLag_log2 = 0.0;
    }

    /* Set up stage 2 codebook based on number of subframes */
    if( nb_subfr == 4 ) {
        cbk_size   = 11;
        Lag_CB_ptr = &silk_CB_lags_stage2[ 0 ][ 0 ];
        if( Fs_kHz == 8 && complexity > 0 ) {
            /* If input is 8 khz use a larger codebook here because it is last stage */
            nb_cbk_search = 11;
        } else {
            nb_cbk_search = 3;
        }
    } else {
        cbk_size       = 3;
        Lag_CB_ptr     = &silk_CB_lags_stage2_10_ms[ 0 ][ 0 ];
        nb_cbk_search  = 3;
    }

    for( k = 0; k < length_d_srch; k++ ) {
        d = d_srch[ k ];
        for( j = 0; j < nb_cbk_search; j++ ) {
            CC[j] = 0.0f;
            for( i = 0; i < nb_subfr; i++ ) {
                /* Try all codebooks */
                CC[ j ] += C[ i ][ d + *(Lag_CB_ptr + ((i)*(cbk_size)+(j)))];
            }
        }
        /* Find best codebook */
        CCmax_new  = -1000.0f;
        CBimax_new = 0;
        for( i = 0; i < nb_cbk_search; i++ ) {
            if( CC[ i ] > CCmax_new ) {
                CCmax_new = CC[ i ];
                CBimax_new = i;
            }
        }
        CCmax_new = (((CCmax_new) > (0.0f)) ? (CCmax_new) : (0.0f)); /* To avoid taking square root of negative number later */
        CCmax_new_b = CCmax_new;

        /* Bias towards shorter lags */
        lag_log2 = silk_log2((float)d);
        CCmax_new_b -= 0.2f * (float)nb_subfr * lag_log2;

        /* Bias towards previous lag */
        if( prevLag > 0 ) {
            delta_lag_log2_sqr = lag_log2 - prevLag_log2;
            delta_lag_log2_sqr *= delta_lag_log2_sqr;
            CCmax_new_b -= 0.2f * (float)nb_subfr * (*LTPCorr) * delta_lag_log2_sqr / (delta_lag_log2_sqr + 0.5f);
        }

        if( CCmax_new_b > CCmax_b                                   &&  /* Find maximum biased correlation                  */
            CCmax_new > (float)nb_subfr * search_thres2 * search_thres2    &&  /* Correlation needs to be high enough to be voiced */
            silk_CB_lags_stage2[ 0 ][ CBimax_new ] <= min_lag_8kHz      /* Lag must be in range                             */
        ) {
            CCmax_b = CCmax_new_b;
            CCmax   = CCmax_new;
            lag     = d;
            CBimax  = CBimax_new;
        }
    }

    if( lag == -1 ) {
        /* No suitable candidate found */
        memset((char*)(pitch_out), (0), (4 * ((int)(int ' size  ))));
        *LTPCorr      = 0.0f;
        *lagIndex     = 0;
        *contourIndex = 0;
        return 1;
    }

    if( Fs_kHz > 8 ) {
        /* Search in original signal */

        /* Compensate for decimation */
        ;
        if( Fs_kHz == 12 ) {
            lag = ((1) == 1 ? ((((opus_int32)((opus_int16)(lag)) * (opus_int32)((opus_int16)(3)))) >> 1) + ((((opus_int32)((opus_int16)(lag)) * (opus_int32)((opus_int16)(3)))) & 1) : (((((opus_int32)((opus_int16)(lag)) * (opus_int32)((opus_int16)(3)))) >> ((1) - 1)) + 1) >> 1);
        } else { /* Fs_kHz == 16 */
            lag = ((opus_int32)((opus_uint32)(lag)<<(1)));
        }

        lag = ((min_lag) > (max_lag) ? ((lag) > (min_lag) ? (min_lag) : ((lag) < (max_lag) ? (max_lag) : (lag))) : ((lag) > (max_lag) ? (max_lag) : ((lag) < (min_lag) ? (min_lag) : (lag))));
        start_lag = silk_max_int( lag - 2, min_lag );
        end_lag   = silk_min_int( lag + 2, max_lag );
        lag_new   = lag;                                    /* to avoid undefined lag */
        CBimax    = 0;                                      /* to avoid undefined lag */
        ;
        *LTPCorr = (float)sqrt( CCmax / (float)nb_subfr );    /* Output normalized correlation */

        CCmax = -1000.0f;

        /* Calculate the correlations and energies needed in stage 3 */
        silk_P_Ana_calc_corr_st3( &cross_corr_st3, frame, start_lag, sf_length, nb_subfr, complexity );
        silk_P_Ana_calc_energy_st3( &energies_st3, frame, start_lag, sf_length, nb_subfr, complexity );

        lag_counter = 0;
        ;
        contour_bias = 0.05f / (float)lag;

        /* Set up cbk parameters acording to complexity setting and frame length */
        if( nb_subfr == 4 ) {
            nb_cbk_search = (int)silk_nb_cbk_searchs_stage3[ complexity ];
            cbk_size      = 34;
            Lag_CB_ptr    = &silk_CB_lags_stage3[ 0 ][ 0 ];
        } else {
            nb_cbk_search = 12;
            cbk_size      = 12;
            Lag_CB_ptr    = &silk_CB_lags_stage3_10_ms[ 0 ][ 0 ];
        }

        for( d = start_lag; d <= end_lag; d++ ) {
            for( j = 0; j < nb_cbk_search; j++ ) {
                cross_corr = 0.0;
                energy = 1.192092896e-07f;
                for( k = 0; k < nb_subfr; k++ ) {
                    energy     +=   energies_st3[ k ][ j ][ lag_counter ];
                    cross_corr += cross_corr_st3[ k ][ j ][ lag_counter ];
                }
                if( cross_corr > 0.0 ) {
                    CCmax_new = (float)(cross_corr * cross_corr / energy);
                    /* Reduce depending on flatness of contour */
                    CCmax_new *= 1.0f - contour_bias * (float)j;
                } else {
                    CCmax_new = 0.0f;
                }

                if( CCmax_new > CCmax &&
                   ( d + (int)silk_CB_lags_stage3[ 0 ][ j ] ) <= max_lag
                   ) {
                    CCmax   = CCmax_new;
                    lag_new = d;
                    CBimax  = j;
                }
            }
            lag_counter++;
        }

        for( k = 0; k < nb_subfr; k++ ) {
            pitch_out[ k ] = lag_new + *(Lag_CB_ptr + ((k)*(cbk_size)+(CBimax)));
            pitch_out[ k ] = ((min_lag) > (18 * Fs_kHz) ? ((pitch_out[ k ]) > (min_lag) ? (min_lag) : ((pitch_out[ k ]) < (18 * Fs_kHz) ? (18 * Fs_kHz) : (pitch_out[ k ]))) : ((pitch_out[ k ]) > (18 * Fs_kHz) ? (18 * Fs_kHz) : ((pitch_out[ k ]) < (min_lag) ? (min_lag) : (pitch_out[ k ]))));
        }
        *lagIndex = (opus_int16)( lag_new - min_lag );
        *contourIndex = (int1)CBimax;
    } else {        /* Fs_kHz == 8 */
        /* Save Lags and correlation */
        ;
        *LTPCorr = (float)sqrt( CCmax / (float)nb_subfr ); /* Output normalized correlation */
        for( k = 0; k < nb_subfr; k++ ) {
            pitch_out[ k ] = lag + *(Lag_CB_ptr + ((k)*(cbk_size)+(CBimax)));
            pitch_out[ k ] = ((min_lag_8kHz) > (18 * Fs_kHz) ? ((pitch_out[ k ]) > (min_lag_8kHz) ? (min_lag_8kHz) : ((pitch_out[ k ]) < (18 * Fs_kHz) ? (18 * Fs_kHz) : (pitch_out[ k ]))) : ((pitch_out[ k ]) > (18 * Fs_kHz) ? (18 * Fs_kHz) : ((pitch_out[ k ]) < (min_lag_8kHz) ? (min_lag_8kHz) : (pitch_out[ k ]))));
        }
        *lagIndex = (opus_int16)( lag - min_lag_8kHz );
        *contourIndex = (int1)CBimax;
    }
    ;
    /* return as voiced */
    return 0;
}

public
void silk_P_Ana_calc_corr_st3(
    float cross_corr_st3*[ 34 ][ 5 ], /* O 3 DIM correlation array */
          float    frame*,            /* I vector to correlate                                            */
    int            start_lag,          /* I start lag                                                      */
    int            sf_length,          /* I sub frame length                                               */
    int            nb_subfr,           /* I number of subframes                                            */
    int            complexity          /* I Complexity setting                                             */
)
    /***********************************************************************
     Calculates the correlations used in stage 3 search. In order to cover
     the whole lag codebook for all the searched offset lags (lag +- 2),
     the following correlations are needed in each sub frame:

     sf1: lag range [-8,...,7] total 16 correlations
     sf2: lag range [-4,...,4] total 9 correlations
     sf3: lag range [-3,....4] total 8 correltions
     sf4: lag range [-6,....8] total 15 correlations

     In total 48 correlations. The direct implementation computed in worst case
     4*12*5 = 240 correlations, but more likely around 120.
     **********************************************************************/
{
          float *target_ptr, basis_ptr;
    int   i, j, k, lag_counter, lag_low, lag_high;
    int   nb_cbk_search, delta, idx, cbk_size;
    float scratch_mem[ 22 ];
          int1 *Lag_range_ptr, Lag_CB_ptr;

    clear scratch_mem;
    ;

    if( nb_subfr == 4 ) {
        Lag_range_ptr = &silk_Lag_range_stage3[ complexity ][ 0 ][ 0 ];
        Lag_CB_ptr    = &silk_CB_lags_stage3[ 0 ][ 0 ];
        nb_cbk_search = silk_nb_cbk_searchs_stage3[ complexity ];
        cbk_size      = 34;
    } else {
        ;
        Lag_range_ptr = &silk_Lag_range_stage3_10_ms[ 0 ][ 0 ];
        Lag_CB_ptr    = &silk_CB_lags_stage3_10_ms[ 0 ][ 0 ];
        nb_cbk_search = 12;
        cbk_size      = 12;
    }

    target_ptr = &frame[ ((opus_int32)((opus_uint32)(sf_length)<<(2))) ]; /* Pointer to middle of frame */
    for( k = 0; k < nb_subfr; k++ ) {
        lag_counter = 0;

        /* Calculate the correlations for each subframe */
        lag_low  = *(Lag_range_ptr + ((k)*(2)+(0)));
        lag_high = *(Lag_range_ptr + ((k)*(2)+(1)));
        for( j = lag_low; j <= lag_high; j++ ) {
            basis_ptr = target_ptr - ( start_lag + j );
            ;
            scratch_mem[ lag_counter ] = (float)silk_inner_product_FLP( target_ptr, basis_ptr, sf_length );
            lag_counter++;
        }

        delta = *(Lag_range_ptr + ((k)*(2)+(0)));
        for( i = 0; i < nb_cbk_search; i++ ) {
            /* Fill out the 3 dim array that stores the correlations for */
            /* each code_book vector for each start lag */
            idx = *(Lag_CB_ptr + ((k)*(cbk_size)+(i))) - delta;
            for( j = 0; j < 5; j++ ) {
                ;
                ;
                cross_corr_st3[ k ][ i ][ j ] = scratch_mem[ idx + j ];
            }
        }
        target_ptr += sf_length;
    }
}

public
void silk_P_Ana_calc_energy_st3(
    float energies_st3*[ 34 ][ 5 ], /* O 3 DIM correlation array */
          float    frame*,            /* I vector to correlate                                            */
    int            start_lag,          /* I start lag                                                      */
    int            sf_length,          /* I sub frame length                                               */
    int            nb_subfr,           /* I number of subframes                                            */
    int            complexity          /* I Complexity setting                                             */
)
/****************************************************************
Calculate the energies for first two subframes. The energies are
calculated recursively.
****************************************************************/
{
          float *target_ptr, basis_ptr;
    double    energy;
    int   k, i, j, lag_counter;
    int   nb_cbk_search, delta, idx, cbk_size, lag_diff;
    float scratch_mem[ 22 ];
          int1 *Lag_range_ptr, Lag_CB_ptr;

    clear scratch_mem;
    ;

    if( nb_subfr == 4 ) {
        Lag_range_ptr = &silk_Lag_range_stage3[ complexity ][ 0 ][ 0 ];
        Lag_CB_ptr    = &silk_CB_lags_stage3[ 0 ][ 0 ];
        nb_cbk_search = silk_nb_cbk_searchs_stage3[ complexity ];
        cbk_size      = 34;
    } else {
        ;
        Lag_range_ptr = &silk_Lag_range_stage3_10_ms[ 0 ][ 0 ];
        Lag_CB_ptr    = &silk_CB_lags_stage3_10_ms[ 0 ][ 0 ];
        nb_cbk_search = 12;
        cbk_size      = 12;
    }

    target_ptr = &frame[ ((opus_int32)((opus_uint32)(sf_length)<<(2))) ];
    for( k = 0; k < nb_subfr; k++ ) {
        lag_counter = 0;

        /* Calculate the energy for first lag */
        basis_ptr = target_ptr - ( start_lag + *(Lag_range_ptr + ((k)*(2)+(0))) );
        energy = silk_energy_FLP( basis_ptr, sf_length ) + 1.0e-3;
        ;
        scratch_mem[lag_counter] = (float)energy;
        lag_counter++;

        lag_diff = ( *(Lag_range_ptr + ((k)*(2)+(1))) -  *(Lag_range_ptr + ((k)*(2)+(0))) + 1 );
        for( i = 1; i < lag_diff; i++ ) {
            /* remove part outside new window */
            energy -= basis_ptr[sf_length - i] * (double)basis_ptr[sf_length - i];
            ;

            /* add part that comes into window */
            energy += basis_ptr[ -i ] * (double)basis_ptr[ -i ];
            ;
            ;
            scratch_mem[lag_counter] = (float)energy;
            lag_counter++;
        }

        delta = *(Lag_range_ptr + ((k)*(2)+(0)));
        for( i = 0; i < nb_cbk_search; i++ ) {
            /* Fill out the 3 dim array that stores the correlations for    */
            /* each code_book vector for each start lag                     */
            idx = *(Lag_CB_ptr + ((k)*(cbk_size)+(i))) - delta;
            for( j = 0; j < 5; j++ ) {
                ;
                ;
                energies_st3[ k ][ i ][ j ] = scratch_mem[ idx + j ];
                ;
            }
        }
        target_ptr += sf_length;
    }
}
#end unsafe
