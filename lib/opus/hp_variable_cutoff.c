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

use opus_types, structs_flp, structs, lin2log;

/* High-pass filter with cutoff frequency adaptation based on pitch lag statistics */
public
void silk_HP_variable_cutoff(
    silk_encoder_state_FLP          state_Fxx*                         /* I/O  Encoder states                              */
)
{
   int   quality_Q15;
   opus_int32 pitch_freq_Hz_Q16, pitch_freq_log_Q7, delta_freq_Q7;
   silk_encoder_state *psEncC1 = &state_Fxx[ 0 ].sCmn;

   /* Adaptive cutoff frequency: estimate low end of pitch frequency range */
   if( psEncC1->prevSignalType == 2 ) {
      /* difference, in log domain */
      pitch_freq_Hz_Q16 = ((opus_int32)((((opus_int32)((opus_uint32)(((psEncC1->fs_kHz) * (1000)))<<(16)))) / (psEncC1->prevLag)));
      pitch_freq_log_Q7 = silk_lin2log( pitch_freq_Hz_Q16 ) - ( 16 << 7 );

      /* adjustment based on quality */
      quality_Q15 = psEncC1->input_quality_bands_Q15[ 0 ];
      pitch_freq_log_Q7 = 
((pitch_freq_log_Q7) + ((((((((((opus_int32)((opus_uint32)(-quality_Q15)<<(2)))) >> 16) * (opus_int32)((opus_int16)(quality_Q15))) + ((((((opus_int32)((opus_uint32)(-quality_Q15)<<(2)))) & 0x0000FFFF) * (opus_int32)((opus_int16)(quality_Q15))) >> 16))) >> 16) * (opus_int32)((opus_int16)(pitch_freq_log_Q7 - ( silk_lin2log( ((opus_int32)((60.0) * (float)((int8)1 << (16)) + 0.5)) ) - ( 16 << 7 ) )))) + ((((((((((opus_int32)((opus_uint32)(-quality_Q15)<<(2)))) >> 16) * (opus_int32)((opus_int16)(quality_Q15))) + ((((((opus_int32)((opus_uint32)(-quality_Q15)<<(2)))) & 0x0000FFFF) * (opus_int32)((opus_int16)(quality_Q15))) >> 16))) & 0x0000FFFF) * (opus_int32)((opus_int16)(pitch_freq_log_Q7 - ( silk_lin2log( ((opus_int32)((60.0) * (float)((int8)1 << (16)) + 0.5)) ) - ( 16 << 7 ) )))) >> 16)));

      /* delta_freq = pitch_freq_log - psEnc->variable_HP_smth1; */
      delta_freq_Q7 = pitch_freq_log_Q7 - ((psEncC1->variable_HP_smth1_Q15)>>(8));
      if( delta_freq_Q7 < 0 ) {
         /* less smoothing for decreasing pitch frequency, to track something close to the minimum */
         delta_freq_Q7 = ((delta_freq_Q7) * (3));
      }

      /* limit delta, to reduce impact of outliers in pitch estimation */
      delta_freq_Q7 = ((-((opus_int32)((0.4f) * (float)((int8)1 << (7)) + 0.5))) > (((opus_int32)((0.4f) * (float)((int8)1 << (7)) + 0.5))) ? ((delta_freq_Q7) > (-((opus_int32)((0.4f) * (float)((int8)1 << (7)) + 0.5))) ? (-((opus_int32)((0.4f) * (float)((int8)1 << (7)) + 0.5))) : ((delta_freq_Q7) < (((opus_int32)((0.4f) * (float)((int8)1 << (7)) + 0.5))) ? (((opus_int32)((0.4f) * (float)((int8)1 << (7)) + 0.5))) : (delta_freq_Q7))) : ((delta_freq_Q7) > (((opus_int32)((0.4f) * (float)((int8)1 << (7)) + 0.5))) ? (((opus_int32)((0.4f) * (float)((int8)1 << (7)) + 0.5))) : ((delta_freq_Q7) < (-((opus_int32)((0.4f) * (float)((int8)1 << (7)) + 0.5))) ? (-((opus_int32)((0.4f) * (float)((int8)1 << (7)) + 0.5))) : (delta_freq_Q7))));

      /* update smoother */
      psEncC1->variable_HP_smth1_Q15 = 
((psEncC1->variable_HP_smth1_Q15) + ((((((opus_int32)((opus_int16)(psEncC1->speech_activity_Q8)) * (opus_int32)((opus_int16)(delta_freq_Q7)))) >> 16) * (opus_int32)((opus_int16)(((opus_int32)((0.1f) * (float)((int8)1 << (16)) + 0.5))))) + ((((((opus_int32)((opus_int16)(psEncC1->speech_activity_Q8)) * (opus_int32)((opus_int16)(delta_freq_Q7)))) & 0x0000FFFF) * (opus_int32)((opus_int16)(((opus_int32)((0.1f) * (float)((int8)1 << (16)) + 0.5))))) >> 16)));

      /* limit frequency range */
      psEncC1->variable_HP_smth1_Q15 = 

((((opus_int32)((opus_uint32)(silk_lin2log( 60 ))<<(8)))) > (((opus_int32)((opus_uint32)(silk_lin2log( 100 ))<<(8)))) ? ((psEncC1->variable_HP_smth1_Q15) > (((opus_int32)((opus_uint32)(silk_lin2log( 60 ))<<(8)))) ? (((opus_int32)((opus_uint32)(silk_lin2log( 60 ))<<(8)))) : ((psEncC1->variable_HP_smth1_Q15) < (((opus_int32)((opus_uint32)(silk_lin2log( 100 ))<<(8)))) ? (((opus_int32)((opus_uint32)(silk_lin2log( 100 ))<<(8)))) : (psEncC1->variable_HP_smth1_Q15))) : ((psEncC1->variable_HP_smth1_Q15) > (((opus_int32)((opus_uint32)(silk_lin2log( 100 ))<<(8)))) ? (((opus_int32)((opus_uint32)(silk_lin2log( 100 ))<<(8)))) : ((psEncC1->variable_HP_smth1_Q15) < (((opus_int32)((opus_uint32)(silk_lin2log( 60 ))<<(8)))) ? (((opus_int32)((opus_uint32)(silk_lin2log( 60 ))<<(8)))) : (psEncC1->variable_HP_smth1_Q15))));
   }
}
#end unsafe
