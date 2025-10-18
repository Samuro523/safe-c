
/* pack2.c : Huffman Algorithm */

use ../sorting;

/***********************************************************************/

#define debug_ordered_codes   0   /* printf codes ordered by frequency */
#define debug_huff_tree       0   /* printf huffman tree */
#define debug_constrain       0   /* algorithm to constrain code length */
#define debug_code_value_1    0   /* printfs code values ordered by code */
#define debug_code_value_2    0   /* printfs code values ordered by length */

/***********************************************************************/
#begin unsafe
/***********************************************************************/

typedef NODE *PNODE;

int compare_freq (PNODE p1, PNODE p2)
{
  if (p1->count < p2->count)
    return -1;
  if (p1->count > p2->count)
    return +1;
  return 0;
}

/***********************************************************************/

package do_sort = new HeapSort (ELEMENT => PNODE, compare => compare_freq);

/***********************************************************************/

/* Attention: the caller must reserve 2*max_nodes entries in node[] */
/* (the first max_nodes entries contain the data, the second are    */
/* used as temporary workspace).                                    */
/* node[] should have been cleared to 0x00.                         */

public int compute_huffman (ref NODE node[],
                            int      max_nodes,
                            int      max_code_bits)    /* <= MAX_CODE_BITS */
{
  PNODE[]^  sort, sort2;    /* ptrs to nodes ordered by frequency */
  short[]^  order;          /* travel order to determine code_len */
  int       order_count;
  int       high_code_len;  /* used by code length computation */


  assert (max_code_bits >= 1 && max_code_bits <= MAX_CODE_BITS);


  /* reserve work tables on stack */

  sort  = new PNODE [max_nodes];
  sort2 = new PNODE [max_nodes];
  order = new short [2 * max_nodes];


  /* fill table 'sort' with ordered frequencies */

  {
    int i;

    for (i=0; i<max_nodes; i++)
      sort^[i] = &node[i];

    do_sort.sort (ref sort^);

    /* make a copy of sort into sort2 for later */
    sort2^ = sort^;
  }


#if debug_ordered_codes
  {
    int  i, value;
    NODE *n;

    printf ("\nCODES SORTED BY FREQUENCIES :\n");
    for (i=0; i<max_nodes; i++)
    {
      n = sort^[i];
      value = n - node;
      printf ("value %3d '%c' : %4d\n",
              value,
              isprint(value) && value < 256 ? value : '.',
              n->count);
    }
    printf ("\n");
  }
#endif


  /* Huffman algorithm : build Huffman Tree */

  {
    int first, last;       /* valid entries in sort[] */
    int nb_nodes;          /* nb entries in node[] */
    int left, right, i;

    first = 0;
    last  = max_nodes;

    nb_nodes = max_nodes;

    order_count = 0;

    /* skip entries with zero frequency, but leave at least 2 entries */
    while (last - first >= 3 && sort^[first]->count == 0)
      first++;

    while (last - first >= 2)   /* at least 2 entries left */
    {
      /* create a new node, father of the 2 first nodes in sort[] */

      left  = (int)(sort^[first]     - &node);
      right = (int)(sort^[first + 1] - &node);

      order^[order_count++] = (short)left;
      order^[order_count++] = (short)right;

      node[nb_nodes].count = node[left].count + node[right].count;
      node[left].parent = (short)nb_nodes;
      node[right].parent = (short)nb_nodes;

      first++;      /* remove 1 node from sort[] */

      sort^[first] = &node[nb_nodes];

      nb_nodes++;

      /* order again the [first..last[ entries by frequency */

      for (i=first; i+1<last; i++)
      {
        PNODE temp;

        if (sort^[i]->count <= sort^[i+1]->count)
          break;

        /* swap entries */
        temp       = sort^[i];
        sort^[i]   = sort^[i+1];
        sort^[i+1] = temp;
      }
    }
  }


#if debug_huff_tree
  {
    int i;

    printf ("\nHUFFMAN TREE :\n");
    for (i=0; i<2*max_nodes; i++)
    {
      printf ("node %3d '%c' : freq %4d parent node %d\n",
              i,
              isprint(i) && i < 256 ? i : '.',
              node[i].count,
              node[i].parent);
    }
    printf ("\n");
  }
#endif


  /* compute the code length of each node */

  {
    int k, i, len;

    high_code_len = 1;       /* highest generated code length */

    for (k=order_count-1; k>=0; k--)     /* scan tree from head to leafs */
    {
      i = order^[k];

      /* length of node = length of parent node + 1 (root node == 0) */
      len = node[node[i].parent].code_length + 1;

      node[i].code_length = (short)len;

      if (len > high_code_len)
        high_code_len = len;
    }
  }

  if (high_code_len > max_code_bits)    /* tree is too tall */
  {
    short[]^ count;
    int      i, lev, overflow;

    count = new short [high_code_len + 1];


    /* read-in current code_lengths into count[] */

    for (i=0; i<max_nodes; i++)
      count^[node[i].code_length]++;      /* count[0] is never used */

#if debug_constrain
  printf ("code lengths frequencies before overflow handling :\n");
  for (i=0; i<=high_code_len; i++)
    printf ("len %d : %ld codes\n", i, count[i]);
  printf ("\n");
#endif

    /* remove all levels > high_code_len */

    overflow = 0;
    while (high_code_len > max_code_bits)
    {
      if (count^[high_code_len] > 0)
      {
        /* make sure it's a multiple of two */
        assert ((count^[high_code_len] & 1) == 0);

        i = count^[high_code_len] / 2;
        count^[high_code_len-1] += (short)i;
        overflow += i;
      }

      high_code_len--;
    }


    /* melt overflow nodes into the tree */

    lev = high_code_len - 1;
    while (overflow > 0)
    {
      /* find level with some leaf nodes to move down */
      while (count^[lev] == 0)
        lev--;

      assert (lev >= 1);

      count^[lev  ]--;
      count^[lev+1]++;

      overflow--;
      count^[lev+1]++;

      if (lev < high_code_len - 1)
        lev++;
    }

#if debug_constrain
  printf ("code lengths frequencies after overflow handling :\n");
  for (i=0; i<=max_code_bits; i++)
    printf ("len %d : %ld codes\n", i, count[i]);
  printf ("\n");
#endif

    /* re-distribute the available code_lengths to all symbols */
    /* depending on their frequency.                           */

    /* assign code_length == 0 to all unused nodes */
    for (i=0; i<max_nodes && sort2^[i]->count == 0; i++)
      sort2^[i]->code_length = 0;

    lev = high_code_len;
    while (i < max_nodes)
    {
      /* update level until some values are left in it */
      while (lev >= 1 && count^[lev] == 0)
        lev--;
      assert (lev >= 1);

      sort2^[i]->code_length = (short)lev;
      count^[lev]--;

      i++;
    }


    free count;
  }


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
    for (i=1; i<=high_code_len; i++)
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

#if debug_code_value_1
  {
    int  i, j;
    char code[32];

    printf ("\nCODE VALUES ORDERED BY CODE:\n");
    for (i=0; i<max_nodes; i++)
    {
      clear code;

      for (j=0; j<node[i].code_length; j++)
        code[j] = '0' + ((node[i].code_value
                          & (1 << (node[i].code_length-1-j))) > 0);

      printf ("value (%3d) '%c' : freq %4d code %s\n",
              i,
              isprint(i) && i < 256 ? i : '.',
              node[i].count,
              code);
    }
    printf ("\n");
  }
#endif

#if debug_code_value_2
  {
    int  i, j, len, count;
    char code[32];

    printf ("\nCODE VALUES ORDERED BY LENGTH :\n");

    count = 0;
    for (len = 1; len <= high_code_len; len ++)
    {
      for (i=0; i<max_nodes; i++)
      {
        if (node[i].code_length != len)
          continue;

        count++;
        clear code;

        for (j=0; j<node[i].code_length; j++)
          code[j] = '0' + ((node[i].code_value
                            & (1 << (node[i].code_length-1-j))) > 0);

        printf ("value (%3d) '%c' : freq %4d code %s\n",
                i,
                isprint(i) && i < 256 ? i : '.',
                node[i].count,
                code);
      }
    }
    printf ("%d codes\n", count);
    printf ("\n");
  }
#endif



  free sort;
  free sort2;
  free order;

  return 0;
}

/***********************************************************************/
#end unsafe
/***********************************************************************/
