
// rsa.c : asymmetric encryption (invented by Rivest, Shamir & Adelman)

// to do :
// - add another primality test : Lucas, see http://en.wikipedia.org/wiki/Baillie%E2%80%93PSW_primality_test

#define debug 0

use arithm, strings, random, crc, integer;

#if debug
  use console, tracing, files, exception;
#endif

#if debug
  const uint RND_LEN = 30;     // PART OF DATA RESERVED FOR RANDOM NUMBER
#else  
  const uint RND_LEN = 64;
#endif

#if debug
  const uint LIMIT = 512;      // MINIMUM BITS OF N
#else  
  const uint LIMIT = 1024;
#endif

/***************************************************************************/

public bool public_is_valid (RSA_PUBLIC_KEY public_key)
{
  if (public_key.e < 3 || !integer_is_valid (public_key.n))
    return false;

  if (integer_bit_size(public_key.n) < LIMIT)
    return false;
    
  return true;
}

/***************************************************************************/

public bool secret_is_valid (RSA_SECRET_KEY secret_key)
{
  if (!integer_is_valid (secret_key.n) ||
      !integer_is_valid (secret_key.d) ||
      !integer_is_valid (secret_key.p) ||
      !integer_is_valid (secret_key.q) ||
      !integer_is_valid (secret_key.dp) ||
      !integer_is_valid (secret_key.dq) ||
      !integer_is_valid (secret_key.qinv))
    return false;

  if (integer_bit_size(secret_key.n) < LIMIT ||
      integer_bit_size(secret_key.p) < LIMIT/2 ||
      integer_bit_size(secret_key.q) < LIMIT/2)
    return false;

  return true;
}

/***************************************************************************/

// 'data_in' must be smaller than n.
// returns 0 if OK, -1 if error.

int RSA_encrypt_raw (    INTEGER        data_in,
                     out INTEGER        data_out,
                         RSA_PUBLIC_KEY public_key)
{
  INTEGER e;

  if (!integer_is_valid (data_in))
  {
    clear data_out;
    return -1;
  }
  
  if (!public_is_valid (public_key))
  {
    clear data_out;
    return -1;
  }

  make_integer (public_key.e, out e);

  if (exponent_modulo_integer (data_in, e, public_key.n, out data_out) != 0)
    return -1;

  return 0;
}

/***************************************************************************/

// 'data_in' must be smaller than n.
// returns 0 if OK, -1 if error.

int RSA_decrypt_raw (    INTEGER        data_in,
                     out INTEGER        data_out,
                         RSA_SECRET_KEY secret_key)
{
  if (!integer_is_valid (data_in))
  {
    clear data_out;
    return -1;
  }

  if (!secret_is_valid (secret_key))
  {
    clear data_out;
    return -1;
  }

  {
    INTEGER m1, m2, h;

    if (exponent_modulo_integer (data_in, secret_key.dp, secret_key.p, out m1) != 0)
    {
      clear data_out;
      return -1;
    }

    if (exponent_modulo_integer (data_in, secret_key.dq, secret_key.q, out m2) != 0)
    {
      clear data_out;
      return -1;
    }

    // h = (qinv * (m1 + p - m2 )) mod p   (add p in case m1 < m2)
    copy_integer (m1, out h);
    
    if (compare_integer (m1, m2) < 0)
    {
      if (add_integer (h, secret_key.p, out h) != 0)
      {
        clear data_out;
        return -1;
      }
    }
    
    if (subtract_integer (h, m2, out h) != 0 ||
        multiply_modulo_integer (secret_key.qinv, h, secret_key.p, out h) != 0)
    {
      clear data_out;
      return -1;
    }

    if (multiply_integer (h, secret_key.q, out data_out) != 0)
    {
      clear data_out;
      return -1;
    }

    if (add_integer (data_out, m2, out data_out) != 0)
    {
      clear data_out;
      return -1;
    }
  }

  return 0;
}

/***************************************************************************/

void compute_g (    byte r[RND_LEN],
                out byte g[])
{
  SHA512_INFO sha;
  byte[64]    digest;
  int         ofs, rest, chunk;

  clear g;
  
  sha512_init   (out sha);
  sha512_update (ref sha, r);
  sha512_final  (ref sha, out digest);

  rest = g'length;
  ofs = 0;
  while (rest > 0)
  {
    chunk = min (rest, digest'length);
    g[ofs:chunk] = digest[0:chunk];
    ofs += chunk;
    rest -= chunk;
  }
}

/***************************************************************************/

void compute_h (    byte m[],
                out byte h[RND_LEN])
{
  SHA512_INFO sha;
  byte[64]    digest;
  uint        len;
  
  sha512_init   (out sha);
  sha512_update (ref sha, m);
  sha512_final  (ref sha, out digest);
  
  clear h;
  len = umin(RND_LEN, digest'size);
  h[0:len] = digest[0:len];
}
    
/***************************************************************************/

void xor (byte[] a, byte[] b, out byte[] r)
{
  int i;
  
#begin unsafe
  byte* pa = &a;
  byte* pb = &b;
  byte* pr = &r;
  
  assert a'length == b'length;
  assert a'length == r'length;
  
  for (i=0; i<a'length; i++)
    pr[i] = (byte)(pa[i] ^ pb[i]);
#end unsafe
}

/***************************************************************************/

void pad_message (    byte[32] data_in,
                      INTEGER  n,
                  out INTEGER  data_out)
{
  const uint MSG_LEN = data_in'size;
  byte[INTEGER_SIZE] m, g;
  uint               size = integer_bit_size(n)/8 - 1;  // minus 1 byte to be smaller than n
  byte[RND_LEN]      r, h;

  assert RND_LEN + MSG_LEN <= size;
  clear data_out;

  for (;;)
  {
    clear m, g, h;
    m[0:MSG_LEN] = data_in;   // note: the zero-padded 'm' has size-RND_LEN bytes

    get_random_number (out r);

    compute_g (r, out g[0:size-RND_LEN]);

    xor (m[0:size-RND_LEN], g[0:size-RND_LEN], out m[0:size-RND_LEN]);

    compute_h (m[0:size-RND_LEN], out h);

    xor (r, h, out m[size-RND_LEN:RND_LEN]);

    if (m[size-1] == 0)  // highest byte is zero : result will be too small
    {
      #if debug
        printf ("padded result is too small, retry padding\n");
        trace ("padded result is too small, retry padding\n");
      #endif
      continue;
    }
    
    make_large_integer (m[0:size], out data_out);

    // check that the size is smaller than 'n', otherwise retry
    if (compare_integer (data_out, n) >= 0)   // too large
    {
      #if debug
        printf ("padded result >= n, retry padding\n");
        trace ("padded result >= n, retry padding\n");
      #endif
      continue;
    }

    return;
  }
}

/***************************************************************************/

void unpad_message (    INTEGER  c,
                        INTEGER  n,
                    out byte[32] data_out)
{
  const uint MSG_LEN = data_out'size;
  byte[RND_LEN]      r, h;
  uint               size = integer_bit_size(n)/8 - 1;  // minus 1 byte to be smaller than n
  byte[INTEGER_SIZE] m, g;

  assert RND_LEN + MSG_LEN <= size;
  clear m, r, g;

  extract_integer (c, out m[0:size]);

  compute_h (m[0:size-RND_LEN], out h);

  xor (m[size-RND_LEN:RND_LEN], h, out r);

  compute_g (r, out g[0:size-RND_LEN]);

  xor (m[0:size-RND_LEN], g[0:size-RND_LEN], out m[0:size-RND_LEN]);

  data_out = m[0:data_out'size];
}

/***************************************************************************/

// returns 0 if OK, -1 if error.

public int RSA_encrypt (    byte[32]       data_in,
                        out INTEGER        data_out,
                            RSA_PUBLIC_KEY public_key)
{
  INTEGER m;

  pad_message (data_in, public_key.n, out m);

  if (RSA_encrypt_raw (m, out data_out, public_key) != 0)
    return -1;

  return 0;
}

/***************************************************************************/

// returns 0 if OK, -1 if error.

public int RSA_decrypt (    INTEGER        data_in,
                        out byte[32]       data_out,
                            RSA_SECRET_KEY secret_key)
{
  INTEGER m;

  if (RSA_decrypt_raw (data_in, out m, secret_key) != 0)
  {
    clear data_out;
    return -1;
  }

  unpad_message (m, secret_key.n, out data_out);
  return 0;
}

/***************************************************************************/

// returns 0 if OK, -1 if error.

public int RSA_make_signature (    byte[32]       data_in,
                               out INTEGER        data_out,
                                   RSA_SECRET_KEY secret_key)
{
  INTEGER m;

  pad_message (data_in, secret_key.n, out m);

  if (RSA_decrypt_raw (m, out data_out, secret_key) != 0)
    return -1;

  return 0;
}

/***************************************************************************/

// returns 0 if OK, -1 if error

public int RSA_verify_signature (    INTEGER        data_in,
                                 out byte[32]       data_out,
                                     RSA_PUBLIC_KEY public_key)
{
  INTEGER m;

  if (RSA_encrypt_raw (data_in, out m, public_key) != 0)
  {
    clear data_out;
    return -1;
  }

  unpad_message (m, public_key.n, out data_out);
  return 0;
}

/***************************************************************************/

bool is_prime (uint n)
{
  uint i;

  assert (n >= 2);

  for (i=2; i<n; i++)
  {
    if ((n % i) == 0)
      return false;
  }

  return true;
}

/***************************************************************************/

void generate_potential_prime (uint size, out INTEGER p)
{
  byte n[INTEGER_SIZE];

  clear n;
  get_random_number (out n[0:size]);
  n[0]      |= 1;      // set lowest bit
  n[size-1] |= 0xC0;   // set 2 highest bits so that p*q always has same nb bits

  make_large_integer (n[0:size], out p);
}

/***************************************************************************/

// simple test

bool is_composite1 (INTEGER n)
{
  int     i;
  uint    a;
  INTEGER zero, aa, result;

  make_integer (0, out zero);

  a = 1;
  for (i=0; i<16; i++)
  {
    a += 2;
    while (!is_prime(a))
      a += 2;
    make_integer (a, out aa);

    #if debug
      printf ("simple test %u : ", a);
      trace ("simple test %u : ", a);
    #endif

    assert modulo_integer (n, aa, out result) == 0;

    if (compare_integer (result, zero) == 0)   // equals 0
    {
      #if debug
        printf ("nok\n");
        trace ("nok\n");
      #endif
      return true;
    }

    #if debug
      printf ("ok\n");
      trace ("ok\n");
    #endif
  }

  return false;
}

/***************************************************************************/

// fermat test

bool is_composite2 (INTEGER n)
{
  int     i;
  uint    a;
  INTEGER one, n_minus_1, aa, result;

  make_integer (1, out one);
  assert subtract_integer (n, one, out n_minus_1) == 0;

  a = 1;
  for (i=0; i<100; i++)
  {
    a += 2;
    while (!is_prime(a))
      a += 2;
    make_integer (a, out aa);

    #if debug
      printf ("fermat test %u : ", a);
      trace ("fermat test %u : ", a);
    #endif

    assert exponent_modulo_integer (aa, n_minus_1, n, out result) == 0;

    if (compare_integer (result, one) != 0)   // not equal to 1
    {
      #if debug
        printf ("nok\n");
        trace ("nok\n");
      #endif
      return true;
    }

    #if debug
      printf ("ok\n");
      trace ("ok\n");
    #endif
  }

  return false;
}

/***************************************************************************/

// Miller-Rabin primality test

bool is_composite3 (INTEGER n)
{
  INTEGER n_minus_1, one, two, d, q, r, aa, x;
  uint     s, i, a;

  make_integer (1, out one);
  make_integer (2, out two);

  assert subtract_integer (n, one, out n_minus_1) == 0;

  // write n-1 as (2^s * d) with d odd by factoring powers of 2 from n - 1
  copy_integer (n_minus_1, out d);
  s = 0;
  for (;;)
  {
    assert divide_modulo_integer (d, two, out q, out r) == 0;
    if (integer_bit_size (q) == 0)   // q is zero
      return true;   // illegal n

    if (integer_value(r) != 0)   // r is odd : can't further divide by 2
      break;

    copy_integer (q, out d);
    s++;
  }

  assert s >= 1;

#if debug
  printf ("miller-rabin s = %u\n", s);
  trace ("miller-rabin s = %u\n", s);
#endif
#if debug
  {
    char buf[2500];

    integer_to_string (n, out buf);
    printf ("n = %s\n", buf);
    trace ("n = %s\n", buf);

    integer_to_string (d, out buf);
    printf ("d = %s\n", buf);
    trace ("d = %s\n", buf);
  }
#endif

  for (a=2; a<100; a++)
  {
    // pick a random integer a in the range [2, n - 2]
    make_integer (a, out aa);

    #if debug
      printf ("miller-rabin test %u : ", a);
      trace ("miller-rabin test %u : ", a);
    #endif

    // x = a^d mod n
    assert exponent_modulo_integer (aa, d, n, out x) == 0;

    if (compare_integer (x, one) == 0 || compare_integer (x, n_minus_1) == 0)
    {
      #if debug
        printf ("ok\n");
        trace ("ok\n");
      #endif
      continue;
    }

    for (i=1; i<s; i++)  // repeat s-1 times
    {
      // x = x^2 mod n
      assert exponent_modulo_integer (x, two, n, out x) == 0;

      if (compare_integer (x, one) == 0)
      {
        #if debug
          printf ("nok\n");
          trace ("nok\n");
        #endif
        return true;  // composite
      }

      if (compare_integer (x, n_minus_1) == 0)
        break;
    }

    if (i == s)   // above loop finished
    {
      #if debug
        printf ("nok\n");
        trace ("nok\n");
      #endif
      return true;  // composite
    }

    #if debug
      printf ("ok\n");
      trace ("ok\n");
    #endif
  }

  return false;  // probably prime
}

/***************************************************************************/

/*
The standard way to generate big prime numbers is to take a preselected random number of the desired length.

The preselection is done either by test divisions by small prime numbers (up to few hundreds)
 or by sieving out primes up to 10,000 - 1,000,000 considering many prime candidates of the
 form b+2i  (b  big, i  up to few thousands).

Then, apply a Fermat test (best with the base 2  as it can be optimized for speed)

  http://en.wikipedia.org/wiki/Fermat_primality_test

and then to apply a certain number of Miller-Rabin tests
 (depending on the length and the allowed error rate like 2 -100  )

http://en.wikipedia.org/wiki/Miller-Rabin_primality_test

to get a number which is very probably a prime number.

Using large random primes offers security equivalent to that obtained by using strong primes.
Current requirements for strong	primes do not make them any more secure than randomly chosen primes
of the same size.

*/

void generate_prime (uint size, out INTEGER p)
{
  for (;;)
  {
    generate_potential_prime (size, out p);

    // simple test
    if (is_composite1 (p))
    {
      #if debug
        printf ("simple test failed -> retry\n");
        trace ("simple test failed -> retry\n");
      #endif
      continue;
    }

    // fermat test
    if (is_composite2 (p))
    {
      #if debug
        printf ("fermat test failed -> retry\n");
        trace ("fermat test failed -> retry\n");
      #endif
      continue;
    }

    // miller_rabin test
    if (is_composite3 (p))
    {
      #if debug
        printf ("miller_rabin test failed -> retry\n");
        trace ("miller_rabin test failed -> retry\n");
      #endif
      continue;
    }

    break;
  }
}

/***************************************************************************/

// returns 0 if OK, -1 if attempt failed

int generate_keys (    uint           size,
                   out RSA_PUBLIC_KEY public_key,
                   out RSA_SECRET_KEY secret_key)
{
  INTEGER one, p, q, temp, phi, n, ee, d;
  uint    e;

  clear public_key;
  clear secret_key;

  make_integer (1, out one);

  // compute two large primes : p and q
  generate_prime (size / 2, out p);
  generate_prime (size / 2, out q);

  if (compare_integer (p, q) == 0)   // p and q identical
  {
#if debug
    printf ("p and q identical : retry\n");
    trace ("p and q identical : retry\n");
#endif
    return -1;  // failed
  }

  if (compare_integer (p, q) < 0)   // p < q is not ok for computing qinv below
  {
    copy_integer (p, out temp);
    copy_integer (q, out p);
    copy_integer (temp, out q);
  }

#if debug
  {
    char buf[2500];

    integer_to_hex_string (p, out buf);
    printf ("P = 0x%s\n", buf);
    trace ("P = 0x%s\n", buf);

    integer_to_hex_string (q, out buf);
    printf ("Q = 0x%s\n", buf);
    trace ("Q = 0x%s\n", buf);
  }
#endif

  // compute n = p*q
  assert multiply_integer (p, q, out n) == 0;

#if debug
  {
    char buf[2500];

    integer_to_hex_string (n, out buf);
    printf ("N = 0x%s\n", buf);
    trace ("N = 0x%s\n", buf);
  }
#endif

  // compute phi = (p-1)*(q-1)
  assert subtract_integer (p, one, out phi) == 0;
  assert subtract_integer (q, one, out temp) == 0;
  assert multiply_integer (phi, temp, out phi) == 0;

#if debug
  {
    char buf[2500];

    integer_to_hex_string (phi, out buf);
    printf ("PHI = 0x%s\n", buf);
    trace ("PHI = 0x%s\n", buf);
  }
#endif

  /* select 'e' as a prime where pgcd(e,phi)==1 */
#if debug
  e = 65537;  // can be 3
#else
  e = 65537;
#endif

  for (;;)
  {
    if (!is_prime (e))   // it's not a prime
    {
      e += 2;
      continue;
    }

    make_integer (e, out ee);
    pgcd (ee, phi, out temp);
    if (compare_integer (temp, one) != 0)   // not 1
    {
      e += 2;
      continue;
    }

    break;  // leave with result value in 'e' and 'ee'
  }

#if debug
  {
    char buf[2500];

    integer_to_string (ee, out buf);
    printf ("e = %s\n", buf);
    trace ("e = %s\n", buf);
  }
#endif

  // compute 'd'
  assert inverse_multiply_modulo_integer (ee, phi, out d) == 0;

#if debug
  {
    char buf[2500];

    integer_to_hex_string (d, out buf);
    printf ("d = 0x%s\n", buf);
    trace ("d = 0x%s\n", buf);
  }
#endif

  // store keys
  copy_integer (n, out public_key.n);
  public_key.e = e;
  copy_integer (n, out secret_key.n);
  copy_integer (d, out secret_key.d);
  copy_integer (p, out secret_key.p);
  copy_integer (q, out secret_key.q);
  assert subtract_integer (p, one, out temp) == 0;
  assert modulo_integer (d, temp, out secret_key.dp) == 0;
  assert subtract_integer (q, one, out temp) == 0;
  assert modulo_integer (d, temp, out secret_key.dq) == 0;
  assert inverse_multiply_modulo_integer (q, p, out secret_key.qinv) == 0;

#if debug
  {
    char buf[2500];

    integer_to_hex_string (secret_key.dp, out buf);
    printf ("dp = 0x%s\n", buf);
    trace ("dp = 0x%s\n", buf);
    
    integer_to_hex_string (secret_key.dq, out buf);
    printf ("dq = 0x%s\n", buf);
    trace ("dq = 0x%s\n", buf);
    
    integer_to_hex_string (secret_key.qinv, out buf);
    printf ("qinv = 0x%s\n", buf);
    trace ("qinv = 0x%s\n", buf);
  }
#endif

  return 0;
}

/***************************************************************************/

// returns 0 if OK, -1 if tests failed

int check_keys (RSA_PUBLIC_KEY public_key,
                RSA_SECRET_KEY secret_key)
{
  INTEGER temp;
  uint    i;

  /* final test : M ^ E ^ D mod N = M */
  for (i=1100; i<1348; i+=7)
  {
    make_integer (i, out temp);

#if debug
    printf ("(a)test1 for i = %u\n", i);
    trace ("(a)test1 for i = %u\n", i);
#endif

    assert RSA_encrypt_raw (temp, out temp, public_key) == 0;

#if debug
    printf ("(b)test1\n");
    trace ("(b)test1\n");
#endif

    assert RSA_decrypt_raw (temp, out temp, secret_key) == 0;

#if debug
    printf ("(c)test1\n");
    trace ("(c)test1\n");
#endif

    if (compare_integer_int (temp, i) != 0)
    {
#if debug
      printf ("nok - test1 failed\n");
      trace (" nok - test1 failed\n");
#endif
      return -1;
    }

#if debug
    printf ("ok\n");
    trace ("ok\n");
#endif
  }

  /* final test 2 */
  {
    byte inp[32], outp[32];
    clear inp;

    for (i=1101; i<1248; i+=7)
    {
      inp[i%32] = (byte)i;

      assert RSA_encrypt (inp, out temp, public_key) == 0;
      assert RSA_decrypt (temp, out outp, secret_key) == 0;
      if (memcmp (inp, outp) != 0)
      {
#if debug
        printf ("test2 failed for i = %u\n", i);
        trace ("test2 failed for i = %u\n", i);
#endif
        return -1;
      }
    }
  }

  /* final test 3 */
  {
    byte inp[32], outp[32];
    clear inp;

    for (i=1102; i<1248; i+=7)
    {
      inp[i%32] = (byte)i;

      assert RSA_make_signature (inp, out temp, secret_key) == 0;
      assert RSA_verify_signature (temp, out outp, public_key) == 0;
      if (memcmp (inp, outp) != 0)
      {
#if debug
        printf ("test3 failed for i = %u\n", i);
        trace ("test3 failed for i = %u\n", i);
#endif
        return -1;
      }
    }
  }

  return 0;
}

/***************************************************************************/

public void RSA_generate_keys (    uint           bit_size,
                               out RSA_PUBLIC_KEY public_key,
                               out RSA_SECRET_KEY secret_key)
{
#if debug
  assert (bit_size >= 16 && bit_size <= INTEGER_SIZE*8 && (bit_size & 7) == 0);
  printf ("DEBUG MODE for bit_size %u\n", bit_size);
  trace ("DEBUG MODE for bit_size %u\n", bit_size);
#else
  assert (bit_size >= 1024 && bit_size <= INTEGER_SIZE*8 && (bit_size & 511) == 0);
#endif

  for (;;)
  {
    if (generate_keys (bit_size >> 3, out public_key, out secret_key) == 0 &&
        check_keys (public_key, secret_key) == 0)
      break;
  }
  
#if debug
  printf ("keys generated : ok\n");
  trace ("keys generated : ok\n");
#endif
}

/***************************************************************************/

#if debug
void main()
{
  RSA_PUBLIC_KEY public_key;
  RSA_SECRET_KEY secret_key;
  arm_exception_handler();
  delete_file ("rsa.tra");
  open_trace ("rsa.tra", max_file_size => 64*1024*1024, date=> true, time => true, msec => true);
  RSA_generate_keys (bit_size => 2048, out public_key, out secret_key);
  _unused public_key;
  _unused secret_key;
}
#endif

/***************************************************************************/
