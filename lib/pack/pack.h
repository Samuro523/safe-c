
/* pack.h : packing using Deflate+Huffman Algorithm */

/***********************************************************************/

generic <USER_INFO>
package GEN_PACK

  //--------------------------------------------------------------------

  /* user-provided read function.                 */
  /* must return the number of bytes transferred, */
  /* zero in case of end-of-data,                 */
  /* or a negative value in case of error.        */

  typedef int IO_READ (ref USER_INFO user_info,
                       out byte[]    buffer);

  //--------------------------------------------------------------------

  /* user-provided write function.                */
  /* must return the number of bytes transferred, */
  /* or a negative value in case of error.        */

  typedef int IO_WRITE (ref USER_INFO user_info,
                        byte[]        buffer);

  //--------------------------------------------------------------------

  int pack (ref USER_INFO user_info, IO_READ user_read, IO_WRITE user_write);

  //--------------------------------------------------------------------

end GEN_PACK;

//--------------------------------------------------------------------
