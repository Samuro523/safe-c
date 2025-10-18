#begin unsafe
/* Copyright (c) 2010 Xiph.Org Foundation, Skype Limited
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

use ../math;

use opus_types, arch;
use celt;
use entdec, entcode;
use modes;
use dec_api, control;
use float_cast;
use os_support;
use repacketizer;

struct OpusDecoder {
   int          celt_dec_offset;
   int          silk_dec_offset;
   int          channels;
   opus_int32   Fs;          /** Sampling rate (at the API level) */
   silk_DecControlStruct DecControl;
   int          decode_gain;

   /* Everything beyond this point gets cleared on a reset */

   int          stream_channels;

   int          bandwidth;
   int          mode;
   int          prev_mode;
   int          frame_size;
   int          prev_redundancy;

   opus_uint32  rangeFinal;
}

public int decoder_channels (OpusDecoder d)
{
  return d.channels;
}

public
int opus_decoder_get_size(int channels)
{
   int silkDecSizeBytes, celtDecSizeBytes;
   int ret;
   if (channels<1 || channels > 2)
      return 0;
   ret = silk_Get_Decoder_Size( &silkDecSizeBytes );
   if(ret!=0)
      return 0;
   silkDecSizeBytes = align(silkDecSizeBytes);
   celtDecSizeBytes = celt_decoder_get_size(channels);
   return align(((int)(OpusDecoder ' size  )))+silkDecSizeBytes+celtDecSizeBytes;
}

public
int opus_decoder_init(OpusDecoder *st, opus_int32 Fs, int channels)
{
   byte *silk_dec;
   OpusCustomDecoder *celt_dec;
   int ret, silkDecSizeBytes;

   if ((Fs!=48000&&Fs!=24000&&Fs!=16000&&Fs!=12000&&Fs!=8000)
    || (channels!=1&&channels!=2))
      return -1;

   memset(((char*)st), 0, (opus_decoder_get_size(channels))*((int)((*((char*)st)) ' size  )));
   /* Initialize SILK encoder */
   ret = silk_Get_Decoder_Size(&silkDecSizeBytes);
   if (ret!=0)
      return -3;

   silkDecSizeBytes = align(silkDecSizeBytes);
   st->silk_dec_offset = align(((int)(OpusDecoder ' size  )));
   st->celt_dec_offset = st->silk_dec_offset+silkDecSizeBytes;
   silk_dec = (byte*)st+st->silk_dec_offset;
   celt_dec = (OpusCustomDecoder*)((char*)st+st->celt_dec_offset);
   st->stream_channels = channels;
   st->channels = channels;

   st->Fs = Fs;
   st->DecControl.API_sampleRate = st->Fs;
   st->DecControl.nChannelsAPI      = st->channels;

   /* Reset decoder */
   ret = silk_InitDecoder( silk_dec );
   if(ret!=0)return -3;

   /* Initialize CELT decoder */
   ret = celt_decoder_init(celt_dec, Fs, channels);
   if(ret!=0)return -3;

   opus_custom_decoder_ctl(celt_dec, 10016,  (opus_int32)(0));

   st->prev_mode = 0;
   st->frame_size = Fs/400;
   return 0;
}

public
OpusDecoder *opus_decoder_create(opus_int32 Fs, int channels, int *error)
{
   int ret;
   OpusDecoder *st;
   if ((Fs!=48000&&Fs!=24000&&Fs!=16000&&Fs!=12000&&Fs!=8000)
    || (channels!=1&&channels!=2))
   {
      if (error!=null)
         *error = -1;
      return null;
   }
   st = (OpusDecoder *)opus_alloc((uint)opus_decoder_get_size(channels));
   if (st == null)
   {
      if (error!=null)
         *error = -7;
      return null;
   }
   ret = opus_decoder_init(st, Fs, channels);
   if (error!=null)
      *error = ret;
   if (ret != 0)
   {
      opus_free((byte*)st);
      st = null;
   }
   return st;
}

void smooth_fade(      opus_val16 *in1,       opus_val16 *in2,
      opus_val16 *_out, int overlap, int channels,
            opus_val16 *window, opus_int32 Fs)
{
   int i, c;
   int inc = 48000/Fs;
   for (c=0;c<channels;c++)
   {
      for (i=0;i<overlap;i++)
      {
         opus_val16 w = ((window[i*inc])*(window[i*inc]));
         _out[i*channels+c] = 
(((((opus_val32)(w)*(opus_val32)(in2[i*channels+c])))+(opus_val32)(1.0f-w)*(opus_val32)(in1[i*channels+c])));
      }
   }
}

int opus_packet_get_mode(          byte      *data)
{
   int mode;
   if ((data[0]&0x80) != 0)
   {
      mode = 1002;
   } else if ((data[0]&0x60) == 0x60)
   {
      mode = 1001;
   } else {
      mode = 1000;
   }
   return mode;
}

int opus_decode_frame(OpusDecoder *st,           byte      *data,
      opus_int32 len, opus_val16 *pcm, int frame_size, int decode_fec)
{
   byte *silk_dec;
   OpusCustomDecoder *celt_dec;
   int i, silk_ret=0, celt_ret=0;
   ec_dec dec;
   opus_int32 silk_frame_size;
   opus_int16 *pcm_silk;
   opus_val16 *pcm_transition;
   opus_val16 *redundant_audio;

   int audiosize;
   int mode;
   int transition=0;
   int start_band;
   int redundancy=0;
   int redundancy_bytes = 0;
   int celt_to_silk=0;
   int c;
   int F2_5, F5, F10, F20;
   opus_val16 *window;
   opus_uint32 redundant_rng = 0;
   ;

   clear dec;
   silk_dec = (byte*)st+st->silk_dec_offset;
   celt_dec = (OpusCustomDecoder*)((byte*)st+st->celt_dec_offset);
   F20 = st->Fs/50;
   F10 = F20>>1;
   F5 = F10>>1;
   F2_5 = F5>>1;
   if (frame_size < F2_5)
   {
      ;
      return -2;
   }
   /* Limit frame_size to avoid excessive stack allocations. */
   *&frame_size = ((frame_size) < (st->Fs/25*3) ? (frame_size) : (st->Fs/25*3));
   /* Payloads of 1 (2 including ToC) or 0 trigger the PLC/DTX */
   if (len<=1)
   {
      *&data = null;
      /* In that case, don't conceal more than what the ToC says */
      *&frame_size = ((frame_size) < (st->frame_size) ? (frame_size) : (st->frame_size));
   }
   if (data != null)
   {
      audiosize = st->frame_size;
      mode = st->mode;
      ec_dec_init(&dec,(    byte     *)data,(uint)len);
   } else {
      audiosize = frame_size;

      if (st->prev_mode == 0)
      {
         /* If we haven't got any packet yet, all we can do is return zeros */
         for (i=0;i<audiosize*st->channels;i++)
            pcm[i] = 0.0;
         ;
         return audiosize;
      } else {
         mode = st->prev_mode;
      }
   }

   /* For CELT/hybrid PLC of more than 20 ms, do multiple calls */
   if (data==null && frame_size > F20 && mode != 1000)
   {
      int nb_samples = 0;
      for(;;)
	  {
         int ret = opus_decode_frame(st, null, 0, pcm, F20, 0);
         if (ret != F20)
         {
            ;
            return -3;
         }
         *&pcm += F20*st->channels;
         nb_samples += F20;
        if (!(nb_samples < frame_size)) break;
	  }
      ;
      return frame_size;
   }
   pcm_transition = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(F5*st->channels)));

   if (data!=null && st->prev_mode > 0 && (
       (mode == 1002 && st->prev_mode != 1002 && st->prev_redundancy==0)
    || (mode != 1002 && st->prev_mode == 1002) )
      )
   {
      transition = 1;
      if (mode == 1002)
         opus_decode_frame(st, null, 0, pcm_transition, ((F5) < (audiosize) ? (F5) : (audiosize)), 0);
   }
   if (audiosize > frame_size)
   {
      /*fprintf(stderr, "PCM buffer too small: %d vs %d (mode = %d)\n", audiosize, frame_size, mode);*/
	  afree((byte*)pcm_transition);
      ;
      return -1;
   } else {
      *&frame_size = audiosize;
   }

   pcm_silk = ((opus_int16*)malloc(((int)(opus_int16 ' size  ))*(((F10) > (frame_size) ? (F10) : (frame_size))*st->channels)));
   redundant_audio = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(F5*st->channels)));

   /* SILK processing */
   if (mode != 1002)
   {
      int lost_flag, decoded_samples;
      opus_int16 *pcm_ptr = pcm_silk;

      if (st->prev_mode==1002)
         silk_InitDecoder( silk_dec );

      /* The SILK PLC cannot produce frames of less than 10 ms */
      st->DecControl.payloadSize_ms = ((10) > (1000 * audiosize / st->Fs) ? (10) : (1000 * audiosize / st->Fs));

      if (data != null)
      {
        st->DecControl.nChannelsInternal = st->stream_channels;
        if( mode == 1000 ) {
           if( st->bandwidth == 1101 ) {
              st->DecControl.internalSampleRate = 8000;
           } else if( st->bandwidth == 1102 ) {
              st->DecControl.internalSampleRate = 12000;
           } else if( st->bandwidth == 1103 ) {
              st->DecControl.internalSampleRate = 16000;
           } else {
              st->DecControl.internalSampleRate = 16000;
              ;
           }
        } else {
           /* Hybrid mode */
           st->DecControl.internalSampleRate = 16000;
        }
     }

     lost_flag = data == null ? 1 : 2 * decode_fec;
     decoded_samples = 0;
     for(;;)
	 {
        /* Call SILK decoder */
        int first_frame = (int)(decoded_samples == 0);
        silk_ret = silk_Decode( silk_dec, &st->DecControl,
                                lost_flag, first_frame, &dec, pcm_ptr, &silk_frame_size );
        if( silk_ret!=0 ) {
           if (lost_flag!=0 ) {
              /* PLC failure should not be fatal */
              silk_frame_size = frame_size;
              for (i=0;i<frame_size*st->channels;i++)
                 pcm_ptr[i] = 0;
           } else {
			afree((byte*)pcm_silk);
			afree((byte*)redundant_audio);
            afree((byte*)pcm_transition);
             ;
             return -4;
           }
        }
        pcm_ptr += silk_frame_size * st->channels;
        decoded_samples += silk_frame_size;
        if (!( decoded_samples < frame_size )) break;
	  }
   }

   start_band = 0;
   if (decode_fec==0 && mode != 1002 && data != null
    && ec_tell(&dec)+17+20*(int)(st->mode == 1001) <= 8*len)
   {
      /* Check if we have a redundant 0-8 kHz band */
      if (mode == 1001)
         redundancy = ec_dec_bit_logp(&dec, 12);
      else
         redundancy = 1;
      if (redundancy!=0)
      {
         celt_to_silk = ec_dec_bit_logp(&dec, 1);
         /* redundancy_bytes will be at least two, in the non-hybrid
            case due to the ec_tell() check above */
         redundancy_bytes = mode==1001 ?
               (opus_int32)ec_dec_uint(&dec, 256)+2 :
               len-((ec_tell(&dec)+7)>>3);
         *&len -= redundancy_bytes;
         /* This is a sanity check. It should never happen for a valid
            packet, so the exact behaviour is not normative. */
         if (len*8 < ec_tell(&dec))
         {
            *&len = 0;
            redundancy_bytes = 0;
            redundancy = 0;
         }
         /* Shrink decoder because of raw bits */
         dec.storage -= (uint)redundancy_bytes;
      }
   }
   if (mode != 1002)
      start_band = 17;

   {
      int endband=21;

      switch(st->bandwidth)
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
      opus_custom_decoder_ctl(celt_dec, 10012,  (opus_int32)(endband));
      opus_custom_decoder_ctl(celt_dec, 10008,  (opus_int32)(st->stream_channels));
   }

   if (redundancy!=0)
      transition = 0;

   if (transition!=0 && mode != 1002)
      opus_decode_frame(st, null, 0, pcm_transition, ((F5) < (audiosize) ? (F5) : (audiosize)), 0);

   /* 5 ms redundant frame for CELT->SILK*/
   if (redundancy !=0 && celt_to_silk != 0)
   {
      opus_custom_decoder_ctl(celt_dec, 10010,  (opus_int32)(0));
      celt_decode_with_ec(celt_dec, data+len, redundancy_bytes,
                          redundant_audio, F5, null);
      opus_custom_decoder_ctl(celt_dec, 4031, ((&redundant_rng) + ((&redundant_rng) - (opus_uint32*)(&redundant_rng))));
   }

   /* MUST be after PLC */
   opus_custom_decoder_ctl(celt_dec, 10010,  (opus_int32)(start_band));

   if (mode != 1000)
   {
      int celt_frame_size = ((F20) < (frame_size) ? (F20) : (frame_size));
      /* Make sure to discard any previous CELT state */
      if (mode != st->prev_mode && st->prev_mode > 0 && st->prev_redundancy==0)
         opus_custom_decoder_ctl(celt_dec, 4028, 0);
      /* Decode CELT */
      celt_ret = celt_decode_with_ec(celt_dec, decode_fec!=0 ? null : data,
                                     len, pcm, celt_frame_size, &dec);
   } else {
          byte      silence[2] = {0xFF, 0xFF};
      for (i=0;i<frame_size*st->channels;i++)
         pcm[i] = 0.0;
      /* For hybrid -> SILK transitions, we let the CELT MDCT
         do a fade-out by decoding a silence frame */
      if (st->prev_mode == 1001 && !(redundancy!=0 && celt_to_silk!=0 && st->prev_redundancy!=0) )
      {
         opus_custom_decoder_ctl(celt_dec, 10010,  (opus_int32)(0));
         celt_decode_with_ec(celt_dec, &silence, 2, pcm, F2_5, null);
      }
   }

   if (mode != 1002)
   {

      for (i=0;i<frame_size*st->channels;i++)
         pcm[i] = pcm[i] + (opus_val16)((1.0f/32768.0f)*(float)pcm_silk[i]);
   }

   {
            OpusCustomMode *celt_mode;
      opus_custom_decoder_ctl(celt_dec, 10015, ((&celt_mode) + ((&celt_mode) - (      OpusCustomMode**)(&celt_mode))));
      window = &celt_mode->window;
   }

   /* 5 ms redundant frame for SILK->CELT */
   if (redundancy!=0 && celt_to_silk==0)
   {
      opus_custom_decoder_ctl(celt_dec, 4028, 0);
      opus_custom_decoder_ctl(celt_dec, 10010, (opus_int32)(0));

      celt_decode_with_ec(celt_dec, data+len, redundancy_bytes, redundant_audio, F5, null);
      opus_custom_decoder_ctl(celt_dec, 4031, ((&redundant_rng) + ((&redundant_rng) - (opus_uint32*)(&redundant_rng))));
      smooth_fade(pcm+st->channels*(frame_size-F2_5), redundant_audio+st->channels*F2_5,
                  pcm+st->channels*(frame_size-F2_5), F2_5, st->channels, window, st->Fs);
   }
   if (redundancy!=0 && celt_to_silk!=0)
   {
      for (c=0;c<st->channels;c++)
      {
         for (i=0;i<F2_5;i++)
            pcm[st->channels*i+c] = redundant_audio[st->channels*i+c];
      }
      smooth_fade(redundant_audio+st->channels*F2_5, pcm+st->channels*F2_5,
                  pcm+st->channels*F2_5, F2_5, st->channels, window, st->Fs);
   }
   if (transition!=0)
   {
      if (audiosize >= F5)
      {
         for (i=0;i<st->channels*F2_5;i++)
            pcm[i] = pcm_transition[i];
         smooth_fade(pcm_transition+st->channels*F2_5, pcm+st->channels*F2_5,
                     pcm+st->channels*F2_5, F2_5,
                     st->channels, window, st->Fs);
      } else {
         /* Not enough time to do a clean transition, but we do it anyway
            This will not preserve amplitude perfectly and may introduce
            a bit of temporal aliasing, but it shouldn't be too bad and
            that's pretty much the best we can do. In any case, generating this
            transition it pretty silly in the first place */
         smooth_fade(pcm_transition, pcm,
                     pcm, F2_5,
                     st->channels, window, st->Fs);
      }
   }

   if(st->decode_gain!=0)
   {
      opus_val32 gain;
      gain = ((float)exp(0.6931471805599453094D*((((6.48814081e-4f))*(float)(st->decode_gain)))));
      for (i=0;i<frame_size*st->channels;i++)
      {
         opus_val32 x;
         x = ((pcm[i])*(gain));
         pcm[i] = (x);
      }
   }

   if (len <= 1)
      st->rangeFinal = 0;
   else
      st->rangeFinal = dec.rng ^ redundant_rng;

   st->prev_mode = mode;
   st->prev_redundancy = (int)(redundancy!=0 && celt_to_silk==0);
   
	afree((byte*)pcm_silk);
	afree((byte*)redundant_audio);
   afree((byte*)pcm_transition);
   ;
   return celt_ret < 0 ? celt_ret : audiosize;

}

int parse_size(          byte      *data, opus_int32 len, short *size)
{
   if (len<1)
   {
      *size = -1;
      return -1;
   } else if (data[0]<252)
   {
      *size = data[0];
      return 1;
   } else if (len<2)
   {
      *size = -1;
      return -1;
   } else {
      *size = (short)(4*data[1] + data[0]);
      return 2;
   }
}

int opus_packet_parse_impl(          byte      *data, opus_int32 len,
                           int self_delimited,     byte      *out_toc,
                           byte      *frames*, short size*, int *payload_offset)
{
   int i, bytes;
   int count;
   int cbr;
   byte      ch, toc;
   int framesize;
   int last_size;
   byte   *data0 = data;

   if (size==null)
      return -1;

   framesize = opus_packet_get_samples_per_frame(data, 48000);

   cbr = 0;
   *&toc = *(*&data)++;
   (*&len)--;
   last_size = len;
   switch (toc&0x3)
   {
   /* One frame */
   case 0:
      count=1;
      break;
   /* Two CBR frames */
   case 1:
      count=2;
      cbr = 1;
      if (self_delimited==0)
      {
         if ((len&0x1)!=0)
            return -4;
         last_size = len/2;
         size[0] = (short)(last_size);
      }
      break;
   /* Two VBR frames */
   case 2:
      count = 2;
      bytes = parse_size(data, len, size);
      *&len -= bytes;
      if (size[0]<0 || size[0] > len)
         return -4;
      *&data += bytes;
      last_size = len-size[0];
      break;
   /* Multiple CBR/VBR frames (from 0 to 120 ms) */
   default: /*case 3:*/
      if (len<1)
         return -4;
      /* Number of frames encoded in bits 0 to 5 */
      *&ch = *(*&data)++;
      count = (int)(ch&0x3F);
      if (count <= 0 || framesize*count > 5760)
         return -4;
      (*&len)--;
      /* Padding flag is bit 6 */
      if ((ch&0x40)!=0)
      {
         int padding=0;
         int p;
         for(;;)
		 {
            if (len<=0)
               return -4;
            p = *(*&data)++;
            (*&len)--;
            padding += p==255 ? 254: p;
           if (!(p==255)) break;
		 }
         *&len -= padding;
      }
      if (len<0)
         return -4;
      /* VBR flag is bit 7 */
      cbr = (int)!(bool)(ch&0x80);
      if (cbr==0)
      {
         /* VBR case */
         last_size = len;
         for (i=0;i<count-1;i++)
         {
            bytes = parse_size(data, len, size+i);
            *&len -= bytes;
            if (size[i]<0 || size[i] > len)
               return -4;
            *&data += bytes;
            last_size -= bytes+size[i];
         }
         if (last_size<0)
            return -4;
      } else if (self_delimited==0)
      {
         /* CBR case */
         last_size = len/count;
         if (last_size*count!=len)
            return -4;
         for (i=0;i<count-1;i++)
            (*&size)[i] = (short)last_size;
      }
      break;
   }
   /* Self-delimited framing has an extra size for the last frame. */
   if (self_delimited!=0)
   {
      bytes = parse_size(data, len, size+count-1);
      *&len -= bytes;
      if (size[count-1]<0 || size[count-1] > len)
         return -4;
      *&data += bytes;
      /* For CBR packets, apply the size to all the frames. */
      if (cbr!=0)
      {
         if (size[count-1]*count > len)
            return -4;
         for (i=0;i<count-1;i++)
            size[i] = size[count-1];
      } else if(size[count-1] > last_size)
         return -4;
   } else
   {
      /* Because it's not encoded explicitly, it's possible the size of the
         last packet (or all the packets, for the CBR case) is larger than
         1275. Reject them here.*/
      if (last_size > 1275)
         return -4;
      size[count-1] = (short)last_size;
   }

   if (frames!=null)
   {
      for (i=0;i<count;i++)
      {
         frames[i] = data;
         *&data += size[i];
      }
   }

   if (out_toc!=null)
      *out_toc = toc;

   if (payload_offset!=null)
      *payload_offset = (int)(data-data0);

   return count;
}

public
int opus_packet_parse(          byte      *data, opus_int32 len,
          byte      *out_toc,           byte      *frames*,
      short size*, int *payload_offset)
{
   return opus_packet_parse_impl(data, len, 0, out_toc,
                                 frames, size, payload_offset);
}

public
int opus_decode_native(OpusDecoder *st,           byte      *data,
      opus_int32 len, opus_val16 *pcm, int frame_size, int decode_fec,
      int self_delimited, int *packet_offset)
{
   int i, nb_samples;
   int count, offset;
   byte      toc;
   int tot_offset;
   /* 48 x 2.5 ms = 120 ms */
   short size[48];
   if (decode_fec<0 || decode_fec>1)
      return -1;
   if (len==0 || data==null)
      return opus_decode_frame(st, null, 0, pcm, frame_size, 0);
   else if (len<0)
      return -1;

   tot_offset = 0;
   st->mode = opus_packet_get_mode(data);
   st->bandwidth = opus_packet_get_bandwidth(data);
   st->frame_size = opus_packet_get_samples_per_frame(data, st->Fs);
   st->stream_channels = opus_packet_get_nb_channels(data);

   count = opus_packet_parse_impl(data, len, self_delimited, &toc, null, &size, &offset);
   if (count < 0)
      return count;

   *&data += offset;
   tot_offset += offset;

   if (count*st->frame_size > frame_size)
      return -2;
   nb_samples=0;
   for (i=0;i<count;i++)
   {
      int ret;
      ret = opus_decode_frame(st, data, size[i], pcm, frame_size-nb_samples, decode_fec);
      if (ret<0)
         return ret;
      *&data += size[i];
      tot_offset += size[i];
      *&pcm += ret*st->channels;
      nb_samples += ret;
   }
   if (packet_offset != null)
      *packet_offset = tot_offset;
   return nb_samples;
}

public
int opus_decode(OpusDecoder *st,           byte      *data,
      opus_int32 len, opus_int16 *pcm, int frame_size, int decode_fec)
{
   float *_out;
   int ret, i;
   ;

   if(frame_size<0)
   {
      ;
      return -1;
   }

   _out = ((float*)malloc(((int)(float ' size  ))*(frame_size*st->channels)));

   ret = opus_decode_native(st, data, len, _out, frame_size, decode_fec, 0, null);
   if (ret > 0)
   {
      for (i=0;i<ret*st->channels;i++)
         *&pcm[i] = FLOAT2INT16(_out[i]);
   }
   afree((byte*)_out);
   ;
   return ret;
}

public
int opus_decode_float(OpusDecoder *st,           byte      *data,
      opus_int32 len, opus_val16 *pcm, int frame_size, int decode_fec)
{
   return opus_decode_native(st, data, len, pcm, frame_size, decode_fec, 0, null);
}

public
int opus_decoder_ctl(OpusDecoder *st, int request, byte[] value0)
{
   int ret = 0;
   byte *silk_dec;
   OpusCustomDecoder *celt_dec;

   silk_dec = (byte*)st+st->silk_dec_offset;
   celt_dec = (OpusCustomDecoder*)((byte*)st+st->celt_dec_offset);

   switch (request)
   {
   case 4009:
   {
      opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
	  value'byte = value0'byte;
      *value = st->bandwidth;
   }
   break;
   case 4031:
   {
      opus_uint32 *value; // = ( *(opus_uint32* *)((ap += ( (((int)((opus_uint32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_uint32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
	  value'byte = value0'byte;
      *value = st->rangeFinal;
   }
   break;
   case 4028:
   {
      memset(((char*)&st->stream_channels), 0, (((int)(OpusDecoder ' size  ))- (int)((char*)&st->stream_channels - (char*)st))*((int)((*((char*)&st->stream_channels)) ' size  )));

      opus_custom_decoder_ctl(celt_dec, 4028, (int)0);
      silk_InitDecoder( silk_dec );
      st->stream_channels = st->channels;
      st->frame_size = st->Fs/400;
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
   case 4033:
   {
      opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
	  value'byte = value0'byte;
      if (value==null)
      {
         ret = -1;
         break;
      }
      if (st->prev_mode == 1002)
         opus_custom_decoder_ctl(celt_dec, 4033, ((value) + ((value) - (opus_int32*)(value))));
      else
         *value = st->DecControl.prevPitchLag;
   }
   break;
   case 4045:
   {
      opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
	  value'byte = value0'byte;
      if (value==null)
      {
         ret = -1;
         break;
      }
      *value = st->decode_gain;
   }
   break;
   case 4034:
   {
       opus_int32 value; //( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
       value'byte = value0'byte;
       if (value<-32768 || value>32767)
       {
          ret = -1;
          break;
       }
       st->decode_gain = value;
   }
   break;
   default:
      /*fprintf(stderr, "unknown opus_decoder_ctl() request: %d", request);*/
      ret = -5;
      break;
   }

//   ( ap = (va_list)0 );
   return ret;
}

public
void opus_decoder_destroy(OpusDecoder *st)
{
   opus_free((byte*)st);
}

public
int opus_packet_get_bandwidth(          byte      *data)
{
   int bandwidth;
   if ((data[0]&0x80) != 0)
   {
      bandwidth = 1102 + (int)((data[0]>>5)&0x3);
      if (bandwidth == 1102)
         bandwidth = 1101;
   } else if ((data[0]&0x60) == 0x60)
   {
      bandwidth = (data[0]&0x10)!=0 ? 1105 :
                                   1104;
   } else {
      bandwidth = 1101 + (int)((data[0]>>5)&0x3);
   }
   return bandwidth;
}

public
int opus_packet_get_samples_per_frame(          byte      *data,
      opus_int32 Fs)
{
   int audiosize;
   if ((data[0]&0x80)!=0)
   {
      audiosize = (int)((data[0]>>3)&0x3);
      audiosize = (Fs<<audiosize)/400;
   } else if ((data[0]&0x60) == 0x60)
   {
      audiosize = (data[0]&0x08)!=0 ? Fs/50 : Fs/100;
   } else {
      audiosize = (int)((data[0]>>3)&0x3);
      if (audiosize == 3)
         audiosize = Fs*60/1000;
      else
         audiosize = (Fs<<audiosize)/100;
   }
   return audiosize;
}

public
int opus_packet_get_nb_channels(          byte      *data)
{
   return (data[0]&0x4)!=0 ? 2 : 1;
}

public
int opus_packet_get_nb_frames(          byte      packet*, opus_int32 len)
{
   int count;
   if (len<1)
      return -1;
   count = (int)(packet[0]&0x3);
   if (count==0)
      return 1;
   else if (count!=3)
      return 2;
   else if (len<2)
      return -4;
   else
      return (int)(packet[1]&0x3F);
}

public
int opus_decoder_get_nb_samples(      OpusDecoder *dec,
                byte      packet*, opus_int32 len)
{
   int samples;
   int count = opus_packet_get_nb_frames(packet, len);

   if (count<0)
      return count;

   samples = count*opus_packet_get_samples_per_frame(packet, dec->Fs);
   /* Can't have more than 120 ms */
   if (samples*25 > dec->Fs*3)
      return -4;
   else
      return samples;
}
#end unsafe
