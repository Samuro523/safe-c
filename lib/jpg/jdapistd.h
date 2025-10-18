

use jpeglib, jmorecfg;

#begin unsafe

boolean
jpeg_start_decompress (j_decompress_ptr cinfo);

JDIMENSION
jpeg_read_scanlines (j_decompress_ptr cinfo, JSAMPARRAY scanlines,
		     JDIMENSION max_lines);

JDIMENSION
jpeg_read_raw_data (j_decompress_ptr cinfo, JSAMPIMAGE data,
		    JDIMENSION max_lines);


boolean
jpeg_start_output (j_decompress_ptr cinfo, int scan_number);


boolean
jpeg_finish_output (j_decompress_ptr cinfo);


#end unsafe

