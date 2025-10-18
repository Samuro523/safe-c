
/* pack1.h */

struct CODE
{
  short value;   /* literal (0..255) or distance (-1..-32768) */
  byte  length;  /* add 3 (3 .. 258) (for distance only)      */
}


generic <OUTPUT_INFO, USER_INFO>

  int send_code (ref OUTPUT_INFO info, int code, int nb_bits, int bit_order, ref USER_INFO user);
  int send_block (ref OUTPUT_INFO info, byte[] data, ref USER_INFO user);

package OUTPUT_BLOCK

  int output_block
       (CODE[]          output,
        byte[]          data,      /* in clear */
        ref OUTPUT_INFO rout,
        ref USER_INFO   user);

end OUTPUT_BLOCK;

