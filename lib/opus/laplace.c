#begin unsafe
/* Copyright (c) 2007 CSIRO
   Copyright (c) 2007-2009 Xiph.Org Foundation
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
use entcode, entenc, entdec;

/* The minimum probability of an energy delta (out of 32768). */

/* The minimum number of guaranteed representable energy deltas (in one
    direction). */

/* When called, decay is positive and at most 11456. */
unsigned ec_laplace_get_freq1(unsigned fs0, int decay)
{
   unsigned ft;
   ft = 32768 - (1<<(0))*(2*(16)) - fs0;
   return ft*(uint)(opus_int32)(16384-decay)>>15;
}

public void ec_laplace_encode(ec_enc *enc, int *value, unsigned fs, int decay)
{
   unsigned fl;
   int val = *value;
   fl = 0;
   if (val != 0)
   {
      int s;
      int i;
      s = -(int)(val<0);
      val = (val+s)^s;
      fl = fs;
      *&fs = ec_laplace_get_freq1(fs, decay);
      /* Search the decaying part of the PDF.*/
      for (i=1; fs > 0 && i < val; i++)
      {
         *&fs *= 2;
         fl += fs+2*(1<<(0));
         *&fs = (fs*(uint)(opus_int32)decay)  >>  15;
	  }
	  
      /* Everything beyond that has probability LAPLACE_MINP. */
      if (fs == 0)
      {
         int di;
         int ndi_max;
         ndi_max = (int)((32768-fl+(1<<(0))-1)>>(0));
         ndi_max = (ndi_max-s)>>1;
         di = ((val - i) < (ndi_max - 1) ? (val - i) : (ndi_max - 1));
         fl += (uint)((2*di+1+s)*(1<<(0)));
         *&fs = (((1<<(0))) < (32768-fl) ? ((1<<(0))) : (32768-fl));
         *value = (i+di+s)^s;
      }
      else
      {
         *&fs += (1<<(0));
         fl += (fs & (uint)~s);
      }
      ;
      ;
   }
   ec_encode_bin(enc, fl, fl+fs, 15);
}

public int ec_laplace_decode(ec_dec *dec, unsigned fs, int decay)
{
   int val=0;
   unsigned fl;
   unsigned fm;
   fm = ec_decode_bin(dec, 15);
   fl = 0;
   if (fm >= fs)
   {
      val++;
      fl = fs;
      *&fs = ec_laplace_get_freq1(fs, decay)+(1<<(0));
      /* Search the decaying part of the PDF.*/
      while(fs > (1<<(0)) && fm >= fl+2*fs)
      {
         *&fs *= 2;
         fl += fs;
         *&fs = ((fs-2*(1<<(0)))*(uint)(opus_int32)decay)>>15;
         *&fs += (1<<(0));
         val++;
      }
      /* Everything beyond that has probability LAPLACE_MINP. */
      if (fs <= (1<<(0)))
      {
         int di;
         di = (int)((fm-fl)>>((0)+1));
         val += di;
         fl += (uint)(2*di*(1<<(0)));
      }
      if (fm < fl+fs)
         val = -val;
      else
         fl += fs;
   }
   ;
   ;
   ;
   ;
   ec_dec_update(dec, fl, ((fl+fs) < (32768) ? (fl+fs) : (32768)), 32768);
   return val;
}
#end unsafe
