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

/*****************************************************************************
* Pitch analyser function
******************************************************************************/

/************************************************************/
/* CORE PITCH ANALYSIS FUNCTION                             */
/************************************************************/
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
);


void silk_P_Ana_calc_corr_st3(
    float cross_corr_st3*[ 34 ][ 5 ], /* O 3 DIM correlation array */
          float    frame*,            /* I vector to correlate                                            */
    int            start_lag,          /* I start lag                                                      */
    int            sf_length,          /* I sub frame length                                               */
    int            nb_subfr,           /* I number of subframes                                            */
    int            complexity          /* I Complexity setting                                             */
);


void silk_P_Ana_calc_energy_st3(
    float energies_st3*[ 34 ][ 5 ], /* O 3 DIM correlation array */
          float    frame*,            /* I vector to correlate                                            */
    int            start_lag,          /* I start lag                                                      */
    int            sf_length,          /* I sub frame length                                               */
    int            nb_subfr,           /* I number of subframes                                            */
    int            complexity          /* I Complexity setting                                             */
);
#end unsafe
