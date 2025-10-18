#begin unsafe
/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2008 Xiph.Org Foundation
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

/* This is a simple MDCT implementation that uses a N/4 complex FFT
   to do most of the work. It should be relatively straightforward to
   plug in pretty much and FFT here.

   This replaces the Vorbis FFT (and uses the exact same API), which
   was a bit too messy and that was ending up duplicating code
   (might as well use the same FFT everywhere).

   The algorithm is similar to (and inspired from) Fabrice Bellard's
   MDCT implementation in FFMPEG, but has differences in signs, ordering
   and scaling in many places.
*/

use kiss_fft;
use arch, os_support;

/* Forward MDCT trashes the input array */
public void clt_mdct_forward(      mdct_lookup *l, float *in, float *  _out,
            opus_val16 *window, int overlap, int shift, int stride)
{
   int i;
   int N, N2, N4;
   float sine;
   float *f;
   ;
   N = l->n;
   N >>= shift;
   N2 = N>>1;
   N4 = N>>2;
   f = ((float*)malloc(((int)(float ' size  ))*(N2)));
   /* sin(x) ~= x here */

   sine = (float)2.0*3.141592653f*(0.125f)/(float)N;

   /* Consider the input to be composed of four blocks: [a, b, c, d] */
   /* Window, shuffle, fold */
   {
      /* Temp pointers to make it really clear to the compiler what we're doing */
      float *  xp1 = in+(overlap>>1);
      float *  xp2 = in+N2-1+(overlap>>1);
      float *  yp = f;
      opus_val16 *  wp1 = window+(overlap>>1);
      opus_val16 *  wp2 = window+(overlap>>1)-1;
      for(i=0;i<(overlap>>2);i++)
      {
         /* Real part arranged as -d-cR, Imag part arranged as -b+aR*/
         *yp++ = ((*wp2)*(xp1[N2])) + ((*wp1)*(*xp2));
         *yp++ = ((*wp1)*(*xp1))    - ((*wp2)*(xp2[-N2]));
         xp1+=2;
         xp2-=2;
         wp1+=2;
         wp2-=2;
      }
      wp1 = window;
      wp2 = window+overlap-1;
      for(;i<N4-(overlap>>2);i++)
      {
         /* Real part arranged as a-bR, Imag part arranged as -c-dR */
         *yp++ = *xp2;
         *yp++ = *xp1;
         xp1+=2;
         xp2-=2;
      }
      for(;i<N4;i++)
      {
         /* Real part arranged as a-bR, Imag part arranged as -c-dR */
         *yp++ =  -((*wp1)*(xp1[-N2])) + ((*wp2)*(*xp2));
         *yp++ = ((*wp2)*(*xp1))     + ((*wp1)*(xp2[N2]));
         xp1+=2;
         xp2-=2;
         wp1+=2;
         wp2-=2;
      }
   }
   /* Pre-rotation */
   {
      float *  yp = f;
      float *t = &l->trig[0];
      for(i=0;i<N4;i++)
      {
         float re, im, yr, yi;
         re = yp[0];
         im = yp[1];
         yr = -( (re)*(t[i<<shift]) )  -  ( (im)*(t[(N4-i)<<shift]) );
         yi = -( (im)*(t[i<<shift]) )  +  ( (re)*(t[(N4-i)<<shift]) );
         /* works because the cos is nearly one */
         *yp++ = yr + ( (yi)*(sine) );
         *yp++ = yi - ( (yr)*(sine) );
      }
   }

   /* N/4 complex FFT, down-scales by 4/N */
   opus_fft(&l->kfft[shift], (kiss_fft_cpx *)f, (kiss_fft_cpx *)in);

   /* Post-rotate */
   {
      /* Temp pointers to make it really clear to the compiler what we're doing */
      float *  fp = in;
      float *  yp1 = _out;
      float *  yp2 = _out+stride*(N2-1);
      float *t = &l->trig[0];
      /* Temp pointers to make it really clear to the compiler what we're doing */
      for(i=0;i<N4;i++)
      {
         float yr, yi;
         yr = ( (fp[1])*(t[(N4-i)<<shift]) ) + ( (fp[0])*(t[i<<shift]) );
         yi = ( (fp[0])*(t[(N4-i)<<shift]) ) - ( (fp[1])*(t[i<<shift]) );
         /* works because the cos is nearly one */
         *yp1 = yr - ( (yi)*(sine) );
         *yp2 = yi + ( (yr)*(sine) );;
         fp += 2;
         yp1 += 2*stride;
         yp2 -= 2*stride;
      }
   }
   afree((byte*)f);
   ;
}

public void clt_mdct_backward(      mdct_lookup *l, float *in, float *  _out,
            opus_val16 *  window, int overlap, int shift, int stride)
{
   int i;
   int N, N2, N4;
   float sine;
   float *f;
   float *f2;
   ;
   N = l->n;
   N >>= shift;
   N2 = N>>1;
   N4 = N>>2;
   f = ((float*)malloc(((int)(float ' size  ))*(N2)));
   f2 = ((float*)malloc(((int)(float ' size  ))*(N2)));
   /* sin(x) ~= x here */

   sine = (float)2.0*3.141592653f*(0.125f)/(float)N;

   /* Pre-rotate */
   {
      /* Temp pointers to make it really clear to the compiler what we're doing */
      float *  xp1 = in;
      float *  xp2 = in+stride*(N2-1);
      float *  yp = f2;
      float *t = &l->trig[0];
      for(i=0;i<N4;i++)
      {
         float yr, yi;
         yr = -( (*xp2)*(t[i<<shift]) ) + ( (*xp1)*(t[(N4-i)<<shift]) );
         yi =  -( (*xp2)*(t[(N4-i)<<shift]) ) - ( (*xp1)*(t[i<<shift]) );
         /* works because the cos is nearly one */
         *yp++ = yr - ( (yi)*(sine) );
         *yp++ = yi + ( (yr)*(sine) );
         xp1+=2*stride;
         xp2-=2*stride;
      }
   }

   /* Inverse N/4 complex FFT. This one should *not* downscale even in fixed-point */
   opus_ifft(&l->kfft[shift], (kiss_fft_cpx *)f2, (kiss_fft_cpx *)f);

   /* Post-rotate */
   {
      float *  fp = f;
      float *t = &l->trig[0];

      for(i=0;i<N4;i++)
      {
         float re, im, yr, yi;
         re = fp[0];
         im = fp[1];
         /* We'd scale up by 2 here, but instead it's done when mixing the windows */
         yr = ( (re)*(t[i<<shift]) ) - ( (im)*(t[(N4-i)<<shift]) );
         yi = ( (im)*(t[i<<shift]) ) + ( (re)*(t[(N4-i)<<shift]) );
         /* works because the cos is nearly one */
         *fp++ = yr - ( (yi)*(sine) );
         *fp++ = yi + ( (yr)*(sine) );
      }
   }
   /* De-shuffle the components for the middle of the window only */
   {
      float *  fp1 = f;
      float *  fp2 = f+N2-1;
      float *  yp = f2;
      for(i = 0; i < N4; i++)
      {
         *yp++ =-*fp1;
         *yp++ = *fp2;
         fp1 += 2;
         fp2 -= 2;
      }
   }
   *&_out -= (N2-overlap)>>1;
   /* Mirror on both sides for TDAC */
   {
      float *  fp1 = f2+N4-1;
      float *  xp1 = _out+N2-1;
      float *  yp1 = _out+N4-overlap/2;
      opus_val16 *  wp1 = window;
      opus_val16 *  wp2 = window+overlap-1;
      for(i = 0; i< N4-overlap/2; i++)
      {
         *xp1 = *fp1;
         xp1--;
         fp1--;
      }
      for(; i < N4; i++)
      {
         float x1;
         x1 = *fp1--;
         *yp1++ +=-((*wp1)*(x1));
         *xp1-- += ((*wp2)*(x1));
         wp1++;
         wp2--;
      }
   }
   {
      float *  fp2 = f2+N4;
      float *  xp2 = _out+N2;
      float *  yp2 = _out+N-1-(N4-overlap/2);
      opus_val16 *  wp1 = window;
      opus_val16 *  wp2 = window+overlap-1;
      for(i = 0; i< N4-overlap/2; i++)
      {
         *xp2 = *fp2;
         xp2++;
         fp2++;
      }
      for(; i < N4; i++)
      {
         float x2;
         x2 = *fp2++;
         *yp2--  = ((*wp1)*(x2));
         *xp2++  = ((*wp2)*(x2));
         wp1++;
         wp2--;
      }
   }
   afree((byte*)f);
   afree((byte*)f2);
   ;
}
#end unsafe
