
/* unpack.h : unpacking using Deflate+Huffman Algorithm */

/***********************************************************************/

use ../zip;

//--------------------------------------------------------------------

/* 'user_info' can be used to pass data to the user's i/o functions */

generic <USER_INFO>
package GEN_UNPACK

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

  /* note: unpack() reads data in large blocks and will probably read  */
  /*       past the compression part. The data read but not used by    */
  /*       unpack will be returned in the structure 'extra'.           */

  int unpack (ref USER_INFO  user_info, 
                  IO_READ    user_read, 
                  IO_WRITE   user_write, 
              out READ_AHEAD extra,
                  bool       deflate_64 = false);  // false=deflate, true=deflate64 algorithm

  //--------------------------------------------------------------------

end GEN_UNPACK;

//--------------------------------------------------------------------
