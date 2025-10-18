#begin unsafe
/* Copyright (c) 2007-2008 CSIRO
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

use ../math;

use opus_types;
use cwrs;
use arch;
use os_support;
use entcode;

void exp_rotation1(celt_norm *X, int len, int stride, opus_val16 c, opus_val16 s)
{
   int i;
   celt_norm *Xptr;
   Xptr = X;
   for (i=0;i<len-stride;i++)
   {
      celt_norm x1, x2;
      x1 = Xptr[0];
      x2 = Xptr[stride];
      Xptr[stride] = ((((opus_val32)(c)*(opus_val32)(x2)) + ((opus_val32)(s)*(opus_val32)(x1))));
      *Xptr++      = ((((opus_val32)(c)*(opus_val32)(x1)) - ((opus_val32)(s)*(opus_val32)(x2))));
   }
   Xptr = &X[len-2*stride-1];
   for (i=len-2*stride-1;i>=0;i--)
   {
      celt_norm x1, x2;
      x1 = Xptr[0];
      x2 = Xptr[stride];
      Xptr[stride] = ((((opus_val32)(c)*(opus_val32)(x2)) + ((opus_val32)(s)*(opus_val32)(x1))));
      *Xptr--      = ((((opus_val32)(c)*(opus_val32)(x1)) - ((opus_val32)(s)*(opus_val32)(x2))));
   }
}

public
void exp_rotation(celt_norm *X, int len, int dir, int stride, int K, int spread)
{
   const        int SPREAD_FACTOR[3]={15,10,5};
   int i;
   opus_val16 c, s;
   opus_val16 gain, theta;
   int stride2=0;
   int factor;

   if (2*K>=len || spread==(0))
      return;
   factor = SPREAD_FACTOR[spread-1];

   gain = (((opus_val32)((opus_val32)(((opus_val16)1.0f))*(opus_val32)(len)))/((opus_val32)(len+factor*K)));
   theta = (0.5f*(((gain)*(gain))));

   c = ((float)cos((0.5f*3.141592653f)*((theta))));
   s = ((float)cos((0.5f*3.141592653f)*((((1.0f)-(theta)))))); /*  sin(theta) */

   if (len>=8*stride)
   {
      stride2 = 1;
      /* This is just a simple (equivalent) way of computing sqrt(len/stride) with rounding.
         It's basically incrementing long as (stride2+0.5)^2 < len/stride. */
      while ((stride2*stride2+stride2)*stride + (stride>>2) < len)
         stride2++;
   }
   /*NOTE: As a minor optimization, we could be passing around log2(B), not B, for both this and for
      extract_collapse_mask().*/
   *&len /= stride;
   for (i=0;i<stride;i++)
   {
      if (dir < 0)
      {
         if (stride2 != 0)
            exp_rotation1(X+i*len, len, stride2, s, c);
         exp_rotation1(X+i*len, len, 1, c, s);
      } else {
         exp_rotation1(X+i*len, len, 1, c, -s);
         if (stride2 != 0)
            exp_rotation1(X+i*len, len, stride2, s, -c);
      }
   }
}

/** Takes the pitch vector and the decoded residual vector, computes the gain
    that will give ||p+g*y||=1 and mixes the residual with the pitch. */
void normalise_residual(int *  iy, celt_norm *  X,
      int N, opus_val32 Ryy, opus_val16 gain)
{
   int i;

   opus_val32 t;
   opus_val16 g;

   t = (Ryy);
   g = ((((1.0f/((float)sqrt(t)))))*(gain));

   i=0;
   for (;;)
   {
      X[i] = ((((opus_val32)(g)*(opus_val32)(iy[i]))));
     if (!(++i < N)) break;
   }
}

unsigned extract_collapse_mask(int *iy, int N, int B)
{
   unsigned collapse_mask;
   int N0;
   int i;
   if (B<=1)
      return 1;
   /*NOTE: As a minor optimization, we could be passing around log2(B), not B, for both this and for
      exp_rotation().*/
   N0 = N/B;
   collapse_mask = 0;
   i=0;
   for (;;)
   {
      int j;
      j=0;
	  for (;;)
	  {
         collapse_mask |= (uint)(((int)(iy[i*N0+j]!=0))<<i);
        if (!(++j<N0)) break;
	  }
     if (!(++i<B)) break;
   }
   return collapse_mask;
}

public unsigned alg_quant(celt_norm *X, int N, int K, int spread, int B, ec_enc *enc)
{
   celt_norm *y;
   int *iy;
   opus_val16 *signx;
   int i, j;
   opus_val16 s;
   int pulsesLeft;
   opus_val32 sum;
   opus_val32 xy;
   opus_val16 yy;
   unsigned collapse_mask;
   ;

   ;
   ;

   y = ((celt_norm*)malloc(((int)(celt_norm ' size  ))*(N)));
   iy = ((int*)malloc(((int)(int ' size  ))*(N)));
   signx = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(N)));

   exp_rotation(X, N, 1, B, K, spread);

   /* Get rid of the sign */
   sum = 0.0;
   j=0;
   for (;;)
   {
      if (X[j]>0.0)
         signx[j]=1.0;
      else {
         signx[j]=-1.0;
         X[j]=-X[j];
      }
      iy[j] = 0;
      y[j] = 0.0;
     if (!(++j<N)) break;
   }

   xy = 0.0; yy = 0.0;

   pulsesLeft = K;

   /* Do a pre-search by projecting on the pyramid */
   if (K > (N>>1))
   {
      opus_val16 rcp;
      j=0;
	  for (;;)
	  {
         sum += X[j];
        if (!(++j<N)) break;
      }
	  
      /* If X is too small, just replace it with a pulse at 0 */

      /* Prevents infinities and NaNs from causing too many pulses
         to be allocated. 64 is an approximation of infinity here. */
      if (!(sum > 1.0e-15f && sum < 64.0))
      {
         X[0] = (1.0f);
         j=1;
		 for (;;)
		 {
            X[j]=0.0;
            if (!(++j<N)) break;
		 }
         sum = (1.0f);
      }
      rcp = (((float)(K-1)*((1.0f/(sum)))));
      j=0;
	  for (;;)
	  {
         iy[j] = (int)floor(rcp*X[j]);
         y[j] = (celt_norm)iy[j];
         yy = ((yy)+(opus_val32)(y[j])*(opus_val32)(y[j]));
         xy = ((xy)+(opus_val32)(X[j])*(opus_val32)(y[j]));
         y[j] *= 2.0;
         pulsesLeft -= iy[j];
        if (!(++j<N)) break;
	  }
   }
   ;

   /* This should never happen, but just in case it does (e.g. on silence)
      we fill the first bin with pulses. */

   if (pulsesLeft > N+3)
   {
      opus_val16 tmp = (opus_val16)pulsesLeft;
      yy = ((yy)+(opus_val32)(tmp)*(opus_val32)(tmp));
      yy = ((yy)+(opus_val32)(tmp)*(opus_val32)(y[0]));
      iy[0] += pulsesLeft;
      pulsesLeft=0;
   }

   s = 1.0;
   for (i=0;i<pulsesLeft;i++)
   {
      int best_id;
      opus_val32 best_num = -1.0e15f;
      opus_val16 best_den = 0.0;

      best_id = 0;
      /* The squared magnitude term gets added anyway, so we might as well
         add it outside the loop */
      yy = ((yy)+(1.0));
      j=0;
      for (;;)
	  {
         opus_val16 Rxy, Ryy;
         /* Temporary sums of the new pulse(s) */
         Rxy = ((((xy)+((X[j])))));
         /* We're multiplying y[j] by two so we don't have to do it here */
         Ryy = ((yy)+(y[j]));

         /* Approximate score: we maximise Rxy/sqrt(Ryy) (we're guaranteed that
            Rxy is positive because the sign is pre-computed) */
         Rxy = ((Rxy)*(Rxy));
         /* The idea is to check for num/den >= best_num/best_den, but that way
            we can do it without any division */
         /* OPT: Make sure to use conditional moves here */
         if (((opus_val32)(best_den)*(opus_val32)(Rxy)) > ((opus_val32)(Ryy)*(opus_val32)(best_num)))
         {
            best_den = Ryy;
            best_num = Rxy;
            best_id = j;
         }
        if (!(++j<N)) break;
      }
	  
      /* Updating the sums of the new pulse(s) */
      xy = ((xy)+((X[best_id])));
      /* We're multiplying y[j] by two so we don't have to do it here */
      yy = ((yy)+(y[best_id]));

      /* Only now that we've made the final choice, update y/iy */
      /* Multiplying y[j] by 2 so we don't have to do it everywhere else */
      y[best_id] += 2.0*s;
      iy[best_id]++;
   }

   /* Put the original sign back */
   j=0;
   for (;;)
   {
      X[j] = ((opus_val32)(signx[j])*(opus_val32)(X[j]));
      if (signx[j] < 0.0)
         iy[j] = -iy[j];
     if (!(++j<N)) break;
   }
   encode_pulses(iy, N, K, enc);

   collapse_mask = extract_collapse_mask(iy, N, B);
   
   afree((byte*)y);
   afree((byte*)iy);
   afree((byte*)signx);
   
   ;
   return collapse_mask;
}

/** Decode pulse vector and combine the result with the pitch vector to produce
    the final normalised signal in the current band. */
public unsigned alg_unquant(celt_norm *X, int N, int K, int spread, int B,
      ec_dec *dec, opus_val16 gain)
{
   int i;
   opus_val32 Ryy;
   unsigned collapse_mask;
   int *iy;
   ;

   ;
   ;
   iy = ((int*)malloc(((int)(int ' size  ))*(N)));
   decode_pulses(iy, N, K, dec);
   Ryy = 0.0;
   i=0;
   for (;;)
   {
      Ryy = ((Ryy)+(opus_val32)(iy[i])*(opus_val32)(iy[i]));
     if (!(++i < N)) break;
   }
   normalise_residual(iy, X, N, Ryy, gain);
   exp_rotation(X, N, -1, B, K, spread);
   collapse_mask = extract_collapse_mask(iy, N, B);
   afree((byte*)iy);
   ;
   return collapse_mask;
}

public void renormalise_vector(celt_norm *X, int N, opus_val16 gain)
{
   int i;

   opus_val32 E = 1.0e-15f;
   opus_val16 g;
   opus_val32 t;
   celt_norm *xptr = X;
   for (i=0;i<N;i++)
   {
      E = ((E)+(opus_val32)(*xptr)*(opus_val32)(*xptr));
      xptr++;
   }

   t = (E);
   g = ((((1.0f/((float)sqrt(t)))))*(gain));

   xptr = X;
   for (i=0;i<N;i++)
   {
      *xptr = ((((opus_val32)(g)*(opus_val32)(*xptr))));
      xptr++;
   }
   /*return celt_sqrt(E);*/
}

public int stereo_itheta(celt_norm *X, celt_norm *Y, int stereo, int N)
{
   int i;
   int itheta;
   opus_val16 mid, side;
   opus_val32 Emid, Eside;

   Eside = 1.0e-15f;
   Emid = 1.0e-15f;
   if (stereo != 0)
   {
      for (i=0;i<N;i++)
      {
         celt_norm m, s;
         m = (((X[i]))+((Y[i])));
         s = (((X[i]))-((Y[i])));
         Emid = ((Emid)+(opus_val32)(m)*(opus_val32)(m));
         Eside = ((Eside)+(opus_val32)(s)*(opus_val32)(s));
      }
   } else {
      for (i=0;i<N;i++)
      {
         celt_norm m, s;
         m = X[i];
         s = Y[i];
         Emid = ((Emid)+(opus_val32)(m)*(opus_val32)(m));
         Eside = ((Eside)+(opus_val32)(s)*(opus_val32)(s));
      }
   }
   mid = ((float)sqrt(Emid));
   side = ((float)sqrt(Eside));

   itheta = (int)floor(0.5f+16384.0*0.63662f*atan2(side,mid));

   return itheta;
}
#end unsafe
