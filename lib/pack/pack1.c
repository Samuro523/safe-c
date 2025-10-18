
/* pack1.c : compute frequencies, select method, call huffman routine */

use ../zip, pack0, pack2;

/***********************************************************************/

#define debug_alpha       0    /* printf alpha codes */
#define debug_show_sizes  0    /* printf bit sizes required by each method */
#define debug_rle         0    /* printf RLE */

/***********************************************************************/

struct _length_table
{
  short first_length;
  short last_length;
  short first_code;
  short extra_bits;    /* per code */
  short extra_mask;
}

const _length_table length_table [7] =
{{  3,  10, 257, 0,  0},
 { 11,  18, 265, 1,  1},
 { 19,  34, 269, 2,  3},
 { 35,  66, 273, 3,  7},
 { 67, 130, 277, 4, 15},
 {131, 257, 281, 5, 31},
 {258, 258, 285, 0,  0}};

/***********************************************************************/

struct _distance_table
{
  int   first_dist;
  int   last_dist;
  short first_code;
  short extra_bits;    /* per code */
  short extra_mask;
}

const _distance_table distance_table [14] =
{{    1,     4,  0,  0,    0},
 {    5,     8,  4,  1,    1},
 {    9,    16,  6,  2,    3},
 {   17,    32,  8,  3,    7},
 {   33,    64, 10,  4,   15},
 {   65,   128, 12,  5,   31},
 {  129,   256, 14,  6,   63},
 {  257,   512, 16,  7,  127},
 {  513,  1024, 18,  8,  255},
 { 1025,  2048, 20,  9,  511},
 { 2049,  4096, 22, 10, 1023},
 { 4097,  8192, 24, 11, 2047},
 { 8193, 16384, 26, 12, 4095},
 {16385, 32768, 28, 13, 8191}};

/***********************************************************************/

const short MAX_NODES_A = 286;   /* literal and lengths */
const short MAX_NODES_B =  30;   /* distance */
const short MAX_NODES_C =  19;   /* code_len */

struct RLE
{
  byte  code;
  short repeat;  /* used for codes 16, 17 and 18 */
}

const short alpha_tab[MAX_NODES_C] =
  {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/* methods: */
enum METHOD
{
  METHOD_UNCOMPRESSED, // 0
  METHOD_FIX_HUFF,     // 1
  METHOD_DYN_HUFF,     // 2
};

/***********************************************************************/

package body OUTPUT_BLOCK

public int output_block
        (CODE[]          output,     /* literal/refer.*/
         byte[]          data,       /* data in clear */
         ref OUTPUT_INFO rout,
         ref USER_INFO   user)
{
  NODE node_a[2 * MAX_NODES_A];   /* info for literals and lengths */
                                  /* + place for intermediate huffman nodes */
  NODE node_b[2 * MAX_NODES_B];   /* info for distances */
                                  /* + place for intermediate huffman nodes */

  NODE node_c[2 * MAX_NODES_C];   /* info for code_lengths */
                                  /* + place for intermediate huffman nodes */

  short  count_a, count_b;
  byte   code_length[MAX_NODES_A + MAX_NODES_B];
  short  count_code_length;

  RLE    rle[MAX_NODES_A + MAX_NODES_B];
  short  count_rle;

  short  code_alpha[MAX_NODES_C];
  short  count_code_alpha;

  int    nb_bits_dyn_huff, nb_bits_fixed_huff, nb_bits_uncompressed;
  METHOD method;

  int    rc;


  /* fill node tables A and B with frequencies of literals, */
  /* lengths and distances.                                 */

  {
    int i, j, len, code, dist;

    clear node_a, node_b;

    node_a[256].count = 1;     /* code 'end-of-block' : set frequency = 1 */

    for (i=0; i<output'length; i++)    /* scan PASS 1 result data */
    {
      if (output[i].value >= 0)      /* a literal */
        node_a[output[i].value].count++;
      else                           /* a reference (distance,length) */
      {
        /* first, compute code for length */

        len = (int)output[i].length + 3;
        assert (len <= 258);

        j = 0;
        while (len > length_table[j].last_length)
          j++;

        code = length_table[j].first_code
               + ((len - length_table[j].first_length) >> length_table[j].extra_bits);

        node_a[code].count++;


        /* now, compute code for distance */

        dist = -output[i].value;
        assert (dist <= 32768);

        j = 0;
        while (dist > distance_table[j].last_dist)
          j++;

        code = distance_table[j].first_code
               + ((dist - distance_table[j].first_dist) >> distance_table[j].extra_bits);

        node_b[code].count++;
      }
    }
  }


  rc = compute_huffman (ref node_a, MAX_NODES_A, /*max_code_bits=*/ 15);
  if (rc < 0)
    return rc;

  rc = compute_huffman (ref node_b, MAX_NODES_B, /*max_code_bits=*/ 15);
  if (rc < 0)
    return rc;


  /* build table 'code_length' with the catenated code lengths */
  /* of node_a[] and node_b[].                                 */

  {
    int i;

    count_a = MAX_NODES_A;
    while (count_a > 257 && node_a[count_a-1].code_length == 0)
      count_a--;

    count_b = MAX_NODES_B;
    while (count_b > 1 && node_b[count_b-1].code_length == 0)
      count_b--;

    count_code_length = 0;

    clear code_length;

    for (i=0; i<count_a; i++)
      code_length[count_code_length++] = (byte)node_a[i].code_length;

    for (i=0; i<count_b; i++)
      code_length[count_code_length++] = (byte)node_b[i].code_length;
  }


  /* encode code_length[] into rle[] using RLE encoding */

  {
    int i, repeat, len, code;

    count_rle = 0;

    clear rle;

    for (i=0; i<count_code_length; /* nothing */)
    {
      /* compute 'code' and 'repeat' */

      code = code_length[i];
      repeat = 0;
      while (i<count_code_length && (int)code_length[i] == code)
      {
        i++;
        repeat++;
      }


      /* send code 'code', 'repeat' times */

      if (code == 0)       /* special codes 18 and 17 for code 0 */
      {
        while (repeat >= 11)
        {
          len = repeat;
          if (len > 138)
            len = 138;

          rle[count_rle].code   = 18;
          rle[count_rle].repeat = (short)len;

          count_rle++;
          repeat -= len;
        }

        if (repeat >= 3)
        {
          rle[count_rle].code   = 17;
          rle[count_rle].repeat = (short)repeat;

          count_rle++;
          repeat -= repeat;
        }
      }

      if (repeat > 0)         /* send a single code */
      {
        rle[count_rle++].code = (byte)code;
        repeat--;
      }

      while (repeat >= 3)    /* send repeat code 16 (repeat 3 .. 6 times) */
      {
        len = repeat;
        if (len > 6)
          len = 6;

        rle[count_rle].code   = 16;
        rle[count_rle].repeat = (short)len;

        count_rle++;
        repeat -= len;
      }

      while (repeat > 0)        /* send remaining single codes */
      {
        rle[count_rle++].code = (byte)code;
        repeat--;
      }
    }
  }


#if debug_rle
  {
    int i;
    printf ("RLE TABLE\n");
    for (i=0; i<count_rle; i++)
    {
      if (rle[i].code < 16)
        printf ("code %u\n", rle[i].code);
      else
        printf ("code %u (repeat %d)\n", rle[i].code, rle[i].repeat);
    }
    printf ("\n");
  }
#endif


  /* compute node_c : frequencies of code_lengths */

  {
    int i;

    clear node_c;

    for (i=0; i<count_rle; i++)
      node_c[rle[i].code].count++;
  }


  compute_huffman (ref node_c, MAX_NODES_C, /*max_code_bits=*/ 7);


  /* compute code_alpha[] : code lengths of node_c in order alpha_tab[] */

  {
    int i;

    clear code_alpha;

    for (i=0; i<MAX_NODES_C; i++)
      code_alpha[i] = node_c[alpha_tab[i]].code_length;

    count_code_alpha = MAX_NODES_C;

    /* cut trailing zero code lengths */
    while (count_code_alpha > 4 && code_alpha[count_code_alpha-1] == 0)
      count_code_alpha--;
  }

#if debug_alpha
  {
    int i;
    printf ("code alpha :");
    for (i=0; i<count_code_alpha; i++)
      printf ("%u ", code_alpha[i]);
    printf ("\n\n");
  }
#endif


  /* compute nb bits required to store data block using dynamic huffman */

  {
    int i, extra;

    nb_bits_dyn_huff =  3                      /* block header */
                     + 14                      /* header of huff codes */
                     +  3 * count_code_alpha;  /* alpha codes */

    /* add rle[] using node_c[] frequency */
    for (i=0; i<MAX_NODES_C; i++)
    {
      if (i <= 15)
        extra = 0;
      else if (i == 16)
        extra = 2;
      else if (i == 17)
        extra = 3;
      else if (i == 18)
        extra = 7;
      else
        abort;

      nb_bits_dyn_huff += node_c[i].count
                          * ((int)node_c[i].code_length + extra);
    }


    /* add actual data using node_a[] and node_b[] frequency */

    for (i=0; i<MAX_NODES_A; i++)
    {
      if (i <= 264 || i == 285)   /* literal, end-of-block or length */
        extra = 0;                /* with no extra bits              */
      else
        extra = (i - 261) / 4;    /* length with extra bits */

      nb_bits_dyn_huff += node_a[i].count *
                          ((int)node_a[i].code_length + extra);
    }

    for (i=0; i<MAX_NODES_B; i++)
    {
      if (i <= 3)              /* distance with no extra bits */
        extra = 0;
      else
        extra = (i - 2) / 2;   /* distance with extra bits */

      nb_bits_dyn_huff += node_b[i].count *
                          ((int)node_b[i].code_length + extra);
    }
  }



  /* compute nb bits required to store block using fixed huffman */

  {
    int i, len, extra;

    nb_bits_fixed_huff = 3;     /* block header */


    /* add actual data using node_a[] and node_b[] frequency */

    for (i=0; i<MAX_NODES_A; i++)
    {
      if (i <= 143)
        len = 8;
      else if (i <= 255)
        len = 9;
      else if (i <= 279)
        len = 7;
      else if (i <= 285)
        len = 8;
      else
        abort;

      if (i <= 264 || i == 285)   /* literal, end-of-block or length */
        extra = 0;                /* with no extra bits              */
      else
        extra = (i - 261) / 4;    /* length with extra bits */

      nb_bits_fixed_huff += node_a[i].count * ((int)len + extra);
    }

    for (i=0; i<MAX_NODES_B; i++)
    {
      if (i <= 3)              /* distance with no extra bits */
        extra = 0;
      else
        extra = (i - 2) / 2;   /* distance with extra bits */

      nb_bits_fixed_huff += node_b[i].count * (5 + extra);
    }
  }



  /* compute nb bits required to store block uncompressed */

  nb_bits_uncompressed = 3 + 4 * 8 + data'length * 8;



#if debug_show_sizes
  /* display alternatives */

  printf ("uncompressed size = %7d bits (%7d bytes)\n",
          nb_bits_uncompressed, (nb_bits_uncompressed + 7) / 8);

  printf ("fix huff size     = %7d bits (%7d bytes)\n",
          nb_bits_fixed_huff, (nb_bits_fixed_huff + 7) / 8);

  printf ("dyn huff size     = %7d bits (%7d bytes)\n",
          nb_bits_dyn_huff, (nb_bits_dyn_huff + 7) / 8);
#endif


  /* select method generating the minimum bits */

  {
    int minimum;

    method = METHOD_DYN_HUFF;
    minimum = nb_bits_dyn_huff;

    if (nb_bits_fixed_huff <= minimum)
    {
      method = METHOD_FIX_HUFF;
      minimum = nb_bits_fixed_huff;
    }

    if (nb_bits_uncompressed <= minimum)
    {
      method = METHOD_UNCOMPRESSED;
      minimum = nb_bits_uncompressed;
    }
  }



  /* output BTYPE */
  rc = send_code (ref rout, (int)method, 2, LSB_FIRST, ref user);
  if (rc < 0)
    return rc;


  /* output the compressed block */

  switch (method)
  {
    case METHOD_DYN_HUFF:     /* dynamic huffman */

      /* output HLIT (5 bits) / HDIST (5 bits) / HCLEN (4 bits) */
      rc = send_code (ref rout, count_a-257, 5, LSB_FIRST, ref user);
      if (rc < 0)
        return rc;

      rc = send_code (ref rout, count_b-1, 5, LSB_FIRST, ref user);
      if (rc < 0)
        return rc;

      rc = send_code (ref rout, count_code_alpha-4, 4, LSB_FIRST, ref user);
      if (rc < 0)
        return rc;

      /* output alpha table */
      {
        int i;
        for (i=0; i<count_code_alpha; i++)
        {
          rc = send_code (ref rout, code_alpha[i], 3, LSB_FIRST, ref user);
          if (rc < 0)
            return rc;
        }
      }

      /* output RLE table */
      {
        int i, code;
        for (i=0; i<count_rle; i++)
        {
          code = rle[i].code;
          rc = send_code (ref rout,
                          node_c[code].code_value,
                          node_c[code].code_length,
                          MSB_FIRST, ref user);
          if (rc < 0)
            return rc;

          /* send extra bits, if necessary */
          if (code == 16)
          {
            rc = send_code (ref rout, rle[i].repeat - 3, 2, LSB_FIRST, ref user);
            if (rc < 0)
              return rc;
          }
          else if (code == 17)
          {
            rc = send_code (ref rout, rle[i].repeat - 3, 3, LSB_FIRST, ref user);
            if (rc < 0)
              return rc;
          }
          else if (code == 18)
          {
            rc = send_code (ref rout, rle[i].repeat - 11, 7, LSB_FIRST, ref user);
            if (rc < 0)
              return rc;
          }
        }
      }

      /* output the actual data */

      {
        int i, j, code, len, extra_length, extra_value, dist;
        for (i=0; i<output'length; i++)
        {
          if (output[i].value >= 0)      /* a literal */
          {
            code = output[i].value;
            rc = send_code (ref rout,
                            node_a[code].code_value,
                            node_a[code].code_length,
                            MSB_FIRST, ref user);
            if (rc < 0)
              return rc;
          }
          else                      /* a reference (distance,length) */
          {
            /* send the length first */

            len = (int)output[i].length + 3;
            assert (len <= 258);

            j = 0;
            while (len > length_table[j].last_length)
              j++;

            len -= length_table[j].first_length;
            code = length_table[j].first_code + (len >> length_table[j].extra_bits);
            extra_length = length_table[j].extra_bits;
            extra_value = len & length_table[j].extra_mask;

            rc = send_code (ref rout,
                            node_a[code].code_value,
                            node_a[code].code_length,
                            MSB_FIRST, ref user);
            if (rc < 0)
              return rc;

            if (extra_length != 0)
            {
              rc = send_code (ref rout, extra_value, extra_length, LSB_FIRST, ref user);
              if (rc < 0)
                return rc;
            }


            /* now, send the distance */

            dist = -output[i].value;
            assert (dist <= 32768);

            j = 0;
            while (dist > distance_table[j].last_dist)
              j++;

            dist -= distance_table[j].first_dist;
            code = distance_table[j].first_code
                   + (dist >> distance_table[j].extra_bits);
            extra_length = distance_table[j].extra_bits;
            extra_value = dist & distance_table[j].extra_mask;

            rc = send_code (ref rout,
                            node_b[code].code_value,
                            node_b[code].code_length,
                            MSB_FIRST, ref user);
            if (rc < 0)
              return rc;

            if (extra_length != 0)
            {
              rc = send_code (ref rout, extra_value, extra_length, LSB_FIRST, ref user);
              if (rc < 0)
                return rc;
            }
          }
        }
      }

      /* output code 256 (end of block) */

      rc = send_code (ref rout,
                      node_a[256].code_value,
                      node_a[256].code_length,
                      MSB_FIRST, ref user);
      if (rc < 0)
        return rc;

      break;


    case METHOD_FIX_HUFF:

      /* setup fixed huffman codes in node_a[] and node_b[] */

      {
        int i;

        for (i=0; i<=143; i++)
        {
          node_a[i].code_length = 8;
          node_a[i].code_value  = (short)(0x30 + (i - 0));
        }

        for (i=144; i<=255; i++)
        {
          node_a[i].code_length = 9;
          node_a[i].code_value  = (short)(0x190 + (i - 144));
        }

        for (i=256; i<=279; i++)
        {
          node_a[i].code_length = 7;
          node_a[i].code_value  = (short)(0x00 + (i - 256));
        }

        for (i=280; i<MAX_NODES_A; i++)
        {
          node_a[i].code_length = 8;
          node_a[i].code_value  = (short)(0xC0 + (i - 280));
        }

        for (i=0; i<MAX_NODES_B; i++)
        {
          node_b[i].code_length = 5;
          node_b[i].code_value  = (short)i;
        }
      }


      /* output the actual data */

      {
        int i, j, code, len, extra_length, extra_value, dist;
        for (i=0; i<output'length; i++)
        {
          if (output[i].value >= 0)      /* a literal */
          {
            code = output[i].value;
            rc = send_code (ref rout,
                            node_a[code].code_value,
                            node_a[code].code_length,
                            MSB_FIRST, ref user);
            if (rc < 0)
              return rc;
          }
          else                      /* a reference (distance,length) */
          {
            /* send the length first */

            len = (int)output[i].length + 3;
            assert (len <= 258);

            j = 0;
            while (len > length_table[j].last_length)
              j++;

            len -= length_table[j].first_length;
            code = length_table[j].first_code + (len >> length_table[j].extra_bits);
            extra_length = length_table[j].extra_bits;
            extra_value = len & length_table[j].extra_mask;

            rc = send_code (ref rout,
                            node_a[code].code_value,
                            node_a[code].code_length,
                            MSB_FIRST, ref user);
            if (rc < 0)
              return rc;

            if (extra_length != 0)
            {
              rc = send_code (ref rout, extra_value, extra_length, LSB_FIRST, ref user);
              if (rc < 0)
                return rc;
            }


            /* now, send the distance */

            dist = -output[i].value;
            assert (dist <= 32768);

            j = 0;
            while (dist > distance_table[j].last_dist)
              j++;

            dist -= distance_table[j].first_dist;
            code = distance_table[j].first_code + (dist >> distance_table[j].extra_bits);
            extra_length = distance_table[j].extra_bits;
            extra_value = dist & distance_table[j].extra_mask;

            rc = send_code (ref rout,
                            node_b[code].code_value,
                            node_b[code].code_length,
                            MSB_FIRST, ref user);
            if (rc < 0)
              return rc;

            if (extra_length != 0)
            {
              rc = send_code (ref rout, extra_value, extra_length, LSB_FIRST, ref user);
              if (rc < 0)
                return rc;
            }
          }
        }
      }

      /* output code 256 (end of block) */

      rc = send_code (ref rout,
                      node_a[256].code_value,
                      node_a[256].code_length,
                      MSB_FIRST, ref user);
      if (rc < 0)
        return rc;

      break;


    case METHOD_UNCOMPRESSED:

      /* output LEN / NLEN */
      {
        ushort len, nlen;
        byte   buf[4];

        len = (ushort)data'length;
        nlen = (ushort)(~len);

        buf = {(byte)(len & 255),
               (byte)(len >> 8),
               (byte)(nlen & 255),
               (byte)(nlen >> 8)};

        rc = send_block (ref rout, buf, ref user);
        if (rc < 0)
          return rc;
      }

      /* output uncompressed data */
      rc = send_block (ref rout, data[0:data'length], ref user);
      if (rc < 0)
        return rc;

      break;

    default:
      return PK_INTERN_1;
  }

  return 0;
}

end OUTPUT_BLOCK;

/***********************************************************************/
