
use jpeglib, jmorecfg;

#begin unsafe

boolean jpeg_resync_to_restart (j_decompress_ptr cinfo, int desired);

void jinit_marker_reader (j_decompress_ptr cinfo);

#end unsafe
