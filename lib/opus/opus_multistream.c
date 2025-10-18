#begin unsafe
/* Copyright (c) 2011 Xiph.Org Foundation
   Written by Jean-Marc Valin */
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

//use opus_multistream;
//use opus;
//use opus_private;
//use stack_alloc;
use opus_types;
use arch;
use float_cast;
use os_support;
use opus_encoder, opus_decoder;
use repacketizer;

typedef void opus_copy_channel_in_func(   // removed *
  opus_val16 *dst,
  int dst_stride,
  byte *src,
  int src_stride,
  int src_channel,
  int frame_size
);

typedef void opus_copy_channel_out_func(   // removed *
  byte *dst,
  int dst_stride,
  int dst_channel,
  opus_val16 *src,
  int src_stride,
  int frame_size
);


int validate_layout(ChannelLayout *layout)
{
   int i, max_channel;

   max_channel = layout->nb_streams+layout->nb_coupled_streams;
   if (max_channel>255)
      return 0;
   for (i=0;i<layout->nb_channels;i++)
   {
      if ((int)layout->mapping[i] >= max_channel && layout->mapping[i] != 255)
         return 0;
   }
   return 1;
}

int get_left_channel(      ChannelLayout *layout, int stream_id, int prev)
{
   int i;
   i = (prev<0) ? 0 : prev+1;
   for (;i<layout->nb_channels;i++)
   {
      if ((int)layout->mapping[i]==stream_id*2)
         return i;
   }
   return -1;
}

int get_right_channel(      ChannelLayout *layout, int stream_id, int prev)
{
   int i;
   i = (prev<0) ? 0 : prev+1;
   for (;i<layout->nb_channels;i++)
   {
      if ((int)layout->mapping[i]==stream_id*2+1)
         return i;
   }
   return -1;
}

int get_mono_channel(      ChannelLayout *layout, int stream_id, int prev)
{
   int i;
   i = (prev<0) ? 0 : prev+1;
   for (;i<layout->nb_channels;i++)
   {
      if ((int)layout->mapping[i]==stream_id+layout->nb_coupled_streams)
         return i;
   }
   return -1;
}

int validate_encoder_layout(      ChannelLayout *layout)
{
   int s;
   for (s=0;s<layout->nb_streams;s++)
   {
      if (s < layout->nb_coupled_streams)
      {
         if (get_left_channel(layout, s, -1)==-1)
            return 0;
         if (get_right_channel(layout, s, -1)==-1)
            return 0;
      } else {
         if (get_mono_channel(layout, s, -1)==-1)
            return 0;
      }
   }
   return 1;
}

public
opus_int32 opus_multistream_encoder_get_size(int nb_streams, int nb_coupled_streams)
{
   int coupled_size;
   int mono_size;

   if(nb_streams<1||nb_coupled_streams>nb_streams||nb_coupled_streams<0)return 0;
   coupled_size = opus_encoder_get_size(2);
   mono_size = opus_encoder_get_size(1);
   return align(((int)(OpusMSEncoder ' size  )))
        + nb_coupled_streams * align(coupled_size)
        + (nb_streams-nb_coupled_streams) * align(mono_size);
}

public
int opus_multistream_encoder_init(
      OpusMSEncoder *st,
      opus_int32 Fs,
      int channels,
      int streams,
      int coupled_streams,
                byte      *mapping,
      int application
)
{
   int coupled_size;
   int mono_size;
   int i;
   char *ptr;

   if ((channels>255) || (channels<1) || (coupled_streams>streams) ||
       (coupled_streams+streams>255) || (streams<1) || (coupled_streams<0))
      return -1;

   st->layout.nb_channels = channels;
   st->layout.nb_streams = streams;
   st->layout.nb_coupled_streams = coupled_streams;

   for (i=0;i<st->layout.nb_channels;i++)
      st->layout.mapping[i] = mapping[i];
   if (validate_layout(&st->layout)==0 || validate_encoder_layout(&st->layout)==0)
      return -1;
   ptr = (char*)st + align(((int)(OpusMSEncoder ' size  )));
   coupled_size = opus_encoder_get_size(2);
   mono_size = opus_encoder_get_size(1);

   for (i=0;i<st->layout.nb_coupled_streams;i++)
   {
      opus_encoder_init((OpusEncoder*)ptr, Fs, 2, application);
      ptr += align(coupled_size);
   }
   for (;i<st->layout.nb_streams;i++)
   {
      opus_encoder_init((OpusEncoder*)ptr, Fs, 1, application);
      ptr += align(mono_size);
   }
   return 0;
}

public
OpusMSEncoder *opus_multistream_encoder_create(
      opus_int32 Fs,
      int channels,
      int streams,
      int coupled_streams,
                byte      *mapping,
      int application,
      int *error
)
{
   int ret;
   OpusMSEncoder *st = (OpusMSEncoder *)opus_alloc((uint)opus_multistream_encoder_get_size(streams, coupled_streams));
   if (st==null)
   {
      if (error!=null)
         *error = -7;
      return null;
   }
   ret = opus_multistream_encoder_init(st, Fs, channels, streams, coupled_streams, mapping, application);
   if (ret != 0)
   {
      opus_free((byte*)st);
      *&st = null;
   }
   if (error != null)
      *error = ret;
   return st;
}

/* Max size in case the encoder decides to return three frames */

int opus_multistream_encode_native
(
    OpusMSEncoder *st,
    opus_copy_channel_in_func copy_channel_in,
    byte *pcm,
    int frame_size,
    byte      *data,
    opus_int32 max_data_bytes
)
{
   opus_int32 Fs;
   int coupled_size;
   int mono_size;
   int s;
   char *ptr;
   int tot_size;
   opus_val16 *buf;
   byte tmp_data[(3*1275+7)];
   OpusRepacketizer rp;
   ;

   ptr = (char*)st + align(((int)(OpusMSEncoder ' size  )));
   opus_encoder_ctl((OpusEncoder*)ptr, 4029, ((&Fs) + ((&Fs) - (opus_int32*)(&Fs))));
   /* Validate frame_size before using it to allocate stack space.
      This mirrors the checks in opus_encode[_float](). */
   if (400*frame_size != Fs && 200*frame_size != Fs &&
       100*frame_size != Fs &&  50*frame_size != Fs &&
        25*frame_size != Fs &&  50*frame_size != 3*Fs)
   {
      ;
      return -1;
   }
   buf = ((opus_val16*)malloc(((int)(int)(opus_val16 ' size  ))*(2*frame_size)));
   coupled_size = opus_encoder_get_size(2);
   mono_size = opus_encoder_get_size(1);

   if (max_data_bytes < 4*st->layout.nb_streams-1)
   {
      afree((byte*)buf);
      ;
      return -2;
   }
   /* Counting ToC */
   tot_size = 0;
   for (s=0;s<st->layout.nb_streams;s++)
   {
      OpusEncoder *enc;
      int len;
      int curr_max;

      opus_repacketizer_init(&rp);
      enc = (OpusEncoder*)ptr;
      if (s < st->layout.nb_coupled_streams)
      {
         int left, right;
         left = get_left_channel(&st->layout, s, -1);
         right = get_right_channel(&st->layout, s, -1);
         (copy_channel_in)(buf, 2,
            pcm, st->layout.nb_channels, left, frame_size);
         (copy_channel_in)(buf+1, 2,
            pcm, st->layout.nb_channels, right, frame_size);
         ptr += align(coupled_size);
      } else {
         int chan = get_mono_channel(&st->layout, s, -1);
         (copy_channel_in)(buf, 1,
            pcm, st->layout.nb_channels, chan, frame_size);
         ptr += align(mono_size);
      }
      /* number of bytes left (+Toc) */
      curr_max = max_data_bytes - tot_size;
      /* Reserve three bytes for the last stream and four for the others */
      curr_max -= ((0) > (4*(st->layout.nb_streams-s-1)-1) ? (0) : (4*(st->layout.nb_streams-s-1)-1));
      curr_max = ((curr_max) < ((3*1275+7)) ? (curr_max) : ((3*1275+7)));
      len = opus_encode_float(enc, buf, frame_size, &tmp_data, curr_max);
      if (len<0)
      {
         afree((byte*)buf);
         ;
         return len;
      }
      /* We need to use the repacketizer to add the self-delimiting lengths
         while taking into account the fact that the encoder can now return
         more than one frame at a time (e.g. 60 ms CELT-only) */
      opus_repacketizer_cat(&rp, &tmp_data, len);
      len = opus_repacketizer_out_range_impl(&rp, 0, opus_repacketizer_get_nb_frames(&rp), data, max_data_bytes-tot_size, (int)(s != st->layout.nb_streams-1));
      *&data += len;
      tot_size += len;
   }
   afree((byte*)buf);
   ;
   return tot_size;
}

void opus_copy_channel_in_float(
  opus_val16 *dst,
  int dst_stride,
  byte *src,
  int src_stride,
  int src_channel,
  int frame_size
)
{
   float *float_src;
   int i;
   float_src = (float *)src;
   for (i=0;i<frame_size;i++)
     dst[i*dst_stride] = float_src[i*src_stride+src_channel];
}

void opus_copy_channel_in_short(
  opus_val16 *dst,
  int dst_stride,
  byte* src,
  int src_stride,
  int src_channel,
  int frame_size
)
{
   opus_int16 *short_src;
   int i;
   short_src = (opus_int16 *)src;
   for (i=0;i<frame_size;i++)
     dst[i*dst_stride] = (1.0/32768.0f)*(float)short_src[i*src_stride+src_channel];
}

public
int opus_multistream_encode_float
(
    OpusMSEncoder *st,
    opus_val16 *pcm,
    int frame_size,
    byte      *data,
    opus_int32 max_data_bytes)
{
   return opus_multistream_encode_native(st, opus_copy_channel_in_float, (byte*)pcm, frame_size, data, max_data_bytes);
}

public
int opus_multistream_encode(
    OpusMSEncoder *st,
          opus_int16 *pcm,
    int frame_size,
        byte      *data,
    opus_int32 max_data_bytes)
{
   return opus_multistream_encode_native(st, opus_copy_channel_in_short,  (byte*)pcm, frame_size, data, max_data_bytes);
}

public
int opus_multistream_encoder_ctl(OpusMSEncoder *st, int request, byte[] value0)
{
   int coupled_size, mono_size;
   char *ptr;
   int ret = 0;

//   ( ap = (va_list)&request + ( (((int)((request) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) ) );

   coupled_size = opus_encoder_get_size(2);
   mono_size = opus_encoder_get_size(1);
   ptr = (char*)st + align(((int)(OpusMSEncoder ' size  )));
   switch (request)
   {
   case 4002:
   {
      int chan, s;
      opus_int32 value;  // = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
  
      value'byte = value0'byte;
	  
      chan = st->layout.nb_streams + st->layout.nb_coupled_streams;
      value /= chan;
      for (s=0;s<st->layout.nb_streams;s++)
      {
         OpusEncoder *enc;
         enc = (OpusEncoder*)ptr;
         if (s < st->layout.nb_coupled_streams)
            ptr += align(coupled_size);
         else
            ptr += align(mono_size);
         opus_encoder_ctl(enc, request, value * (s < st->layout.nb_coupled_streams ? 2 : 1));
      }
   }
   break;
   case 4003:
   {
      int s;
      opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
      value'byte = value0'byte;
	  
      *value = 0;
      for (s=0;s<st->layout.nb_streams;s++)
      {
         opus_int32 rate;
         OpusEncoder *enc;
         enc = (OpusEncoder*)ptr;
         if (s < st->layout.nb_coupled_streams)
            ptr += align(coupled_size);
         else
            ptr += align(mono_size);
         opus_encoder_ctl(enc, request, &rate);
         *value += rate;
      }
   }
   break;
   case 4037:
   case 4007:
   case 4001:
   case 4009:
   case 4011:
   case 4015:
   case 4017:
   case 11019:
   case 4021:
   case 4025:
   case 4027:
   case 4029:
   case 4013:
   {
      OpusEncoder *enc;
      /* For int32* GET params, just query the first stream */
      opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
      enc = (OpusEncoder*)ptr;
      value'byte = value0'byte;
      ret = opus_encoder_ctl(enc, request, value);
   }
   break;
   case 4031:
   {
      int s;
      opus_uint32 *value; // = ( *(opus_uint32* *)((ap += ( (((int)((opus_uint32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_uint32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
      opus_uint32 tmp;
      value'byte = value0'byte;
      *value=0;

      for (s=0;s<st->layout.nb_streams;s++)
      {
         OpusEncoder *enc;
         enc = (OpusEncoder*)ptr;
         if (s < st->layout.nb_coupled_streams)
            ptr += align(coupled_size);
         else
            ptr += align(mono_size);
         ret = opus_encoder_ctl(enc, request, &tmp);
         if (ret != 0) break;
         *value ^= tmp;
      }
   }
   break;
   case 4036:
   case 4010:
   case 4006:
   case 4020:
   case 4008:
   case 4024:
   case 4000:
   case 4012:
   case 4014:
   case 4016:
   case 11002:
   {
      int s;
      /* This works for int32 params */
      opus_int32 value; // = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
      value'byte = value0'byte;
      for (s=0;s<st->layout.nb_streams;s++)
      {
         OpusEncoder *enc;

         enc = (OpusEncoder*)ptr;
         if (s < st->layout.nb_coupled_streams)
            ptr += align(coupled_size);
         else
            ptr += align(mono_size);
         ret = opus_encoder_ctl(enc, request, value);
         if (ret != 0)
            break;
      }
   }
   break;
   case 5120:
     abort;  // not implemented
#if 0   
   {
      int s;
      opus_int32 stream_id;
      OpusEncoder **value;
      stream_id = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
      if (stream_id<0 || stream_id >= st->layout.nb_streams)
         ret = -1;
      value = ( *(OpusEncoder** *)((ap += ( (((int)((OpusEncoder**) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((OpusEncoder**) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
      for (s=0;s<stream_id;s++)
      {
         if (s < st->layout.nb_coupled_streams)
            ptr += align(coupled_size);
         else
            ptr += align(mono_size);
      }
      *value = (OpusEncoder*)ptr;
   }
      break;
#endif	  
   default:
      ret = -5;
      break;
   }

//   ( ap = (va_list)0 );
   return ret;
}

public
void opus_multistream_encoder_destroy(OpusMSEncoder *st)
{
    opus_free((byte*)st);
}

/* DECODER */

public
opus_int32 opus_multistream_decoder_get_size(int nb_streams, int nb_coupled_streams)
{
   int coupled_size;
   int mono_size;

   if(nb_streams<1||nb_coupled_streams>nb_streams||nb_coupled_streams<0)return 0;
   coupled_size = opus_decoder_get_size(2);
   mono_size = opus_decoder_get_size(1);
   return align(((int)(OpusMSDecoder ' size  )))
         + nb_coupled_streams * align(coupled_size)
         + (nb_streams-nb_coupled_streams) * align(mono_size);
}

public
int opus_multistream_decoder_init(
      OpusMSDecoder *st,
      opus_int32 Fs,
      int channels,
      int streams,
      int coupled_streams,
                byte      *mapping
)
{
   int coupled_size;
   int mono_size;
   int i, ret;
   char *ptr;

   if ((channels>255) || (channels<1) || (coupled_streams>streams) ||
       (coupled_streams+streams>255) || (streams<1) || (coupled_streams<0))
      return -1;

   st->layout.nb_channels = channels;
   st->layout.nb_streams = streams;
   st->layout.nb_coupled_streams = coupled_streams;

   for (i=0;i<st->layout.nb_channels;i++)
      st->layout.mapping[i] = mapping[i];
   if (validate_layout(&st->layout)==0)
      return -1;

   ptr = (char*)st + align(((int)(OpusMSDecoder ' size  )));
   coupled_size = opus_decoder_get_size(2);
   mono_size = opus_decoder_get_size(1);

   for (i=0;i<st->layout.nb_coupled_streams;i++)
   {
      ret=opus_decoder_init((OpusDecoder*)ptr, Fs, 2);
      if(ret!=0)return ret;
      ptr += align(coupled_size);
   }
   for (;i<st->layout.nb_streams;i++)
   {
      ret=opus_decoder_init((OpusDecoder*)ptr, Fs, 1);
      if(ret!=0)return ret;
      ptr += align(mono_size);
   }
   return 0;
}

public
OpusMSDecoder *opus_multistream_decoder_create(
      opus_int32 Fs,
      int channels,
      int streams,
      int coupled_streams,
                byte      *mapping,
      int *error
)
{
   int ret;
   OpusMSDecoder *st = (OpusMSDecoder *)opus_alloc((uint)opus_multistream_decoder_get_size(streams, coupled_streams));
   if (st==null)
   {
      if (error != null)
         *error = -7;
      return null;
   }
   ret = opus_multistream_decoder_init(st, Fs, channels, streams, coupled_streams, mapping);
   if (error != null)
      *error = ret;
   if (ret != 0)
   {
      opus_free((byte*)st);
      st = null;
   }
   return st;

}

int opus_multistream_decode_native(
      OpusMSDecoder *st,
                byte      *data,
      opus_int32 len,
      byte *pcm,
      opus_copy_channel_out_func copy_channel_out,
      int frame_size,
      int decode_fec
)
{
   opus_int32 Fs;
   int coupled_size;
   int mono_size;
   int s, c;
   char *ptr;
   int do_plc=0;
   opus_val16 *buf;
   ;

   /* Limit frame_size to avoid excessive stack allocations. */
   opus_multistream_decoder_ctl(st, 4029, ((&Fs) + ((&Fs) - (opus_int32*)(&Fs))));
   *&frame_size = ((frame_size) < (Fs/25*3) ? (frame_size) : (Fs/25*3));
   buf = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(2*frame_size)));
   ptr = (char*)st + align(((int)(OpusMSDecoder ' size  )));
   coupled_size = opus_decoder_get_size(2);
   mono_size = opus_decoder_get_size(1);

   if (len==0)
      do_plc = 1;
   if (len < 0)
   {
      afree((byte*)buf);
      ;
      return -1;
   }
   if (do_plc==0 && len < 2*st->layout.nb_streams-1)
   {
      afree((byte*)buf);
      ;
      return -4;
   }
   for (s=0;s<st->layout.nb_streams;s++)
   {
      OpusDecoder *dec;
      int packet_offset, ret;

      dec = (OpusDecoder*)ptr;
      ptr += (s < st->layout.nb_coupled_streams) ? align(coupled_size) : align(mono_size);

      if (do_plc==0 && len<=0)
      {
         afree((byte*)buf);
         ;
         return -4;
      }
      packet_offset = 0;
      ret = opus_decode_native(dec, data, len, buf, frame_size, decode_fec,
                               (int)(s!=st->layout.nb_streams-1), &packet_offset);
      *&data += packet_offset;
      *&len -= packet_offset;
      if (ret > frame_size)
      {
         afree((byte*)buf);
         ;
         return -2;
      }
      if (s>0 && ret != frame_size)
      {
         afree((byte*)buf);
         ;
         return -4;
      }
      if (ret <= 0)
      {
         afree((byte*)buf);
         ;
         return ret;
      }
      *&frame_size = ret;
      if (s < st->layout.nb_coupled_streams)
      {
         int chan, prev;
         prev = -1;
         /* Copy "left" audio to the channel(s) where it belongs */
         for (;;)
		 {
		   chan = get_left_channel(&st->layout, s, prev);
		   if (chan == -1)
             break;
           (copy_channel_out)(pcm, st->layout.nb_channels, chan, buf, 2, frame_size);
           prev = chan;
         }
         prev = -1;
         /* Copy "right" audio to the channel(s) where it belongs */
         for (;;)
		 {
           chan = get_right_channel(&st->layout, s, prev);
		   if (chan == -1)
             break;
           (copy_channel_out)(pcm, st->layout.nb_channels, chan, buf+1, 2, frame_size);
           prev = chan;
         }
      } else {
         int chan, prev;
         prev = -1;
         /* Copy audio to the channel(s) where it belongs */
         for (;;)
		 {
           chan = get_mono_channel(&st->layout, s, prev);
		   if (chan == -1)
             break;
            (copy_channel_out)(pcm, st->layout.nb_channels, chan,
               buf, 1, frame_size);
            prev = chan;
         }
      }
   }
   /* Handle muted channels */
   for (c=0;c<st->layout.nb_channels;c++)
   {
      if (st->layout.mapping[c] == 255)
      {
         (copy_channel_out)(pcm, st->layout.nb_channels, c, null, 0, frame_size);
      }
   }
   afree((byte*)buf);
   ;
   return frame_size;
}

void opus_copy_channel_out_float(
  byte *dst,
  int dst_stride,
  int dst_channel,
        opus_val16 *src,
  int src_stride,
  int frame_size
)
{
   float *float_dst;
   int i;
   float_dst = (float*)dst;
   if (src != null)
   {
      for (i=0;i<frame_size;i++)

         float_dst[i*dst_stride+dst_channel] = src[i*src_stride];
   }
   else
   {
      for (i=0;i<frame_size;i++)
         float_dst[i*dst_stride+dst_channel] = 0.0;
   }
}

void opus_copy_channel_out_short(
  byte *dst,
  int dst_stride,
  int dst_channel,
        opus_val16 *src,
  int src_stride,
  int frame_size
)
{
   opus_int16 *short_dst;
   int i;
   short_dst = (opus_int16*)dst;
   if (src != null)
   {
      for (i=0;i<frame_size;i++)

         short_dst[i*dst_stride+dst_channel] = FLOAT2INT16(src[i*src_stride]);
   }
   else
   {
      for (i=0;i<frame_size;i++)
         short_dst[i*dst_stride+dst_channel] = 0;
   }
}

public
int opus_multistream_decode(OpusMSDecoder *st,           byte      *data,
      opus_int32 len, opus_int16 *pcm, int frame_size, int decode_fec)
{
   return opus_multistream_decode_native(st, data, len,
       (byte*)pcm, opus_copy_channel_out_short, frame_size, decode_fec);
}

public
int opus_multistream_decode_float(
      OpusMSDecoder *st,
                byte      *data,
      opus_int32 len,
      float *pcm,
      int frame_size,
      int decode_fec
)
{
   return opus_multistream_decode_native(st, data, len,
       (byte*)pcm, opus_copy_channel_out_float, frame_size, decode_fec);
}

public
int opus_multistream_decoder_ctl(OpusMSDecoder *st, int request, byte[] value0)
{
//   va_list ap;
   int coupled_size, mono_size;
   char *ptr;
   int ret = 0;

//   ( ap = (va_list)&request + ( (((int)((request) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) ) );

   coupled_size = opus_decoder_get_size(2);
   mono_size = opus_decoder_get_size(1);
   ptr = (char*)st + align(((int)(OpusMSDecoder ' size  )));
   switch (request)
   {
       case 4009:
       case 4029:
       {
          OpusDecoder *dec;
          /* For int32* GET params, just query the first stream */
          opus_int32 *value; // = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
		  value'byte = value0'byte;
          dec = (OpusDecoder*)ptr;
          ret = opus_decoder_ctl(dec, request, value);
       }
       break;
       case 4031:
       {
          int s;
          opus_uint32 *value; // = ( *(opus_uint32* *)((ap += ( (((int)((opus_uint32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_uint32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
          opus_uint32 tmp;
		  value'byte = value0'byte;
          *value = 0;
          for (s=0;s<st->layout.nb_streams;s++)
          {
             OpusDecoder *dec;
             dec = (OpusDecoder*)ptr;
             if (s < st->layout.nb_coupled_streams)
                ptr += align(coupled_size);
             else
                ptr += align(mono_size);
             ret = opus_decoder_ctl(dec, request, &tmp);
             if (ret != 0) break;
             *value ^= tmp;
          }
       }
       break;
       case 4028:
       {
          int s;
          for (s=0;s<st->layout.nb_streams;s++)
          {
             OpusDecoder *dec;

             dec = (OpusDecoder*)ptr;
             if (s < st->layout.nb_coupled_streams)
                ptr += align(coupled_size);
             else
                ptr += align(mono_size);
             ret = opus_decoder_ctl(dec, 4028, 0);
             if (ret != 0)
                break;
          }
       }
       break;
       case 5122:
abort;

#if 0
       {
          int s;
          opus_int32 stream_id;
          OpusDecoder **value;
          stream_id = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
          if (stream_id<0 || stream_id >= st->layout.nb_streams)
             ret = -1;
          value = ( *(OpusDecoder** *)((ap += ( (((int)((OpusDecoder**) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((OpusDecoder**) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
          for (s=0;s<stream_id;s++)
          {
             if (s < st->layout.nb_coupled_streams)
                ptr += align(coupled_size);
             else
                ptr += align(mono_size);
          }
          *value = (OpusDecoder*)ptr;
       }
       break;
#endif
	   
       case 4034:
       {
          int s;
          /* This works for int32 params */
          opus_int32 value; // = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
value'byte = value0'byte;		  
          for (s=0;s<st->layout.nb_streams;s++)
          {
             OpusDecoder *dec;

             dec = (OpusDecoder*)ptr;
             if (s < st->layout.nb_coupled_streams)
                ptr += align(coupled_size);
             else
                ptr += align(mono_size);
             ret = opus_decoder_ctl(dec, request, value);
             if (ret != 0)
                break;
          }
       }
       break;
       default:
          ret = -5;
       break;
   }

//   ( ap = (va_list)0 );
   return ret;
}

public
void opus_multistream_decoder_destroy(OpusMSDecoder *st)
{
    opus_free((byte*)st);
}
#end unsafe
