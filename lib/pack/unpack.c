
/* unpack0.c */

use ../zip, unpack1;

#define debug  0

#if debug
  use ../console;
#endif

/**************************************************************************/

enum METHOD {
  METHOD_UNCOMPRESSED,   // 0
  METHOD_FIX_HUFF,       // 1
  METHOD_DYN_HUFF,       // 2
};

const int MAX_NODES_A  = 286;   // literal and lengths
const int MAX_NODES_B  =  32;   // distance (deflate_64 : we added 2 here)
const int MAX_NODES_C  =  19;   // code_len

struct _length_info
{
  short extra_bits;
  short min_length;
}

const _length_info length_info [MAX_NODES_A - 257] =
{{0,   3},    /* 4 */
 {0,   4},
 {0,   5},
 {0,   6},
 {0,   7},    /* 4 */
 {0,   8},
 {0,   9},
 {0,  10},
 {1,  11},    /* 8 */
 {1,  13},
 {1,  15},
 {1,  17},
 {2,  19},    /* 16 */
 {2,  23},
 {2,  27},
 {2,  31},
 {3,  35},    /* 32 */
 {3,  43},
 {3,  51},
 {3,  59},
 {4,  67},    /* 64 */
 {4,  83},
 {4,  99},
 {4, 115},
 {5, 131},    /* 127 !! */
 {5, 163},
 {5, 195},
 {5, 227},
 {0, 258},   /* 1 */   // has different semantic for deflate_64
};


struct _distance_info
{
  short extra_bits;
  int   min_distance;
}

const _distance_info distance_info [MAX_NODES_B] =
{{ 0,     1 },
 { 0,     2 },
 { 0,     3 },
 { 0,     4 },
 { 1,     5 },
 { 1,     7 },
 { 2,     9 },
 { 2,    13 },
 { 3,    17 },
 { 3,    25 },
 { 4,    33 },
 { 4,    49 },
 { 5,    65 },
 { 5,    97 },
 { 6,   129 },
 { 6,   193 },
 { 7,   257 },
 { 7,   385 },
 { 8,   513 },
 { 8,   769 },
 { 9,  1025 },
 { 9,  1537 },
 {10,  2049 },
 {10,  3073 },
 {11,  4097 },
 {11,  6145 },
 {12,  8193 },
 {12, 12289 },
 {13, 16385 },
 {13, 24577 },
 {14, 32769 },  // for deflate_64
 {14, 49153 },  // for deflate_64
};

const short alpha_tab[MAX_NODES_C] =
  {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/**************************************************************************/

package body GEN_UNPACK

  package INPUT

    /**************************************************************************/

    struct INPUT_INFO;

    /**************************************************************************/

    void init_input (out INPUT_INFO info,
                         IO_READ    user_read);

    /**************************************************************************/

    /* returns a fixed-length code in LSB order, or a negative error. */

    int receive_code (ref INPUT_INFO info, int nb_bits, ref USER_INFO user_info);

    /**************************************************************************/

    /* skip remaining bits of current byte and receive 'length' bytes */

    int receive_block (ref INPUT_INFO  info,
                       out byte[]      buf,
                       ref USER_INFO   user_info);

    /**************************************************************************/

    /* analyzes a varying-length bit sequence in MSB order.    */
    /* returns 0 and the corresponding huffman code in 'code'  */
    /* or an error code if it does not exist.                  */

    int receive_huffman_code (ref INPUT_INFO info,
                                  NODE[]     node,       /* Huffman tree  */
                                  int        max_nodes,  /* nb leaf nodes */
                              out int        code,       /* result code   */
                              ref USER_INFO  user_info);

    /**************************************************************************/

    void copy_to_read_ahead (ref INPUT_INFO info,
                             out READ_AHEAD extra);

    /**************************************************************************/

  end INPUT;


  package body INPUT    // input bit stream

    /**************************************************************************/

    const int INPUT_BUFFER_SIZE = 32768;  /* must match with UNPACK_MAX_READ_AHEAD ! */

    struct INPUT_INFO
    {
      byte    buffer[INPUT_BUFFER_SIZE];
      int     first;   /* index of byte currently being read bit per bit */
      int     count;   /* nb of bytes present in buffer[] */
      uint    power;   /* power of 2 that will be extracted from   */
                       /* buffer[first] next time. when power == 1 */
                       /* was extracted, 'first' is incremented.   */
      IO_READ user_read;  /* function used to read a buffer */
    }

    /**************************************************************************/

    public void init_input (out INPUT_INFO info,
                                IO_READ    user_read)
    {
      clear info;
      info.user_read = user_read;
    }

    /**************************************************************************/

    /* returns a fixed-length code in LSB order, or a negative error. */

    public int receive_code (ref INPUT_INFO info, int nb_bits, ref USER_INFO user_info)
    {
      int rc, code, power, bits;

      assert (nb_bits >= 1 && nb_bits <= 31);

      code = 0;
      power = 1;      // term to add to 'code'
      bits = nb_bits;

      while (bits-- > 0)
      {
        if (info.first == info.count)    // we need to read a new block
        {
          rc = info.user_read (ref user_info, out info.buffer);
          if (rc <= 0)
            return UNPK_READ_ERROR;    // we need to read at least 1 byte

          info.first = 0;
          info.count = rc;
          info.power = 1;
        }

        if ((info.buffer[info.first] & info.power) != 0)
          code |= power;

        info.power <<= 1;
        if (info.power > 128)
        {
          info.power = 1;
          info.first++;
        }

        power <<= 1;
      }

      return code;
    }

    /**************************************************************************/

    public int receive_block (ref INPUT_INFO info,
                              out byte[]     buf,
                              ref USER_INFO  user_info)
    {
      int i, rc;

      clear buf;

      /* skip all remaining bits of the current byte */
      if (info.power > 1)
      {
        info.power = 1;
        info.first++;
      }

      /* read 'length' bytes */
      for (i=0; i<buf'length; i++)
      {
        if (info.first == info.count)    /* we need to read a new block */
        {
          rc = info.user_read (ref user_info, out info.buffer);
          if (rc <= 0)
            return UNPK_READ_ERROR;    /* we need to read at least 1 byte */

          info.first = 0;
          info.count = rc;
          info.power = 1;
        }

        buf[i] = info.buffer[info.first++];
      }

      return 0;
    }

    /**************************************************************************/

    /* analyzes a varying-length bit sequence in MSB order.    */
    /* returns 0 and the corresponding huffman code in 'code'  */
    /* or an error code if it does not exist.                  */

    public int receive_huffman_code (ref INPUT_INFO info,
                                         NODE[]     node,       /* Huffman tree  */
                                         int        max_nodes,  /* nb leaf nodes */
                                     out int        code,       /* result code   */
                                     ref USER_INFO  user_info)
    {
      int  n, rc;
      bool bit;

      clear code;

      n = 2 * max_nodes - 1;     /* root node */

      for (;;)
      {
        if (info.first == info.count)    /* we need to read a new block */
        {
          rc = info.user_read (ref user_info, out info.buffer);
          if (rc <= 0)
            return UNPK_READ_ERROR;    /* we need to read at least 1 byte */

          info.first = 0;
          info.count = rc;
          info.power = 1;
        }

        bit = ((info.buffer[info.first] & info.power) != 0);

        info.power <<= 1;
        if (info.power > 128)
        {
          info.power = 1;
          info.first++;
        }

        if (bit)
          n = node[n].index_one;
        else
          n = node[n].index_zero;

        if (n == -1)
          return UNPK_HUFF_BAD_CODE;

        if (n < max_nodes)
        {
          code = n;
          return 0;
        }
      }
    }

    /**************************************************************************/

    public void copy_to_read_ahead (ref INPUT_INFO info, out READ_AHEAD extra)
    {
      assert (INPUT_BUFFER_SIZE <= UNPACK_MAX_READ_AHEAD);

      /* skip all remaining bits of the current byte */
      if (info.power > 1)
      {
        info.power = 1;
        info.first++;
      }

      clear extra;
      extra.size = (uint)(info.count - info.first);
      extra.buffer[0:extra.size] =  info.buffer[info.first : extra.size];
    }

    /**************************************************************************/

  end INPUT;

  package OUTPUT

    /**************************************************************************/

    struct OUTPUT_INFO;

    /**************************************************************************/

    void init_output (out OUTPUT_INFO info,
                          IO_WRITE    user_write);

    /**************************************************************************/

    int send_block (ref OUTPUT_INFO info,
                        byte[]      data,
                    ref USER_INFO   user_info);

    /**************************************************************************/

    /* send all remaining data to output device */

    int flush_output (ref OUTPUT_INFO info,
                      ref USER_INFO   user_info);

    /**************************************************************************/

  end OUTPUT;


  package body OUTPUT    // output byte stream

    /**************************************************************************/

    const int OUTPUT_BUFFER_SIZE = 32768;

    struct OUTPUT_INFO
    {
      byte      buffer[OUTPUT_BUFFER_SIZE];
      int       count;        /* nb of bytes in buffer[] */
      IO_WRITE  user_write;   /* function used to write a buffer */
    }

    /**************************************************************************/

    public void init_output (out OUTPUT_INFO info,
                                 IO_WRITE    user_write)
    {
      clear info;
      info.user_write = user_write;
    }

    /**************************************************************************/

    /* send all remaining data to output device */

    public int flush_output (ref OUTPUT_INFO info,
                             ref USER_INFO   user_info)
    {
      int rc;

      rc = info.user_write (ref user_info, info.buffer[0:info.count]);
      if (rc != info.count)
        return UNPK_WRITE_ERROR;

      info.count = 0;

      return 0;
    }

    /**************************************************************************/

    public int send_block (ref OUTPUT_INFO info,
                               byte[]      data,
                           ref USER_INFO   user_info)
    {
      int i;
      int rc;

      for (i=0; i<data'length; i++)
      {
        if (info.count == OUTPUT_BUFFER_SIZE)
        {
          rc = flush_output (ref info, ref user_info);
          if (rc < 0)
            return rc;
        }

        info.buffer[info.count++] = data[i];
      }

      return 0;
    }

    /**************************************************************************/

  end OUTPUT;

/**************************************************************************/

public int unpack (ref USER_INFO  user_info,
                       IO_READ    user_read,
                       IO_WRITE   user_write,
                   out READ_AHEAD extra,
                       bool       deflate_64 = false)  // false=deflate, true=deflate_64 algorithm
{
  INPUT_INFO    in;
  OUTPUT_INFO   rout;
  int           final, method, rc;
  byte          output[65536];         // 32K for deflate, 64K for deflate_64
  int           output_size      = deflate_64 ? 64*1024 : 32*1024;  // the maximum backward distance of a reference
  int           output_size_mask = output_size - 1;
  int           output_i = 0;

  clear extra;

  /* init input/output modules */
  init_input (out in, user_read);
  init_output (out rout, user_write);

  clear output;

  for (;;)
  {
    /* is it the final block ? */
    final = receive_code (ref in, 1, ref user_info);
    if (final < 0)
      return final;

#if debug
    printf ("BFINAL : %d\n", final);
#endif

    /* get method */
    method = receive_code (ref in, 2, ref user_info);
    if (method < 0)
      return method;

#if debug
    printf ("BTYPE : %s\n", ((METHOD)method)'string);
#endif

    switch ((METHOD)method)
    {
      case METHOD_UNCOMPRESSED:
        {
          byte   len[4];
          ushort length, nlength, cmp;


          /* receive LEN/NLEN */
          rc = receive_block (ref in, out len, ref user_info);
          if (rc < 0)
            return rc;

          length  = (ushort)(len[0] + (len[1] << 8));        // 0 .. 65535
          nlength = (ushort)(len[2] + (len[3] << 8));

#if debug
    printf ("LENGTH : 0x%04x (NLEN=0x%04x)\n", length, nlength);
#endif

          /* check validity of NLEN */
          cmp = (ushort)(~nlength);    /* avoid converting to int ! */
          if (length != cmp)
            return UNPK_BAD_NLEN;


          {
            byte buf[8192];   // temp buffer must not exceed 32K, because we copy in max 2 operations below
            uint chunk;

            clear buf;

            while (length > 0)
            {
              chunk = buf'size;
              if (length < chunk)
                chunk = length;

              rc = receive_block (ref in, out buf[0:chunk], ref user_info);
              if (rc < 0)
                return rc;

              rc = send_block (ref rout, buf[0:chunk], ref user_info);
              if (rc < 0)
                return rc;

              {
                uint part1;

                part1 = (uint)(output_size - output_i);   // space left til end of output
                if (part1 > chunk)  // size to first part to copy
                  part1 = chunk;

                output[output_i : part1] = buf[0 : part1];        // copy possibly til the end of the output
                output_i = (output_i + (int)part1) & output_size_mask;

                if (chunk > part1)  // there is more to copy to the beginning of the output
                {
                  uint part2;

                  part2 = chunk - part1;
                  assert part2 <= (uint)output_size;
                  assert output_i == 0;

                  output[0 : part2] = buf[part1 : part2];
                  output_i = (int)part2 & output_size_mask;
                }
              }

              length -= (ushort)chunk;
            }
          }
        }
        break;


      case METHOD_FIX_HUFF:
      case METHOD_DYN_HUFF:
        {
          NODE node_a[2 * MAX_NODES_A];  /* info for literals and lengths */
                                         /* + place for intermediate huffman nodes */
          NODE node_b[2 * MAX_NODES_B];  /* info for distances */
                                         /* + place for intermediate huffman nodes */

          if (method == (int)METHOD_DYN_HUFF)
          {
            int   count_a, count_b, hclen, i;

            short code_alpha[MAX_NODES_C];    /* 'hclen' values */

            NODE  node_c[2 * MAX_NODES_C];  /* info for code_lengths */
                                            /* + place for intermediate huffman nodes */

            byte   code_length[MAX_NODES_A + MAX_NODES_B];
            short  count_code_length, max_count_code_length;


            /* input HLIT (5 bits) / HDIST (5 bits) / HCLEN (4 bits) */

            count_a = receive_code (ref in, 5, ref user_info);
            if (count_a < 0)
              return count_a;

            count_b = receive_code (ref in, 5, ref user_info);
            if (count_b < 0)
              return count_b;

            hclen = receive_code (ref in, 4, ref user_info);
            if (hclen < 0)
              return hclen;

            count_a += 257;
            count_b += 1;
            hclen   += 4;

#if debug
  printf ("HLIT=%d HDIST=%d HCLEN=%d\n", count_a, count_b, hclen);
#endif

            /* read-in alpha table */

#if debug
  printf ("alpha table : ");
#endif
            /* make sure unused codes are set to zero */
            clear code_alpha;

            for (i=0; i<hclen; i++)
            {
              rc = receive_code (ref in, 3, ref user_info);
              if (rc < 0)
                return rc;
              code_alpha[i] = (short)rc;
#if debug
  printf ("%d ", rc);
#endif
            }

#if debug
  printf ("\n");
#endif

            /* convert back to node_c[] using alpha_tab[] */

            clear node_c;
            for (i=0; i<MAX_NODES_C; i++)
              node_c[alpha_tab[i]].code_length = code_alpha[i];


            // from 'code_length', compute 'code_value'
            rc = compute_huffman_code_values (ref node_c, MAX_NODES_C);
            if (rc < 0)
              return rc;

            // build huffman tree
            rc = build_huffman_tree (ref node_c, MAX_NODES_C);
            if (rc < 0)
              return rc;



#if debug
  printf ("Read-in code_lengths for node_a[] and node_b[]\n");
#endif
            /* read-in the code lengths for node_a[] and node_b[] */

            count_code_length = 0;
            clear code_length;

            max_count_code_length = (short)(count_a + count_b);

            {
              int previous_code, code, code_len, repeat;

              previous_code = -1;

              while (count_code_length < max_count_code_length)
              {
                /* read-in a code length */
                rc = receive_huffman_code (ref in, node_c, MAX_NODES_C, out code, ref user_info);
                if (rc < 0)
                  return rc;

                assert (code >= 0 && code <= 18);

                if (code <= 15)
                {
                  code_len = code;
                  repeat = 1;
                }
                else if (code == 16)
                {
                  rc = receive_code (ref in, 2, ref user_info);
                  if (rc < 0)
                    return rc;

                  code_len = previous_code;
                  repeat = 3 + rc;
                }
                else if (code == 17)
                {
                  rc = receive_code (ref in, 3, ref user_info);
                  if (rc < 0)
                    return rc;

                  code_len = 0;
                  repeat = 3 + rc;
                }
                else if (code == 18)
                {
                  rc = receive_code (ref in, 7, ref user_info);
                  if (rc < 0)
                    return rc;

                  code_len = 0;
                  repeat = 11 + rc;
                }
                else
                {
                  abort;
                }

#if debug
  if (code < 16)
    printf ("RLE CODE %d\n", code);
  else
    printf ("RLE CODE %d (repeat = %d)\n", code, repeat);
#endif

                if (code_len == -1)
                  return UNPK_NO_PREV_CODE;

                previous_code = code_len;

                if (count_code_length + repeat > max_count_code_length)
                  return UNPK_TOO_MANY;

                while (repeat-- > 0)
                  code_length [count_code_length++] = (byte)code_len;
              }
            }


            /* copy the code lengths into node_a[] and node_b[] */

            {
              clear node_a;
              clear node_b;

              for (i=0; i<count_a; i++)
                node_a[i].code_length = code_length[i];

              for (i=0; i<count_b; i++)
                node_b[i].code_length = code_length[count_a + i];
            }


            /* build huffman trees */

            rc = compute_huffman_code_values (ref node_a, MAX_NODES_A);
            if (rc < 0)
              return rc;
            rc = build_huffman_tree (ref node_a, MAX_NODES_A);
            if (rc < 0)
              return rc;

            rc = compute_huffman_code_values (ref node_b, MAX_NODES_B);
            if (rc < 0)
              return rc;
            rc = build_huffman_tree (ref node_b, MAX_NODES_B);
            if (rc < 0)
              return rc;
          }
          else     /* method == METHOD_FIX_HUFF */
          {
            int i;

            clear node_a, node_b;

/*
                   Lit Value    Bits        Codes
                   ---------    ----        -----
                     0 - 143     8          00110000 through
                                            10111111
                   144 - 255     9          110010000 through
                                            111111111
                   256 - 279     7          0000000 through
                                            0010111
                   280 - 287     8          11000000 through
                                            11000111
*/

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

            for (i=280; i<MAX_NODES_A; i++)   // MAX_NODES_A = 286 (we don't use 286 and 287)
            {
              node_a[i].code_length = 8;
              node_a[i].code_value  = (short)(0xC0 + (i - 280));
            }

            rc = build_huffman_tree (ref node_a, MAX_NODES_A);
            if (rc < 0)
              return rc;

            for (i=0; i<MAX_NODES_B; i++)
            {
              node_b[i].code_length = 5;
              node_b[i].code_value  = (short)i;
            }

            rc = build_huffman_tree (ref node_b, MAX_NODES_B);
            if (rc < 0)
              return rc;
          }


          /* receive the actual data */

          for (;;)
          {
            int code, extra_bits, length, distance;

            rc = receive_huffman_code (ref in, node_a, MAX_NODES_A, out code, ref user_info);
            if (rc < 0)
              return rc;

            if (code == 256)     // end of block
              break;

            if (code <= 255)     // a literal
            {
#if debug
              printf ("literal : %d (%c)\n", code, code >= 32 ? (char)code : '.');
#endif
              {
                byte ch;

                ch = (byte)code;

                output[output_i] = ch;
                output_i = (output_i + 1) & output_size_mask;

                rc = send_block (ref rout, ch, ref user_info);
                if (rc < 0)
                  return rc;
              }
            }
            else                 /* a length */
            {
              /* read the length code suffix (if any) */

              assert (code >= 257 && code < MAX_NODES_A);  // 257 .. 285

              length     = length_info[code-257].min_length;  // 3 .. 258
              extra_bits = length_info[code-257].extra_bits;  // 0 .. 5

              if (deflate_64 && code == 285)
              {
                // last length code (285) gets a different meaning : instead of coding a fixed maximum length of 258,
                // it is used as a generic length code capable of coding any length from 3 to 65538.
                // Length code 285 thus takes 16 bit of uncoded extra data, added to a fixed min length of 3.
                length = 3;
                extra_bits = 16;
              }

              if (extra_bits > 0)
              {
                rc = receive_code (ref in, extra_bits, ref user_info);
                if (rc < 0)
                  return rc;

                if (!deflate_64)
                {
                  if (code == 284 && rc == 31)   // not allowed value, in doubt allow for deflate_64
                    return UNPK_BAD_CODE;
                }

                length += rc;
              }


              /* read-in a distance code */

              rc = receive_huffman_code (ref in, node_b, MAX_NODES_B, out code, ref user_info);
              if (rc < 0)
                return rc;

              /* read the distance code suffix (if any) */

              if (!deflate_64 && code > 30)
                return UNPK_BAD_CODE;

              distance   = distance_info [code] . min_distance;   // 1 .. 24577 (or 49153 for deflate_64)
              extra_bits = distance_info [code] . extra_bits;     // 0 .. 13    (or 14    for deflate_64)

              if (extra_bits > 0)
              {
                rc = receive_code (ref in, extra_bits, ref user_info);
                if (rc < 0)
                  return rc;
                distance += rc;
              }

              // length   (3..  258), or (3..65538) for deflate_64
              // distance (1..32768), or (1..65536) for deflate_64
#if debug
              printf ("reference=(%d,%d)\n", -distance, length);
#endif

              {
                int  from0, i;
                byte ch;

                from0 = (output_i - distance) & output_size_mask;

                // note that the referenced string may overlap the current position.
                // the backward distance may cross one or more block boundaries.
                // length can now be 65538 instead of only 258 !   that's larger than the buffer !
                // However a distance cannot refer past the beginning of the output stream.

                if (from0    + length >= output_size ||   // source in 2 parts
                    output_i + length >= output_size ||   // target in 2 parts
                    length > distance)                    // overlapping, or length larger than buffer size
                {
                  // use slow byte-wise copy

                  for (i=0; i<length; i++)
                  {
                    ch = output[from0];
                    from0 = (from0 + 1) & output_size_mask;

                    output[output_i] = ch;
                    output_i = (output_i + 1) & output_size_mask;

                    rc = send_block (ref rout, ch, ref user_info);
                    if (rc < 0)
                      return rc;
                  }
                }
                else    // optimized version : copy entire block
                {
                  // assert length <= distance, so length is smaller than buffer size

                  // write bytes first
                  rc = send_block (ref rout, output[from0:length], ref user_info);
                  if (rc < 0)
                    return rc;

                  // and potentially overwrite them later, since output_i and from0 can denote almost the same location !
                  output[output_i:length] = output[from0:length];
                  output_i += length;
                }
              }
            }
          }
        }
        break;


      default:
        return UNPK_BAD_BTYPE;
    }

    if (final != 0)    /* was this the final block ? */
      break;
  }

  rc = flush_output (ref rout, ref user_info);
  if (rc < 0)
    return rc;

  copy_to_read_ahead (ref in, out extra);

  return 0;
}

end GEN_UNPACK;

/**************************************************************************/
