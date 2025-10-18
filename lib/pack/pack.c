
/* pack.c */

use pack0, pack1, ../zip;

//-------------------------------------------------------------------
#begin unsafe
//-------------------------------------------------------------------

/* !!! BTYPE = 3 seems to exist and is supported by Winzip ! */

/* deflate algorithm (pass 1) */

//-------------------------------------------------------------------

/* the maximum backward distance of a reference is : */
/* from -1 to -(CHUNK_SIZE - LOOK_AHEAD)             */

const int CHUNK_SIZE = 32768;

/* the minimum possible length of a reference at the worst point */
/* in the input buffer is LOOK_AHEAD.                            */

const int LOOK_AHEAD = 256;

/* constrained range of lengths accepted in a reference */
const int MIN_LENGTH  =    3;      /* at least 3 */
const int MAX_LENGTH  =  258;

/* max entries in hash table (2^N should be close to CHUNK_SIZE) */
const uint MAX_HASH_BITS = 15;     /* must be >= 8 */

/* DO NOT CHANGE */
const uint MAX_HASH_ENTRIES =  (1 << MAX_HASH_BITS);

/* DO NOT CHANGE */
const int INPUT_BUFFER_SIZE = (2 * CHUNK_SIZE);

/* DO NOT CHANGE */
const int MAX_HNODES = (CHUNK_SIZE - LOOK_AHEAD);

/* this type must hold all values in the range 0 .. MAX_HNODES - 1 */
/* as well as the extra value NIL (see below).                     */
typedef short LINK;

/* note: use a value of LINK outside the range 0 .. MAX_HNODES - 1 */
const LINK NIL = (-1);

/* the maximum chain length we want to travel per input byte */
const int MAX_HASH_CHAIN_LENGTH =  20;

/**********************************************************************/
/* memory allocation notes:                                           */
/* we allocate an input buffer of (2 * CHUNK_SIZE) bytes.             */
/* we allocate a hnode table of (CHUNK_SIZE - LOOK_AHEAD) * (6 or 8). */
/* we allocate a hash table of MAX_ENTRIES * (2 or 4 bytes).          */
/* the input file is loaded with chunks of CHUNK_SIZE bytes.          */
/**********************************************************************/

struct HNODE
{
  LINK  next;  /* link to next hnode of the chain. */
  LINK* prev;  /* ptr to previous hnode, to hash table or null */
}

//-------------------------------------------------------------------

/* produce a well-shaken hash value from a 3-byte character array */
/* (we take all bits into account in an evenly manner)            */

uint hash_value_of (byte[3] s)
{
  return ( s[0]
        ^ (s[1] << ((MAX_HASH_BITS-8+1)) / 2)
        ^ (s[2] << (MAX_HASH_BITS-8)));
}

//-------------------------------------------------------------------

package body GEN_PACK

  //-------------------------------------------------------------------

  package OUTPUT

    const int OUTPUT_BUFFER_SIZE = 32768;

    struct OUTPUT_INFO
    {
      byte     buffer[OUTPUT_BUFFER_SIZE];
      int      count;   /* nb of complete bytes in buffer[] */
      byte     by;      /* new byte that is being built bit by bit */
      int      power;   /* power of 2 that will be added to 'by' next time */
                        /* when power == 128 is added, the byte is moved   */
                        /* into buffer.                                    */
      IO_WRITE user_write;  /* function used to write a buffer */
    }

    /**************************************************************************/

    void init_output (out OUTPUT_INFO info,
                      IO_WRITE        user_write);

    /**************************************************************************/

    int send_code (ref OUTPUT_INFO info, int code, int nb_bits, int bit_order, ref USER_INFO user);

    /**************************************************************************/

    /* flush the remaining bits (if any) of the current byte       */
    /* in order to be on a byte boundary, then send the data block */

    int send_block (ref OUTPUT_INFO info, byte[] data, ref USER_INFO user);

    /**************************************************************************/

    /* send all remaining data to output device */

    int flush_output (ref OUTPUT_INFO info, ref USER_INFO user);

    /**************************************************************************/

  end OUTPUT;

  //-------------------------------------------------------------------

  package body OUTPUT   // output bit stream

    #define debug_count  0     /* printf nb of bits output */

    #if debug_count
      long global_bit_count;
    #endif

    /**************************************************************************/

    public void init_output (out OUTPUT_INFO info,
                             IO_WRITE        user_write)
    {
      clear info;

      info.power      = 1;
      info.user_write = user_write;

    #if debug_count
      global_bit_count = 0;
    #endif
    }

    /**************************************************************************/

    public int send_code (ref OUTPUT_INFO info, int code, int nb_bits, int bit_order, ref USER_INFO user)
    {
      int  i, power, rc;
      byte bit;

      assert (nb_bits >= 1 && nb_bits <= 15);
      assert (code >= 0 && code < (1 << nb_bits));

      if (bit_order == LSB_FIRST)
        power = 1;
      else if (bit_order == MSB_FIRST)
        power = (1 << (nb_bits-1));
      else
        abort;

      for (i=0; i<nb_bits; i++)
      {
        bit = (byte)((code & power) != 0);

        /* send 'bit' to output stream */

        if (bit != 0)
          info.by |= (byte)info.power;
        info.power <<= 1;

        if (info.power > 128)
        {
          if (info.count == OUTPUT_BUFFER_SIZE)  /* buffer full */
          {
            rc = info.user_write (ref user, info.buffer[0:info.count]);
            if (rc != info.count)
              return PK_WRITE_ERROR;

            info.count = 0;
          }

          info.buffer[info.count++] = info.by;

          info.by = 0;
          info.power = 1;
        }

        /* advance to next power */

        if (bit_order == LSB_FIRST)
          power <<= 1;
        else
          power >>= 1;

    #if debug_count
      global_bit_count++;
    #endif
      }

      return 0;
    }

    /**************************************************************************/

    /* flush the remaining bits (if any) of the current byte       */
    /* in order to be on a byte boundary, then send the data block */

    public int send_block (ref OUTPUT_INFO info, byte[] data, ref USER_INFO user)
    {
      int rc, i;

      /* flush remaining bits of current byte (if any) */

      while (info.power != 1)
      {
        rc = send_code (ref info, 0, 1, MSB_FIRST, ref user);
        if (rc < 0)
          return rc;

    #if debug_count
      global_bit_count--;
    #endif
      }

      for (i=0; i<data'length; i++)
      {
        if (info.count == OUTPUT_BUFFER_SIZE)  /* buffer full */
        {
          rc = info.user_write (ref user, info.buffer);
          if (rc != info.count)
            return PK_WRITE_ERROR;

          info.count = 0;
        }

        info.buffer[info.count++] = data[i];

    #if debug_count
      global_bit_count += 8;
    #endif
      }

      return 0;
    }

    /**************************************************************************/

    /* send all remaining data to output device */

    public int flush_output (ref OUTPUT_INFO info, ref USER_INFO user)
    {
      int rc;

      /* flush remaining bits of current byte (if any) */

      while (info.power != 1)
      {
        rc = send_code (ref info, 0, 1, MSB_FIRST, ref user);
        if (rc < 0)
          return rc;

    #if debug_count
      global_bit_count--;
    #endif
      }

      if (info.count != 0)    /* buffer contains something */
      {
        rc = info.user_write (ref user, info.buffer[0:info.count]);
        if (rc != info.count)
          return PK_WRITE_ERROR;

        info.count = 0;
      }

    #if debug_count
      printf ("global_bit_count = %ld\n", global_bit_count);
    #endif

      return 0;
    }

    /**************************************************************************/

  end OUTPUT;

  //-------------------------------------------------------------------

  package OUTPUT_INFO_OCC = new OUTPUT_BLOCK (OUTPUT_INFO => OUTPUT_INFO,
                                              USER_INFO   => USER_INFO,
                                              send_code   => send_code,
                                              send_block  => send_block);

  //-------------------------------------------------------------------

  /* main packing function */

  public int pack (ref USER_INFO user_info, IO_READ user_read, IO_WRITE user_write)
  {
    byte          input  [INPUT_BUFFER_SIZE+2];  /* input buffer + sentinel */
    LINK          hash   [MAX_HASH_ENTRIES];     /* hash table */
    HNODE         hnode  [MAX_HNODES];           /* hnode table */
    CODE          output [CHUNK_SIZE];
    int           output_i, i;
    int           rc, first, first9, last, size, limit, hnode_i, first0;
    bool          eof;
    int           p;     // index into input
    HNODE*        nd;
    OUTPUT_INFO   rout;

    assert MAX_HASH_BITS >= 8;

    clear input, output;

    /* init hash table */
    clear hash;
    for (i=0; i<hash'length; i++)
      hash[i] = NIL;      /* set to -1 */

    /* init hnode table */
    clear hnode;     /* set all .prev fields to null */

    /* init 'out' structure */
    init_output (out rout, user_write);


    /* init various variables */

    first = CHUNK_SIZE;    /* portion of input[] buffer */
    last  = CHUNK_SIZE;

    eof     = false;        /* end-of-file = FALSE */
    hnode_i = 0;            /* index of next free entry in hnode table */


    /* main loop : fill input buffer, deflate,               */
    /*             send to huffman/output, shift input data. */

    for (;;)
    {
      /* fill input buffer as much as possible, increasing 'last'. */
      /* position 'eof' in case we reached end-of-file.            */

      while (!eof)
      {
        size = INPUT_BUFFER_SIZE - last;         /* max size to fill */

        if (size == 0)                           /* the buffer is full */
          break;

        if (size > CHUNK_SIZE)                   /* limit reading size */
          size = CHUNK_SIZE;

        rc = user_read (ref user_info, out input[last:size]);
        if (rc < 0 || rc > size)
          return PK_READ_ERROR;

        if (rc == 0)      /* end-of-file */
          eof = true;

        last += rc;
      }


      /* compute 'limit' : we will treat the range 'first' .. 'limit' */
      /* note however that we try to keep look-ahead bytes and that   */
      /* the treatment can use the entire range [first..last[.        */

      limit = INPUT_BUFFER_SIZE - LOOK_AHEAD;
      if (limit > last)   /* we don't have so many bytes */
        limit = last;


      /* save initial input buffer position for this block */
      first0 = first;


      /* setup index for output[] buffer */
      output_i = 0;


      /* setup pointer to current input position */

      p = first;


      /* attention : 'first' can be set to a value larger than 'limit'    */
      /*             within the loop, in case a long reference is found   */
      /*             that crosses into the look-ahead part.               */

      while (first < limit)
      {
        int  h_value, index, idx, best_len, max_len, counter;
        int  offset, len, q, best_offset = 0;
        byte save;


        /* compute maximum length of input buffer at current position */

        max_len = last - first;
        if (max_len > MAX_LENGTH)           /* algorithmic contraint */
          max_len = MAX_LENGTH;


        /* compute 'index' : the start hnode of the hash chain.         */
        /* note: p[1] and p[2] point possibly at input[MAX_BUFFER_SIZE] */
        /*       and input[MAX_BUFFER_SIZE+1], respectively.            */
        /* note: 'h_value' and 'idx' will be used later on.             */

        h_value = (int)hash_value_of (input[p:3]);

        idx = hash [h_value];
        index = idx;

        /* find longest possible common match */

        best_len = 0;

        counter = 0;              /* init chain length */
        for (;;)
        {
          if (index == NIL)       /* end of chain reached */
            break;

          /* compute pattern's offset in history of input buffer */
          offset = index - hnode_i;
          if (offset >= 0)
            offset -= MAX_HNODES;
          offset += first;

          /* compute pointer to fragment to compare */
          q = offset;

          /* compute common length of fragments */
          if (input[p+best_len] == input[q+best_len])   /* could become a longer */
          {                                             /* matching pattern.     */
            /* store sentinel at max_len */
            save = input[p+max_len];                    /* save sentinel value */
            input[p+max_len] = (byte)(input[q+max_len] ^ 0x01); /* put mismatching character */

            len = 0;
            while (input[p+len] == input[q+len])
              len++;

            input[p+max_len] = save;              /* restore sentinel value */


            /* evaluate the result 'len' */

            if (len > best_len)       /* we found a better length */
            {
              best_offset = offset;
              best_len    = len;

              if (best_len == max_len)   /* we cannot find a better match */
                break;
            }
          }

          index = hnode[index].next;    /* continue with next hnode */

          /* check if we reached the maximum chain length */
          counter++;
          if (counter >= MAX_HASH_CHAIN_LENGTH)
            break;
        }


        /* if we found an interesting match, store its backward reference */

        if (best_len >= MIN_LENGTH)    /* a match of acceptable length */
        {
          output[output_i].value = (short)(best_offset - first);
          output[output_i++].length = (byte)(best_len - 3);
        }
        else   /* insufficient common length : store as single literal */
        {
          output[output_i++].value = input[first];
          best_len = 1;                              /* force to length 1 */
        }


        /* advance 'best_len' bytes, updating the hnode table. */
        /* note: this might increment 'first' past 'limit'.   */

        first9 = first + best_len;     /* final value for 'first' */

        for (;;)
        {
          /* setup pointer to current hnode */
          nd = &hnode[hnode_i];

          /* remove hnode from old chain (if any) */
          if (nd->prev != null)
            *(nd->prev) = NIL;

          /* note: 'h_value' and 'idx' were initialized above */
          if (idx != NIL)                 /* at least 1 hnode in chain */
            hnode[idx].prev = &nd->next;

          nd->next = (LINK)idx;
          nd->prev = &hash[h_value];

          hash[h_value] = (LINK)hnode_i;

          /* increment hnode_i */
          hnode_i++;
          if (hnode_i == MAX_HNODES)
            hnode_i = 0;

          /* increment 'first' and 'p' */
          first++;
          p++;

          if (first == first9)    /* 'first' reached its final value */
            break;

          /* initialize 'h_value' and 'idx' for next loop */
          h_value = (int)hash_value_of (input[p:3]);
          idx = hash[h_value];
        }
      }


      /* output code bit BFINAL (indicates if this is the final block) */

      rc = send_code (ref rout, (int)(limit == last), 1, LSB_FIRST, ref user_info);
      if (rc < 0)
        return rc;


      /* encode output[] using Huffman (pass2) and send it to output */

      rc = output_block (output[0:output_i],
                         input[first0:first - first0],
                         ref rout,
                         ref user_info);
      if (rc < 0)
        return rc;


      /* check if there is more data to treat */

      if (limit == last)   /* we have treated all data until the end */
        break;


      /* move the input buffer backwards CHUNK_SIZE bytes */

      input[0:CHUNK_SIZE] = input[CHUNK_SIZE:CHUNK_SIZE];
      first -= CHUNK_SIZE;
      last  -= CHUNK_SIZE;
    }


    /* send remaining bytes in output buffer */

    rc = flush_output (ref rout, ref user_info);
    if (rc < 0)
      return rc;

    return 0;
  }

  //-------------------------------------------------------------------

end GEN_PACK;

//-------------------------------------------------------------------
#end unsafe
//-------------------------------------------------------------------
