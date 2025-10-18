


use jpeglib, jmorecfg;

#begin unsafe


int
jpeg_start_compress (j_compress_ptr cinfo, boolean write_all_tables);


JDIMENSION
jpeg_write_scanlines (j_compress_ptr cinfo, JSAMPARRAY scanlines,
		      JDIMENSION num_lines);



JDIMENSION
jpeg_write_raw_data (j_compress_ptr cinfo, JSAMPIMAGE data,
		     JDIMENSION num_lines);




#end unsafe

