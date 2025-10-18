
use jpeglib, jmorecfg;


#begin unsafe

public JSAMPLE *IDCT_range_limit (j_decompress_ptr cinfo)
{
  return ((cinfo)->sample_range_limit + CENTERJSAMPLE);
}

public int DESCALE (int x, int n)
{
  return ((x) + (ONE << ((n)-1))) >> n;
}

#end unsafe
