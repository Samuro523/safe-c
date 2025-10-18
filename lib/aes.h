
// aes.h : Advanced Encryption Standard (Rijndael, 2001)

//--------------------------------------------------------------------------

// encrypt 1 block
void aes_encrypt (out byte[16] data_out,
                      byte[16] data_in,
                      byte[]   key);      // key'length must be 16, 24 or 32.

//--------------------------------------------------------------------------

// decrypt 1 block
void aes_decrypt (out byte[16] data_out,
                      byte[16] data_in,
                      byte[]   key);      // key'length must be 16, 24 or 32.

//--------------------------------------------------------------------------

void aes_mac (out byte[] mac,    // mac'length must be in range 1 .. 16
                  byte[] data,   // any length
                  byte[] key);   // key'length must be 16, 24 or 32.

//--------------------------------------------------------------------------

// encrypt multiple blocks with the same key
struct AES_ENCRYPT_KEY;

// key'length must be 16, 24 or 32.
void aes_init_encrypt (out AES_ENCRYPT_KEY aes, byte[] key);
void aes_fast_encrypt (AES_ENCRYPT_KEY aes, byte[16] data_in, out byte[16] data_out);

//--------------------------------------------------------------------------

// decrypt multiple blocks with the same key
struct AES_DECRYPT_KEY;

// key'length must be 16, 24 or 32.
void aes_init_decrypt (out AES_DECRYPT_KEY aes, byte[] key);
void aes_fast_decrypt (AES_DECRYPT_KEY aes, byte[16] data_in, out byte[16] data_out);

//--------------------------------------------------------------------------

// encrypt or decrypt a block of data using OFB (output feedback)
void crypt_block (    byte[]    key,      // key'length must be 16, 24 or 32.
                      byte[16]  init,     // initialization vector
                  ref byte[]    data);

//--------------------------------------------------------------------------
