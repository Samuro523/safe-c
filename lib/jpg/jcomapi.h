

use jpeglib;

#begin unsafe


void
jpeg_abort (j_common_ptr cinfo);

void
jpeg_destroy (j_common_ptr cinfo);

JQUANT_TBL *
jpeg_alloc_quant_table (j_common_ptr cinfo);

JHUFF_TBL *
jpeg_alloc_huff_table (j_common_ptr cinfo);

#end unsafe
