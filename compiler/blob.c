
// blob.c : binary large object

use goptions;

struct BLOB   // binary large object
{
  byte[]^ b;
  int     index;
  int     size;
}

public void blob_create (out BLOB blob)
{
  clear blob;
  blob.b = new byte[1024];   // 1 Kbyte
}

public void blob_close (ref BLOB blob)
{
  free blob.b;
  clear blob;
}

void probe (ref BLOB blob, int pos)
{
  while (pos > blob.b^'length)
  {
    int     len = blob.b^'length;
    byte[]^ old = blob.b;
    blob.b = new byte[2*len];
    blob.b^[0 : len] = old^;
    free old;
  }
}

public void blob_put_sequence (ref BLOB blob, byte[] seq)
{
  probe (ref blob, pos => blob.index + seq'length);

  blob.b^[blob.index : seq'length] = seq;
  blob.index += seq'length;

  if (blob.size < blob.index)
    blob.size = blob.index;
}

public void blob_put_byte (ref BLOB blob, byte value)
{
  blob_put_sequence (ref blob, (byte)value);
}

public void blob_put_int2 (ref BLOB blob, int2 value)
{
  blob_put_sequence (ref blob, (int2)value);
}

public void blob_put_uint2 (ref BLOB blob, uint2 value)
{
  blob_put_sequence (ref blob, (uint2)value);
}

public void blob_put_int4 (ref BLOB blob, int4 value)
{
  blob_put_sequence (ref blob, (int4)value);
}

public void blob_put_uint4 (ref BLOB blob, uint4 value)
{
  blob_put_sequence (ref blob, (uint4)value);
}

public void blob_put_int8 (ref BLOB blob, int8 value)
{
  blob_put_sequence (ref blob, (int8)value);
}


// write in LSB-to-MSB order,  4 or 8 bytes, depending on goptions.address_size
public void blob_put_address (ref BLOB blob, int8 address)
{
  if (goptions.address_size == 8)
    blob_put_sequence (ref blob, (int8)address);
  else if (goptions.address_size == 4)
    blob_put_sequence (ref blob, (uint4)address);
  else
    abort;
}

public void blob_set_index (ref BLOB blob, int index)
{
  blob.index = index;
}


public void blob_align (ref BLOB blob, uint4 mod)  // ex: mod = PAGE
{
  while (((uint)blob.index & (mod-1)) > 0)
    blob_put_byte (ref blob, 0);
}


public int blob_index (BLOB blob)
{
  return blob.index;
}

#begin unsafe
public byte* blob_ptr (BLOB blob)
{
  return &blob.b^[0];
}
#end unsafe

public int blob_size (BLOB blob)
{
  return blob.size;
}


#begin unsafe
public void blob_insert_bytes (ref BLOB blob, int pos, int nb_bytes)
{
  int len = blob.size - pos;
  
  probe (ref blob, pos => blob.size + nb_bytes);

  blob_ptr (blob)[pos+nb_bytes : len] = blob_ptr (blob)[pos : len];
  clear blob_ptr (blob)[pos : nb_bytes];
  blob.size += nb_bytes;
}
#end unsafe


#begin unsafe
public void blob_remove_bytes (ref BLOB blob, int pos, int nb_bytes)
{
  int len = blob.size - (pos + nb_bytes);

  blob_ptr (blob)[pos : len] = blob_ptr (blob)[pos+nb_bytes : len];
  clear blob_ptr (blob)[blob.size : nb_bytes];
  blob.size -= nb_bytes;
}
#end unsafe

