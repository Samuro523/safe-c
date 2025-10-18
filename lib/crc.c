
// crc.c : md5, sha-2, adler and crc checksums

use thread;

//---------------------------------------------------------------------------

// full MD5 type

struct MD5_INFO
{
  uint state[4];
  uint count[2];
  byte buffer[64];
}

//---------------------------------------------------------------------------

// full SHA2 types

const uint SHA256_BLOCK_SIZE_POWER = 6;
const uint SHA512_BLOCK_SIZE_POWER = 7;

const uint SHA256_BLOCK_SIZE = 1 << SHA256_BLOCK_SIZE_POWER;  //  64 bytes
const uint SHA512_BLOCK_SIZE = 1 << SHA512_BLOCK_SIZE_POWER;  // 128 bytes

struct SHA256_INFO
{
  long  tot_len;
  uint  len;
  byte  block[2 * SHA256_BLOCK_SIZE];   // a double buffer
  uint4 h[8];
}

struct SHA512_INFO
{
  long tot_len_low;
  long tot_len_high;
  uint len;
  byte block[2 * SHA512_BLOCK_SIZE];   // a double buffer
  int8 h[8];
}

//---------------------------------------------------------------------------

package MD5_DATA

  const byte PADDING[64] =
   {0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

end MD5_DATA;

public void MD5_init (out MD5_INFO info)
{
  clear info;
  info.state = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
}

//---------------------------------------------------------------------------

void Encode (out byte[] output, uint[] input)
{
  int  i, j;
  uint value;

  clear output;

  for (i=0, j=0; j<output'length; i++, j+=4)
  {
    value = input[i];
    output[j]   = (byte)value;
    output[j+1] = (byte)(value >> 8);
    output[j+2] = (byte)(value >> 16);
    output[j+3] = (byte)(value >> 24);
  }
}

//---------------------------------------------------------------------------

void Decode (out uint[] output, byte[] input)
{
  int i, j;

  clear output;

  for (i=0, j=0; j<input'length; i++, j+=4)
  {
    output[i] = ((uint)input[j]) | (((uint)input[j+1]) << 8) |
      (((uint)input[j+2]) << 16) | (((uint)input[j+3]) << 24);
  }
}

//---------------------------------------------------------------------------

void MD5Transform (ref uint state[4], byte block[64])
{
  uint a = state[0], b = state[1], c = state[2], d = state[3], x[16];

  Decode (out x, block);

  
  { (a) += ((((b)) & ((c))) | ((~(b)) & ((d)))) + (x[ 0]) + (uint)(0xd76aa478); (a) = ((((a)) << ((7))) | (((a)) >> (32-((7))))); (a) += (b); }; 
  { (d) += ((((a)) & ((b))) | ((~(a)) & ((c)))) + (x[ 1]) + (uint)(0xe8c7b756); (d) = ((((d)) << ((12))) | (((d)) >> (32-((12))))); (d) += (a); }; 
  { (c) += ((((d)) & ((a))) | ((~(d)) & ((b)))) + (x[ 2]) + (uint)(0x242070db); (c) = ((((c)) << ((17))) | (((c)) >> (32-((17))))); (c) += (d); }; 
  { (b) += ((((c)) & ((d))) | ((~(c)) & ((a)))) + (x[ 3]) + (uint)(0xc1bdceee); (b) = ((((b)) << ((22))) | (((b)) >> (32-((22))))); (b) += (c); }; 
  { (a) += ((((b)) & ((c))) | ((~(b)) & ((d)))) + (x[ 4]) + (uint)(0xf57c0faf); (a) = ((((a)) << ((7))) | (((a)) >> (32-((7))))); (a) += (b); }; 
  { (d) += ((((a)) & ((b))) | ((~(a)) & ((c)))) + (x[ 5]) + (uint)(0x4787c62a); (d) = ((((d)) << ((12))) | (((d)) >> (32-((12))))); (d) += (a); }; 
  { (c) += ((((d)) & ((a))) | ((~(d)) & ((b)))) + (x[ 6]) + (uint)(0xa8304613); (c) = ((((c)) << ((17))) | (((c)) >> (32-((17))))); (c) += (d); }; 
  { (b) += ((((c)) & ((d))) | ((~(c)) & ((a)))) + (x[ 7]) + (uint)(0xfd469501); (b) = ((((b)) << ((22))) | (((b)) >> (32-((22))))); (b) += (c); }; 
  { (a) += ((((b)) & ((c))) | ((~(b)) & ((d)))) + (x[ 8]) + (uint)(0x698098d8); (a) = ((((a)) << ((7))) | (((a)) >> (32-((7))))); (a) += (b); }; 
  { (d) += ((((a)) & ((b))) | ((~(a)) & ((c)))) + (x[ 9]) + (uint)(0x8b44f7af); (d) = ((((d)) << ((12))) | (((d)) >> (32-((12))))); (d) += (a); }; 
  { (c) += ((((d)) & ((a))) | ((~(d)) & ((b)))) + (x[10]) + (uint)(0xffff5bb1); (c) = ((((c)) << ((17))) | (((c)) >> (32-((17))))); (c) += (d); }; 
  { (b) += ((((c)) & ((d))) | ((~(c)) & ((a)))) + (x[11]) + (uint)(0x895cd7be); (b) = ((((b)) << ((22))) | (((b)) >> (32-((22))))); (b) += (c); }; 
  { (a) += ((((b)) & ((c))) | ((~(b)) & ((d)))) + (x[12]) + (uint)(0x6b901122); (a) = ((((a)) << ((7))) | (((a)) >> (32-((7))))); (a) += (b); }; 
  { (d) += ((((a)) & ((b))) | ((~(a)) & ((c)))) + (x[13]) + (uint)(0xfd987193); (d) = ((((d)) << ((12))) | (((d)) >> (32-((12))))); (d) += (a); }; 
  { (c) += ((((d)) & ((a))) | ((~(d)) & ((b)))) + (x[14]) + (uint)(0xa679438e); (c) = ((((c)) << ((17))) | (((c)) >> (32-((17))))); (c) += (d); }; 
  { (b) += ((((c)) & ((d))) | ((~(c)) & ((a)))) + (x[15]) + (uint)(0x49b40821); (b) = ((((b)) << ((22))) | (((b)) >> (32-((22))))); (b) += (c); }; 

  
  { (a) += ((((b)) & ((d))) | (((c)) & (~(d)))) + (x[ 1]) + (uint)(0xf61e2562); (a) = ((((a)) << ((5))) | (((a)) >> (32-((5))))); (a) += (b); }; 
  { (d) += ((((a)) & ((c))) | (((b)) & (~(c)))) + (x[ 6]) + (uint)(0xc040b340); (d) = ((((d)) << ((9))) | (((d)) >> (32-((9))))); (d) += (a); }; 
  { (c) += ((((d)) & ((b))) | (((a)) & (~(b)))) + (x[11]) + (uint)(0x265e5a51); (c) = ((((c)) << ((14))) | (((c)) >> (32-((14))))); (c) += (d); }; 
  { (b) += ((((c)) & ((a))) | (((d)) & (~(a)))) + (x[ 0]) + (uint)(0xe9b6c7aa); (b) = ((((b)) << ((20))) | (((b)) >> (32-((20))))); (b) += (c); }; 
  { (a) += ((((b)) & ((d))) | (((c)) & (~(d)))) + (x[ 5]) + (uint)(0xd62f105d); (a) = ((((a)) << ((5))) | (((a)) >> (32-((5))))); (a) += (b); }; 
  { (d) += ((((a)) & ((c))) | (((b)) & (~(c)))) + (x[10]) + (uint)(0x2441453); (d) = ((((d)) << ((9))) | (((d)) >> (32-((9))))); (d) += (a); }; 
  { (c) += ((((d)) & ((b))) | (((a)) & (~(b)))) + (x[15]) + (uint)(0xd8a1e681); (c) = ((((c)) << ((14))) | (((c)) >> (32-((14))))); (c) += (d); }; 
  { (b) += ((((c)) & ((a))) | (((d)) & (~(a)))) + (x[ 4]) + (uint)(0xe7d3fbc8); (b) = ((((b)) << ((20))) | (((b)) >> (32-((20))))); (b) += (c); }; 
  { (a) += ((((b)) & ((d))) | (((c)) & (~(d)))) + (x[ 9]) + (uint)(0x21e1cde6); (a) = ((((a)) << ((5))) | (((a)) >> (32-((5))))); (a) += (b); }; 
  { (d) += ((((a)) & ((c))) | (((b)) & (~(c)))) + (x[14]) + (uint)(0xc33707d6); (d) = ((((d)) << ((9))) | (((d)) >> (32-((9))))); (d) += (a); }; 
  { (c) += ((((d)) & ((b))) | (((a)) & (~(b)))) + (x[ 3]) + (uint)(0xf4d50d87); (c) = ((((c)) << ((14))) | (((c)) >> (32-((14))))); (c) += (d); }; 
  { (b) += ((((c)) & ((a))) | (((d)) & (~(a)))) + (x[ 8]) + (uint)(0x455a14ed); (b) = ((((b)) << ((20))) | (((b)) >> (32-((20))))); (b) += (c); }; 
  { (a) += ((((b)) & ((d))) | (((c)) & (~(d)))) + (x[13]) + (uint)(0xa9e3e905); (a) = ((((a)) << ((5))) | (((a)) >> (32-((5))))); (a) += (b); }; 
  { (d) += ((((a)) & ((c))) | (((b)) & (~(c)))) + (x[ 2]) + (uint)(0xfcefa3f8); (d) = ((((d)) << ((9))) | (((d)) >> (32-((9))))); (d) += (a); }; 
  { (c) += ((((d)) & ((b))) | (((a)) & (~(b)))) + (x[ 7]) + (uint)(0x676f02d9); (c) = ((((c)) << ((14))) | (((c)) >> (32-((14))))); (c) += (d); }; 
  { (b) += ((((c)) & ((a))) | (((d)) & (~(a)))) + (x[12]) + (uint)(0x8d2a4c8a); (b) = ((((b)) << ((20))) | (((b)) >> (32-((20))))); (b) += (c); }; 

  
  { (a) += (((b)) ^ ((c)) ^ ((d))) + (x[ 5]) + (uint)(0xfffa3942); (a) = ((((a)) << ((4))) | (((a)) >> (32-((4))))); (a) += (b); }; 
  { (d) += (((a)) ^ ((b)) ^ ((c))) + (x[ 8]) + (uint)(0x8771f681); (d) = ((((d)) << ((11))) | (((d)) >> (32-((11))))); (d) += (a); }; 
  { (c) += (((d)) ^ ((a)) ^ ((b))) + (x[11]) + (uint)(0x6d9d6122); (c) = ((((c)) << ((16))) | (((c)) >> (32-((16))))); (c) += (d); }; 
  { (b) += (((c)) ^ ((d)) ^ ((a))) + (x[14]) + (uint)(0xfde5380c); (b) = ((((b)) << ((23))) | (((b)) >> (32-((23))))); (b) += (c); }; 
  { (a) += (((b)) ^ ((c)) ^ ((d))) + (x[ 1]) + (uint)(0xa4beea44); (a) = ((((a)) << ((4))) | (((a)) >> (32-((4))))); (a) += (b); }; 
  { (d) += (((a)) ^ ((b)) ^ ((c))) + (x[ 4]) + (uint)(0x4bdecfa9); (d) = ((((d)) << ((11))) | (((d)) >> (32-((11))))); (d) += (a); }; 
  { (c) += (((d)) ^ ((a)) ^ ((b))) + (x[ 7]) + (uint)(0xf6bb4b60); (c) = ((((c)) << ((16))) | (((c)) >> (32-((16))))); (c) += (d); }; 
  { (b) += (((c)) ^ ((d)) ^ ((a))) + (x[10]) + (uint)(0xbebfbc70); (b) = ((((b)) << ((23))) | (((b)) >> (32-((23))))); (b) += (c); }; 
  { (a) += (((b)) ^ ((c)) ^ ((d))) + (x[13]) + (uint)(0x289b7ec6); (a) = ((((a)) << ((4))) | (((a)) >> (32-((4))))); (a) += (b); }; 
  { (d) += (((a)) ^ ((b)) ^ ((c))) + (x[ 0]) + (uint)(0xeaa127fa); (d) = ((((d)) << ((11))) | (((d)) >> (32-((11))))); (d) += (a); }; 
  { (c) += (((d)) ^ ((a)) ^ ((b))) + (x[ 3]) + (uint)(0xd4ef3085); (c) = ((((c)) << ((16))) | (((c)) >> (32-((16))))); (c) += (d); }; 
  { (b) += (((c)) ^ ((d)) ^ ((a))) + (x[ 6]) + (uint)(0x4881d05); (b) = ((((b)) << ((23))) | (((b)) >> (32-((23))))); (b) += (c); }; 
  { (a) += (((b)) ^ ((c)) ^ ((d))) + (x[ 9]) + (uint)(0xd9d4d039); (a) = ((((a)) << ((4))) | (((a)) >> (32-((4))))); (a) += (b); }; 
  { (d) += (((a)) ^ ((b)) ^ ((c))) + (x[12]) + (uint)(0xe6db99e5); (d) = ((((d)) << ((11))) | (((d)) >> (32-((11))))); (d) += (a); }; 
  { (c) += (((d)) ^ ((a)) ^ ((b))) + (x[15]) + (uint)(0x1fa27cf8); (c) = ((((c)) << ((16))) | (((c)) >> (32-((16))))); (c) += (d); }; 
  { (b) += (((c)) ^ ((d)) ^ ((a))) + (x[ 2]) + (uint)(0xc4ac5665); (b) = ((((b)) << ((23))) | (((b)) >> (32-((23))))); (b) += (c); }; 

  
  { (a) += (((c)) ^ (((b)) | (~(d)))) + (x[ 0]) + (uint)(0xf4292244); (a) = ((((a)) << ((6))) | (((a)) >> (32-((6))))); (a) += (b); }; 
  { (d) += (((b)) ^ (((a)) | (~(c)))) + (x[ 7]) + (uint)(0x432aff97); (d) = ((((d)) << ((10))) | (((d)) >> (32-((10))))); (d) += (a); }; 
  { (c) += (((a)) ^ (((d)) | (~(b)))) + (x[14]) + (uint)(0xab9423a7); (c) = ((((c)) << ((15))) | (((c)) >> (32-((15))))); (c) += (d); }; 
  { (b) += (((d)) ^ (((c)) | (~(a)))) + (x[ 5]) + (uint)(0xfc93a039); (b) = ((((b)) << ((21))) | (((b)) >> (32-((21))))); (b) += (c); }; 
  { (a) += (((c)) ^ (((b)) | (~(d)))) + (x[12]) + (uint)(0x655b59c3); (a) = ((((a)) << ((6))) | (((a)) >> (32-((6))))); (a) += (b); }; 
  { (d) += (((b)) ^ (((a)) | (~(c)))) + (x[ 3]) + (uint)(0x8f0ccc92); (d) = ((((d)) << ((10))) | (((d)) >> (32-((10))))); (d) += (a); }; 
  { (c) += (((a)) ^ (((d)) | (~(b)))) + (x[10]) + (uint)(0xffeff47d); (c) = ((((c)) << ((15))) | (((c)) >> (32-((15))))); (c) += (d); }; 
  { (b) += (((d)) ^ (((c)) | (~(a)))) + (x[ 1]) + (uint)(0x85845dd1); (b) = ((((b)) << ((21))) | (((b)) >> (32-((21))))); (b) += (c); }; 
  { (a) += (((c)) ^ (((b)) | (~(d)))) + (x[ 8]) + (uint)(0x6fa87e4f); (a) = ((((a)) << ((6))) | (((a)) >> (32-((6))))); (a) += (b); }; 
  { (d) += (((b)) ^ (((a)) | (~(c)))) + (x[15]) + (uint)(0xfe2ce6e0); (d) = ((((d)) << ((10))) | (((d)) >> (32-((10))))); (d) += (a); }; 
  { (c) += (((a)) ^ (((d)) | (~(b)))) + (x[ 6]) + (uint)(0xa3014314); (c) = ((((c)) << ((15))) | (((c)) >> (32-((15))))); (c) += (d); }; 
  { (b) += (((d)) ^ (((c)) | (~(a)))) + (x[13]) + (uint)(0x4e0811a1); (b) = ((((b)) << ((21))) | (((b)) >> (32-((21))))); (b) += (c); }; 
  { (a) += (((c)) ^ (((b)) | (~(d)))) + (x[ 4]) + (uint)(0xf7537e82); (a) = ((((a)) << ((6))) | (((a)) >> (32-((6))))); (a) += (b); }; 
  { (d) += (((b)) ^ (((a)) | (~(c)))) + (x[11]) + (uint)(0xbd3af235); (d) = ((((d)) << ((10))) | (((d)) >> (32-((10))))); (d) += (a); }; 
  { (c) += (((a)) ^ (((d)) | (~(b)))) + (x[ 2]) + (uint)(0x2ad7d2bb); (c) = ((((c)) << ((15))) | (((c)) >> (32-((15))))); (c) += (d); }; 
  { (b) += (((d)) ^ (((c)) | (~(a)))) + (x[ 9]) + (uint)(0xeb86d391); (b) = ((((b)) << ((21))) | (((b)) >> (32-((21))))); (b) += (c); }; 

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

//---------------------------------------------------------------------------

public void MD5_update (ref MD5_INFO info, byte[] buffer)
{
  uint i, index, partLen;

  index = (info.count[0] >> 3) & 0x3F;

  info.count[0] += (buffer'size << 3);

  if (info.count[0] < (buffer'size << 3))
    info.count[1]++;

  info.count[1] += (buffer'size >> 29);

  partLen = 64 - index;

  
  if (buffer'size >= partLen)
  {
    info.buffer[index:partLen] = buffer[0:partLen];

    MD5Transform (ref info.state, info.buffer);

    for (i = partLen; i + 63 < buffer'size; i += 64)
      MD5Transform (ref info.state, buffer[i:64]);

    index = 0;
  }
  else
  {
    i = 0;
  }

  info.buffer[index:buffer'size-i] = buffer[i:buffer'size-i];
}

//---------------------------------------------------------------------------

public void MD5_final (ref MD5_INFO info, out byte[16] md5)
{
  byte bits[8];
  uint index, padLen;

  Encode (out bits, info.count);
  
  index = (info.count[0] >> 3) & 0x3f;
  padLen = (index < 56) ? (56 - index) : (120 - index);
  MD5_update (ref info, PADDING[0:padLen]);
  MD5_update (ref info, bits);

  Encode (out md5, info.state);

  clear info;
}

//---------------------------------------------------------------------------

package SHA2_DATA

#define unroll_loops  false   // enable loops unrolling

const uint4 sha256_h0[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

const int8 sha512_h0[8] = {0x6A09E667F3BCC908, -0x4498517A7B3558C5, 0x3C6EF372FE94F82B, -0x5AB00AC5A0E2C90F,
                           0x510E527FADE682D1, -0x64FA9773D4C193E1, 0x1F83D9ABFB41BD6B,  0x5BE0CD19137E2179};

const uint4 sha256_k[64] =
            {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
             0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
             0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
             0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
             0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
             0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
             0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
             0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
             0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
             0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
             0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
             0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
             0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
             0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
             0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
             0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

const int8 sha512_k[80] =
  { 0x428A2F98D728AE22,  0x7137449123EF65CD, -0x4A3F043013B2C4D1,  -0x164A245A7E762444,
    0x3956C25BF348B538,  0x59F111F1B605D019, -0x6DC07D5B50E6B065,  -0x54E3A12A25927EE8,
   -0x27F855675CFCFDBE,  0x12835B0145706FBE,  0x243185BE4EE4B28C,   0x550C7DC3D5FFB4E2,
    0x72BE5D74F27B896F, -0x7F214E01C4E9694F, -0x6423F958DA38EDCB,  -0x3E640E8B3096D96C,
   -0x1B64963E610EB52E, -0x1041B879C7B0DA1D,  0xFC19DC68B8CD5B5,    0x240CA1CC77AC9C65,
    0x2DE92C6F592B0275,  0x4A7484AA6EA6E483,  0x5CB0A9DCBD41FBD4,   0x76F988DA831153B5,
   -0x67C1AEAD11992055, -0x57CE3992D24BCDF0, -0x4FFCD8376704DEC1,  -0x40A680384110F11C,
   -0x391FF40CC257703E, -0x2A586EB86CF558DB,  0x6CA6351E003826F,    0x142929670A0E6E70,
    0x27B70A8546D22FFC,  0x2E1B21385C26C926,  0x4D2C6DFC5AC42AED,   0x53380D139D95B3DF,
    0x650A73548BAF63DE,  0x766A0ABB3C77B2A8, -0x7E3D36D1B812511A,  -0x6D8DD37AEB7DCAC5,
   -0x5D40175EB30EFC9C, -0x57E599B443BDCFFF, -0x3DB4748F2F07686F,  -0x3893AE5CF9AB41D0,
   -0x2E6D17E62910ADE8, -0x2966F9DBAA9A56F0, -0xBF1CA7AA88EDFD6,    0x106AA07032BBD1B8,
    0x19A4C116B8D2D0C8,  0x1E376C085141AB53,  0x2748774CDF8EEB99,   0x34B0BCB5E19B48A8,
    0x391C0CB3C5C95A63,  0x4ED8AA4AE3418ACB,  0x5B9CCA4F7763E373,   0x682E6FF3D6B2B8A3,
    0x748F82EE5DEFB2FC,  0x78A5636F43172F60, -0x7B3787EB5E0F548E,  -0x7338FDF7E59BC614,
   -0x6F410005DC9CE1D8, -0x5BAF9314217D4217, -0x41065C084D3986EB,  -0x398E870D1C8DACD5,
   -0x35D8C13115D99E64, -0x2E794738DE3F3DF9, -0x15258229321F14E2,  -0xA82B08011912E88,
    0x6F067AA72176FBA,   0xA637DC5A2C898A6,   0x113F9804BEF90DAE,   0x1B710B35131C471B,
    0x28DB77F523047D84,  0x32CAAB7B40C72493,  0x3C9EBE0A15C9BEBC,   0x431D67C49C100D4C,
    0x4CC5D4BECB3E42B6,  0x597F299CFC657E2A,  0x5FCB6FAB3AD6FAEC,   0x6C44198C4A475817};

end SHA2_DATA;


int8 USHR (int8 a, int b)
{
  return (b > 0) ? (((a >> 1) & int8'max) >> (b-1)) : a;
}

uint4 ROTR32 (uint4 x, uint4 n)
{
  return (x >> n) | (x << ((x'size << 3) - n));
}

uint4 CH32 (uint4 x, uint4 y, uint4 z)
{
  return (x & y) ^ (~x & z);
}

uint4 MAJ32 (uint4 x, uint4 y, uint4 z)
{
  return (x & y) ^ (x & z) ^ (y & z);
}

uint4 SHA256_F1 (uint4 x)
{
  return ROTR32(x,  2) ^ ROTR32(x, 13) ^ ROTR32(x, 22);
}

uint4 SHA256_F2 (uint4 x)
{
  return ROTR32(x,  6) ^ ROTR32(x, 11) ^ ROTR32(x, 25);
}

uint4 SHA256_F3 (uint4 x)
{
  return ROTR32(x,  7) ^ ROTR32(x, 18) ^ (x>>3);
}

uint4 SHA256_F4 (uint4 x)
{
  return ROTR32(x, 17) ^ ROTR32(x, 19) ^ (x>>10);
}

int8 ROTR64 (int8 x, int n)
{
  return USHR(x,n) | (x << (((int)x'size << 3) - n));
}

int8 CH64 (int8 x, int8 y, int8 z)
{
  return (x & y) ^ (~x & z);
}

int8 MAJ64 (int8 x, int8 y, int8 z)
{
  return (x & y) ^ (x & z) ^ (y & z);
}

int8 SHA512_F1 (int8 x)
{
  return ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39);
}

int8 SHA512_F2 (int8 x)
{
  return ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41);
}

int8 SHA512_F3 (int8 x)
{
  return ROTR64(x,  1) ^ ROTR64(x,  8) ^ USHR(x,7);
}

int8 SHA512_F4 (int8 x)
{
  return ROTR64(x, 19) ^ ROTR64(x, 61) ^ USHR(x,6);
}

void UNPACK32 (uint4 x, out byte[4] str)
{
  str = {x'byte[3], x'byte[2], x'byte[1], x'byte[0]};
}

void PACK32 (byte[4] str, out uint4 x)
{
  x'byte = {str[3], str[2], str[1], str[0]};
}

void UNPACK64 (int8 x, out byte[8] str)
{
  str = {x'byte[7], x'byte[6], x'byte[5], x'byte[4], x'byte[3], x'byte[2], x'byte[1], x'byte[0]};
}

void PACK64 (byte[8] str, out int8 x)
{
  x'byte = {str[7], str[6], str[5], str[4], str[3], str[2], str[1], str[0]};
}


void SHA256_SCR (ref uint4 w[64], uint i)
{
  w[i] = SHA256_F4(w[i - 2]) + w[i - 7] + SHA256_F3(w[i - 15]) + w[i - 16];
}

void SHA512_SCR (ref int8 w[80], uint i)
{
  w[i] = SHA512_F4(w[i - 2]) + w[i - 7] + SHA512_F3(w[i - 15]) + w[i - 16];
}

void SHA256_EXP (uint4 w[64], ref uint4 wv[8], int a, int b, int c, int d, int e, int f, int g, int h, uint j)
{
  uint4 t1, t2;
  t1 = wv[h] + SHA256_F2(wv[e]) + CH32(wv[e], wv[f], wv[g]) + sha256_k[j] + w[j];
  t2 = SHA256_F1(wv[a]) + MAJ32(wv[a], wv[b], wv[c]);
  wv[d] += t1;
  wv[h] = t1 + t2;
}

void SHA512_EXP (int8 w[80], ref int8 wv[8], int a, int b, int c, int d, int e, int f, int g ,int h, uint j)
{
  int8 t1, t2;
  t1 = wv[h] + SHA512_F2(wv[e]) + CH64(wv[e], wv[f], wv[g]) + sha512_k[j] + w[j];
  t2 = SHA512_F1(wv[a]) + MAJ64(wv[a], wv[b], wv[c]);
  wv[d] += t1;
  wv[h] = t1 + t2;
}


void sha256_transf (ref SHA256_INFO ctx, byte[] message, uint block_nb)
{
  uint4 w[64];
  uint4 wv[8];
  uint  sub_block;
  uint  i;

#if !unroll_loops
  uint  j;
  uint4 t1, t2;
#endif

#begin unsafe      // avoids a clear
  uint* p = &w;
  uint* q = &wv;
  _unused p;
  _unused q;
#end unsafe

  for (i=0; i<block_nb; i++)
  {
    sub_block = (i << 6);

#if !unroll_loops
    for (j=0; j<16; j++)
      PACK32 (message[sub_block+(j << 2):4], out w[j]);

    for (j = 16; j < 64; j++)
      SHA256_SCR (ref w, j);

    for (j = 0; j < 8; j++)
      wv[j] = ctx.h[j];

    for (j = 0; j < 64; j++)
    {
      t1 = wv[7] + SHA256_F2(wv[4]) + CH32(wv[4], wv[5], wv[6]) + sha256_k[j] + w[j];
      t2 = SHA256_F1(wv[0]) + MAJ32(wv[0], wv[1], wv[2]);
      wv[7] = wv[6];
      wv[6] = wv[5];
      wv[5] = wv[4];
      wv[4] = wv[3] + t1;
      wv[3] = wv[2];
      wv[2] = wv[1];
      wv[1] = wv[0];
      wv[0] = t1 + t2;
    }

    for (j = 0; j < 8; j++)
      ctx.h[j] += wv[j];
#else
    PACK32 (message[sub_block+ 0:4], out w[ 0]); PACK32 (message[sub_block+ 4:4], out w[ 1]);
    PACK32 (message[sub_block+ 8:4], out w[ 2]); PACK32 (message[sub_block+12:4], out w[ 3]);
    PACK32 (message[sub_block+16:4], out w[ 4]); PACK32 (message[sub_block+20:4], out w[ 5]);
    PACK32 (message[sub_block+24:4], out w[ 6]); PACK32 (message[sub_block+28:4], out w[ 7]);
    PACK32 (message[sub_block+32:4], out w[ 8]); PACK32 (message[sub_block+36:4], out w[ 9]);
    PACK32 (message[sub_block+40:4], out w[10]); PACK32 (message[sub_block+44:4], out w[11]);
    PACK32 (message[sub_block+48:4], out w[12]); PACK32 (message[sub_block+52:4], out w[13]);
    PACK32 (message[sub_block+56:4], out w[14]); PACK32 (message[sub_block+60:4], out w[15]);

    SHA256_SCR(ref w,16); SHA256_SCR(ref w,17); SHA256_SCR(ref w,18); SHA256_SCR(ref w,19);
    SHA256_SCR(ref w,20); SHA256_SCR(ref w,21); SHA256_SCR(ref w,22); SHA256_SCR(ref w,23);
    SHA256_SCR(ref w,24); SHA256_SCR(ref w,25); SHA256_SCR(ref w,26); SHA256_SCR(ref w,27);
    SHA256_SCR(ref w,28); SHA256_SCR(ref w,29); SHA256_SCR(ref w,30); SHA256_SCR(ref w,31);
    SHA256_SCR(ref w,32); SHA256_SCR(ref w,33); SHA256_SCR(ref w,34); SHA256_SCR(ref w,35);
    SHA256_SCR(ref w,36); SHA256_SCR(ref w,37); SHA256_SCR(ref w,38); SHA256_SCR(ref w,39);
    SHA256_SCR(ref w,40); SHA256_SCR(ref w,41); SHA256_SCR(ref w,42); SHA256_SCR(ref w,43);
    SHA256_SCR(ref w,44); SHA256_SCR(ref w,45); SHA256_SCR(ref w,46); SHA256_SCR(ref w,47);
    SHA256_SCR(ref w,48); SHA256_SCR(ref w,49); SHA256_SCR(ref w,50); SHA256_SCR(ref w,51);
    SHA256_SCR(ref w,52); SHA256_SCR(ref w,53); SHA256_SCR(ref w,54); SHA256_SCR(ref w,55);
    SHA256_SCR(ref w,56); SHA256_SCR(ref w,57); SHA256_SCR(ref w,58); SHA256_SCR(ref w,59);
    SHA256_SCR(ref w,60); SHA256_SCR(ref w,61); SHA256_SCR(ref w,62); SHA256_SCR(ref w,63);

    wv[0] = ctx.h[0]; wv[1] = ctx.h[1];   wv[2] = ctx.h[2]; wv[3] = ctx.h[3];
    wv[4] = ctx.h[4]; wv[5] = ctx.h[5];   wv[6] = ctx.h[6]; wv[7] = ctx.h[7];

    SHA256_EXP(w, ref wv,0,1,2,3,4,5,6,7, 0); SHA256_EXP(w, ref wv,7,0,1,2,3,4,5,6, 1);
    SHA256_EXP(w, ref wv,6,7,0,1,2,3,4,5, 2); SHA256_EXP(w, ref wv,5,6,7,0,1,2,3,4, 3);
    SHA256_EXP(w, ref wv,4,5,6,7,0,1,2,3, 4); SHA256_EXP(w, ref wv,3,4,5,6,7,0,1,2, 5);
    SHA256_EXP(w, ref wv,2,3,4,5,6,7,0,1, 6); SHA256_EXP(w, ref wv,1,2,3,4,5,6,7,0, 7);
    SHA256_EXP(w, ref wv,0,1,2,3,4,5,6,7, 8); SHA256_EXP(w, ref wv,7,0,1,2,3,4,5,6, 9);
    SHA256_EXP(w, ref wv,6,7,0,1,2,3,4,5,10); SHA256_EXP(w, ref wv,5,6,7,0,1,2,3,4,11);
    SHA256_EXP(w, ref wv,4,5,6,7,0,1,2,3,12); SHA256_EXP(w, ref wv,3,4,5,6,7,0,1,2,13);
    SHA256_EXP(w, ref wv,2,3,4,5,6,7,0,1,14); SHA256_EXP(w, ref wv,1,2,3,4,5,6,7,0,15);
    SHA256_EXP(w, ref wv,0,1,2,3,4,5,6,7,16); SHA256_EXP(w, ref wv,7,0,1,2,3,4,5,6,17);
    SHA256_EXP(w, ref wv,6,7,0,1,2,3,4,5,18); SHA256_EXP(w, ref wv,5,6,7,0,1,2,3,4,19);
    SHA256_EXP(w, ref wv,4,5,6,7,0,1,2,3,20); SHA256_EXP(w, ref wv,3,4,5,6,7,0,1,2,21);
    SHA256_EXP(w, ref wv,2,3,4,5,6,7,0,1,22); SHA256_EXP(w, ref wv,1,2,3,4,5,6,7,0,23);
    SHA256_EXP(w, ref wv,0,1,2,3,4,5,6,7,24); SHA256_EXP(w, ref wv,7,0,1,2,3,4,5,6,25);
    SHA256_EXP(w, ref wv,6,7,0,1,2,3,4,5,26); SHA256_EXP(w, ref wv,5,6,7,0,1,2,3,4,27);
    SHA256_EXP(w, ref wv,4,5,6,7,0,1,2,3,28); SHA256_EXP(w, ref wv,3,4,5,6,7,0,1,2,29);
    SHA256_EXP(w, ref wv,2,3,4,5,6,7,0,1,30); SHA256_EXP(w, ref wv,1,2,3,4,5,6,7,0,31);
    SHA256_EXP(w, ref wv,0,1,2,3,4,5,6,7,32); SHA256_EXP(w, ref wv,7,0,1,2,3,4,5,6,33);
    SHA256_EXP(w, ref wv,6,7,0,1,2,3,4,5,34); SHA256_EXP(w, ref wv,5,6,7,0,1,2,3,4,35);
    SHA256_EXP(w, ref wv,4,5,6,7,0,1,2,3,36); SHA256_EXP(w, ref wv,3,4,5,6,7,0,1,2,37);
    SHA256_EXP(w, ref wv,2,3,4,5,6,7,0,1,38); SHA256_EXP(w, ref wv,1,2,3,4,5,6,7,0,39);
    SHA256_EXP(w, ref wv,0,1,2,3,4,5,6,7,40); SHA256_EXP(w, ref wv,7,0,1,2,3,4,5,6,41);
    SHA256_EXP(w, ref wv,6,7,0,1,2,3,4,5,42); SHA256_EXP(w, ref wv,5,6,7,0,1,2,3,4,43);
    SHA256_EXP(w, ref wv,4,5,6,7,0,1,2,3,44); SHA256_EXP(w, ref wv,3,4,5,6,7,0,1,2,45);
    SHA256_EXP(w, ref wv,2,3,4,5,6,7,0,1,46); SHA256_EXP(w, ref wv,1,2,3,4,5,6,7,0,47);
    SHA256_EXP(w, ref wv,0,1,2,3,4,5,6,7,48); SHA256_EXP(w, ref wv,7,0,1,2,3,4,5,6,49);
    SHA256_EXP(w, ref wv,6,7,0,1,2,3,4,5,50); SHA256_EXP(w, ref wv,5,6,7,0,1,2,3,4,51);
    SHA256_EXP(w, ref wv,4,5,6,7,0,1,2,3,52); SHA256_EXP(w, ref wv,3,4,5,6,7,0,1,2,53);
    SHA256_EXP(w, ref wv,2,3,4,5,6,7,0,1,54); SHA256_EXP(w, ref wv,1,2,3,4,5,6,7,0,55);
    SHA256_EXP(w, ref wv,0,1,2,3,4,5,6,7,56); SHA256_EXP(w, ref wv,7,0,1,2,3,4,5,6,57);
    SHA256_EXP(w, ref wv,6,7,0,1,2,3,4,5,58); SHA256_EXP(w, ref wv,5,6,7,0,1,2,3,4,59);
    SHA256_EXP(w, ref wv,4,5,6,7,0,1,2,3,60); SHA256_EXP(w, ref wv,3,4,5,6,7,0,1,2,61);
    SHA256_EXP(w, ref wv,2,3,4,5,6,7,0,1,62); SHA256_EXP(w, ref wv,1,2,3,4,5,6,7,0,63);

    ctx.h[0] += wv[0]; ctx.h[1] += wv[1];        ctx.h[2] += wv[2]; ctx.h[3] += wv[3];
    ctx.h[4] += wv[4]; ctx.h[5] += wv[5];        ctx.h[6] += wv[6]; ctx.h[7] += wv[7];
#endif
  }
}


public void sha256_init (out SHA256_INFO ctx)
{
  clear ctx;
  ctx.h = sha256_h0;
  ctx.len = 0;
  ctx.tot_len = 0;
}


public void sha256_update (ref SHA256_INFO ctx, byte[] message)
{
  uint len = message'size;
  uint block_nb;
  uint new_len, rem_len, tmp_len;
  uint shifted_message;

  tmp_len = SHA256_BLOCK_SIZE - ctx.len;
  rem_len = len < tmp_len ? len : tmp_len;

  ctx.block[ctx.len : rem_len] = message[0 : rem_len];

  if (ctx.len + len < SHA256_BLOCK_SIZE)
  {
    ctx.len += len;
    return;
  }

  new_len = len - rem_len;
  block_nb = new_len >> SHA256_BLOCK_SIZE_POWER;

  shifted_message = rem_len;

  sha256_transf (ref ctx, ctx.block, 1);
  sha256_transf (ref ctx, message[shifted_message:message'size-shifted_message], block_nb);

  rem_len = new_len & (SHA256_BLOCK_SIZE - 1);

  ctx.block[0:rem_len] = message[shifted_message + (block_nb << 6) : rem_len];

  ctx.len = rem_len;
  ctx.tot_len += (block_nb + 1) << 6;
}


public void sha256_final (ref SHA256_INFO ctx, out byte[32] digest)
{
  uint block_nb;
  uint pm_len;
  long len_b;

#if !unroll_loops
  int i;
#endif

  block_nb = (1 + (uint)((SHA256_BLOCK_SIZE - 9) < (ctx.len & (SHA256_BLOCK_SIZE-1))));

  len_b = (ctx.tot_len + ctx.len) << 3;
  pm_len = block_nb << 6;

  ctx.block[ctx.len :  pm_len - ctx.len] = {all => 0};
  ctx.block[ctx.len] = 0x80;
  UNPACK64 (len_b, out ctx.block[pm_len - 8 : 8]);

  sha256_transf (ref ctx, ctx.block, block_nb);

  clear digest;

#if !unroll_loops
  for (i = 0 ; i < 8; i++)
    UNPACK32 (ctx.h[i], out digest[i << 2 : 4]);
#else
   UNPACK32 (ctx.h[0], out digest[ 0:4]);
   UNPACK32 (ctx.h[1], out digest[ 4:4]);
   UNPACK32 (ctx.h[2], out digest[ 8:4]);
   UNPACK32 (ctx.h[3], out digest[12:4]);
   UNPACK32 (ctx.h[4], out digest[16:4]);
   UNPACK32 (ctx.h[5], out digest[20:4]);
   UNPACK32 (ctx.h[6], out digest[24:4]);
   UNPACK32 (ctx.h[7], out digest[28:4]);
#endif
}


void sha512_transf (ref SHA512_INFO ctx, byte[] message, uint block_nb)
{
  int8 w[80];
  int8 wv[8];
  uint sub_block;
  uint i, j;

#if !unroll_loops
  int8 t1, t2;
#endif

#begin unsafe      // avoids a clear
  int8* p = &w;
  int8* q = &wv;
  _unused p;
  _unused q;
#end unsafe

  for (i=0; i<block_nb; i++)
  {
    sub_block = (i << 7);

#if !unroll_loops
    for (j = 0; j < 16; j++)
      PACK64 (message[sub_block+(j << 3):8], out w[j]);

    for (j = 16; j < 80; j++)
      SHA512_SCR (ref w, j);

    for (j = 0; j < 8; j++)
      wv[j] = ctx.h[j];

    for (j = 0; j < 80; j++)
    {
      t1 = wv[7] + SHA512_F2(wv[4]) + CH64(wv[4], wv[5], wv[6]) + sha512_k[j] + w[j];
      t2 = SHA512_F1(wv[0]) + MAJ64(wv[0], wv[1], wv[2]);
      wv[7] = wv[6];
      wv[6] = wv[5];
      wv[5] = wv[4];
      wv[4] = wv[3] + t1;
      wv[3] = wv[2];
      wv[2] = wv[1];
      wv[1] = wv[0];
      wv[0] = t1 + t2;
    }

    for (j = 0; j < 8; j++)
      ctx.h[j] += wv[j];
#else
    PACK64 (message[sub_block+  0:8], out w[ 0]); PACK64 (message[sub_block+  8:8], out w[ 1]);
    PACK64 (message[sub_block+ 16:8], out w[ 2]); PACK64 (message[sub_block+ 24:8], out w[ 3]);
    PACK64 (message[sub_block+ 32:8], out w[ 4]); PACK64 (message[sub_block+ 40:8], out w[ 5]);
    PACK64 (message[sub_block+ 48:8], out w[ 6]); PACK64 (message[sub_block+ 56:8], out w[ 7]);
    PACK64 (message[sub_block+ 64:8], out w[ 8]); PACK64 (message[sub_block+ 72:8], out w[ 9]);
    PACK64 (message[sub_block+ 80:8], out w[10]); PACK64 (message[sub_block+ 88:8], out w[11]);
    PACK64 (message[sub_block+ 96:8], out w[12]); PACK64 (message[sub_block+104:8], out w[13]);
    PACK64 (message[sub_block+112:8], out w[14]); PACK64 (message[sub_block+120:8], out w[15]);

    SHA512_SCR(ref w,16); SHA512_SCR(ref w,17); SHA512_SCR(ref w,18); SHA512_SCR(ref w,19);
    SHA512_SCR(ref w,20); SHA512_SCR(ref w,21); SHA512_SCR(ref w,22); SHA512_SCR(ref w,23);
    SHA512_SCR(ref w,24); SHA512_SCR(ref w,25); SHA512_SCR(ref w,26); SHA512_SCR(ref w,27);
    SHA512_SCR(ref w,28); SHA512_SCR(ref w,29); SHA512_SCR(ref w,30); SHA512_SCR(ref w,31);
    SHA512_SCR(ref w,32); SHA512_SCR(ref w,33); SHA512_SCR(ref w,34); SHA512_SCR(ref w,35);
    SHA512_SCR(ref w,36); SHA512_SCR(ref w,37); SHA512_SCR(ref w,38); SHA512_SCR(ref w,39);
    SHA512_SCR(ref w,40); SHA512_SCR(ref w,41); SHA512_SCR(ref w,42); SHA512_SCR(ref w,43);
    SHA512_SCR(ref w,44); SHA512_SCR(ref w,45); SHA512_SCR(ref w,46); SHA512_SCR(ref w,47);
    SHA512_SCR(ref w,48); SHA512_SCR(ref w,49); SHA512_SCR(ref w,50); SHA512_SCR(ref w,51);
    SHA512_SCR(ref w,52); SHA512_SCR(ref w,53); SHA512_SCR(ref w,54); SHA512_SCR(ref w,55);
    SHA512_SCR(ref w,56); SHA512_SCR(ref w,57); SHA512_SCR(ref w,58); SHA512_SCR(ref w,59);
    SHA512_SCR(ref w,60); SHA512_SCR(ref w,61); SHA512_SCR(ref w,62); SHA512_SCR(ref w,63);
    SHA512_SCR(ref w,64); SHA512_SCR(ref w,65); SHA512_SCR(ref w,66); SHA512_SCR(ref w,67);
    SHA512_SCR(ref w,68); SHA512_SCR(ref w,69); SHA512_SCR(ref w,70); SHA512_SCR(ref w,71);
    SHA512_SCR(ref w,72); SHA512_SCR(ref w,73); SHA512_SCR(ref w,74); SHA512_SCR(ref w,75);
    SHA512_SCR(ref w,76); SHA512_SCR(ref w,77); SHA512_SCR(ref w,78); SHA512_SCR(ref w,79);

    wv[0] = ctx.h[0]; wv[1] = ctx.h[1];    wv[2] = ctx.h[2]; wv[3] = ctx.h[3];
    wv[4] = ctx.h[4]; wv[5] = ctx.h[5];    wv[6] = ctx.h[6]; wv[7] = ctx.h[7];

    j = 0;

    for (;;)
    {
      SHA512_EXP(w, ref wv,0,1,2,3,4,5,6,7,j); j++;
      SHA512_EXP(w, ref wv,7,0,1,2,3,4,5,6,j); j++;
      SHA512_EXP(w, ref wv,6,7,0,1,2,3,4,5,j); j++;
      SHA512_EXP(w, ref wv,5,6,7,0,1,2,3,4,j); j++;
      SHA512_EXP(w, ref wv,4,5,6,7,0,1,2,3,j); j++;
      SHA512_EXP(w, ref wv,3,4,5,6,7,0,1,2,j); j++;
      SHA512_EXP(w, ref wv,2,3,4,5,6,7,0,1,j); j++;
      SHA512_EXP(w, ref wv,1,2,3,4,5,6,7,0,j); j++;
      if (j >= 80)
        break;
    }

    ctx.h[0] += wv[0]; ctx.h[1] += wv[1];
    ctx.h[2] += wv[2]; ctx.h[3] += wv[3];
    ctx.h[4] += wv[4]; ctx.h[5] += wv[5];
    ctx.h[6] += wv[6]; ctx.h[7] += wv[7];
#endif
  }
}


public void sha512_init (out SHA512_INFO ctx)
{
  clear ctx;
  ctx.h = sha512_h0;
  ctx.len = 0;
  ctx.tot_len_low = 0;
  ctx.tot_len_high = 0;
}


public void sha512_update (ref SHA512_INFO ctx, byte[] message)
{
  uint len = message'size;
  uint block_nb;
  uint new_len, rem_len, tmp_len;
  uint shifted_message;
  long low;

  tmp_len = SHA512_BLOCK_SIZE - ctx.len;
  rem_len = len < tmp_len ? len : tmp_len;

  ctx.block[ctx.len : rem_len] = message [0 : rem_len];

  if (ctx.len + len < SHA512_BLOCK_SIZE)
  {
    ctx.len += len;
    return;
  }

  new_len = len - rem_len;
  block_nb = new_len >> SHA512_BLOCK_SIZE_POWER;

  shifted_message = rem_len;

  sha512_transf (ref ctx, ctx.block, 1);
  sha512_transf (ref ctx, message[shifted_message : message'size - shifted_message], block_nb);

  rem_len = new_len & (SHA512_BLOCK_SIZE-1);

  ctx.block[0 : rem_len] =  message[shifted_message + (block_nb << 7) : rem_len];

  ctx.len = rem_len;
  low = ctx.tot_len_low + ((block_nb + 1) << 7);
  if (ctx.tot_len_low < 0 && low >= 0)
    ctx.tot_len_high++;
  ctx.tot_len_low = low;
}


public void sha512_final (ref SHA512_INFO ctx, out byte[64] digest)
{
  uint block_nb;
  uint pm_len;
  long low;
  long len_b_low, len_b_high;
  
#if !unroll_loops
  int i;
#endif

  block_nb = 1 + (uint)((SHA512_BLOCK_SIZE - 17) < (ctx.len & (SHA512_BLOCK_SIZE-1)));

  low = ctx.tot_len_low + ctx.len;
  if (ctx.tot_len_low < 0 && low >= 0)
    ctx.tot_len_high++;
  ctx.tot_len_low = low;

  len_b_low  = ctx.tot_len_low << 3;
  len_b_high = (ctx.tot_len_high << 3) | ((ctx.tot_len_low >> 61) & 7);
  
  pm_len = block_nb << 7;

  ctx.block[ctx.len : pm_len - ctx.len] = {all => 0};
  ctx.block[ctx.len] = 0x80;

  UNPACK64 (len_b_high, out ctx.block[pm_len - 16 : 8]);
  UNPACK64 (len_b_low, out ctx.block[pm_len - 8 : 8]);

  sha512_transf (ref ctx, ctx.block, block_nb);

  clear digest;

#if !unroll_loops
  for (i = 0 ; i < 8; i++)
    UNPACK64 (ctx.h[i], out digest[i << 3:8]);
#else
  UNPACK64 (ctx.h[0], out digest[ 0:8]);
  UNPACK64 (ctx.h[1], out digest[ 8:8]);
  UNPACK64 (ctx.h[2], out digest[16:8]);
  UNPACK64 (ctx.h[3], out digest[24:8]);
  UNPACK64 (ctx.h[4], out digest[32:8]);
  UNPACK64 (ctx.h[5], out digest[40:8]);
  UNPACK64 (ctx.h[6], out digest[48:8]);
  UNPACK64 (ctx.h[7], out digest[56:8]);
#endif
}

//---------------------------------------------------------------------------

public void update_adler (ref uint adler, byte[] buf)
{
  uint len = buf'size;
  uint s1 = adler & 0xffff;
  uint s2 = (adler >> 16) & 0xffff;
  int  i = 0;

  while (len-- > 0)
  {
                           /* s1 in range 0 .. 65520 */
    s1 += buf[i];
                           /* s1 in range 0 .. 65520 + 255 */
    if (s1 >= 65521)
      s1 -= 65521;
                           /* s1, s2 in range 0 .. 65520 */
    s2 += s1;
                           /* s2 in range 0 .. 65520 + 65520 */
    if (s2 >= 65521)
    {
      s2 -= 65521;
      if (s2 >= 65521)
        s2 -= 65521;
    }
                           /* s2 in range 0 .. 65520 */

    i++;
  }

  adler = (s2 << 16) + s1;
}

//---------------------------------------------------------------------------

package TABLE
  uint[256]^ crc_table;    /* ptr to 256 uint values */
  uint       init;
end TABLE;

//---------------------------------------------------------------------------

public void update_crc (ref uint crc, byte[] buf)
{
  uint len = buf'size;
  uint c, n;
  int  k;

  if (crc_table == null)
  {
    if (InterlockedExchange (ref init, 1) == 0)  // first call
    {
      uint[256]^ p;

      p = new uint[256];

      for (n=0; n<256; n++)
      {
        c = n;

        for (k=0; k<8; k++)
        {
          if ((c & 1) != 0)
            c = 0xedb88320 ^ (c >> 1);
          else
            c = c >> 1;
        }

        p^[n] = c;
      }

      crc_table = p;
    }
    else
    {
      while (crc_table == null)
        sleep 0.01;
    }
  }

  c = crc ^ 0xffffffff;

  for (n=0; n<len; n++)
    c = crc_table^[(c ^ buf[n]) & 0xff] ^ (c >> 8);

  crc = c ^ 0xffffffff;
}

//---------------------------------------------------------------------------
