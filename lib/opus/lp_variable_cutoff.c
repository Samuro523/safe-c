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

use opus_types, tables, os_support, structs, biquad_alt;

/*
    Elliptic/Cauer filters designed with 0.1 dB passband ripple,
    80 dB minimum stopband attenuation, and
    [0.95 : 0.15 : 0.35] normalized cut off frequencies.
*/



/* Helper function, interpolates the filter taps */
public void silk_LP_interpolate_filter_taps(
    opus_int32           B_Q28*, // [ 3 ],
    opus_int32           A_Q28*, // [ 2 ],
          int       ind,
          opus_int32     fac_Q16
)
{
    int nb, na;

    if( ind < 5 - 1 ) {
        if( fac_Q16 > 0 ) {
            if( fac_Q16 < 32768 ) { /* fac_Q16 is in range of a 16-bit int */
                /* Piece-wise linear interpolation of B and A */
                for( nb = 0; nb < 3; nb++ ) {
                    B_Q28[ nb ] = 

((silk_Transition_LP_B_Q28[ ind ][ nb ]) + ((((silk_Transition_LP_B_Q28[ ind + 1 ][ nb ] - silk_Transition_LP_B_Q28[ ind ][ nb ]) >> 16) * (opus_int32)((opus_int16)(fac_Q16))) + ((((silk_Transition_LP_B_Q28[ ind + 1 ][ nb ] - silk_Transition_LP_B_Q28[ ind ][ nb ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(fac_Q16))) >> 16)));
                }
                for( na = 0; na < 2; na++ ) {
                    A_Q28[ na ] = 

((silk_Transition_LP_A_Q28[ ind ][ na ]) + ((((silk_Transition_LP_A_Q28[ ind + 1 ][ na ] - silk_Transition_LP_A_Q28[ ind ][ na ]) >> 16) * (opus_int32)((opus_int16)(fac_Q16))) + ((((silk_Transition_LP_A_Q28[ ind + 1 ][ na ] - silk_Transition_LP_A_Q28[ ind ][ na ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(fac_Q16))) >> 16)));
                }
            } else { /* ( fac_Q16 - ( 1 << 16 ) ) is in range of a 16-bit int */
                ;
                /* Piece-wise linear interpolation of B and A */
                for( nb = 0; nb < 3; nb++ ) {
                    B_Q28[ nb ] = 

((silk_Transition_LP_B_Q28[ ind + 1 ][ nb ]) + ((((silk_Transition_LP_B_Q28[ ind + 1 ][ nb ] - silk_Transition_LP_B_Q28[ ind ][ nb ]) >> 16) * (opus_int32)((opus_int16)(fac_Q16 - ( (opus_int32)1 << 16 )))) + ((((silk_Transition_LP_B_Q28[ ind + 1 ][ nb ] - silk_Transition_LP_B_Q28[ ind ][ nb ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(fac_Q16 - ( (opus_int32)1 << 16 )))) >> 16)));
                }
                for( na = 0; na < 2; na++ ) {
                    A_Q28[ na ] = 

((silk_Transition_LP_A_Q28[ ind + 1 ][ na ]) + ((((silk_Transition_LP_A_Q28[ ind + 1 ][ na ] - silk_Transition_LP_A_Q28[ ind ][ na ]) >> 16) * (opus_int32)((opus_int16)(fac_Q16 - ( (opus_int32)1 << 16 )))) + ((((silk_Transition_LP_A_Q28[ ind + 1 ][ na ] - silk_Transition_LP_A_Q28[ ind ][ na ]) & 0x0000FFFF) * (opus_int32)((opus_int16)(fac_Q16 - ( (opus_int32)1 << 16 )))) >> 16)));
                }
            }
        } else {
            memcpy((byte*)(B_Q28), (byte*)&(silk_Transition_LP_B_Q28[ ind ]), (3 * ((int)(opus_int32 ' size  ))));
            memcpy((byte*)(A_Q28), (byte*)&(silk_Transition_LP_A_Q28[ ind ]), (2 * ((int)(opus_int32 ' size  ))));
        }
    } else {
        memcpy((byte*)(B_Q28), (byte*)&(silk_Transition_LP_B_Q28[ 5 - 1 ]), (3 * ((int)(opus_int32 ' size  ))));
        memcpy((byte*)(A_Q28), (byte*)&(silk_Transition_LP_A_Q28[ 5 - 1 ]), (2 * ((int)(opus_int32 ' size  ))));
    }
}

/* Low-pass filter with variable cutoff frequency based on  */
/* piece-wise linear interpolation between elliptic filters */
/* Start by setting psEncC->mode <> 0;                      */
/* Deactivate by setting psEncC->mode = 0;                  */
public void silk_LP_variable_cutoff(
    silk_LP_state               *psLP,                          /* I/O  LP filter state                             */
    opus_int16                  *frame,                         /* I/O  Low-pass filtered output signal             */
          int              frame_length                    /* I    Frame length                                */
)
{
    opus_int32   B_Q28[ 3 ], A_Q28[ 2 ], fac_Q16 = 0;
    int     ind = 0;

    ;

    /* Run filter if needed */
    if( psLP->mode != 0 ) {
        /* Calculate index and interpolation factor for interpolation */

        fac_Q16 = ((opus_int32)((opus_uint32)(( 5120 / ( 5 * 4 ) ) - psLP->transition_frame_no)<<(16 - 6)));

        ind      = ((fac_Q16)>>(16));
        fac_Q16 -= ((opus_int32)((opus_uint32)(ind)<<(16)));

        ;
        ;

        /* Interpolate filter coefficients */
        silk_LP_interpolate_filter_taps( &B_Q28, &A_Q28, ind, fac_Q16 );

        /* Update transition frame number for next frame */
        psLP->transition_frame_no = ((0) > (( 5120 / ( 5 * 4 ) )) ? ((psLP->transition_frame_no + psLP->mode) > (0) ? (0) : ((psLP->transition_frame_no + psLP->mode) < (( 5120 / ( 5 * 4 ) )) ? (( 5120 / ( 5 * 4 ) )) : (psLP->transition_frame_no + psLP->mode))) : ((psLP->transition_frame_no + psLP->mode) > (( 5120 / ( 5 * 4 ) )) ? (( 5120 / ( 5 * 4 ) )) : ((psLP->transition_frame_no + psLP->mode) < (0) ? (0) : (psLP->transition_frame_no + psLP->mode))));

        /* ARMA low-pass filtering */
        ;
        silk_biquad_alt( frame, &B_Q28, &A_Q28, &psLP->In_LP_State, frame, frame_length, 1);
    }
}
#end unsafe
