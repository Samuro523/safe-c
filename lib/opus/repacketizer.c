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

use opus_types;
use os_support;
use opus_decoder;


public int align(int i)
{
  byte *p;
    return (i+((int)(p ' size  ))-1)&-((int)((int)(p ' size  )));
}



public int opus_repacketizer_get_size()
{
   return ((int)(OpusRepacketizer ' size  ));
}

public  OpusRepacketizer *opus_repacketizer_init(OpusRepacketizer *rp)
{
   rp->nb_frames = 0;
   return rp;
}

public OpusRepacketizer *opus_repacketizer_create()
{
   OpusRepacketizer *rp;
   rp=(OpusRepacketizer *)opus_alloc((uint)opus_repacketizer_get_size());
   if(rp==null)return null;
   return opus_repacketizer_init(rp);
}

public void opus_repacketizer_destroy(OpusRepacketizer *rp)
{
   opus_free((byte*)rp);
}

public int opus_repacketizer_cat(OpusRepacketizer *rp,           byte      *data, opus_int32 len)
{
       byte      tmp_toc;
   int curr_nb_frames,ret;
   /* Set of check ToC */
   if (len<1) return -4;
   if (rp->nb_frames == 0)
   {
      rp->toc = data[0];
      rp->framesize = opus_packet_get_samples_per_frame(data, 8000);
   } else if ((rp->toc&0xFC) != (data[0]&0xFC))
   {
      /*fprintf(stderr, "toc mismatch: 0x%x vs 0x%x\n", rp->toc, data[0]);*/
      return -4;
   }
   curr_nb_frames = opus_packet_get_nb_frames(data, len);
   if(curr_nb_frames<1) return -4;

   /* Check the 120 ms maximum packet size */
   if ((curr_nb_frames+rp->nb_frames)*rp->framesize > 960)
   {
      return -4;
   }

   ret=opus_packet_parse(data, len, &tmp_toc, &rp->frames[rp->nb_frames], &rp->len[rp->nb_frames], null);
   if(ret<1)return ret;

   rp->nb_frames += curr_nb_frames;
   return 0;
}

public int opus_repacketizer_get_nb_frames(OpusRepacketizer *rp)
{
   return rp->nb_frames;
}

public 
opus_int32 opus_repacketizer_out_range_impl(OpusRepacketizer *rp, int begin, int _end,     byte      *data, opus_int32 maxlen, int self_delimited)
{
   int i, count;
   opus_int32 tot_size;
   short *len;
             byte      **frames;

   if (begin<0 || begin>=_end || _end>rp->nb_frames)
   {
      /*fprintf(stderr, "%d %d %d\n", begin, end, rp->nb_frames);*/
      return -1;
   }
   count = _end-begin;

   len = &rp->len+begin;
   frames = &rp->frames+begin;
   if (self_delimited!=0)
      tot_size = 1 + (int)(len[count-1]>=252);
   else
      tot_size = 0;

   switch (count)
   {
   case 1:
   {
      /* Code 0 */
      tot_size += len[0]+1;
      if (tot_size > maxlen)
         return -2;
      *(*&data)++ = (byte)(rp->toc&0xFC);
   }
   break;
   case 2:
   {
      if (len[1] == len[0])
      {
         /* Code 1 */
         tot_size += 2*len[0]+1;
         if (tot_size > maxlen)
            return -2;
         *(*&data)++ = (byte)((rp->toc&0xFC) | 0x1);
      } else {
         /* Code 2 */
         tot_size += len[0]+len[1]+2+(int)(len[0]>=252);
         if (tot_size > maxlen)
            return -2;
         *(*&data)++ = (byte)((rp->toc&0xFC) | 0x2);
         *&data += encode_size(len[0], data);
      }
   }
   break;
   default:
   {
      /* Code 3 */
      int vbr;

      vbr = 0;
      for (i=1;i<count;i++)
      {
         if (len[i] != len[0])
         {
            vbr=1;
            break;
         }
      }
      if (vbr!=0)
      {
         tot_size += 2;
         for (i=0;i<count-1;i++)
            tot_size += 1 + (int)(len[i]>=252) + len[i];
         tot_size += len[count-1];

         if (tot_size > maxlen)
            return -2;
         *(*&data)++ = (byte)((rp->toc&0xFC) | 0x3);
         *(*&data)++ = (byte)(count | 0x80);
         for (i=0;i<count-1;i++)
            *&data += encode_size(len[i], data);
      } else {
         tot_size += count*len[0]+2;
         if (tot_size > maxlen)
            return -2;
         *(*&data)++ = (byte)((rp->toc&0xFC) | 0x3);
         *(*&data)++ = (byte)count;
      }
   }
   break;
   }
   if (self_delimited!=0) {
      int sdlen = encode_size(len[count-1], data);
      *&data += sdlen;
   }
   /* Copy the actual data */
   for (i=0;i<count;i++)
   {
      memcpy((data), (frames[i]), (len[i])*((int)((*(data)) ' size  )) + 0*(int)((data)-(frames[i])) );
      *&data += len[i];
   }
   return tot_size;
}

public opus_int32 opus_repacketizer_out_range(OpusRepacketizer *rp, int begin, int _end,     byte      *data, opus_int32 maxlen)
{
   return opus_repacketizer_out_range_impl(rp, begin, _end, data, maxlen, 0);
}

public opus_int32 opus_repacketizer_out(OpusRepacketizer *rp,     byte      *data, opus_int32 maxlen)
{
   return opus_repacketizer_out_range_impl(rp, 0, rp->nb_frames, data, maxlen, 0);
}

public
int encode_size(int size, byte *data)
{
   if (size < 252)
   {
      data[0] = (byte)size;
      return 1;
   } else {
      data[0] = (byte)(252+(size&0x3));
      data[1] = (byte)((size-(int)data[0])>>2);
      return 2;
   }
}


#end unsafe
