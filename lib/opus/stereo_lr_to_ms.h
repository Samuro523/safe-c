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

use opus_types, structs;


/* Convert Left/Right stereo signal to adaptive Mid/Side representation */

void silk_stereo_LR_to_MS(
    stereo_enc_state            *state,                         /* I/O  State                                       */
    opus_int16                  x1*,                           /* I/O  Left input signal, becomes mid signal       */
    opus_int16                  x2*,                           /* I/O  Right input signal, becomes side signal     */
    int1                   ix*[ 3 ],                   /* O    Quantization indices                        */
    int1                   *mid_only_flag,                 /* O    Flag: only mid signal coded                 */
    opus_int32                  mid_side_rates_bps*,           /* O    Bitrates for mid and side signals           */
    opus_int32                  total_rate_bps,                 /* I    Total bitrate                               */
    int                    prev_speech_act_Q8,             /* I    Speech activity level in previous frame     */
    int                    toMono,                         /* I    Last frame before a stereo->mono transition */
    int                    fs_kHz,                         /* I    Sample rate (kHz)                           */
    int                    frame_length                    /* I    Number of samples                           */
);

#end unsafe
