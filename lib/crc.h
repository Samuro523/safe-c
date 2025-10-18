
// crc.h : md5, sha-2, adler and crc checksums

//------------------------------------------------------------------------

struct MD5_INFO;    // private type

// init private type
void MD5_init (out MD5_INFO info);

// add next block of input (must be repeated until end of input)
void MD5_update (ref MD5_INFO info, byte[] buffer);

// compute resulting MD5 checksum
void MD5_final (ref MD5_INFO info, out byte[16] md5);

//------------------------------------------------------------------------

struct SHA256_INFO;    // private type

void sha256_init   (out SHA256_INFO ctx);
void sha256_update (ref SHA256_INFO ctx, byte[] message);
void sha256_final  (ref SHA256_INFO ctx, out byte[32] digest);

//------------------------------------------------------------------------

struct SHA512_INFO;    // private type

void sha512_init   (out SHA512_INFO ctx);
void sha512_update (ref SHA512_INFO ctx, byte[] message);
void sha512_final  (ref SHA512_INFO ctx, out byte[64] digest);

//------------------------------------------------------------------------

/* Updates an Adler-32 checksum with the bytes in buf[]     */
/* The Adler-32 checksum must be initialized to 1 (!!!!!).  */

void update_adler (ref uint adler, byte[] buf);

/*
 Example:

 uint adler = 1;
 while (read_buffer (out buffer, length) != EOF)
   update_adler (ref adler, buffer[0:length]);
 if (adler != original_adler) error();
*/

//------------------------------------------------------------------------

/* Updates a CRC checksum with the bytes in buf[]     */
/* The CRC checksum must be initialized to 0 (!!!!!). */
/* note: the Adler-32 should be preferred as it is faster, safer */
/*       and uses less memory than the CRC.                      */

void update_crc (ref uint crc, byte[] buf);

/*
 Example:

 uint crc = 0;
 while (read_buffer (out buffer, length) != EOF)
   update_crc (ref crc, buffer[0:length]);
 if (crc != original_crc) error();
*/

//------------------------------------------------------------------------
