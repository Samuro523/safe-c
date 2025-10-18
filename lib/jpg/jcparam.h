
use jpeglib, jmorecfg;

#begin unsafe


void
jpeg_set_defaults (j_compress_ptr cinfo);


void
jpeg_add_quant_table (j_compress_ptr cinfo, int which_tbl,
		      uint *basic_table,
		      int scale_factor, boolean force_baseline);

void
jpeg_set_linear_quality (j_compress_ptr cinfo, int scale_factor,
			 boolean force_baseline);


int
jpeg_quality_scaling (int quality);

void
jpeg_set_quality (j_compress_ptr cinfo, int quality, boolean force_baseline);


void
jpeg_default_colorspace (j_compress_ptr cinfo);

void
jpeg_set_colorspace (j_compress_ptr cinfo, J_COLOR_SPACE colorspace);

void
jpeg_simple_progression (j_compress_ptr cinfo);




#end unsafe

