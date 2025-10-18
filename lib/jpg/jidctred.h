
use jpeglib, jmorecfg;

#begin unsafe

void
jpeg_idct_4x4 (j_decompress_ptr cinfo, jpeg_component_info * compptr,
	       JCOEFPTR coef_block,
	       JSAMPARRAY output_buf, JDIMENSION output_col);

void
jpeg_idct_2x2 (j_decompress_ptr cinfo, jpeg_component_info * compptr,
	       JCOEFPTR coef_block,
	       JSAMPARRAY output_buf, JDIMENSION output_col);

void
jpeg_idct_1x1 (j_decompress_ptr cinfo, jpeg_component_info * compptr,
	       JCOEFPTR coef_block,
	       JSAMPARRAY output_buf, JDIMENSION output_col);

#end unsafe
