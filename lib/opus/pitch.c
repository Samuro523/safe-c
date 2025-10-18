#begin unsafe
/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2009 Xiph.Org Foundation
   Written by Jean-Marc Valin */
/**
   @file pitch.c
   @brief Pitch analysis
 */

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

use ../arithm, ../math;

use arch, os_support;
use celt_lpc;

void find_best_pitch(opus_val32 *xcorr, opus_val16 *y, int len,
                            int max_pitch, int *best_pitch

                            )
{
   int i, j;
   opus_val32 Syy=1.0;
   opus_val16 best_num[2];
   opus_val32 best_den[2];

   clear best_num, best_den;
   best_num[0] = -1.0;
   best_num[1] = -1.0;
   best_den[0] = 0.0;
   best_den[1] = 0.0;
   best_pitch[0] = 0;
   best_pitch[1] = 1;
   for (j=0;j<len;j++)
      Syy = ((Syy)+((((opus_val32)(y[j])*(opus_val32)(y[j])))));
   for (i=0;i<max_pitch;i++)
   {
      if (xcorr[i]>0.0)
      {
         opus_val16 num;
         opus_val32 xcorr16;
         xcorr16 = ((xcorr[i]));

         /* Considering the range of xcorr16, this should avoid both underflows
            and overflows (inf) when squaring xcorr16 */
         xcorr16 *= 1.0e-12;
         num = ((xcorr16)*(xcorr16));
         if (((num)*(best_den[1])) > ((best_num[1])*(Syy)))
         {
            if (((num)*(best_den[0])) > ((best_num[0])*(Syy)))
            {
               best_num[1] = best_num[0];
               best_den[1] = best_den[0];
               best_pitch[1] = best_pitch[0];
               best_num[0] = num;
               best_den[0] = Syy;
               best_pitch[0] = i;
            } else {
               best_num[1] = num;
               best_den[1] = Syy;
               best_pitch[1] = i;
            }
         }
      }
      Syy += (((opus_val32)(y[i+len])*(opus_val32)(y[i+len]))) - (((opus_val32)(y[i])*(opus_val32)(y[i])));
      Syy = ((1.0) > (Syy) ? (1.0) : (Syy));
   }
}

public void pitch_downsample(celt_sig *  x[], opus_val16 *  x_lp,
      int len, int C)
{
   int i;
   opus_val32 ac[5];
   opus_val16 tmp=1.0f;
   opus_val16 lpc[4], mem[4];

   clear mem;
   for (i=1;i<len>>1;i++)
      x_lp[i] = ((0.5f*((0.5f*(x[0][(2*i-1)]+x[0][(2*i+1)]))+x[0][2*i])));
   x_lp[0] = ((0.5f*((0.5f*(x[0][1]))+x[0][0])));
   if (C==2)
   {
      for (i=1;i<len>>1;i++)
         x_lp[i] += ((0.5f*((0.5f*(x[1][(2*i-1)]+x[1][(2*i+1)]))+x[1][2*i])));
      x_lp[0] += ((0.5f*((0.5f*(x[1][1]))+x[1][0])));
   }

   _celt_autocorr(x_lp, &ac, null, 0,
                  4, len>>1);

   /* Noise floor -40 dB */

   ac[0] *= 1.0001f;
   /* Lag windowing */
   for (i=1;i<=4;i++)
   {
      /*ac[i] *= exp(-.5*(2*M_PI*.002*i)*(2*M_PI*.002*i));*/

      ac[i] -= ac[i]*(0.008f*(float)i)*(0.008f*(float)i);
   }

   _celt_lpc(&lpc, &ac, 4);
   for (i=0;i<4;i++)
   {
      tmp = (((0.9f))*(tmp));
      lpc[i] = ((lpc[i])*(tmp));
   }
   celt_fir(x_lp, &lpc, x_lp, len>>1, 4, &mem);

   mem[0]=0.0;
   lpc[0]=(0.8f);
   celt_fir(x_lp, &lpc, x_lp, len>>1, 1, &mem);

}

public
void pitch_search(      opus_val16 *  x_lp, opus_val16 *  y,
                  int len, int max_pitch, int *pitch)
{
   int i, j;
   int lag;
   int best_pitch[2]={0,0};
   opus_val16 *x_lp4;
   opus_val16 *y_lp4;
   opus_val32 *xcorr;

   int offset;

   ;

   ;
   ;
   lag = len+max_pitch;

   x_lp4 = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(len>>2)));
   y_lp4 = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(lag>>2)));
   xcorr = ((opus_val32*)malloc(((int)(opus_val32 ' size  ))*(max_pitch>>1)));

   /* Downsample by 2 again */
   for (j=0;j<len>>2;j++)
      x_lp4[j] = x_lp[2*j];
   for (j=0;j<lag>>2;j++)
      y_lp4[j] = y[2*j];

   /* Coarse search with 4x decimation */

   for (i=0;i<max_pitch>>2;i++)
   {
      opus_val32 sum = 0.0;
      for (j=0;j<len>>2;j++)
         sum = ((sum)+(opus_val32)(x_lp4[j])*(opus_val32)(y_lp4[i+j]));
      xcorr[i] = ((-1.0) > (sum) ? (-1.0) : (sum));

   }
   find_best_pitch(xcorr, y_lp4, len>>2, max_pitch>>2, &best_pitch

                   );

   /* Finer search with 2x decimation */

   for (i=0;i<max_pitch>>1;i++)
   {
      opus_val32 sum=0.0;
      xcorr[i] = 0.0;
      if (abs(i-2*best_pitch[0])>2 && abs(i-2*best_pitch[1])>2)
         continue;
      for (j=0;j<len>>1;j++)
         sum += (((opus_val32)(x_lp[j])*(opus_val32)(y[i+j])));
      xcorr[i] = ((-1.0) > (sum) ? (-1.0) : (sum));

   }
   find_best_pitch(xcorr, y, len>>1, max_pitch>>1, &best_pitch

                   );

   /* Refine by pseudo-interpolation */
   if (best_pitch[0]>0 && best_pitch[0]<(max_pitch>>1)-1)
   {
      opus_val32 a, b, c;
      a = xcorr[best_pitch[0]-1];
      b = xcorr[best_pitch[0]];
      c = xcorr[best_pitch[0]+1];
      if ((c-a) > (((0.7f))*(b-a)))
         offset = 1;
      else if ((a-c) > (((0.7f))*(b-c)))
         offset = -1;
      else
         offset = 0;
   } else {
      offset = 0;
   }
   *pitch = 2*best_pitch[0]-offset;

   afree((byte*)x_lp4);
   afree((byte*)y_lp4);
   afree((byte*)xcorr);
   ;
}


package CTES
  const int second_check[16] = {0, 0, 3, 2, 3, 2, 5, 2, 3, 2, 3, 2, 5, 2, 3, 2};
end CTES;

public opus_val16 remove_doubling(opus_val16 *x, int maxperiod, int minperiod,
      int N, int *T0_, int prev_period, opus_val16 prev_gain)
{
   int k, i, T, T0;
   opus_val16 g, g0;
   opus_val16 pg;
   opus_val32 xy,xx,yy;
   opus_val32 xcorr[3];
   opus_val32 best_xy, best_yy;
   int offset;
   int minperiod0;

   minperiod0 = minperiod;
   *&maxperiod /= 2;
   *&minperiod /= 2;
   *T0_ /= 2;
   *&prev_period /= 2;
   *&N /= 2;
   *&x += maxperiod;
   if (*T0_>=maxperiod)
      *T0_=maxperiod-1;

   T0 = *T0_;
   T = T0;
   xx=0.0; xy=0.0; yy=0.0;
   for (i=0;i<N;i++)
   {
      xy = ((xy)+(opus_val32)(x[i])*(opus_val32)(x[i-T0]));
      xx = ((xx)+(opus_val32)(x[i])*(opus_val32)(x[i]));
      yy = ((yy)+(opus_val32)(x[i-T0])*(opus_val32)(x[i-T0]));
   }
   best_xy = xy;
   best_yy = yy;

      g0 = xy/((float)sqrt(1.0+xx*yy));
	  g = g0;
   /* Look for any pitch at T/k */
   for (k=2;k<=15;k++)
   {
      int T1, T1b;
      opus_val16 g1;
      opus_val16 cont=0.0;
      T1 = (2*T0+k)/(2*k);
      if (T1 < minperiod)
         break;
      /* Look for another strong correlation at T1b */
      if (k==2)
      {
         if (T1+T0>maxperiod)
            T1b = T0;
         else
            T1b = T0+T1;
      } else
      {
         T1b = (2*second_check[k]*T0+k)/(2*k);
      }
      xy=0.0; yy=0.0;
      for (i=0;i<N;i++)
      {
         xy = ((xy)+(opus_val32)(x[i])*(opus_val32)(x[i-T1]));
         yy = ((yy)+(opus_val32)(x[i-T1])*(opus_val32)(x[i-T1]));

         xy = ((xy)+(opus_val32)(x[i])*(opus_val32)(x[i-T1b]));
         yy = ((yy)+(opus_val32)(x[i-T1b])*(opus_val32)(x[i-T1b]));
      }

      g1 = xy/((float)sqrt(1.0+2.0f*xx*1.0f*yy));
      if (abs(T1-prev_period)<=1)
         cont = prev_gain;
      else if (abs(T1-prev_period)<=2 && 5*k*k < T0)
         cont = (0.5f*(prev_gain));
      else
         cont = 0.0;
      if (g1 > (0.3f) + (((0.4f))*(g0))-cont)
      {
         best_xy = xy;
         best_yy = yy;
         T = T1;
         g = g1;
      }
   }
   best_xy = ((0.0) > (best_xy) ? (0.0) : (best_xy));
   if (best_yy <= best_xy)
      pg = 1.0f;
   else
      pg = (((float)(best_xy)/(best_yy+1.0)));

   clear xcorr;
   for (k=0;k<3;k++)
   {
      int T1 = T+k-1;
      xy = 0.0;
      for (i=0;i<N;i++)
         xy = ((xy)+(opus_val32)(x[i])*(opus_val32)(x[i-T1]));
      xcorr[k] = xy;
   }
   if ((xcorr[2]-xcorr[0]) > (((0.7f))*(xcorr[1]-xcorr[0])))
      offset = 1;
   else if ((xcorr[0]-xcorr[2]) > (((0.7f))*(xcorr[1]-xcorr[2])))
      offset = -1;
   else
      offset = 0;
   if (pg > g)
      pg = g;
   *T0_ = 2*T+offset;

   if (*T0_<minperiod0)
      *T0_=minperiod0;
   return pg;
}
#end unsafe
