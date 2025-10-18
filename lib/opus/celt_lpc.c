#begin unsafe
/* Copyright (c) 2009-2010 Xiph.Org Foundation
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

use arch;
use os_support;


public
void _celt_lpc(
      opus_val16       *_lpc, /* out: [0...p-1] LPC coefficients      */
      opus_val32 *ac,  /* in:  [0...p] autocorrelation values  */
int          p
)
{
   int i, j;
   opus_val32 r;
   opus_val32 error = ac[0];

   float *lpc = _lpc;

   for (i = 0; i < p; i++)
      lpc[i] = 0.0;
   if (ac[0] != 0.0)
   {
      for (i = 0; i < p; i++) {
         /* Sum up this iteration's reflection coefficient */
         opus_val32 rr = 0.0;
         for (j = 0; j < i; j++)
            rr += ((lpc[j])*(ac[i - j]));
         rr += (ac[i + 1]);
         r = -((float)((rr))/(error));
         /*  Update LPC coefficients and total error */
         lpc[i] = (r);
         for (j = 0; j < (i+1)>>1; j++)
         {
            opus_val32 tmp1, tmp2;
            tmp1 = lpc[j];
            tmp2 = lpc[i-1-j];
            lpc[j]     = tmp1 + ((r)*(tmp2));
            lpc[i-1-j] = tmp2 + ((r)*(tmp1));
         }

         error = error - ((((r)*(r)))*(error));
         /* Bail out once we get 30 dB gain */

         if (error<0.001f*ac[0])
            break;
      }
   }

}

public
void celt_fir(      opus_val16 *x,
               opus_val16 *num,
         opus_val16 *y,
         int N,
         int ord,
         opus_val16 *mem)
{
   int i,j;

   for (i=0;i<N;i++)
   {
      opus_val32 sum = ((x[i]));
      for (j=0;j<ord;j++)
      {
         sum += ((opus_val32)(num[j])*(opus_val32)(mem[j]));
      }
      for (j=ord-1;j>=1;j--)
      {
         mem[j]=mem[j-1];
      }
      mem[0] = x[i];
      y[i] = (sum);
   }
}

public
void celt_iir(      opus_val32 *x,
               opus_val16 *den,
         opus_val32 *y,
         int N,
         int ord,
         opus_val16 *mem)
{
   int i,j;
   for (i=0;i<N;i++)
   {
      opus_val32 sum = x[i];
      for (j=0;j<ord;j++)
      {
         sum -= ((opus_val32)(den[j])*(opus_val32)(mem[j]));
      }
      for (j=ord-1;j>=1;j--)
      {
         mem[j]=mem[j-1];
      }
      mem[0] = (sum);
      y[i] = sum;
   }
}

public
void _celt_autocorr(
                         opus_val16 *x,   /*  in: [0...n-1] samples x   */
                   opus_val32       *ac,  /* out: [0...lag-1] ac values */
                         opus_val16       *window,
                   int          overlap,
                   int          lag,
                   int          n
                  )
{
   opus_val32 d;
   int i;
   opus_val16 *xx;
   ;
   xx = ((opus_val16*)malloc(((int)(opus_val16 ' size  ))*(n)));
   ;
   ;
   for (i=0;i<n;i++)
      xx[i] = x[i];
   for (i=0;i<overlap;i++)
   {
      xx[i] = ((x[i])*(window[i]));
      xx[n-i-1] = ((x[n-i-1])*(window[i]));
   }

   while (lag>=0)
   {
      for (i = lag, d = 0.0; i < n; i++)
         d += xx[i] * xx[i-lag];
      ac[lag] = d;
      /*printf ("%f ", ac[lag]);*/
      (*&lag)--;
   }
   /*printf ("\n");*/
   ac[0] += 10.0;

   afree((byte*)xx);
   ;
}
#end unsafe
