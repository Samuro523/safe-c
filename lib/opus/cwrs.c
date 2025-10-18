#begin unsafe
/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2009 Xiph.Org Foundation
   Copyright (c) 2007-2009 Timothy B. Terriberry
   Written by Timothy B. Terriberry and Jean-Marc Valin */
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

use ../arithm;

use os_support;
use mathops, ecintrin, entcode, entenc, entdec;
use opus_types;


/*INV_TABLE[i] holds the multiplicative inverse of (2*i+1) mod 2**32.*/
const        opus_uint32 INV_TABLE[53]={
  0x00000001,0xAAAAAAAB,0xCCCCCCCD,0xB6DB6DB7,
  0x38E38E39,0xBA2E8BA3,0xC4EC4EC5,0xEEEEEEEF,
  0xF0F0F0F1,0x286BCA1B,0x3CF3CF3D,0xE9BD37A7,
  0xC28F5C29,0x684BDA13,0x4F72C235,0xBDEF7BDF,
  0x3E0F83E1,0x8AF8AF8B,0x914C1BAD,0x96F96F97,
  0xC18F9C19,0x2FA0BE83,0xA4FA4FA5,0x677D46CF,
  0x1A1F58D1,0xFAFAFAFB,0x8C13521D,0x586FB587,
  0xB823EE09,0xA08AD8F3,0xC10C9715,0xBEFBEFBF,
  0xC0FC0FC1,0x07A44C6B,0xA33F128D,0xE327A977,
  0xC7E3F1F9,0x962FC963,0x3F2B3885,0x613716AF,
  0x781948B1,0x2B2E43DB,0xFCFCFCFD,0x6FD0EB67,
  0xFA3F47E9,0xD2FD2FD3,0x3F4FD3F5,0xD4E25B9F,
  0x5F02A3A1,0xBF5A814B,0x7C32B16D,0xD3431B57,
  0xD8FD8FD9,
};

/*Computes (_a*_b-_c)/(2*_d+1) when the quotient is known to be exact.
  _a, _b, _c, and _d may be arbitrary so long as the arbitrary precision result
   fits in 32 bits, but currently the table for multiplicative inverses is only
   valid for _d<=52.*/
opus_uint32 imusdiv32odd(opus_uint32 _a,opus_uint32 _b,
 opus_uint32 _c,int _d){
  ;
  return (_a*_b-_c)*INV_TABLE[_d]&(0xFFFFFFFF);
}

/*Computes (_a*_b-_c)/_d when the quotient is known to be exact.
  _d does not actually have to be even, but imusdiv32odd will be faster when
   it's odd, so you should use that instead.
  _a and _d are assumed to be small (e.g., _a*_d fits in 32 bits; currently the
   table for multiplicative inverses is only valid for _d<=54).
  _b and _c may be arbitrary so long as the arbitrary precision reuslt fits in
   32 bits.*/
opus_uint32 imusdiv32even(opus_uint32 _a,opus_uint32 _b,
 opus_uint32 _c,int _d){
  opus_uint32 inv;
  int           mask;
  int           shift;
  int           one;
  ;
  ;
  shift=(ec_ilog((uint)(_d^(_d-1))));
  inv=INV_TABLE[(_d-1)>>shift];
  shift--;
  one=1<<shift;
  mask=one-1;
  return (_a*(_b>>(uint)shift)-(_c>>(uint)shift)+
   ((_a*(_b&(uint)mask)+(uint)one-(_c&(uint)mask))>>(uint)shift)-1)*inv&(0xFFFFFFFF);
}


/*Although derived separately, the pulse vector coding scheme is equivalent to
   a Pyramid Vector Quantizer \cite{Fis86}.
  Some additional notes about an early version appear at
   http://people.xiph.org/~tterribe/notes/cwrs.html, but the codebook ordering
   and the definitions of some terms have evolved since that was written.

  The conversion from a pulse vector to an integer index (encoding) and back
   (decoding) is governed by two related functions, V(N,K) and U(N,K).

  V(N,K) = the number of combinations, with replacement, of N items, taken K
   at a time, when a sign bit is added to each item taken at least once (i.e.,
   the number of N-dimensional unit pulse vectors with K pulses).
  One way to compute this is via
    V(N,K) = K>0 ? sum(k=1...K,2**k*choose(N,k)*choose(K-1,k-1)) : 1,
   where choose() is the binomial function.
  A table of values for N<10 and K<10 looks like:
  V[10][10] = {
    {1,  0,   0,    0,    0,     0,     0,      0,      0,       0},
    {1,  2,   2,    2,    2,     2,     2,      2,      2,       2},
    {1,  4,   8,   12,   16,    20,    24,     28,     32,      36},
    {1,  6,  18,   38,   66,   102,   146,    198,    258,     326},
    {1,  8,  32,   88,  192,   360,   608,    952,   1408,    1992},
    {1, 10,  50,  170,  450,  1002,  1970,   3530,   5890,    9290},
    {1, 12,  72,  292,  912,  2364,  5336,  10836,  20256,   35436},
    {1, 14,  98,  462, 1666,  4942, 12642,  28814,  59906,  115598},
    {1, 16, 128,  688, 2816,  9424, 27008,  68464, 157184,  332688},
    {1, 18, 162,  978, 4482, 16722, 53154, 148626, 374274,  864146}
  };

  U(N,K) = the number of such combinations wherein N-1 objects are taken at
   most K-1 at a time.
  This is given by
    U(N,K) = sum(k=0...K-1,V(N-1,k))
           = K>0 ? (V(N-1,K-1) + V(N,K-1))/2 : 0.
  The latter expression also makes clear that U(N,K) is half the number of such
   combinations wherein the first object is taken at least once.
  Although it may not be clear from either of these definitions, U(N,K) is the
   natural function to work with when enumerating the pulse vector codebooks,
   not V(N,K).
  U(N,K) is not well-defined for N=0, but with the extension
    U(0,K) = K>0 ? 0 : 1,
   the function becomes symmetric: U(N,K) = U(K,N), with a similar table:
  U[10][10] = {
    {1, 0,  0,   0,    0,    0,     0,     0,      0,      0},
    {0, 1,  1,   1,    1,    1,     1,     1,      1,      1},
    {0, 1,  3,   5,    7,    9,    11,    13,     15,     17},
    {0, 1,  5,  13,   25,   41,    61,    85,    113,    145},
    {0, 1,  7,  25,   63,  129,   231,   377,    575,    833},
    {0, 1,  9,  41,  129,  321,   681,  1289,   2241,   3649},
    {0, 1, 11,  61,  231,  681,  1683,  3653,   7183,  13073},
    {0, 1, 13,  85,  377, 1289,  3653,  8989,  19825,  40081},
    {0, 1, 15, 113,  575, 2241,  7183, 19825,  48639, 108545},
    {0, 1, 17, 145,  833, 3649, 13073, 40081, 108545, 265729}
  };

  With this extension, V(N,K) may be written in terms of U(N,K):
    V(N,K) = U(N,K) + U(N,K+1)
   for all N>=0, K>=0.
  Thus U(N,K+1) represents the number of combinations where the first element
   is positive or zero, and U(N,K) represents the number of combinations where
   it is negative.
  With a large enough table of U(N,K) values, we could write O(N) encoding
   and O(min(N*log(K),N+K)) decoding routines, but such a table would be
   prohibitively large for small embedded devices (K may be as large as 32767
   for small N, and N may be as large as 200).

  Both functions obey the same recurrence relation:
    V(N,K) = V(N-1,K) + V(N,K-1) + V(N-1,K-1),
    U(N,K) = U(N-1,K) + U(N,K-1) + U(N-1,K-1),
   for all N>0, K>0, with different initial conditions at N=0 or K=0.
  This allows us to      ruct a row of one of the tables above given the
   previous row or the next row.
  Thus we can derive O(NK) encoding and decoding routines with O(K) memory
   using only addition and subtraction.

  When encoding, we build up from the U(2,K) row and work our way forwards.
  When decoding, we need to start at the U(N,K) row and work our way backwards,
   which requires a means of computing U(N,K).
  U(N,K) may be computed from two previous values with the same N:
    U(N,K) = ((2*N-1)*U(N,K-1) - U(N,K-2))/(K-1) + U(N,K-2)
   for all N>1, and since U(N,K) is symmetric, a similar relation holds for two
   previous values with the same K:
    U(N,K>1) = ((2*K-1)*U(N-1,K) - U(N-2,K))/(N-1) + U(N-2,K)
   for all K>1.
  This allows us to      ruct an arbitrary row of the U(N,K) table by starting
   with the first two values, which are      ants.
  This saves roughly 2/3 the work in our O(NK) decoding routine, but costs O(K)
   multiplications.
  Similar relations can be derived for V(N,K), but are not used here.

  For N>0 and K>0, U(N,K) and V(N,K) take on the form of an (N-1)-degree
   polynomial for fixed N.
  The first few are
    U(1,K) = 1,
    U(2,K) = 2*K-1,
    U(3,K) = (2*K-2)*K+1,
    U(4,K) = (((4*K-6)*K+8)*K-3)/3,
    U(5,K) = ((((2*K-4)*K+10)*K-8)*K+3)/3,
   and
    V(1,K) = 2,
    V(2,K) = 4*K,
    V(3,K) = 4*K*K+2,
    V(4,K) = 8*(K*K+2)*K/3,
    V(5,K) = ((4*K*K+20)*K*K+6)/3,
   for all K>0.
  This allows us to derive O(N) encoding and O(N*log(K)) decoding routines for
   small N (and indeed decoding is also O(N) for N<3).

  @ARTICLE{Fis86,
    author="Thomas R. Fischer",
    title="A Pyramid Vector Quantizer",
    journal="IEEE Transactions on Information Theory",
    volume="IT-32",
    number=4,
    pages="568--583",
    month=Jul,
    year=1986
  }*/

/*Compute U(2,_k).
  Note that this may be called with _k=32768 (maxK[2]+1).*/
unsigned ucwrs2(unsigned _k){
  ;
  return _k+(_k-1);
}

/*Compute V(2,_k).*/
public opus_uint32 ncwrs2(int _k){
  ;
  return 4*(opus_uint32)_k;
}

/*Compute U(3,_k).
  Note that this may be called with _k=32768 (maxK[3]+1).*/
opus_uint32 ucwrs3(unsigned _k){
  ;
  return (2*(opus_uint32)_k-2)*_k+1;
}

/*Compute V(3,_k).*/
public opus_uint32 ncwrs3(int _k){
  ;
  return 2*(2*(unsigned)_k*(opus_uint32)_k+1);
}

/*Compute U(4,_k).*/
  opus_uint32 ucwrs4(int _k){
  ;
  return imusdiv32odd(2*(uint)_k,(2*(uint)_k-3)*(opus_uint32)_k+4,3,1);
}

/*Compute V(4,_k).*/
  public opus_uint32 ncwrs4(int _k){
  ;
  return (((uint)_k*(opus_uint32)_k+2)*(uint)_k)/3<<3;
}


/*Computes the next row/column of any recurrence that obeys the relation
   u[i][j]=u[i-1][j]+u[i][j-1]+u[i-1][j-1].
  _ui0 is the base case for the new row/column.*/
 void unext(opus_uint32 *_ui,unsigned _len,opus_uint32 _ui0){
  opus_uint32 ui1;
  unsigned      j;
  /*This do-while will overrun the array if we don't have storage for at least
     2 values.*/
  j=1;
  for (;;)
  {
    ui1=((((_ui[j])+(_ui[j-1])))+(_ui0));
    _ui[j-1]=_ui0;
    *&_ui0=ui1;
    if (!(++j<_len)) break;
  }
  _ui[j-1]=_ui0;
}

/*Computes the previous row/column of any recurrence that obeys the relation
   u[i-1][j]=u[i][j]-u[i][j-1]-u[i-1][j-1].
  _ui0 is the base case for the new row/column.*/
  void uprev(opus_uint32 *_ui,unsigned _n,opus_uint32 _ui0){
  opus_uint32 ui1;
  unsigned      j;
  /*This do-while will overrun the array if we don't have storage for at least
     2 values.*/
  j=1;
  for (;;)
  {
    ui1=((((_ui[j])-(_ui[j-1])))-(_ui0));
    _ui[j-1]=_ui0;
    *&_ui0=ui1;
    if (!(++j<_n)) break;
  }
  _ui[j-1]=_ui0;
}

/*Compute V(_n,_k), as well as U(_n,0..._k+1).
  _u: On exit, _u[i] contains U(_n,i) for i in [0..._k+1].*/
 public opus_uint32 ncwrs_urow(unsigned _n,unsigned _k,opus_uint32 *_u){
  opus_uint32 um2;
  unsigned      len;
  unsigned      k;
  len=_k+2;
  /*We require storage at least 3 values (e.g., _k>0).*/
  ;
  _u[0]=0;
  _u[1]=1; um2=1;

  /*_k>52 doesn't work in the false branch due to the limits of INV_TABLE,
    but _k isn't tested here because k<=52 for n=7*/
  if(_n<=6)
 {
    /*If _n==0, _u[0] should be 1 and the rest should be 0.*/
    /*If _n==1, _u[i] should be 1 for i>1.*/
    ;
    /*If _k==0, the following do-while loop will overflow the buffer.*/
    ;
    k=2;
    for (;;)
	{
	  _u[k]=(k<<1)-1;
      if (!(++k<len)) break;
	}  
    for(k=2;k<_n;k++)unext(_u+1,_k+1,1);
  }

  else{
    opus_uint32 um1;
    opus_uint32 n2m1;
    um1=(_n<<1)-1;
	n2m1=um1;
	_u[2]=n2m1;
    for(k=3;k<len;k++){
      /*U(N,K) = ((2*N-1)*U(N,K-1)-U(N,K-2))/(K-1) + U(N,K-2)*/
      um2=(uint)imusdiv32even(n2m1,um1,um2,(int)k-1)+um2;
	  _u[k]=um2;
      if(++k>=len)break;
      um1=(uint)imusdiv32odd(n2m1,um2,um1,((int)k-1)>>1)+um1;
	  _u[k]=um1;
    }
  }
  return _u[_k]+_u[_k+1];
}

/*Returns the _i'th combination of _k elements (at most 32767) chosen from a
   set of size 1 with associated sign bits.
  _y: Returns the vector of pulses.*/
  void cwrsi1(int _k,opus_uint32 _i,int *_y){
  int s;
  s=-(int)_i;
  _y[0]=(_k+s)^s;
}

/*Returns the _i'th combination of _k elements (at most 32767) chosen from a
   set of size 2 with associated sign bits.
  _y: Returns the vector of pulses.*/
  public void cwrsi2(int _k,opus_uint32 _i,int *_y){
  opus_uint32 p;
  int           s;
  int           yj;
  p=ucwrs2((uint)_k+1);
  s=-(int)(_i>=p);
  *&_i -= p&(uint)s;
  yj=_k;
  *&_k = ((int)_i+1)>>1;
  p=_k!=0?ucwrs2((uint)_k):0;
  *&_i-=p;
  yj-=_k;
  _y[0]=(yj+s)^s;
  cwrsi1(_k,_i,_y+1);
}

/*Returns the _i'th combination of _k elements (at most 32767) chosen from a
   set of size 3 with associated sign bits.
  _y: Returns the vector of pulses.*/
 public void cwrsi3(int _k,opus_uint32 _i,int *_y){
  opus_uint32 p;
  int           s;
  int           yj;
  p=(uint)ucwrs3((uint)_k+1);
  s=-(int)(_i>=p);
  *&_i-=p&(uint)s;
  yj=_k;
  /*Finds the maximum _k such that ucwrs3(_k)<=_i (tested for all
     _i<2147418113=U(3,32768)).*/
  *&_k = (int)(_i>0?(isqrt32(2*_i-1)+1)>>1:0);
  p= _k!=0 ? ucwrs3((uint)_k):0;
  *&_i-=p;
  yj-=_k;
  _y[0]=(yj+s)^s;
  cwrsi2(_k,_i,_y+1);
}

/*Returns the _i'th combination of _k elements (at most 1172) chosen from a set
   of size 4 with associated sign bits.
  _y: Returns the vector of pulses.*/
 public void cwrsi4(int _k,opus_uint32 _i,int *_y){
  opus_uint32 p;
  int           s;
  int           yj;
  int           kl;
  int           kr;
  p=ucwrs4(_k+1);
  s=-(int)(_i>=p);
  *&_i -= p&(uint)s;
  yj=_k;
  /*We could solve a cubic for k here, but the form of the direct solution does
     not lend itself well to exact integer arithmetic.
    Instead we do a binary search on U(4,K).*/
  kl=0;
  kr=_k;
  for(;;){
    *&_k=(kl+kr)>>1;
    p=_k!=0?ucwrs4(_k):0;
    if(p<_i){
      if(_k>=kr)break;
      kl=_k+1;
    }
    else if(p>_i)kr=_k-1;
    else break;
  }
  *&_i-=p;
  yj-=_k;
  _y[0]=(yj+s)^s;
  cwrsi3(_k,_i,_y+1);
}


/*Returns the _i'th combination of _k elements chosen from a set of size _n
   with associated sign bits.
  _y: Returns the vector of pulses.
  _u: Must contain entries [0..._k+1] of row _n of U() on input.
      Its contents will be destructively modified.*/
public void cwrsi(int _n,int _k,opus_uint32 _i,int *_y,opus_uint32 *_u){
  int j;
  ;
  j=0;
  for (;;)
  {
    opus_uint32 p;
    int           s;
    int           yj;
    p=_u[_k+1];
    s=-(int)(_i>=p);
    *&_i-=p&(uint)s;
    yj=_k;
    p=_u[_k];
    while(p>_i) *&p=_u[--(*&_k)];
    *&_i-=p;
    yj-=_k;
    _y[j]=(yj+s)^s;
    uprev(_u,(uint)_k+2,0);
    if (!(++j<_n)) break;
  }
}

/*Returns the index of the given combination of K elements chosen from a set
   of size 1 with associated sign bits.
  _y: The vector of pulses, whose sum of absolute values is K.
  _k: Returns K.*/
  opus_uint32 icwrs1(      int *_y,int *_k){
  *_k=abs(_y[0]);
  return (uint)(_y[0]<0);
}

/*Returns the index of the given combination of K elements chosen from a set
   of size 2 with associated sign bits.
  _y: The vector of pulses, whose sum of absolute values is K.
  _k: Returns K.*/
  public opus_uint32 icwrs2(      int *_y,int *_k){
  opus_uint32 i;
  int           k;
  i=icwrs1(_y+1,&k);
  i+= k!=0 ?ucwrs2((uint)k):0;
  k+=abs(_y[0]);
  if(_y[0]<0)i+=ucwrs2((uint)k+1);
  *_k=k;
  return i;
}

/*Returns the index of the given combination of K elements chosen from a set
   of size 3 with associated sign bits.
  _y: The vector of pulses, whose sum of absolute values is K.
  _k: Returns K.*/
  public opus_uint32 icwrs3(      int *_y,int *_k){
  opus_uint32 i;
  int           k;
  i=icwrs2(_y+1,&k);
  i+= k!=0 ? ucwrs3((uint)k):0;
  k+=abs(_y[0]);
  if(_y[0]<0)i+=ucwrs3((uint)k+1);
  *_k=k;
  return i;
}

/*Returns the index of the given combination of K elements chosen from a set
   of size 4 with associated sign bits.
  _y: The vector of pulses, whose sum of absolute values is K.
  _k: Returns K.*/
  public opus_uint32 icwrs4(      int *_y,int *_k){
  opus_uint32 i;
  int           k;
  i=icwrs3(_y+1,&k);
  i+=k!=0?ucwrs4(k):0;
  k+=abs(_y[0]);
  if(_y[0]<0)i+=ucwrs4(k+1);
  *_k=k;
  return i;
}


/*Returns the index of the given combination of K elements chosen from a set
   of size _n with associated sign bits.
  _y:  The vector of pulses, whose sum of absolute values must be _k.
  _nc: Returns V(_n,_k).*/
  public
  opus_uint32 icwrs(int _n,int _k,opus_uint32 *_nc,      int *_y,
 opus_uint32 *_u){
  opus_uint32 i;
  int           j;
  int           k;
  /*We can't unroll the first two iterations of the loop unless _n>=2.*/
  ;
  _u[0]=0;
  for(k=1;k<=_k+1;k++)_u[k]=(uint)(k<<1)-1;
  i=icwrs1(_y+_n-1,&k);
  j=_n-2;
  i+=_u[k];
  k+=abs(_y[j]);
  if(_y[j]<0)i+=_u[k+1];
  while(j-->0){
    unext(_u,(uint)_k+2,0);
    i+=_u[k];
    k+=abs(_y[j]);
    if(_y[j]<0)i+=_u[k+1];
  }
  *_nc=_u[k]+_u[k+1];
  return i;
}

public void encode_pulses(      int *_y,int _n,int _k,ec_enc *_enc){
  opus_uint32 i;
  ;

  switch(_n){
    case 2:{
      i=icwrs2(_y,&_k);
      ec_enc_uint(_enc,i,ncwrs2(_k));
    }break;
    case 3:{
      i=icwrs3(_y,&_k);
      ec_enc_uint(_enc,i,ncwrs3(_k));
    }break;
    case 4:{
      i=icwrs4(_y,&_k);
      ec_enc_uint(_enc,i,ncwrs4(_k));
    }break;
     default:
    {
      opus_uint32 *u;
      opus_uint32 nc;
      ;
      u = ((opus_uint32*)malloc(((int)(opus_uint32 ' size  ))*(_k+2)));
      i=icwrs(_n,_k,&nc,_y,u);
      ec_enc_uint(_enc,i,nc);
      afree((byte*)u);
	  ;

    }
    break;
  }
}

public void decode_pulses(int *_y,int _n,int _k,ec_dec *_dec)
{
  ;

   switch(_n){
    case 2:cwrsi2(_k,ec_dec_uint(_dec,ncwrs2(_k)),_y);break;
    case 3:cwrsi3(_k,ec_dec_uint(_dec,ncwrs3(_k)),_y);break;
    case 4:cwrsi4(_k,ec_dec_uint(_dec,ncwrs4(_k)),_y);break;
    default:
    {
      opus_uint32 *u;
      ;
      u = ((opus_uint32*)malloc(((int)(opus_uint32 ' size  ))*(_k+2)));
      cwrsi(_n,_k,ec_dec_uint(_dec,ncwrs_urow((uint)_n,(uint)_k,u)),_y,u);
      afree((byte*)u);
	  ;

    }
    break;
  }
}
#end unsafe
