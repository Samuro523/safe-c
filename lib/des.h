
// des.h : Data Encryption Standard (1977)

//--------------------------------------------------------------------------
// the lowest bit of each key byte is not used.

void des_encrypt (out byte[8] data_out,
                      byte[8] data_in,
                      byte[8] key);

void des_decrypt (out byte[8] data_out,
                      byte[8] data_in,
                      byte[8] key);

void des_mac (out byte[]  mac,   // must be between 1 and 8 bytes large.
                  byte[]  data,
                  byte[8] key);

//--------------------------------------------------------------------------
