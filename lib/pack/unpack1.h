
/* unpack1.h : Huffman Computation */

/***********************************************************************/

struct NODE
{
  short code_length;  /* nb bits of huffman code_value (0 = unused) */
  short code_value;   /* value of huffman code                      */
  short index_zero;   /* index to node entry with zero bit (or -1)  */
  short index_one;    /* index to node entry with one bit (or -1)   */
}

/* Attention: the caller must reserve 2*max_nodes entries in node[] */
/* (the first max_nodes entries contain the data, the second        */
/* contain the non-leaf nodes).                                     */
/* node[] should have been cleared to 0x00.                         */
/* the root of the tree will be at index (2*max_nodes-1).           */

/***********************************************************************/

// already computed : 'code_length'
// computes         : 'code_value'

int compute_huffman_code_values (ref NODE[] node, int max_nodes);

/***********************************************************************/

// already computed : 'code_value'
// computes         : 'index_zero' and 'index_one'

int build_huffman_tree (ref NODE[] node, int max_nodes);

/***********************************************************************/
