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

use ../math, ../arithm;

use laplace;
use os_support;
use opus_types, arch;
use modes, entcode, entenc, entdec;


/* Mean energy in each band quantized in Q6 and converted back to float */
const        opus_val16 eMeans[25] = {
      6.437500f, 6.250000f, 5.750000f, 5.312500f, 5.062500f,
      4.812500f, 4.500000f, 4.375000f, 4.875000f, 4.687500f,
      4.562500f, 4.437500f, 4.875000f, 4.625000f, 4.312500f,
      4.500000f, 4.375000f, 4.625000f, 4.750000f, 4.437500f,
      3.750000f, 3.750000f, 3.750000f, 3.750000f, 3.750000f
};
/* prediction coefficients: 0.9, 0.8, 0.65, 0.5 */

const        opus_val16 pred_coef[4] = {29440.0/32768.0, 26112.0/32768.0, 21248.0/32768.0, 16384.0/32768.0};
const        opus_val16 beta_coef[4] = {30147.0/32768.0, 22282.0/32768.0, 12124.0/32768.0, 6554.0/32768.0};
const        opus_val16 beta_intra = 4915.0/32768.0;

/*Parameters of the Laplace-like probability models used for the coarse energy.
  There is one pair of parameters for each frame size, prediction type
   (inter/intra), and band number.
  The first number of each pair is the probability of 0, and the second is the
   decay rate, both in Q8 precision.*/
const            byte      e_prob_model[4][2][42] = {
   /*120 sample frames.*/
   {
      /*Inter*/
      {
          72, 127,  65, 129,  66, 128,  65, 128,  64, 128,  62, 128,  64, 128,
          64, 128,  92,  78,  92,  79,  92,  78,  90,  79, 116,  41, 115,  40,
         114,  40, 132,  26, 132,  26, 145,  17, 161,  12, 176,  10, 177,  11
      },
      /*Intra*/
      {
          24, 179,  48, 138,  54, 135,  54, 132,  53, 134,  56, 133,  55, 132,
          55, 132,  61, 114,  70,  96,  74,  88,  75,  88,  87,  74,  89,  66,
          91,  67, 100,  59, 108,  50, 120,  40, 122,  37,  97,  43,  78,  50
      }
   },
   /*240 sample frames.*/
   {
      /*Inter*/
      {
          83,  78,  84,  81,  88,  75,  86,  74,  87,  71,  90,  73,  93,  74,
          93,  74, 109,  40, 114,  36, 117,  34, 117,  34, 143,  17, 145,  18,
         146,  19, 162,  12, 165,  10, 178,   7, 189,   6, 190,   8, 177,   9
      },
      /*Intra*/
      {
          23, 178,  54, 115,  63, 102,  66,  98,  69,  99,  74,  89,  71,  91,
          73,  91,  78,  89,  86,  80,  92,  66,  93,  64, 102,  59, 103,  60,
         104,  60, 117,  52, 123,  44, 138,  35, 133,  31,  97,  38,  77,  45
      }
   },
   /*480 sample frames.*/
   {
      /*Inter*/
      {
          61,  90,  93,  60, 105,  42, 107,  41, 110,  45, 116,  38, 113,  38,
         112,  38, 124,  26, 132,  27, 136,  19, 140,  20, 155,  14, 159,  16,
         158,  18, 170,  13, 177,  10, 187,   8, 192,   6, 175,   9, 159,  10
      },
      /*Intra*/
      {
          21, 178,  59, 110,  71,  86,  75,  85,  84,  83,  91,  66,  88,  73,
          87,  72,  92,  75,  98,  72, 105,  58, 107,  54, 115,  52, 114,  55,
         112,  56, 129,  51, 132,  40, 150,  33, 140,  29,  98,  35,  77,  42
      }
   },
   /*960 sample frames.*/
   {
      /*Inter*/
      {
          42, 121,  96,  66, 108,  43, 111,  40, 117,  44, 123,  32, 120,  36,
         119,  33, 127,  33, 134,  34, 139,  21, 147,  23, 152,  20, 158,  25,
         154,  26, 166,  21, 173,  16, 184,  13, 184,  10, 150,  13, 139,  15
      },
      /*Intra*/
      {
          22, 178,  63, 114,  74,  82,  84,  83,  92,  82, 103,  62,  96,  72,
          96,  67, 101,  73, 107,  72, 113,  55, 118,  52, 125,  52, 118,  52,
         117,  55, 135,  49, 137,  39, 157,  32, 145,  29,  97,  33,  77,  40
      }
   }
};

const            byte      small_energy_icdf[3]={2,1,0};

opus_val32 loss_distortion(      opus_val16 *eBands, opus_val16 *oldEBands, int start, int _end, int len, int C)
{
   int c, i;
   opus_val32 dist = 0.0;
   c=0;
   for (;;)
   {
      for (i=start;i<_end;i++)
      {
         opus_val16 d = (((eBands[i+c*len]))-((oldEBands[i+c*len])));
         dist = ((dist)+(opus_val32)(d)*(opus_val32)(d));
      }
     if (!(++c<C)) break;
   }
   return ((200.0) < ((dist)) ? (200.0) : ((dist)));
}

int quant_coarse_energy_impl(      OpusCustomMode *m, int start, int _end,
            opus_val16 *eBands, opus_val16 *oldEBands,
      opus_int32 budget, opus_int32 tell,
                byte      *prob_model, opus_val16 *error, ec_enc *enc,
      int C, int LM, int intra, opus_val16 max_decay)
{
   int i, c;
   int badness = 0;
   opus_val32 prev[2] = {0.0,0.0};
   opus_val16 coef;
   opus_val16 beta;

   if (tell+3 <= budget)
      ec_enc_bit_logp(enc, intra, 3);
   if (intra != 0)
   {
      coef = 0.0;
      beta = beta_intra;
   } else {
      beta = beta_coef[LM];
      coef = pred_coef[LM];
   }

   /* Encode at a fixed coarse resolution */
   for (i=start;i<_end;i++)
   {
      c=0;
      for (;;)
	  {
         int bits_left;
         int qi, qi0;
         opus_val32 q;
         opus_val16 x;
         opus_val32 f, tmp;
         opus_val16 oldE;
         opus_val16 decay_bound;
         x = eBands[i+c*m->nbEBands];
         oldE = ((-(9.0f)) > (oldEBands[i+c*m->nbEBands]) ? (-(9.0f)) : (oldEBands[i+c*m->nbEBands]));

         f = x-coef*oldE-prev[c];
         /* Rounding to nearest integer here is really important! */
         qi = (int)floor(0.5f+f);
         decay_bound = ((-(28.0f)) > (oldEBands[i+c*m->nbEBands]) ? (-(28.0f)) : (oldEBands[i+c*m->nbEBands])) - max_decay;
         /* Prevent the energy from going down too quickly (e.g. for bands
            that have just one bin) */
         if (qi < 0 && x < decay_bound)
         {
            qi += (int)(((decay_bound)-(x)));
            if (qi > 0)
               qi = 0;
         }
         qi0 = qi;
         /* If we don't have enough bits to encode all the energy, just assume
             something safe. */
         *&tell = ec_tell(enc);
         bits_left = budget-tell-3*C*(_end-i);
         if (i!=start && bits_left < 30)
         {
            if (bits_left < 24)
               qi = ((1) < (qi) ? (1) : (qi));
            if (bits_left < 16)
               qi = ((-1) > (qi) ? (-1) : (qi));
         }
         if (budget-tell >= 15)
         {
            int pi;
            pi = 2*((i) < (20) ? (i) : (20));
            ec_laplace_encode(enc, &qi,
                  prob_model[pi]<<7, (int)(prob_model[pi+1]<<6));
         }
         else if(budget-tell >= 2)
         {
            qi = ((-1) > (((qi) < (1) ? (qi) : (1))) ? (-1) : (((qi) < (1) ? (qi) : (1))));
            ec_enc_icdf(enc, 2*qi^-(int)(qi<0), &small_energy_icdf, 2);
         }
         else if(budget-tell >= 1)
         {
            qi = ((0) < (qi) ? (0) : (qi));
            ec_enc_bit_logp(enc, -qi, 1);
         }
         else
            qi = -1;
         error[i+c*m->nbEBands] = (f) - (float)(qi);
         badness += abs(qi0-qi);
         q = (opus_val32)((qi));

         tmp = (((opus_val32)(coef)*(opus_val32)(oldE))) + prev[c] + (q);

         oldEBands[i+c*m->nbEBands] = (tmp);
         prev[c] = prev[c] + (q) - ((opus_val32)(beta)*(opus_val32)((q)));
        if (!(++c < C)) break;
	  }
   }
   return badness;
}

public
void quant_coarse_energy(      OpusCustomMode *m, int start, int _end, int effEnd,
            opus_val16 *eBands, opus_val16 *oldEBands, opus_uint32 budget,
      opus_val16 *error, ec_enc *enc, int C, int LM, int nbAvailableBytes,
      int force_intra, opus_val32 *delayedIntra, int two_pass, int loss_rate)
{
   int intra;
   opus_val16 max_decay;
   opus_val16 *oldEBands_intra;
   opus_val16 *error_intra;
   ec_enc enc_start_state;
   opus_uint32 tell;
   int badness1=0;
   opus_int32 intra_bias;
   opus_val32 new_distortion;
   ;

   intra = (int)(force_intra != 0 || (two_pass == 0 && *delayedIntra>(float)(2*C*(_end-start)) && nbAvailableBytes > (_end-start)*C));
   intra_bias = (opus_int32)(((float)budget * *delayedIntra*(float)loss_rate)/(float)(C*512));
   new_distortion = loss_distortion(eBands, oldEBands, start, effEnd, m->nbEBands, C);

   tell = (uint)ec_tell(enc);
   if (tell+3 > budget)
   {
     *&two_pass = 0;
	 intra = 0;
   }

   /* Encode the global flags using a simple probability model
      (first symbols in the stream) */

   max_decay = ((16.0f) < (0.125f*(float)nbAvailableBytes) ? (16.0f) : (0.125f*(float)nbAvailableBytes));

   enc_start_state = *enc;

   oldEBands_intra = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(C*m->nbEBands)));
   error_intra = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(C*m->nbEBands)));
   memcpy((byte*)(oldEBands_intra), (byte*)(oldEBands), (C*m->nbEBands)*((int)((*(oldEBands_intra)) ' size  )) + (int)(0*((oldEBands_intra)-(oldEBands)) ));

   if (two_pass != 0 || intra != 0)
   {
      badness1 = quant_coarse_energy_impl(m, start, _end, eBands, oldEBands_intra, (int)budget,
            (int)tell, (byte*)&e_prob_model[LM][1], error_intra, enc, C, LM, 1, max_decay);
   }

   if (intra == 0)
   {
      byte      *intra_buf;
      ec_enc enc_intra_state;
      opus_int32 tell_intra;
      opus_uint32 nstart_bytes;
      opus_uint32 nintra_bytes;
      int badness2;
          byte      *intra_bits;

      tell_intra = (int)ec_tell_frac(enc);

      enc_intra_state = *enc;

      nstart_bytes = ec_range_bytes(&enc_start_state);
      nintra_bytes = ec_range_bytes(&enc_intra_state);
      intra_buf = ec_get_buffer(&enc_intra_state) + nstart_bytes;
      intra_bits = ((    byte     *)malloc(((int)(    byte      ' size  ))*(int)(nintra_bytes-nstart_bytes)));
      /* Copy bits from intra bit-stream */
      memcpy((intra_bits), (intra_buf), (int)(nintra_bytes - nstart_bytes)*((int)((*(intra_bits)) ' size  )) + (int)(0*((intra_bits)-(intra_buf)) ));

      *enc = enc_start_state;

      badness2 = quant_coarse_energy_impl(m, start, _end, eBands, oldEBands, (int)budget,
            (int)tell, (byte*)&e_prob_model[LM][intra], error, enc, C, LM, 0, max_decay);

      if (two_pass != 0 && (badness1 < badness2 || (badness1 == badness2 && ((opus_int32)ec_tell_frac(enc))+intra_bias > tell_intra)))
      {
         *enc = enc_intra_state;
         /* Copy intra bits to bit-stream */
         memcpy((intra_buf), (intra_bits), (int)(nintra_bytes - nstart_bytes)*((int)((*(intra_buf)) ' size  )) + (int)(0*((intra_buf)-(intra_bits))) );
         memcpy((byte*)(oldEBands), (byte*)(oldEBands_intra), (C*m->nbEBands)*((int)((*(oldEBands)) ' size  )) + (int)(0*((oldEBands)-(oldEBands_intra)) ));
         memcpy((byte*)(error), (byte*)(error_intra), (C*m->nbEBands)*((int)((*(error)) ' size  )) + (int)(0*((error)-(error_intra)) ));
         intra = 1;
      }
	  
      afree((byte*)intra_bits);
   }
   else
   {
      memcpy((byte*)(oldEBands), (byte*)(oldEBands_intra), (C*m->nbEBands)*((int)((*(oldEBands)) ' size  )) + (int)(0*((oldEBands)-(oldEBands_intra))) );
      memcpy((byte*)(error), (byte*)(error_intra), (C*m->nbEBands)*((int)((*(error)) ' size  )) + (int)(0*((error)-(error_intra))) );
   }

   if (intra != 0)
      *delayedIntra = new_distortion;
   else
      *delayedIntra = 
((((((pred_coef[LM])*(pred_coef[LM])))*(*delayedIntra)))+(new_distortion));

   afree((byte*)oldEBands_intra);
   afree((byte*)error_intra);
   ;
}

public void quant_fine_energy (OpusCustomMode *m, int start, int _end, opus_val16 *oldEBands, opus_val16 *error, int *fine_quant, ec_enc *enc, int C)
{
   int i, c;

   /* Encode finer resolution */
   for (i=start;i<_end;i++)
   {
      opus_int16 frac = (opus_int16)(1<<fine_quant[i]);
      if (fine_quant[i] <= 0)
         continue;
      c=0;
      for (;;)
	  {
         int q2;
         opus_val16 offset;

         q2 = (int)floor((error[i+c*m->nbEBands]+0.5f)*(float)frac);
         if (q2 > frac-1)
            q2 = frac-1;
         if (q2<0)
            q2 = 0;
         ec_enc_bits(enc, (uint)q2, (uint)fine_quant[i]);

         offset = ((float)q2+0.5f)*(float)(1<<(14-fine_quant[i]))*(1.0f/16384.0) - 0.5f;
         oldEBands[i+c*m->nbEBands] += offset;
         error[i+c*m->nbEBands] -= offset;
         /*printf ("%f ", error[i] - offset);*/
        if (!(++c < C)) break;
	  }
   }
}

public
void quant_energy_finalise(      OpusCustomMode *m, int start, int _end, opus_val16 *oldEBands, opus_val16 *error, int *fine_quant, int *fine_priority, int bits_left, ec_enc *enc, int C)
{
   int i, prio, c;

   /* Use up the remaining bits */
   for (prio=0;prio<2;prio++)
   {
      for (i=start;i<_end && bits_left>=C ;i++)
      {
         if (fine_quant[i] >= 8 || fine_priority[i]!=prio)
            continue;
         c=0;
         for (;;)
		 {
            int q2;
            opus_val16 offset;
            q2 = error[i+c*m->nbEBands]<0.0 ? 0 : 1;
            ec_enc_bits(enc, (uint)q2, 1);

            offset = ((float)q2-0.5f)*(float)(1<<(14-fine_quant[i]-1))*(1.0f/16384.0);
            oldEBands[i+c*m->nbEBands] += offset;
            (*&bits_left)--;
           if (!(++c < C)) break;
		 }
      }
   }
}

public void unquant_coarse_energy(      OpusCustomMode *m, int start, int _end, opus_val16 *oldEBands, int intra, ec_dec *dec, int C, int LM)
{
   byte *prob_model = &e_prob_model[LM][intra];
   int i, c;
   opus_val32 prev[2] = {0.0, 0.0};
   opus_val16 coef;
   opus_val16 beta;
   opus_int32 budget;
   opus_int32 tell;

   if (intra != 0)
   {
      coef = 0.0;
      beta = beta_intra;
   } else {
      beta = beta_coef[LM];
      coef = pred_coef[LM];
   }

   budget = (int)dec->storage*8;

   /* Decode at a fixed coarse resolution */
   for (i=start;i<_end;i++)
   {
      c=0;
      for (;;)
	  {
         int qi;
         opus_val32 q;
         opus_val32 tmp;
         /* It would be better to express this invariant as a
            test on C at function entry, but that isn't enough
            to make the static analyzer happy. */
         ;
         tell = ec_tell(dec);
         if(budget-tell>=15)
         {
            int pi;
            pi = 2*((i) < (20) ? (i) : (20));
            qi = ec_laplace_decode(dec,
                  prob_model[pi]<<7, (int)(prob_model[pi+1]<<6));
         }
         else if(budget-tell>=2)
         {
            qi = ec_dec_icdf(dec, &small_energy_icdf, 2);
            qi = (qi>>1)^-(qi&1);
         }
         else if(budget-tell>=1)
         {
            qi = -ec_dec_bit_logp(dec, 1);
         }
         else
            qi = -1;
         q = (opus_val32)((qi));

         oldEBands[i+c*m->nbEBands] = ((-(9.0f)) > (oldEBands[i+c*m->nbEBands]) ? (-(9.0f)) : (oldEBands[i+c*m->nbEBands]));
         tmp = (((opus_val32)(coef)*(opus_val32)(oldEBands[i+c*m->nbEBands]))) + prev[c] + (q);

         oldEBands[i+c*m->nbEBands] = (tmp);
         prev[c] = prev[c] + (q) - ((opus_val32)(beta)*(opus_val32)((q)));
        if (!(++c < C)) break;
	  }
   }
}

public void unquant_fine_energy(      OpusCustomMode *m, int start, int _end, opus_val16 *oldEBands, int *fine_quant, ec_dec *dec, int C)
{
   int i, c;
   /* Decode finer resolution */
   for (i=start;i<_end;i++)
   {
      if (fine_quant[i] <= 0)
         continue;
      c=0;
      for (;;)
	  {
         int q2;
         opus_val16 offset;
         q2 = (int)ec_dec_bits(dec, (uint)fine_quant[i]);

         offset = ((float)q2+0.5f)*(float)(1<<(14-fine_quant[i]))*(1.0f/16384.0) - 0.5f;
         oldEBands[i+c*m->nbEBands] += offset;
        if (!(++c < C)) break;
	  }
   }
}

public void unquant_energy_finalise(      OpusCustomMode *m, int start, int _end, opus_val16 *oldEBands, int *fine_quant,  int *fine_priority, int bits_left, ec_dec *dec, int C)
{
   int i, prio, c;

   /* Use up the remaining bits */
   for (prio=0;prio<2;prio++)
   {
      for (i=start;i<_end && bits_left>=C ;i++)
      {
         if (fine_quant[i] >= 8 || fine_priority[i]!=prio)
            continue;
         c=0;
         for (;;)
		 {
            int q2;
            opus_val16 offset;
            q2 = (int)ec_dec_bits(dec, 1);

            offset = ((float)q2-0.5f)*(float)(1<<(14-fine_quant[i]-1))*(1.0f/16384.0);
            oldEBands[i+c*m->nbEBands] += offset;
            (*&bits_left)--;
           if (!(++c < C)) break;
		 }
      }
   }
}

public void log2Amp(      OpusCustomMode *m, int start, int _end,
      celt_ener *eBands,       opus_val16 *oldEBands, int C)
{
   int c, i;
   c=0;
   for (;;)
   {
      for (i=0;i<start;i++)
         eBands[i+c*m->nbEBands] = 0.0;
      for (;i<_end;i++)
      {
         opus_val16 lg = ((oldEBands[i+c*m->nbEBands])+(((opus_val16)eMeans[i])));
         eBands[i+c*m->nbEBands] = (((float)exp(0.6931471805599453094D*(lg))));
      }
      for (;i<m->nbEBands;i++)
         eBands[i+c*m->nbEBands] = 0.0;
     if (!(++c < C)) break;
   }
}

public void amp2Log2(      OpusCustomMode *m, int effEnd, int _end,
      celt_ener *bandE, opus_val16 *bandLogE, int C)
{
   int c, i;
   c=0;
   for (;;)
   {
      for (i=0;i<effEnd;i++)
      {	  
         float log = (float)(1.442695040888963387D * ln((bandE[i+c*m->nbEBands])));
         bandLogE[i+c*m->nbEBands] = log - eMeans[i];
      }
      for (i=effEnd;i<_end;i++)
         bandLogE[c*m->nbEBands+i] = -(14.0f);
     if (!(++c < C)) break;
   }
}
#end unsafe
