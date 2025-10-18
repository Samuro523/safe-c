
// blob2.2

use blob;

public void init_blobs ()
{
  blob_create (out g_blob_code);
  blob_create (out g_blob_data);
}

//---------------------------------------------------------------------

public uint4 current_RIP ()
{
  return (uint)blob_index (g_blob_code);
}

//---------------------------------------------------------------------
