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
use structs, tables;

/* Control SNR of redidual quantizer */
public
int silk_control_SNR(
    silk_encoder_state          *psEncC,                        /* I/O  Pointer to Silk encoder state               */
    opus_int32                  TargetRate_bps                  /* I    Target max bitrate (bps)                    */
)
{
    int k, ret = 0;
    opus_int32 frac_Q6;
          opus_int32 *rateTable;

    /* Set bitrate/coding quality */
    *&TargetRate_bps = ((5000) > (80000) ? ((TargetRate_bps) > (5000) ? (5000) : ((TargetRate_bps) < (80000) ? (80000) : (TargetRate_bps))) : ((TargetRate_bps) > (80000) ? (80000) : ((TargetRate_bps) < (5000) ? (5000) : (TargetRate_bps))));
    if( TargetRate_bps != psEncC->TargetRate_bps ) {
        psEncC->TargetRate_bps = TargetRate_bps;

        /* If new TargetRate_bps, translate to SNR_dB value */
        if( psEncC->fs_kHz == 8 ) {
            rateTable = &silk_TargetRate_table_NB;
        } else if( psEncC->fs_kHz == 12 ) {
            rateTable = &silk_TargetRate_table_MB;
        } else {
            rateTable = &silk_TargetRate_table_WB;
        }

        /* Reduce bitrate for 10 ms modes in these calculations */
        if( psEncC->nb_subfr == 2 ) {
            *&TargetRate_bps -= 2200;
        }

        /* Find bitrate interval in table and interpolate */
        for( k = 1; k < 8; k++ ) {
            if( TargetRate_bps <= rateTable[ k ] ) {
                frac_Q6 = 
((opus_int32)((((opus_int32)((opus_uint32)(TargetRate_bps - rateTable[ k - 1 ])<<(6)))) / (rateTable[ k ] - rateTable[ k - 1 ])));
                psEncC->SNR_dB_Q7 = ((opus_int32)((opus_uint32)(silk_SNR_table_Q1[ k - 1 ])<<(6))) + ((frac_Q6) * (silk_SNR_table_Q1[ k ] - silk_SNR_table_Q1[ k - 1 ]));
                break;
            }
        }

        /* Reduce coding quality whenever LBRR is enabled, to free up some bits */
        if( psEncC->LBRR_enabled != 0 ) {
            psEncC->SNR_dB_Q7 = ((psEncC->SNR_dB_Q7) + ((opus_int32)((opus_int16)(12 - psEncC->LBRR_GainIncreases))) * (opus_int32)((opus_int16)(((opus_int32)((-0.25) * (float)((int8)1 << (7)) + 0.5)))));
        }
    }

    return ret;
}
#end unsafe
