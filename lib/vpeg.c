
// vpeg.c

use tracing, image;
use vpeghuff;

#define debug 0

//-------------------------------------------------------------------------------------
#begin unsafe
//-------------------------------------------------------------------------------------

const int DCTSIZE = 8;

const int CONST_BITS = 13;
const int PASS1_BITS =  2;

const int FIX_0_298631336  =  2446;
const int FIX_0_390180644  =  3196;
const int FIX_0_541196100  =  4433;
const int FIX_0_765366865  =  6270;
const int FIX_0_899976223  =  7373;
const int FIX_1_175875602  =  9633;
const int FIX_1_501321110  =  12299;
const int FIX_1_847759065  =  15137;
const int FIX_1_961570560  =  16069;
const int FIX_2_053119869  =  16819;
const int FIX_2_562915447  =  20995;
const int FIX_3_072711026  =  25172;

//-------------------------------------------------------------------------------------

inline int DESCALE (int x, int n)
{
  return ((x) + (1 << ((n)-1))) >> n;
}

//-------------------------------------------------------------------------------------

inline int FIX (double x)
{
  return (int) (x * 65536.0 + 0.5);
}

//-------------------------------------------------------------------------------------

/* transformation DCT : transforme un block de 8x8 valeurs en 8x8 fréquences */

/* IN  : valeurs RGB  -128 ..  +127 */
/* OUT : valeurs DCT -8192 .. +8191 */

/* les coefficients de la matrice résultante sont multipliés par 8 */
/* afin d'avoir une bonne précision pour la quantization.          */
/* les coefficients doivent être divisés par 8 avant de            */
/* faire la transformation DCT inverse.                            */

/* les coefficients (multipliés par 8) varient entre -8192 et +8191, */
/* donc doivent être stockés dans 13 bits + le bit de signe.         */

void perform_dct (ref DCT_MATRIX coef)
{
  int    tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  int    tmp10, tmp11, tmp12, tmp13;
  int    z1, z2, z3, z4, z5;
  short* dataptr;
  int    ctr;


  /* pass 1 : traitement ligne par ligne */

  dataptr = &coef;
  for (ctr = DCTSIZE-1; ctr >= 0; ctr--)
  {
    tmp0 = dataptr[0] + dataptr[7];   // -256 .. 254
    tmp7 = dataptr[0] - dataptr[7];
    tmp1 = dataptr[1] + dataptr[6];
    tmp6 = dataptr[1] - dataptr[6];
    tmp2 = dataptr[2] + dataptr[5];
    tmp5 = dataptr[2] - dataptr[5];
    tmp3 = dataptr[3] + dataptr[4];
    tmp4 = dataptr[3] - dataptr[4];

    tmp10 = tmp0 + tmp3;   // -512 .. +508
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    dataptr[0] = (short)((tmp10 + tmp11) << PASS1_BITS);    // -4096 .. +4064
    dataptr[4] = (short)((tmp10 - tmp11) << PASS1_BITS);

    z1 = (tmp12 + tmp13) * FIX_0_541196100;    // (-1024 .. 1016) x 4433 = -4 539 392 .. +4 503 928

    dataptr[2] = (short)DESCALE(z1 + tmp13 * (  FIX_0_765366865), CONST_BITS-PASS1_BITS);
    dataptr[6] = (short)DESCALE(z1 + tmp12 * (- FIX_1_847759065), CONST_BITS-PASS1_BITS);

                     // (( -4 539 392 .. +4 503 928 ) + (-7 750 144 .. +7 689 596) )>> 11
                     //  = (-12 289 536 .. +12 193 524)   >> 11
                     // -6000 .. 5953

    z1 = tmp4 + tmp7;   // -512 .. +508
    z2 = tmp5 + tmp6;
    z3 = tmp4 + tmp6;
    z4 = tmp5 + tmp7;

    z5 = (z3 + z4) * FIX_1_175875602;  /* sqrt(2) * c3 */    // (-1024 .. +1016) x 9633 = (-9 864 192 .. +9 787 128)

    tmp4 *= FIX_0_298631336;   /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp5 *= FIX_2_053119869;   /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp6 *= FIX_3_072711026;   /* sqrt(2) * ( c1+c3+c5-c7) */   // ( -256 .. 254 ) x 25172 = -6 444 032 .. +6 530 848
    tmp7 *= FIX_1_501321110;   /* sqrt(2) * ( c1+c3-c5-c7) */

    z1 *= (- FIX_0_899976223);   /* sqrt(2) * (c7-c3)  */
    z2 *= (- FIX_2_562915447);   /* sqrt(2) * (-c1-c3) */   //  (-512 .. +508) x 20995 = (-10 749 440 .. +10 665 460)
    z3 *= (- FIX_1_961570560);   /* sqrt(2) * (-c3-c5) */
    z4 *= (- FIX_0_390180644);   /* sqrt(2) * (c5-c3)  */

    z3 += z5;   // (-10 749 440 .. +10 665 460) + (-9 864 192 .. +9 787 128) = (-21 414 900 .. +20 452 588)
    z4 += z5;

    dataptr[7] = (short)DESCALE(tmp4 + z1 + z3, CONST_BITS-PASS1_BITS);
    dataptr[5] = (short)DESCALE(tmp5 + z2 + z4, CONST_BITS-PASS1_BITS);
    dataptr[3] = (short)DESCALE(tmp6 + z2 + z3, CONST_BITS-PASS1_BITS);
    dataptr[1] = (short)DESCALE(tmp7 + z1 + z4, CONST_BITS-PASS1_BITS);

            // ((-6 444 032 .. +6 530 848) + (-10 749 440 .. +10 665 460) + (-21 414 900 .. +20 452 588)) / 2048
            // -18 851 .. +18 383

    dataptr += DCTSIZE;
  }

  /* pass 2: traitement colonne par colonne */

  dataptr = &coef;
  for (ctr = DCTSIZE-1; ctr >= 0; ctr--)
  {
    tmp0 = dataptr[DCTSIZE*0] + dataptr[DCTSIZE*7];
    tmp7 = dataptr[DCTSIZE*0] - dataptr[DCTSIZE*7];
    tmp1 = dataptr[DCTSIZE*1] + dataptr[DCTSIZE*6];
    tmp6 = dataptr[DCTSIZE*1] - dataptr[DCTSIZE*6];
    tmp2 = dataptr[DCTSIZE*2] + dataptr[DCTSIZE*5];
    tmp5 = dataptr[DCTSIZE*2] - dataptr[DCTSIZE*5];
    tmp3 = dataptr[DCTSIZE*3] + dataptr[DCTSIZE*4];
    tmp4 = dataptr[DCTSIZE*3] - dataptr[DCTSIZE*4];

    tmp10 = tmp0 + tmp3;
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    dataptr[DCTSIZE*0] = (short)DESCALE(tmp10 + tmp11, PASS1_BITS);
    dataptr[DCTSIZE*4] = (short)DESCALE(tmp10 - tmp11, PASS1_BITS);

    z1 = (tmp12 + tmp13) * FIX_0_541196100;

    dataptr[DCTSIZE*2] = (short)DESCALE(z1 + tmp13 * ( FIX_0_765366865), CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*6] = (short)DESCALE(z1 + tmp12 * (-FIX_1_847759065), CONST_BITS+PASS1_BITS);

    z1 = tmp4 + tmp7;
    z2 = tmp5 + tmp6;
    z3 = tmp4 + tmp6;
    z4 = tmp5 + tmp7;

    z5 = (z3 + z4) * FIX_1_175875602;   /* sqrt(2) * c3 */
    
    tmp4 = tmp4 * FIX_0_298631336;  /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp5 = tmp5 * FIX_2_053119869;  /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp6 = tmp6 * FIX_3_072711026;  /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp7 = tmp7 * FIX_1_501321110;  /* sqrt(2) * ( c1+c3-c5-c7) */

    z1 = z1 * (- FIX_0_899976223);  /* sqrt(2) * (c7-c3) */
    z2 = z2 * (- FIX_2_562915447);  /* sqrt(2) * (-c1-c3) */
    z3 = z3 * (- FIX_1_961570560);  /* sqrt(2) * (-c3-c5) */
    z4 = z4 * (- FIX_0_390180644);  /* sqrt(2) * (c5-c3) */

    z3 += z5;
    z4 += z5;
    
    dataptr[DCTSIZE*7] = (short)DESCALE(tmp4 + z1 + z3, CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*5] = (short)DESCALE(tmp5 + z2 + z4, CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*3] = (short)DESCALE(tmp6 + z2 + z3, CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*1] = (short)DESCALE(tmp7 + z1 + z4, CONST_BITS+PASS1_BITS);

    dataptr++;
  }
}

//-------------------------------------------------------------------------------------

// conversion (-128 .. +128) vers (0 .. 255) avec vérification de limite

short range_limit (int x)
{
  int inner_x = x + 128;

  if (((uint)inner_x) > 255)   // on utilise + de 8 bits
  {
    if (inner_x < 0)
      inner_x = 0;
    if (inner_x > 255)
      inner_x = 255;
  }

  return (short)inner_x;
}

//-------------------------------------------------------------------------------------

/* transformation DCT INVERSE : transforme un block de 8x8 fréquences en 8x8 valeurs */

/* IN  : valeurs DCT -1024 .. +1023 */
/* OUT : valeurs RGB     0 ..   255 */

void inverse_dct (ref DCT_MATRIX coef)
{
  int    tmp0, tmp1, tmp2, tmp3;
  int    tmp10, tmp11, tmp12, tmp13;
  int    z1, z2, z3, z4, z5;
  short* inptr;
  int*   wsptr;
  int    workspace[64];
  short* outptr;
  int    ctr;

  /* pass 1 : traitement colonne par colonne */

  inptr = &coef;
  wsptr = &workspace;
  for (ctr = DCTSIZE; ctr > 0; ctr--)
  {
    /* optimisation : dans environ 50% des cas, les coefficients de toute la colonne */
    /*                (excepté le 1er coef) sont à zéro.                             */

    if ((inptr[DCTSIZE*1] | inptr[DCTSIZE*2] | inptr[DCTSIZE*3] | inptr[DCTSIZE*4] |
         inptr[DCTSIZE*5] | inptr[DCTSIZE*6] | inptr[DCTSIZE*7]) == 0)
    {
      int dcval = inptr[DCTSIZE*0] << PASS1_BITS;
      
      wsptr[DCTSIZE*0] = dcval;
      wsptr[DCTSIZE*1] = dcval;
      wsptr[DCTSIZE*2] = dcval;
      wsptr[DCTSIZE*3] = dcval;
      wsptr[DCTSIZE*4] = dcval;
      wsptr[DCTSIZE*5] = dcval;
      wsptr[DCTSIZE*6] = dcval;
      wsptr[DCTSIZE*7] = dcval;

      inptr++;
      wsptr++;
      continue;
    }

    z2 = inptr[DCTSIZE*2];
    z3 = inptr[DCTSIZE*6];

    z1 = (z2 + z3) * FIX_0_541196100;

    tmp2 = z1 + (z3 * (- FIX_1_847759065));
    tmp3 = z1 + (z2 * (  FIX_0_765366865));

    z2 = inptr[DCTSIZE*0];
    z3 = inptr[DCTSIZE*4];

    tmp0 = (z2 + z3) << CONST_BITS;
    tmp1 = (z2 - z3) << CONST_BITS;
    
    tmp10 = tmp0 + tmp3;
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    tmp0 = inptr[DCTSIZE*7];
    tmp1 = inptr[DCTSIZE*5];
    tmp2 = inptr[DCTSIZE*3];
    tmp3 = inptr[DCTSIZE*1];

    z1 = tmp0 + tmp3;
    z2 = tmp1 + tmp2;
    z3 = tmp0 + tmp2;
    z4 = tmp1 + tmp3;

    z5 = (z3 + z4) * FIX_1_175875602;   /* sqrt(2) * c3 */
    
    tmp0 *= FIX_0_298631336;  /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp1 *= FIX_2_053119869;  /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp2 *= FIX_3_072711026;  /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp3 *= FIX_1_501321110;  /* sqrt(2) * ( c1+c3-c5-c7) */

    z1 *= (- FIX_0_899976223);  /* sqrt(2) * (c7-c3) */
    z2 *= (- FIX_2_562915447);  /* sqrt(2) * (-c1-c3) */
    z3 *= (- FIX_1_961570560);  /* sqrt(2) * (-c3-c5) */
    z4 *= (- FIX_0_390180644);  /* sqrt(2) * (c5-c3) */

    z3 += z5;
    z4 += z5;
    
    tmp0 += (z1 + z3);
    tmp1 += (z2 + z4);
    tmp2 += (z2 + z3);
    tmp3 += (z1 + z4);

    wsptr[DCTSIZE*0] = DESCALE(tmp10 + tmp3, CONST_BITS-PASS1_BITS);
    wsptr[DCTSIZE*7] = DESCALE(tmp10 - tmp3, CONST_BITS-PASS1_BITS);
    wsptr[DCTSIZE*1] = DESCALE(tmp11 + tmp2, CONST_BITS-PASS1_BITS);
    wsptr[DCTSIZE*6] = DESCALE(tmp11 - tmp2, CONST_BITS-PASS1_BITS);
    wsptr[DCTSIZE*2] = DESCALE(tmp12 + tmp1, CONST_BITS-PASS1_BITS);
    wsptr[DCTSIZE*5] = DESCALE(tmp12 - tmp1, CONST_BITS-PASS1_BITS);
    wsptr[DCTSIZE*3] = DESCALE(tmp13 + tmp0, CONST_BITS-PASS1_BITS);
    wsptr[DCTSIZE*4] = DESCALE(tmp13 - tmp0, CONST_BITS-PASS1_BITS);

    inptr++;
    wsptr++;
  }

  /* pass 2: traitement ligne par ligne */

  wsptr = &workspace[0];
  for (ctr = 0; ctr < DCTSIZE; ctr++)
  {
    outptr = &coef[DCTSIZE*ctr];

    z2 = wsptr[2];
    z3 = wsptr[6];

    z1 = (z2 + z3) * FIX_0_541196100;

    tmp2 = z1 + (z3 * (- FIX_1_847759065));
    tmp3 = z1 + (z2 * (  FIX_0_765366865));

    tmp0 = (wsptr[0] + wsptr[4]) << CONST_BITS;
    tmp1 = (wsptr[0] - wsptr[4]) << CONST_BITS;

    tmp10 = tmp0 + tmp3;
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    tmp0 = wsptr[7];
    tmp1 = wsptr[5];
    tmp2 = wsptr[3];
    tmp3 = wsptr[1];

    z1 = tmp0 + tmp3;
    z2 = tmp1 + tmp2;
    z3 = tmp0 + tmp2;
    z4 = tmp1 + tmp3;

    z5 = (z3 + z4) * FIX_1_175875602;   /* sqrt(2) * c3 */

    tmp0 *= FIX_0_298631336;   /* sqrt(2) * (-c1+c3+c5-c7) */
    tmp1 *= FIX_2_053119869;   /* sqrt(2) * ( c1+c3-c5+c7) */
    tmp2 *= FIX_3_072711026;   /* sqrt(2) * ( c1+c3+c5-c7) */
    tmp3 *= FIX_1_501321110;   /* sqrt(2) * ( c1+c3-c5-c7) */

    z1 *= (- FIX_0_899976223);   /* sqrt(2) * (c7-c3) */
    z2 *= (- FIX_2_562915447);   /* sqrt(2) * (-c1-c3) */
    z3 *= (- FIX_1_961570560);   /* sqrt(2) * (-c3-c5) */
    z4 *= (- FIX_0_390180644);   /* sqrt(2) * (c5-c3) */

    z3 += z5;
    z4 += z5;

    tmp0 += z1 + z3;
    tmp1 += z2 + z4;
    tmp2 += z2 + z3;
    tmp3 += z1 + z4;

    outptr[0] = range_limit(DESCALE(tmp10 + tmp3, CONST_BITS+PASS1_BITS+3));
    outptr[7] = range_limit(DESCALE(tmp10 - tmp3, CONST_BITS+PASS1_BITS+3));
    outptr[1] = range_limit(DESCALE(tmp11 + tmp2, CONST_BITS+PASS1_BITS+3));
    outptr[6] = range_limit(DESCALE(tmp11 - tmp2, CONST_BITS+PASS1_BITS+3));
    outptr[2] = range_limit(DESCALE(tmp12 + tmp1, CONST_BITS+PASS1_BITS+3));
    outptr[5] = range_limit(DESCALE(tmp12 - tmp1, CONST_BITS+PASS1_BITS+3));
    outptr[3] = range_limit(DESCALE(tmp13 + tmp0, CONST_BITS+PASS1_BITS+3));
    outptr[4] = range_limit(DESCALE(tmp13 - tmp0, CONST_BITS+PASS1_BITS+3));

    wsptr += DCTSIZE;
  }
}

//-------------------------------------------------------------------------------------

package TABLES

/* tables de quantization constantes pour une quality jpeg de 50 */
/* allowed range : 0 .. 100 (100 = no quantization) */

/* pour créer des tables pour d'autres qualités Q,                          */
/*   il faut appliquer à chaque valeur V la formule : (V*(200-2*Q)+50)/100  */
/*   (le +50 dans la formule sert à arrondir)                               */

/* standard jpeg tables */
const int std_luminance_quant_tbl[8*8] =
  { 16,  11,  10,  16,  24,  40,  51,  61,
    12,  12,  14,  19,  26,  58,  60,  55,
    14,  13,  16,  24,  40,  57,  69,  56,
    14,  17,  22,  29,  51,  87,  80,  62,
    18,  22,  37,  56,  68, 109, 103,  77,
    24,  35,  55,  64,  81, 104, 113,  92,
    49,  64,  78,  87, 103, 121, 120, 101,
    72,  92,  95,  98, 112, 100, 103,  99 };

const int std_chrominance_quant_tbl[8*8] =
  { 17,  18,  24,  47,  99,  99,  99,  99,
    18,  21,  26,  66,  99,  99,  99,  99,
    24,  26,  56,  99,  99,  99,  99,  99,
    47,  66,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99,
    99,  99,  99,  99,  99,  99,  99,  99 };

end TABLES;

//-------------------------------------------------------------------------------------

void create_quantization_tables (    int   quality,    // 0 .. 100
                                 out short quantization_table[2][8*8])
{
  int  typ, val, i, scale_percent;
  int* qtab;

  clear quantization_table;

  for (typ=0; typ<2; typ++)
  {
    if (typ == 0)
      qtab = &std_luminance_quant_tbl;
    else
      qtab = &std_chrominance_quant_tbl;

    scale_percent = (200 - 2*quality);

    for (i=0; i<64; i++)
    {
      val = (qtab[i] * scale_percent + 50) / 100;

      if (val <= 0)   // zero not allowed
        val = 1;

      if (val > 32767)
        val = 32767;

      quantization_table[typ][i] = (short)val;
    }
  }
}

//-------------------------------------------------------------------------------------

public void quantize_dct_image (    int       quality,   // 0 .. 100
                                ref DCT_IMAGE dct)
{
  short       quantization_table[2][8*8];
  int         pl, checked_quality;
  DCT_PLANE*  p;
  DCT_MATRIX* m;
  int         i, j, qval, c, count;


  /* store quality */

  if (quality < 0)
    checked_quality = 0;
  else if (quality > 100)
    checked_quality = 100;
  else
    checked_quality = quality;

  dct.quality = checked_quality;


  /* prepare quantization tables */

  create_quantization_tables (checked_quality, out quantization_table);


  /* Quantize (avec arrondi très important !) */

  for (pl=0; pl<3; pl++)
  {
    p = &dct.component[pl]^;
    m = &p->dct^;
    count = p->dct^'length;

    for (i=0; i<count; i++)
    {
      for (j=0; j<64; j++)
      {
        qval = quantization_table[(int)(pl>0)][j] << 3;

        c = m[0][j];
        if (c < 0)
        {
          c = -c;

          c += qval>>1;   // for rounding
          c = (c >= qval) ? c/qval : 0;

          c = -c;
        }
        else
        {
          c += qval>>1;  // for rounding
          c = (c >= qval) ? c/qval : 0;
        }

        m[0][j] = (short)c;
      }

      m++;  // next DCT block
    }
  }
}

//-------------------------------------------------------------------------------------

public void unquantize_dct_image (ref DCT_IMAGE dct)
{
  short       quantization_table[2][8*8];
  DCT_PLANE*  p;
  DCT_MATRIX* m;
  int         pl, i, j, count;


  // prepare quantization tables

  create_quantization_tables (dct.quality, out quantization_table);


  for (pl=0; pl<3; pl++)
  {
    p = &dct.component[pl]^;
    m = &p->dct^;
    count = p->dct^'length;

    for (i=0; i<count; i++)
    {
      for (j=0; j<64; j++)
        m[i][j] *= quantization_table[(int)(pl>0)][j];
    }
  }
}

//-------------------------------------------------------------------------------------

package RGB_TABLE
  bool  rgb_to_plane_table_filled;
  int rgb_to_plane[3][3][256];
end RGB_TABLE;

void fill_rgb_to_plane_table ()
{
  int i;
  for (i=0; i<256; i++)
  {
    /* Y */
    rgb_to_plane[0][0][i] = FIX(0.29900) * i;
    rgb_to_plane[1][0][i] = FIX(0.58700) * i;
    rgb_to_plane[2][0][i] = (FIX(0.11400) * i) + 32768 + (-128 << 16);

    /* Cr */
    rgb_to_plane[0][1][i] = FIX(0.50000) * i + 32767;
    rgb_to_plane[1][1][i] = (-FIX(0.41869)) * i;
    rgb_to_plane[2][1][i] = ((-FIX(0.08131)) * i);

    /* Cb */
    rgb_to_plane[0][2][i] = (-FIX(0.16874)) * i;
    rgb_to_plane[1][2][i] = (-FIX(0.33126)) * i;
    rgb_to_plane[2][2][i] = (FIX(0.50000) * i) + 32767;
  }
}

//-------------------------------------------------------------------------------------

package RGB_TABLE2
  bool plane_to_rgb_table_filled;
  int  plane_to_rgb[3][3][256];
end RGB_TABLE2;

void fill_plane_to_rgb_table ()
{
  int i, c;
  for (i=0,c=-128; i<256; i++,c++)
  {
    plane_to_rgb[1][0][i] = ( (FIX(1.40200) * c + 32768) >> 16);
    plane_to_rgb[2][2][i] = ( (FIX(1.77200) * c + 32768) >> 16);
    plane_to_rgb[1][1][i] = (- FIX(0.71414)) * c;
    plane_to_rgb[2][1][i] = (- FIX(0.34414)) * c + 32768;
  }
}

//-------------------------------------------------------------------------------------

void convert_16x16_rgb_to_colorspace (    int        pl,   // 1 or 2
                                          short**    m,
                                          uint       x,
                                          uint       y,
                                          uint       ofs_x,
                                          uint       width,
                                          uint       height,
                                          byte*      pix)
{
  uint  ofs_yy, ofs_xx, yy, xx, yyy, xxx, last_xx, last_yy, ofs_yyy, ofs_xxx, last_xxx, last_yyy;
  short val = 0, sum;

  /* examine a 16x16 square that is possibly cut at image right and lower borders */

  ofs_yy = ofs_x;          /* byte-offset within pix[] */
  last_yy = y+16;

  for (yy=y; yy<last_yy; yy+=2)    /* loop 8x */
  {
    ofs_xx = ofs_yy;
    last_xx = x+16;

    for (xx=x; xx<last_xx; xx+=2)   /* loop 8x */
    {
      ofs_yyy = ofs_xx;
      last_yyy = yy+2;

      /* take average color of a 2x2 rgbs square */

      sum = 0;

      for (yyy=yy; yyy<last_yyy; yyy++)    /* loop 2x */
      {
        ofs_xxx = ofs_yyy;
        last_xxx = xx+2;

        for (xxx=xx; xxx<last_xxx; xxx++)   /* loop 2x */
        {
          if (xxx < width)    // repeat last pixel color for right border
          {
            val = (short)
                ((  rgb_to_plane[0][pl][pix[ofs_xxx + 0]]
                  + rgb_to_plane[1][pl][pix[ofs_xxx + 1]]
                  + rgb_to_plane[2][pl][pix[ofs_xxx + 2]]) >> 16);
          }
          sum += (short)val;

          ofs_xxx += 4;   /* 1 pixel right */
        }

        if (yyy+1 < height)
          ofs_yyy += 4*width;              /* down 1 line  */
      }

      *((*m)++) = (short)(sum >> 2);

      ofs_xx += (4*2);    // 2 pixels right
    }

    if (yy+2 < height)
      ofs_yy += ((2*4)*width);          /* down 2 lines */
  }
}

//-------------------------------------------------------------------------------------

uint ROUND_UP8 (uint x)
{
  return ((((x)+7) >> 3) << 3);
}

uint ROUND_UP16 (uint x)
{
  return ((((x)+15) >> 4) << 4);
}

//-------------------------------------------------------------------------------------

int allocate_dct_planes (out DCT_IMAGE dct, uint width, uint height, int quality)
{
  int  pl;
  uint rwidth, rheight, count;

  clear dct;

  if (width > 4096 || height > 4096 || quality < 0 || quality > 100)
    return -1;

  dct.width  = width;
  dct.height = height;
  dct.quality = quality;

  rwidth  = ROUND_UP8 (width);       /* 0, 8, 16, 24, 32, 40, 48, ... */
  rheight = ROUND_UP8 (height);

  for (pl=0; pl<3; pl++)
  {
    if (pl == 1)   /* first chromatic plane : change resolution */
    {
      rwidth  = ROUND_UP16 (rwidth)  >> 1;    /* 0, 16, 32, 48, .. */
      rheight = ROUND_UP16 (rheight) >> 1;    /* divide by 2 because we downsample color planes */
    }

    count = (rwidth * rheight) >> 6;    /* nb of DCT matrices (64 coefficients) */

    dct.component[pl] = new DCT_PLANE
                         ' {nb_cols  => (rwidth >> 3),     /* divide by 8 because each DCT is a 8x8 block */
                            nb_lines => (rheight >> 3),
                            dct      => new DCT_MATRIX[count]};
  }

  return 0;
}

//-------------------------------------------------------------------------------------

public void free_dct_image (ref DCT_IMAGE dct)
{
  int pl;
  for (pl=0; pl<3; pl++)
  {
    if (dct.component[pl] != null)
      free dct.component[pl]^.dct;
    free dct.component[pl];
  }
  clear dct;
}

//-------------------------------------------------------------------------------------

public void free_huff_buffer_list (ref HUFF_CHUNK^ list)
{
  HUFF_CHUNK^ p;
  while (list != null)
  {
    p = list^.next;
    free list^.buffer;
    free list;
    list = p;
  }
}

//-------------------------------------------------------------------------------------

public int convert_rgbs_image_to_dct_image (IMAGE_INFO image, out DCT_IMAGE dct)
{
  int        rc, count, pl;
  DCT_PLANE* p;


  /* 1) create Y, Cr and Cb DCT planes */

  rc = allocate_dct_planes (out dct, image.width, image.height, quality => 0);
  if (rc < 0)
    return rc;


  /* 2) prepare RGB to Y Cr Cb conversion table */

  if (!rgb_to_plane_table_filled)
  {
    fill_rgb_to_plane_table();
    rgb_to_plane_table_filled = true;
  }


  /* 3) extract Y from RGB and convert it directly in DCT matrix form */

  {
    short  val=0, m*;
    uint   width, height;
    uint   ofs_y, ofs_x, ofs_yy, ofs_xx, y, x, yy, xx, last_xx, last_yy;
    byte*  pix;

    p   = &dct.component[0]^;
    m   = &p->dct^[0];      /* destination pointer */
    pix = &image.pixel^;

    width  = dct.width;
    height = dct.height;

    ofs_y = 0;                       /* byte-offset into pix[] */
    for (y=0; y<height; y+=8)
    {
      ofs_x = ofs_y;
      for (x=0; x<width; x+=8)
      {
        /* examine a 8x8 square that is possibly cut at image right and lower borders */

        ofs_yy = ofs_x;
        last_yy = y+8;

        for (yy=y; yy<last_yy; yy++)
        {
          ofs_xx = ofs_yy;
          last_xx = x+8;

          for (xx=x; xx<last_xx; xx++)
          {
            if (xx < width)    // keep last color for right border
            {
              val = (short)
                ((  rgb_to_plane[0][0][pix[ofs_xx + 0]]
                  + rgb_to_plane[1][0][pix[ofs_xx + 1]]
                  + rgb_to_plane[2][0][pix[ofs_xx + 2]]) >> 16);
            }

            *m++ = val;

            ofs_xx += 4;
          }

          if (yy < height-1)    /* repeat last line if at bottom */
            ofs_yy += 4*width;
        }

        ofs_x += 4*8;         /* one DCT matrix to the right */
      }

      ofs_y += (4*8)*width;   /* one row of DCT matrices lower */
    }
  }


  /* 4) extract Cr Cb from RGB, downsample it and convert directly in DCT matrix form */

  for (pl=1; pl<=2; pl++)
  {
    short* m;
    uint   width, height;
    uint   x, y, ofs_y, ofs_x;
    byte*  pix;

    p   = &dct.component[pl]^;
    m   = &p->dct^[0];      /* destination pointer */
    pix = &image.pixel^;

    width  = dct.width;
    height = dct.height;

    ofs_y = 0;                  /* byte-offset within pix[] */
    for (y=0; y<height; y+=16)
    {
      ofs_x = ofs_y;
      for (x=0; x<width; x+=16)
      {
        /* examine a 16x16 square that is possibly cut at image right and lower borders */
        convert_16x16_rgb_to_colorspace (pl, &m, x, y, ofs_x, width, height, pix);

        ofs_x += 4*16;         /* one DCT matrix to the right */
      }

      ofs_y += (4*16)*width;   /* one row of DCT matrices lower */
    }
  }


  /* 5) perform DCT */

  for (pl=0; pl<3; pl++)
  {
    DCT_MATRIX* m;
    int         i;

    p = &dct.component[pl]^;
    m = &p->dct^;          /* destination pointer */
    count = p->dct^'length;

    for (i=0; i<count; i++)
      perform_dct (ref m[i]);
  }

  return 0;
}

//-------------------------------------------------------------------------------------

int compute_total_encodings_bits (HUF_STATS           stats,
                                  HUFF_TABLE          dc_ht,
                                  HUFF_TABLE          ac_ht,
                                  HUFF_ENCODING_TABLE dc_et,
                                  HUFF_ENCODING_TABLE ac_et)
{
  int i, sum, count;

  sum = stats.nb_bits;

  /* dc table */
  sum += 16 * 4;
  count = 0;
  for (i=1; i<=16; i++)
    count += dc_ht.nb_symbols_having_nb_bits[i];
  sum += count * 4;

  /* ac table */
  sum += 16 * 8;
  count = 0;
  for (i=1; i<=16; i++)
    count += ac_ht.nb_symbols_having_nb_bits[i];
  sum += count * 8;

  for (i=0; i<16; i++)
    sum += stats.dc_counts[i] * (int)dc_et.code[i].nb_bits;

  for (i=0; i<256; i++)
    sum += stats.ac_counts[i] * (int)ac_et.code[i].nb_bits;

  return sum;
}

//-------------------------------------------------------------------------------------

void seuillage (int width, int height, tiny* p)
{
  bool changes;
  int  s, count, x, y, x0, y0;


  /* flood surrounded islands */
  /* note: we do NOT remove isolated blocks because this produces image defects (arm moving slowly, frontier is double) */

  for (;;)
  {
    changes = false;
    for (y=0; y<height; y++)
    {
      for (x=0; x<width; x++)
      {
        s = p[x+y*width];

        if (s < 0 && s != -99)  /* very slight move */
        {
          count = 0;
          for (x0=x-1; x0<=x+1; x0++)
          {
            for (y0=y-1; y0<=y+1; y0++)
            {
              if (x != x0 || y != y0)
              {
                if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height && p[x0+y0*width] >= 0)
                  count++;
              }
            }
          }

          if (count >= 5)    /* 5 neighbours */
          {
            p[x+y*width] = 77;    /* move */
            changes = true;
          }
        }
      }
    }
    if (!changes)
      break;
  }
}

//-------------------------------------------------------------------------------------

/* 'cache' : set to zero by caller at start,    */
/*           will be allocated by function,     */
/*           kept by caller between calls,      */
/*           freed by caller at end of session. */
/* 'dct' must be freed after call.              */

public int huff_encode_dct_image (    DCT_IMAGE   dct,                  /* source */
                                  ref DCT_IMAGE   cache,                /* last sent dct */
                                      uint        luminosity_threshold, /* 64 */
                                      uint        chromatic_threshold,  /* 32 */
                                  out HUFF_CHUNK^ pbuffers)
{
  const int MIN_ZERO_MATRICES = 1;

  int                 pl;
  DCT_PLANE*          p, p0;
  DCT_MATRIX          m*, m0*, delta_matrix;
  tiny[]^             changed[3];
  tiny*               pchanged;
  byte                method[3];      /* 0=don't send, 1=send value, 2=send delta */
  int                 i, count, rc, j, k, nb_zero_matrices, threshold, sum, diff;
  HUFF_TABLE          dc_huff_table[3], ac_huff_table[3];
  HUFF_ENCODING_TABLE huff_encoding_dc_table[3], huff_encoding_ac_table[3];
  HUFF_OUTPUT         hout;


  pbuffers = null;


  if (cache.component[0] == null ||   /* no cache or bad size : create new cache */
      cache.width != dct.width   ||   /* filled with zeroes */
      cache.height != dct.height ||
      cache.quality != dct.quality)
  {
    free_dct_image (ref cache);    /* in case it's allocated with wrong size/wrong quality */

    rc = allocate_dct_planes (out cache, dct.width, dct.height, dct.quality);
    if (rc < 0)
      return rc;
  }
  else
  {
    for (pl=0; pl<3; pl++)
    {
      if (cache.component[pl]^.dct^'length != dct.component[pl]^.dct^'length)
      {
        trace ("error: huff_encode_dct_image (plane %d) : mismatching plane count\n", pl);
        return -5002;
      }
    }
  }


  /* allocate changed[] arrays */

  clear changed;
  for (pl=0; pl<3; pl++)
    changed[pl] = new tiny [dct.component[pl]^.dct^'length];


  /* fill changed[] arrays with all matrices that have changed */

  clear method;

  for (pl=0; pl<3; pl++)
  {
    p0 = &cache.component[pl]^;
    m0 = &p0->dct^;

    p = &dct.component[pl]^;
    m = &p->dct^;
    count = p->dct^'length;

    if (pl == 0)
      threshold = (int)luminosity_threshold;
    else
      threshold = (int)chromatic_threshold;

    pchanged = &changed[pl]^;

    method[pl] = 0;

    for (i=0; i<count; i++)
    {
      sum = 0;

      for (k=0; k<64; k++)
      {
        diff = m[i][k] - m0[i][k];
        sum += diff * diff;
      }

      if (sum == 0 && threshold > 0)
        sum = -99;           /* no move at all -> -99 */
      else
      {
        sum -= threshold;    /* < 0 means no move, >= 0 means move */

        if (sum > 98)
          sum = 98;
        if (sum < -98)
          sum = -98;
      }

      pchanged[i] = (tiny)sum;
    }

    seuillage ((int)p->nb_cols, (int)p->nb_lines, pchanged);


    /* select method */

    pchanged = &changed[pl]^;
    for (i=0; i<count; i++)
    {
      if (pchanged[i] >= 0)   /* move */
      {
        method[pl] = 1;
        break;
      }
    }
  }


  clear dc_huff_table, ac_huff_table, huff_encoding_dc_table, huff_encoding_ac_table;
  clear delta_matrix;

  /*
   there are 3 methods for each plane:
   0) don't transmit plane (and no DC/AC tables for this plane)
   1) transmit plane by values (full plane in case of null cache)
   2) transmit plane by delta
  */

  for (pl=0; pl<3; pl++)
  {
    HUF_STATS huf_stats[2];    /* 0 = per value, 1 = per delta */

    if (method[pl] == 0)   /* don't send this plane */
      continue;

    p0 = &cache.component[pl]^;
    m0 = &p0->dct^;

    p = &dct.component[pl]^;
    m = &p->dct^;
    count = p->dct^'length;

    pchanged = &changed[pl]^;


    /* compute statistics for transmit per value, per delta */

    clear huf_stats;

    for (i=0; i<count; i++)
    {
      nb_zero_matrices = 0;

      for (j=i; j<count; j++)
      {
        if (pchanged[j] >= 0)
          break;
        nb_zero_matrices++;
      }

      if (nb_zero_matrices >= MIN_ZERO_MATRICES)
      {
        rc = gather_statistics_zero_matrices (nb_zero_matrices, ref huf_stats[0]);  /* per value */
        if (rc < 0)
        {
          trace ("error: gather_statistics_zero_matrices (plane %d)(0) returned %d\n", pl, rc);
          for (pl=0; pl<3; pl++)
            free (changed[pl]);
          return rc;
        }

        rc = gather_statistics_zero_matrices (nb_zero_matrices, ref huf_stats[1]);  /* per delta */
        if (rc < 0)
        {
          trace ("error: gather_statistics_zero_matrices (plane %d)(1) returned %d\n", pl, rc);
          for (pl=0; pl<3; pl++)
            free (changed[pl]);
          return rc;
        }

        i = j - 1;
      }
      else
      {
        rc = gather_dct_coef_statistics (m[i], ref huf_stats[0]);  /* per value */
        if (rc < 0)
        {
          trace ("error: gather_dct_coef_statistics (plane %d)(0) returned %d\n", pl, rc);
          for (pl=0; pl<3; pl++)
            free (changed[pl]);
          return rc;
        }

        for (k=0; k<delta_matrix'length; k++)
          delta_matrix[k] = (short)(m[i][k] - m0[i][k]);

        rc = gather_dct_coef_statistics (delta_matrix, ref huf_stats[1]);  /* per delta */
        if (rc < 0)
        {
          trace ("error: gather_dct_coef_statistics (plane %d)(1) returned %d\n", pl, rc);
          for (pl=0; pl<3; pl++)
            free (changed[pl]);
          return rc;
        }
      }
    }


    /* now evaluate which method is best */

    {
      HUFF_TABLE          dc_ht[2], ac_ht[2];
      HUFF_ENCODING_TABLE dc_et[2], ac_et[2];
      int                 summ[2];

      clear dc_ht, ac_ht, dc_et, ac_et, summ;

      /* compute huffman tables */
      for (i=0; i<2; i++)
      {
        create_huff_table (huf_stats[i].dc_counts, out dc_ht[i]);
        create_huff_table (huf_stats[i].ac_counts, out ac_ht[i]);

        rc = create_huff_encoding_table (dc_ht[i],  16, out dc_et[i]);
        if (rc < 0)
        {
          trace ("error: create_huff_encoding_table(plane %d) dc returned %d\n", pl, rc);
          for (pl=0; pl<3; pl++)
            free (changed[pl]);
          return rc;
        }

        rc = create_huff_encoding_table (ac_ht[i], 256, out ac_et[i]);
        if (rc < 0)
        {
          trace ("error: create_huff_encoding_table(plane %d) ac returned %d\n", pl, rc);
          for (pl=0; pl<3; pl++)
            free (changed[pl]);
          return rc;
        }

        summ[i] = compute_total_encodings_bits (huf_stats[i], dc_ht[i], ac_ht[i], dc_et[i], ac_et[i]);
      }

//      trace ("info: plane %d : value=%d bytes, delta=%d bytes\n", pl, summ[0]>>3, summ[1]>>3);

      if (summ[1] < summ[0])
      {
        method[pl] = 2;      /* delta method produces less output */
        i = 1;
      }
      else
      {
        method[pl] = 1;      /* value method produces less output */
        i = 0;
      }

      dc_huff_table[pl] = dc_ht[i];  // copy HUFF_TABLE
      ac_huff_table[pl] = ac_ht[i];  // copy HUFF_TABLE

      huff_encoding_dc_table[pl] = dc_et[i];   // copy HUFF_ENCODING_TABLE
      huff_encoding_ac_table[pl] = ac_et[i];   // copy HUFF_ENCODING_TABLE
    }
  }


  /* produce output */

  huff_init_output (out hout);

  rc = huff_emit_bits (ref hout, dct.width, 13);
  if (rc < 0)
  {
    free_huff_buffer_list (ref hout.head);
    for (pl=0; pl<3; pl++)
      free changed[pl];
    return rc;
  }

  rc = huff_emit_bits (ref hout, dct.height, 13);
  if (rc < 0)
  {
    free_huff_buffer_list (ref hout.head);
    for (pl=0; pl<3; pl++)
      free changed[pl];
    return rc;
  }

  rc = huff_emit_bits (ref hout, (uint)dct.quality, 7);
  if (rc < 0)
  {
    free_huff_buffer_list (ref hout.head);
    for (pl=0; pl<3; pl++)
      free changed[pl];
    return rc;
  }


  /* for all 3 planes, send methods + dc/ac tables */

  for (pl=0; pl<3; pl++)
  {
    rc = huff_emit_bits (ref hout, method[pl], 2);    /* 0 = no table, 1 = values, 2 = delta */
    if (rc < 0)
    {
      free_huff_buffer_list (ref hout.head);
      for (pl=0; pl<3; pl++)
        free changed[pl];
      return rc;
    }

    if (method[pl] == 0)    /* no table */
      continue;

    rc = send_huff_table (ref hout, dc_huff_table[pl], 16);
    if (rc < 0)
    {
      free_huff_buffer_list (ref hout.head);
      for (pl=0; pl<3; pl++)
        free changed[pl];
      return rc;
    }

    rc = send_huff_table (ref hout, ac_huff_table[pl], 256);
    if (rc < 0)
    {
      free_huff_buffer_list (ref hout.head);
      for (pl=0; pl<3; pl++)
        free changed[pl];
      return rc;
    }
  }


  /* send data */

  for (pl=0; pl<3; pl++)
  {
    if (method[pl] == 0)   /* don't send this plane */
      continue;

    p0 = &cache.component[pl]^;
    m0 = &p0->dct^;

    p = &dct.component[pl]^;
    m = &p->dct^;
    count = p->dct^'length;

    pchanged = &changed[pl]^;

    for (i=0; i<count; i++)
    {
      nb_zero_matrices = 0;

      for (j=i; j<count; j++)
      {
        if (pchanged[j] >= 0)
          break;
        nb_zero_matrices++;
      }

      if (nb_zero_matrices >= MIN_ZERO_MATRICES)
      {
        rc = huff_encode_zero_matrices (ref hout, nb_zero_matrices, huff_encoding_dc_table[pl]);
        if (rc < 0)
        {
          trace ("error: huff_encode_zero_matrices (plane %d) returned %d\n", pl, rc);
          free_huff_buffer_list (ref hout.head);
          for (pl=0; pl<3; pl++)
            free changed[pl];
          return rc;
        }

        i = j - 1;
      }
      else
      {
        if (method[pl] == 1)
        {
          rc = huff_encode_dct_coef (ref hout, m[i], huff_encoding_dc_table[pl], huff_encoding_ac_table[pl]);
        }
        else
        {
          for (k=0; k<delta_matrix'length; k++)
            delta_matrix[k] = (short)(m[i][k] - m0[i][k]);
          rc = huff_encode_dct_coef (ref hout, delta_matrix, huff_encoding_dc_table[pl], huff_encoding_ac_table[pl]);
        }

        if (rc < 0)
        {
          trace ("error: huff_encode_dct_coef (plane %d) returned %d\n", pl, rc);
          free_huff_buffer_list (ref hout.head);
          for (pl=0; pl<3; pl++)
            free changed[pl];
          return rc;
        }

        m0[i] = m[i];   // copy DCT_MATRIX
      }
    }
  }


  for (pl=0; pl<3; pl++)
    free changed[pl];


  rc = flush_bits (ref hout);
  if (rc < 0)
  {
    trace ("error: flush_bits() returned %d\n", rc);
    free_huff_buffer_list (ref hout.head);
    return rc;
  }

  pbuffers = hout.head;

  return 0;
}

//-------------------------------------------------------------------------------------

public int copy_dct_image (    DCT_IMAGE previous_dct,
                           out DCT_IMAGE dct)
{
  int pl, rc;

  if (previous_dct.width == 0 || previous_dct.height == 0)
  {
    clear dct;
    return 0;
  }

  rc = allocate_dct_planes (out dct, previous_dct.width, previous_dct.height, previous_dct.quality);
  if (rc < 0)
    return rc;

  for (pl=0; pl<3; pl++)
    dct.component[pl]^.dct^ = previous_dct.component[pl]^.dct^;

  return 0;
}

//-------------------------------------------------------------------------------------

public int convert_dct_image_to_rgbs_image (    DCT_IMAGE  dct,
                                            out IMAGE_INFO image)
{
  DCT_PLANE*  p;
  DCT_MATRIX* m;
  int         pl, i, count;


  /* inverse DCT transformation */

  for (pl=0; pl<3; pl++)
  {
    p = &dct.component[pl]^;
    m = &p->dct^;
    count = p->dct^'length;

    for (i=0; i<count; i++)
      inverse_dct (ref m[i]);
  }


  /* prepare Y Cr Cb to RGB conversion table */

  if (!plane_to_rgb_table_filled)
  {
    fill_plane_to_rgb_table();
    plane_to_rgb_table_filled = true;
  }

  image = {pixel  => new byte [4 * dct.width * dct.height],
           width  => dct.width,
           height => dct.height};


  /* convert Y Cr Cb into RGB with upsampling of the chromatic planes */

  {
    uint        width, height, x, y;
    int         y_offset, n, yy, cr, cb;
    DCT_PLANE*  plane[3];
    DCT_MATRIX* mm[3];
    ref byte[]  pixel = image.pixel^;

    width  = dct.width;
    height = dct.height;

    plane = {&dct.component[0]^, &dct.component[1]^, &dct.component[2]^};
    mm    = {&plane[0]->dct^,    &plane[1]->dct^,    &plane[2]->dct^};

    y_offset = 0;
    for (y=0; y<height; y++)
    {
      for (x=0; x<width; x++)
      {
        yy = mm[0] [(y>>3)*plane[0]->nb_cols + (x>>3)] [((y&7)<<3)+(x&7)];
        cr = mm[1] [(y>>4)*plane[1]->nb_cols + (x>>4)] [(((y&15)>>1)<<3)+((x&15)>>1)];
        cb = mm[2] [(y>>4)*plane[2]->nb_cols + (x>>4)] [(((y&15)>>1)<<3)+((x&15)>>1)];

        n = yy + plane_to_rgb[1][0][cr];
        if (n < 0)
          n = 0;
        if (n > 255)
          n = 255;
        pixel[4*(x + (uint)y_offset) + 0] = (byte)n;

        n = yy + ((plane_to_rgb[1][1][cr] + plane_to_rgb[2][1][cb]) >> 16);
        if (n < 0)
          n = 0;
        if (n > 255)
          n = 255;
        pixel[4*(x + (uint)y_offset) + 1] = (byte)n;

        n = yy + plane_to_rgb[2][2][cb];
        if (n < 0)
          n = 0;
        if (n > 255)
          n = 255;
        pixel[4*(x + (uint)y_offset) + 2] = (byte)n;

        pixel[4*(x + (uint)y_offset) + 3] = 255;
      }

      y_offset += (int)image.width;
    }
  }

  return 0;
}

//-------------------------------------------------------------------------------------

public int huff_decode_dct_image (    HUFF_CHUNK^ buffers,
                                  ref DCT_IMAGE   cache,            // last received dct
                                  out int         percent_change)
{
  HUFF_INPUT          in;
  uint                width, height;
  int                 pl, i, count, rc, skip_zero_matrices, quality;
  int                 method[3];
  HUFF_TABLE          dc_huff_table[3], ac_huff_table[3];
  HUFF_DECODING_TABLE huff_decoding_dc_table[3], huff_decoding_ac_table[3];
  DCT_PLANE*          p;
  DCT_MATRIX*         m;
  int                 nb_changed, nb_total;

  huff_init_input (out in, buffers);

  width   = (uint)huff_receive_bits (ref in, 13);
  height  = (uint)huff_receive_bits (ref in, 13);
  quality = huff_receive_bits (ref in,  7);

  percent_change = 0;

  if (width > 4096 || height > 4096 || quality < 0 || quality > 100)
    return -2002;

  if (cache.component[0] == null ||    // no cache or cache with wrong size/wrong quality
      cache.width != width ||          // create new cache
      cache.height != height ||
      cache.quality != quality)
  {
    free_dct_image (ref cache);    /* in case it's allocated with wrong size */

    rc = allocate_dct_planes (out cache, width, height, quality);
    if (rc < 0)
      return rc;
  }


  /* receive method + huffman tables */

  clear method, dc_huff_table, ac_huff_table, huff_decoding_dc_table, huff_decoding_ac_table;

  for (pl=0; pl<3; pl++)
  {
    method[pl] = huff_receive_bits (ref in, 2);

    if (method[pl] > 2)
    {
      trace ("error: huff_decode_dct_image (plane %d) : bad method\n", pl);
      return -6003;
    }

    if (method[pl] == 0)    /* no table */
      continue;

    rc = receive_huff_table (ref in, out dc_huff_table[pl], 16);
    if (rc < 0)
    {
      trace ("error: receive_huff_table(dc, plane %d) returned %d\n", pl, rc);
      return rc;
    }

    rc = receive_huff_table (ref in, out ac_huff_table[pl], 256);
    if (rc < 0)
    {
      trace ("error: receive_huff_table(ac, plane %d) returned %d\n", pl, rc);
      return rc;
    }
  }

  for (pl=0; pl<3; pl++)
  {
    if (method[pl] == 0)    /* no plane */
      continue;

    rc = create_huff_decoding_table (    dc_huff_table[pl],
                                         16,
                                     out huff_decoding_dc_table[pl]);
    if (rc < 0)
    {
      trace ("error: create_huff_decoding_table(plane %d) returned %d\n", pl, rc);
      return rc;
    }

    rc = create_huff_decoding_table (    ac_huff_table[pl],
                                         256,
                                     out huff_decoding_ac_table[pl]);
    if (rc < 0)
    {
      trace ("error: create_huff_decoding_table(plane %d) returned %d\n", pl, rc);
      return rc;
    }
  }


  nb_changed = 0;
  nb_total = 0;

  for (pl=0; pl<3; pl++)
  {
    if (method[pl] == 0)    /* no plane */
      continue;

    p = &cache.component[pl]^;
    m = &p->dct^;
    count = p->dct^'length;

    nb_total += count;

    if (method[pl] == 1)    /* value */
    {
      DCT_MATRIX matrix;

      i = 0;
      while (i < count)
      {
        rc = huff_decode_dct_coef (ref in,
                                   out matrix,
                                       huff_decoding_dc_table[pl],
                                       huff_decoding_ac_table[pl],
                                   out skip_zero_matrices);
        if (rc < 0)
        {
          trace ("error: huff_decode_dct_coef (plane %d) returned %d\n", pl, rc);
          return rc;
        }

        if (skip_zero_matrices != 0)
        {
          i += skip_zero_matrices;   /* don't touch cache's skipped matrices */
        }
        else
        {
          nb_changed++;
          m[i] = matrix;
          i++;
        }
      }
    }
    else    /* method 2 : delta */
    {
      DCT_MATRIX matrix;
      int        k;

      i = 0;
      while (i < count)
      {
        rc = huff_decode_dct_coef (ref in,
                                   out matrix,
                                       huff_decoding_dc_table[pl],
                                       huff_decoding_ac_table[pl],
                                   out skip_zero_matrices);
        if (rc < 0)
        {
          trace ("error: huff_decode_dct_coef (plane %d) returned %d\n", pl, rc);
          return rc;
        }

        if (skip_zero_matrices != 0)
        {
          i += skip_zero_matrices;    /* don't touch cache's skipped matrices */
        }
        else
        {
          nb_changed++;
          for (k=0; k<64; k++)
            m[i][k] += matrix[k];
          i++;
        }
      }
    }
  }

  if (nb_total != 0)
    percent_change = nb_changed * 100 / nb_total;
  else
    percent_change = 0;

  return 0;
}

//-------------------------------------------------------------------------------------
#end unsafe
//-------------------------------------------------------------------------------------
