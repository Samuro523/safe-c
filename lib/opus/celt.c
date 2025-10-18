#begin unsafe
/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2010 Xiph.Org Foundation
   Copyright (c) 2008 Gregory Maxwell
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

use ../arithm, ../math;

use opus_types, arch;
use os_support;
use mdct;
use pitch;
use bands;
use modes;
use entenc, entdec, entcode;
use quant_bands;
use rate;
use celt_lpc;
use vq;
use ecintrin;


const            byte      trim_icdf[11] = {126, 124, 119, 109, 87, 41, 19, 9, 4, 2, 0};
/* Probs: NONE: 21.875%, LIGHT: 6.25%, NORMAL: 65.625%, AGGRESSIVE: 6.25% */
const            byte      spread_icdf[4] = {25, 23, 2, 0};

const            byte      tapset_icdf[3]={2,1,0};

/** Encoder state
 @brief Encoder state
 */
struct OpusCustomEncoder {
   OpusCustomMode *mode;     /**< Mode used by the encoder */
   int overlap;
   int channels;
   int stream_channels;

   int force_intra;
   int clip;
   int disable_pf;
   int complexity;
   int upsample;
   int start, _end;

   opus_int32 bitrate;
   int vbr;
   int signalling;
   int constrained_vbr;      /* If zero, VBR can do whatever it likes with the rate */
   int loss_rate;
   int lsb_depth;

   /* Everything beyond this point gets cleared on a reset */

   opus_uint32 rng;
   int spread_decision;
   opus_val32 delayedIntra;
   int tonal_average;
   int lastCodedBands;
   int hf_average;
   int tapset_decision;

   int prefilter_period;
   opus_val16 prefilter_gain;
   int prefilter_tapset;

   int consec_transient;

   opus_val32 preemph_memE[2];
   opus_val32 preemph_memD[2];

   /* VBR-related parameters */
   opus_int32 vbr_reservoir;
   opus_int32 vbr_drift;
   opus_int32 vbr_offset;
   opus_int32 vbr_count;

   celt_sig in_mem[1]; /* Size = channels*mode->overlap */
   /* celt_sig prefilter_mem[],  Size = channels*COMBFILTER_PERIOD */
   /* celt_sig overlap_mem[],  Size = channels*mode->overlap */
   /* opus_val16 oldEBands[], Size = 2*channels*mode->nbEBands */
}

/** Decoder state
 @brief Decoder state
 */
struct OpusCustomDecoder {
         OpusCustomMode *mode;
   int overlap;
   int channels;
   int stream_channels;

   int downsample;
   int start, _end;
   int signalling;

   /* Everything beyond this point gets cleared on a reset */

   opus_uint32 rng;
   int error;
   int last_pitch_index;
   int loss_count;
   int postfilter_period;
   int postfilter_period_old;
   opus_val16 postfilter_gain;
   opus_val16 postfilter_gain_old;
   int postfilter_tapset;
   int postfilter_tapset_old;

   celt_sig preemph_memD[2];

   celt_sig _decode_mem[1]; /* Size = channels*(DECODE_BUFFER_SIZE+mode->overlap) */
   /* opus_val16 lpc[],  Size = channels*LPC_ORDER */
   /* opus_val16 oldEBands[], Size = 2*mode->nbEBands */
   /* opus_val16 oldLogE[], Size = 2*mode->nbEBands */
   /* opus_val16 oldLogE2[], Size = 2*mode->nbEBands */
   /* opus_val16 backgroundLogE[], Size = 2*mode->nbEBands */
}


int resampling_factor(opus_int32 rate)
{
   int ret;
   switch (rate)
   {
   case 48000:
      ret = 1;
      break;
   case 24000:
      ret = 2;
      break;
   case 16000:
      ret = 3;
      break;
   case 12000:
      ret = 4;
      break;
   case 8000:
      ret = 6;
      break;
   default:

      ;
      ret = 0;
      break;
   }
   return ret;
}



public int celt_encoder_get_size(int channels)
{
   OpusCustomMode *mode = opus_custom_mode_create(48000, 960, null);
   return opus_custom_encoder_get_size(mode, channels);
}

public
int opus_custom_encoder_get_size(      OpusCustomMode *mode, int channels)
{
   int size = ((int)(OpusCustomEncoder ' size  ))
         + (2*channels*mode->overlap-1)*((int)(celt_sig ' size  ))
         + channels*1024*((int)(celt_sig ' size  ))
         + 3*channels*mode->nbEBands*((int)(opus_val16 ' size  ));
   return size;
}

public int celt_encoder_init(OpusCustomEncoder *st, opus_int32 sampling_rate, int channels)
{
   int ret;
   ret = opus_custom_encoder_init(st, opus_custom_mode_create(48000, 960, null), channels);
   if (ret != 0)
      return ret;
   st->upsample = resampling_factor(sampling_rate);
   return 0;
}

public
int opus_custom_encoder_init(OpusCustomEncoder *st,       OpusCustomMode *mode, int channels)
{
   if (channels < 0 || channels > 2)
      return -1;

   if (st==null || mode==null)
      return -7;

   memset(((char*)st), 0, (opus_custom_encoder_get_size(mode, channels))*((int)((*((char*)st)) ' size  )));

   st->mode = mode;
   st->overlap = mode->overlap;
   st->stream_channels = channels;
   st->channels = channels;

   st->upsample = 1;
   st->start = 0;
   st->_end = st->mode->effEBands;
   st->signalling = 1;

   st->constrained_vbr = 1;
   st->clip = 1;

   st->bitrate = -1;
   st->vbr = 0;
   st->force_intra  = 0;
   st->complexity = 5;
   st->lsb_depth=24;

   opus_custom_encoder_ctl(st, 4028, (int)0);

   return 0;
}

opus_val16 SIG2WORD16(celt_sig x)
{
   return (opus_val16)x;
}

int transient_analysis(      opus_val32 *  in, int len, int C,
                              int overlap)
{
   int i;
   opus_val16 *tmp;
   opus_val32 mem0=0.0,mem1=0.0;
   int is_transient = 0;
   int block;
   int N;
   opus_val16 *bins;
   ;
   tmp = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(len)));

   block = overlap/2;
   N=len/block;
   bins = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(N)));
   if (C==1)
   {
      for (i=0;i<len;i++)
         tmp[i] = (in[i]);
   } else {
      for (i=0;i<len;i++)
         tmp[i] = (((in[i])+(in[i+len])));
   }

   /* High-pass filter: (1 - 2*z^-1 + z^-2) / (1 - z^-1 + .5*z^-2) */
   for (i=0;i<len;i++)
   {
      opus_val32 x,y;
      x = tmp[i];
      y = ((mem0)+(x));

      mem0 = mem1 + y - 2.0*x;
      mem1 = x - 0.5f*y;
      tmp[i] = ((y));
   }
   /* First few samples are bad because we don't propagate the memory */
   for (i=0;i<12;i++)
      tmp[i] = 0.0;

   for (i=0;i<N;i++)
   {
      int j;
      opus_val16 max_abs=0.0;
      for (j=0;j<block;j++)
         max_abs = ((max_abs) > (((tmp[i*block+j]) < 0.0 ? (-(tmp[i*block+j])) : (tmp[i*block+j]))) ? (max_abs) : (((tmp[i*block+j]) < 0.0 ? (-(tmp[i*block+j])) : (tmp[i*block+j]))));
      bins[i] = max_abs;
   }
   for (i=0;i<N;i++)
   {
      int j;
      int conseq=0;
      opus_val16 t1, t2, t3;

      t1 = (((0.15f))*(bins[i]));
      t2 = (((0.4f))*(bins[i]));
      t3 = (((0.15f))*(bins[i]));
      for (j=0;j<i;j++)
      {
         if (bins[j] < t1)
            conseq++;
         if (bins[j] < t2)
            conseq++;
         else
            conseq = 0;
      }
      if (conseq>=3)
         is_transient=1;
      conseq = 0;
      for (j=i+1;j<N;j++)
      {
         if (bins[j] < t3)
            conseq++;
         else
            conseq = 0;
      }
      if (conseq>=7)
         is_transient=1;
   }
   afree((byte*)tmp);
   afree((byte*)bins);
   ;

   return is_transient;
}

/** Apply window and compute the MDCT for all sub-frames and
    all channels in a frame */
void compute_mdcts(      OpusCustomMode *mode, int shortBlocks, celt_sig *  in, celt_sig *  _out, int C, int LM)
{
   if (C==1 && shortBlocks==0)
   {
      int overlap = ((mode)->overlap);
      clt_mdct_forward(&mode->mdct, in, _out, &mode->window, overlap, mode->maxLM-LM, 1);
   } else {
      int overlap = ((mode)->overlap);
      int N = mode->shortMdctSize<<LM;
      int B = 1;
      int b, c;
      if (shortBlocks != 0)
      {
         N = mode->shortMdctSize;
         B = shortBlocks;
      }
      c=0;
	 for (;;)
	 {
         for (b=0;b<B;b++)
         {
            /* Interleaving the sub-frames while doing the MDCTs */
            clt_mdct_forward(&mode->mdct, in+c*(B*N+overlap)+b*N, &_out[b+c*N*B], &mode->window, overlap, shortBlocks!=0 ? mode->maxLM : mode->maxLM-LM, B);
         }
       if (!(++c<C)) break;
	  }
   }
}

/** Compute the IMDCT and apply window for all sub-frames and
    all channels in a frame */
void compute_inv_mdcts(      OpusCustomMode *mode, int shortBlocks, celt_sig *X,
      celt_sig *  out_mem[],
      celt_sig *  overlap_mem[], int C, int LM)
{
   int c;
   int N = mode->shortMdctSize<<LM;
   int overlap = ((mode)->overlap);
   opus_val32 *x;
   ;

   x = ((opus_val32*)malloc(((int)(opus_val32 ' size  ))*(N+overlap)));
   c=0;
   for (;;)
   {
      int j;
      int b;
      int N2 = N;
      int B = 1;

      if (shortBlocks != 0)
      {
         N2 = mode->shortMdctSize;
         B = shortBlocks;
      }
      /* Prevents problems from the imdct doing the overlap-add */
      memset((char*)(x), 0, (overlap)*((int)((*(x)) ' size  )));

      for (b=0;b<B;b++)
      {
         /* IMDCT on the interleaved the sub-frames */
         clt_mdct_backward(&mode->mdct, &X[b+c*N2*B], x+N2*b, &mode->window, overlap, shortBlocks!=0 ? mode->maxLM : mode->maxLM-LM, B);
      }

      for (j=0;j<overlap;j++)
         out_mem[c][j] = x[j] + overlap_mem[c][j];
      for (;j<N;j++)
         out_mem[c][j] = x[j];
      for (j=0;j<overlap;j++)
         overlap_mem[c][j] = x[N+j];
     if (!(++c<C)) break;
   }
   afree((byte*)x);
   ;
}

void deemphasis(celt_sig *in[], opus_val16 *pcm, int N, int C, int downsample,       opus_val16 *coef, celt_sig *mem)
{
   int c;
   int count=0;
   c=0;
   for (;;)
   {
      int j;
      celt_sig *  x;
      opus_val16  *  y;
      celt_sig m = mem[c];
      x =in[c];
      y = pcm+c;
      for (j=0;j<N;j++)
      {
         celt_sig tmp = *x + m;
         m = ((coef[0])*(tmp))
           - ((coef[1])*(*x));
         tmp = (((coef[3])*(tmp)));
         x++;
         /* Technically the store could be moved outside of the if because
            the stores we don't want will just be overwritten */
         if (count==0)
            *y = ((SIG2WORD16(tmp))*(1.0f/32768.0f));
         if (++count==downsample)
         {
            y+=C;
            count=0;
         }
      }
      mem[c] = m;
      if (!(++c<C)) break;
   }
}

void comb_filter(opus_val32 *y, opus_val32 *x, int T0, int T1, int N,
      opus_val16 g0, opus_val16 g1, int tapset0, int tapset1,
            opus_val16 *window, int overlap)
{
   int i;
   /* printf ("%d %d %f %f\n", T0, T1, g0, g1); */
   opus_val16 g00, g01, g02, g10, g11, g12;
   const        opus_val16 gains[3][3] = {
         {(0.3066406250f), (0.2170410156f), (0.1296386719f)},
         {(0.4638671875f), (0.2680664062f), (0.0f)},
         {(0.7998046875f), (0.1000976562f), (0.0f)}};
   g00 = ((g0)*(gains[tapset0][0]));
   g01 = ((g0)*(gains[tapset0][1]));
   g02 = ((g0)*(gains[tapset0][2]));
   g10 = ((g1)*(gains[tapset1][0]));
   g11 = ((g1)*(gains[tapset1][1]));
   g12 = ((g1)*(gains[tapset1][2]));
   for (i=0;i<overlap;i++)
   {
      opus_val16 f;
      f = ((window[i])*(window[i]));
      y[i] = x[i]
               + (((((1.0f-f))*(g00)))*(x[i-T0]))
               + (((((1.0f-f))*(g01)))*(x[i-T0-1]))
               + (((((1.0f-f))*(g01)))*(x[i-T0+1]))
               + (((((1.0f-f))*(g02)))*(x[i-T0-2]))
               + (((((1.0f-f))*(g02)))*(x[i-T0+2]))
               + ((((f)*(g10)))*(x[i-T1]))
               + ((((f)*(g11)))*(x[i-T1-1]))
               + ((((f)*(g11)))*(x[i-T1+1]))
               + ((((f)*(g12)))*(x[i-T1-2]))
               + ((((f)*(g12)))*(x[i-T1+2]));

   }
   for (i=overlap;i<N;i++)
      y[i] = x[i]
               + ((g10)*(x[i-T1]))
               + ((g11)*(x[i-T1-1]))
               + ((g11)*(x[i-T1+1]))
               + ((g12)*(x[i-T1-2]))
               + ((g12)*(x[i-T1+2]));
}

package TAB
const int1 tf_select_table[4][8] = {
      {0, -1, 0, -1,    0,-1, 0,-1},
      {0, -1, 0, -2,    1, 0, 1,-1},
      {0, -2, 0, -3,    2, 0, 1,-1},
      {0, -2, 0, -3,    3, 0, 1,-1},
};
end TAB;

opus_val32 l1_metric(      celt_norm *tmp, int N, int LM, int width)
{
   int i, j;
   const        opus_val16 sqrtM_1[4] = {1.0f, (0.70710678f), (0.5f), (0.35355339f)};
   opus_val32 L1;
   opus_val16 bias;
   L1=0.0;
   for (i=0;i<1<<LM;i++)
   {
      opus_val32 L2 = 0.0;
      for (j=0;j<N>>LM;j++)
         L2 = ((L2)+(opus_val32)(tmp[(j<<LM)+i])*(opus_val32)(tmp[(j<<LM)+i]));
      L1 += ((float)sqrt(L2));
   }
   L1 = ((sqrtM_1[LM])*(L1));
   if (width==1)
      bias = (0.12f)*(float)LM;
   else if (width==2)
      bias = (0.05f)*(float)LM;
   else
      bias = (0.02f)*(float)LM;
   L1 = ((L1)+(bias)*(L1));
   return L1;
}

int tf_analysis(      OpusCustomMode *m, int len, int C, int isTransient,
      int *tf_res, int nbCompressedBytes, celt_norm *X, int N0, int LM,
      int *tf_sum)
{
   int i;
   int *metric;
   int cost0;
   int cost1;
   int *path0;
   int *path1;
   celt_norm *tmp;
   int lambda;
   int tf_select=0;
   ;

   if (nbCompressedBytes<15*C)
   {
      *tf_sum = 0;
      for (i=0;i<len;i++)
         tf_res[i] = isTransient;
      return 0;
   }
   if (nbCompressedBytes<40)
      lambda = 12;
   else if (nbCompressedBytes<60)
      lambda = 6;
   else if (nbCompressedBytes<100)
      lambda = 4;
   else
      lambda = 3;

   metric = ((int*)malloc(((int)(int ' size  ))*(len)));
   tmp = ((celt_norm*)malloc(((int)(celt_norm ' size  ))*((m->eBands[len]-m->eBands[len-1])<<LM)));
   path0 = ((int*)malloc(((int)(int ' size  ))*(len)));
   path1 = ((int*)malloc(((int)(int ' size  ))*(len)));

   *tf_sum = 0;
   for (i=0;i<len;i++)
   {
      int j, k, N;
      opus_val32 L1, best_L1;
      int best_level=0;
      N = (m->eBands[i+1]-m->eBands[i])<<LM;
      for (j=0;j<N;j++)
         tmp[j] = X[j+(m->eBands[i]<<LM)];
      /* Just add the right channel if we're in stereo */
      if (C==2)
         for (j=0;j<N;j++)
            tmp[j] = (((tmp[j]))+((X[N0+j+(m->eBands[i]<<LM)])));
      L1 = l1_metric(tmp, N, isTransient!=0 ? LM : 0, N>>LM);
      best_L1 = L1;
      /*printf ("%f ", L1);*/
      for (k=0;k<LM;k++)
      {
         int B;

         if (isTransient != 0)
            B = (LM-k-1);
         else
            B = k+1;

         if (isTransient != 0)
            haar1(tmp, N>>(LM-k), 1<<(LM-k));
         else
            haar1(tmp, N>>k, 1<<k);

         L1 = l1_metric(tmp, N, B, N>>LM);

         if (L1 < best_L1)
         {
            best_L1 = L1;
            best_level = k+1;
         }
      }
      /*printf ("%d ", isTransient ? LM-best_level : best_level);*/
      if (isTransient != 0)
         metric[i] = best_level;
      else
         metric[i] = -best_level;
      *tf_sum += metric[i];
   }
   /*printf("\n");*/
   /* NOTE: Future optimized implementations could detect extreme transients and set
      tf_select = 1 but so far we have not found a reliable way of making this useful */
   tf_select = 0;

   cost0 = 0;
   cost1 = isTransient != 0 ? 0 : lambda;
   /* Viterbi forward pass */
   for (i=1;i<len;i++)
   {
      int curr0, curr1;
      int from0, from1;

      from0 = cost0;
      from1 = cost1 + lambda;
      if (from0 < from1)
      {
         curr0 = from0;
         path0[i]= 0;
      } else {
         curr0 = from1;
         path0[i]= 1;
      }

      from0 = cost0 + lambda;
      from1 = cost1;
      if (from0 < from1)
      {
         curr1 = from0;
         path1[i]= 0;
      } else {
         curr1 = from1;
         path1[i]= 1;
      }
      cost0 = curr0 + abs(metric[i]-tf_select_table[LM][4*isTransient+2*tf_select+0]);
      cost1 = curr1 + abs(metric[i]-tf_select_table[LM][4*isTransient+2*tf_select+1]);
   }
   tf_res[len-1] = cost0 < cost1 ? 0 : 1;
   /* Viterbi backward pass to check the decisions */
   for (i=len-2;i>=0;i--)
   {
      if (tf_res[i+1] == 1)
         tf_res[i] = path1[i+1];
      else
         tf_res[i] = path0[i+1];
   }
   
   afree((byte*)metric);
   afree((byte*)tmp);
   afree((byte*)path0);
   afree((byte*)path1);
   
   ;

   return tf_select;
}

void tf_encode(int start, int _end, int isTransient, int *tf_res, int LM, int tf_select, ec_enc *enc)
{
   int curr, i;
   int tf_select_rsv;
   int tf_changed;
   int logp;
   opus_uint32 budget;
   opus_uint32 tell;
   budget = enc->storage*8;
   tell = (uint)ec_tell(enc);
   logp = isTransient!=0 ? 2 : 4;
   /* Reserve space to code the tf_select decision. */
   tf_select_rsv = (int)(LM>0 && tell+(uint)logp+1 <= budget);
   budget -= (uint)tf_select_rsv;
   curr = 0;
   tf_changed = 0;
   for (i=start;i<_end;i++)
   {
      if (tell+(uint)logp<=budget)
      {
         ec_enc_bit_logp(enc, tf_res[i] ^ curr, (uint)logp);
         tell = (uint)ec_tell(enc);
         curr = tf_res[i];
         tf_changed |= curr;
      }
      else
         tf_res[i] = curr;
      logp = isTransient!=0 ? 4 : 5;
   }
   /* Only code tf_select if it would actually make a difference. */
   if (tf_select_rsv!=0 &&
         tf_select_table[LM][4*isTransient+0+tf_changed]!=
         tf_select_table[LM][4*isTransient+2+tf_changed])
      ec_enc_bit_logp(enc, tf_select, 1);
   else
      *&tf_select = 0;
   for (i=start;i<_end;i++)
      tf_res[i] = tf_select_table[LM][4*isTransient+2*tf_select+tf_res[i]];
   /*printf("%d %d ", isTransient, tf_select); for(i=0;i<end;i++)printf("%d ", tf_res[i]);printf("\n");*/
}

void tf_decode(int start, int _end, int isTransient, int *tf_res, int LM, ec_dec *dec)
{
   int i, curr, tf_select;
   int tf_select_rsv;
   int tf_changed;
   int logp;
   opus_uint32 budget;
   opus_uint32 tell;

   budget = dec->storage*8;
   tell = (uint)ec_tell(dec);
   logp = isTransient!=0 ? 2 : 4;
   tf_select_rsv = (int)(LM>0 && tell+(uint)logp+1<=budget);
   budget -= (uint)tf_select_rsv;
   tf_changed = 0;
   curr = 0;
   for (i=start;i<_end;i++)
   {
      if (tell+(uint)logp<=budget)
      {
         curr ^= ec_dec_bit_logp(dec, (uint)logp);
         tell = (uint)ec_tell(dec);
         tf_changed |= curr;
      }
      tf_res[i] = curr;
      logp = isTransient!=0 ? 4 : 5;
   }
   tf_select = 0;
   if (tf_select_rsv!=0 &&
     tf_select_table[LM][4*isTransient+0+tf_changed] !=
     tf_select_table[LM][4*isTransient+2+tf_changed])
   {
      tf_select = ec_dec_bit_logp(dec, 1);
   }
   for (i=start;i<_end;i++)
   {
      tf_res[i] = tf_select_table[LM][4*isTransient+2*tf_select+tf_res[i]];
   }
}

void init_caps(      OpusCustomMode *m,int *cap,int LM,int C)
{
   int i;
   for (i=0;i<m->nbEBands;i++)
   {
      int N;
      N=(m->eBands[i+1]-m->eBands[i])<<LM;
      cap[i] = ((int)m->cache.caps[m->nbEBands*(2*LM+C-1)+i] + 64)*C*N>>2;
   }
}

int alloc_trim_analysis(      OpusCustomMode *m,       celt_norm *X,
            opus_val16 *bandLogE, int _end, int LM, int C, int N0)
{
   int i;
   opus_val32 diff=0.0;
   int c;
   int trim_index = 5;
   if (C==2)
   {
      opus_val16 sum = 0.0; /* Q10 */
      /* Compute inter-channel correlation for low frequencies */
      for (i=0;i<8;i++)
      {
         int j;
         opus_val32 partial = 0.0;
         for (j=m->eBands[i]<<LM;j<m->eBands[i+1]<<LM;j++)
            partial = ((partial)+(opus_val32)(X[j])*(opus_val32)(X[N0+j]));
         sum = ((sum)+(((partial))));
      }
      sum = (((1.0f/8.0f))*(sum));
      /*printf ("%f\n", sum);*/
      if (sum > (0.995f))
         trim_index-=4;
      else if (sum > (0.92f))
         trim_index-=3;
      else if (sum > (0.85f))
         trim_index-=2;
      else if (sum > (0.8f))
         trim_index-=1;
   }

   /* Estimate spectral tilt */
   c=0;
   for (;;)
   {
      for (i=0;i<_end-1;i++)
      {
         diff += bandLogE[i+c*m->nbEBands]*(float)(opus_int32)(2+2*i-m->nbEBands);
      }
      if (!(++c<C)) break;
   }
   /* We divide by two here to avoid making the tilt larger for stereo as a
      result of a bug in the loop above */
   diff /= 2.0*(float)(C*(_end-1));
   /*printf("%f\n", diff);*/
   if (diff > (2.0f))
      trim_index--;
   if (diff > (8.0f))
      trim_index--;
   if (diff < -(4.0f))
      trim_index++;
   if (diff < -(10.0f))
      trim_index++;

   if (trim_index<0)
      trim_index = 0;
   if (trim_index>10)
      trim_index = 10;

   return trim_index;
}

int stereo_analysis(      OpusCustomMode *m,       celt_norm *X,
      int LM, int N0)
{
   int i;
   int thetas;
   opus_val32 sumLR = 1.0e-15f, sumMS = 1.0e-15f;

   /* Use the L1 norm to model the entropy of the L/R signal vs the M/S signal */
   for (i=0;i<13;i++)
   {
      int j;
      for (j=m->eBands[i]<<LM;j<m->eBands[i+1]<<LM;j++)
      {
         opus_val32 L, R, M, S;
         /* We cast to 32-bit first because of the -32768 case */
         L = (X[j]);
         R = (X[N0+j]);
         M = ((L)+(R));
         S = ((L)-(R));
         sumLR = ((sumLR)+(((((L) < 0.0 ? (-(L)) : (L)))+(((R) < 0.0 ? (-(R)) : (R))))));
         sumMS = ((sumMS)+(((((M) < 0.0 ? (-(M)) : (M)))+(((S) < 0.0 ? (-(S)) : (S))))));
      }
   }
   sumMS = (((0.707107f))*(sumMS));
   thetas = 13;
   /* We don't need thetas for lower bands with LM<=1 */
   if (LM<=1)
      thetas -= 8;
   return (int)(((float)((m->eBands[13]<<(LM+1))+thetas)*(sumMS))
                 > ((float)(m->eBands[13]<<(LM+1))*(sumLR)));
}

public int celt_encode_with_ec(OpusCustomEncoder *  st,       opus_val16 * pcm, int frame_size,     byte      *compressed, int nbCompressedBytes, ec_enc *enc)
{
   int i, c, N;
   opus_int32 bits;
   ec_enc _enc;
   celt_sig *in;
   celt_sig *freq;
   celt_norm *X;
   celt_ener *bandE;
   opus_val16 *bandLogE;
   int *fine_quant;
   opus_val16 *error;
   int *pulses;
   int *cap;
   int *offsets;
   int *fine_priority;
   int *tf_res;
       byte      *collapse_masks;
   celt_sig *prefilter_mem;
   opus_val16* oldBandE, oldLogE, oldLogE2;
   int shortBlocks=0;
   int isTransient=0;
   int CC = st->channels;
   int C = st->stream_channels;
   int LM, M;
   int tf_select;
   int nbFilledBytes, nbAvailableBytes;
   int effEnd;
   int codedBands;
   int tf_sum;
   int alloc_trim;
   int pitch_index=15;
   opus_val16 gain1 = 0.0;
   int intensity=0;
   int dual_stereo=0;
   int effectiveBytes;
   opus_val16 pf_threshold;
   int dynalloc_logp;
   opus_int32 vbr_rate;
   opus_int32 total_bits;
   opus_int32 total_boost;
   opus_int32 balance;
   opus_int32 tell;
   int prefilter_tapset=0;
   int pf_on;
   int anti_collapse_rsv;
   int anti_collapse_on=0;
   int silence=0;
   ;

   if (nbCompressedBytes<2 || pcm==null)
     return -1;

   *&frame_size *= st->upsample;
   for (LM=0;LM<=st->mode->maxLM;LM++)
      if (st->mode->shortMdctSize<<LM==frame_size)
         break;
   if (LM>st->mode->maxLM)
      return -1;
   M=1<<LM;
   N = M*st->mode->shortMdctSize;

   prefilter_mem = &st->in_mem+CC*(st->overlap);
   oldBandE = (opus_val16*)(&st->in_mem+CC*(2*st->overlap+1024));
   oldLogE = oldBandE + CC*st->mode->nbEBands;
   oldLogE2 = oldLogE + CC*st->mode->nbEBands;

   if (enc==null)
   {
      tell=1;
      nbFilledBytes=0;
   } else {
      tell=ec_tell(enc);
      nbFilledBytes=(tell+4)>>3;
   }

   ;

   /* Can't produce more than 1275 output bytes */
   *&nbCompressedBytes = ((nbCompressedBytes) < (1275) ? (nbCompressedBytes) : (1275));
   nbAvailableBytes = nbCompressedBytes - nbFilledBytes;

   if (st->vbr != 0 && st->bitrate!=-1)
   {
      opus_int32 den=st->mode->Fs>>3;
      vbr_rate=(st->bitrate*frame_size+(den>>1))/den;

      effectiveBytes = vbr_rate>>(3+3);
   } else {
      opus_int32 tmp;
      vbr_rate = 0;
      tmp = st->bitrate*frame_size;
      if (tell>1)
         tmp += tell;
      if (st->bitrate!=-1)
         *&nbCompressedBytes = ((2) > (((nbCompressedBytes) < ((tmp+4*st->mode->Fs)/(8*st->mode->Fs)-(int)!!(bool)st->signalling) ? (nbCompressedBytes) : ((tmp+4*st->mode->Fs)/(8*st->mode->Fs)-(int)!!(bool)st->signalling))) ? (2) : (((nbCompressedBytes) < ((tmp+4*st->mode->Fs)/(8*st->mode->Fs)-(int)!!(bool)st->signalling) ? (nbCompressedBytes) : ((tmp+4*st->mode->Fs)/(8*st->mode->Fs)-(int)!!(bool)st->signalling))));
      effectiveBytes = nbCompressedBytes;
   }

   if (enc==null)
   {
      ec_enc_init(&_enc, compressed, (uint)nbCompressedBytes);
      *&enc = &_enc;
   }

   if (vbr_rate>0)
   {
      /* Computes the max bit-rate allowed in VBR mode to avoid violating the
          target rate and buffering.
         We must do this up front so that bust-prevention logic triggers
          correctly if we don't have enough bits. */
      if (st->constrained_vbr != 0)
      {
         opus_int32 vbr_bound;
         opus_int32 max_allowed;
         /* We could use any multiple of vbr_rate as bound (depending on the
             delay).
            This is clamped to ensure we use at least two bytes if the encoder
             was entirely empty, but to allow 0 in hybrid mode. */
         vbr_bound = vbr_rate;
         max_allowed = 

((((tell==1?2:0) > ((vbr_rate+vbr_bound-st->vbr_reservoir)>>(3+3)) ? (tell==1?2:0) : ((vbr_rate+vbr_bound-st->vbr_reservoir)>>(3+3)))) < (nbAvailableBytes) ? (((tell==1?2:0) > ((vbr_rate+vbr_bound-st->vbr_reservoir)>>(3+3)) ? (tell==1?2:0) : ((vbr_rate+vbr_bound-st->vbr_reservoir)>>(3+3)))) : (nbAvailableBytes));
         if(max_allowed < nbAvailableBytes)
         {
            *&nbCompressedBytes = nbFilledBytes+max_allowed;
            nbAvailableBytes = max_allowed;
            ec_enc_shrink(enc, (uint)nbCompressedBytes);
         }
      }
   }
   total_bits = nbCompressedBytes*8;

   effEnd = st->_end;
   if (effEnd > st->mode->effEBands)
      effEnd = st->mode->effEBands;

   in = ((celt_sig*)malloc(((int)(celt_sig ' size  ))*(CC*(N+st->overlap))));

   /* Find pitch period and gain */
   {
      celt_sig *_pre;
      celt_sig *pre[2];
      
	  clear pre;
	  ;
      _pre = ((celt_sig*)malloc(((int)(celt_sig ' size  ))*(CC*(N+1024))));

      pre[0] = _pre;
      pre[1] = _pre + (N+1024);

      silence = 1;
      c=0;
	  for (;;)
	  {
         int count = 0;
         opus_val16 *  pcmp = pcm+c;
         celt_sig *  inp = in+c*(N+st->overlap)+st->overlap;

         for (i=0;i<N;i++)
         {
            celt_sig x, tmp;

            x = ((*pcmp)*32768.0f);

            if (!(x==x))
               x = 0.0;
            if (st->clip != 0)
               x = ((-65536.0f) > (((65536.0f) < (x) ? (65536.0f) : (x))) ? (-65536.0f) : (((65536.0f) < (x) ? (65536.0f) : (x))));
            if (++count==st->upsample)
            {
               count=0;
               pcmp+=CC;
            } else {
               x = 0.0;
            }
            /* Apply pre-emphasis */
            tmp = ((opus_val32)(st->mode->preemph[2])*(opus_val32)(x));
            *inp = tmp + st->preemph_memE[c];
            st->preemph_memE[c] = ((st->mode->preemph[1])*(*inp))
                                   - ((st->mode->preemph[0])*(tmp));
            silence = (int)(silence != 0 && *inp == 0.0);
            inp++;
         }
         memcpy((byte*)(pre[c]), (byte*)(prefilter_mem+c*1024), (1024)*((int)((*(pre[c])) ' size  )) + (int)(0*((pre[c])-(prefilter_mem+c*1024))) );
         memcpy((byte*)(pre[c]+1024), (byte*)(in+c*(N+st->overlap)+st->overlap), (N)*((int)((*(pre[c]+1024)) ' size  )) + (int)(0*((pre[c]+1024)-(in+c*(N+st->overlap)+st->overlap)) ));
        if (!(++c<CC)) break;
      }
      if (tell==1)
         ec_enc_bit_logp(enc, silence, 15);
      else
         silence=0;
      if (silence != 0)
      {
         /*In VBR mode there is no need to send more than the minimum. */
         if (vbr_rate>0)
         {
            *&nbCompressedBytes=((nbCompressedBytes) < (nbFilledBytes+2) ? (nbCompressedBytes) : (nbFilledBytes+2));
			effectiveBytes=nbCompressedBytes;
            total_bits=nbCompressedBytes*8;
            nbAvailableBytes=2;
            ec_enc_shrink(enc, (uint)nbCompressedBytes);
         }
         /* Pretend we've filled all the remaining bits with zeros
            (that's what the initialiser did anyway) */
         tell = nbCompressedBytes*8;
         enc->nbits_total+=tell-ec_tell(enc);
      }
      if (nbAvailableBytes>12*C && st->start==0 && silence==0 && st->disable_pf==0 && st->complexity >= 5)
      {
         opus_val16 *pitch_buf;
         pitch_buf = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*((1024+N)>>1)));

         pitch_downsample(pre, pitch_buf, 1024+N, CC);
         pitch_search(pitch_buf+(1024>>1), pitch_buf, N,
               1024-15, &pitch_index);
         pitch_index = 1024-pitch_index;

         gain1 = remove_doubling(pitch_buf, 1024, 15,
               N, &pitch_index, st->prefilter_period, st->prefilter_gain);
         if (pitch_index > 1024-2)
            pitch_index = 1024-2;
         gain1 = (((0.7f))*(gain1));
         if (st->loss_rate>2)
            gain1 = (0.5f*(gain1));
         if (st->loss_rate>4)
            gain1 = (0.5f*(gain1));
         if (st->loss_rate>8)
            gain1 = 0.0;
         prefilter_tapset = st->tapset_decision;
		 
		 afree((byte*)pitch_buf);
      } else {
         gain1 = 0.0;
      }

      /* Gain threshold for enabling the prefilter/postfilter */
      pf_threshold = (0.2f);

      /* Adjusting the threshold based on rate and continuity */
      if (abs(pitch_index-st->prefilter_period)*10>pitch_index)
         pf_threshold += (0.2f);
      if (nbAvailableBytes<25)
         pf_threshold += (0.1f);
      if (nbAvailableBytes<35)
         pf_threshold += (0.1f);
      if (st->prefilter_gain > (0.4f))
         pf_threshold -= (0.1f);
      if (st->prefilter_gain > (0.55f))
         pf_threshold -= (0.1f);

      /* Hard threshold at 0.2 */
      pf_threshold = ((pf_threshold) > ((0.2f)) ? (pf_threshold) : ((0.2f)));
      if (gain1<pf_threshold)
      {
         if(st->start==0 && tell+16<=total_bits)
            ec_enc_bit_logp(enc, 0, 1);
         gain1 = 0.0;
         pf_on = 0;
      } else {
         /*This block is not gated by a total bits check only because
           of the nbAvailableBytes check above.*/
         int qg;
         int octave;

         if (((gain1-st->prefilter_gain) < 0.0 ? (-(gain1-st->prefilter_gain)) : (gain1-st->prefilter_gain))<(0.1f))
            gain1=st->prefilter_gain;

         qg = (int)floor(0.5f+gain1*32.0/3.0)-1;
         qg = ((0) > (((7) < (qg) ? (7) : (qg))) ? (0) : (((7) < (qg) ? (7) : (qg))));
         ec_enc_bit_logp(enc, 1, 1);
         pitch_index += 1;
         octave = (ec_ilog((uint)pitch_index))-5;
         ec_enc_uint(enc, (uint)octave, 6);
         ec_enc_bits(enc, (uint)(pitch_index-(16<<octave)), (uint)(4+octave));
         pitch_index -= 1;
         ec_enc_bits(enc, (uint)qg, 3);
         if (ec_tell(enc)+2<=total_bits)
            ec_enc_icdf(enc, prefilter_tapset, (byte*)&tapset_icdf, 2);
         else
           prefilter_tapset = 0;
         gain1 = (0.09375f)*(float)(qg+1);
         pf_on = 1;
      }
      /*printf("%d %f\n", pitch_index, gain1);*/

      c=0;
	  for (;;)
	  {
         int offset = st->mode->shortMdctSize-st->mode->overlap;
         st->prefilter_period=((st->prefilter_period) > (15) ? (st->prefilter_period) : (15));
         memcpy((byte*)(in+c*(N+st->overlap)), (byte*)(&st->in_mem+c*(st->overlap)), (st->overlap)*((int)((*(in+c*(N+st->overlap))) ' size  )) + (int)(0*((in+c*(N+st->overlap))-(&st->in_mem+c*(st->overlap))) ));
         if (offset != 0)
            comb_filter(in+c*(N+st->overlap)+st->overlap, pre[c]+1024,
                  st->prefilter_period, st->prefilter_period, offset, -st->prefilter_gain, -st->prefilter_gain,
                  st->prefilter_tapset, st->prefilter_tapset, null, 0);

         comb_filter(in+c*(N+st->overlap)+st->overlap+offset, pre[c]+1024+offset,
               st->prefilter_period, pitch_index, N-offset, -st->prefilter_gain, -gain1,
               st->prefilter_tapset, prefilter_tapset, &st->mode->window, st->mode->overlap);
         memcpy((byte*)(&st->in_mem+c*(st->overlap)), (byte*)(in+c*(N+st->overlap)+N), (st->overlap)*((int)((*(&st->in_mem+c*(st->overlap))) ' size  )) + (int)(0*((&st->in_mem+c*(st->overlap))-(in+c*(N+st->overlap)+N)) ));

         if (N>1024)
         {
            memmove((byte*)(prefilter_mem+c*1024), (byte*)(pre[c]+N), (1024)*((int)((*(prefilter_mem+c*1024)) ' size  )) + (int)(0*((prefilter_mem+c*1024)-(pre[c]+N)) ));
         } else {
            memmove((byte*)(prefilter_mem+c*1024), (byte*)(prefilter_mem+c*1024+N), (1024-N)*((int)((*(prefilter_mem+c*1024)) ' size  )) + (int)(0*((prefilter_mem+c*1024)-(prefilter_mem+c*1024+N)) ));
            memmove((byte*)(prefilter_mem+c*1024+1024-N), (byte*)(pre[c]+1024), (N)*((int)((*(prefilter_mem+c*1024+1024-N)) ' size  )) + (int)(0*((prefilter_mem+c*1024+1024-N)-(pre[c]+1024)) ));
         }
       if (!(++c<CC)) break;
      }
	  
	  afree((byte*)_pre);
      ;
   }

   isTransient = 0;
   shortBlocks = 0;
   if (LM>0 && ec_tell(enc)+3<=total_bits)
   {
      if (st->complexity > 1)
      {
         isTransient = transient_analysis(in, N+st->overlap, CC,
                  st->overlap);
         if (isTransient != 0)
            shortBlocks = M;
      }
      ec_enc_bit_logp(enc, isTransient, 3);
   }

   freq = ((celt_sig*)malloc(((int)(celt_sig ' size  ))*(CC*N))); /**< Interleaved signal MDCTs */
   bandE = ((celt_ener*)malloc(((int)(celt_ener ' size  ))*(st->mode->nbEBands*CC)));
   bandLogE = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(st->mode->nbEBands*CC)));
   
   /* Compute MDCTs */
   compute_mdcts(st->mode, shortBlocks, in, freq, CC, LM);

   if (CC==2&&C==1)
   {
      for (i=0;i<N;i++)
         freq[i] = (((0.5f*(freq[i])))+((0.5f*(freq[N+i]))));
   }
   if (st->upsample != 1)
   {
      c=0;
	  for (;;)
      {
         int bound = N/st->upsample;
         for (i=0;i<bound;i++)
            freq[c*N+i] *= (celt_sig)st->upsample;
         for (;i<N;i++)
            freq[c*N+i] = 0.0;
        if (!(++c<C)) break;
	  }
   }
   X = ((celt_norm*)malloc(((int)(celt_norm ' size  ))*(C*N)));         /**< Interleaved normalised MDCTs */

   compute_band_energies(st->mode, freq, bandE, effEnd, C, M);

   amp2Log2(st->mode, effEnd, st->_end, bandE, bandLogE, C);

   /* Band normalisation */
   normalise_bands(st->mode, freq, X, bandE, effEnd, C, M);

   tf_res = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));
   tf_select = tf_analysis(st->mode, effEnd, C, isTransient, tf_res, effectiveBytes, X, N, LM, &tf_sum);
   for (i=effEnd;i<st->_end;i++)
      tf_res[i] = tf_res[effEnd-1];

   error = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(C*st->mode->nbEBands)));
   quant_coarse_energy(st->mode, st->start, st->_end, effEnd, bandLogE,
         oldBandE, (uint)total_bits, error, enc,
         C, LM, nbAvailableBytes, st->force_intra,
         &st->delayedIntra, (int)(st->complexity >= 4), st->loss_rate);

   tf_encode(st->start, st->_end, isTransient, tf_res, LM, tf_select, enc);

   st->spread_decision = (2);
   if (ec_tell(enc)+4<=total_bits)
   {
      if (shortBlocks != 0 || st->complexity < 3 || nbAvailableBytes < 10*C)
      {
         if (st->complexity == 0)
            st->spread_decision = (0);
      } else {
         st->spread_decision = spreading_decision(st->mode, X,
               &st->tonal_average, st->spread_decision, &st->hf_average,
               &st->tapset_decision, (int)(pf_on != 0 && shortBlocks == 0), effEnd, C, M);
      }
      ec_enc_icdf(enc, st->spread_decision, &spread_icdf, 5);
   }

   cap = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));
   offsets = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));

   init_caps(st->mode,cap,LM,C);
   for (i=0;i<st->mode->nbEBands;i++)
      offsets[i] = 0;
   /* Dynamic allocation code */
   /* Make sure that dynamic allocation can't make us bust the budget */
   if (effectiveBytes > 50 && LM>=1)
   {
      int t1, t2;
      if (LM <= 1)
      {
         t1 = 3;
         t2 = 5;
      } else {
         t1 = 2;
         t2 = 4;
      }
      for (i=st->start+1;i<st->_end-1;i++)
      {
         opus_val32 d2;
         d2 = 2.0f*bandLogE[i]-bandLogE[i-1]-bandLogE[i+1];
         if (C==2)
            d2 = (0.5f*(d2 + 2.0*bandLogE[i+st->mode->nbEBands]- bandLogE[i-1+st->mode->nbEBands]-bandLogE[i+1+st->mode->nbEBands]));

         if (d2 > (opus_val32)(t1))
            offsets[i] += 1;
         if (d2 > (opus_val32)(t2))
            offsets[i] += 1;
      }
   }
   dynalloc_logp = 6;
   total_bits<<=3;
   total_boost = 0;
   tell = (opus_int32)ec_tell_frac(enc);
   for (i=st->start;i<st->_end;i++)
   {
      int width, quanta;
      int dynalloc_loop_logp;
      int boost;
      int j;
      width = C*(st->mode->eBands[i+1]-st->mode->eBands[i])<<LM;
      /* quanta is 6 bits, but no more than 1 bit/sample
         and no less than 1/8 bit/sample */
      quanta = ((width<<3) < (((6<<3) > (width) ? (6<<3) : (width))) ? (width<<3) : (((6<<3) > (width) ? (6<<3) : (width))));
      dynalloc_loop_logp = dynalloc_logp;
      boost = 0;
      for (j = 0; tell+(dynalloc_loop_logp<<3) < total_bits-total_boost
            && boost < cap[i]; j++)
      {
         int flag;
         flag = (int)(j<offsets[i]);
         ec_enc_bit_logp(enc, flag, (uint)dynalloc_loop_logp);
         tell = (opus_int32)ec_tell_frac(enc);
         if (!(bool)flag)
            break;
         boost += quanta;
         total_boost += quanta;
         dynalloc_loop_logp = 1;
      }
      /* Making dynalloc more likely */
      if (j != 0)
         dynalloc_logp = ((2) > (dynalloc_logp-1) ? (2) : (dynalloc_logp-1));
      offsets[i] = boost;
   }
   alloc_trim = 5;
   if (tell+(6<<3) <= total_bits - total_boost)
   {
      alloc_trim = alloc_trim_analysis(st->mode, X, bandLogE,
            st->_end, LM, C, N);
      ec_enc_icdf(enc, alloc_trim, (byte*)&trim_icdf, 7);
      tell = (opus_int32)ec_tell_frac(enc);
   }

   /* Variable bitrate */
   if (vbr_rate>0)
   {
     opus_val16 alpha;
     opus_int32 delta;
     /* The target rate in 8th bits per frame */
     opus_int32 target;
     opus_int32 min_allowed;
     int lm_diff = st->mode->maxLM - LM;

     /* Don't attempt to use more than 510 kb/s, even for frames smaller than 20 ms.
        The CELT allocator will just not be able to use more than that anyway. */
     *&nbCompressedBytes = ((nbCompressedBytes) < (1275>>(3-LM)) ? (nbCompressedBytes) : (1275>>(3-LM)));
     target = vbr_rate + (st->vbr_offset>>lm_diff) - ((40*C+20)<<3);

     /* Shortblocks get a large boost in bitrate, but since they
        are uncommon long blocks are not greatly affected */
     if (shortBlocks != 0 || tf_sum < -2*(st->_end-st->start))
        target = 7*target/4;
     else if (tf_sum < -(st->_end-st->start))
        target = 3*target/2;
     else if (M > 1)
        target-=(target+14)/28;

     /* The current offset is removed from the target and the space used
        so far is added*/
     target=target+tell;

     /* In VBR mode the frame size must not be reduced so much that it would
         result in the encoder running out of bits.
        The margin of 2 bytes ensures that none of the bust-prevention logic
         in the decoder will have triggered so far. */
     min_allowed = ((tell+total_boost+(1<<(3+3))-1)>>(3+3)) + 2 - nbFilledBytes;

     nbAvailableBytes = (target+(1<<(3+2)))>>(3+3);
     nbAvailableBytes = ((min_allowed) > (nbAvailableBytes) ? (min_allowed) : (nbAvailableBytes));
     nbAvailableBytes = ((nbCompressedBytes) < (nbAvailableBytes+nbFilledBytes) ? (nbCompressedBytes) : (nbAvailableBytes+nbFilledBytes)) - nbFilledBytes;

     /* By how much did we "miss" the target on that frame */
     delta = target - vbr_rate;

     target=nbAvailableBytes<<(3+3);

     /*If the frame is silent we don't adjust our drift, otherwise
       the encoder will shoot to very high rates after hitting a
       span of silence, but we do allow the bitres to refill.
       This means that we'll undershoot our target in CVBR/VBR modes
       on files with lots of silence. */
     if(silence != 0)
     {
       nbAvailableBytes = 2;
       target = 2*8<<3;
       delta = 0;
     }

     if (st->vbr_count < 970)
     {
        st->vbr_count++;
        alpha = (1.0f/(float)(((st->vbr_count+20))));
     } else
        alpha = (0.001f);
     /* How many bits have we used in excess of what we're allowed */
     if (st->constrained_vbr != 0)
        st->vbr_reservoir += target - vbr_rate;
     /*printf ("%d\n", st->vbr_reservoir);*/

     /* Compute the offset we need to apply in order to reach the target */
     st->vbr_drift += (opus_int32)((alpha)*(float)((delta*(1<<lm_diff))-st->vbr_offset-st->vbr_drift));
     st->vbr_offset = -st->vbr_drift;
     /*printf ("%d\n", st->vbr_drift);*/

     if (st->constrained_vbr != 0 && st->vbr_reservoir < 0)
     {
        /* We're under the min value -- increase rate */
        int adjust = (-st->vbr_reservoir)/(8<<3);
        /* Unless we're just coding silence */
        nbAvailableBytes += silence!=0?0:adjust;
        st->vbr_reservoir = 0;
        /*printf ("+%d\n", adjust);*/
     }
     *&nbCompressedBytes = ((nbCompressedBytes) < (nbAvailableBytes+nbFilledBytes) ? (nbCompressedBytes) : (nbAvailableBytes+nbFilledBytes));
     /* This moves the raw bits to take into account the new compressed size */
     ec_enc_shrink(enc, (uint)nbCompressedBytes);
   }
   if (C==2)
   {
      int effectiveRate;

      /* Always use MS for 2.5 ms frames until we can do a better analysis */
      if (LM!=0)
         dual_stereo = stereo_analysis(st->mode, X, LM, N);

      /* Account for coarse energy */
      effectiveRate = (8*effectiveBytes - 80)>>LM;

      /* effectiveRate in kb/s */
      effectiveRate = 2*effectiveRate/5;
      if (effectiveRate<35)
         intensity = 8;
      else if (effectiveRate<50)
         intensity = 12;
      else if (effectiveRate<68)
         intensity = 16;
      else if (effectiveRate<84)
         intensity = 18;
      else if (effectiveRate<102)
         intensity = 19;
      else if (effectiveRate<130)
         intensity = 20;
      else
         intensity = 100;
      intensity = ((st->_end) < (((st->start) > (intensity) ? (st->start) : (intensity))) ? (st->_end) : (((st->start) > (intensity) ? (st->start) : (intensity))));
   }

   /* Bit allocation */
   fine_quant = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));
   pulses = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));
   fine_priority = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));

   /* bits =           packet size                    - where we are - safety*/
   bits = (((opus_int32)nbCompressedBytes*8)<<3) - (int)ec_tell_frac(enc) - 1;
   anti_collapse_rsv = isTransient!=0&&LM>=2&&bits>=((LM+2)<<3) ? (1<<3) : 0;
   bits -= anti_collapse_rsv;
   codedBands = compute_allocation(st->mode, st->start, st->_end, offsets, cap,
         alloc_trim, &intensity, &dual_stereo, bits, &balance, pulses,
         fine_quant, fine_priority, C, LM, enc, 1, st->lastCodedBands);
   st->lastCodedBands = codedBands;

   quant_fine_energy(st->mode, st->start, st->_end, oldBandE, error, fine_quant, enc, C);

   /* Residual quantisation */
   collapse_masks = ((    byte     *)malloc(((int)(    byte      ' size  ))*(C*st->mode->nbEBands)));
   quant_all_bands(1, st->mode, st->start, st->_end, X, C==2 ? X+N : null, collapse_masks,
         bandE, pulses, shortBlocks, st->spread_decision, dual_stereo, intensity, tf_res,
         nbCompressedBytes*(8<<3)-anti_collapse_rsv, balance, enc, LM, codedBands, &st->rng);

   if (anti_collapse_rsv > 0)
   {
      anti_collapse_on = (int)(st->consec_transient<2);

      ec_enc_bits(enc, (uint)anti_collapse_on, 1);
   }
   quant_energy_finalise(st->mode, st->start, st->_end, oldBandE, error, fine_quant, fine_priority, nbCompressedBytes*8-ec_tell(enc), enc, C);

   if (silence != 0)
   {
      for (i=0;i<C*st->mode->nbEBands;i++)
         oldBandE[i] = -(28.0f);
   }

   st->prefilter_period = pitch_index;
   st->prefilter_gain = gain1;
   st->prefilter_tapset = prefilter_tapset;

   if (CC==2&&C==1) {
      for (i=0;i<st->mode->nbEBands;i++)
         oldBandE[st->mode->nbEBands+i]=oldBandE[i];
   }

   if (isTransient == 0)
   {
      for (i=0;i<CC*st->mode->nbEBands;i++)
         oldLogE2[i] = oldLogE[i];
      for (i=0;i<CC*st->mode->nbEBands;i++)
         oldLogE[i] = oldBandE[i];
   } else {
      for (i=0;i<CC*st->mode->nbEBands;i++)
         oldLogE[i] = ((oldLogE[i]) < (oldBandE[i]) ? (oldLogE[i]) : (oldBandE[i]));
   }
   /* In case start or end were to change */
   c=0;
   for (;;)
   {
      for (i=0;i<st->start;i++)
      {
         oldBandE[c*st->mode->nbEBands+i]=0.0;
         oldLogE2[c*st->mode->nbEBands+i]=-(28.0f);
		 oldLogE[c*st->mode->nbEBands+i]=-(28.0f);
      }
      for (i=st->_end;i<st->mode->nbEBands;i++)
      {
         oldBandE[c*st->mode->nbEBands+i]=0.0;
         oldLogE2[c*st->mode->nbEBands+i]=-(28.0f);
		 oldLogE[c*st->mode->nbEBands+i]=-(28.0f);
      }
      if (!(++c<CC)) break;
   }
   if (isTransient != 0)
      st->consec_transient++;
   else
      st->consec_transient=0;
   st->rng = enc->rng;

   /* If there's any room left (can only happen for very high rates),
      it's already filled with zeros */
   ec_enc_done(enc);

   afree((byte*)collapse_masks);
   afree((byte*)fine_quant);
   afree((byte*)pulses);
   afree((byte*)fine_priority);
   afree((byte*)cap);
   afree((byte*)offsets);
   afree((byte*)error);
   afree((byte*)tf_res);
   afree((byte*)X);
   afree((byte*)freq);
   afree((byte*)bandE);
   afree((byte*)bandLogE);
   afree((byte*)in);
   ;

   if (ec_get_error(enc) != 0)
      return -3;
   else
      return nbCompressedBytes;
}

public
int opus_custom_encoder_ctl(OpusCustomEncoder *  st, int request, byte[] value0)
{
   switch (request)
   {
      case 4010:
      {
//         int value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         if (value<0 || value>10)
           return -1; // goto bad_arg;
         st->complexity = value;
      }
      break;
      case 10010:
      {
//         opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         if (value<0 || value>=st->mode->nbEBands)
           return -1; // goto bad_arg;
         st->start = value;
      }
      break;
      case 10012:
      {
//         opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         if (value<1 || value>st->mode->nbEBands)
           return -1; // goto bad_arg;
         st->_end = value;
      }
      break;
      case 10002:
      {
//         int value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         if (value<0 || value>2)
           return -1; // goto bad_arg;
         st->disable_pf = (int)(value<=1);
         st->force_intra = (int)(value==0);
      }
      break;
      case 4014:
      {
//         int value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         if (value<0 || value>100)
           return -1; // goto bad_arg;
         st->loss_rate = value;
      }
      break;
      case 4020:
      {
//         opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         st->constrained_vbr = value;
      }
      break;
      case 4006:
      {
//         opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         st->vbr = value;
      }
      break;
      case 4002:
      {
//         opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         if (value<=500 && value!=-1)
           return -1; // goto bad_arg;
         value = ((value) < (260000*st->channels) ? (value) : (260000*st->channels));
         st->bitrate = value;
      }
      break;
      case 10008:
      {
//         opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         if (value<1 || value>2)
           return -1; // goto bad_arg;
         st->stream_channels = value;
      }
      break;
      case 4036:
      {
//          opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
          if (value<8 || value>24)
            return -1; // goto bad_arg;
          st->lsb_depth=value;
      }
      break;
      case 4037:
      {
//          opus_int32 *value = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int* value;
		 value'byte = value0'byte;
          *value=st->lsb_depth;
      }
      break;
      case 4028:
      {
         int i;
         opus_val16* oldBandE, oldLogE, oldLogE2;
         oldBandE = (opus_val16*)(&st->in_mem+st->channels*(2*st->overlap+1024));
         oldLogE = oldBandE + st->channels*st->mode->nbEBands;
         oldLogE2 = oldLogE + st->channels*st->mode->nbEBands;
         

memset(((char*)&st->rng), 0, (opus_custom_encoder_get_size(st->mode, st->channels)- (int)(((char*)&st->rng - (char*)st))*((int)((*((char*)&st->rng)) ' size  ))));
         for (i=0;i<st->channels*st->mode->nbEBands;i++)
         {
  		    oldLogE2[i]=-(28.0f);
			oldLogE[i]=-(28.0f);
		 }
         st->vbr_offset = 0;
         st->delayedIntra = 1.0;
         st->spread_decision = (2);
         st->tonal_average = 256;
         st->hf_average = 0;
         st->tapset_decision = 0;
      }
      break;

      case 10016:
      {
//         opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         st->signalling = value;
      }
      break;
      case 10015:
      {
//         const OpusCustomMode ** value = ( *(const OpusCustomMode** *)((ap += ( (((int)((const OpusCustomMode**) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((const OpusCustomMode**) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         OpusCustomMode** value;
		 value'byte = value0'byte;
         if (value==null)
           return -1; // goto bad_arg;
         *value=st->mode;
      }
      break;
      case 4031:
      {
//         opus_uint32 * value = ( *(opus_uint32 * *)((ap += ( (((int)((opus_uint32 *) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_uint32 *) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         uint* value;
		 value'byte = value0'byte;
         if (value==null)
           return -1; // goto bad_arg;
         *value=st->rng;
      }
      break;
      default:
         return -5; //goto bad_request;
   }
//   ( ap = (va_list)0 );
   return 0;
//bad_arg:
//   ( ap = (va_list)0 );
//   return -1;
//bad_request:
//   ( ap = (va_list)0 );
//   return -5;
}

/**********************************************************************/
/*                                                                    */
/*                             DECODER                                */
/*                                                                    */
/**********************************************************************/



public int celt_decoder_get_size(int channels)
{
   OpusCustomMode* mode = opus_custom_mode_create(48000, 960, null);
   return opus_custom_decoder_get_size(mode, channels);
}

public int opus_custom_decoder_get_size(      OpusCustomMode *mode, int channels)
{
   int size = ((int)(OpusCustomDecoder ' size  ))
            + (channels*(2048+mode->overlap)-1)*((int)(celt_sig ' size  ))
            + channels*24*((int)(opus_val16 ' size  ))
            + 4*2*mode->nbEBands*((int)(opus_val16 ' size  ));
   return size;
}

public int celt_decoder_init(OpusCustomDecoder *st, opus_int32 sampling_rate, int channels)
{
   int ret;
   ret = opus_custom_decoder_init(st, opus_custom_mode_create(48000, 960, null), channels);
   if (ret != 0)
      return ret;
   st->downsample = resampling_factor(sampling_rate);
   if (st->downsample==0)
      return -1;
   else
      return 0;
}

public int opus_custom_decoder_init(OpusCustomDecoder *st,       OpusCustomMode *mode, int channels)
{
   if (channels < 0 || channels > 2)
      return -1;

   if (st==null)
      return -7;

   (memset(((char*)st), 0, (opus_custom_decoder_get_size(mode, channels))*((int)((*((char*)st)) ' size  ))));

   st->mode = mode;
   st->overlap = mode->overlap;
   st->channels = channels;
   st->stream_channels = channels;

   st->downsample = 1;
   st->start = 0;
   st->_end = st->mode->effEBands;
   st->signalling = 1;

   st->loss_count = 0;

   opus_custom_decoder_ctl(st, 4028, (int)0);

   return 0;
}

void celt_decode_lost(OpusCustomDecoder *  st, opus_val16 *  pcm, int N, int LM)
{
   int c;
   int pitch_index;
   int overlap = st->mode->overlap;
   opus_val16 fade = 1.0f;
   int i, len;
   int C = st->channels;
   int offset;
   celt_sig *out_mem[2];
   celt_sig *decode_mem[2];
   celt_sig *overlap_mem[2];
   opus_val16 *lpc;
   opus_val32 *out_syn[2];
   opus_val16* oldBandE, oldLogE, oldLogE2, backgroundLogE;
   ;
   clear decode_mem, out_mem, out_syn, overlap_mem;
   
   c=0;
   for (;;)
   {
      decode_mem[c] = &st->_decode_mem + c*(2048+st->overlap);
      out_mem[c] = decode_mem[c]+2048-1024;
      overlap_mem[c] = decode_mem[c]+2048;
     if (!(++c<C)) break;
   }
   lpc = (opus_val16*)(&st->_decode_mem+(2048+st->overlap)*C);
   oldBandE = lpc+C*24;
   oldLogE = oldBandE + 2*st->mode->nbEBands;
   oldLogE2 = oldLogE + 2*st->mode->nbEBands;
   backgroundLogE = oldLogE2  + 2*st->mode->nbEBands;

   out_syn[0] = out_mem[0]+1024-N;
   if (C==2)
      out_syn[1] = out_mem[1]+1024-N;

   len = N+st->mode->overlap;

   if (st->loss_count >= 5 || st->start!=0)
   {
      /* Noise-based PLC/CNG */
      celt_sig *freq;
      celt_norm *X;
      celt_ener *bandE;
      opus_uint32 seed;
      int effEnd;

      effEnd = st->_end;
      if (effEnd > st->mode->effEBands)
         effEnd = st->mode->effEBands;

      freq = ((celt_sig*)malloc(((int)(celt_sig ' size  ))*(C*N))); /**< Interleaved signal MDCTs */
      X = ((celt_norm*)malloc(((int)(celt_norm ' size  ))*(C*N)));   /**< Interleaved normalised MDCTs */
      bandE = ((celt_ener*)malloc(((int)(celt_ener ' size  ))*(st->mode->nbEBands*C)));

      if (st->loss_count >= 5)
         log2Amp(st->mode, st->start, st->_end, bandE, backgroundLogE, C);
      else {
         /* Energy decay */
         opus_val16 decay = st->loss_count==0 ? (1.5f) : (0.5f);
         c=0;
		 for (;;)
         {
            for (i=st->start;i<st->_end;i++)
               oldBandE[c*st->mode->nbEBands+i] -= decay;
           if (!(++c<C)) break;
		 }
         log2Amp(st->mode, st->start, st->_end, bandE, oldBandE, C);
      }
      seed = st->rng;
      for (c=0;c<C;c++)
      {
         for (i=0;i<(st->mode->eBands[st->start]<<LM);i++)
            X[c*N+i] = 0.0;
         for (i=st->start;i<st->mode->effEBands;i++)
         {
            int j;
            int boffs;
            int blen;
            boffs = N*c+(st->mode->eBands[i]<<LM);
            blen = (st->mode->eBands[i+1]-st->mode->eBands[i])<<LM;
            for (j=0;j<blen;j++)
            {
               seed = celt_lcg_rand(seed);
               X[boffs+j] = (celt_norm)((opus_int32)seed>>20);
            }
            renormalise_vector(X+boffs, blen, 1.0f);
         }
         for (i=(st->mode->eBands[st->_end]<<LM);i<N;i++)
            X[c*N+i] = 0.0;
      }
      st->rng = seed;

      denormalise_bands(st->mode, X, freq, bandE, st->mode->effEBands, C, 1<<LM);

      c=0;
	  for (;;)
	  {
         for (i=0;i<st->mode->eBands[st->start]<<LM;i++)
            freq[c*N+i] = 0.0;
        if (!(++c<C)) break;
	  }
      c=0;
      for (;;)
	  {
         int bound = st->mode->eBands[effEnd]<<LM;
         if (st->downsample!=1)
            bound = ((bound) < (N/st->downsample) ? (bound) : (N/st->downsample));
         for (i=bound;i<N;i++)
            freq[c*N+i] = 0.0;
        if (!(++c<C)) break;
	  }
      compute_inv_mdcts(st->mode, 0, freq, out_syn, overlap_mem, C, LM);
	  
      afree((byte*)freq);
      afree((byte*)X);
      afree((byte*)bandE);
	  
   } else {
      /* Pitch-based PLC */
      if (st->loss_count == 0)
      {
         opus_val16 pitch_buf[2048>>1];
         /* Corresponds to a min pitch of 67 Hz. It's possible to save CPU in this
         search by using only part of the decode buffer */
         int poffset = 720;
         pitch_downsample(decode_mem, &pitch_buf, 2048, C);
         /* Max pitch is 100 samples (480 Hz) */
         pitch_search(&pitch_buf+((poffset)>>1), &pitch_buf, 2048-poffset,
               poffset-100, &pitch_index);
         pitch_index = poffset-pitch_index;
         st->last_pitch_index = pitch_index;
      } else {
         pitch_index = st->last_pitch_index;
         fade = (0.8f);
      }

      c=0;
	  for (;;)
	  {
         opus_val32 *e;
         opus_val16 exc[1024];
         opus_val32 ac[24+1];
         opus_val16 decay = 1.0;
         opus_val32 S1=0.0;
         opus_val16 mem[24];

		 clear mem;
         e = ((opus_val32*)malloc(((int)(opus_val32 ' size  ))*(1024+2*st->mode->overlap)));

         offset = 1024-pitch_index;
         for (i=0;i<1024;i++)
            exc[i] = (out_mem[c][i]);

         if (st->loss_count == 0)
         {
            _celt_autocorr(&exc, &ac, &st->mode->window, st->mode->overlap,
                  24, 1024);

            /* Noise floor -40 dB */

            ac[0] *= 1.0001f;
            /* Lag windowing */
            for (i=1;i<=24;i++)
            {
               /*ac[i] *= exp(-.5*(2*M_PI*.002*i)*(2*M_PI*.002*i));*/

               ac[i] -= ac[i]*(0.008f*(float)i)*(0.008f*(float)i);
            }

            _celt_lpc(lpc+c*24, &ac, 24);
         }
         for (i=0;i<24;i++)
            mem[i] = (out_mem[c][1024-1-i]);
         celt_fir(&exc, lpc+c*24, &exc, 1024, 24, &mem);
         /*for (i=0;i<MAX_PERIOD;i++)printf("%d ", exc[i]); printf("\n");*/
         /* Check if the waveform is decaying (and if so how fast) */
         {
            opus_val32 E1=1.0, E2=1.0;
            int period;
            if (pitch_index <= 1024/2)
               period = pitch_index;
            else
               period = 1024/2;
            for (i=0;i<period;i++)
            {
               E1 += (((opus_val32)(exc[1024-period+i])*(opus_val32)(exc[1024-period+i])));
               E2 += (((opus_val32)(exc[1024-2*period+i])*(opus_val32)(exc[1024-2*period+i])));
            }
            if (E1 > E2)
               E1 = E2;
            decay = ((float)sqrt(((float)((E1))/(E2))));
         }

         /* Copy excitation, taking decay into account */
         for (i=0;i<len+st->mode->overlap;i++)
         {
            opus_val16 tmp;
            if (offset+i >= 1024)
            {
               offset -= pitch_index;
               decay = ((decay)*(decay));
            }
            e[i] = ((((decay)*(exc[offset+i]))));
            tmp = (out_mem[c][offset+i]);
            S1 += (((opus_val32)(tmp)*(opus_val32)(tmp)));
         }
         for (i=0;i<24;i++)
            mem[i] = (out_mem[c][1024-1-i]);
         for (i=0;i<len+st->mode->overlap;i++)
            e[i] = ((fade)*(e[i]));
         celt_iir(e, lpc+c*24, e, len+st->mode->overlap, 24, &mem);

         {
            opus_val32 S2=0.0;
            for (i=0;i<len+overlap;i++)
            {
               opus_val16 tmp = (e[i]);
               S2 += (((opus_val32)(tmp)*(opus_val32)(tmp)));
            }
            /* This checks for an "explosion" in the synthesis */

               /* Float test is written this way to catch NaNs at the same time */
               if (!(S1 > 0.2f*S2))
               {
                  for (i=0;i<len+overlap;i++)
                     e[i] = 0.0;
               } else if (S1 < S2)
               {
                  opus_val16 ratio = ((float)sqrt(((float)((S1)+1.0)/(S2+1.0))));
                  for (i=0;i<len+overlap;i++)
                     e[i] = ((ratio)*(e[i]));
               }
         }

         /* Apply post-filter to the MDCT overlap of the previous frame */
         comb_filter(out_mem[c]+1024, out_mem[c]+1024, st->postfilter_period, st->postfilter_period, st->overlap,
               st->postfilter_gain, st->postfilter_gain, st->postfilter_tapset, st->postfilter_tapset,
               null, 0);

         for (i=0;i<1024+st->mode->overlap-N;i++)
            out_mem[c][i] = out_mem[c][N+i];

         /* Apply TDAC to the concealed audio so that it blends with the
         previous and next frames */
         for (i=0;i<overlap/2;i++)
         {
            opus_val32 tmp;
            tmp = ((st->mode->window[i])*(e[N+overlap-1-i])) +
                  ((st->mode->window[overlap-i-1])*(e[N+i ]));
            out_mem[c][1024+i] = ((st->mode->window[overlap-i-1])*(tmp));
            out_mem[c][1024+overlap-i-1] = ((st->mode->window[i])*(tmp));
         }
         for (i=0;i<N;i++)
            out_mem[c][1024-N+i] = e[i];

         /* Apply pre-filter to the MDCT overlap for the next frame (post-filter will be applied then) */
         comb_filter(e, out_mem[c]+1024, st->postfilter_period, st->postfilter_period, st->overlap,
               -st->postfilter_gain, -st->postfilter_gain, st->postfilter_tapset, st->postfilter_tapset,
               null, 0);
         for (i=0;i<overlap;i++)
            out_mem[c][1024+i] = e[i];
			
		 afree((byte*)e);
		 
        if (!(++c<C)) break;
	  }
   }

   deemphasis(out_syn, pcm, N, C, st->downsample, &st->mode->preemph, &st->preemph_memD);

   st->loss_count++;

   ;
}

public int celt_decode_with_ec(OpusCustomDecoder *  st,           byte      *data, int len, opus_val16 *  pcm, int frame_size, ec_dec *dec)
{
   int c, i, N;
   int spread_decision;
   opus_int32 bits;
   ec_dec _dec;
   celt_sig *freq;
   celt_norm *X;
   celt_ener *bandE;
   int *fine_quant;
   int *pulses;
   int *cap;
   int *offsets;
   int *fine_priority;
   int *tf_res;
       byte      *collapse_masks;
   celt_sig *out_mem[2];
   celt_sig *decode_mem[2];
   celt_sig *overlap_mem[2];
   celt_sig *out_syn[2];
   opus_val16 *lpc;
   opus_val16* oldBandE, oldLogE, oldLogE2, backgroundLogE;

   int shortBlocks;
   int isTransient;
   int intra_ener;
   int CC = st->channels;
   int LM, M;
   int effEnd;
   int codedBands;
   int alloc_trim;
   int postfilter_pitch;
   opus_val16 postfilter_gain;
   int intensity=0;
   int dual_stereo=0;
   opus_int32 total_bits;
   opus_int32 balance;
   opus_int32 tell;
   int dynalloc_logp;
   int postfilter_tapset;
   int anti_collapse_rsv;
   int anti_collapse_on=0;
   int silence;
   int C = st->stream_channels;
   ;

   clear decode_mem, out_mem, out_syn, overlap_mem;
   
   *&frame_size *= st->downsample;

   c=0;
   for (;;)
   {
      decode_mem[c] = &st->_decode_mem + c*(2048+st->overlap);
      out_mem[c] = decode_mem[c]+2048-1024;
      overlap_mem[c] = decode_mem[c]+2048;
     if (!(++c<CC)) break;
   }
   lpc = (opus_val16*)(&st->_decode_mem+(2048+st->overlap)*CC);
   oldBandE = lpc+CC*24;
   oldLogE = oldBandE + 2*st->mode->nbEBands;
   oldLogE2 = oldLogE + 2*st->mode->nbEBands;
   backgroundLogE = oldLogE2  + 2*st->mode->nbEBands;

   {
      for (LM=0;LM<=st->mode->maxLM;LM++)
         if (st->mode->shortMdctSize<<LM==frame_size)
            break;
      if (LM>st->mode->maxLM)
         return -1;
   }
   M=1<<LM;

   if (len<0 || len>1275 || pcm==null)
      return -1;

   N = M*st->mode->shortMdctSize;

   effEnd = st->_end;
   if (effEnd > st->mode->effEBands)
      effEnd = st->mode->effEBands;

   freq = ((celt_sig*)malloc(((int)(celt_sig ' size  ))*(((CC) > (C) ? (CC) : (C))*N))); /**< Interleaved signal MDCTs */
   X = ((celt_norm*)malloc(((int)(celt_norm ' size  ))*(C*N)));   /**< Interleaved normalised MDCTs */
   bandE = ((celt_ener*)malloc(((int)(celt_ener ' size  ))*(st->mode->nbEBands*C)));
   c=0;
   for (;;)
   {
      for (i=0;i<M*st->mode->eBands[st->start];i++)
         X[c*N+i] = 0.0;
     if (!(++c<C)) break;
   }
   c=0;
   for (;;)
   {
      for (i=M*st->mode->eBands[effEnd];i<N;i++)
         X[c*N+i] = 0.0;
     if (!(++c<C)) break;
   }
   
   if (data == null || len<=1)
   {
      celt_decode_lost(st, pcm, N, LM);

      afree((byte*)freq);
      afree((byte*)X);
      afree((byte*)bandE);
      ;
      return frame_size/st->downsample;
   }

   if (dec == null)
   {
      ec_dec_init(&_dec,(    byte     *)data,(uint)len);
      *&dec = &_dec;
   }

   if (C==1)
   {
      for (i=0;i<st->mode->nbEBands;i++)
         oldBandE[i]=((oldBandE[i]) > (oldBandE[st->mode->nbEBands+i]) ? (oldBandE[i]) : (oldBandE[st->mode->nbEBands+i]));
   }

   total_bits = len*8;
   tell = ec_tell(dec);

   if (tell >= total_bits)
      silence = 1;
   else if (tell==1)
      silence = ec_dec_bit_logp(dec, 15);
   else
      silence = 0;
   if (silence != 0)
   {
      /* Pretend we've read all the remaining bits */
      tell = len*8;
      dec->nbits_total+=tell-ec_tell(dec);
   }

   postfilter_gain = 0.0;
   postfilter_pitch = 0;
   postfilter_tapset = 0;
   if (st->start==0 && tell+16 <= total_bits)
   {
      if(ec_dec_bit_logp(dec, 1) != 0)
      {
         int qg, octave;
         octave = (int)ec_dec_uint(dec, 6);
         postfilter_pitch = (16<<octave)+(int)ec_dec_bits(dec, 4+(uint)octave)-1;
         qg = (int)ec_dec_bits(dec, 3);
         if (ec_tell(dec)+2<=total_bits)
            postfilter_tapset = ec_dec_icdf(dec, &tapset_icdf, 2);
         postfilter_gain = (0.09375f)*(float)(qg+1);
      }
      tell = ec_tell(dec);
   }

   if (LM > 0 && tell+3 <= total_bits)
   {
      isTransient = ec_dec_bit_logp(dec, 3);
      tell = ec_tell(dec);
   }
   else
      isTransient = 0;

   if (isTransient != 0)
      shortBlocks = M;
   else
      shortBlocks = 0;

   /* Decode the global flags (first symbols in the stream) */
   intra_ener = tell+3<=total_bits ? ec_dec_bit_logp(dec, 3) : 0;
   /* Get band energies */
   unquant_coarse_energy(st->mode, st->start, st->_end, oldBandE,
         intra_ener, dec, C, LM);

   tf_res = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));
   tf_decode(st->start, st->_end, isTransient, tf_res, LM, dec);

   tell = ec_tell(dec);
   spread_decision = (2);
   if (tell+4 <= total_bits)
      spread_decision = ec_dec_icdf(dec, &spread_icdf, 5);

   pulses = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));
   cap = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));
   offsets = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));
   fine_priority = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));

   init_caps(st->mode,cap,LM,C);

   dynalloc_logp = 6;
   total_bits<<=3;
   tell = (int)ec_tell_frac(dec);
   for (i=st->start;i<st->_end;i++)
   {
      int width, quanta;
      int dynalloc_loop_logp;
      int boost;
      width = C*(st->mode->eBands[i+1]-st->mode->eBands[i])<<LM;
      /* quanta is 6 bits, but no more than 1 bit/sample
         and no less than 1/8 bit/sample */
      quanta = ((width<<3) < (((6<<3) > (width) ? (6<<3) : (width))) ? (width<<3) : (((6<<3) > (width) ? (6<<3) : (width))));
      dynalloc_loop_logp = dynalloc_logp;
      boost = 0;
      while (tell+(dynalloc_loop_logp<<3) < total_bits && boost < cap[i])
      {
         int flag;
         flag = ec_dec_bit_logp(dec, (uint)dynalloc_loop_logp);
         tell = (int)ec_tell_frac(dec);
         if (flag == 0)
            break;
         boost += quanta;
         total_bits -= quanta;
         dynalloc_loop_logp = 1;
      }
      offsets[i] = boost;
      /* Making dynalloc more likely */
      if (boost>0)
         dynalloc_logp = ((2) > (dynalloc_logp-1) ? (2) : (dynalloc_logp-1));
   }

   fine_quant = ((int*)malloc(((int)(int ' size  ))*(st->mode->nbEBands)));
   alloc_trim = tell+(6<<3) <= total_bits ?
         ec_dec_icdf(dec, &trim_icdf, 7) : 5;

   bits = (((opus_int32)len*8)<<3) - (int)ec_tell_frac(dec) - 1;
   anti_collapse_rsv = isTransient!=0&&LM>=2&&bits>=((LM+2)<<3) ? (1<<3) : 0;
   bits -= anti_collapse_rsv;
   codedBands = compute_allocation(st->mode, st->start, st->_end, offsets, cap,
         alloc_trim, &intensity, &dual_stereo, bits, &balance, pulses,
         fine_quant, fine_priority, C, LM, dec, 0, 0);

   unquant_fine_energy(st->mode, st->start, st->_end, oldBandE, fine_quant, dec, C);

   /* Decode fixed codebook */
   collapse_masks = ((    byte     *)malloc(((int)(    byte      ' size  ))*(C*st->mode->nbEBands)));
   quant_all_bands(0, st->mode, st->start, st->_end, X, C==2 ? X+N : null, collapse_masks,
         null, pulses, shortBlocks, spread_decision, dual_stereo, intensity, tf_res,
         len*(8<<3)-anti_collapse_rsv, balance, dec, LM, codedBands, &st->rng);

   if (anti_collapse_rsv > 0)
   {
      anti_collapse_on = (int)ec_dec_bits(dec, 1);
   }

   unquant_energy_finalise(st->mode, st->start, st->_end, oldBandE,
         fine_quant, fine_priority, len*8-ec_tell(dec), dec, C);

   if (anti_collapse_on != 0)
      anti_collapse(st->mode, X, collapse_masks, LM, C, N,
            st->start, st->_end, oldBandE, oldLogE, oldLogE2, pulses, st->rng);

   log2Amp(st->mode, st->start, st->_end, bandE, oldBandE, C);

   if (silence != 0)
   {
      for (i=0;i<C*st->mode->nbEBands;i++)
      {
         bandE[i] = 0.0;
         oldBandE[i] = -(28.0f);
      }
   }
   /* Synthesis */
   denormalise_bands(st->mode, X, freq, bandE, effEnd, C, M);

   memmove((byte*)(decode_mem[0]), (byte*)(decode_mem[0]+N), (2048-N)*((int)((*(decode_mem[0])) ' size  )) + (int)(0*((decode_mem[0])-(decode_mem[0]+N))) );
   if (CC==2)
      memmove((byte*)(decode_mem[1]), (byte*)(decode_mem[1]+N), (2048-N)*((int)((*(decode_mem[1])) ' size  )) +(int)(0*((decode_mem[1])-(decode_mem[1]+N))) );

   c=0;
   for (;;)
   {
     for (i=0;i<M*st->mode->eBands[st->start];i++)
         freq[c*N+i] = 0.0;
     if (!(++c<C)) break;
   }
   c=0;
   for (;;)
   {
      int bound = M*st->mode->eBands[effEnd];
      if (st->downsample!=1)
         bound = ((bound) < (N/st->downsample) ? (bound) : (N/st->downsample));
      for (i=bound;i<N;i++)
         freq[c*N+i] = 0.0;
     if (!(++c<C)) break;;
   }
   
   out_syn[0] = out_mem[0]+1024-N;
   if (CC==2)
      out_syn[1] = out_mem[1]+1024-N;

   if (CC==2&&C==1)
   {
      for (i=0;i<N;i++)
         freq[N+i] = freq[i];
   }
   if (CC==1&&C==2)
   {
      for (i=0;i<N;i++)
         freq[i] = (0.5f*(((freq[i])+(freq[N+i]))));
   }

   /* Compute inverse MDCTs */
   compute_inv_mdcts(st->mode, shortBlocks, freq, out_syn, overlap_mem, CC, LM);

   c=0;
   for (;;)
   {
      st->postfilter_period=((st->postfilter_period) > (15) ? (st->postfilter_period) : (15));
      st->postfilter_period_old=((st->postfilter_period_old) > (15) ? (st->postfilter_period_old) : (15));
      comb_filter(out_syn[c], out_syn[c], st->postfilter_period_old, st->postfilter_period, st->mode->shortMdctSize,
            st->postfilter_gain_old, st->postfilter_gain, st->postfilter_tapset_old, st->postfilter_tapset,
            &st->mode->window, st->overlap);
      if (LM!=0)
         comb_filter(out_syn[c]+st->mode->shortMdctSize, out_syn[c]+st->mode->shortMdctSize, st->postfilter_period, postfilter_pitch, N-st->mode->shortMdctSize,
               st->postfilter_gain, postfilter_gain, st->postfilter_tapset, postfilter_tapset,
               &st->mode->window, st->mode->overlap);

     if (!(++c<CC)) break;
   }
   st->postfilter_period_old = st->postfilter_period;
   st->postfilter_gain_old = st->postfilter_gain;
   st->postfilter_tapset_old = st->postfilter_tapset;
   st->postfilter_period = postfilter_pitch;
   st->postfilter_gain = postfilter_gain;
   st->postfilter_tapset = postfilter_tapset;
   if (LM!=0)
   {
      st->postfilter_period_old = st->postfilter_period;
      st->postfilter_gain_old = st->postfilter_gain;
      st->postfilter_tapset_old = st->postfilter_tapset;
   }

   if (C==1) {
      for (i=0;i<st->mode->nbEBands;i++)
         oldBandE[st->mode->nbEBands+i]=oldBandE[i];
   }

   /* In case start or end were to change */
   if (isTransient == 0)
   {
      for (i=0;i<2*st->mode->nbEBands;i++)
         oldLogE2[i] = oldLogE[i];
      for (i=0;i<2*st->mode->nbEBands;i++)
         oldLogE[i] = oldBandE[i];
      for (i=0;i<2*st->mode->nbEBands;i++)
         backgroundLogE[i] = ((backgroundLogE[i] + (float)M*(0.001f)) < (oldBandE[i]) ? (backgroundLogE[i] + (float)M*(0.001f)) : (oldBandE[i]));
   } else {
      for (i=0;i<2*st->mode->nbEBands;i++)
         oldLogE[i] = ((oldLogE[i]) < (oldBandE[i]) ? (oldLogE[i]) : (oldBandE[i]));
   }
   c=0;
   for (;;)
   {
      for (i=0;i<st->start;i++)
      {
         oldBandE[c*st->mode->nbEBands+i]=0.0;
         oldLogE2[c*st->mode->nbEBands+i]=-(28.0f);
         oldLogE[c*st->mode->nbEBands+i]=-(28.0f);
      }
      for (i=st->_end;i<st->mode->nbEBands;i++)
      {
         oldBandE[c*st->mode->nbEBands+i]=0.0;
         oldLogE2[c*st->mode->nbEBands+i]=-(28.0f);
         oldLogE[c*st->mode->nbEBands+i]=-(28.0f);
      }
      if (!(++c<2)) break;
   }
   st->rng = dec->rng;

   deemphasis(out_syn, pcm, N, CC, st->downsample, &st->mode->preemph, &st->preemph_memD);
   st->loss_count = 0;

   afree((byte*)collapse_masks);
   afree((byte*)fine_quant);
   afree((byte*)pulses);
   afree((byte*)cap);
   afree((byte*)offsets);
   afree((byte*)fine_priority);
   afree((byte*)tf_res);
   afree((byte*)freq);
   afree((byte*)X);
   afree((byte*)bandE);
   ;
   if (ec_tell(dec) > 8*len)
      return -3;
   if(ec_get_error(dec) != 0)
      st->error = 1;
   return frame_size/st->downsample;
}

public
int opus_custom_decoder_ctl(OpusCustomDecoder *  st, int request, byte[] value0)
{
//   va_list ap;

//   ( ap = (va_list)&request + ( (((int)((request) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) ) );
   switch (request)
   {
      case 10010:
      {
//         opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         if (value<0 || value>=st->mode->nbEBands)
           return -1; // goto bad_arg;
         st->start = value;
      }
      break;
      case 10012:
      {
//         opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         if (value<1 || value>st->mode->nbEBands)
           return -1; // goto bad_arg;
         st->_end = value;
      }
      break;
      case 10008:
      {
//         opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         if (value<1 || value>2)
           return -1; //    goto bad_arg;
         st->stream_channels = value;
      }
      break;
      case 10007:
      {
//         opus_int32 *value = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int* value;
		 value'byte = value0'byte;
         if (value==null)
            return -1; // goto bad_arg;
         *value=st->error;
         st->error = 0;
      }
      break;
      case 4027:
      {
//         opus_int32 *value = ( *(opus_int32* *)((ap += ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32*) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int* value;
		 value'byte = value0'byte;
         if (value==null)
           return -1; //    goto bad_arg;
         *value = st->overlap/st->downsample;
      }
      break;
      case 4028:
      {
         int i;
         opus_val16* lpc, oldBandE, oldLogE, oldLogE2;
         lpc = (opus_val16*)(&st->_decode_mem+(2048+st->overlap)*st->channels);
         oldBandE = lpc+st->channels*24;
         oldLogE = oldBandE + 2*st->mode->nbEBands;
         oldLogE2 = oldLogE + 2*st->mode->nbEBands;
         

memset(((char*)&st->rng), 0, (opus_custom_decoder_get_size(st->mode, st->channels)- (int)(((char*)&st->rng - (char*)st))*((int)((*((char*)&st->rng)) ' size  ))));
         for (i=0;i<2*st->mode->nbEBands;i++)
		 {
            oldLogE2[i] = -(28.0f);
            oldLogE[i]  = -(28.0f);
		 }
      }
      break;
      case 4033:
      {
         int* value;
		 value'byte = value0'byte;
         if (value==null)
           return -1; //    goto bad_arg;
         *value = st->postfilter_period;
      }
      break;

      case 10015:
      {
         OpusCustomMode ** value;
         value'byte = value0'byte;   // = ( *(const OpusCustomMode** *)((ap += ( (((int)((const OpusCustomMode**) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((const OpusCustomMode**) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         if (value==null)
            return -1; //    goto bad_arg;
         *value=st->mode;
      }
      break;
      case 10016:
      {
//         opus_int32 value = ( *(opus_int32 *)((ap += ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_int32) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         int value;
		 value'byte = value0'byte;
         st->signalling = value;
      }
      break;
      case 4031:
      {
//         opus_uint32 * value = ( *(opus_uint32 * *)((ap += ( (((int)((opus_uint32 *) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) - ( (((int)((opus_uint32 *) ' size  )) + ((int)((int) ' size  )) - 1) & ~(((int)((int) ' size  )) - 1) )) );
         uint* value;
		 value'byte = value0'byte;
         if (value==null)
           return -1; //       goto bad_arg;
         *value=st->rng;
      }
      break;
      default:
        return -5; //     goto bad_request;
   }
//   ( ap = (va_list)0 );
   return 0;
//bad_arg:
//   ( ap = (va_list)0 );
//   return -1;
//bad_request:
//      ( ap = (va_list)0 );
//  return -5;
}

char *opus_strerror(int error)
{
   const string error_strings[8] = {
      "success\0",
      "invalid argument\0",
      "buffer too small\0",
      "internal error\0",
      "corrupted stream\0",
      "request not implemented\0",
      "invalid state\0",
      "memory allocation failed\0"
   };
   if (error > 0 || error < -7)
   {
     const string unknown = "unknown error";
      return &unknown;
   }
   else
      return &error_strings[-error];
}

char *opus_get_version_string()
{
  const string unknown = "libopus "  + "unknown\0";
  return &unknown;
}
#end unsafe
