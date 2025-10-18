
/* pack2.h : Huffman Computation */

/***********************************************************************/

struct NODE
{
  int   count;        /* frequency */
  short parent;       /* index to parent node within huffman tree */
  short code_length;  /* nb bits of huffman code_value */
  short code_value;   /* value of huffman code (store from most to least) */
}

/***********************************************************************/

const int MAX_CODE_BITS  = 15;

/* Attention: the caller must reserve 2*max_nodes entries in node[] */
/* (the first max_nodes entries contain the data, the second are    */
/* used as temporary workspace).                                    */
/* node[] should have been cleared to 0x00.                         */

int compute_huffman (ref NODE node[],
                     int      max_nodes,
                     int      max_code_bits);    /* <= MAX_CODE_BITS */

/***********************************************************************/
