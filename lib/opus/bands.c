#begin unsafe
/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2009 Xiph.Org Foundation
   Copyright (c) 2008-2009 Gregory Maxwell
   Written by Jean-Marc Valin and Gregory Maxwell */
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
use modes;
use vq;
use os_support;
use mathops;
use rate, ecintrin, entcode, entenc, entdec;

public opus_uint32 celt_lcg_rand(opus_uint32 seed)
{
   return 1664525 * seed + 1013904223;
}

/* This is a cos() approximation designed to be bit-exact on any platform. Bit exactness
   with this approximation is important because it has an impact on the bit allocation */
opus_int16 bitexact_cos(opus_int16 x)
{
   opus_int32 tmp;
   opus_int16 x2;
   tmp = (4096+((opus_int32)(x)*(x)))>>13;
   ;
   x2 = (opus_int16)tmp;
   x2 = (opus_int16)((32767-x2) + ((16384+((opus_int32)(opus_int16)(x2)*(opus_int16)((-7651 + ((16384+((opus_int32)(opus_int16)(x2)*(opus_int16)((8277 + ((16384+((opus_int32)(opus_int16)(-626)*(opus_int16)(x2)))>>15)))))>>15)))))>>15));
   ;
   return (opus_int16)(1+x2);
}

int bitexact_log2tan(int isin,int icos)
{
   int lc;
   int ls;
   lc=(int)(ec_ilog((uint)icos));
   ls=(int)(ec_ilog((uint)isin));
   *&icos<<=15-lc;
   *&isin<<=15-ls;
   return (ls-lc)*(1<<11)
         +((16384+((opus_int32)(opus_int16)(isin)*(opus_int16)(((16384+((opus_int32)(opus_int16)(isin)*(opus_int16)(-2597)))>>15) + 7932)))>>15)
         -((16384+((opus_int32)(opus_int16)(icos)*(opus_int16)(((16384+((opus_int32)(opus_int16)(icos)*(opus_int16)(-2597)))>>15) + 7932)))>>15);
}

/* Compute the amplitude (sqrt energy) in each of the bands */
public void compute_band_energies(      OpusCustomMode *m,       celt_sig *X, celt_ener *bandE, int _end, int C, int M)
{
   int i, c, N;
   opus_int16 *eBands = &m->eBands;
   N = M*m->shortMdctSize;
   c=0;
   for (;;)
   {
      for (i=0;i<_end;i++)
      {
         int j;
         opus_val32 sum = 1.0e-27f;
         for (j=M*eBands[i];j<M*eBands[i+1];j++)
            sum += X[j+c*N]*X[j+c*N];
         bandE[i+c*m->nbEBands] = ((float)sqrt(sum));
         /*printf ("%f ", bandE[i+c*m->nbEBands]);*/
      }
     if (!(++c<C)) break;
   }
   /*printf ("\n");*/
}

/* Normalise each band such that the energy is one. */
public void normalise_bands(      OpusCustomMode *m,       celt_sig *  freq, celt_norm *  X,       celt_ener *bandE, int _end, int C, int M)
{
   int i, c, N;
   opus_int16 *eBands = &m->eBands;
   N = M*m->shortMdctSize;
   c=0;
   for (;;)
   {
      for (i=0;i<_end;i++)
      {
         int j;
         opus_val16 g = 1.0f/(1.0e-27f+bandE[i+c*m->nbEBands]);
         for (j=M*eBands[i];j<M*eBands[i+1];j++)
            X[j+c*N] = freq[j+c*N]*g;
      }
     if (!(++c<C)) break;
   }
}


/* De-normalise the energy to produce the synthesis from the unit-energy bands */
public void denormalise_bands(      OpusCustomMode *m,       celt_norm *  X, celt_sig *  freq,       celt_ener *bandE, int _end, int C, int M)
{
   int i, c, N;
   opus_int16 *eBands = &m->eBands;
   N = M*m->shortMdctSize;
   ;
   c=0;
   for (;;)
   {
      celt_sig *  f;
            celt_norm *  x;
      f = freq+c*N;
      x = X+c*N;
      for (i=0;i<_end;i++)
      {
         int j, band_end;
         opus_val32 g = (bandE[i+c*m->nbEBands]);
         j=M*eBands[i];
         band_end = M*eBands[i+1];
         for (;;)
		 {
            *f++ = (((*x)*(g)));
            x++;
           if (!(++j<band_end)) break;
		 }
      }
      for (i=M*eBands[_end];i<N;i++)
         *f++ = 0.0;
     if (!(++c<C)) break;
   }
}

/* This prevents energy collapse for transients with multiple short MDCTs */
public void anti_collapse(      OpusCustomMode *m, celt_norm *X_,     byte      *collapse_masks, int LM, int C, int size,
      int start, int _end, opus_val16 *logE, opus_val16 *prev1logE,
      opus_val16 *prev2logE, int *pulses, opus_uint32 seed)
{
   int c, i, j, k;
   for (i=start;i<_end;i++)
   {
      int N0;
      opus_val16 thresh, sqrt_1;
      int depth;

      N0 = m->eBands[i+1]-m->eBands[i];
      /* depth in 1/8 bits */
      depth = (1+pulses[i])/((m->eBands[i+1]-m->eBands[i])<<LM);

      thresh = 0.5f*((float)exp(0.6931471805599453094D*(-0.125f*(float)depth)));
      sqrt_1 = (1.0f/((float)sqrt((float)(N0<<LM))));

      c=0;
	  for (;;)
      {
         celt_norm *X;
         opus_val16 prev1;
         opus_val16 prev2;
         opus_val32 Ediff;
         opus_val16 r;
         int renormalize=0;
         prev1 = prev1logE[c*m->nbEBands+i];
         prev2 = prev2logE[c*m->nbEBands+i];
         if (C==1)
         {
            prev1 = ((prev1) > (prev1logE[m->nbEBands+i]) ? (prev1) : (prev1logE[m->nbEBands+i]));
            prev2 = ((prev2) > (prev2logE[m->nbEBands+i]) ? (prev2) : (prev2logE[m->nbEBands+i]));
         }
         Ediff = (logE[c*m->nbEBands+i])-(((prev1) < (prev2) ? (prev1) : (prev2)));
         Ediff = ((0.0) > (Ediff) ? (0.0) : (Ediff));

         /* r needs to be multiplied by 2 or 2*sqrt(2) depending on LM because
            short blocks don't have the same energy as long */
         r = 2.0f*((float)exp(0.6931471805599453094D*(-Ediff)));
         if (LM==3)
            r *= 1.41421356f;
         r = ((thresh) < (r) ? (thresh) : (r));
         r = r*sqrt_1;
         X = X_+c*size+(m->eBands[i]<<LM);
         for (k=0;k<1<<LM;k++)
         {
            /* Detect collapse */
            if ((collapse_masks[i*C+c]&1<<(uint)k) == 0)
            {
               /* Fill with noise */
               for (j=0;j<N0;j++)
               {
                  *&seed = celt_lcg_rand(seed);
                  X[(j<<LM)+k] = ((seed&0x8000) != 0 ? r : -r);
               }
               renormalize = 1;
            }
         }
         /* We just added some energy, so we need to renormalise */
         if (renormalize != 0)
            renormalise_vector(X, N0<<LM, 1.0f);
        if (!(++c<C)) break;
	  }
   }
}

void intensity_stereo(      OpusCustomMode *m, celt_norm *X, celt_norm *Y,       celt_ener *bandE, int bandID, int N)
{
   int i = bandID;
   int j;
   opus_val16 a1, a2;
   opus_val16 left, right;
   opus_val16 norm;

   left = (bandE[i]);
   right = (bandE[i+m->nbEBands]);
   norm = 1.0e-15f + ((float)sqrt(1.0e-15f+((opus_val32)(left)*(opus_val32)(left))+((opus_val32)(right)*(opus_val32)(right))));
   a1 = (((opus_val32)(((left))))/(opus_val16)(norm));
   a2 = (((opus_val32)(((right))))/(opus_val16)(norm));
   for (j=0;j<N;j++)
   {
      celt_norm r, l;
      l = X[j];
      r = Y[j];
      X[j] = ((a1)*(l)) + ((a2)*(r));
      /* Side is not encoded, no need to calculate */
   }
}

void stereo_split(celt_norm *X, celt_norm *Y, int N)
{
   int j;
   for (j=0;j<N;j++)
   {
      celt_norm r, l;
      l = (((0.70710678f))*(X[j]));
      r = (((0.70710678f))*(Y[j]));
      X[j] = l+r;
      Y[j] = r-l;
   }
}

void stereo_merge(celt_norm *X, celt_norm *Y, opus_val16 mid, int N)
{
   int j;
   opus_val32 xp=0.0, side=0.0;
   opus_val32 El, Er;
   opus_val16 mid2;

   opus_val32 t, lgain, rgain;

   /* Compute the norm of X+Y and X-Y as |X|^2 + |Y|^2 +/- sum(xy) */
   for (j=0;j<N;j++)
   {
      xp = ((xp)+(opus_val32)(X[j])*(opus_val32)(Y[j]));
      side = ((side)+(opus_val32)(Y[j])*(opus_val32)(Y[j]));
   }
   /* Compensating for the mid normalization */
   xp = ((mid)*(xp));
   /* mid and side are in Q15, not Q14 like X and Y */
   mid2 = (mid);
   El = ((opus_val32)(mid2)*(opus_val32)(mid2)) + side - 2.0*xp;
   Er = ((opus_val32)(mid2)*(opus_val32)(mid2)) + side + 2.0*xp;
   if (Er < (6.0e-4f) || El < (6.0e-4f))
   {
      for (j=0;j<N;j++)
         Y[j] = X[j];
      return;
   }

   t = (El);
   lgain = ((1.0f/((float)sqrt(t))));
   t = (Er);
   rgain = ((1.0f/((float)sqrt(t))));

   for (j=0;j<N;j++)
   {
      celt_norm r, l;
      /* Apply mid scaling (side is already scaled) */
      l = ((mid)*(X[j]));
      r = Y[j];
      X[j] = ((((opus_val32)(lgain)*(opus_val32)(((l)-(r))))));
      Y[j] = ((((opus_val32)(rgain)*(opus_val32)(((l)+(r))))));
   }
}

/* Decide whether we should spread the pulses in the current frame */
public int spreading_decision(      OpusCustomMode *m, celt_norm *X, int *average,
      int last_decision, int *hf_average, int *tapset_decision, int update_hf,
      int _end, int C, int M)
{
   int i, c, N0;
   int sum = 0, nbBands=0;
   opus_int16 *  eBands = &m->eBands;
   int decision;
   int hf_sum=0;

   ;

   N0 = M*m->shortMdctSize;

   if (M*(eBands[_end]-eBands[_end-1]) <= 8)
      return (0);
   c=0;
   for (;;)
   {
      for (i=0;i<_end;i++)
      {
         int j, N, tmp=0;
         int tcount[3] = {0,0,0};
         celt_norm *  x = X+M*eBands[i]+c*N0;
         N = M*(eBands[i+1]-eBands[i]);
         if (N<=8)
            continue;
         /* Compute rough CDF of |x[j]| */
         for (j=0;j<N;j++)
         {
            opus_val32 x2N; /* Q13 */

            x2N = ((opus_val32)(((x[j])*(x[j])))*(opus_val32)(N));
            if (x2N < (0.25f))
               tcount[0]++;
            if (x2N < (0.0625f))
               tcount[1]++;
            if (x2N < (0.015625f))
               tcount[2]++;
         }

         /* Only include four last bands (8 kHz and up) */
         if (i>m->nbEBands-4)
            hf_sum += 32*(tcount[1]+tcount[0])/N;
         tmp = (int)(2*tcount[2] >= N) + (int)(2*tcount[1] >= N) + (int)(2*tcount[0] >= N);
         sum += tmp*256;
         nbBands++;
      }
     if (!(++c<C)) break;
   }
   
   if (update_hf != 0)
   {
      if (hf_sum != 0)
         hf_sum /= C*(4-m->nbEBands+_end);
      *hf_average = (*hf_average+hf_sum)>>1;
      hf_sum = *hf_average;
      if (*tapset_decision==2)
         hf_sum += 4;
      else if (*tapset_decision==0)
         hf_sum -= 4;
      if (hf_sum > 22)
         *tapset_decision=2;
      else if (hf_sum > 18)
         *tapset_decision=1;
      else
         *tapset_decision=0;
   }
   /*printf("%d %d %d\n", hf_sum, *hf_average, *tapset_decision);*/
   ; /*M*(eBands[end]-eBands[end-1]) <= 8 assures this*/
   sum /= nbBands;
   /* Recursive averaging */
   sum = (sum+*average)>>1;
   *average = sum;
   /* Hysteresis */
   sum = (3*sum + (((3-last_decision)<<7) + 64) + 2)>>2;
   if (sum < 80)
   {
      decision = (3);
   } else if (sum < 256)
   {
      decision = (2);
   } else if (sum < 384)
   {
      decision = (1);
   } else {
      decision = (0);
   }

   return decision;
}

package CTES
/* Indexing table for converting from natural Hadamard to ordery Hadamard
   This is essentially a bit-reversed Gray, on top of which we've added
   an inversion of the order because we want the DC at the end rather than
   the beginning. The lines are for N=2, 4, 8, 16 */
const        int ordery_table[] = {
       1,  0,
       3,  0,  2,  1,
       7,  0,  4,  3,  6,  1,  5,  2,
      15,  0,  8,  7, 12,  3, 11,  4, 14,  1,  9,  6, 13,  2, 10,  5,
};
end CTES;

void deinterleave_hadamard(celt_norm *X, int N0, int stride, int hadamard)
{
   int i,j;
   celt_norm *tmp;
   int N;
   ;
   N = N0*stride;
   tmp = ((celt_norm*)malloc(((int)(celt_norm ' size  ))*(N)));
   ;
   if (hadamard != 0)
   {
      int *ordery = &ordery_table+stride-2;
      for (i=0;i<stride;i++)
      {
         for (j=0;j<N0;j++)
            tmp[ordery[i]*N0+j] = X[j*stride+i];
      }
   } else {
      for (i=0;i<stride;i++)
         for (j=0;j<N0;j++)
            tmp[i*N0+j] = X[j*stride+i];
   }
   for (j=0;j<N;j++)
      X[j] = tmp[j];
   afree((byte*)tmp);
   ;
}

void interleave_hadamard(celt_norm *X, int N0, int stride, int hadamard)
{
   int i,j;
   celt_norm *tmp;
   int N;
   ;
   N = N0*stride;
   tmp = ((celt_norm*)malloc(((int)(celt_norm ' size  ))*(N)));
   if (hadamard != 0)
   {
      int *ordery = &ordery_table+stride-2;
      for (i=0;i<stride;i++)
         for (j=0;j<N0;j++)
            tmp[j*stride+i] = X[ordery[i]*N0+j];
   } else {
      for (i=0;i<stride;i++)
         for (j=0;j<N0;j++)
            tmp[j*stride+i] = X[i*N0+j];
   }
   for (j=0;j<N;j++)
      X[j] = tmp[j];
   afree((byte*)tmp);
   ;
}

public void haar1(celt_norm *X, int N0, int stride)
{
   int i, j;
   *&N0 >>= 1;
   for (i=0;i<stride;i++)
      for (j=0;j<N0;j++)
      {
         celt_norm tmp1, tmp2;
         tmp1 = (((0.70710678f))*(X[stride*2*j+i]));
         tmp2 = (((0.70710678f))*(X[stride*(2*j+1)+i]));
         X[stride*2*j+i] = tmp1 + tmp2;
         X[stride*(2*j+1)+i] = tmp1 - tmp2;
      }
}

int compute_qn(int N, int b, int offset, int pulse_cap, int stereo)
{
   const        opus_int16 exp2_table8[8] =
      {16384, 17866, 19483, 21247, 23170, 25267, 27554, 30048};
   int qn, qb;
   int N2 = 2*N-1;
   if (stereo != 0 && N==2)
      N2--;
   /* The upper limit ensures that in a stereo split with itheta==16384, we'll
       always have enough bits left over to code at least one pulse in the
       side; otherwise it would collapse, since it doesn't get folded. */
   qb = ((b-pulse_cap-(4<<3)) < ((b+N2*offset)/N2) ? (b-pulse_cap-(4<<3)) : ((b+N2*offset)/N2));

   qb = ((8<<3) < (qb) ? (8<<3) : (qb));

   if (qb<((1<<3)>>1)) {
      qn = 1;
   } else {
      qn = exp2_table8[qb&0x7]>>(14-(qb>>3));
      qn = ((qn+1)>>1)<<1;
   }
   ;
   return qn;
}

/* This function is responsible for encoding and decoding a band for both
   the mono and stereo case. Even in the mono case, it can split the band
   in two and transmit the energy difference with the two half-bands. It
   can be called recursively so bands can end up being split in 8 parts. */
unsigned quant_band(int encode,       OpusCustomMode *m, int i, celt_norm *X, celt_norm *Y,
      int N, int b, int spread, int B, int intensity, int tf_change, celt_norm *lowband, ec_ctx *ec,
      opus_int32 *remaining_bits, int LM, celt_norm *lowband_out,       celt_ener *bandE, int level,
      opus_uint32 *seed, opus_val16 gain, celt_norm *lowband_scratch, int fill)
{
             byte      *cache;
   int q;
   int curr_bits;
   int stereo, split;
   int imid=0, iside=0;
   int N0=N;
   int N_B=N;
   int N_B0;
   int B0=B;
   int time_divide=0;
   int recombine=0;
   int inv = 0;
   opus_val16 mid=0.0, side=0.0;
   int longBlocks;
   unsigned cm=0;

   int resynth = (int)!(bool)encode;

   longBlocks = (int)(B0==1);

   N_B /= B;
   N_B0 = N_B;

   stereo = (int)(Y != null);
   split = stereo;
  
   /* Special case for one sample */
   if (N==1)
   {
      int c;
      celt_norm *x = X;
      c=0;
	  for (;;)
	  {
         int sign=0;
         if (*remaining_bits>=1<<3)
         {
            if (encode != 0)
            {
               sign = (int)(x[0]<0.0);
               ec_enc_bits(ec, (uint)sign, 1);
            } else {
               sign = (int)ec_dec_bits(ec, 1);
            }
            *remaining_bits -= 1<<3;
            *&b -= 1<<3;
         }
         if (resynth != 0)
            x[0] = sign!=0 ? -1.0f : 1.0f;
         x = Y;
        if (!(++c<1+stereo)) break;
	  }
      if (lowband_out != null)
         lowband_out[0] = (X[0]);
      return 1;
   }

   if (stereo == 0 && level == 0)
   {
      int k;
      if (tf_change>0)
         recombine = tf_change;
      /* Band recombining to increase frequency resolution */

      if (lowband != null && (recombine!=0 || ((N_B&1) == 0 && tf_change<0) || B0>1))
      {
         int j;
         for (j=0;j<N;j++)
            lowband_scratch[j] = lowband[j];
         *&lowband = lowband_scratch;
      }

      for (k=0;k<recombine;k++)
      {
         const            byte      bit_interleave_table[16]={
           0,1,1,1,2,3,3,3,2,3,3,3,2,3,3,3
         };
         if (encode != 0)
            haar1(X, N>>k, 1<<k);
         if (lowband != null)
            haar1(lowband, N>>k, 1<<k);
         *&fill = (int)(bit_interleave_table[fill&0xF]|bit_interleave_table[fill>>4]<<2);
      }
      *&B >>= recombine;
      N_B <<= recombine;

      /* Increasing the time resolution */
      while ((N_B&1) == 0 && tf_change<0)
      {
         if (encode != 0)
            haar1(X, N_B, B);
         if (lowband != null)
            haar1(lowband, N_B, B);
         *&fill |= fill<<B;
         *&B <<= 1;
         N_B >>= 1;
         time_divide++;
         (*&tf_change)++;
      }
      B0=B;
      N_B0 = N_B;

      /* Reorganize the samples in time order instead of frequency order */
      if (B0>1)
      {
         if (encode != 0)
            deinterleave_hadamard(X, N_B>>recombine, B0<<recombine, longBlocks);
         if (lowband != null)
            deinterleave_hadamard(lowband, N_B>>recombine, B0<<recombine, longBlocks);
      }
   }

   /* If we need 1.5 more bit than we can produce, split the band in two. */
   cache = &m->cache.bits + m->cache.index[(LM+1)*m->nbEBands+i];
   if (stereo==0 && LM != -1 && (uint)b > cache[cache[0]]+12 && N>2)
   {
      *&N >>= 1;
      *&Y = X+N;
      split = 1;
      *&LM -= 1;
      if (B==1)
         *&fill = (fill&1)|(fill<<1);
      *&B = (B+1)>>1;
   }

   if (split != 0)
   {
      int qn;
      int itheta=0;
      int mbits, sbits, delta;
      int qalloc;
      int pulse_cap;
      int offset;
      int orig_fill;
      opus_int32 tell;

      /* Decide on the resolution to give to the split parameter theta */
      pulse_cap = m->logN[i]+LM*(1<<3);
      offset = (pulse_cap>>1) - (stereo!=0&&N==2 ? 16 : 4);
      qn = compute_qn(N, b, offset, pulse_cap, stereo);
      if (stereo!=0 && i>=intensity)
         qn = 1;
      if (encode!=0)
      {
         /* theta is the atan() of the ratio between the (normalized)
            side and mid. With just that parameter, we can re-scale both
            mid and side because we know that 1) they have unit norm and
            2) they are orthogonal. */
         itheta = stereo_itheta(X, Y, stereo, N);
      }
      tell = (int)ec_tell_frac(ec);
      if (qn!=1)
      {
         if (encode!=0)
            itheta = (itheta*qn+8192)>>14;

         /* Entropy coding of the angle. We use a uniform pdf for the
            time split, a step for stereo, and a triangular one for the rest. */
         if (stereo!=0 && N>2)
         {
            int p0 = 3;
            int x = itheta;
            int x0 = qn/2;
            int ft = p0*(x0+1) + x0;
            /* Use a probability of p0 up to itheta=8192 and then use 1 after */
            if (encode!=0)
            {
               ec_encode(ec,  (uint)(x<=x0?p0*x:(x-1-x0)+(x0+1)*p0),   (uint)(x<=x0?p0*(x+1):(x-x0)+(x0+1)*p0),  (uint)ft);
            } else {
               int fs;
               fs=(int)ec_decode(ec,(uint)ft);
               if (fs<(x0+1)*p0)
                  x=fs/p0;
               else
                  x=x0+1+(fs-(x0+1)*p0);
               ec_dec_update(ec, (uint)(x<=x0?p0*x:(x-1-x0)+(x0+1)*p0),  (uint)(x<=x0?p0*(x+1):(x-x0)+(x0+1)*p0), (uint)ft);
               itheta = x;
            }
         } else if (B0>1 || stereo!=0) {
            /* Uniform pdf */
            if (encode !=0)
               ec_enc_uint(ec, (uint)itheta, (uint)qn+1);
            else
               itheta = (int)ec_dec_uint(ec, (uint)qn+1);
         } else {
            int fs=1, ft;
            ft = ((qn>>1)+1)*((qn>>1)+1);
            if (encode!=0)
            {
               int fl;

               fs = itheta <= (qn>>1) ? itheta + 1 : qn + 1 - itheta;
               fl = itheta <= (qn>>1) ? itheta*(itheta + 1)>>1 :
                ft - ((qn + 1 - itheta)*(qn + 2 - itheta)>>1);

               ec_encode(ec, (uint)fl, (uint)(fl+fs), (uint)ft);
            } else {
               /* Triangular pdf */
               int fl=0;
               int fm;
               fm = (int)ec_decode(ec, (uint)ft);

               if (fm < ((qn>>1)*((qn>>1) + 1)>>1))
               {
                  itheta = (int)((isqrt32(8*(opus_uint32)fm + 1) - 1)>>1);
                  fs = itheta + 1;
                  fl = itheta*(itheta + 1)>>1;
               }
               else
               {
                  itheta = (int)((2*(qn + 1) - (int)isqrt32(8*(opus_uint32)(ft - fm - 1) + 1))>>1);
                  fs = qn + 1 - itheta;
                  fl = ft - ((qn + 1 - itheta)*(qn + 2 - itheta)>>1);
               }

               ec_dec_update(ec, (uint)fl, (uint)(fl+fs), (uint)ft);
            }
         }
         itheta = (opus_int32)itheta*16384/qn;
         if (encode!=0 && stereo!=0)
         {
            if (itheta==0)
               intensity_stereo(m, X, Y, bandE, i, N);
            else
               stereo_split(X, Y, N);
         }
         /* NOTE: Renormalising X and Y *may* help fixed-point a bit at very high rate.
                  Let's do that at higher complexity */
      } else if (stereo != 0) {
         if (encode != 0)
         {
            inv = (int)(itheta > 8192);
            if (inv != 0)
            {
               int j;
               for (j=0;j<N;j++)
                  Y[j] = -Y[j];
            }
            intensity_stereo(m, X, Y, bandE, i, N);
         }
         if (b>2<<3 && *remaining_bits > 2<<3)
         {
            if (encode != 0)
               ec_enc_bit_logp(ec, inv, 2);
            else
               inv = ec_dec_bit_logp(ec, 2);
         } else
            inv = 0;
         itheta = 0;
      }
      qalloc = (int)ec_tell_frac(ec) - tell;
      *&b -= qalloc;

      orig_fill = fill;
      if (itheta == 0)
      {
         imid = 32767;
         iside = 0;
         *&fill &= (1<<B)-1;
         delta = -16384;
      } else if (itheta == 16384)
      {
         imid = 0;
         iside = 32767;
         *&fill &= ((1<<B)-1)<<B;
         delta = 16384;
      } else {
         imid = bitexact_cos((opus_int16)itheta);
         iside = bitexact_cos((opus_int16)(16384-itheta));
         /* This is the mid vs side allocation that minimizes squared error
            in that band. */
         delta = ((16384+((opus_int32)(opus_int16)((N-1)<<7)*(opus_int16)(bitexact_log2tan(iside,imid))))>>15);
      }

      mid = (1.0f/32768.0)*(float)imid;
      side = (1.0f/32768.0)*(float)iside;

      /* This is a special case for N=2 that only works for stereo and takes
         advantage of the fact that mid and side are orthogonal to encode
         the side with just one bit. */
      if (N==2 && stereo!=0)
      {
         int c;
         int sign=0;
         celt_norm *x2, y2;
         mbits = b;
         sbits = 0;
         /* Only need one bit for the side */
         if (itheta != 0 && itheta != 16384)
            sbits = 1<<3;
         mbits -= sbits;
         c = (int)(itheta > 8192);
         *remaining_bits -= qalloc+sbits;

         x2 = c!=0 ? Y : X;
         y2 = c!=0 ? X : Y;
         if (sbits !=0)
         {
            if (encode!=0)
            {
               /* Here we only need to encode a sign for the side */
               sign = (int)(x2[0]*y2[1] - x2[1]*y2[0] < 0.0);
               ec_enc_bits(ec, (uint)sign, 1);
            } else {
               sign = (int)ec_dec_bits(ec, 1);
            }
         }
         sign = 1-2*sign;
         /* We use orig_fill here because we want to fold the side, but if
             itheta==16384, we'll have cleared the low bits of fill. */
         cm = quant_band(encode, m, i, x2, null, N, mbits, spread, B, intensity, tf_change, lowband, ec, remaining_bits, LM, lowband_out, null, level, seed, gain, lowband_scratch, orig_fill);
         /* We don't split N=2 bands, so cm is either 1 or 0 (for a fold-collapse),
             and there's no need to worry about mixing with the other channel. */
         y2[0] = -(float)sign*x2[1];
         y2[1] = (float)sign*x2[0];
         if (resynth != 0)
         {
            celt_norm tmp;
            X[0] = ((mid)*(X[0]));
            X[1] = ((mid)*(X[1]));
            Y[0] = ((side)*(Y[0]));
            Y[1] = ((side)*(Y[1]));
            tmp = X[0];
            X[0] = ((tmp)-(Y[0]));
            Y[0] = ((tmp)+(Y[0]));
            tmp = X[1];
            X[1] = ((tmp)-(Y[1]));
            Y[1] = ((tmp)+(Y[1]));
         }
      } else {
         /* "Normal" split code */
         celt_norm *next_lowband2     = null;
         celt_norm *next_lowband_out1 = null;
         int next_level=0;
         opus_int32 rebalance;

         /* Give more bits to low-energy MDCTs than they would otherwise deserve */
         if (B0>1 && stereo==0 && (itheta&0x3fff) != 0)
         {
            if (itheta > 8192)
               /* Rough approximation for pre-echo masking */
               delta -= delta>>(4-LM);
            else
               /* Corresponds to a forward-masking slope of 1.5 dB per 10 ms */
               delta = ((0) < (delta + ((N<<3)>>(5-LM))) ? (0) : (delta + ((N<<3)>>(5-LM))));
         }
         mbits = ((0) > (((b) < ((b-delta)/2) ? (b) : ((b-delta)/2))) ? (0) : (((b) < ((b-delta)/2) ? (b) : ((b-delta)/2))));
         sbits = b-mbits;
         *remaining_bits -= qalloc;

         if (lowband!=null && stereo==0)
            next_lowband2 = lowband+N; /* >32-bit split case */

         /* Only stereo needs to pass on lowband_out. Otherwise, it's
            handled at the end */
         if (stereo!=0)
            next_lowband_out1 = lowband_out;
         else
            next_level = level+1;

         rebalance = *remaining_bits;
         if (mbits >= sbits)
         {
            /* In stereo mode, we do not apply a scaling to the mid because we need the normalized
               mid for folding later */
            cm = quant_band(encode, m, i, X, null, N, mbits, spread, B, intensity, tf_change,
                  lowband, ec, remaining_bits, LM, next_lowband_out1,
                  null, next_level, seed, stereo!=0 ? 1.0f : ((gain)*(mid)), lowband_scratch, fill);
            rebalance = mbits - (rebalance-*remaining_bits);
            if (rebalance > 3<<3 && itheta!=0)
               sbits += rebalance - (3<<3);

            /* For a stereo split, the high bits of fill are always zero, so no
               folding will be done to the side. */
            cm |= quant_band(encode, m, i, Y, null, N, sbits, spread, B, intensity, tf_change,
                  next_lowband2, ec, remaining_bits, LM, null,
                  null, next_level, seed, ((gain)*(side)), null, fill>>B)  <<  (uint)((B0>>1)&(stereo-1));
         } else {
            /* For a stereo split, the high bits of fill are always zero, so no
               folding will be done to the side. */
            cm = quant_band(encode, m, i, Y, null, N, sbits, spread, B, intensity, tf_change,
                  next_lowband2, ec, remaining_bits, LM, null,
                  null, next_level, seed, ((gain)*(side)), null, fill>>B) << (uint)((B0>>1)&(stereo-1));
            rebalance = sbits - (rebalance-*remaining_bits);
            if (rebalance > 3<<3 && itheta!=16384)
               mbits += rebalance - (3<<3);
            /* In stereo mode, we do not apply a scaling to the mid because we need the normalized
               mid for folding later */
            cm |= quant_band(encode, m, i, X, null, N, mbits, spread, B, intensity, tf_change,
                  lowband, ec, remaining_bits, LM, next_lowband_out1,
                  null, next_level, seed, stereo!=0 ? 1.0f : ((gain)*(mid)), lowband_scratch, fill);
         }
      }

   } else {
      /* This is the basic no-split case */
      q = bits2pulses(m, i, LM, b);
      curr_bits = pulses2bits(m, i, LM, q);
      *remaining_bits -= curr_bits;

      /* Ensures we can never bust the budget */
      while (*remaining_bits < 0 && q > 0)
      {
         *remaining_bits += curr_bits;
         q--;
         curr_bits = pulses2bits(m, i, LM, q);
         *remaining_bits -= curr_bits;
      }

      if (q!=0)
      {
         int K = get_pulses(q);

         /* Finally do the actual quantization */
         if (encode!=0)
         {
            cm = alg_quant(X, N, K, spread, B, ec

                 );
         } else {
            cm = alg_unquant(X, N, K, spread, B, ec, gain);
         }
      } else {
         /* If there's no pulse, fill the band anyway */
         int j;
         if (resynth!=0)
         {
            unsigned cm_mask;
            /*B can be as large as 16, so this shift might overflow an int on a
               16-bit platform; use a long to get defined behavior.*/
            cm_mask = (unsigned)(1<<B)-1;
            *&fill &= (int)cm_mask;
            if (fill == 0)
            {
               for (j=0;j<N;j++)
                  X[j] = 0.0;
            } else {
               if (lowband == null)
               {
                  /* Noise */
                  for (j=0;j<N;j++)
                  {
                     *seed = celt_lcg_rand(*seed);
                     X[j] = (celt_norm)((opus_int32)*seed>>20);
                  }
                  cm = cm_mask;
               } else {
                  /* Folded spectrum */
                  for (j=0;j<N;j++)
                  {
                     opus_val16 tmp;
                     *seed = celt_lcg_rand(*seed);
                     /* About 48 dB below the "normal" folding level */
                     tmp = (1.0f/256.0);
                     tmp = ((*seed)&0x8000)!=0 ? tmp : -tmp;
                     X[j] = lowband[j]+tmp;
                  }
                  cm = (uint)fill;
               }
               renormalise_vector(X, N, gain);
            }
         }
      }
   }

   /* This code is used by the decoder and by the resynthesis-enabled encoder */
   if (resynth != 0)
   {
      if (stereo != 0)
      {
         if (N!=2)
            stereo_merge(X, Y, mid, N);
         if (inv != 0)
         {
            int j;
            for (j=0;j<N;j++)
               Y[j] = -Y[j];
         }
      } else if (level == 0)
      {
         int k;

         /* Undo the sample reorganization going from time order to frequency order */
         if (B0>1)
            interleave_hadamard(X, N_B>>recombine, B0<<recombine, longBlocks);

         /* Undo time-freq changes that we did earlier */
         N_B = N_B0;
         *&B = B0;
         for (k=0;k<time_divide;k++)
         {
            *&B >>= 1;
            N_B <<= 1;
            cm |= cm>>(uint)B;
            haar1(X, N_B, B);
         }

         for (k=0;k<recombine;k++)
         {
            const            byte      bit_deinterleave_table[16]={
              0x00,0x03,0x0C,0x0F,0x30,0x33,0x3C,0x3F,
              0xC0,0xC3,0xCC,0xCF,0xF0,0xF3,0xFC,0xFF
            };
            cm = bit_deinterleave_table[cm];
            haar1(X, N0>>k, 1<<k);
         }
         *&B <<= recombine;

         /* Scale output for later folding */
         if (lowband_out != null)
         {
            int j;
            opus_val16 n;
            n = ((float)sqrt(((float)(N0))));
            for (j=0;j<N0;j++)
               lowband_out[j] = ((n)*(X[j]));
         }
         cm &= (uint)((1<<B)-1);
      }
   }
   return cm;
}

public void quant_all_bands(int encode,       OpusCustomMode *m, int start, int _end,
      celt_norm *X_, celt_norm *Y_,     byte      *collapse_masks,       celt_ener *bandE, int *pulses,
      int shortBlocks, int spread, int dual_stereo, int intensity, int *tf_res,
      opus_int32 total_bits, opus_int32 balance, ec_ctx *ec, int LM, int codedBands, opus_uint32 *seed)
{
   int i;
   opus_int32 remaining_bits;
   opus_int16 *  eBands = &m->eBands;
   celt_norm *  norm, norm2;
   celt_norm *_norm;
   celt_norm *lowband_scratch;
   int B;
   int M;
   int lowband_offset;
   int update_lowband = 1;
   int C = Y_ != null ? 2 : 1;

   int resynth = (int)!(bool)encode;
   ;

   M = 1<<LM;
   B = shortBlocks!=0 ? M : 1;
   _norm = ((celt_norm*)malloc(((int)(celt_norm ' size  ))*(C*M*eBands[m->nbEBands])));
   lowband_scratch = ((celt_norm*)malloc(((int)(celt_norm ' size  ))*(M*(eBands[m->nbEBands]-eBands[m->nbEBands-1]))));
   norm = _norm;
   norm2 = norm + M*eBands[m->nbEBands];

   lowband_offset = 0;
   for (i=start;i<_end;i++)
   {
      opus_int32 tell;
      int b;
      int N;
      opus_int32 curr_balance;
      int effective_lowband=-1;
      celt_norm *  X, Y;
      int tf_change=0;
      unsigned x_cm;
      unsigned y_cm;

      X = X_+M*eBands[i];
      if (Y_!=null)
         Y = Y_+M*eBands[i];
      else
         Y = null;
      N = M*eBands[i+1]-M*eBands[i];
      tell = (int)ec_tell_frac(ec);

      /* Compute how many bits we want to allocate to this band */
      if (i != start)
         *&balance -= (int)tell;
      remaining_bits = total_bits-tell-1;
      if (i <= codedBands-1)
      {
         curr_balance = balance / ((3) < (codedBands-i) ? (3) : (codedBands-i));
         b = ((0) > (((16383) < (((remaining_bits+1) < (pulses[i]+curr_balance) ? (remaining_bits+1) : (pulses[i]+curr_balance))) ? (16383) : (((remaining_bits+1) < (pulses[i]+curr_balance) ? (remaining_bits+1) : (pulses[i]+curr_balance))))) ? (0) : (((16383) < (((remaining_bits+1) < (pulses[i]+curr_balance) ? (remaining_bits+1) : (pulses[i]+curr_balance))) ? (16383) : (((remaining_bits+1) < (pulses[i]+curr_balance) ? (remaining_bits+1) : (pulses[i]+curr_balance))))));
      } else {
         b = 0;
      }

      if (resynth!=0 && M*eBands[i]-N >= M*eBands[start] && (update_lowband!=0 || lowband_offset==0))
            lowband_offset = i;

      tf_change = tf_res[i];
      if (i>=m->effEBands)
      {
         X=norm;
         if (Y_!=null)
            Y = norm;
      }

      /* Get a conservative estimate of the collapse_mask's for the bands we're
          going to be folding from. */
      if (lowband_offset != 0 && (spread!=(3) || B>1 || tf_change<0))
      {
         int fold_start;
         int fold_end;
         int fold_i;
         /* This ensures we never repeat spectral content within one band */
         effective_lowband = ((M*eBands[start]) > (M*eBands[lowband_offset]-N) ? (M*eBands[start]) : (M*eBands[lowband_offset]-N));
         fold_start = lowband_offset;
         while(M*eBands[--fold_start] > effective_lowband);
         fold_end = lowband_offset-1;
         while(M*eBands[++fold_end] < effective_lowband+N);
         x_cm = 0;
		 y_cm = 0;
         fold_i = fold_start;
		 for (;;)
		 {
           x_cm |= collapse_masks[fold_i*C+0];
           y_cm |= collapse_masks[fold_i*C+C-1];
           if (!(++fold_i<fold_end)) break;
		 }
      }
      /* Otherwise, we'll be using the LCG to fold, so all blocks will (almost
          always) be non-zero.*/
      else
	  {
         y_cm = (uint)((1<<B)-1);
		 x_cm = y_cm;
	  }

      if (dual_stereo!=0 && i==intensity)
      {
         int j;

         /* Switch off dual stereo to do intensity */
         *&dual_stereo = 0;
         if (resynth != 0)
            for (j=M*eBands[start];j<M*eBands[i];j++)
               norm[j] = (0.5f*(norm[j]+norm2[j]));
      }
      if (dual_stereo!=0)
      {
         x_cm = quant_band(encode, m, i, X, null, N, b/2, spread, B, intensity, tf_change,
               effective_lowband != -1 ? norm+effective_lowband : null, ec, &remaining_bits, LM,
               norm+M*eBands[i], bandE, 0, seed, 1.0f, lowband_scratch, (int)x_cm);
         y_cm = quant_band(encode, m, i, Y, null, N, b/2, spread, B, intensity, tf_change,
               effective_lowband != -1 ? norm2+effective_lowband : null, ec, &remaining_bits, LM,
               norm2+M*eBands[i], bandE, 0, seed, 1.0f, lowband_scratch, (int)y_cm);
      } else {
         x_cm = quant_band(encode, m, i, X, Y, N, b, spread, B, intensity, tf_change,
               effective_lowband != -1 ? norm+effective_lowband : null, ec, &remaining_bits, LM,
               norm+M*eBands[i], bandE, 0, seed, 1.0f, lowband_scratch, (int)(x_cm|y_cm));
         y_cm = x_cm;
      }
      collapse_masks[i*C+0] = (    byte     )x_cm;
      collapse_masks[i*C+C-1] = (    byte     )y_cm;
      *&balance += pulses[i] + tell;

      /* Update the folding position only as long as we have 1 bit/sample depth */
      update_lowband = (int)(b>(N<<3));
   }
   afree((byte*)_norm);
   afree((byte*)lowband_scratch);
   ;
}

#end unsafe
