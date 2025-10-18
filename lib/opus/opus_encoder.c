#begin unsafe
/* Copyright (c) 2010-2011 Xiph.Org Foundation, Skype Limited
   Written by Jean-Marc Valin and Koen Vos */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

use opus_types;
use arch;
use celt;
use entenc, entcode;
use modes;
use enc_api;
use float_cast;
use os_support;
use structs_flp;
use control;
use repacketizer;
use lin2log, log2lin;

struct OpusEncoder {
    int          celt_enc_offset;
    int          silk_enc_offset;
    silk_EncControlStruct silk_mode;
    int          application;
    int          channels;
    int          delay_compensation;
    int          force_channels;
    int          signal_type;
    int          user_bandwidth;
    int          max_bandwidth;
    int          user_forced_mode;
    int          voice_ratio;
    opus_int32   Fs;
    int          use_vbr;
    int          vbr_constraint;
    opus_int32   bitrate_bps;
    opus_int32   user_bitrate_bps;
    int          encoder_buffer;

    int          stream_channels;
    opus_int16   hybrid_stereo_width_Q14;
    opus_int32   variable_HP_smth2_Q15;
    opus_val32   hp_mem[4];
    int          mode;
    int          prev_mode;
    int          prev_channels;
    int          prev_framesize;
    int          bandwidth;
    int          silk_bw_switch;
    /* Sampling rate (at the API level) */
    int          first;
    opus_val16   delay_buffer[480*2];

    opus_uint32  rangeFinal;
}

/* Transition tables for the voice and music. First column is the
   middle (memoriless) threshold. The second column is the hysteresis
   (difference with the middle) */
const        opus_int32 mono_voice_bandwidth_thresholds[8] = {
        11000, 1000, /* NB<->MB */
        14000, 1000, /* MB<->WB */
        21000, 2000, /* WB<->SWB */
        29000, 2000, /* SWB<->FB */
};
const        opus_int32 mono_music_bandwidth_thresholds[8] = {
        14000, 1000, /* MB not allowed */
        18000, 2000, /* MB<->WB */
        24000, 2000, /* WB<->SWB */
        33000, 2000, /* SWB<->FB */
};
const        opus_int32 stereo_voice_bandwidth_thresholds[8] = {
        11000, 1000, /* NB<->MB */
        14000, 1000, /* MB<->WB */
        21000, 2000, /* WB<->SWB */
        32000, 2000, /* SWB<->FB */
};
const        opus_int32 stereo_music_bandwidth_thresholds[8] = {
        14000, 1000, /* MB not allowed */
        18000, 2000, /* MB<->WB */
        24000, 2000, /* WB<->SWB */
        48000, 2000, /* SWB<->FB */
};
/* Threshold bit-rates for switching between mono and stereo */
const        opus_int32 stereo_voice_threshold = 26000;
const        opus_int32 stereo_music_threshold = 36000;

/* Threshold bit-rate for switching between SILK/hybrid and CELT-only */
const        opus_int32 mode_thresholds[2][2] = {
      /* voice */ /* music */
      {  48000,      24000}, /* mono */
      {  48000,      24000}, /* stereo */
};

public int encoder_channels (OpusEncoder e)
{
  return e.channels;
}

public int opus_encoder_get_size(int channels)
{
    int silkEncSizeBytes, celtEncSizeBytes;
    int ret;
    if (channels<1 || channels > 2)
        return 0;
    ret = silk_Get_Encoder_Size( &silkEncSizeBytes );
    if (ret!=0)
        return 0;
    silkEncSizeBytes = align(silkEncSizeBytes);
    celtEncSizeBytes = celt_encoder_get_size(channels);
    return align(((int)(OpusEncoder ' size  )))+silkEncSizeBytes+celtEncSizeBytes;
}

public int opus_encoder_init(OpusEncoder* st, opus_int32 Fs, int channels, int application)
{
    byte *silk_enc;
    OpusCustomEncoder *celt_enc;
    int err;
    int ret, silkEncSizeBytes;

   if((Fs!=48000&&Fs!=24000&&Fs!=16000&&Fs!=12000&&Fs!=8000)||(channels!=1&&channels!=2)||
        (application != 2048 && application != 2049
        && application != 2051))
        return -1;

    (memset(((char*)st), 0, (opus_encoder_get_size(channels))*((int)((*((char*)st)) ' size  ))));
    /* Create SILK encoder */
    ret = silk_Get_Encoder_Size( &silkEncSizeBytes );
    if (ret!=0)
        return -1;
    silkEncSizeBytes = align(silkEncSizeBytes);
    st->silk_enc_offset = align(((int)(OpusEncoder ' size  )));
    st->celt_enc_offset = st->silk_enc_offset+silkEncSizeBytes;
    silk_enc = (byte*)st+st->silk_enc_offset;
    celt_enc = (OpusCustomEncoder*)((char*)st+st->celt_enc_offset);

    st->stream_channels  = channels;
	st->channels = channels;

    st->Fs = Fs;

    ret = silk_InitEncoder( silk_enc, &st->silk_mode );
    if(ret!=0)return -3;

    /* default SILK parameters */
    st->silk_mode.nChannelsAPI              = channels;
    st->silk_mode.nChannelsInternal         = channels;
    st->silk_mode.API_sampleRate            = st->Fs;
    st->silk_mode.maxInternalSampleRate     = 16000;
    st->silk_mode.minInternalSampleRate     = 8000;
    st->silk_mode.desiredInternalSampleRate = 16000;
    st->silk_mode.payloadSize_ms            = 20;
    st->silk_mode.bitRate                   = 25000;
    st->silk_mode.packetLossPercentage      = 0;
    st->silk_mode.complexity                = 10;
    st->silk_mode.useInBandFEC              = 0;
    st->silk_mode.useDTX                    = 0;
    st->silk_mode.useCBR                    = 0;

    /* Create CELT encoder */
    /* Initialize CELT encoder */
    err = celt_encoder_init(celt_enc, Fs, channels);
    if(err!=0)return -3;

    opus_custom_encoder_ctl(celt_enc, 10016,  (opus_int32)(0) );
    opus_custom_encoder_ctl(celt_enc, 4010,   (opus_int32)(10) );

    st->use_vbr = 1;
    /* Makes constrained VBR the default (safer for real-time use) */
    st->vbr_constraint = 1;
    st->user_bitrate_bps = -1000;
    st->bitrate_bps = 3000+Fs*channels;
    st->application = application;
    st->signal_type = -1000;
    st->user_bandwidth = -1000;
    st->max_bandwidth = 1105;
    st->force_channels = -1000;
    st->user_forced_mode = -1000;
    st->voice_ratio = -1;
    st->encoder_buffer = st->Fs/100;

    /* Delay compensation of 4 ms (2.5 ms for SILK's extra look-ahead 
       + 1.5 ms for SILK resamplers and stereo prediction) */
    st->delay_compensation = st->Fs/250;

    st->hybrid_stereo_width_Q14 = 1 << 14;
    st->variable_HP_smth2_Q15 = ((opus_int32)((opus_uint32)(silk_lin2log( 60 ))<<(8)));
    st->first = 1;
    st->mode = 1001;
    st->bandwidth = 1105;

    return 0;
}

int pad_frame(    byte      *data, opus_int32 len, opus_int32 new_len)
{
   if (len == new_len)
      return 0;
   if (len > new_len)
      return 1;

   if ((data[0]&0x3)==0)
   {
      int i;
      int padding, nb_255s;

      padding = new_len - len;
      if (padding >= 2)
      {
         nb_255s = (padding-2)/255;

         for (i=len-1;i>=1;i--)
            data[i+nb_255s+2] = data[i];
         data[0] |= 0x3;
         data[1] = 0x41;
         for (i=0;i<nb_255s;i++)
            data[i+2] = 255;
         data[nb_255s+2] = (byte)(padding-255*nb_255s-2);
         for (i=len+3+nb_255s;i<new_len;i++)
            data[i] = 0;
      } else {
         for (i=len-1;i>=1;i--)
            data[i+1] = data[i];
         data[0] |= 0x3;
         data[1] = 1;
      }
      return 0;
   } else {
      return 1;
   }
}

     byte      gen_toc(int mode, int framerate, int bandwidth, int channels)
{
   int period;
       byte      toc;
   period = 0;
   while (framerate < 400)
   {
       *&framerate <<= 1;
       period++;
   }
   if (mode == 1000)
   {
       toc = (byte)((bandwidth-1101)<<5);
       toc |= (byte)((period-2)<<3);
   } else if (mode == 1002)
   {
       int tmp = bandwidth-1102;
       if (tmp < 0)
           tmp = 0;
       toc = 0x80;
       toc |= (byte)(tmp << 5);
       toc |= (byte)(period<<3);
   } else /* Hybrid */
   {
       toc = 0x60;
       toc |= (byte)((bandwidth-1104)<<4);
       toc |= (byte)((period-2)<<3);
   }
   toc |= (byte)((uint)(channels==2)<<2);
   return toc;
}

void silk_biquad_float(
          opus_val16      *in,            /* I:    Input signal                   */
          opus_int32      *B_Q28,         /* I:    MA coefficients [3]            */
          opus_int32      *A_Q28,         /* I:    AR coefficients [2]            */
    opus_val32            *S,             /* I/O:  State vector [2]               */
    opus_val16            *_out,           /* O:    Output signal                  */
          opus_int32      len,            /* I:    Signal length (must be even)   */
    int stride
)
{
    /* DIRECT FORM II TRANSPOSED (uses 2 element state vector) */
    int   k;
    opus_val32 vout;
    opus_val32 inval;
    opus_val32 A[2], B[3];

    A = {(opus_val32)((float)A_Q28[0] * (1.0f/(float)((opus_int32)1<<28))),
         (opus_val32)((float)A_Q28[1] * (1.0f/(float)((opus_int32)1<<28)))};
    B = {(opus_val32)((float)B_Q28[0] * (1.0f/(float)((opus_int32)1<<28))),
         (opus_val32)((float)B_Q28[1] * (1.0f/(float)((opus_int32)1<<28))),
         (opus_val32)((float)B_Q28[2] * (1.0f/(float)((opus_int32)1<<28)))};

    /* Negate A_Q28 values and split in two parts */

    for( k = 0; k < len; k++ ) {
        /* S[ 0 ], S[ 1 ]: Q12 */
        inval = in[ k*stride ];
        vout = S[ 0 ] + B[0]*inval;

        S[ 0 ] = S[1] - vout*A[0] + B[1]*inval;

        S[ 1 ] = - vout*A[1] + B[2]*inval;

        /* Scale back to Q0 and saturate */
        _out[ k*stride ] = vout;
    }
}

void hp_cutoff(      opus_val16 *in, opus_int32 cutoff_Hz, opus_val16 *_out, opus_val32 *hp_mem, int len, int channels, opus_int32 Fs)
{
   opus_int32 B_Q28[ 3 ], A_Q28[ 2 ];
   opus_int32 Fc_Q19, r_Q28, r_Q22;

   ;
   Fc_Q19 = ((opus_int32)((((opus_int32)((opus_int16)(((opus_int32)((1.5 * 3.14159 / 1000.0) * (float)(1 << (19)) + 0.5)))) * (opus_int32)((opus_int16)(cutoff_Hz)))) / (Fs/1000)));
   ;

   r_Q28 = ((opus_int32)((1.0) * (float)(1 << (28)) + 0.5)) - ((((opus_int32)((0.92) * (float)(1 << (9)) + 0.5))) * (Fc_Q19));

   /* b = r * [ 1; -2; 1 ]; */
   /* a = [ 1; -2 * r * ( 1 - 0.5 * Fc^2 ); r^2 ]; */
   B_Q28[ 0 ] = r_Q28;
   B_Q28[ 1 ] = ((opus_int32)((opus_uint32)(-r_Q28)<<(1)));
   B_Q28[ 2 ] = r_Q28;

   /* -r * ( 2 - Fc * Fc ); */
   r_Q22  = ((r_Q28)>>(6));
   A_Q28[ 0 ] = ((((((((r_Q22)) >> 16) * (opus_int32)((opus_int16)((((((((((Fc_Q19)) >> 16) * (opus_int32)((opus_int16)((Fc_Q19)))) + (((((Fc_Q19)) & 0x0000FFFF) * (opus_int32)((opus_int16)((Fc_Q19)))) >> 16)))) + ((((Fc_Q19)) * (((16) == 1 ? (((Fc_Q19)) >> 1) + (((Fc_Q19)) & 1) : ((((Fc_Q19)) >> ((16) - 1)) + 1) >> 1))))) - ((opus_int32)((2.0) * (float)(1 << (22)) + 0.5)))))) + (((((r_Q22)) & 0x0000FFFF) * (opus_int32)((opus_int16)((((((((((Fc_Q19)) >> 16) * (opus_int32)((opus_int16)((Fc_Q19)))) + (((((Fc_Q19)) & 0x0000FFFF) * (opus_int32)((opus_int16)((Fc_Q19)))) >> 16)))) + ((((Fc_Q19)) * (((16) == 1 ? (((Fc_Q19)) >> 1) + (((Fc_Q19)) & 1) : ((((Fc_Q19)) >> ((16) - 1)) + 1) >> 1))))) - ((opus_int32)((2.0) * (float)(1 << (22)) + 0.5)))))) >> 16)))) + ((((r_Q22)) * (((16) == 1 ? (((((((((((Fc_Q19)) >> 16) * (opus_int32)((opus_int16)((Fc_Q19)))) + (((((Fc_Q19)) & 0x0000FFFF) * (opus_int32)((opus_int16)((Fc_Q19)))) >> 16)))) + ((((Fc_Q19)) * (((16) == 1 ? (((Fc_Q19)) >> 1) + (((Fc_Q19)) & 1) : ((((Fc_Q19)) >> ((16) - 1)) + 1) >> 1))))) - ((opus_int32)((2.0) * (float)(1 << (22)) + 0.5)))) >> 1) + (((((((((((Fc_Q19)) >> 16) * (opus_int32)((opus_int16)((Fc_Q19)))) + (((((Fc_Q19)) & 0x0000FFFF) * (opus_int32)((opus_int16)((Fc_Q19)))) >> 16)))) + ((((Fc_Q19)) * (((16) == 1 ? (((Fc_Q19)) >> 1) + (((Fc_Q19)) & 1) : ((((Fc_Q19)) >> ((16) - 1)) + 1) >> 1))))) - ((opus_int32)((2.0) * (float)(1 << (22)) + 0.5)))) & 1) : ((((((((((((Fc_Q19)) >> 16) * (opus_int32)((opus_int16)((Fc_Q19)))) + (((((Fc_Q19)) & 0x0000FFFF) * (opus_int32)((opus_int16)((Fc_Q19)))) >> 16)))) + ((((Fc_Q19)) * (((16) == 1 ? (((Fc_Q19)) >> 1) + (((Fc_Q19)) & 1) : ((((Fc_Q19)) >> ((16) - 1)) + 1) >> 1))))) - ((opus_int32)((2.0) * (float)(1 << (22)) + 0.5)))) >> ((16) - 1)) + 1) >> 1)))));
   A_Q28[ 1 ] = ((((((((r_Q22)) >> 16) * (opus_int32)((opus_int16)((r_Q22)))) + (((((r_Q22)) & 0x0000FFFF) * (opus_int32)((opus_int16)((r_Q22)))) >> 16)))) + ((((r_Q22)) * (((16) == 1 ? (((r_Q22)) >> 1) + (((r_Q22)) & 1) : ((((r_Q22)) >> ((16) - 1)) + 1) >> 1)))));

   silk_biquad_float( in, &B_Q28, &A_Q28, hp_mem, _out, len, channels );
   if( channels == 2 ) {
       silk_biquad_float( in+1, &B_Q28, &A_Q28, hp_mem+2, _out+1, len, channels );
   }
}

void stereo_fade(      opus_val16 *in, opus_val16 *_out, opus_val16 g1, opus_val16 g2,
        int overlap48, int frame_size, int channels,       opus_val16 *window, opus_int32 Fs)
{
    int i;
    int overlap;
    int inc;
    inc = 48000/Fs;
    overlap=overlap48/inc;
    *&g1 = 1.0f-g1;
    *&g2 = 1.0f-g2;
    for (i=0;i<overlap;i++)
    {
       opus_val32 diff;
       opus_val16 g, w;
       w = ((window[i*inc])*(window[i*inc]));
       g = 
(((((opus_val32)(w)*(opus_val32)(g2)))+(opus_val32)(1.0f-w)*(opus_val32)(g1)));
       diff = ((0.5f*((opus_val32)in[i*channels] - (opus_val32)in[i*channels+1])));
       diff = ((g)*(diff));
       _out[i*channels] = _out[i*channels] - diff;
       _out[i*channels+1] = _out[i*channels+1] + diff;
    }
    for (;i<frame_size;i++)
    {
       opus_val32 diff;
       diff = ((0.5f*((opus_val32)in[i*channels] - (opus_val32)in[i*channels+1])));
       diff = ((g2)*(diff));
       _out[i*channels] = _out[i*channels] - diff;
       _out[i*channels+1] = _out[i*channels+1] + diff;
    }
}

public
OpusEncoder *opus_encoder_create(opus_int32 Fs, int channels, int application, int *error)
{
   int ret;
   OpusEncoder *st;
   if((Fs!=48000&&Fs!=24000&&Fs!=16000&&Fs!=12000&&Fs!=8000)||(channels!=1&&channels!=2)||
       (application != 2048 && application != 2049
       && application != 2051))
   {
      if (error!=null)
         *error = -1;
      return null;
   }
   st = (OpusEncoder *)opus_alloc((uint)opus_encoder_get_size(channels));
   if (st == null)
   {
      if (error!=null)
         *error = -7;
      return null;
   }
   ret = opus_encoder_init(st, Fs, channels, application);
   if (error != null)
      *error = ret;
   if (ret != 0)
   {
      opus_free((byte*)st);
      st = null;
   }
   return st;
}

opus_int32 user_bitrate_to_bitrate(OpusEncoder *st, int frame_size, int max_data_bytes)
{
  if(frame_size==0) *&frame_size=st->Fs/400;
  if (st->user_bitrate_bps==-1000)
    return 60*st->Fs/frame_size + st->Fs*st->channels;
  else if (st->user_bitrate_bps==-1)
    return max_data_bytes*8*st->Fs/frame_size;
  else
    return st->user_bitrate_bps;
}

public
opus_int32 opus_encode_float (OpusEncoder *st,       opus_val16 *pcm, int frame_size,
                               byte      *data, opus_int32 out_data_bytes)
{
    byte *silk_enc;
    OpusCustomEncoder *celt_enc;
    int i;
    int ret=0;
    opus_int32 nBytes;
    ec_enc enc;
    int bytes_target;
    int prefill=0;
    int start_band = 0;
    int redundancy = 0;
    int redundancy_bytes = 0;
    int celt_to_silk = 0;
    opus_val16 *pcm_buf;
    int nb_compr_bytes;
    int to_celt = 0;
    opus_uint32 redundant_rng = 0;
    int cutoff_Hz, hp_freq_smth1;
    int voice_est;
    opus_int32 equiv_rate;
    int delay_compensation;
    int frame_rate;
    opus_int32 max_rate;
    int curr_bandwidth;
    opus_int32 max_data_bytes;
    opus_val16 *tmp_prefill;

    ;

    max_data_bytes = ((1276) < (out_data_bytes) ? (1276) : (out_data_bytes));

    st->rangeFinal = 0;
    if (400*frame_size != st->Fs && 200*frame_size != st->Fs && 100*frame_size != st->Fs &&
         50*frame_size != st->Fs &&  25*frame_size != st->Fs &&  50*frame_size != 3*st->Fs)
    {
       ;
       return -1;
    }
    if (max_data_bytes<=0)
    {
       ;
       return -1;
    }
    silk_enc = (byte*)st+st->silk_enc_offset;
    celt_enc = (OpusCustomEncoder*)((char*)st+st->celt_enc_offset);

    if (st->application == 2051)
       delay_compensation = 0;
    else
       delay_compensation = st->delay_compensation;

    st->bitrate_bps = user_bitrate_to_bitrate(st, frame_size, max_data_bytes);

    frame_rate = st->Fs/frame_size;
    if (max_data_bytes<3 || st->bitrate_bps < 3*frame_rate*8
       || (frame_rate<50 && (max_data_bytes*frame_rate<300 || st->bitrate_bps < 2400)))
    {
       /*If the space is too low to do something useful, emit 'PLC' frames.*/
       int tocmode = st->mode;
       int bw = st->bandwidth == 0 ? 1101 : st->bandwidth;
       if (tocmode==0)
          tocmode = 1000;
       if (frame_rate>100)
          tocmode = 1002;
       if (frame_rate < 50)
          tocmode = 1000;
       if(tocmode==1000&&bw>1103)
          bw=1103;
       else if (tocmode==1002&&bw==1102)
          bw=1101;
       else if (bw<=1104)
          bw=1104;
       data[0] = gen_toc(tocmode, frame_rate, bw, st->stream_channels);
       ;
       return 1;
    }
    if (st->use_vbr==0)
    {
       int cbrBytes;
       cbrBytes = (((st->bitrate_bps + 4*frame_rate)/(8*frame_rate)) < (max_data_bytes) ? ((st->bitrate_bps + 4*frame_rate)/(8*frame_rate)) : (max_data_bytes));
       st->bitrate_bps = cbrBytes * (8*frame_rate);
       max_data_bytes = cbrBytes;
    }
    max_rate = frame_rate*max_data_bytes*8;

    /* Equivalent 20-ms rate for mode/channel/bandwidth decisions */
    equiv_rate = st->bitrate_bps - 60*(st->Fs/frame_size - 50);

    if (st->signal_type == 3001)
       voice_est = 127;
    else if (st->signal_type == 3002)
       voice_est = 0;
    else if (st->voice_ratio >= 0)
       voice_est = st->voice_ratio*327>>8;
    else if (st->application == 2048)
       voice_est = 115;
    else
       voice_est = 48;

    if (st->force_channels!=-1000 && st->channels == 2)
    {
        st->stream_channels = st->force_channels;
    } else {

       /* Rate-dependent mono-stereo decision */
       if (st->channels == 2)
       {
          opus_int32 stereo_threshold;
          stereo_threshold = stereo_music_threshold + ((voice_est*voice_est*(stereo_voice_threshold-stereo_music_threshold))>>14);
          if (st->stream_channels == 2)
             stereo_threshold -= 4000;
          else
             stereo_threshold += 4000;
          st->stream_channels = (equiv_rate > stereo_threshold) ? 2 : 1;
       } else {
          st->stream_channels = st->channels;
       }
    }

    /* Mode selection depending on application and signal type */
    if (st->application == 2051)
    {
       st->mode = 1002;
    } else if (st->user_forced_mode == -1000)
    {

       int chan;
       opus_int32 mode_voice, mode_music;
       opus_int32 threshold;

       chan = (int)((st->channels==2) && st->force_channels!=1);
       mode_voice = mode_thresholds[chan][0];
       mode_music = mode_thresholds[chan][1];
       threshold = mode_music + ((voice_est*voice_est*(mode_voice-mode_music))>>14);

       /* Hysteresis */
       if (st->prev_mode == 1002)
           threshold -= 4000;
       else if (st->prev_mode>0)
           threshold += 4000;

       st->mode = (equiv_rate >= threshold) ? 1002: 1000;

       /* When FEC is enabled and there's enough packet loss, use SILK */
       if (st->silk_mode.useInBandFEC!=0 && st->silk_mode.packetLossPercentage > (128-voice_est)>>4)
          st->mode = 1000;
       /* When encoding voice and DTX is enabled, set the encoder to SILK mode (at least for now) */
       if (st->silk_mode.useDTX!=0 && voice_est > 100)
          st->mode = 1000;
    } else {
       st->mode = st->user_forced_mode;
    }

    /* Override the chosen mode to make sure we meet the requested frame size */
    if (st->mode != 1002 && frame_size < st->Fs/100)
       st->mode = 1002;

    if (st->stream_channels == 1 && st->prev_channels ==2 && st->silk_mode.toMono==0
          && st->mode != 1002 && st->prev_mode != 1002)
    {
       /* Delay stereo->mono transition by two frames so that SILK can do a smooth downmix */
       st->silk_mode.toMono = 1;
       st->stream_channels = 2;
    } else {
       st->silk_mode.toMono = 0;
    }

    if (st->prev_mode > 0 &&
        ((st->mode != 1002 && st->prev_mode == 1002) ||
    (st->mode == 1002 && st->prev_mode != 1002)))
    {
        redundancy = 1;
        celt_to_silk = (int)(st->mode != 1002);
        if (celt_to_silk==0)
        {
            /* Switch to SILK/hybrid if frame size is 10 ms or more*/
            if (frame_size >= st->Fs/100)
            {
                st->mode = st->prev_mode;
                to_celt = 1;
            } else {
                redundancy=0;
            }
        }
    }
    if (st->silk_bw_switch!=0)
    {
       redundancy = 1;
       celt_to_silk = 1;
       st->silk_bw_switch = 0;
    }

    if (st->mode != 1002 && st->prev_mode == 1002)
    {
        silk_EncControlStruct dummy;
        silk_InitEncoder( silk_enc, &dummy);
        prefill=1;
    }

    /* Automatic (rate-dependent) bandwidth selection */
    if (st->mode == 1002 || st->first!=0 || st->silk_mode.allowBandwidthSwitch!=0)
    {
        opus_int32* voice_bandwidth_thresholds, music_bandwidth_thresholds;
        opus_int32 bandwidth_thresholds[8];
        int bandwidth = 1105;
        opus_int32 equiv_rate2;

        equiv_rate2 = equiv_rate;
        if (st->mode != 1002)
        {
           /* Adjust the threshold +/- 10% depending on complexity */
           equiv_rate2 = equiv_rate2 * (45+st->silk_mode.complexity)/50;
           /* CBR is less efficient by ~1 kb/s */
           if (st->use_vbr==0)
              equiv_rate2 -= 1000;
        }
        if (st->channels==2 && st->force_channels!=1)
        {
           voice_bandwidth_thresholds = &stereo_voice_bandwidth_thresholds;
           music_bandwidth_thresholds = &stereo_music_bandwidth_thresholds;
        } else {
           voice_bandwidth_thresholds = &mono_voice_bandwidth_thresholds;
           music_bandwidth_thresholds = &mono_music_bandwidth_thresholds;
        }
        /* Interpolate bandwidth thresholds depending on voice estimation */
        clear bandwidth_thresholds;
		for (i=0;i<8;i++)
        {
           bandwidth_thresholds[i] = music_bandwidth_thresholds[i]
                    + ((voice_est*voice_est*(voice_bandwidth_thresholds[i]-music_bandwidth_thresholds[i]))>>14);
        }
        for(;;)
		{
            int threshold, hysteresis;
            threshold = bandwidth_thresholds[2*(bandwidth-1102)];
            hysteresis = bandwidth_thresholds[2*(bandwidth-1102)+1];
            if (st->first==0)
            {
                if (st->bandwidth >= bandwidth)
                    threshold -= hysteresis;
                else
                    threshold += hysteresis;
            }
            if (equiv_rate2 >= threshold)
                break;
          if (!(--bandwidth>1101)) break;
		}
        st->bandwidth = bandwidth;
        /* Prevents any transition to SWB/FB until the SILK layer has fully
           switched to WB mode and turned the variable LP filter off */
        if (st->first==0 && st->mode != 1002 && st->silk_mode.inWBmodeWithoutVariableLP==0 && st->bandwidth > 1103)
            st->bandwidth = 1103;
    }

    if (st->bandwidth>st->max_bandwidth)
       st->bandwidth = st->max_bandwidth;

    if (st->user_bandwidth != -1000)
        st->bandwidth = st->user_bandwidth;

    /* This prevents us from using hybrid at unsafe CBR/max rates */
    if (st->mode != 1002 && max_rate < 15000)
    {
       st->bandwidth = ((st->bandwidth) < (1103) ? (st->bandwidth) : (1103));
    }

    /* Prevents Opus from wasting bits on frequencies that are above
       the Nyquist rate of the input signal */
    if (st->Fs <= 24000 && st->bandwidth > 1104)
        st->bandwidth = 1104;
    if (st->Fs <= 16000 && st->bandwidth > 1103)
        st->bandwidth = 1103;
    if (st->Fs <= 12000 && st->bandwidth > 1102)
        st->bandwidth = 1102;
    if (st->Fs <= 8000 && st->bandwidth > 1101)
        st->bandwidth = 1101;

    /* If max_data_bytes represents less than 8 kb/s, switch to CELT-only mode */
    if (max_data_bytes < (frame_rate > 50 ? 12000 : 8000)*frame_size / (st->Fs * 8))
       st->mode = 1002;

    /* CELT mode doesn't support mediumband, use wideband instead */
    if (st->mode == 1002 && st->bandwidth == 1102)
        st->bandwidth = 1103;

    /* Can't support higher than wideband for >20 ms frames */
    if (frame_size > st->Fs/50 && (st->mode == 1002 || st->bandwidth > 1103))
    {
           byte      *tmp_data;
       int nb_frames;
       int bak_mode, bak_bandwidth, bak_channels, bak_to_mono;
       OpusRepacketizer rp;
       opus_int32 bytes_per_frame;

       nb_frames = frame_size > st->Fs/25 ? 3 : 2;
       bytes_per_frame = ((1276) < ((out_data_bytes-3)/nb_frames) ? (1276) : ((out_data_bytes-3)/nb_frames));

       tmp_data = ((    byte     *)malloc(((int)(    byte      ' size  ))*(nb_frames*bytes_per_frame)));

       opus_repacketizer_init(&rp);

       bak_mode = st->user_forced_mode;
       bak_bandwidth = st->user_bandwidth;
       bak_channels = st->force_channels;

       st->user_forced_mode = st->mode;
       st->user_bandwidth = st->bandwidth;
       st->force_channels = st->stream_channels;
       bak_to_mono = st->silk_mode.toMono;

       if (bak_to_mono!=0)
          st->force_channels = 1;
       else
          st->prev_channels = st->stream_channels;
       for (i=0;i<nb_frames;i++)
       {
          int tmp_len;
          st->silk_mode.toMono = 0;
          /* When switching from SILK/Hybrid to CELT, only ask for a switch at the last frame */
          if (to_celt!=0 && i==nb_frames-1)
             st->user_forced_mode = 1002;
          tmp_len = opus_encode_float(st, pcm+i*(st->channels*st->Fs/50), st->Fs/50, tmp_data+i*bytes_per_frame, bytes_per_frame);
          if (tmp_len<0)
          {
             afree((byte*)tmp_data);
             ;
             return -3;
          }
          ret = opus_repacketizer_cat(&rp, tmp_data+i*bytes_per_frame, tmp_len);
          if (ret<0)
          {
             afree((byte*)tmp_data);
             ;
             return -3;
          }
       }
       ret = opus_repacketizer_out(&rp, data, out_data_bytes);
       if (ret<0)
       {
		  afree((byte*)tmp_data);
          ;
          return -3;
       }
       st->user_forced_mode = bak_mode;
       st->user_bandwidth = bak_bandwidth;
       st->force_channels = bak_channels;
       st->silk_mode.toMono = bak_to_mono;
	   afree((byte*)tmp_data);
       ;
       return ret;
    }

    curr_bandwidth = st->bandwidth;

    /* Chooses the appropriate mode for speech
       *NEVER* switch to/from CELT-only mode here as this will invalidate some assumptions */
    if (st->mode == 1000 && curr_bandwidth > 1103)
        st->mode = 1001;
    if (st->mode == 1001 && curr_bandwidth <= 1103)
        st->mode = 1000;

    /* printf("%d %d %d %d\n", st->bitrate_bps, st->stream_channels, st->mode, curr_bandwidth); */
    bytes_target = ((max_data_bytes) < (st->bitrate_bps * frame_size / (st->Fs * 8)) ? (max_data_bytes) : (st->bitrate_bps * frame_size / (st->Fs * 8))) - 1;

    *&data += 1;

    ec_enc_init(&enc, data, (uint)max_data_bytes-1);

    pcm_buf = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*((delay_compensation+frame_size)*st->channels)));
    for (i=0;i<delay_compensation*st->channels;i++)
       pcm_buf[i] = st->delay_buffer[(st->encoder_buffer-delay_compensation)*st->channels+i];

    if (st->mode == 1002)
       hp_freq_smth1 = ((opus_int32)((opus_uint32)(silk_lin2log( 60 ))<<(8)));
    else
       hp_freq_smth1 = ((silk_encoder*)silk_enc)->state_Fxx[0].sCmn.variable_HP_smth1_Q15;

    st->variable_HP_smth2_Q15 = 
((st->variable_HP_smth2_Q15) + ((((hp_freq_smth1 - st->variable_HP_smth2_Q15) >> 16) * (opus_int32)((opus_int16)(((opus_int32)((0.015f) * (float)(1 << (16)) + 0.5))))) + ((((hp_freq_smth1 - st->variable_HP_smth2_Q15) & 0x0000FFFF) * (opus_int32)((opus_int16)(((opus_int32)((0.015f) * (float)(1 << (16)) + 0.5))))) >> 16)));

    /* convert from log scale to Hertz */
    cutoff_Hz = silk_log2lin( ((st->variable_HP_smth2_Q15)>>(8)) );

    if (st->application == 2048)
    {
       hp_cutoff(pcm, cutoff_Hz, &pcm_buf[delay_compensation*st->channels], &st->hp_mem, frame_size, st->channels, st->Fs);
    } else {
       for (i=0;i<frame_size*st->channels;i++)
          pcm_buf[delay_compensation*st->channels + i] = pcm[i];
    }

    /* SILK processing */
    if (st->mode != 1002)
    {

       opus_int16 *pcm_silk;
       pcm_silk = ((opus_int16*)malloc(((int)(opus_int16 ' size  ))*(st->channels*frame_size)));
        st->silk_mode.bitRate = 8*bytes_target*frame_rate;
        if( st->mode == 1001 ) {
            st->silk_mode.bitRate /= st->stream_channels;
            if( curr_bandwidth == 1104 ) {
                if( st->Fs == 100 * frame_size ) {
                    /* 24 kHz, 10 ms */
                    st->silk_mode.bitRate = ( ( st->silk_mode.bitRate + 2000 + st->use_vbr * 1000 ) * 2 ) / 3;
                } else {
                    /* 24 kHz, 20 ms */
                    st->silk_mode.bitRate = ( ( st->silk_mode.bitRate + 1000 + st->use_vbr * 1000 ) * 2 ) / 3;
                }
            } else {
                if( st->Fs == 100 * frame_size ) {
                    /* 48 kHz, 10 ms */
                    st->silk_mode.bitRate = ( st->silk_mode.bitRate + 8000 + st->use_vbr * 3000 ) / 2;
                } else {
                    /* 48 kHz, 20 ms */
                    st->silk_mode.bitRate = ( st->silk_mode.bitRate + 9000 + st->use_vbr * 1000 ) / 2;
                }
            }
            st->silk_mode.bitRate *= st->stream_channels;
            /* don't let SILK use more than 80% */
            if( st->silk_mode.bitRate > ( st->bitrate_bps - 8*st->Fs/frame_size ) * 4/5 ) {
                st->silk_mode.bitRate = ( st->bitrate_bps - 8*st->Fs/frame_size ) * 4/5;
            }
        }

        st->silk_mode.payloadSize_ms = 1000 * frame_size / st->Fs;
        st->silk_mode.nChannelsAPI = st->channels;
        st->silk_mode.nChannelsInternal = st->stream_channels;
        if (curr_bandwidth == 1101) {
            st->silk_mode.desiredInternalSampleRate = 8000;
        } else if (curr_bandwidth == 1102) {
            st->silk_mode.desiredInternalSampleRate = 12000;
        } else {
            ;
            st->silk_mode.desiredInternalSampleRate = 16000;
        }
        if( st->mode == 1001 ) {
            /* Don't allow bandwidth reduction at lowest bitrates in hybrid mode */
            st->silk_mode.minInternalSampleRate = 16000;
        } else {
            st->silk_mode.minInternalSampleRate = 8000;
        }

        if (st->mode == 1000)
        {
           opus_int32 effective_max_rate = max_rate;
           st->silk_mode.maxInternalSampleRate = 16000;
           if (frame_rate > 50)
              effective_max_rate = effective_max_rate*2/3;
           if (effective_max_rate < 13000)
           {
              st->silk_mode.maxInternalSampleRate = 12000;
              st->silk_mode.desiredInternalSampleRate = ((12000) < (st->silk_mode.desiredInternalSampleRate) ? (12000) : (st->silk_mode.desiredInternalSampleRate));
           }
           if (effective_max_rate < 9600)
           {
              st->silk_mode.maxInternalSampleRate = 8000;
              st->silk_mode.desiredInternalSampleRate = ((8000) < (st->silk_mode.desiredInternalSampleRate) ? (8000) : (st->silk_mode.desiredInternalSampleRate));
           }
        } else {
           st->silk_mode.maxInternalSampleRate = 16000;
        }

        st->silk_mode.useCBR = (int)(st->use_vbr==0);

        /* Call SILK encoder for the low band */
        nBytes = ((1275) < (max_data_bytes-1) ? (1275) : (max_data_bytes-1));

        st->silk_mode.maxBits = nBytes*8;
        /* Only allow up to 90% of the bits for hybrid mode*/
        if (st->mode == 1001)
           st->silk_mode.maxBits = (opus_int32)st->silk_mode.maxBits*9/10;
        if (st->silk_mode.useCBR!=0)
        {
           st->silk_mode.maxBits = (st->silk_mode.bitRate * frame_size / (st->Fs * 8))*8;
           /* Reduce the initial target to make it easier to reach the CBR rate */
           st->silk_mode.bitRate = ((1) > (st->silk_mode.bitRate-2000) ? (1) : (st->silk_mode.bitRate-2000));
        }
        if (redundancy!=0)
           st->silk_mode.maxBits -= st->silk_mode.maxBits/(1 + frame_size/(st->Fs/200));

        if (prefill!=0)
        {
            opus_int32 zero=0;

            for (i=0;i<st->encoder_buffer*st->channels;i++)
                pcm_silk[i] = FLOAT2INT16(st->delay_buffer[i]);
            silk_Encode( silk_enc, &st->silk_mode, pcm_silk, st->encoder_buffer, null, &zero, 1 );
        }

        for (i=0;i<frame_size*st->channels;i++)
            pcm_silk[i] = FLOAT2INT16(pcm_buf[delay_compensation*st->channels + i]);
        ret = silk_Encode( silk_enc, &st->silk_mode, pcm_silk, frame_size, &enc, &nBytes, 0 );
        if( ret!=0 ) {
            /*fprintf (stderr, "SILK encode error: %d\n", ret);*/
            /* Handle error */
           afree((byte*)pcm_buf);
		   
		     afree((byte*)pcm_silk);
		   ;
           return -3;
        }
        if (nBytes==0)
        {
           st->rangeFinal = 0;
           data[-1] = gen_toc(st->mode, st->Fs/frame_size, curr_bandwidth, st->stream_channels);
           afree((byte*)pcm_buf);
	       afree((byte*)pcm_silk);
           ;
           return 1;
        }
        /* Extract SILK internal bandwidth for signaling in first byte */
        if( st->mode == 1000 ) {
            if( st->silk_mode.internalSampleRate == 8000 ) {
               curr_bandwidth = 1101;
            } else if( st->silk_mode.internalSampleRate == 12000 ) {
               curr_bandwidth = 1102;
            } else if( st->silk_mode.internalSampleRate == 16000 ) {
               curr_bandwidth = 1103;
            }
        } else {
            ;
        }

        st->silk_mode.opusCanSwitch = st->silk_mode.switchReady;
        if (st->silk_mode.opusCanSwitch!=0)
        {
           redundancy = 1;
           celt_to_silk = 0;
           st->silk_bw_switch = 1;
        }
		
		
		   afree((byte*)pcm_silk);
    }

    /* CELT processing */
    {
        int endband=21;

        switch(curr_bandwidth)
        {
            case 1101:
                endband = 13;
                break;
            case 1102:
            case 1103:
                endband = 17;
                break;
            case 1104:
                endband = 19;
                break;
            case 1105:
                endband = 21;
                break;
			default:
			    break;
        }
        opus_custom_encoder_ctl(celt_enc, 10012,  (opus_int32)(endband));
        opus_custom_encoder_ctl(celt_enc, 10008, (opus_int32)(st->stream_channels));
    }
    opus_custom_encoder_ctl(celt_enc, 4002,  (opus_int32)(-1));
    if (st->mode != 1000)
    {
        opus_custom_encoder_ctl(celt_enc, 4006,  (opus_int32)(0));
        /* Allow prediction unless we decide to disable it later */
        opus_custom_encoder_ctl(celt_enc, 10002, (opus_int32)(2));

        if (st->mode == 1001)
        {
            int len;

            len = (ec_tell(&enc)+7)>>3;
            if (redundancy!=0)
               len += st->mode == 1001 ? 3 : 1;
            if( st->use_vbr!=0 ) {
                nb_compr_bytes = len + bytes_target - (st->silk_mode.bitRate * frame_size) / (8 * st->Fs);
            } else {
                /* check if SILK used up too much */
                nb_compr_bytes = len > bytes_target ? len : bytes_target;
            }
        } else {
            if (st->use_vbr!=0)
            {
                opus_custom_encoder_ctl(celt_enc, 4006,  (opus_int32)(1));
                opus_custom_encoder_ctl(celt_enc, 4020, (opus_int32)(st->vbr_constraint));
                opus_custom_encoder_ctl(celt_enc, 4002,  (opus_int32)(st->bitrate_bps));
                nb_compr_bytes = max_data_bytes-1;
            } else {
                nb_compr_bytes = bytes_target;
            }
        }

    } else {
        nb_compr_bytes = 0;
    }

    tmp_prefill = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(st->channels*st->Fs/400)));
    if (st->mode != 1000 && st->mode != st->prev_mode && st->prev_mode > 0)
    {
       for (i=0;i<st->channels*st->Fs/400;i++)
          tmp_prefill[i] = st->delay_buffer[(st->encoder_buffer-st->delay_compensation-st->Fs/400)*st->channels + i];
    }

    for (i=0;i<st->channels*(st->encoder_buffer-(frame_size+delay_compensation));i++)
        st->delay_buffer[i] = st->delay_buffer[i+st->channels*frame_size];
    for (;i<st->encoder_buffer*st->channels;i++)
        st->delay_buffer[i] = pcm_buf[(frame_size+delay_compensation-st->encoder_buffer)*st->channels+i];

    if (st->mode != 1001 || st->stream_channels==1)
       st->silk_mode.stereoWidth_Q14 = 1<<14;
    if( st->channels == 2 ) {
        /* Apply stereo width reduction (at low bitrates) */
        if( st->hybrid_stereo_width_Q14 < (1 << 14) || st->silk_mode.stereoWidth_Q14 < (1 << 14) ) {
            opus_val16 g1, g2;
            OpusCustomMode *celt_mode;

            opus_custom_encoder_ctl(celt_enc, 10015, ((&celt_mode) + ((&celt_mode) - (      OpusCustomMode**)(&celt_mode))));
            g1 = (float)st->hybrid_stereo_width_Q14;
            g2 = (opus_val16)(st->silk_mode.stereoWidth_Q14);

            g1 *= (1.0f/16384.0);
            g2 *= (1.0f/16384.0);
            stereo_fade(pcm_buf, pcm_buf, g1, g2, celt_mode->overlap,
                  frame_size, st->channels, &celt_mode->window, st->Fs);
            st->hybrid_stereo_width_Q14 = (int2)st->silk_mode.stereoWidth_Q14;
        }
    }

    if ( st->mode != 1002 && ec_tell(&enc)+17+20*(int)(st->mode == 1001) <= 8*(max_data_bytes-1))
    {
        /* For SILK mode, the redundancy is inferred from the length */
        if (st->mode == 1001 && (redundancy!=0 || ec_tell(&enc)+37 <= 8*nb_compr_bytes))
           ec_enc_bit_logp(&enc, redundancy, 12);
        if (redundancy!=0)
        {
            int max_redundancy;
            ec_enc_bit_logp(&enc, celt_to_silk, 1);
            if (st->mode == 1001)
               max_redundancy = (max_data_bytes-1)-nb_compr_bytes-1;
            else
               max_redundancy = (max_data_bytes-1)-((ec_tell(&enc)+7)>>3);
            /* Target the same bit-rate for redundancy as for the rest,
               up to a max of 257 bytes */
            redundancy_bytes = ((max_redundancy) < (st->bitrate_bps/1600) ? (max_redundancy) : (st->bitrate_bps/1600));
            redundancy_bytes = ((257) < (((2) > (redundancy_bytes) ? (2) : (redundancy_bytes))) ? (257) : (((2) > (redundancy_bytes) ? (2) : (redundancy_bytes))));
            if (st->mode == 1001)
                ec_enc_uint(&enc, (uint)redundancy_bytes-2, 256);
        }
    } else {
        redundancy = 0;
    }

    if (redundancy==0)
       st->silk_bw_switch = 0;

    if (st->mode != 1002)start_band=17;

    if (st->mode == 1000)
    {
        ret = (ec_tell(&enc)+7)>>3;
        ec_enc_done(&enc);
        nb_compr_bytes = ret;
    } else {
       nb_compr_bytes = (((max_data_bytes-1)-redundancy_bytes) < (nb_compr_bytes) ? ((max_data_bytes-1)-redundancy_bytes) : (nb_compr_bytes));
       ec_enc_shrink(&enc, (uint)nb_compr_bytes);
    }

    /* 5 ms redundant frame for CELT->SILK */
    if (redundancy!=0 && celt_to_silk!=0)
    {
        int err;
        opus_custom_encoder_ctl(celt_enc, 10010,  (opus_int32)(0));
        opus_custom_encoder_ctl(celt_enc, 4006,  (opus_int32)(0));
        err = celt_encode_with_ec(celt_enc, pcm_buf, st->Fs/200, data+nb_compr_bytes, redundancy_bytes, null);
        if (err < 0)
        {
           afree((byte*)pcm_buf);
		   afree((byte*)tmp_prefill);
		   ;
           return -3;
        }
        opus_custom_encoder_ctl(celt_enc, 4031, ((&redundant_rng) + ((&redundant_rng) - (opus_uint32*)(&redundant_rng))));
        opus_custom_encoder_ctl(celt_enc, 4028, 0);
    }

    opus_custom_encoder_ctl(celt_enc, 10010,  (opus_int32)(start_band));

    if (st->mode != 1000)
    {
        if (st->mode != st->prev_mode && st->prev_mode > 0)
        {
           byte      dummy[2];
           opus_custom_encoder_ctl(celt_enc, 4028, 0);

           /* Prefilling */
           celt_encode_with_ec(celt_enc, tmp_prefill, st->Fs/400, &dummy, 2, null);
           opus_custom_encoder_ctl(celt_enc, 10002, (opus_int32)(0));
        }
        /* If false, we already busted the budget and we'll end up with a "PLC packet" */
        if (ec_tell(&enc) <= 8*nb_compr_bytes)
        {
           ret = celt_encode_with_ec(celt_enc, pcm_buf, frame_size, null, nb_compr_bytes, &enc);
           if (ret < 0)
           {
              afree((byte*)pcm_buf);
		      afree((byte*)tmp_prefill);
			  ;
              return -3;
           }
        }
    }

    /* 5 ms redundant frame for SILK->CELT */
    if (redundancy!=0 && celt_to_silk==0)
    {
        int err;
            byte      dummy[2];
        int N2, N4;
        N2 = st->Fs/200;
        N4 = st->Fs/400;

        opus_custom_encoder_ctl(celt_enc, 4028, (int)0);
        opus_custom_encoder_ctl(celt_enc, 10010,  (opus_int32)(0));
        opus_custom_encoder_ctl(celt_enc, 10002,  (opus_int32)(0));

        /* NOTE: We could speed this up slightly (at the expense of code size) by just adding a function that prefills the buffer */
        celt_encode_with_ec(celt_enc, pcm_buf+st->channels*(frame_size-N2-N4), N4, &dummy, 2, null);

        err = celt_encode_with_ec(celt_enc, pcm_buf+st->channels*(frame_size-N2), N2, data+nb_compr_bytes, redundancy_bytes, null);
        if (err < 0)
        {
           afree((byte*)pcm_buf);
		   afree((byte*)tmp_prefill);
		   ;
           return -3;
        }
        opus_custom_encoder_ctl(celt_enc, 4031, ((&redundant_rng) + ((&redundant_rng) - (opus_uint32*)(&redundant_rng))));
    }

    /* Signalling the mode in the first byte */
    (*&data)--;
    data[0] = gen_toc(st->mode, st->Fs/frame_size, curr_bandwidth, st->stream_channels);

    st->rangeFinal = enc.rng ^ redundant_rng;

    if (to_celt!=0)
        st->prev_mode = 1002;
    else
        st->prev_mode = st->mode;
    st->prev_channels = st->stream_channels;
    st->prev_framesize = frame_size;

    st->first = 0;

    /* In the unlikely case that the SILK encoder busted its target, tell
       the decoder to call the PLC */
    if (ec_tell(&enc) > (max_data_bytes-1)*8)
    {
       if (max_data_bytes < 2)
       {
          afree((byte*)pcm_buf);
		  afree((byte*)tmp_prefill);
		  ;
          return -2;
       }
       data[1] = 0;
       ret = 1;
       st->rangeFinal = 0;
    } else if (st->mode==1000&&redundancy==0)
    {
       /*When in LPC only mode it's perfectly
         reasonable to strip off trailing zero bytes as
         the required range decoder behavior is to
         fill these in. This can't be done when the MDCT
         modes are used because the decoder needs to know
         the actual length for allocation purposes.*/
       while(ret>2&&data[ret]==0)ret--;
    }
    /* Count ToC and redundancy */
    ret += 1+redundancy_bytes;
    if (st->use_vbr==0 && ret >= 3)
    {
       if (pad_frame(data, ret, max_data_bytes) != 0)
       {
          afree((byte*)pcm_buf);
	      afree((byte*)tmp_prefill);
          ;
          return -3;
       }
       ret = max_data_bytes;
    }
    afree((byte*)pcm_buf);
    afree((byte*)tmp_prefill);
    ;
    return ret;
}

public
opus_int32 opus_encode(OpusEncoder *st,       opus_int16 *pcm, int frame_size,
          byte      *data, opus_int32 max_data_bytes)
{
   int i, ret;
   float *in;
   ;

   in = ((float*)malloc(((int)(float ' size  ))*(frame_size*st->channels)));

   for (i=0;i<frame_size*st->channels;i++)
      in[i] = (1.0f/32768.0)*(float)pcm[i];
   ret = opus_encode_float(st, in, frame_size, data, max_data_bytes);
   afree((byte*)in);
   ;
   return ret;
}

public int opus_encoder_ctl(OpusEncoder *st, int request, byte[] value0)
{
    int ret;
    OpusCustomEncoder *celt_enc;

    ret = 0;

//    ( ap = (va_list)&request + ( (((int)((request) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) ) );

    celt_enc = (OpusCustomEncoder*)((char*)st+st->celt_enc_offset);

    switch (request)
    {
        case 4000:
        {
            opus_int32 value; // ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if (   (value != 2048 && value != 2049
                 && value != 2051)
               || (st->first==0 && st->application != value))
            {
               ret = -1;
               break;
            }
            st->application = value;
        }
        break;
        case 4001:
        {
            opus_int32 *value;
			value'byte = value0'byte;
            *value = st->application;
        }
        break;
        case 4002:
        {
            opus_int32 value; // ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if (value != -1000 && value != -1)
            {
                if (value <= 0)
                    return -1;
                else if (value <= 500)
                    value = 500;
                else if (value > (opus_int32)300000*st->channels)
                    value = (opus_int32)300000*st->channels;
            }
            st->user_bitrate_bps = value;
        }
        break;
        case 4003:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = user_bitrate_to_bitrate(st, st->prev_framesize, 1276);
        }
        break;
        case 4022:
        {
            opus_int32 value; // ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if((value<1 || value>st->channels) && value != -1000)
                return -1;
            st->force_channels = value;
        }
        break;
        case 4023:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->force_channels;
        }
        break;
        case 4004:
        {
            opus_int32 value; // ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if (value < 1101 || value > 1105)
                return -1;
            st->max_bandwidth = value;
            if (st->max_bandwidth == 1101) {
                st->silk_mode.maxInternalSampleRate = 8000;
            } else if (st->max_bandwidth == 1102) {
                st->silk_mode.maxInternalSampleRate = 12000;
            } else {
                st->silk_mode.maxInternalSampleRate = 16000;
            }
        }
        break;
        case 4005:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->max_bandwidth;
        }
        break;
        case 4008:
        {
            opus_int32 value; //( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if ((value < 1101 || value > 1105) && value != -1000)
                return -1;
            st->user_bandwidth = value;
            if (st->user_bandwidth == 1101) {
                st->silk_mode.maxInternalSampleRate = 8000;
            } else if (st->user_bandwidth == 1102) {
                st->silk_mode.maxInternalSampleRate = 12000;
            } else {
                st->silk_mode.maxInternalSampleRate = 16000;
            }
        }
        break;
        case 4009:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->bandwidth;
        }
        break;
        case 4016:
        {
            opus_int32 value; //( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if(value<0 || value>1)
                return -1;
            st->silk_mode.useDTX = value;
        }
        break;
        case 4017:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->silk_mode.useDTX;
        }
        break;
        case 4010:
        {
            opus_int32 value; //( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if(value<0 || value>10)
                return -1;
            st->silk_mode.complexity = value;
            opus_custom_encoder_ctl(celt_enc, 4010,  (opus_int32)(value));
        }
        break;
        case 4011:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->silk_mode.complexity;
        }
        break;
        case 4012:
        {
            opus_int32 value; //( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if(value<0 || value>1)
                return -1;
            st->silk_mode.useInBandFEC = value;
        }
        break;
        case 4013:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->silk_mode.useInBandFEC;
        }
        break;
        case 4014:
        {
            opus_int32 value; //( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if (value < 0 || value > 100)
                return -1;
            st->silk_mode.packetLossPercentage = value;
            opus_custom_encoder_ctl(celt_enc, 4014, (opus_int32)(value));
        }
        break;
        case 4015:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->silk_mode.packetLossPercentage;
        }
        break;
        case 4006:
        {
            opus_int32 value; // ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if(value<0 || value>1)
                return -1;
            st->use_vbr = value;
            st->silk_mode.useCBR = 1-value;
        }
        break;
        case 4007:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->use_vbr;
        }
        break;
        case 11018:
        {
            opus_int32 value; // ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if (value>100 || value<-1)
                return -1; // goto bad_arg;
            st->voice_ratio = value;
        }
        break;
        case 11019:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->voice_ratio;
        }
        break;
        case 4020:
        {
            opus_int32 value; //( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if(value<0 || value>1)
                return -1;
            st->vbr_constraint = value;
        }
        break;
        case 4021:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->vbr_constraint;
        }
        break;
        case 4024:
        {
            opus_int32 value; // ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if(value!=-1000 && value!=3001 && value!=3002)
                return -1;
            st->signal_type = value;
        }
        break;
        case 4025:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->signal_type;
        }
        break;
        case 4027:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->Fs/400;
            if (st->application != 2051)
                *value += st->delay_compensation;
        }
        break;
        case 4029:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if (value==null)
            {
                ret = -1;
                break;
            }
            *value = st->Fs;
        }
        break;
        case 4031:
        {
            opus_uint32 *value; // = ( *(opus_uint32* *)((ap += ( (((int)((opus_uint32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_uint32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            *value = st->rangeFinal;
        }
        break;
        case 4036:
        {
            opus_int32 value; //( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            ret = opus_custom_encoder_ctl(celt_enc, 4036,  (opus_int32)(value));
        }
        break;
        case 4037:
        {
            opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            opus_custom_encoder_ctl(celt_enc, 4037, ((value) + ((value) - (opus_int32*)(value))));
        }
        break;
        case 4028:
        {
           byte *silk_enc;
           silk_EncControlStruct dummy;
           silk_enc = (byte*)st+st->silk_enc_offset;
           
           memset(((char*)&st->stream_channels), 0,
                 (((int)(OpusEncoder ' size  ))- (int)((char*)&st->stream_channels - (char*)st))*((int)((*((char*)&st->stream_channels)) ' size  )));

           opus_custom_encoder_ctl(celt_enc, 4028, 0);
           silk_InitEncoder( silk_enc, &dummy );
           st->stream_channels = st->channels;
           st->hybrid_stereo_width_Q14 = 1 << 14;
           st->first = 1;
           st->mode = 1001;
           st->bandwidth = 1105;
           st->variable_HP_smth2_Q15 = ((opus_int32)((opus_uint32)(silk_lin2log( 60 ))<<(8)));
        }
        break;
        case 11002:
        {
            opus_int32 value; //( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
			value'byte = value0'byte;
            if ((value < 1000 || value > 1002) && value != -1000)
               return -1;
            st->user_forced_mode = value;
        }
        break;
        default:
            /* fprintf(stderr, "unknown opus_encoder_ctl() request: %d", request);*/
            ret = -5;
            break;
    }
    return ret;
}

public
void opus_encoder_destroy(OpusEncoder *st)
{
    opus_free((byte*)st);
}
#end unsafe
