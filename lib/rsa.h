//--------------------------------------------------------------------------
// rsa.h : asymmetric encryption (invented by Rivest, Shamir & Adelman)
//--------------------------------------------------------------------------

use integer;

struct RSA_PUBLIC_KEY
{
  INTEGER n;
  uint    e;
}

struct RSA_SECRET_KEY
{
  INTEGER n, d, p, q, dp, dq, qinv;
}

//--------------------------------------------------------------------------

// bit_size = 1024(corporate), 1536(intermediate), 2048(military), .., 8192.
// takes 1 min to generate 1024-bit keys, 4 min for 1536-bit keys, 10 min for 2048-bit keys.

void RSA_generate_keys (    uint           bit_size,
                        out RSA_PUBLIC_KEY public_key,
                        out RSA_SECRET_KEY secret_key);

//--------------------------------------------------------------------------

// verify that the keys have at least 1024 bits.
bool public_is_valid (RSA_PUBLIC_KEY public_key);
bool secret_is_valid (RSA_SECRET_KEY secret_key);

//--------------------------------------------------------------------------

// encrypt a 32-byte value.
// duration: for a 1024-bit key :  5 msecs.
// duration: for a 1536-bit key : 10 msecs.
// duration: for a 2048-bit key : 15 msecs.
// returns 0 if OK, -1 if error.

int RSA_encrypt (    byte[32]       data_in,    // input value
                 out INTEGER        data_out,   // encrypted RSA data
                     RSA_PUBLIC_KEY public_key);


// decrypt RSA data to obtain a 32-byte value.
// duration: for a 1024-bit key :  250 msec.
// duration: for a 1536-bit key :  800 msec.
// duration: for a 2048-bit key : 1800 msec.
// returns 0 if OK, -1 if error.

int RSA_decrypt (    INTEGER        data_in,   // encrypted RSA data
                 out byte[32]       data_out,  // output value
                     RSA_SECRET_KEY secret_key);

//--------------------------------------------------------------------------

// make a signature from a 32-byte checksum.
// duration: for a 1024-bit key :  250 msec.
// duration: for a 1536-bit key :  800 msec.
// duration: for a 2048-bit key : 1800 msec.
// returns 0 if OK, -1 if error.

int RSA_make_signature (    byte[32]       data_in,   // checksum
                        out INTEGER        data_out,  // RSA signature
                            RSA_SECRET_KEY secret_key);


// convert a signature into a 32-bit checksum value.
// (this value must then be compared with the message text's checksum)
// duration: for a 1024-bit key :  5 msecs.
// duration: for a 1536-bit key : 10 msecs.
// duration: for a 2048-bit key : 15 msecs.
// returns 0 if OK, -1 if error.

int RSA_verify_signature (    INTEGER        data_in,    // RSA signature
                          out byte[32]       data_out,   // checksum
                              RSA_PUBLIC_KEY public_key);

//--------------------------------------------------------------------------
