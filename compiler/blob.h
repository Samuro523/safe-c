
// blob.h : binary large object

struct BLOB;   // binary large object

void blob_create (out BLOB blob);
void blob_close (ref BLOB blob);

// write in LSB-to-MSB order
void blob_put_byte     (ref BLOB blob, byte  value);
void blob_put_int2     (ref BLOB blob, int2  value);
void blob_put_uint2    (ref BLOB blob, uint2 value);
void blob_put_int4     (ref BLOB blob, int4  value);
void blob_put_uint4    (ref BLOB blob, uint4 value);
void blob_put_int8     (ref BLOB blob, int8  value);
void blob_put_sequence (ref BLOB blob, byte[] seq);

// write in LSB-to-MSB order,  4 or 8 bytes, depending on goptions.address_size
void blob_put_address  (ref BLOB blob, int8 address);

int blob_index (BLOB blob);
void blob_set_index (ref BLOB blob, int index);
void blob_align (ref BLOB blob, uint4 mod);  // ex: mod = PAGE

#begin unsafe
  byte* blob_ptr (BLOB blob);
  int blob_size (BLOB blob);
#end unsafe


void blob_insert_bytes (ref BLOB blob, int pos, int nb_bytes);
void blob_remove_bytes (ref BLOB blob, int pos, int nb_bytes);
