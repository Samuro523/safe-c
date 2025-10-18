
/* unpack1.c : Huffman Algorithm */

use ../zip;

/***********************************************************************/

#define debug_code_value_1    0   /* printfs code values ordered by code */
#define debug_code_value_2    0   /* printfs code values ordered by length */
#define debug_huff_tree       0   /* printf huffman tree */

#if debug_code_value_1 || debug_code_value_2 || debug_huff_tree 
use ../console;
#endif

/***********************************************************************/

const int MAX_CODE_BITS = 15;

/***********************************************************************/

// already computed : 'code_length'
// computes         : 'code_value'

public int compute_huffman_code_values (ref NODE[] node, int max_nodes)
{

  /* convert code lengths to code values */

  {
    short count[MAX_CODE_BITS+1];
    short start[MAX_CODE_BITS+1];
    int   i, value, length;


    /* read-in frequency of code_lengths into count[] */

    clear count;
    for (i=0; i<max_nodes; i++)
      count[node[i].code_length]++;      /* count[0] is never used */


    /* fill start[] as offset of code_value */

    clear start;
    value = 0;
    count[0] = 0;
    for (i=1; i<=MAX_CODE_BITS; i++)
    {
      value = ((value + count[i-1]) << 1);
      start[i] = (short)value;
    }


    /* assign code values to all nodes */

    for (i=0; i<max_nodes; i++)
    {
      length = node[i].code_length;
      if (length == 0)
        continue;

      node[i].code_value = start[length]++;
    }
  }

  return 0;
}

/***********************************************************************/

// already computed : 'code_value'
// computes         : 'index_zero' and 'index_one'

public int build_huffman_tree (ref NODE[] node, int max_nodes)
{
  int root, i, j, k;
  int first, last;    /* range of free entries in node[] */

#if debug_code_value_1
  {
    char code[32];

    printf ("\nCODE VALUES ORDERED BY CODE:\n");
    for (i=0; i<max_nodes; i++)
    {
      clear code;

      for (j=0; j<node[i].code_length; j++)
        code[j] = (char)(48 + (uint)((node[i].code_value & (1 << (node[i].code_length-1-j))) > 0));

      printf ("value (%3d) '%c' : code %s\n",
              i,
              i >= 32 && i < 128 ? (char)i : '.',
              code);
    }
    printf ("\n");
  }
#endif

#if debug_code_value_2
  {
    int  len, count;
    char code[32];

    printf ("\nCODE VALUES ORDERED BY LENGTH :\n");

    count = 0;
    for (len = 1; len <= MAX_CODE_BITS; len ++)
    {
      for (i=0; i<max_nodes; i++)
      {
        if (node[i].code_length != len)
          continue;

        count++;
        clear code;

        for (j=0; j<node[i].code_length; j++)
          code[j] = (char)(48 + (uint)((node[i].code_value & (1 << (node[i].code_length-1-j))) > 0));

        printf ("value (%3d) '%c' : code %s\n",
                i,
                i >= 32 && i < 128 ? (char)i : '.',
                code);
      }
    }
    printf ("%d codes\n", count);
    printf ("\n");
  }
#endif

  /* init all indexes to -1 */
  for (i=0; i<2*max_nodes; i++)
  {
    node[i].index_zero = -1;
    node[i].index_one  = -1;
  }

  /* init range of free entries */
  first = max_nodes;     /* included */
  last  = 2*max_nodes;   /* not included */

  /* reserve root node */
  root = last-1;
  last--;

  /* enter all leaf nodes in tree, creating all intermediate nodes */
  for (i=0; i<max_nodes; i++)
  {
    if (node[i].code_length == 0)  /* does not belong to tree */
      continue;

    k = root;
    for (j=node[i].code_length-1; /*nothing*/; j--)
    {
      if ((node[i].code_value & (1 << j)) != 0)   /* bit 1 */
      {
        ref short next = node[k].index_one;

        if (j == 0)   /* last bit of sequence */
        {
          if (next != -1)                  /* must be NIL ! */
            return UNPK_HUFF_BAD_STRUCT;
          next = (short)i;                        /* becomes the new leaf */
          break;
        }

        if (next == -1)  /* NIL */
        {
          /* create a new intermediate node */
          if (first == last)
            return UNPK_HUFF_OVERFLOW;
          next = (short)first++;
        }

        k = next;
      }
      else
      {
        ref short next = node[k].index_zero;

        if (j == 0)   /* last bit of sequence */
        {
          if (next != -1)                  /* must be NIL ! */
            return UNPK_HUFF_BAD_STRUCT;
          next = (short)i;                        /* becomes the new leaf */
          break;
        }

        if (next == -1)  /* NIL */
        {
          /* create a new intermediate node */
          if (first == last)
            return UNPK_HUFF_OVERFLOW;
          next = (short)first++;
        }

        k = next;
      }
    }
  }

#if debug_huff_tree
  {
    printf ("\nHUFFMAN TREE :\n");
    for (i=0; i<2*max_nodes; i++)
    {
      printf ("node %3d '%c' : zero:%d one:%d\n",
              i,
              i >= 32 && i < 128 ? (char)i : '.',
              node[i].index_zero,
              node[i].index_one);
    }
    printf ("\n");
  }
#endif

  return 0;
}

/***********************************************************************/
