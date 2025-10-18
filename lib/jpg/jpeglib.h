/*
 * jpeglib.h
 *
 * Copyright (C) 1991-1998, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file defines the application interface for the JPEG library.
 * Most applications using the library need only include this file,
 * and perhaps jerror.h if they want to know the exact error codes.
 */


/*
 * First we include the configuration files that record how this
 * installation of the JPEG library is set up.  jconfig.h can be
 * generated automatically for many systems.  jmorecfg.h contains
 * manual configuration options that most people need not worry about.
 */

use jmorecfg;   /* seldom changed options */


#begin unsafe

/* Version ID for the JPEG library.
 * Might be useful for tests like "#if JPEG_LIB_VERSION >= 60".
 */

const int JPEG_LIB_VERSION  = 62; /* Version 6b */


/* Various constants determining the sizes of things.
 * All of these are specified by the JPEG standard, so don't change them
 * if you want to be compatible.
 */

const int DCTSIZE     =  8; /* The basic DCT block is 8x8 samples */
const int DCTSIZE2      = 64; /* DCTSIZE squared; # of elements in a block */
const int NUM_QUANT_TBLS    =  4; /* Quantization tables are numbered 0..3 */
const int NUM_HUFF_TBLS     =  4; /* Huffman tables are numbered 0..3 */
const int NUM_ARITH_TBLS    =  16;  /* Arith-coding tables are numbered 0..15 */
const int MAX_COMPS_IN_SCAN =  4; /* JPEG limit on # of components in one scan */
const int MAX_SAMP_FACTOR   =  4; /* JPEG limit on sampling factors */
/* Unfortunately, some bozo at Adobe saw no reason to be bound by the standard;
 * the PostScript DCT filter can emit files with many more than 10 blocks/MCU.
 * If you happen to run across such a file, you can up D_MAX_BLOCKS_IN_MCU
 * to handle it.  We even let you do this from the jconfig.h file.  However,
 * we strongly discourage changing C_MAX_BLOCKS_IN_MCU; just because Adobe
 * sometimes emits noncompliant files doesn't mean you should too.
 */
const int C_MAX_BLOCKS_IN_MCU   = 10; /* compressor's limit on blocks per MCU */
const int D_MAX_BLOCKS_IN_MCU   = 10; /* decompressor's limit on blocks per MCU */


/* Data structures for images (arrays of samples and of DCT coefficients).
 * On 80x86 machines, the image arrays are too big for near pointers,
 * but the pointer arrays can fit in near memory.
 */

typedef JSAMPLE    *JSAMPROW; /* ptr to one image row of pixel samples. */
typedef JSAMPROW   *JSAMPARRAY; /* ptr to some rows (a 2-D sample array) */
typedef JSAMPARRAY *JSAMPIMAGE; /* a 3-D sample array: top index is color */

typedef JCOEF JBLOCK[DCTSIZE2]; /* one block of coefficients */
typedef JBLOCK  *JBLOCKROW; /* pointer to one row of coefficient blocks */
typedef JBLOCKROW *JBLOCKARRAY;   /* a 2-D array of coefficient blocks */
typedef JBLOCKARRAY *JBLOCKIMAGE; /* a 3-D array of coefficient blocks */

typedef JCOEF  *JCOEFPTR; /* useful in a couple of places */


/* Types for JPEG compression parameters and working tables. */


/* DCT coefficient quantization tables. */

struct JQUANT_TBL {
  /* This array gives the coefficient quantizers in natural array order
   * (not the zigzag order in which they are stored in a JPEG DQT marker).
   * CAUTION: IJG versions prior to v6a kept this array in zigzag order.
   */
  UINT16 quantval[DCTSIZE2];  /* quantization step for each coefficient */
  /* This field is used only during compression.  It's initialized FALSE when
   * the table is created, and set TRUE when it's been output to the file.
   * You could suppress output of a table by setting this to TRUE.
   * (See jpeg_suppress_tables for an example.)
   */
  boolean sent_table;   /* TRUE when table has been output */
}


/* Huffman coding tables. */

struct  JHUFF_TBL {
  /* These two fields directly represent the contents of a JPEG DHT marker */
  UINT8 bits[17];   /* bits[k] = # of symbols with codes of */
        /* length k bits; bits[0] is unused */
  UINT8 huffval[256];   /* The symbols, in order of incr code length */
  /* This field is used only during compression.  It's initialized FALSE when
   * the table is created, and set TRUE when it's been output to the file.
   * You could suppress output of a table by setting this to TRUE.
   * (See jpeg_suppress_tables for an example.)
   */
  boolean sent_table;   /* TRUE when table has been output */
}


/* Basic info about one component (color channel). */

struct  jpeg_component_info {
  /* These values are fixed over the whole image. */
  /* For compression, they must be supplied by parameter setup; */
  /* for decompression, they are read from the SOF marker. */
  int component_id;   /* identifier for this component (0..255) */
  int component_index;    /* its index in SOF or cinfo->comp_info[] */
  int h_samp_factor;    /* horizontal sampling factor (1..4) */
  int v_samp_factor;    /* vertical sampling factor (1..4) */
  int quant_tbl_no;   /* quantization table selector (0..3) */
  /* These values may vary between scans. */
  /* For compression, they must be supplied by parameter setup; */
  /* for decompression, they are read from the SOS marker. */
  /* The decompressor output side may not use these variables. */
  int dc_tbl_no;    /* DC entropy table selector (0..3) */
  int ac_tbl_no;    /* AC entropy table selector (0..3) */
  
  /* Remaining fields should be treated as private by applications. */
  
  /* These values are computed during compression or decompression startup: */
  /* Component's size in DCT blocks.
   * Any dummy blocks added to complete an MCU are not counted; therefore
   * these values do not depend on whether a scan is interleaved or not.
   */
  JDIMENSION width_in_blocks;
  JDIMENSION height_in_blocks;
  /* Size of a DCT block in samples.  Always DCTSIZE for compression.
   * For decompression this is the size of the output from one DCT block,
   * reflecting any scaling we choose to apply during the IDCT step.
   * Values of 1,2,4,8 are likely to be supported.  Note that different
   * components may receive different IDCT scalings.
   */
  int DCT_scaled_size;
  /* The downsampled dimensions are the component's actual, unpadded number
   * of samples at the main buffer (preprocessing/compression interface), thus
   * downsampled_width = ceil(image_width * Hi/Hmax)
   * and similarly for height.  For decompression, IDCT scaling is included, so
   * downsampled_width = ceil(image_width * Hi/Hmax * DCT_scaled_size/DCTSIZE)
   */
  JDIMENSION downsampled_width;  /* actual width in samples */
  JDIMENSION downsampled_height; /* actual height in samples */
  /* This flag is used only for decompression.  In cases where some of the
   * components will be ignored (eg grayscale output from YCbCr image),
   * we can skip most computations for the unused components.
   */
  boolean component_needed; /* do we need the value of this component? */

  /* These values are computed before starting a scan of the component. */
  /* The decompressor output side may not use these variables. */
  int MCU_width;    /* number of blocks per MCU, horizontally */
  int MCU_height;   /* number of blocks per MCU, vertically */
  int MCU_blocks;   /* MCU_width * MCU_height */
  int MCU_sample_width;   /* MCU width in samples, MCU_width*DCT_scaled_size */
  int last_col_width;   /* # of non-dummy blocks across in last MCU */
  int last_row_height;    /* # of non-dummy blocks down in last MCU */

  /* Saved quantization table for component; NULL if none yet saved.
   * See jdinput.c comments about the need for this information.
   * This field is currently used only for decompression.
   */
  JQUANT_TBL * quant_table;

  /* Private per-component storage for DCT or IDCT subsystem. */
  byte* dct_table;
}


/* The script for encoding a multiple-scan file is an array of these: */

struct  jpeg_scan_info {
  int comps_in_scan;    /* number of components encoded in this scan */
  int component_index[MAX_COMPS_IN_SCAN]; /* their SOF/comp_info[] indexes */
  int Ss, Se;     /* progressive JPEG spectral selection parms */
  int Ah, Al;     /* progressive JPEG successive approx. parms */
}

/* The decompressor can save APPn and COM markers in a list of these: */


struct jpeg_marker_struct {
  jpeg_marker_struct * next;  /* next in list, or NULL */
  UINT8 marker;     /* marker code: JPEG_COM, or JPEG_APP0+n */
  uint original_length; /* # bytes of data in the file */
  uint data_length; /* # bytes of data saved at data[] */
  JOCTET  * data;   /* the data contained in the marker */
  /* the marker length word is not counted in data_length or original_length */
}

typedef jpeg_marker_struct  * jpeg_saved_marker_ptr;

/* Known color spaces. */

enum  J_COLOR_SPACE {
  JCS_UNKNOWN,   /* error/unspecified */
  JCS_GRAYSCALE, /* monochrome */
  JCS_RGB,       /* red/green/blue */
  JCS_YCbCr,     /* Y/Cb/Cr (also known as YUV) */
  JCS_CMYK,      /* C/M/Y/K */
  JCS_YCCK,      /* Y/Cb/Cr/K */
  JCS_RGBA,      /* red/green/blue/alpha (non-standard) */
  JCS_YCbCrA,    /* Y/Cb/Cr/A (same as JCS_YCbCr with alpha channel) (non-standard) */
};

/* DCT/IDCT algorithm options. */

enum  J_DCT_METHOD {
  JDCT_ISLOW,   /* slow but accurate integer algorithm */
  JDCT_IFAST,   /* faster, less accurate integer method */
  JDCT_FLOAT    /* floating-point: accurate, fast on fast HW */
};


/* Dithering options for decompression. */

enum  J_DITHER_MODE {
  JDITHER_NONE,   /* no dithering */
  JDITHER_ORDERED,  /* simple ordered dither */
  JDITHER_FS    /* Floyd-Steinberg error diffusion dither */
};



/* "Object" declarations for JPEG modules that may be supplied or called
 * directly by the surrounding application.
 * As with all objects in the JPEG library, these structs only define the
 * publicly visible methods and state variables of a module.  Additional
 * private fields may exist after the public ones.
 */


/* Error handler object */

const int JMSG_LENGTH_MAX = 200;  /* recommended size of format_message buffer */
const int JMSG_STR_PARM_MAX = 80;

typedef j_common_ptr;
typedef j_compress_ptr;
typedef j_decompress_ptr;

struct jpeg_error_mgr {
}


/* Progress monitor object */

typedef void PROGRESS_MONITOR (j_common_ptr cinfo);

struct jpeg_progress_mgr {
  PROGRESS_MONITOR progress_monitor;

  int pass_counter;   /* work units completed in this pass */
  int pass_limit;   /* total number of work units in this pass */
  int completed_passes;   /* passes completed so far */
  int total_passes;   /* total number of passes expected */
}


/* Data destination object for compression */

typedef void INIT_DESTINATION (j_compress_ptr cinfo);
typedef boolean EMPTY_OUTPUT_BUFFER (j_compress_ptr cinfo);
typedef boolean ITERM_DESTINATION (j_compress_ptr cinfo);

struct jpeg_destination_mgr {
  JOCTET * next_output_byte;  /* => next byte to write in buffer */
  size_t free_in_buffer;  /* # of byte spaces remaining in buffer */

  INIT_DESTINATION init_destination;
  EMPTY_OUTPUT_BUFFER empty_output_buffer;
  ITERM_DESTINATION iterm_destination;
}


/* Data source object for decompression */

typedef void     INIT_SOURCE (j_decompress_ptr cinfo);
typedef boolean  FILL_INPUT_BUFFER (j_decompress_ptr cinfo);
typedef void     SKIP_INPUT_DATA (j_decompress_ptr cinfo, int num_bytes);
typedef boolean  RESYNC_TO_RESTART (j_decompress_ptr cinfo, int desired);
typedef void     TERM_SOURCE (j_decompress_ptr cinfo);

struct jpeg_source_mgr {
  JOCTET * next_input_byte; /* => next byte to read from buffer */
  size_t bytes_in_buffer; /* # of bytes remaining in buffer */

  INIT_SOURCE init_source;
  FILL_INPUT_BUFFER fill_input_buffer;
  SKIP_INPUT_DATA skip_input_data;
  RESYNC_TO_RESTART resync_to_restart;
  TERM_SOURCE term_source;
}


/* Memory manager object.
 * Allocates "small" objects (a few K total), "large" objects (tens of K),
 * and "really big" objects (virtual arrays with backing store if needed).
 * The memory manager does not allow individual objects to be freed; rather,
 * each created object is assigned to a pool, and whole pools can be freed
 * at once.  This is faster and more convenient than remembering exactly what
 * to free, especially where malloc()/free() are not too speedy.
 * NB: alloc routines never return NULL.  They exit to error_exit if not
 * successful.
 */

const int JPOOL_PERMANENT = 0;  /* lasts until master record is destroyed */
const int JPOOL_IMAGE         = 1;  /* lasts until done with image/datastream */
const int JPOOL_NUMPOOLS  = 2;



/*
 * We allocate objects from "pools", where each pool is gotten with a single
 * request to jpeg_get_small() or jpeg_get_large().  There is no per-object
 * overhead within a pool, except for alignment padding.  Each pool has a
 * header with a link to the next pool of the same class.
 * Small and large pool headers are identical except that the latter's
 * link pointer must be FAR on 80x86 machines.
 * Notice that the "real" header fields are union'ed with a dummy ALIGN_TYPE
 * field.  This forces the compiler to make SIZEOF(small_pool_hdr) a multiple
 * of the alignment requirement of ALIGN_TYPE.
 */

typedef double ALIGN_TYPE;

typedef small_pool_struct;

struct small_hdr {
  small_pool_struct * next; /* next in list of pools */
  size_t bytes_used;    /* how many bytes already used within pool */
  size_t bytes_left;    /* bytes still available in this pool */
  byte[]^ this;
}

struct small_pool_struct {
  small_hdr hdr;
//  ALIGN_TYPE dummy;   /* included in union to ensure alignment */
}

typedef small_pool_struct small_pool_hdr;

typedef small_pool_struct * small_pool_ptr;


typedef large_pool_struct;

struct large_hdr {
  large_pool_struct * next; /* next in list of pools */
  size_t bytes_used;    /* how many bytes already used within pool */
  size_t bytes_left;    /* bytes still available in this pool */
  byte[]^ this;
}

struct large_pool_struct {
  large_hdr hdr;
//  ALIGN_TYPE dummy;   /* included in union to ensure alignment */
}

typedef large_pool_struct large_pool_hdr;

typedef large_pool_struct * large_pool_ptr;


typedef backing_store_ptr;


struct backing_store_struct
{
}

typedef backing_store_struct backing_store_info;
typedef backing_store_struct * backing_store_ptr;


/*
 * The control blocks for virtual arrays.
 * Note that these blocks are allocated in the "small" pool area.
 * System-dependent info for the associated backing store (if any) is hidden
 * inside the backing_store_info struct.
 */

struct jvirt_sarray_control {
  JSAMPARRAY mem_buffer;  /* => the in-memory buffer */
  JDIMENSION rows_in_array; /* total virtual array height */
  JDIMENSION samplesperrow; /* width of array (and of memory buffer) */
  JDIMENSION maxaccess;   /* max rows accessed by access_virt_sarray */
  JDIMENSION rows_in_mem; /* height of memory buffer */
  JDIMENSION rowsperchunk;  /* allocation chunk size in mem_buffer */
  JDIMENSION cur_start_row; /* first logical row # in the buffer */
  JDIMENSION first_undef_row; /* row # of first uninitialized row */
  boolean pre_zero;   /* pre-zero mode requested? */
  boolean dirty;    /* do current buffer contents need written? */
  boolean b_s_open;   /* is backing-store data valid? */
  jvirt_sarray_control * next;  /* link to next virtual sarray control block */
  backing_store_info b_s_info;  /* System-dependent control info */
}

struct jvirt_barray_control {
  JBLOCKARRAY mem_buffer; /* => the in-memory buffer */
  JDIMENSION rows_in_array; /* total virtual array height */
  JDIMENSION blocksperrow;  /* width of array (and of memory buffer) */
  JDIMENSION maxaccess;   /* max rows accessed by access_virt_barray */
  JDIMENSION rows_in_mem; /* height of memory buffer */
  JDIMENSION rowsperchunk;  /* allocation chunk size in mem_buffer */
  JDIMENSION cur_start_row; /* first logical row # in the buffer */
  JDIMENSION first_undef_row; /* row # of first uninitialized row */
  boolean pre_zero;   /* pre-zero mode requested? */
  boolean dirty;    /* do current buffer contents need written? */
  boolean b_s_open;   /* is backing-store data valid? */
  jvirt_barray_control * next;  /* link to next virtual barray control block */
  backing_store_info b_s_info;  /* System-dependent control info */
}


typedef jvirt_sarray_control * jvirt_sarray_ptr;
typedef jvirt_barray_control * jvirt_barray_ptr;


typedef byte *ALLOC_SMALL (j_common_ptr cinfo, int pool_id,
        size_t sizeofobject);
typedef byte *ALLOC_LARGE (j_common_ptr cinfo, int pool_id,
             size_t sizeofobject);
typedef JSAMPARRAY ALLOC_SARRAY (j_common_ptr cinfo, int pool_id,
             JDIMENSION samplesperrow,
             JDIMENSION numrows);
typedef JBLOCKARRAY ALLOC_BARRAY (j_common_ptr cinfo, int pool_id,
              JDIMENSION blocksperrow,
              JDIMENSION numrows);
typedef jvirt_sarray_ptr REQUEST_VIRT_SARRAY (j_common_ptr cinfo,
              int pool_id,
              boolean pre_zero,
              JDIMENSION samplesperrow,
              JDIMENSION numrows,
              JDIMENSION maxaccess);
typedef jvirt_barray_ptr REQUEST_VIRT_BARRAY (j_common_ptr cinfo,
              int pool_id,
              boolean pre_zero,
              JDIMENSION blocksperrow,
              JDIMENSION numrows,
              JDIMENSION maxaccess);
typedef void       REALIZE_VIRT_ARRAYS (j_common_ptr cinfo);
typedef JSAMPARRAY ACCESS_VIRT_SARRAY (j_common_ptr cinfo,
             jvirt_sarray_ptr ptr,
             JDIMENSION start_row,
             JDIMENSION num_rows,
             boolean writable);
typedef JBLOCKARRAY ACCESS_VIRT_BARRAY (j_common_ptr cinfo,
              jvirt_barray_ptr ptr,
              JDIMENSION start_row,
              JDIMENSION num_rows,
              boolean writable);
typedef void FREE_POOL (j_common_ptr cinfo, int pool_id);
typedef void SELF_DESTRUCT (j_common_ptr cinfo);


struct jpeg_memory_mgr {
  /* Method pointers */
  ALLOC_SMALL alloc_small;
  ALLOC_LARGE alloc_large;
  ALLOC_SARRAY alloc_sarray;
  ALLOC_BARRAY alloc_barray;
  REQUEST_VIRT_SARRAY request_virt_sarray;
  REQUEST_VIRT_BARRAY request_virt_barray;
  REALIZE_VIRT_ARRAYS realize_virt_arrays;
  ACCESS_VIRT_SARRAY access_virt_sarray;
  ACCESS_VIRT_BARRAY access_virt_barray;
  FREE_POOL free_pool;
  SELF_DESTRUCT self_destruct;

  /* Limit on memory allocation for this JPEG object.  (Note that this is
   * merely advisory, not a guaranteed maximum; it only affects the space
   * used for virtual-array buffers.)  May be changed by outer application
   * after creating the JPEG object.
   */
  int max_memory_to_use;

  /* Maximum allocation request accepted by alloc_large. */
  int max_alloc_chunk;
}


/*
 * Here is the full definition of a memory manager object.
 */

struct  my_memory_mgr {
  jpeg_memory_mgr pub;  /* public fields */

  /* Each pool identifier (lifetime class) names a linked list of pools. */
  small_pool_ptr small_list[JPOOL_NUMPOOLS];
  large_pool_ptr large_list[JPOOL_NUMPOOLS];

  /* Since we only have one lifetime class of virtual arrays, only one
   * linked list is necessary (for each datatype).  Note that the virtual
   * array control blocks being linked together are actually stored somewhere
   * in the small-pool list.
   */
  jvirt_sarray_ptr virt_sarray_list;
  jvirt_barray_ptr virt_barray_list;

  /* This counts total space obtained from jpeg_get_small/large */
  int total_space_allocated;

  /* alloc_sarray and alloc_barray set this value for use by virtual
   * array routines.
   */
  JDIMENSION last_rowsperchunk; /* from most recent alloc_sarray/barray */
}

typedef my_memory_mgr * my_mem_ptr;


/* Routine signature for application-supplied marker processing methods.
 * Need not pass marker code since it is stored in cinfo->unread_marker.
 */
typedef boolean jpeg_marker_parser_method (j_decompress_ptr cinfo);




/* Declarations for both compression & decompression */

enum J_BUF_MODE {     /* Operating modes for buffer controllers */
  JBUF_PASS_THRU,   /* Plain stripwise operation */
  /* Remaining modes require a full-image buffer to have been created */
  JBUF_SAVE_SOURCE, /* Run source subobject only, save output */
  JBUF_CRANK_DEST,  /* Run dest subobject only, using saved data */
  JBUF_SAVE_AND_PASS  /* Run both subobjects, save output */
};

/* Values of global_state field (jdapi.c has some dependencies on ordering!) */
const int CSTATE_START      = 100;  /* after create_compress */
const int CSTATE_SCANNING   = 101;  /* start_compress done, write_scanlines OK */
const int CSTATE_RAW_OK     = 102;  /* start_compress done, write_raw_data OK */
const int CSTATE_WRCOEFS    = 103;  /* jpeg_write_coefficients done */
const int DSTATE_START      = 200;  /* after create_decompress */
const int DSTATE_INHEADER   = 201;  /* reading header markers, no SOS yet */
const int DSTATE_READY      = 202;  /* found SOS, ready for start_decompress */
const int DSTATE_PRELOAD  = 203;  /* reading multiscan file in start_decompress*/
const int DSTATE_PRESCAN  = 204;  /* performing dummy pass for 2-pass quant */
const int DSTATE_SCANNING = 205;  /* start_decompress done, read_scanlines OK */
const int DSTATE_RAW_OK = 206;  /* start_decompress done, read_raw_data OK */
const int DSTATE_BUFIMAGE = 207;  /* expecting jpeg_start_output */
const int DSTATE_BUFPOST  = 208;  /* looking for SOS/EOI in jpeg_finish_output */
const int DSTATE_RDCOEFS  = 209;  /* reading file in jpeg_read_coefficients */
const int DSTATE_STOPPING = 210;  /* looking for EOI in jpeg_finish_decompress */


/* Declarations for compression modules */

typedef int IPREPARE_FOR_PASS (j_compress_ptr cinfo);
typedef void PASS_STARTUP (j_compress_ptr cinfo);
typedef void FINISH_PASS (j_compress_ptr cinfo);

/* Master control module */
struct jpeg_comp_master {
  IPREPARE_FOR_PASS iprepare_for_pass;
  PASS_STARTUP      pass_startup;
  FINISH_PASS       finish_pass;

  /* State variables made visible to other modules */
  boolean call_pass_startup;  /* True if pass_startup must be called */
  boolean is_last_pass;   /* True during last pass */
}


typedef void START_PASS (j_compress_ptr cinfo, J_BUF_MODE pass_mode);
typedef int ISTART_PASS (j_compress_ptr cinfo, J_BUF_MODE pass_mode);
typedef void PROCESS_DATA (j_compress_ptr cinfo,
             JSAMPARRAY input_buf, JDIMENSION *in_row_ctr,
             JDIMENSION in_rows_avail);

/* Main buffer control (downsampled-data buffer) */
struct jpeg_c_main_controller {
  START_PASS   start_pass;
  PROCESS_DATA process_data;
}


typedef void PRE_PROCESS_DATA  (j_compress_ptr cinfo,
           JSAMPARRAY input_buf,
           JDIMENSION *in_row_ctr,
           JDIMENSION in_rows_avail,
           JSAMPIMAGE output_buf,
           JDIMENSION *out_row_group_ctr,
           JDIMENSION out_row_groups_avail);

/* Compression preprocessing (downsampling input buffer control) */
struct jpeg_c_prep_controller {
  ISTART_PASS       istart_pass;
  PRE_PROCESS_DATA  pre_process_data;
}


typedef boolean COMPRESS_DATA (j_compress_ptr cinfo,
           JSAMPIMAGE input_buf);

/* Coefficient buffer control */
struct jpeg_c_coef_controller {
  START_PASS   start_pass;
  COMPRESS_DATA compress_data;
}


typedef void START_PASS0 (j_compress_ptr cinfo);


typedef void COLOR_CONVERT (j_compress_ptr cinfo,
        JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
        JDIMENSION output_row, int num_rows);


/* Colorspace conversion */
struct jpeg_color_converter {
  START_PASS0   start_pass;
  COLOR_CONVERT color_convert;
}

typedef void DOWNSAMPLE (j_compress_ptr cinfo,
           JSAMPIMAGE input_buf, JDIMENSION in_row_index,
           JSAMPIMAGE output_buf,
           JDIMENSION out_row_group_index);

/* Downsampling */
struct jpeg_downsampler {
  START_PASS0   start_pass;
  DOWNSAMPLE    downsample;
  boolean       need_context_rows;  /* TRUE if need rows above & below */
}

typedef void FORWARD_DCT (j_compress_ptr cinfo,
            jpeg_component_info * compptr,
            JSAMPARRAY sample_data, JBLOCKROW coef_blocks,
            JDIMENSION start_row, JDIMENSION start_col,
            JDIMENSION num_blocks);

/* Forward DCT (also controls coefficient quantization) */
struct jpeg_forward_dct {
  START_PASS0   start_pass;
  /* perhaps this should be an array??? */
  FORWARD_DCT  forward_DCT;
}

typedef void START_PASS2 (j_compress_ptr cinfo, boolean gather_statistics);
typedef boolean ENCODE_MCU (j_compress_ptr cinfo, JBLOCKROW *MCU_data);

/* Entropy encoding */
struct jpeg_entropy_encoder {
  START_PASS2  start_pass;
  ENCODE_MCU   encode_mcu;
  FINISH_PASS  finish_pass;
}


typedef void WRITE_MARKER (j_compress_ptr cinfo);

typedef void WRITE_MARKER_HEADER (j_compress_ptr cinfo, int marker, uint datalen);
typedef void WRITE_MARKER_BYTE (j_compress_ptr cinfo, int val);

/* Marker writing */
struct jpeg_marker_writer {
  WRITE_MARKER  write_file_header;
  WRITE_MARKER  write_frame_header;
  WRITE_MARKER  write_scan_header;
  WRITE_MARKER  write_file_trailer;
  WRITE_MARKER  write_tables_only;
  /* These routines are exported to allow insertion of extra markers */
  /* Probably only COM and APPn markers should be written this way */
  WRITE_MARKER_HEADER write_marker_header;
  WRITE_MARKER_BYTE   write_marker_byte;
}


/* Declarations for decompression modules */

typedef int PREPARE_FOR_OUTPUT_PASS (j_decompress_ptr cinfo);
typedef void FINISH_OUTPUT_PASS (j_decompress_ptr cinfo);

/* Master control module */
struct jpeg_decomp_master {
  PREPARE_FOR_OUTPUT_PASS  iprepare_for_output_pass;
  FINISH_OUTPUT_PASS       finish_output_pass;

  /* State variables made visible to other modules */
  boolean is_dummy_pass;  /* True during 1st pass for 2-pass quant */
}


typedef int CONSUME_INPUT  (j_decompress_ptr cinfo);
typedef void RESET_INPUT_CONTROLLER (j_decompress_ptr cinfo);
typedef void START_INPUT_PASS (j_decompress_ptr cinfo);
typedef int ISTART_INPUT_PASS (j_decompress_ptr cinfo);
typedef void FINISH_INPUT_PASS (j_decompress_ptr cinfo);

/* Input control module */
struct jpeg_input_controller {
  CONSUME_INPUT           consume_input;
  RESET_INPUT_CONTROLLER  reset_input_controller;
  ISTART_INPUT_PASS       istart_input_pass;
  FINISH_INPUT_PASS       finish_input_pass;

  /* State variables made visible to other modules */
  boolean has_multiple_scans; /* True if file has multiple scans */
  boolean eoi_reached;    /* True when EOI has been consumed */
}

typedef int ISTART_PASS_D2 (j_decompress_ptr cinfo, J_BUF_MODE pass_mode);
typedef void PROCESS_DATA_D (j_decompress_ptr cinfo,
             JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
             JDIMENSION out_rows_avail);

/* Main buffer control (downsampled-data buffer) */
struct jpeg_d_main_controller {
  ISTART_PASS_D2 istart_pass;
  PROCESS_DATA_D process_data;
}

typedef int  CONSUME_DATA      (j_decompress_ptr cinfo);
typedef void START_OUTPUT_PASS (j_decompress_ptr cinfo);
typedef int  DECOMPRESS_DATA   (j_decompress_ptr cinfo, JSAMPIMAGE output_buf);

/* Coefficient buffer control */
struct jpeg_d_coef_controller {
  ISTART_INPUT_PASS  istart_input_pass;
  CONSUME_DATA       consume_data;
  START_OUTPUT_PASS  start_output_pass;
  DECOMPRESS_DATA    decompress_data;

  /* Pointer to array of coefficient virtual arrays, or NULL if none */
  jvirt_barray_ptr *coef_arrays;
}

typedef void POST_PROCESS_DATA (j_decompress_ptr cinfo,
            JSAMPIMAGE input_buf,
            JDIMENSION *in_row_group_ctr,
            JDIMENSION in_row_groups_avail,
            JSAMPARRAY output_buf,
            JDIMENSION *out_row_ctr,
            JDIMENSION out_rows_avail);

/* Decompression postprocessing (color quantization buffer control) */
struct jpeg_d_post_controller {
  ISTART_PASS_D2    istart_pass;
  POST_PROCESS_DATA post_process_data;
}

typedef void RESET_MARKER_READER (j_decompress_ptr cinfo);
typedef int  READ_MARKERS  (j_decompress_ptr cinfo);

/* Marker reading & parsing */
struct jpeg_marker_reader {
  RESET_MARKER_READER reset_marker_reader;
  /* Read markers until SOS or EOI.
   * Returns same codes as are defined for jpeg_consume_input:
   * JPEG_SUSPENDED, JPEG_REACHED_SOS, or JPEG_REACHED_EOI.
   */
  READ_MARKERS read_markers;
  /* Read a restart marker --- exported for use by entropy decoder only */
  jpeg_marker_parser_method read_restart_marker;

  /* State of marker reader --- nominally internal, but applications
   * supplying COM or APPn handlers might like to know the state.
   */
  boolean saw_SOI;    /* found SOI? */
  boolean saw_SOF;    /* found SOF? */
  int next_restart_num;   /* next restart number expected (0-7) */
  uint discarded_bytes; /* # of bytes skipped looking for a marker */
}

typedef void START_PASS_D (j_decompress_ptr cinfo);
typedef int ISTART_PASS_D (j_decompress_ptr cinfo);
typedef boolean DECODE_MCU  (j_decompress_ptr cinfo, JBLOCKROW *MCU_data);

/* Entropy decoding */
struct jpeg_entropy_decoder {
  ISTART_PASS_D  istart_pass;
  DECODE_MCU     decode_mcu;

  /* This is here to share code between baseline and progressive decoders; */
  /* other modules probably should not use it */
  boolean insufficient_data;  /* set TRUE after emitting warning */
}

/* Inverse DCT (also performs dequantization) */
typedef void inverse_DCT_method_ptr
    (j_decompress_ptr cinfo, jpeg_component_info * compptr,
     JCOEFPTR coef_block,
     JSAMPARRAY output_buf, JDIMENSION output_col);

struct jpeg_inverse_dct {
  ISTART_PASS_D  istart_pass;
  /* It is useful to allow each component to have a separate IDCT method. */
  inverse_DCT_method_ptr inverse_DCT[MAX_COMPONENTS];
}

typedef void UPSAMPLE (j_decompress_ptr cinfo,
         JSAMPIMAGE input_buf,
         JDIMENSION *in_row_group_ctr,
         JDIMENSION in_row_groups_avail,
         JSAMPARRAY output_buf,
         JDIMENSION *out_row_ctr,
         JDIMENSION out_rows_avail);

/* Upsampling (note that upsampler must also call color converter) */
struct jpeg_upsampler {
  START_PASS_D  start_pass;
  UPSAMPLE      upsample;
  boolean       need_context_rows;  /* TRUE if need rows above & below */
}

typedef void COLOR_CONVERT_D (j_decompress_ptr cinfo,
        JSAMPIMAGE input_buf, JDIMENSION input_row,
        JSAMPARRAY output_buf, int num_rows);

/* Colorspace conversion */
struct jpeg_color_deconverter {
  START_PASS_D    start_pass;
  COLOR_CONVERT_D color_convert;
}

typedef void START_PASS_DP (j_decompress_ptr cinfo, boolean is_pre_scan);
typedef void COLOR_QUANTIZE (j_decompress_ptr cinfo,
         JSAMPARRAY input_buf, JSAMPARRAY output_buf,
         int num_rows);
typedef void FINISH_PASS_D (j_decompress_ptr cinfo);
typedef void NEW_COLOR_MAP (j_decompress_ptr cinfo);

/* Color quantization or color precision reduction */
struct jpeg_color_quantizer {
  START_PASS_DP start_pass;
  COLOR_QUANTIZE color_quantize;
  FINISH_PASS_D finish_pass;
  NEW_COLOR_MAP new_color_map;
}


/* Miscellaneous useful macros */

uint MIN (uint a, uint b);
uint MAX (uint a, uint b);

int iMIN (int a, int b);
int iMAX (int a, int b);

/* We assume that right shift corresponds to signed division by 2 with
 * rounding towards minus infinity.  This is correct for typical "arithmetic
 * shift" instructions that shift in copies of the sign bit.  But some
 * C compilers implement >> with an unsigned shift.  For these machines you
 * must define RIGHT_SHIFT_IS_UNSIGNED.
 * RIGHT_SHIFT provides a proper signed right shift of an INT32 quantity.
 * It is only applied with constant shift counts.  SHIFT_TEMPS must be
 * included in the variables of any routine using RIGHT_SHIFT.
 */

int RIGHT_SHIFT (int x, int shft);



/* Routines that are to be used by both halves of the library are declared
 * to receive a pointer to this structure.  There are no actual instances of
 * jpeg_common_struct, only of jpeg_compress_struct and jpeg_decompress_struct.
 */

struct jpeg_common_struct {
  jpeg_error_mgr * err; /* Error handler module */
  my_memory_mgr ^ pmem;
  jpeg_memory_mgr * mem;  /* Memory manager module */
  jpeg_progress_mgr * progress; /* Progress monitor, or NULL if none */
  byte* client_data;    /* Available for use by application */
  boolean is_decompressor;  /* So common code can tell which is which */
  int global_state;   /* For checking call sequence validity */

  /* Additional fields follow in an actual jpeg_compress_struct or
   * jpeg_decompress_struct.  All three structs must agree on these
   * initial fields!  (This would be a lot cleaner in C++.)
   */
}


/* Master record for a compression instance */

struct jpeg_compress_struct {

  jpeg_error_mgr * err; /* Error handler module */
  my_memory_mgr ^ pmem;
  jpeg_memory_mgr * mem;  /* Memory manager module */
  jpeg_progress_mgr * progress; /* Progress monitor, or NULL if none */
  byte* client_data;    /* Available for use by application */
  boolean is_decompressor;  /* So common code can tell which is which */
  int global_state;   /* For checking call sequence validity */

  /* Destination for compressed data */
  jpeg_destination_mgr * dest;

  /* Description of source image --- these fields must be filled in by
   * outer application before starting compression.  in_color_space must
   * be correct before you can even call jpeg_set_defaults().
   */

  JDIMENSION image_width; /* input image width */
  JDIMENSION image_height;  /* input image height */
  int input_components;   /* # of color components in input image */
  J_COLOR_SPACE in_color_space; /* colorspace of input image */

  double input_gamma;   /* image gamma of input image */

  /* Compression parameters --- these fields must be set before calling
   * jpeg_start_compress().  We recommend calling jpeg_set_defaults() to
   * initialize everything to reasonable defaults, then changing anything
   * the application specifically wants to change.  That way you won't get
   * burnt when new parameters are added.  Also note that there are several
   * helper routines to simplify changing parameters.
   */

  int data_precision;   /* bits of precision in image data */

  int num_components;   /* # of color components in JPEG image */
  J_COLOR_SPACE jpeg_color_space; /* colorspace of JPEG image */

  jpeg_component_info * comp_info;
  /* comp_info[i] describes component that appears i'th in SOF */
  
  JQUANT_TBL * quant_tbl_ptrs[NUM_QUANT_TBLS];
  /* ptrs to coefficient quantization tables, or NULL if not defined */
  
  JHUFF_TBL * dc_huff_tbl_ptrs[NUM_HUFF_TBLS];
  JHUFF_TBL * ac_huff_tbl_ptrs[NUM_HUFF_TBLS];
  /* ptrs to Huffman coding tables, or NULL if not defined */
  
  UINT8 arith_dc_L[NUM_ARITH_TBLS]; /* L values for DC arith-coding tables */
  UINT8 arith_dc_U[NUM_ARITH_TBLS]; /* U values for DC arith-coding tables */
  UINT8 arith_ac_K[NUM_ARITH_TBLS]; /* Kx values for AC arith-coding tables */

  int num_scans;    /* # of entries in scan_info array */
  jpeg_scan_info * scan_info; /* script for multi-scan file, or NULL */
  /* The default value of scan_info is NULL, which causes a single-scan
   * sequential JPEG file to be emitted.  To create a multi-scan file,
   * set num_scans and scan_info to point to an array of scan definitions.
   */

  boolean raw_data_in;    /* TRUE=caller supplies downsampled data */
  boolean arith_code;   /* TRUE=arithmetic coding, FALSE=Huffman */
  boolean optimize_coding;  /* TRUE=optimize entropy encoding parms */
  boolean CCIR601_sampling; /* TRUE=first samples are cosited */
  int smoothing_factor;   /* 1..100, or 0 for no input smoothing */
  J_DCT_METHOD dct_method;  /* DCT algorithm selector */

  /* The restart interval can be specified in absolute MCUs by setting
   * restart_interval, or in MCU rows by setting restart_in_rows
   * (in which case the correct restart_interval will be figured
   * for each scan).
   */
  uint restart_interval; /* MCUs per restart, or 0 for no restart */
  int restart_in_rows;    /* if > 0, MCU rows per restart interval */

  /* Parameters controlling emission of special markers. */

  boolean write_JFIF_header;  /* should a JFIF marker be written? */
  UINT8 JFIF_major_version; /* What to write for the JFIF version number */
  UINT8 JFIF_minor_version;
  /* These three values are not used by the JPEG code, merely copied */
  /* into the JFIF APP0 marker.  density_unit can be 0 for unknown, */
  /* 1 for dots/inch, or 2 for dots/cm.  Note that the pixel aspect */
  /* ratio is defined by X_density/Y_density even when density_unit=0. */
  UINT8 density_unit;   /* JFIF code for pixel size units */
  UINT16 X_density;   /* Horizontal pixel density */
  UINT16 Y_density;   /* Vertical pixel density */
  boolean write_Adobe_marker; /* should an Adobe marker be written? */
  
  /* State variable: index of next scanline to be written to
   * jpeg_write_scanlines().  Application may use this to control its
   * processing loop, e.g., "while (next_scanline < image_height)".
   */

  JDIMENSION next_scanline; /* 0 .. image_height-1  */

  /* Remaining fields are known throughout compressor, but generally
   * should not be touched by a surrounding application.
   */

  /*
   * These fields are computed during compression startup
   */
  boolean progressive_mode; /* TRUE if scan script uses progressive mode */
  int max_h_samp_factor;  /* largest h_samp_factor */
  int max_v_samp_factor;  /* largest v_samp_factor */

  JDIMENSION total_iMCU_rows; /* # of iMCU rows to be input to coef ctlr */
  /* The coefficient controller receives data in units of MCU rows as defined
   * for fully interleaved scans (whether the JPEG file is interleaved or not).
   * There are v_samp_factor * DCTSIZE sample rows of each component in an
   * "iMCU" (interleaved MCU) row.
   */
  
  /*
   * These fields are valid during any one scan.
   * They describe the components and MCUs actually appearing in the scan.
   */
  int comps_in_scan;    /* # of JPEG components in this scan */
  jpeg_component_info * cur_comp_info[MAX_COMPS_IN_SCAN];
  /* *cur_comp_info[i] describes component that appears i'th in SOS */
  
  JDIMENSION MCUs_per_row;  /* # of MCUs across the image */
  JDIMENSION MCU_rows_in_scan;  /* # of MCU rows in the image */
  
  int blocks_in_MCU;    /* # of DCT blocks per MCU */
  int MCU_membership[C_MAX_BLOCKS_IN_MCU];
  /* MCU_membership[i] is index in cur_comp_info of component owning */
  /* i'th block in an MCU */

  int Ss, Se, Ah, Al;   /* progressive JPEG parameters for scan */

  /*
   * Links to compression subobjects (methods and private variables of modules)
   */
  jpeg_comp_master * master;
  jpeg_c_main_controller * main;
  jpeg_c_prep_controller * prep;
  jpeg_c_coef_controller * coef;
  jpeg_marker_writer * marker;
  jpeg_color_converter * cconvert;
  jpeg_downsampler * downsample;
  jpeg_forward_dct * fdct;
  jpeg_entropy_encoder * entropy;
  jpeg_scan_info * script_space; /* workspace for jpeg_simple_progression */
  int script_space_size;
}


typedef int[DCTSIZE2] DCTCOEFS;


/* Master record for a decompression instance */

struct jpeg_decompress_struct {

  jpeg_error_mgr * err; /* Error handler module */
  my_memory_mgr ^ pmem;
  jpeg_memory_mgr * mem;  /* Memory manager module */
  jpeg_progress_mgr * progress; /* Progress monitor, or NULL if none */
  byte* client_data;    /* Available for use by application */
  boolean is_decompressor;  /* So common code can tell which is which */
  int global_state;   /* For checking call sequence validity */

  /* Source of compressed data */
  jpeg_source_mgr * src;

  /* Basic description of image --- filled in by jpeg_read_header(). */
  /* Application may inspect these values to decide how to process image. */

  JDIMENSION image_width; /* nominal image width (from SOF marker) */
  JDIMENSION image_height;  /* nominal image height */
  int num_components;   /* # of color components in JPEG image */
  J_COLOR_SPACE jpeg_color_space; /* colorspace of JPEG image */

  /* Decompression processing parameters --- these fields must be set before
   * calling jpeg_start_decompress().  Note that jpeg_read_header() initializes
   * them to default values.
   */

  J_COLOR_SPACE out_color_space; /* colorspace for output */

  uint scale_num, scale_denom; /* fraction by which to scale image */

  double output_gamma;    /* image gamma wanted in output */

  boolean buffered_image; /* TRUE=multiple output passes */
  boolean raw_data_out;   /* TRUE=downsampled data wanted */

  J_DCT_METHOD dct_method;  /* IDCT algorithm selector */
  boolean do_fancy_upsampling;  /* TRUE=apply fancy upsampling */
  boolean do_block_smoothing; /* TRUE=apply interblock smoothing */

  boolean quantize_colors;  /* TRUE=colormapped output wanted */
  /* the following are ignored if not quantize_colors: */
  J_DITHER_MODE dither_mode;  /* type of color dithering to use */
  boolean two_pass_quantize;  /* TRUE=use two-pass color quantization */
  int desired_number_of_colors; /* max # colors to use in created colormap */
  /* these are significant only in buffered-image mode: */
  boolean enable_1pass_quant; /* enable future use of 1-pass quantizer */
  boolean enable_external_quant;/* enable future use of external colormap */
  boolean enable_2pass_quant; /* enable future use of 2-pass quantizer */

  /* Description of actual output image that will be returned to application.
   * These fields are computed by jpeg_start_decompress().
   * You can also use jpeg_calc_output_dimensions() to determine these values
   * in advance of calling jpeg_start_decompress().
   */

  JDIMENSION output_width;  /* scaled image width */
  JDIMENSION output_height; /* scaled image height */
  int out_color_components; /* # of color components in out_color_space */
  int output_components;  /* # of color components returned */
  /* output_components is 1 (a colormap index) when quantizing colors;
   * otherwise it equals out_color_components.
   */
  int rec_outbuf_height;  /* min recommended height of scanline buffer */
  /* If the buffer passed to jpeg_read_scanlines() is less than this many rows
   * high, space and time will be wasted due to unnecessary data copying.
   * Usually rec_outbuf_height will be 1 or 2, at most 4.
   */

  /* When quantizing colors, the output colormap is described by these fields.
   * The application can supply a colormap by setting colormap non-NULL before
   * calling jpeg_start_decompress; otherwise a colormap is created during
   * jpeg_start_decompress or jpeg_start_output.
   * The map has out_color_components rows and actual_number_of_colors columns.
   */
  int actual_number_of_colors;  /* number of entries in use */
  JSAMPARRAY colormap;    /* The color map as a 2-D pixel array */

  /* State variables: these variables indicate the progress of decompression.
   * The application may examine these but must not modify them.
   */

  /* Row index of next scanline to be read from jpeg_read_scanlines().
   * Application may use this to control its processing loop, e.g.,
   * "while (output_scanline < output_height)".
   */
  JDIMENSION output_scanline; /* 0 .. output_height-1  */

  /* Current input scan number and number of iMCU rows completed in scan.
   * These indicate the progress of the decompressor input side.
   */
  int input_scan_number;  /* Number of SOS markers seen so far */
  JDIMENSION input_iMCU_row;  /* Number of iMCU rows completed */

  /* The "output scan number" is the notional scan being displayed by the
   * output side.  The decompressor will not allow output scan/row number
   * to get ahead of input scan/row, but it can fall arbitrarily far behind.
   */
  int output_scan_number; /* Nominal scan number being displayed */
  JDIMENSION output_iMCU_row; /* Number of iMCU rows read */

  /* Current progression status.  coef_bits[c][i] indicates the precision
   * with which component c's DCT coefficient i (in zigzag order) is known.
   * It is -1 when no data has yet been received, otherwise it is the point
   * transform (shift) value for the most recent scan of the coefficient
   * (thus, 0 at completion of the progression).
   * This pointer is NULL when reading a non-progressive file.
   */
  int[DCTSIZE2] coef_bits*; /* -1 or current Al value for each coef */

  /* Internal JPEG parameters --- the application usually need not look at
   * these fields.  Note that the decompressor output side may not use
   * any parameters that can change between scans.
   */

  /* Quantization and Huffman tables are carried forward across input
   * datastreams when processing abbreviated JPEG datastreams.
   */

  JQUANT_TBL * quant_tbl_ptrs[NUM_QUANT_TBLS];
  /* ptrs to coefficient quantization tables, or NULL if not defined */

  JHUFF_TBL * dc_huff_tbl_ptrs[NUM_HUFF_TBLS];
  JHUFF_TBL * ac_huff_tbl_ptrs[NUM_HUFF_TBLS];
  /* ptrs to Huffman coding tables, or NULL if not defined */

  /* These parameters are never carried across datastreams, since they
   * are given in SOF/SOS markers or defined to be reset by SOI.
   */

  int data_precision;   /* bits of precision in image data */

  jpeg_component_info * comp_info;
  /* comp_info[i] describes component that appears i'th in SOF */

  boolean progressive_mode; /* TRUE if SOFn specifies progressive mode */
  boolean arith_code;   /* TRUE=arithmetic coding, FALSE=Huffman */

  UINT8 arith_dc_L[NUM_ARITH_TBLS]; /* L values for DC arith-coding tables */
  UINT8 arith_dc_U[NUM_ARITH_TBLS]; /* U values for DC arith-coding tables */
  UINT8 arith_ac_K[NUM_ARITH_TBLS]; /* Kx values for AC arith-coding tables */

  uint restart_interval; /* MCUs per restart interval, or 0 for no restart */

  /* These fields record data obtained from optional markers recognized by
   * the JPEG library.
   */
  boolean saw_JFIF_marker;  /* TRUE iff a JFIF APP0 marker was found */
  /* Data copied from JFIF marker; only valid if saw_JFIF_marker is TRUE: */
  UINT8 JFIF_major_version; /* JFIF version number */
  UINT8 JFIF_minor_version;
  UINT8 density_unit;   /* JFIF code for pixel size units */
  UINT16 X_density;   /* Horizontal pixel density */
  UINT16 Y_density;   /* Vertical pixel density */
  boolean saw_Adobe_marker; /* TRUE iff an Adobe APP14 marker was found */
  UINT8 Adobe_transform;  /* Color transform code from Adobe marker */

  boolean CCIR601_sampling; /* TRUE=first samples are cosited */

  /* Aside from the specific data retained from APPn markers known to the
   * library, the uninterpreted contents of any or all APPn and COM markers
   * can be saved in a list for examination by the application.
   */
  jpeg_saved_marker_ptr marker_list; /* Head of list of saved markers */

  /* Remaining fields are known throughout decompressor, but generally
   * should not be touched by a surrounding application.
   */

  /*
   * These fields are computed during decompression startup
   */
  int max_h_samp_factor;  /* largest h_samp_factor */
  int max_v_samp_factor;  /* largest v_samp_factor */

  int min_DCT_scaled_size;  /* smallest DCT_scaled_size of any component */

  JDIMENSION total_iMCU_rows; /* # of iMCU rows in image */
  /* The coefficient controller's input and output progress is measured in
   * units of "iMCU" (interleaved MCU) rows.  These are the same as MCU rows
   * in fully interleaved JPEG scans, but are used whether the scan is
   * interleaved or not.  We define an iMCU row as v_samp_factor DCT block
   * rows of each component.  Therefore, the IDCT output contains
   * v_samp_factor*DCT_scaled_size sample rows of a component per iMCU row.
   */

  JSAMPLE * sample_range_limit; /* table for fast range-limiting */

  /*
   * These fields are valid during any one scan.
   * They describe the components and MCUs actually appearing in the scan.
   * Note that the decompressor output side must not use these fields.
   */
  int comps_in_scan;    /* # of JPEG components in this scan */
  jpeg_component_info * cur_comp_info[MAX_COMPS_IN_SCAN];
  /* *cur_comp_info[i] describes component that appears i'th in SOS */

  JDIMENSION MCUs_per_row;  /* # of MCUs across the image */
  JDIMENSION MCU_rows_in_scan;  /* # of MCU rows in the image */

  int blocks_in_MCU;    /* # of DCT blocks per MCU */
  int MCU_membership[D_MAX_BLOCKS_IN_MCU];
  /* MCU_membership[i] is index in cur_comp_info of component owning */
  /* i'th block in an MCU */

  int Ss, Se, Ah, Al;   /* progressive JPEG parameters for scan */

  /* This field is shared between entropy decoder and marker parser.
   * It is either zero or the code of a JPEG marker that has been
   * read from the data source, but has not yet been processed.
   */
  int unread_marker;

  /*
   * Links to decompression subobjects (methods, private variables of modules)
   */
  jpeg_decomp_master * master;
  jpeg_d_main_controller * main;
  jpeg_d_coef_controller * coef;
  jpeg_d_post_controller * post;
  jpeg_input_controller * inputctl;
  jpeg_marker_reader * marker;
  jpeg_entropy_decoder * entropy;
  jpeg_inverse_dct * idct;
  jpeg_upsampler * upsample;
  jpeg_color_deconverter * cconvert;
  jpeg_color_quantizer * cquantize;
}

typedef jpeg_common_struct * j_common_ptr;
typedef jpeg_compress_struct * j_compress_ptr;
typedef jpeg_decompress_struct * j_decompress_ptr;


/* Return value is one of: */
const int JPEG_SUSPENDED    = 0; /* Suspended due to lack of input data */
const int JPEG_HEADER_OK    = 1; /* Found valid image datastream */
const int JPEG_HEADER_TABLES_ONLY = 2; /* Found valid table-specs-only datastream */
/* If you pass require_image = TRUE (normal case), you need not check for
 * a TABLES_ONLY return code; an abbreviated file will cause an error exit.
 * JPEG_SUSPENDED is only possible if you use a data source module that can
 * give a suspension return (the stdio source module doesn't).
 */



/* Return value is one of: */
/* #define JPEG_SUSPENDED 0    Suspended due to lack of input data */
const int JPEG_REACHED_SOS  = 1; /* Reached start of new scan */
const int JPEG_REACHED_EOI  = 2; /* Reached end of image */
const int JPEG_ROW_COMPLETED  = 3; /* Completed one iMCU row */
const int JPEG_SCAN_COMPLETED = 4; /* Completed last iMCU row of a scan */


/* These marker codes are exported since applications and data source modules
 * are likely to want to use them.
 */

const byte JPEG_RST0  = 0xD0; /* RST0 marker code */
const byte JPEG_EOI = 0xD9; /* EOI marker code */
const byte JPEG_APP0  = 0xE0; /* APP0 marker code */
const byte JPEG_COM = 0xFE; /* COM marker code */


/* For maintenance convenience, list is alphabetical by message code name */
const int JMSG_NOMESSAGE = 0;
const int JERR_ARITH_NOTIMPL = 1;
const int JERR_BAD_ALIGN_TYPE = 2;
const int JERR_BAD_ALLOC_CHUNK = 3;
const int JERR_BAD_BUFFER_MODE = 4;
const int JERR_BAD_COMPONENT_ID = 5;
const int JERR_BAD_DCT_COEF = 6;
const int JERR_BAD_DCTSIZE = 7;
const int JERR_BAD_HUFF_TABLE = 8;
const int JERR_BAD_IN_COLORSPACE = 9;
const int JERR_BAD_J_COLORSPACE = 10;
const int JERR_BAD_LENGTH = 11;
const int JERR_BAD_LIB_VERSION = 12;
const int JERR_BAD_MCU_SIZE = 13;
const int JERR_BAD_POOL_ID = 14;
const int JERR_BAD_PRECISION = 15;
const int JERR_BAD_PROGRESSION = 16;
const int JERR_BAD_PROG_SCRIPT = 17;
const int JERR_BAD_SAMPLING = 18;
const int JERR_BAD_SCAN_SCRIPT = 19;
const int JERR_BAD_STATE = 20;
const int JERR_BAD_STRUCT_SIZE = 21;
const int JERR_BAD_VIRTUAL_ACCESS = 22;
const int JERR_BUFFER_SIZE = 23;
const int JERR_CANT_SUSPEND = 24;
const int JERR_CCIR601_NOTIMPL = 25;
const int JERR_COMPONENT_COUNT = 26;
const int JERR_CONVERSION_NOTIMPL = 27;
const int JERR_DAC_INDEX = 28;
const int JERR_DAC_VALUE = 29;
const int JERR_DHT_INDEX = 30;
const int JERR_DQT_INDEX = 31;
const int JERR_EMPTY_IMAGE = 32;
const int JERR_EMS_READ = 33;
const int JERR_EMS_WRITE = 34;
const int JERR_EOI_EXPECTED = 35;
const int JERR_FILE_READ = 36;
const int JERR_FILE_WRITE = 37;
const int JERR_FRACT_SAMPLE_NOTIMPL = 38;
const int JERR_HUFF_CLEN_OVERFLOW = 39;
const int JERR_HUFF_MISSING_CODE = 40;
const int JERR_IMAGE_TOO_BIG = 41;
const int JERR_INPUT_EMPTY = 42;
const int JERR_INPUT_EOF = 43;
const int JERR_MISMATCHED_QUANT_TABLE = 44;
const int JERR_MISSING_DATA = 45;
const int JERR_MODE_CHANGE = 46;
const int JERR_NOTIMPL = 47;
const int JERR_NOT_COMPILED = 48;
const int JERR_NO_BACKING_STORE = 49;
const int JERR_NO_HUFF_TABLE = 50;
const int JERR_NO_IMAGE = 51;
const int JERR_NO_QUANT_TABLE = 52;
const int JERR_NO_SOI = 53;
const int JERR_OUT_OF_MEMORY= 54;
const int JERR_QUANT_COMPONENTS = 55;
const int JERR_QUANT_FEW_COLORS = 56;
const int JERR_QUANT_MANY_COLORS = 57;
const int JERR_SOF_DUPLICATE = 58;
const int JERR_SOF_NO_SOS = 59;
const int JERR_SOF_UNSUPPORTED = 60;
const int JERR_SOI_DUPLICATE = 61;
const int JERR_SOS_NO_SOF = 62;
const int JERR_TFILE_CREATE = 63;
const int JERR_TFILE_READ = 64;
const int JERR_TFILE_SEEK = 65;
const int JERR_TFILE_WRITE = 66;
const int JERR_TOO_LITTLE_DATA = 67;
const int JERR_UNKNOWN_MARKER = 68;
const int JERR_VIRTUAL_BUG = 69;
const int JERR_WIDTH_OVERFLOW = 70;
const int JERR_XMS_READ = 71;
const int JERR_XMS_WRITE = 72;
const int JMSG_COPYRIGHT = 73;
const int JMSG_VERSION = 74;
const int JTRC_16BIT_TABLES = 75;
const int JTRC_ADOBE = 76;
const int JTRC_APP0 = 77;
const int JTRC_APP14 = 78;
const int JTRC_DAC = 79;
const int JTRC_DHT = 80;
const int JTRC_DQT = 81;
const int JTRC_DRI = 82;
const int JTRC_EMS_CLOSE = 83;
const int JTRC_EMS_OPEN = 84;
const int JTRC_EOI = 85;
const int JTRC_HUFFBITS = 86;
const int JTRC_JFIF = 87;
const int JTRC_JFIF_BADTHUMBNAILSIZE = 88;
const int JTRC_JFIF_EXTENSION = 89;
const int JTRC_JFIF_THUMBNAIL = 90;
const int JTRC_MISC_MARKER = 91;
const int JTRC_PARMLESS_MARKER = 92;
const int JTRC_QUANTVALS = 93;
const int JTRC_QUANT_3_NCOLORS = 94;
const int JTRC_QUANT_NCOLORS = 95;
const int JTRC_QUANT_SELECTED = 96;
const int JTRC_RECOVERY_ACTION = 97;
const int JTRC_RST = 98;
const int JTRC_SMOOTH_NOTIMPL = 99;
const int JTRC_SOF = 100;
const int JTRC_SOF_COMPONENT = 101;
const int JTRC_SOI = 102;
const int JTRC_SOS = 103;
const int JTRC_SOS_COMPONENT = 104;
const int JTRC_SOS_PARAMS = 105;
const int JTRC_TFILE_CLOSE = 106;
const int JTRC_TFILE_OPEN = 107;
const int JTRC_THUMB_JPEG = 108;
const int JTRC_THUMB_PALETTE = 109;
const int JTRC_THUMB_RGB = 110;
const int JTRC_UNKNOWN_IDS = 111;
const int JTRC_XMS_CLOSE = 112;
const int JTRC_XMS_OPEN = 113;
const int JWRN_ADOBE_XFORM = 114;
const int JWRN_BOGUS_PROGRESSION = 115;
const int JWRN_EXTRANEOUS_DATA = 116;
const int JWRN_HIT_MARKER = 117;
const int JWRN_HUFF_BAD_CODE = 118;
const int JWRN_JFIF_MAJOR = 119;
const int JWRN_JPEG_EOF = 120;
const int JWRN_MUST_RESYNC = 121;
const int JWRN_NOT_SEQUENTIAL = 122;
const int JWRN_TOO_MUCH_DATA = 123;

#end unsafe
