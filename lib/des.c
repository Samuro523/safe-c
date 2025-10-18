
// des.c : data encryption standard routines

const byte BYTE[] = {1,2,4,8,16,32,64,128};

/**************************************************************************/
#begin unsafe
/**************************************************************************/

/* EXPAND
   convert 8 bytes to 32 or 64 bytes with values 0 or 1.
   if nbit = 8, generate 64 bytes by taking the bits 7 to 0 of each byte,
   if nbit = 4, generate 32 bytes by taking the bits 3 to 0 of each byte.
   pout = output array
   pin = input array
*/

void expand (byte* pout0, byte* pin0, int nbit)
{
  int   by, bi;
  byte  value;
  byte* pin  = pin0;
  byte* pout = pout0;

  by = 8;
  while (by-- > 0)
  {
    value = *pin++;
    bi = nbit;
    while (bi-- > 0)
      *pout++ = (byte)((value & BYTE[bi]) != 0);
  }
}

/**************************************************************************/

/* EXTRACT
   extract 'len' values from the input to the output array,
   according to a permutation table */

void extract (byte* pout0, byte* pin, byte* ptable0, int len0)
{
  byte* pout   = pout0;
  byte* ptable = ptable0;
  int len = len0;

  while (len-- > 0)
    *pout++ = *(pin + (*ptable++) - 1);
}


/**************************************************************************/

/* ROTATE_LEFT_28_BYTES : rotate 28 bytes left by 1 or 2 positions */

void rotate_left_28_bytes (byte* p0, int delta)
{
  int  i;
  byte save1, save2;
  byte* p = p0;

  if (delta == 1)
  {
    save1 = p[0];
    i = 27;
    while (i-- > 0)
    {
      *p = *(p+1);
      p++;
    }
    *p = save1;
  }
  else
  {
    save1 = p[0];
    save2 = p[1];
    i = 26;
    while (i-- > 0)
    {
      *p = *(p+2);
      p++;
    }
    *p++ = save1;
    *p   = save2;
  }
}

/**************************************************************************/

/* INIT_SUBKEY - initializes a subkey table from a key */

void init_subkey (    byte[8]      key,
                  out byte[48][16] SK)   // subkey table
{
  const byte TAB1 [] =    /* extraction table 1 */
    {57,49,41,33,25,17, 9, 1,58,50,42,34,26,18,
     10, 2,59,51,43,35,27,19,11, 3,60,52,44,36,
     63,55,47,39,31,23,15, 7,62,54,46,38,30,22,
     14, 6,61,53,45,37,29,21,13, 5,28,20,12, 4};

  const byte TAB2 [] =    /* extraction table 2 */
    {14,17,11,24, 1, 5, 3,28,15, 6,21,10,23,19,12, 4,26, 8,16, 7,27,20,13, 2,
     41,52,31,37,47,55,30,40,51,45,33,48,44,49,39,56,34,53,46,42,50,36,29,32};

  const byte SHIFTS [] =  /* table of shifts */
    {1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1};

  byte buf64[64];
  byte buf56[56];
  int i;

  /* fill buf64 with 64 bytes representing the bits of key */
  expand (&buf64, &key, 8);

  /* extract 56 bytes from these 64 bit (stored in bytes) using TAB */
  extract (&buf56, &buf64, &TAB1, 56);

  clear SK;
  for (i=0; i<16; i++)
  {
    rotate_left_28_bytes (&buf56[0],  SHIFTS[i]);
    rotate_left_28_bytes (&buf56[28], SHIFTS[i]);
    extract (&SK[i], &buf56[0], &TAB2, 48);
  }
}

/**************************************************************************/

/* XOR - xor two arrays p1 and p2 of length len */

void xor (byte* pout0, byte* p10, byte* p20, int len0)
{
  byte* pout = pout0;
  byte* p1  = p10;
  byte* p2  = p20;
  int   len = len0;

  while (len-- > 0)
    *pout++ = (byte)((*p1++) ^ (*p2++));
}

/**************************************************************************/

/* CONTRACT - contract an input array of length 64 to an array of
              8 characters of 8 bits */

void contract (byte* pout0, byte* pin0)
{
  int n, i;
  byte* pout = pout0;
  byte* pin  = pin0;

  n = 8;
  while (n-- > 0)
  {
    *pout = 0;
    for (i=8; i-- > 0; )
    {
      if (*pin++ > 0)
        *pout += BYTE[i];
    }
    pout++;
  }
}

/**************************************************************************/

/* CRYPT - main cryption routine */

void crypt (     byte[48][16] SK,        /* subkey table */
                 byte[8]      data_in,   /* incoming 8 bytes */
            out  byte[8]      data_out,  /* outgoing 8 bytes */
                 bool         encrypt)   /* true=encrypt, false=decrypt */
{
  const byte IP[] =          /* initial permutation */
    {57,49,41,33,25,17, 9,1,59,51,43,35,27,19,11,3,
     61,53,45,37,29,21,13,5,63,55,47,39,31,23,15,7,
     58,50,42,34,26,18,10,2,60,52,44,36,28,20,12,4,
     62,54,46,38,30,22,14,6,64,56,48,40,32,24,16,8};

  const byte PE[] =          /* permutation E */
    {32, 1, 2, 3, 4, 5, 4, 5, 6, 7, 8, 9,
      8, 9,10,11,12,13,12,13,14,15,16,17,
     16,17,18,19,20,21,20,21,22,23,24,25,
     24,25,26,27,28,29,28,29,30,31,32, 1};

  const byte SE[8][64] =     /* selections functions S1-8 */
    {{14, 0, 4,15,13, 7, 1, 4, 2,14,15, 2,11,13, 8, 1,
       3,10,10, 6 ,6,12,12,11, 5, 9, 9, 5, 0, 3, 7, 8,
       4,15, 1,12,14, 8, 8, 2,13, 4, 6, 9, 2, 1,11, 7,
      15, 5,12,11, 9, 3, 7,14, 3,10,10, 0, 5, 6, 0,13},  /* S1 */
     {15, 3, 1,13, 8, 4,14, 7, 6,15,11, 2, 3, 8, 4,14,
       9,12, 7, 0, 2, 1,13,10,12, 6, 0, 9, 5,11,10, 5,
       0,13,14, 8, 7,10,11, 1,10, 3, 4,15,13, 4, 1, 2,
       5,11, 8, 6,12, 7, 6,12, 9, 0, 3, 5, 2,14,15, 9},  /* S2 */
     {10,13, 0, 7, 9, 0,14, 9, 6, 3, 3, 4,15, 6, 5,10,
       1, 2,13, 8,12, 5, 7,14,11,12, 4,11, 2,15, 8, 1,
      13, 1, 6,10, 4,13, 9, 0, 8, 6,15, 9, 3, 8, 0, 7,
      11, 4, 1,15, 2,14,12, 3, 5,11,10, 5,14, 2, 7,12},  /* S3 */
     { 7,13,13, 8,14,11, 3, 5, 0, 6, 6,15, 9, 0,10, 3,
       1, 4, 2, 7, 8, 2, 5,12,11, 1,12,10, 4,14,15, 9,
      10, 3, 6,15, 9, 0, 0, 6,12,10,11, 1, 7,13,13, 8,
      15, 9, 1, 4, 3, 5,14,11, 5,12, 2, 7, 8, 2, 4,14},  /* S4 */
     { 2,14,12,11, 4, 2, 1,12, 7, 4,10, 7,11,13, 6, 1,
       8, 5, 5, 0, 3,15,15,10,13, 3, 0, 9,14, 8, 9, 6,
       4,11, 2, 8, 1,12,11, 7,10, 1,13,14, 7, 2, 8,13,
      15, 6, 9,15,12, 0, 5, 9, 6,10, 3, 4, 0, 5,14, 3},  /* S5 */
     {12,10, 1,15,10, 4,15, 2, 9, 7, 2,12, 6, 9, 8, 5,
       0, 6,13, 1, 3,13, 4,14,14, 0, 7,11, 5, 3,11, 8,
       9, 4,14, 3,15, 2, 5,12, 2, 9, 8, 5,12,15, 3,10,
       7,11, 0,14, 4, 1,10, 7, 1, 6,13, 0,11, 8, 6,13},  /* S6 */
     { 4,13,11, 0, 2,11,14, 7,15, 4, 0, 9, 8, 1,13,10,
       3,14,12, 3, 9, 5, 7,12, 5, 2,10,15, 6, 8, 1, 6,
       1, 6, 4,11,11,13,13, 8,12, 1, 3, 4, 7,10,14, 7,
      10, 9,15, 5, 6, 0, 8,15, 0,14, 5, 2, 9, 3, 2,12},  /* S7 */
     {13, 1, 2,15, 8,13, 4, 8, 6,10,15, 3,11, 7, 1, 4,
      10,12, 9, 5, 3, 6,14,11, 5, 0, 0,14,12, 9, 7, 2,
       7, 2,11, 1, 4,14, 1, 7, 9, 4,12,10,14, 8, 2,13,
       0,15, 6,12,10, 9,13, 0,15, 3, 3, 5, 5, 6, 8,11}}; /* S8 */

  const byte PP[] =          /* permutation P */
    {16, 7,20,21,29,12,28,17, 1,15,23,26, 5,18,31,10,
      2, 8,24,14,32,27, 3, 9,19,13,30, 6,22,11, 4,25};

  const byte IIP[] =         /* inverse initial permutation */
    {40,8,48,16,56,24,64,32,39,7,47,15,55,23,63,31,
     38,6,46,14,54,22,62,30,37,5,45,13,53,21,61,29,
     36,4,44,12,52,20,60,28,35,3,43,11,51,19,59,27,
     34,2,42,10,50,18,58,26,33,1,41, 9,49,17,57,25};

  byte[64] BUF;
  byte[64] T1;
  byte[8]  T2;
  byte*    pl, pr, p;
  int      loop, i, j, k;

  /* fill BUF with 64 bytes representing the bits of data_in */
  expand (&BUF, &data_in, 8);

  /* extract 64 bytes using IP */
  extract (&T1, &BUF, &IP, 64);

  pl = &T1[32];
  pr = &T1[0];

  for (loop=0; loop<16; loop++)
  {
    /* extract 48 bytes from pr into BUF, using PE */
    extract (&BUF, pr, &PE, 48);

    if (encrypt)
      xor (&BUF, &SK[loop][0], &BUF, 48);
    else
      xor (&BUF, &SK[15-loop][0], &BUF, 48);

    p = &BUF;
    for (i=0; i<8; i++)
    {
      j = 0;
      for (k=0; k<6; k++)
      {
        j <<= 1;
        j += *p++;
      }
      T2[i] = SE[i][j];
    }

    expand (&BUF, &T2, 4);    /* build 32 bytes in BUF */
    extract (&BUF[32], &BUF, &PP, 32);
    xor (pl, pl, &BUF[32], 32);

    /* swap pl & pr */
    p  = pl;
    pl = pr;
    pr = p;
  }

  extract (&BUF, &T1, &IIP, 64);
  contract (&data_out, &BUF);
}

/**************************************************************************/

public void des_encrypt (out byte[8] data_out,
                             byte[8] data_in,
                             byte[8] key)    // the lowest bit of each byte is not used
{
  byte[48][16] SK;                    /* subkey table */
  init_subkey (key, out SK);
  crypt (SK, data_in, out data_out, true);
}

/**************************************************************************/

public void des_decrypt (out byte[8] data_out,
                             byte[8] data_in,
                             byte[8] key)     // the lowest bit of each byte is not used
{
  byte[48][16] SK;                    /* subkey table */
  init_subkey (key, out SK);
  crypt (SK, data_in, out data_out, false);
}

/**************************************************************************/

public void des_mac (out byte[]  mac,   // must be between 1 and 8 bytes large.
                         byte[]  data,
                         byte[8] key)
{
  byte[48][16] SK;                    /* subkey table */
  byte         cipher[8];
  byte         buffer[8];
  int          ofs, i, length;

  init_subkey (key, out SK);

  clear cipher;          /* clear cipher */

  ofs = 0;
  length = data'length;
  while (length >= 8)
  {
    for (i=0; i<8; i++)
      cipher[i] ^= data[ofs++];

    crypt (SK, cipher, out cipher, true);   /* encrypt */

    length -= 8;
  }

  if (length > 0)      /* rest */
  {
    clear buffer;
    buffer[0:length] = data[0:length];   /* copy last data bytes */

    for (i=0; i<8; i++)              /* XOR */
      cipher[i] ^= buffer[i];

    crypt (SK, cipher, out cipher, true);   /* encrypt */
  }

  mac = cipher[0:mac'length];
}

/**************************************************************************/
#end unsafe
/**************************************************************************/
