#begin unsafe

use opus_types;

public int ec_ilog(opus_uint32 _v){
  /*On a Pentium M, this branchless version tested as the fastest on
     1,000,000,000 random 32-bit integers, edging out a similar version with
     branches, and a 256-entry LUT version.*/
  int ret;
  int m;
  
  ret=(int)!!(_v!=0);
  m=(int)((int)((_v&0xFFFF0000)!=0)<<4);
  (*&_v)>>=(uint)m;
  ret|=m;
  m=(int)((int)((_v&0xFF00)!= 0)<<3);
  (*&_v)>>=(uint)m;
  ret|=m;
  m=(int)((int)((_v&0xF0)!=0)<<2);
  (*&_v)>>=(uint)m;
  ret|=m;
  m=(int)((int)((_v&0xC)!=0)<<1);
  (*&_v)>>=(uint)m;
  ret|=m;
  ret+=(int)((int)((_v&0x2)!=0));
  return ret;
}
#end unsafe
