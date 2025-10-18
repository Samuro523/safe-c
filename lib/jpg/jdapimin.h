

use jpeglib, jmorecfg;

#begin unsafe

int
jpeg_CreateDecompress (j_decompress_ptr cinfo, int version, size_t structsize);

void
jpeg_destroy_decompress (j_decompress_ptr cinfo);

void
jpeg_abort_decompress (j_decompress_ptr cinfo);

void
default_decompress_parms (j_decompress_ptr cinfo);

int
jpeg_read_header (j_decompress_ptr cinfo, boolean require_image);

int
jpeg_consume_input (j_decompress_ptr cinfo);

#end unsafe
