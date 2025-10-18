#begin unsafe
/* Copyright (c) 2001-2011 Timothy B. Terriberry
   Copyright (c) 2008-2009 Xiph.Org Foundation */
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
use entcode, ecintrin;

/*A range decoder.
  This is an entropy decoder based upon \cite{Mar79}, which is itself a
   rediscovery of the FIFO arithmetic code introduced by \cite{Pas76}.
  It is very similar to arithmetic encoding, except that encoding is done with
   digits in any base, instead of with bits, and so it is faster when using
   larger bases (i.e.: a byte).
  The author claims an average waste of $\frac{1}{2}\log_b(2b)$ bits, where $b$
   is the base, longer than the theoretical optimum, but to my knowledge there
   is no published justification for this claim.
  This only seems true when using near-infinite precision arithmetic so that
   the process is carried out with no rounding errors.

  An excellent description of implementation details is available at
   http://www.arturocampos.com/ac_range.html
  A recent work \cite{MNW98} which proposes several changes to arithmetic
   encoding for efficiency actually re-discovers many of the principles
   behind range encoding, and presents a good theoretical analysis of them.

  End of stream is handled by writing out the smallest number of bits that
   ensures that the stream will be correctly decoded regardless of the value of
   any subsequent bits.
  ec_tell() can be used to determine how many bits were needed to decode
   all the symbols thus far; other data can be packed in the remaining bits of
   the input buffer.
  @PHDTHESIS{Pas76,
    author="Richard Clark Pasco",
    title="Source coding algorithms for fast data compression",
    school="Dept. of Electrical Engineering, Stanford University",
    address="Stanford, CA",
    month=May,
    year=1976
  }
  @INPROCEEDINGS{Mar79,
   author="Martin, G.N.N.",
   title="Range encoding: an algorithm for removing redundancy from a digitised
    message",
   booktitle="Video & Data Recording Conference",
   year=1979,
   address="Southampton",
   month=Jul
  }
  @ARTICLE{MNW98,
   author="Alistair Moffat and Radford Neal and Ian H. Witten",
   title="Arithmetic Coding Revisited",
   journal="{ACM} Transactions on Information Systems",
   year=1998,
   volume=16,
   number=3,
   pages="256--294",
   month=Jul,
   URL="http://www.stanford.edu/class/ee398/handouts/papers/Moffat98ArithmCoding.pdf"
  }*/

int ec_read_byte(ec_dec *_this){
  return (int)(_this->offs<_this->storage?_this->buf[_this->offs++]:0);
}

int ec_read_byte_from_end(ec_dec *_this){
  return (int)(_this->end_offs<_this->storage?_this->buf[_this->storage-++(_this->end_offs)]:0);
}

/*Normalizes the contents of val and rng so that rng lies entirely in the
   high-order symbol.*/
void ec_dec_normalize(ec_dec *_this){
  /*If the range is too small, rescale it and input some bits.*/
  while(_this->rng<=((((opus_uint32)1)<<((32)-1))>>(8)))
  {
    int sym;
    _this->nbits_total+=(8);
    _this->rng<<=(8);
    /*Use up the remaining bits from our last symbol.*/
    sym=_this->rem;
    /*Read the next value from the input.*/
    _this->rem=ec_read_byte(_this);
    /*Take the rest of the bits we need from this new symbol.*/
    sym=(sym<<(8)|_this->rem)>>((8)-(((32)-2)%(8)+1));
    /*And subtract them from val, capped to be less than EC_CODE_TOP.*/
    _this->val=((_this->val<<(8))+(uint)(((1<<(8))-1)&~sym))&((((opus_uint32)1)<<((32)-1))-1);
  }
}

public void ec_dec_init(ec_dec *_this,    byte      *_buf,opus_uint32 _storage){
  _this->buf=_buf;
  _this->storage=_storage;
  _this->end_offs=0;
  _this->end_window=0;
  _this->nend_bits=0;
  /*This is the offset from which ec_tell() will subtract partial bits.
    The final value after the ec_dec_normalize() call will be the same as in
     the encoder, but we have to compensate for the bits that are added there.*/
  _this->nbits_total=(32)+1
   -(((32)-(((32)-2)%(8)+1))/(8))*(8);
  _this->offs=0;
  _this->rng=1<<(((32)-2)%(8)+1);
  _this->rem=ec_read_byte(_this);
  _this->val=_this->rng-1-(uint)(_this->rem>>((8)-(((32)-2)%(8)+1)));
  _this->error=0;
  /*Normalize the interval.*/
  ec_dec_normalize(_this);
}

public unsigned ec_decode(ec_dec *_this,unsigned _ft){
  unsigned s;
  _this->ext=_this->rng/_ft;
  s=(unsigned)(_this->val/_this->ext);
  return (_ft-((s+1)+(((_ft)-(s+1))&(uint)-(int)((_ft)<(s+1)))));
}

public unsigned ec_decode_bin(ec_dec *_this,unsigned _bits){
   unsigned s;
   _this->ext=_this->rng>>_bits;
  s=(unsigned)(_this->val/_this->ext);
   return (1<<_bits)-((s+1)+(((1<<_bits)-(s+1))&(uint)-(int)((1<<_bits)<(s+1))));
}

public void ec_dec_update(ec_dec *_this,unsigned _fl,unsigned _fh,unsigned _ft){
  opus_uint32 s;
  s=((_this->ext)*(_ft-_fh));
  _this->val-=s;
  _this->rng=_fl>0?((_this->ext)*(_fh-_fl)):_this->rng-s;
  ec_dec_normalize(_this);
}

/*The probability of having a "one" is 1/(1<<_logp).*/
public int ec_dec_bit_logp(ec_dec *_this,unsigned _logp){
  opus_uint32 r;
  opus_uint32 d;
  opus_uint32 s;
  int         ret;
  r=_this->rng;
  d=_this->val;
  s=r>>_logp;
  ret=(int)(d<s);
  if(ret==0)_this->val=d-s;
  _this->rng = ret!=0?s:r-s;
  ec_dec_normalize(_this);
  return ret;
}

public int ec_dec_icdf(ec_dec *_this,          byte      *_icdf,unsigned _ftb){
  opus_uint32 r;
  opus_uint32 d;
  opus_uint32 s;
  opus_uint32 t;
  int         ret;
  s=_this->rng;
  d=_this->val;
  r=s>>_ftb;
  ret=-1;
  for (;;)
  {
    t=s;
    s=((r)*(_icdf[++ret]));
    if (!(d<s)) break;
  }
  _this->val=d-s;
  _this->rng=t-s;
  ec_dec_normalize(_this);
  return ret;
}

public opus_uint32 ec_dec_uint(ec_dec *_this,opus_uint32 _ft){
  unsigned ft;
  unsigned s;
  int      ftb;
  /*In order to optimize EC_ILOG(), it is undefined for the value 0.*/
  ;
  (*&_ft)--;
  ftb=(ec_ilog(_ft));
  if(ftb>(8)){
    opus_uint32 t;
    ftb-=(8);
    ft=(unsigned)(_ft>>(uint)ftb)+1;
    s=ec_decode(_this,ft);
    ec_dec_update(_this,s,s+1,ft);
    t=(opus_uint32)s<<(uint)ftb|ec_dec_bits(_this,(uint)ftb);
    if(t<=_ft)return t;
    _this->error=1;
    return _ft;
  }
  else{
    (*&_ft)++;
    s=ec_decode(_this,(unsigned)_ft);
    ec_dec_update(_this,s,s+1,(unsigned)_ft);
    return s;
  }
}

public opus_uint32 ec_dec_bits(ec_dec *_this,unsigned _bits){
  ec_window   window;
  int         available;
  opus_uint32 ret;
  window=_this->end_window;
  available=_this->nend_bits;
  if((unsigned)available<_bits)
  {
    for (;;)
	{
      window|=(ec_window)ec_read_byte_from_end(_this)<<(uint)available;
      available+=(8);
      if (!(available<=((int)((int)(ec_window ' size  ))*8)-(8))) break;
	}
  }
  ret=(opus_uint32)window&(((opus_uint32)1<<_bits)-1);
  window>>=_bits;
  available -= (int)_bits;
  _this->end_window=window;
  _this->nend_bits=available;
  _this->nbits_total += (int)_bits;
  return ret;
}
#end unsafe
