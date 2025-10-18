
use jpeglib;
use ../stream;

#begin unsafe

void jpeg_stdio_dest (j_compress_ptr cinfo, ref WRITE_STREAM stream);

#end unsafe
