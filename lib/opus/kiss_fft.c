#begin unsafe
/*Copyright (c) 2003-2004, Mark Borgerding
  Lots of modifications by Jean-Marc Valin
  Copyright (c) 2005-2007, Xiph.Org Foundation
  Copyright (c) 2008,      Xiph.Org Foundation, CSIRO

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.*/

/* This code is originally from Mark Borgerding's KISS-FFT but has been
   heavily modified to better suit Opus */

use opus_types;

/* The guts header contains all the multiplication and addition macros that are defined for
   complex numbers.  It also delares the kf_ internal functions.
*/

void kf_bfly2(
                     kiss_fft_cpx * Fout,
                           size_t fstride,
                           kiss_fft_state *st,
                     int m,
                     int N,
                     int mm
                    )
{
   kiss_fft_cpx * Fout2;
   kiss_twiddle_cpx * tw1;
   int i,j;
   kiss_fft_cpx * Fout_beg = Fout;
   for (i=0;i<N;i++)
   {
      *&Fout = Fout_beg + i*mm;
      Fout2 = Fout + m;
      tw1 = &st->twiddles;
      for(j=0;j<m;j++)
      {
         kiss_fft_cpx t;
		 clear t;
         Fout->r = (Fout->r);Fout->i = (Fout->i);
         Fout2->r = (Fout2->r);Fout2->i = (Fout2->i);
         { (t).r = (*Fout2).r*(*tw1).r - (*Fout2).i*(*tw1).i; (t).i = (*Fout2).r*(*tw1).i + (*Fout2).i*(*tw1).r; }
         tw1 += fstride;
         {   (*Fout2).r=(*Fout).r-(t).r; (*Fout2).i=(*Fout).i-(t).i; }
         {   (*Fout).r += (t).r; (*Fout).i += (t).i; }
         ++Fout2;
         ++(*&Fout);
      }
   }
}

void ki_bfly2(
                     kiss_fft_cpx * Fout,
                           size_t fstride,
                           kiss_fft_state *st,
                     int m,
                     int N,
                     int mm
                    )
{
   kiss_fft_cpx * Fout2;
   kiss_twiddle_cpx * tw1;
   kiss_fft_cpx t;
   int i,j;
   kiss_fft_cpx * Fout_beg = Fout;
   clear t;
   for (i=0;i<N;i++)
   {
      *&Fout = Fout_beg + i*mm;
      Fout2 = Fout + m;
      tw1 = &st->twiddles;
      for(j=0;j<m;j++)
      {
         { (t).r = (*Fout2).r*(*tw1).r + (*Fout2).i*(*tw1).i; (t).i = (*Fout2).i*(*tw1).r - (*Fout2).r*(*tw1).i; }
         tw1 += fstride;
         {   (*Fout2).r=(*Fout).r-(t).r; (*Fout2).i=(*Fout).i-(t).i; }
         {   (*Fout).r += (t).r; (*Fout).i += (t).i; }
         ++Fout2;
         ++(*&Fout);
      }
   }
}

void kf_bfly4(       kiss_fft_cpx * Fout,
                     size_t fstride,
                     kiss_fft_state *st,
                     int m,
                     int N,
                     int mm
                    )
{
   kiss_twiddle_cpx * tw1,tw2,tw3;
   kiss_fft_cpx scratch[6];
   size_t m2=2*(uint)m;
   size_t m3=3*(uint)m;
   int i, j;
   kiss_fft_cpx * Fout_beg = Fout;
   
   clear scratch;
   
   for (i=0;i<N;i++)
   {
      *&Fout = Fout_beg + i*mm;
      tw1 = &st->twiddles;
	  tw2 = tw1;
	  tw3 = tw2;
      for (j=0;j<m;j++)
      {
         { (scratch[0]).r = (Fout[m]).r*(*tw1).r - (Fout[m]).i*(*tw1).i; (scratch[0]).i = (Fout[m]).r*(*tw1).i + (Fout[m]).i*(*tw1).r; }
         { (scratch[1]).r = (Fout[m2]).r*(*tw2).r - (Fout[m2]).i*(*tw2).i; (scratch[1]).i = (Fout[m2]).r*(*tw2).i + (Fout[m2]).i*(*tw2).r; }
         { (scratch[2]).r = (Fout[m3]).r*(*tw3).r - (Fout[m3]).i*(*tw3).i; (scratch[2]).i = (Fout[m3]).r*(*tw3).i + (Fout[m3]).i*(*tw3).r; }

         Fout->r = (Fout->r);
         Fout->i = (Fout->i);
         {   (scratch[5]).r=(*Fout).r-(scratch[1]).r; (scratch[5]).i=(*Fout).i-(scratch[1]).i; }
         {   (*Fout).r += (scratch[1]).r; (*Fout).i += (scratch[1]).i; }
         {   (scratch[3]).r=(scratch[0]).r+(scratch[2]).r; (scratch[3]).i=(scratch[0]).i+(scratch[2]).i; }
         {   (scratch[4]).r=(scratch[0]).r-(scratch[2]).r; (scratch[4]).i=(scratch[0]).i-(scratch[2]).i; }
         Fout[m2].r = (Fout[m2].r);
         Fout[m2].i = (Fout[m2].i);
         {   (Fout[m2]).r=(*Fout).r-(scratch[3]).r; (Fout[m2]).i=(*Fout).i-(scratch[3]).i; }
         tw1 += fstride;
         tw2 += fstride*2;
         tw3 += fstride*3;
         {   (*Fout).r += (scratch[3]).r; (*Fout).i += (scratch[3]).i; }

         Fout[m].r = scratch[5].r + scratch[4].i;
         Fout[m].i = scratch[5].i - scratch[4].r;
         Fout[m3].r = scratch[5].r - scratch[4].i;
         Fout[m3].i = scratch[5].i + scratch[4].r;
         ++(*&Fout);
      }
   }
}

void ki_bfly4(  kiss_fft_cpx * Fout,
                size_t fstride,
                kiss_fft_state *st,
                int m,
                int N,
                int mm)
{
   kiss_twiddle_cpx * tw1,tw2,tw3;
   kiss_fft_cpx scratch[6];
   size_t m2=2*(uint)m;
   size_t m3=3*(uint)m;
   int i, j;
   kiss_fft_cpx * Fout_beg = Fout;
   
   clear scratch;
   for (i=0;i<N;i++)
   {
      *&Fout = Fout_beg + i*mm;
      tw1 = &st->twiddles;
	  tw2 = tw1;
	  tw3 = tw2;
      for (j=0;j<m;j++)
      {
         {(scratch[0]).r = (Fout[m]).r*(*tw1).r + (Fout[m]).i*(*tw1).i; (scratch[0]).i = (Fout[m]).i*(*tw1).r - (Fout[m]).r*(*tw1).i;}
         {(scratch[1]).r = (Fout[m2]).r*(*tw2).r + (Fout[m2]).i*(*tw2).i; (scratch[1]).i = (Fout[m2]).i*(*tw2).r - (Fout[m2]).r*(*tw2).i;}
         {(scratch[2]).r = (Fout[m3]).r*(*tw3).r + (Fout[m3]).i*(*tw3).i; (scratch[2]).i = (Fout[m3]).i*(*tw3).r - (Fout[m3]).r*(*tw3).i;}

          { (scratch[5]).r=(*Fout).r-(scratch[1]).r; (scratch[5]).i=(*Fout).i-(scratch[1]).i;}
          { (*Fout).r += (scratch[1]).r; (*Fout).i += (scratch[1]).i;}
          { (scratch[3]).r=(scratch[0]).r+(scratch[2]).r; (scratch[3]).i=(scratch[0]).i+(scratch[2]).i;}
          { (scratch[4]).r=(scratch[0]).r-(scratch[2]).r; (scratch[4]).i=(scratch[0]).i-(scratch[2]).i;}
          { (Fout[m2]).r=(*Fout).r-(scratch[3]).r; (Fout[m2]).i=(*Fout).i-(scratch[3]).i;}
         tw1 += fstride;
         tw2 += fstride*2;
         tw3 += fstride*3;
          { (*Fout).r += (scratch[3]).r; (*Fout).i += (scratch[3]).i;}

         Fout[m].r = scratch[5].r - scratch[4].i;
         Fout[m].i = scratch[5].i + scratch[4].r;
         Fout[m3].r = scratch[5].r + scratch[4].i;
         Fout[m3].i = scratch[5].i - scratch[4].r;
         ++(*&Fout);
      }
   }
}

void kf_bfly3(
                     kiss_fft_cpx * Fout,
                           size_t fstride,
                           kiss_fft_state *st,
                     int m,
                     int N,
                     int mm
                    )
{
   int i;
   size_t k;
   size_t m2 = 2*(uint)m;
   kiss_twiddle_cpx * tw1,tw2;
   kiss_fft_cpx scratch[5];
   kiss_twiddle_cpx epi3;

   kiss_fft_cpx * Fout_beg = Fout;
   epi3 = st->twiddles[fstride*(uint)m];
   
   clear scratch;
   
   for (i=0;i<N;i++)
   {
      *&Fout = Fout_beg + i*mm;
      tw2=&st->twiddles;
	  tw1=tw2;
      k=(uint)m;
      for (;;)
	  {
         ; ; ;

         { (scratch[1]).r = (Fout[m]).r*(*tw1).r - (Fout[m]).i*(*tw1).i; (scratch[1]).i = (Fout[m]).r*(*tw1).i + (Fout[m]).i*(*tw1).r;}
         { (scratch[2]).r = (Fout[m2]).r*(*tw2).r - (Fout[m2]).i*(*tw2).i; (scratch[2]).i = (Fout[m2]).r*(*tw2).i + (Fout[m2]).i*(*tw2).r;}

         {  (scratch[3]).r=(scratch[1]).r+(scratch[2]).r; (scratch[3]).i=(scratch[1]).i+(scratch[2]).i;}
         {  (scratch[0]).r=(scratch[1]).r-(scratch[2]).r; (scratch[0]).i=(scratch[1]).i-(scratch[2]).i;}
         tw1 += fstride;
         tw2 += fstride*2;

         Fout[m].r = Fout->r - ((scratch[3].r)*0.5f);
         Fout[m].i = Fout->i - ((scratch[3].i)*0.5f);

         {(scratch[0]).r *= (epi3.i); (scratch[0]).i *= (epi3.i);}

          { (*Fout).r += (scratch[3]).r; (*Fout).i += (scratch[3]).i;}

         Fout[m2].r = Fout[m].r + scratch[0].i;
         Fout[m2].i = Fout[m].i - scratch[0].r;

         Fout[m].r -= scratch[0].i;
         Fout[m].i += scratch[0].r;

         ++(*&Fout);
        if ((--k)==0) break;
	  }
   }
}

void ki_bfly3(
                     kiss_fft_cpx * Fout,
                           size_t fstride,
                           kiss_fft_state *st,
                     int m,
                     int N,
                     int mm
                    )
{
   int i, k;
   size_t m2 = 2*(uint)m;
   kiss_twiddle_cpx *tw1,tw2;
   kiss_fft_cpx scratch[5];
   kiss_twiddle_cpx epi3;
   kiss_fft_cpx * Fout_beg = Fout;
   
   epi3 = st->twiddles[fstride*(uint)m];
   
   clear scratch;
   
   for (i=0;i<N;i++)
   {
      *&Fout = Fout_beg + i*mm;
      tw2=&st->twiddles;
	  tw1=tw2;
      k=m;
      for (;;)
	  {
        { (scratch[1]).r = (Fout[m]).r*(*tw1).r + (Fout[m]).i*(*tw1).i; (scratch[1]).i = (Fout[m]).i*(*tw1).r - (Fout[m]).r*(*tw1).i;}
        { (scratch[2]).r = (Fout[m2]).r*(*tw2).r + (Fout[m2]).i*(*tw2).i; (scratch[2]).i = (Fout[m2]).i*(*tw2).r - (Fout[m2]).r*(*tw2).i;}

        {   (scratch[3]).r=(scratch[1]).r+(scratch[2]).r; (scratch[3]).i=(scratch[1]).i+(scratch[2]).i;}
        {   (scratch[0]).r=(scratch[1]).r-(scratch[2]).r; (scratch[0]).i=(scratch[1]).i-(scratch[2]).i;}
         tw1 += fstride;
         tw2 += fstride*2;

         Fout[m].r = Fout->r - ((scratch[3].r)*0.5f);
         Fout[m].i = Fout->i - ((scratch[3].i)*0.5f);

         {(scratch[0]).r *= (-epi3.i); (scratch[0]).i *= (-epi3.i);}

         {(*Fout).r += (scratch[3]).r; (*Fout).i += (scratch[3]).i;}

         Fout[m2].r = Fout[m].r + scratch[0].i;
         Fout[m2].i = Fout[m].i - scratch[0].r;

         Fout[m].r -= scratch[0].i;
         Fout[m].i += scratch[0].r;

         ++(*&Fout);
        if ((--k) == 0) break;
	  }
   }
}

void kf_bfly5(
                     kiss_fft_cpx * Fout,
                           size_t fstride,
                           kiss_fft_state *st,
                     int m,
                     int N,
                     int mm
                    )
{
   kiss_fft_cpx * Fout0,Fout1,Fout2,Fout3,Fout4;
   int i;
   uint u;
   kiss_fft_cpx scratch[13];
   kiss_twiddle_cpx * twiddles = &st->twiddles;
   kiss_twiddle_cpx *tw;
   kiss_twiddle_cpx ya,yb;
   kiss_fft_cpx * Fout_beg = Fout;

   ya = twiddles[fstride*(uint)m];
   yb = twiddles[fstride*(uint)(2*m)];
   tw=&st->twiddles;

   clear scratch;
   
   for (i=0;i<N;i++)
   {
      *&Fout = Fout_beg + i*mm;
      Fout0=Fout;
      Fout1=Fout0+m;
      Fout2=Fout0+2*m;
      Fout3=Fout0+3*m;
      Fout4=Fout0+4*m;

      for ( u=0; (int)u<m; ++u ) {
         ; ; ; ; ;
         scratch[0] = *Fout0;

         {(scratch[1]).r = (*Fout1).r*(tw[u*fstride]).r - (*Fout1).i*(tw[u*fstride]).i; (scratch[1]).i = (*Fout1).r*(tw[u*fstride]).i + (*Fout1).i*(tw[u*fstride]).r;}
         {(scratch[2]).r = (*Fout2).r*(tw[2*u*fstride]).r - (*Fout2).i*(tw[2*u*fstride]).i; (scratch[2]).i = (*Fout2).r*(tw[2*u*fstride]).i + (*Fout2).i*(tw[2*u*fstride]).r;}
         {(scratch[3]).r = (*Fout3).r*(tw[3*u*fstride]).r - (*Fout3).i*(tw[3*u*fstride]).i; (scratch[3]).i = (*Fout3).r*(tw[3*u*fstride]).i + (*Fout3).i*(tw[3*u*fstride]).r;}
         {(scratch[4]).r = (*Fout4).r*(tw[4*u*fstride]).r - (*Fout4).i*(tw[4*u*fstride]).i; (scratch[4]).i = (*Fout4).r*(tw[4*u*fstride]).i + (*Fout4).i*(tw[4*u*fstride]).r;}

           {(scratch[7]).r=(scratch[1]).r+(scratch[4]).r; (scratch[7]).i=(scratch[1]).i+(scratch[4]).i;}
           {(scratch[10]).r=(scratch[1]).r-(scratch[4]).r; (scratch[10]).i=(scratch[1]).i-(scratch[4]).i;}
           {(scratch[8]).r=(scratch[2]).r+(scratch[3]).r; (scratch[8]).i=(scratch[2]).i+(scratch[3]).i;}
           {(scratch[9]).r=(scratch[2]).r-(scratch[3]).r; (scratch[9]).i=(scratch[2]).i-(scratch[3]).i;}

         Fout0->r += scratch[7].r + scratch[8].r;
         Fout0->i += scratch[7].i + scratch[8].i;

         scratch[5].r = scratch[0].r + ( (scratch[7].r)*(ya.r) ) + ( (scratch[8].r)*(yb.r) );
         scratch[5].i = scratch[0].i + ( (scratch[7].i)*(ya.r) ) + ( (scratch[8].i)*(yb.r) );

         scratch[6].r =  ( (scratch[10].i)*(ya.i) ) + ( (scratch[9].i)*(yb.i) );
         scratch[6].i = -( (scratch[10].r)*(ya.i) ) - ( (scratch[9].r)*(yb.i) );

         {  (*Fout1).r=(scratch[5]).r-(scratch[6]).r; (*Fout1).i=(scratch[5]).i-(scratch[6]).i;}
         {  (*Fout4).r=(scratch[5]).r+(scratch[6]).r; (*Fout4).i=(scratch[5]).i+(scratch[6]).i;}

         scratch[11].r = scratch[0].r + ( (scratch[7].r)*(yb.r) ) + ( (scratch[8].r)*(ya.r) );
         scratch[11].i = scratch[0].i + ( (scratch[7].i)*(yb.r) ) + ( (scratch[8].i)*(ya.r) );
         scratch[12].r = - ( (scratch[10].i)*(yb.i) ) + ( (scratch[9].i)*(ya.i) );
         scratch[12].i = ( (scratch[10].r)*(yb.i) ) - ( (scratch[9].r)*(ya.i) );

          { (*Fout2).r=(scratch[11]).r+(scratch[12]).r; (*Fout2).i=(scratch[11]).i+(scratch[12]).i;}
          { (*Fout3).r=(scratch[11]).r-(scratch[12]).r; (*Fout3).i=(scratch[11]).i-(scratch[12]).i;}

         ++Fout0;++Fout1;++Fout2;++Fout3;++Fout4;
      }
   }
}

void ki_bfly5(
                     kiss_fft_cpx * Fout,
                           size_t fstride,
                           kiss_fft_state *st,
                     int m,
                     int N,
                     int mm
                    )
{
   kiss_fft_cpx *Fout0,Fout1,Fout2,Fout3,Fout4;
   int i;
   uint u;
   kiss_fft_cpx scratch[13];
   kiss_twiddle_cpx * twiddles = &st->twiddles;
   kiss_twiddle_cpx *tw;
   kiss_twiddle_cpx ya,yb;
   kiss_fft_cpx * Fout_beg = Fout;

   ya = twiddles[fstride*(uint)m];
   yb = twiddles[fstride*2*(uint)m];
   tw=&st->twiddles;

   clear scratch;
   
   for (i=0;i<N;i++)
   {
      *&Fout = Fout_beg + i*mm;
      Fout0=Fout;
      Fout1=Fout0+m;
      Fout2=Fout0+2*m;
      Fout3=Fout0+3*m;
      Fout4=Fout0+4*m;

      for ( u=0; (int)u<m; ++u ) {
         scratch[0] = *Fout0;

        { (scratch[1]).r = (*Fout1).r*(tw[u*fstride]).r + (*Fout1).i*(tw[u*fstride]).i; (scratch[1]).i = (*Fout1).i*(tw[u*fstride]).r - (*Fout1).r*(tw[u*fstride]).i;}
        { (scratch[2]).r = (*Fout2).r*(tw[2*u*fstride]).r + (*Fout2).i*(tw[2*u*fstride]).i; (scratch[2]).i = (*Fout2).i*(tw[2*u*fstride]).r - (*Fout2).r*(tw[2*u*fstride]).i;}
        { (scratch[3]).r = (*Fout3).r*(tw[3*u*fstride]).r + (*Fout3).i*(tw[3*u*fstride]).i; (scratch[3]).i = (*Fout3).i*(tw[3*u*fstride]).r - (*Fout3).r*(tw[3*u*fstride]).i;}
        { (scratch[4]).r = (*Fout4).r*(tw[4*u*fstride]).r + (*Fout4).i*(tw[4*u*fstride]).i; (scratch[4]).i = (*Fout4).i*(tw[4*u*fstride]).r - (*Fout4).r*(tw[4*u*fstride]).i;}

         {  (scratch[7]).r=(scratch[1]).r+(scratch[4]).r; (scratch[7]).i=(scratch[1]).i+(scratch[4]).i;}
         {  (scratch[10]).r=(scratch[1]).r-(scratch[4]).r; (scratch[10]).i=(scratch[1]).i-(scratch[4]).i;}
         {  (scratch[8]).r=(scratch[2]).r+(scratch[3]).r; (scratch[8]).i=(scratch[2]).i+(scratch[3]).i;}
         {  (scratch[9]).r=(scratch[2]).r-(scratch[3]).r; (scratch[9]).i=(scratch[2]).i-(scratch[3]).i;}

         Fout0->r += scratch[7].r + scratch[8].r;
         Fout0->i += scratch[7].i + scratch[8].i;

         scratch[5].r = scratch[0].r + ( (scratch[7].r)*(ya.r) ) + ( (scratch[8].r)*(yb.r) );
         scratch[5].i = scratch[0].i + ( (scratch[7].i)*(ya.r) ) + ( (scratch[8].i)*(yb.r) );

         scratch[6].r = -( (scratch[10].i)*(ya.i) ) - ( (scratch[9].i)*(yb.i) );
         scratch[6].i =  ( (scratch[10].r)*(ya.i) ) + ( (scratch[9].r)*(yb.i) );

          { (*Fout1).r=(scratch[5]).r-(scratch[6]).r; (*Fout1).i=(scratch[5]).i-(scratch[6]).i;}
          { (*Fout4).r=(scratch[5]).r+(scratch[6]).r; (*Fout4).i=(scratch[5]).i+(scratch[6]).i;}

         scratch[11].r = scratch[0].r + ( (scratch[7].r)*(yb.r) ) + ( (scratch[8].r)*(ya.r) );
         scratch[11].i = scratch[0].i + ( (scratch[7].i)*(yb.r) ) + ( (scratch[8].i)*(ya.r) );
         scratch[12].r =  ( (scratch[10].i)*(yb.i) ) - ( (scratch[9].i)*(ya.i) );
         scratch[12].i = -( (scratch[10].r)*(yb.i) ) + ( (scratch[9].r)*(ya.i) );

          { (*Fout2).r=(scratch[11]).r+(scratch[12]).r; (*Fout2).i=(scratch[11]).i+(scratch[12]).i;}
          { (*Fout3).r=(scratch[11]).r-(scratch[12]).r; (*Fout3).i=(scratch[11]).i-(scratch[12]).i;}

         ++Fout0;++Fout1;++Fout2;++Fout3;++Fout4;
      }
   }
}

public
void opus_fft(      kiss_fft_state *st,      kiss_fft_cpx *fin,kiss_fft_cpx *fout)
{
    int m2, m;
    int p;
    int L;
    int fstride[8];
    int i;
    int shift;

	clear fstride;
	
    /* st->shift can be -1 */
    shift = st->shift>0 ? st->shift : 0;

    ;
    /* Bit-reverse the input */
    for (i=0;i<st->nfft;i++)
    {
       fout[st->bitrev[i]] = fin[i];

       fout[st->bitrev[i]].r *= st->scale;
       fout[st->bitrev[i]].i *= st->scale;
    }

    fstride[0] = 1;
    L=0;
    for (;;)
	{
       p = st->factors[2*L];
       m = st->factors[2*L+1];
       fstride[L+1] = fstride[L]*p;
       L++;
      if (!(m!=1)) break;
	}
    m = st->factors[2*L-1];
    for (i=L-1;i>=0;i--)
    {
       if (i!=0)
          m2 = st->factors[2*i-1];
       else
          m2 = 1;
       switch (st->factors[2*i])
       {
       case 2:
          kf_bfly2(fout,(uint)(fstride[i]<<shift),st,m, fstride[i], m2);
          break;
       case 4:
          kf_bfly4(fout,(uint)(fstride[i]<<shift),st,m, fstride[i], m2);
          break;
 
       case 3:
          kf_bfly3(fout,(uint)(fstride[i]<<shift),st,m, fstride[i], m2);
          break;
       case 5:
          kf_bfly5(fout,(uint)(fstride[i]<<shift),st,m, fstride[i], m2);
          break;
		default:
		  abort;
       }
       m = m2;
    }
}

public
void opus_ifft(      kiss_fft_state *st,      kiss_fft_cpx *fin,kiss_fft_cpx *fout)
{
   int m2, m;
   int p;
   int L;
   int fstride[8];
   int i;
   int shift;

   clear fstride;
   
   /* st->shift can be -1 */
   shift = st->shift>0 ? st->shift : 0;
   ;
   /* Bit-reverse the input */
   for (i=0;i<st->nfft;i++)
      fout[st->bitrev[i]] = fin[i];

   fstride[0] = 1;
   L=0;
   for (;;)
   {
      p = st->factors[2*L];
      m = st->factors[2*L+1];
      fstride[L+1] = fstride[L]*p;
      L++;
     if (!(m!=1)) break;
   }
   m = st->factors[2*L-1];
   for (i=L-1;i>=0;i--)
   {
      if (i!=0)
         m2 = st->factors[2*i-1];
      else
         m2 = 1;
      switch (st->factors[2*i])
      {
      case 2:
         ki_bfly2(fout,(uint)(fstride[i]<<shift),st,m, fstride[i], m2);
         break;
      case 4:
         ki_bfly4(fout,(uint)(fstride[i]<<shift),st,m, fstride[i], m2);
         break;

      case 3:
         ki_bfly3(fout,(uint)(fstride[i]<<shift),st,m, fstride[i], m2);
         break;
      case 5:
         ki_bfly5(fout,(uint)(fstride[i]<<shift),st,m, fstride[i], m2);
         break;
	  default:
	    abort;
      }
      m = m2;
   }
}

#end unsafe
