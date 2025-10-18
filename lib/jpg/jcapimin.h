
use jpeglib, jmorecfg;

#begin unsafe

int
jpeg_CreateCompress (j_compress_ptr cinfo, int version, size_t structsize);

void
jpeg_suppress_tables (j_compress_ptr cinfo, boolean suppress);

int
jpeg_finish_compress (j_compress_ptr cinfo);


void
jpeg_write_marker (j_compress_ptr cinfo, int marker,
		   JOCTET *dataptr, uint datalen);

void
jpeg_write_m_header (j_compress_ptr cinfo, int marker, uint datalen);


void
jpeg_write_m_byte (j_compress_ptr cinfo, int val);


#end unsafe

