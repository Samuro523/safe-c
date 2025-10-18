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

use resampler_structs, opus_types;

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


/* Tables with delay compensation values to equalize total delay for different modes */
const        int1 delay_matrix_enc[ 5 ][ 3 ] = {
/* in  \ out  8  12  16 */
/*  8 */   {  6,  0,  3 },
/* 12 */   {  0,  7,  3 },
/* 16 */   {  0,  1, 10 },
/* 24 */   {  0,  2,  6 },
/* 48 */   { 18, 10, 12 }
};

const        int1 delay_matrix_dec[ 3 ][ 5 ] = {
/* in  \ out  8  12  16  24  48 */
/*  8 */   {  4,  0,  2,  0,  0 },
/* 12 */   {  0,  9,  4,  7,  4 },
/* 16 */   {  0,  3, 12,  7,  7 }
};

/* Simple way to make [8000, 12000, 16000, 24000, 48000] to [0, 1, 2, 3, 4] */

/* Initialize/reset the resampler state for a given pair of input/output sampling rates */
int silk_resampler_init(
    silk_resampler_state_struct *S,                 /* I/O  Resampler state                                             */
    opus_int32                  Fs_Hz_in,           /* I    Input sampling rate (Hz)                                    */
    opus_int32                  Fs_Hz_out,          /* I    Output sampling rate (Hz)                                   */
    int                    forEnc              /* I    If 1: encoder; if 0: decoder                                */
);

/* Resampler: convert from one sampling rate to another */
/* Input and output sampling rate are at most 48000 Hz  */
int silk_resampler(
    silk_resampler_state_struct *S,                 /* I/O  Resampler state                                             */
    opus_int16                  _out*,              /* O    Output signal                                               */
          opus_int16            in*,               /* I    Input signal                                                */
    opus_int32                  inLen               /* I    Number of input samples                                     */
);

#end unsafe
