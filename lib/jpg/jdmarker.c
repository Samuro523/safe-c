/*
 * jdmarker.c
 *
 * Copyright (C) 1991-1998, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains routines to decode JPEG datastream markers.
 * Most of the complexity arises from our desire to support input
 * suspension: if not all of the data for a marker is available,
 * we must exit back to the application.  On resumption, we reprocess
 * the marker.
 */

use jpeglib, jmorecfg;
use jcomapi, jutils;

#begin unsafe
			/* JPEG marker codes */
typedef int JPEG_MARKER;

const int M_SOF0  = 0xc0;
const int M_SOF1  = 0xc1;
const int M_SOF2  = 0xc2;
const int M_SOF3  = 0xc3;
  
const int M_SOF5  = 0xc5;
const int M_SOF6  = 0xc6;
const int M_SOF7  = 0xc7;
  
const int M_JPG   = 0xc8;
const int M_SOF9  = 0xc9;
const int M_SOF10 = 0xca;
const int M_SOF11 = 0xcb;
  
const int M_SOF13 = 0xcd;
const int M_SOF14 = 0xce;
const int M_SOF15 = 0xcf;
  
const int M_DHT   = 0xc4;
  
const int M_DAC   = 0xcc;
  
const int  M_RST0  = 0xd0;
const int  M_RST1  = 0xd1;
const int  M_RST2  = 0xd2;
const int  M_RST3  = 0xd3;
const int  M_RST4  = 0xd4;
const int  M_RST5  = 0xd5;
const int  M_RST6  = 0xd6;
const int  M_RST7  = 0xd7;
  
const int  M_SOI   = 0xd8;
const int  M_EOI   = 0xd9;
const int  M_SOS   = 0xda;
const int  M_DQT   = 0xdb;
const int  M_DNL   = 0xdc;
const int  M_DRI   = 0xdd;
const int  M_DHP   = 0xde;
const int  M_EXP   = 0xdf;
  
const int  M_APP0  = 0xe0;
const int  M_APP1  = 0xe1;
const int  M_APP2  = 0xe2;
const int  M_APP3  = 0xe3;
const int  M_APP4  = 0xe4;
const int  M_APP5  = 0xe5;
const int  M_APP6  = 0xe6;
const int  M_APP7  = 0xe7;
const int  M_APP8  = 0xe8;
const int  M_APP9  = 0xe9;
const int  M_APP10 = 0xea;
const int  M_APP11 = 0xeb;
const int  M_APP12 = 0xec;
const int  M_APP13 = 0xed;
const int  M_APP14 = 0xee;
const int  M_APP15 = 0xef;
  
const int  M_JPG0  = 0xf0;
const int  M_JPG13 = 0xfd;
const int  M_COM   = 0xfe;
  
const int  M_TEM   = 0x01;
  
const int  M_ERROR = 0x100;




/* Private state */

struct my_marker_reader
{
  jpeg_marker_reader pub; /* public fields */

  /* Application-overridable marker processing methods */
  jpeg_marker_parser_method process_COM;
  jpeg_marker_parser_method process_APPn[16];

  /* Limit on marker data length to save for each marker type */
  uint length_limit_COM;
  uint length_limit_APPn[16];

  /* Status of COM/APPn marker saving */
  jpeg_saved_marker_ptr cur_marker;	/* null if not processing a marker */
  uint bytes_read;		/* data bytes read so far in marker */
  /* Note: cur_marker is not linked into marker_list until it's all read. */
}

typedef my_marker_reader * my_marker_ptr;


/*
 * Macros for fetching data from the data source module.
 *
 * At all times, cinfo->src->next_input_byte and ->bytes_in_buffer reflect
 * the current restart point; we update them only when we have reached a
 * suitable place to restart if a suspension occurs.
 */


/*
 * Routines to process JPEG markers.
 *
 * Entry condition: JPEG marker itself has been read and its code saved
 *   in cinfo->unread_marker; input restart point is just after the marker.
 *
 * Exit: if return TRUE, have read and processed any parameters, and have
 *   updated the restart point to point after the parameters.
 *   If return FALSE, was forced to suspend before reaching end of
 *   marker parameters; restart point has not been moved.  Same routine
 *   will be called again after application supplies more input data.
 *
 * This approach to suspension assumes that all of a marker's parameters
 * can fit into a single input bufferload.  This should hold for "normal"
 * markers.  Some COM/APPn markers might have large parameter segments
 * that might not fit.  If we are simply dropping such a marker, we use
 * skip_input_data to get past it, and thereby put the problem on the
 * source manager's shoulders.  If we are saving the marker's contents
 * into memory, we use a slightly different convention: when forced to
 * suspend, the marker processor updates the restart point to the end of
 * what it's consumed (ie, the end of the buffer) before returning FALSE.
 * On resumption, cinfo->unread_marker still contains the marker code,
 * but the data source will point to the next chunk of marker data.
 * The marker processor must retain internal state to deal with this.
 *
 * Note that we don't bother to avoid duplicate trace messages if a
 * suspension occurs within marker parameters.  Other side effects
 * require more care.
 */


boolean
get_soi (j_decompress_ptr cinfo)
/* Process an SOI marker */
{
  int i;
  
//  TRACEMS(cinfo, 1, JTRC_SOI);

  if (cinfo->marker->saw_SOI)
    return FALSE; // dERREXIT(cinfo, JERR_SOI_DUPLICATE);

  /* Reset all parameters that are defined to be reset by SOI */

  for (i = 0; i < NUM_ARITH_TBLS; i++) {
    cinfo->arith_dc_L[i] = 0;
    cinfo->arith_dc_U[i] = 1;
    cinfo->arith_ac_K[i] = 5;
  }
  cinfo->restart_interval = 0;

  /* Set initial assumptions for colorspace etc */

  cinfo->jpeg_color_space = JCS_UNKNOWN;
  cinfo->CCIR601_sampling = FALSE; /* Assume non-CCIR sampling??? */

  cinfo->saw_JFIF_marker = FALSE;
  cinfo->JFIF_major_version = 1; /* set default JFIF APP0 values */
  cinfo->JFIF_minor_version = 1;
  cinfo->density_unit = 0;
  cinfo->X_density = 1;
  cinfo->Y_density = 1;
  cinfo->saw_Adobe_marker = FALSE;
  cinfo->Adobe_transform = 0;

  cinfo->marker->saw_SOI = TRUE;

  return TRUE;
}


boolean
get_sof (j_decompress_ptr cinfo, boolean is_prog, boolean is_arith)
/* Process a SOFn marker */
{
  INT32 length;
  int c, ci;
  jpeg_component_info * compptr;

	jpeg_source_mgr * datasrc = (cinfo)->src;
	JOCTET * next_input_byte = datasrc->next_input_byte;
	size_t bytes_in_buffer = datasrc->bytes_in_buffer;

  cinfo->progressive_mode = is_prog;
  cinfo->arith_code = is_arith;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  length = (int)(((uint) GETJOCTET(*next_input_byte++)) << 8);

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  length += GETJOCTET(*next_input_byte++);


  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  cinfo->data_precision = GETJOCTET(*next_input_byte++); 

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  cinfo->image_height = ((uint) GETJOCTET(*next_input_byte++)) << 8; 

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  cinfo->image_height += GETJOCTET(*next_input_byte++);

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  cinfo->image_width = ((uint) GETJOCTET(*next_input_byte++)) << 8; 

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  cinfo->image_width += GETJOCTET(*next_input_byte++);


  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  cinfo->num_components = GETJOCTET(*next_input_byte++); 


  length -= 8;

//  TRACEMS4(cinfo, 1, JTRC_SOF, cinfo->unread_marker,
//	   (int) cinfo->image_width, (int) cinfo->image_height,
//	   cinfo->num_components);

  if (cinfo->marker->saw_SOF)
    return FALSE; // dERREXIT(cinfo, JERR_SOF_DUPLICATE);

  /* We don't support files in which the image height is initially specified */
  /* as 0 and is later redefined by DNL.  As long as we have to check that,  */
  /* might as well have a general sanity check. */
  if (cinfo->image_height <= 0 || cinfo->image_width <= 0
      || cinfo->num_components <= 0)
    return FALSE; // dERREXIT(cinfo, JERR_EMPTY_IMAGE);

  if (length != (cinfo->num_components * 3))
    return FALSE; // dERREXIT(cinfo, JERR_BAD_LENGTH);

  if (cinfo->comp_info == null)	/* do only once, even if suspend */
    cinfo->comp_info = (jpeg_component_info *) (cinfo->mem->alloc_small)
			((j_common_ptr) cinfo, JPOOL_IMAGE,
			 (uint)cinfo->num_components * (jpeg_component_info'size));
  
  for (ci = 0, compptr = cinfo->comp_info; ci < cinfo->num_components;
       ci++, compptr++) {
    compptr->component_index = ci;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  compptr->component_id = GETJOCTET(*next_input_byte++); 

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  c = GETJOCTET(*next_input_byte++); 

    compptr->h_samp_factor = (c >> 4) & 15;
    compptr->v_samp_factor = (c     ) & 15;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  compptr->quant_tbl_no = GETJOCTET(*next_input_byte++); 

//    TRACEMS4(cinfo, 1, JTRC_SOF_COMPONENT,
//	     compptr->component_id, compptr->h_samp_factor,
//	     compptr->v_samp_factor, compptr->quant_tbl_no);

  }

  cinfo->marker->saw_SOF = TRUE;

	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;

  return TRUE;
}


boolean
get_sos (j_decompress_ptr cinfo)
/* Process a SOS marker */
{
  INT32 length;
  int i, ci, n, c, cc;
  jpeg_component_info * compptr;
  bool found;

	jpeg_source_mgr * datasrc = (cinfo)->src;
	JOCTET * next_input_byte = datasrc->next_input_byte;
	size_t bytes_in_buffer = datasrc->bytes_in_buffer;

  if (! cinfo->marker->saw_SOF)
    return FALSE; // dERREXIT(cinfo, JERR_SOS_NO_SOF);

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  length = (int)(((uint) GETJOCTET(*next_input_byte++)) << 8);

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  length += GETJOCTET(*next_input_byte++);



  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  n = GETJOCTET(*next_input_byte++);   /* Number of components */

//  TRACEMS1(cinfo, 1, JTRC_SOS, n);

  if (length != (n * 2 + 6) || n < 1 || n > MAX_COMPS_IN_SCAN)
    return FALSE; // dERREXIT(cinfo, JERR_BAD_LENGTH);

  cinfo->comps_in_scan = n;

  /* Collect the component-spec parameters */

  for (i = 0; i < n; i++)
  {
    if (bytes_in_buffer == 0)
    {
      if (! (datasrc->fill_input_buffer) (cinfo))
      {
        return FALSE;
      }

      next_input_byte = datasrc->next_input_byte;
      bytes_in_buffer = datasrc->bytes_in_buffer;
    }

    bytes_in_buffer--; 
    cc = GETJOCTET(*next_input_byte++); 


  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  c = GETJOCTET(*next_input_byte++); 

  found = false;
    
    for (ci = 0, compptr = cinfo->comp_info;
         !found && ci < cinfo->num_components;
	 ci++, compptr++)
    {
      if (cc == compptr->component_id)
      {
	found = true;
        break;
      }
    }

    if (!found)
      return FALSE; // dERREXIT1(cinfo, JERR_BAD_COMPONENT_ID, cc);


    cinfo->cur_comp_info[i] = compptr;
    compptr->dc_tbl_no = (c >> 4) & 15;
    compptr->ac_tbl_no = (c     ) & 15;
    
//    TRACEMS3(cinfo, 1, JTRC_SOS_COMPONENT, cc,    compptr->dc_tbl_no, compptr->ac_tbl_no);
  }

  /* Collect the additional scan parameters Ss, Se, Ah/Al. */
  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  c = GETJOCTET(*next_input_byte++); 

  cinfo->Ss = c;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  c = GETJOCTET(*next_input_byte++); 

  cinfo->Se = c;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  c = GETJOCTET(*next_input_byte++); 

  cinfo->Ah = (c >> 4) & 15;
  cinfo->Al = (c     ) & 15;

//  TRACEMS4(cinfo, 1, JTRC_SOS_PARAMS, cinfo->Ss, cinfo->Se, cinfo->Ah, cinfo->Al);

  /* Prepare to scan data & restart markers */
  cinfo->marker->next_restart_num = 0;

  /* Count another SOS marker */
  cinfo->input_scan_number++;

	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;

  return TRUE;
}



boolean
get_dht (j_decompress_ptr cinfo)
/* Process a DHT marker */
{
  INT32 length;
  UINT8 bits[17];
  UINT8 huffval[256];
  int i, index, count;
  JHUFF_TBL **htblptr;

	jpeg_source_mgr * datasrc = (cinfo)->src;
	JOCTET * next_input_byte = datasrc->next_input_byte;
	size_t bytes_in_buffer = datasrc->bytes_in_buffer;

  clear bits;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  length = (int)(((uint) GETJOCTET(*next_input_byte++)) << 8);

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  length += GETJOCTET(*next_input_byte++);

  length -= 2;
  
  while (length > 16)
  {
  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  index = GETJOCTET(*next_input_byte++); 

//    TRACEMS1(cinfo, 1, JTRC_DHT, index);
      
    bits[0] = 0;
    count = 0;
    for (i = 1; i <= 16; i++)
    {

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  bits[i] = GETJOCTET(*next_input_byte++); 

      count += (int)bits[i];
    }

    length -= 1 + 16;

//    TRACEMS8(cinfo, 2, JTRC_HUFFBITS, bits[1], bits[2], bits[3], bits[4], bits[5], bits[6], bits[7], bits[8]);
//    TRACEMS8(cinfo, 2, JTRC_HUFFBITS, bits[9], bits[10], bits[11], bits[12], bits[13], bits[14], bits[15], bits[16]);

    /* Here we just do minimal validation of the counts to avoid walking
     * off the end of our table space.  jdhuff.c will check more carefully.
     */
    if (count > 256 || ((INT32) count) > length)
      return FALSE; // dERREXIT(cinfo, JERR_BAD_HUFF_TABLE);

    for (i = 0; i < count; i++)
    {
  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  huffval[i] = GETJOCTET(*next_input_byte++); 

    }

    length -= count;

    if ((index & 0x10) != 0) {		/* AC table definition */
      index -= 0x10;
      htblptr = &cinfo->ac_huff_tbl_ptrs[index];
    } else {			/* DC table definition */
      htblptr = &cinfo->dc_huff_tbl_ptrs[index];
    }

    if (index < 0 || index >= NUM_HUFF_TBLS)
      return FALSE; // dERREXIT1(cinfo, JERR_DHT_INDEX, index);

    if (*htblptr == null)
      *htblptr = jpeg_alloc_huff_table((j_common_ptr) cinfo);
  
    jcopy((byte*)&(*htblptr)->bits, (byte*)&bits, ((*htblptr)->bits'size));
    jcopy(&(*htblptr)->huffval, &huffval, ((*htblptr)->huffval'size));
  }

  if (length != 0)
    return FALSE; // dERREXIT(cinfo, JERR_BAD_LENGTH);

	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;

  return TRUE;
}


boolean
get_dqt (j_decompress_ptr cinfo)
/* Process a DQT marker */
{
  INT32 length;
  int n, i, prec;
  uint tmp;
  JQUANT_TBL *quant_ptr;

	jpeg_source_mgr * datasrc = (cinfo)->src;
	JOCTET * next_input_byte = datasrc->next_input_byte;
	size_t bytes_in_buffer = datasrc->bytes_in_buffer;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  length = (int)(((uint) GETJOCTET(*next_input_byte++)) << 8); 

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  length += GETJOCTET(*next_input_byte++);

  length -= 2;

  while (length > 0)
  {
  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  n = GETJOCTET(*next_input_byte++); 

    prec = n >> 4;
    n &= 0x0F;

//    TRACEMS2(cinfo, 1, JTRC_DQT, n, prec);

    if (n >= NUM_QUANT_TBLS)
      return FALSE; // dERREXIT1(cinfo, JERR_DQT_INDEX, n);
      
    if (cinfo->quant_tbl_ptrs[n] == null)
      cinfo->quant_tbl_ptrs[n] = jpeg_alloc_quant_table((j_common_ptr) cinfo);
    quant_ptr = cinfo->quant_tbl_ptrs[n];

    for (i = 0; i < DCTSIZE2; i++)
    {
      if (prec != 0)
      {

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  tmp = ((uint) GETJOCTET(*next_input_byte++)) << 8; 

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  tmp += GETJOCTET(*next_input_byte++);


      }
      else
      {
  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  tmp = GETJOCTET(*next_input_byte++); 

      }

      /* We convert the zigzag-order table to natural array order. */
      quant_ptr->quantval[jpeg_natural_order[i]] = (UINT16) tmp;
    }

    length -= DCTSIZE2+1;
    if (prec != 0) length -= DCTSIZE2;
  }

  if (length != 0)
    return FALSE; // dERREXIT(cinfo, JERR_BAD_LENGTH);

	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;

  return TRUE;
}


boolean
get_dri (j_decompress_ptr cinfo)
/* Process a DRI marker */
{
  INT32 length;
  uint tmp;

	jpeg_source_mgr * datasrc = (cinfo)->src;
	JOCTET * next_input_byte = datasrc->next_input_byte;
	size_t bytes_in_buffer = datasrc->bytes_in_buffer;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  length = (int)(((uint) GETJOCTET(*next_input_byte++)) << 8);

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  length += GETJOCTET(*next_input_byte++);

  
  if (length != 4)
    return FALSE; // dERREXIT(cinfo, JERR_BAD_LENGTH);

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  tmp = ((uint) GETJOCTET(*next_input_byte++)) << 8; 

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  tmp += GETJOCTET(*next_input_byte++);


  cinfo->restart_interval = tmp;

	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;

  return TRUE;
}


/*
 * Routines for processing APPn and COM markers.
 * These are either saved in memory or discarded, per application request.
 * APP0 and APP14 are specially checked to see if they are
 * JFIF and Adobe markers, respectively.
 */

package P
const int APP0_DATA_LEN	 = 14;	/* Length of interesting data in APP0 */
const int APP14_DATA_LEN = 12;	/* Length of interesting data in APP14 */
const int APPN_DATA_LEN	 = 14;	/* Must be the largest of the above!! */
end P;

void
examine_app0 (j_decompress_ptr cinfo, JOCTET * data,
	      uint datalen, INT32 remaining)
/* Examine first few bytes from an APP0.
 * Take appropriate action if it is a JFIF marker.
 * datalen is # of bytes at data[], remaining is length of rest of marker data.
 */
{
  INT32 totallen = (INT32) datalen + remaining;

  if ((int)datalen >= APP0_DATA_LEN &&
      GETJOCTET(data[0]) == 0x4A &&
      GETJOCTET(data[1]) == 0x46 &&
      GETJOCTET(data[2]) == 0x49 &&
      GETJOCTET(data[3]) == 0x46 &&
      GETJOCTET(data[4]) == 0) {
    /* Found JFIF APP0 marker: save info */
    cinfo->saw_JFIF_marker = TRUE;
    cinfo->JFIF_major_version = GETJOCTET(data[5]);
    cinfo->JFIF_minor_version = GETJOCTET(data[6]);
    cinfo->density_unit = GETJOCTET(data[7]);
    cinfo->X_density = (uint2)((GETJOCTET(data[8]) << 8) + GETJOCTET(data[9]));
    cinfo->Y_density = (uint2)((GETJOCTET(data[10]) << 8) + GETJOCTET(data[11]));
    /* Check version.
     * Major version must be 1, anything else signals an incompatible change.
     * (We used to treat this as an error, but now it's a nonfatal warning,
     * because some bozo at Hijaak couldn't read the spec.)
     * Minor version should be 0..2, but process anyway if newer.
     */

//    if (cinfo->JFIF_major_version != 1)
//      WARNMS2(cinfo, JWRN_JFIF_MAJOR,
//	      cinfo->JFIF_major_version, cinfo->JFIF_minor_version);

    /* Generate trace messages */
//    TRACEMS5(cinfo, 1, JTRC_JFIF,
//	     cinfo->JFIF_major_version, cinfo->JFIF_minor_version,
//	     cinfo->X_density, cinfo->Y_density, cinfo->density_unit);

    /* Validate thumbnail dimensions and issue appropriate messages */
//    if (GETJOCTET(data[12]) | GETJOCTET(data[13]))
//      TRACEMS2(cinfo, 1, JTRC_JFIF_THUMBNAIL,
//	       GETJOCTET(data[12]), GETJOCTET(data[13]));

    totallen -= APP0_DATA_LEN;

//    if (totallen !=
//	((INT32)GETJOCTET(data[12]) * (INT32)GETJOCTET(data[13]) * (INT32) 3))
//      TRACEMS1(cinfo, 1, JTRC_JFIF_BADTHUMBNAILSIZE, (int) totallen);

  } else if (datalen >= 6 &&
      GETJOCTET(data[0]) == 0x4A &&
      GETJOCTET(data[1]) == 0x46 &&
      GETJOCTET(data[2]) == 0x58 &&
      GETJOCTET(data[3]) == 0x58 &&
      GETJOCTET(data[4]) == 0) {
    /* Found JFIF "JFXX" extension APP0 marker */
    /* The library doesn't actually do anything with these,
     * but we try to produce a helpful trace message.
     */
    switch (GETJOCTET(data[5])) {
    case 0x10:
//      TRACEMS1(cinfo, 1, JTRC_THUMB_JPEG, (int) totallen);
      break;
    case 0x11:
//      TRACEMS1(cinfo, 1, JTRC_THUMB_PALETTE, (int) totallen);
      break;
    case 0x13:
//      TRACEMS1(cinfo, 1, JTRC_THUMB_RGB, (int) totallen);
      break;
    default:
//      TRACEMS2(cinfo, 1, JTRC_JFIF_EXTENSION, GETJOCTET(data[5]), (int) totallen);
      break;
    }
  } else {
    /* Start of APP0 does not match "JFIF" or "JFXX", or too short */
//    TRACEMS1(cinfo, 1, JTRC_APP0, (int) totallen);
  }
}


void
examine_app14 (j_decompress_ptr cinfo, JOCTET * data,
	       uint datalen, INT32 remaining)
/* Examine first few bytes from an APP14.
 * Take appropriate action if it is an Adobe marker.
 * datalen is # of bytes at data[], remaining is length of rest of marker data.
 */
{
  uint version, flags0, flags1;
  uint transform;

  _unused remaining;

  if ((int)datalen >= APP14_DATA_LEN &&
      GETJOCTET(data[0]) == 0x41 &&
      GETJOCTET(data[1]) == 0x64 &&
      GETJOCTET(data[2]) == 0x6F &&
      GETJOCTET(data[3]) == 0x62 &&
      GETJOCTET(data[4]) == 0x65) {
    /* Found Adobe APP14 marker */
    version = (GETJOCTET(data[5]) << 8) + GETJOCTET(data[6]);
    flags0 = (GETJOCTET(data[7]) << 8) + GETJOCTET(data[8]);
    flags1 = (GETJOCTET(data[9]) << 8) + GETJOCTET(data[10]);
    transform = GETJOCTET(data[11]);
//    TRACEMS4(cinfo, 1, JTRC_ADOBE, version, flags0, flags1, transform);
    cinfo->saw_Adobe_marker = TRUE;
    cinfo->Adobe_transform = (UINT8) transform;

    _unused version;
    _unused flags0;
    _unused flags1;

  } else {
    /* Start of APP14 does not match "Adobe", or too short */
//    TRACEMS1(cinfo, 1, JTRC_APP14, (int) (datalen + remaining));
  }
}


boolean
get_interesting_appn (j_decompress_ptr cinfo)
/* Process an APP0 or APP14 marker without saving it */
{
  INT32 length;
  JOCTET b[APPN_DATA_LEN];
  uint i, numtoread;

	jpeg_source_mgr * datasrc = (cinfo)->src;
	JOCTET * next_input_byte = datasrc->next_input_byte;
	size_t bytes_in_buffer = datasrc->bytes_in_buffer;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  length = (int)(((uint) GETJOCTET(*next_input_byte++)) << 8); 

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  length += GETJOCTET(*next_input_byte++);

  length -= 2;

  /* get the interesting part of the marker data */
  if (length >= APPN_DATA_LEN)
    numtoread = (uint)APPN_DATA_LEN;
  else if (length > 0)
    numtoread = (uint) length;
  else
    numtoread = 0;
  for (i = 0; i < numtoread; i++)
  {
  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  b[i] = GETJOCTET(*next_input_byte++); 

  }
  length -= (int)numtoread;

  /* process it */
  switch (cinfo->unread_marker) {
  case M_APP0:
    examine_app0(cinfo, (JOCTET *) &b, numtoread, length);
    break;
  case M_APP14:
    examine_app14(cinfo, (JOCTET *) &b, numtoread, length);
    break;
  default:
    /* can't get here unless jpeg_save_markers chooses wrong processor */
    return FALSE; // dERREXIT1(cinfo, JERR_UNKNOWN_MARKER, cinfo->unread_marker);
//    break;
  }

  /* skip any remaining data -- could be lots */
	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;

  if (length > 0)
    (cinfo->src->skip_input_data) (cinfo, (int) length);

  return TRUE;
}


// #ifdef SAVE_MARKERS_SUPPORTED

boolean
save_marker (j_decompress_ptr cinfo)
/* Save an APPn or COM marker into the marker list */
{
  my_marker_ptr marker = (my_marker_ptr) cinfo->marker;
  jpeg_saved_marker_ptr cur_marker = marker->cur_marker;
  uint bytes_read, data_length;
  JOCTET * data;
  INT32 length = 0;

	jpeg_source_mgr * datasrc = (cinfo)->src;
	JOCTET * next_input_byte = datasrc->next_input_byte;
	size_t bytes_in_buffer = datasrc->bytes_in_buffer;

  if (cur_marker == null)
  {
    /* begin reading a marker */

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  length = (int)(((uint) GETJOCTET(*next_input_byte++)) << 8); 

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  length += GETJOCTET(*next_input_byte++);



    length -= 2;
    if (length >= 0) {		/* watch out for bogus length word */
      /* figure out how much we want to save */
      uint limit;
      if (cinfo->unread_marker == (int) M_COM)
	limit = marker->length_limit_COM;
      else
	limit = marker->length_limit_APPn[cinfo->unread_marker - (int) M_APP0];
      if ((uint) length < limit)
	limit = (uint) length;
      /* allocate and initialize the marker item */
      cur_marker = (jpeg_saved_marker_ptr)
	(cinfo->mem->alloc_large) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				    (jpeg_marker_struct'size) + limit);
      cur_marker->next = null;
      cur_marker->marker = (UINT8) cinfo->unread_marker;
      cur_marker->original_length = (uint) length;
      cur_marker->data_length = limit;
      /* data area is just beyond the jpeg_marker_struct */
      cur_marker->data = (JOCTET *) (cur_marker + 1);
      data = cur_marker->data;
      marker->cur_marker = cur_marker;
      marker->bytes_read = 0;
      bytes_read = 0;
      data_length = limit;
    } else {
      /* deal with bogus length word */
      bytes_read = 0;
      data_length = 0;
      data = null;
    }
  } else {
    /* resume reading a marker */
    bytes_read = marker->bytes_read;
    data_length = cur_marker->data_length;
    data = cur_marker->data + bytes_read;
  }

  while (bytes_read < data_length) {
	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;
    marker->bytes_read = bytes_read;
    /* If there's not at least one byte in buffer, suspend */

    if (bytes_in_buffer == 0)
    {
      if (! (datasrc->fill_input_buffer) (cinfo))
      {
        return FALSE;
      }

      next_input_byte = datasrc->next_input_byte;
      bytes_in_buffer = datasrc->bytes_in_buffer;
    }

    /* Copy bytes with reasonable rapidity */
    while (bytes_read < data_length && bytes_in_buffer > 0) {
      *data++ = *next_input_byte++;
      bytes_in_buffer--;
      bytes_read++;
    }
  }

  /* Done reading what we want to read */
  if (cur_marker != null) {	/* will be null if bogus length word */
    /* Add new marker to end of list */
    if (cinfo->marker_list == null) {
      cinfo->marker_list = cur_marker;
    } else {
      jpeg_saved_marker_ptr prev = cinfo->marker_list;
      while (prev->next != null)
	prev = prev->next;
      prev->next = cur_marker;
    }
    /* Reset pointer & calc remaining data length */
    data = cur_marker->data;
    length = (int)(cur_marker->original_length - data_length);
  }
  /* Reset to initial state for next marker */
  marker->cur_marker = null;

  /* Process the marker if interesting; else just make a generic trace msg */
  switch (cinfo->unread_marker) {
  case M_APP0:
    examine_app0(cinfo, data, data_length, length);
    break;
  case M_APP14:
    examine_app14(cinfo, data, data_length, length);
    break;
  default:
//    TRACEMS2(cinfo, 1, JTRC_MISC_MARKER, cinfo->unread_marker,
//	     (int) (data_length + length));
    break;
  }

  /* skip any remaining data -- could be lots */
	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;
  if (length > 0)
    (cinfo->src->skip_input_data) (cinfo, (int) length);

  return TRUE;
}

// #endif /* SAVE_MARKERS_SUPPORTED */


boolean
skip_variable (j_decompress_ptr cinfo)
/* Skip over an unknown or uninteresting variable-length marker */
{
  INT32 length;

	jpeg_source_mgr * datasrc = (cinfo)->src;
	JOCTET * next_input_byte = datasrc->next_input_byte;
	size_t bytes_in_buffer = datasrc->bytes_in_buffer;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }
 
  bytes_in_buffer--; 
  length = (int)(((uint) GETJOCTET(*next_input_byte++)) << 8); 

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  length += GETJOCTET(*next_input_byte++);

  length -= 2;
  
//  TRACEMS2(cinfo, 1, JTRC_MISC_MARKER, cinfo->unread_marker, (int) length);

	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;

  if (length > 0)
    (cinfo->src->skip_input_data) (cinfo, (int) length);

  return TRUE;
}


/*
 * Find the next JPEG marker, save it in cinfo->unread_marker.
 * Returns FALSE if had to suspend before reaching a marker;
 * in that case cinfo->unread_marker is unchanged.
 *
 * Note that the result might not be a valid marker code,
 * but it will never be 0 or FF.
 */

boolean
next_marker (j_decompress_ptr cinfo)
{
  int c;

	jpeg_source_mgr * datasrc = (cinfo)->src;
	JOCTET * next_input_byte = datasrc->next_input_byte;
	size_t bytes_in_buffer = datasrc->bytes_in_buffer;

  for (;;)
  {
  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  c = GETJOCTET(*next_input_byte++); 


    /* Skip any non-FF bytes.
     * This may look a bit inefficient, but it will not occur in a valid file.
     * We sync after each discarded byte so that a suspending data source
     * can discard the byte from its buffer.
     */
    while (c != 0xFF) {
      cinfo->marker->discarded_bytes++;
	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  c = GETJOCTET(*next_input_byte++); 

    }
    /* This loop swallows any duplicate FF bytes.  Extra FFs are legal as
     * pad bytes, so don't count them in discarded_bytes.  We assume there
     * will not be so many consecutive FF bytes as to overflow a suspending
     * data source's input buffer.
     */
    for (;;) {
  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  c = GETJOCTET(*next_input_byte++); 

    if (c != 0xFF) break;
  }

    if (c != 0)
      break;			/* found a valid marker, exit loop */
    /* Reach here if we found a stuffed-zero data sequence (FF/00).
     * Discard it and loop back to try again.
     */
    cinfo->marker->discarded_bytes += 2;
	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;
  }

  if (cinfo->marker->discarded_bytes != 0) {
//    WARNMS2(cinfo, JWRN_EXTRANEOUS_DATA, cinfo->marker->discarded_bytes, c);
    cinfo->marker->discarded_bytes = 0;
  }

  cinfo->unread_marker = c;

	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;
  return TRUE;
}


boolean
first_marker (j_decompress_ptr cinfo)
/* Like next_marker, but used to obtain the initial SOI marker. */
/* For this marker, we do not allow preceding garbage or fill; otherwise,
 * we might well scan an entire input file before realizing it ain't JPEG.
 * If an application wants to process non-JFIF files, it must seek to the
 * SOI before calling the JPEG library.
 */
{
  int c, c2;

	jpeg_source_mgr * datasrc = (cinfo)->src;
	JOCTET * next_input_byte = datasrc->next_input_byte;
	size_t bytes_in_buffer = datasrc->bytes_in_buffer;

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  c = GETJOCTET(*next_input_byte++); 

  if (bytes_in_buffer == 0)
  {
    if (! (datasrc->fill_input_buffer) (cinfo))
    {
      return FALSE;
    }

    next_input_byte = datasrc->next_input_byte;
    bytes_in_buffer = datasrc->bytes_in_buffer;
  }

  bytes_in_buffer--; 
  c2 = GETJOCTET(*next_input_byte++); 

  if (c != 0xFF || c2 != (int) M_SOI)
    return FALSE; // dERREXIT2(cinfo, JERR_NO_SOI, c, c2);

  cinfo->unread_marker = c2;

	 datasrc->next_input_byte = next_input_byte;
	 datasrc->bytes_in_buffer = bytes_in_buffer;
  return TRUE;
}


/*
 * Read markers until SOS or EOI.
 *
 * Returns same codes as are defined for jpeg_consume_input:
 * JPEG_SUSPENDED, JPEG_REACHED_SOS, or JPEG_REACHED_EOI.
 */

int
read_markers (j_decompress_ptr cinfo)
{
  /* Outer loop repeats once for each marker. */
  for (;;) {
    /* Collect the marker proper, unless we already did. */
    /* NB: first_marker() enforces the requirement that SOI appear first. */
    if (cinfo->unread_marker == 0) {
      if (! cinfo->marker->saw_SOI) {
	if (! first_marker(cinfo))
	  return JPEG_SUSPENDED;
      } else {
	if (! next_marker(cinfo))
	  return JPEG_SUSPENDED;
      }
    }
    /* At this point cinfo->unread_marker contains the marker code and the
     * input point is just past the marker proper, but before any parameters.
     * A suspension will cause us to return with this state still true.
     */
    switch (cinfo->unread_marker) {
    case M_SOI:
      if (! get_soi(cinfo))
	return JPEG_SUSPENDED;
      break;

    case M_SOF0:		/* Baseline */
    case M_SOF1:		/* Extended sequential, Huffman */
      if (! get_sof(cinfo, FALSE, FALSE))
	return JPEG_SUSPENDED;
      break;

    case M_SOF2:		/* Progressive, Huffman */
      if (! get_sof(cinfo, TRUE, FALSE))
	return JPEG_SUSPENDED;
      break;

    case M_SOF9:		/* Extended sequential, arithmetic */
      if (! get_sof(cinfo, FALSE, TRUE))
	return JPEG_SUSPENDED;
      break;

    case M_SOF10:		/* Progressive, arithmetic */
      if (! get_sof(cinfo, TRUE, TRUE))
	return JPEG_SUSPENDED;
      break;

    /* Currently unsupported SOFn types */
    case M_SOF3:		/* Lossless, Huffman */
    case M_SOF5:		/* Differential sequential, Huffman */
    case M_SOF6:		/* Differential progressive, Huffman */
    case M_SOF7:		/* Differential lossless, Huffman */
    case M_JPG:			/* Reserved for JPEG extensions */
    case M_SOF11:		/* Lossless, arithmetic */
    case M_SOF13:		/* Differential sequential, arithmetic */
    case M_SOF14:		/* Differential progressive, arithmetic */
    case M_SOF15:		/* Differential lossless, arithmetic */
      return JPEG_SUSPENDED; // dERREXIT1(cinfo, JERR_SOF_UNSUPPORTED, cinfo->unread_marker);
//      break;

    case M_SOS:
      if (! get_sos(cinfo))
	return JPEG_SUSPENDED;
      cinfo->unread_marker = 0;	/* processed the marker */
      return JPEG_REACHED_SOS;
    
    case M_EOI:
//      TRACEMS(cinfo, 1, JTRC_EOI);
      cinfo->unread_marker = 0;	/* processed the marker */
      return JPEG_REACHED_EOI;
      
    case M_DAC:
      if (! skip_variable (cinfo))
	return JPEG_SUSPENDED;
      break;
      
    case M_DHT:
      if (! get_dht(cinfo))
	return JPEG_SUSPENDED;
      break;
      
    case M_DQT:
      if (! get_dqt(cinfo))
	return JPEG_SUSPENDED;
      break;
      
    case M_DRI:
      if (! get_dri(cinfo))
	return JPEG_SUSPENDED;
      break;
      
    case M_APP0:
    case M_APP1:
    case M_APP2:
    case M_APP3:
    case M_APP4:
    case M_APP5:
    case M_APP6:
    case M_APP7:
    case M_APP8:
    case M_APP9:
    case M_APP10:
    case M_APP11:
    case M_APP12:
    case M_APP13:
    case M_APP14:
    case M_APP15:
      if (! (((my_marker_ptr) cinfo->marker)->process_APPn[
		cinfo->unread_marker - (int) M_APP0]) (cinfo))
	return JPEG_SUSPENDED;
      break;
      
    case M_COM:
      if (! (((my_marker_ptr) cinfo->marker)->process_COM) (cinfo))
	return JPEG_SUSPENDED;
      break;

    case M_RST0:		/* these are all parameterless */
    case M_RST1:
    case M_RST2:
    case M_RST3:
    case M_RST4:
    case M_RST5:
    case M_RST6:
    case M_RST7:
    case M_TEM:
//      TRACEMS1(cinfo, 1, JTRC_PARMLESS_MARKER, cinfo->unread_marker);
      break;

    case M_DNL:			/* Ignore DNL ... perhaps the wrong thing */
      if (! skip_variable(cinfo))
	return JPEG_SUSPENDED;
      break;

    default:			/* must be DHP, EXP, JPGn, or RESn */
      /* For now, we treat the reserved markers as fatal errors since they are
       * likely to be used to signal incompatible JPEG Part 3 extensions.
       * Once the JPEG 3 version-number marker is well defined, this code
       * ought to change!
       */
//      dERREXIT1(cinfo, JERR_UNKNOWN_MARKER, cinfo->unread_marker);
      return JPEG_SUSPENDED;
    }

    /* Successfully processed marker, so reset state variable */
    cinfo->unread_marker = 0;
  } /* end loop */
}


/*
 * Read a restart marker, which is expected to appear next in the datastream;
 * if the marker is not there, take appropriate recovery action.
 * Returns FALSE if suspension is required.
 *
 * This is called by the entropy decoder after it has read an appropriate
 * number of MCUs.  cinfo->unread_marker may be nonzero if the entropy decoder
 * has already read a marker from the data source.  Under normal conditions
 * cinfo->unread_marker will be reset to 0 before returning; if not reset,
 * it holds a marker which the decoder will be unable to read past.
 */

boolean
read_restart_marker (j_decompress_ptr cinfo)
{
  /* Obtain a marker unless we already did. */
  /* Note that next_marker will complain if it skips any data. */
  if (cinfo->unread_marker == 0) {
    if (! next_marker(cinfo))
      return FALSE;
  }

  if (cinfo->unread_marker ==
      ((int) M_RST0 + cinfo->marker->next_restart_num)) {
    /* Normal case --- swallow the marker and let entropy decoder continue */
//    TRACEMS1(cinfo, 3, JTRC_RST, cinfo->marker->next_restart_num);
    cinfo->unread_marker = 0;
  } else {
    /* Uh-oh, the restart markers have been messed up. */
    /* Let the data source manager determine how to resync. */
    if (! (cinfo->src->resync_to_restart) (cinfo,
					    cinfo->marker->next_restart_num))
      return FALSE;
  }

  /* Update next-restart state */
  cinfo->marker->next_restart_num = (cinfo->marker->next_restart_num + 1) & 7;

  return TRUE;
}


/*
 * This is the default resync_to_restart method for data source managers
 * to use if they don't have any better approach.  Some data source managers
 * may be able to back up, or may have additional knowledge about the data
 * which permits a more intelligent recovery strategy; such managers would
 * presumably supply their own resync method.
 *
 * read_restart_marker calls resync_to_restart if it finds a marker other than
 * the restart marker it was expecting.  (This code is *not* used unless
 * a nonzero restart interval has been declared.)  cinfo->unread_marker is
 * the marker code actually found (might be anything, except 0 or FF).
 * The desired restart marker number (0..7) is passed as a parameter.
 * This routine is supposed to apply whatever error recovery strategy seems
 * appropriate in order to position the input stream to the next data segment.
 * Note that cinfo->unread_marker is treated as a marker appearing before
 * the current data-source input point; usually it should be reset to zero
 * before returning.
 * Returns FALSE if suspension is required.
 *
 * This implementation is substantially constrained by wanting to treat the
 * input as a data stream; this means we can't back up.  Therefore, we have
 * only the following actions to work with:
 *   1. Simply discard the marker and let the entropy decoder resume at next
 *      byte of file.
 *   2. Read forward until we find another marker, discarding intervening
 *      data.  (In theory we could look ahead within the current bufferload,
 *      without having to discard data if we don't find the desired marker.
 *      This idea is not implemented here, in part because it makes behavior
 *      dependent on buffer size and chance buffer-boundary positions.)
 *   3. Leave the marker unread (by failing to zero cinfo->unread_marker).
 *      This will cause the entropy decoder to process an empty data segment,
 *      inserting dummy zeroes, and then we will reprocess the marker.
 *
 * #2 is appropriate if we think the desired marker lies ahead, while #3 is
 * appropriate if the found marker is a future restart marker (indicating
 * that we have missed the desired restart marker, probably because it got
 * corrupted).
 * We apply #2 or #3 if the found marker is a restart marker no more than
 * two counts behind or ahead of the expected one.  We also apply #2 if the
 * found marker is not a legal JPEG marker code (it's certainly bogus data).
 * If the found marker is a restart marker more than 2 counts away, we do #1
 * (too much risk that the marker is erroneous; with luck we will be able to
 * resync at some future point).
 * For any valid non-restart JPEG marker, we apply #3.  This keeps us from
 * overrunning the end of a scan.  An implementation limited to single-scan
 * files might find it better to apply #2 for markers other than EOI, since
 * any other marker would have to be bogus data in that case.
 */

public boolean
jpeg_resync_to_restart (j_decompress_ptr cinfo, int desired)
{
  int marker = cinfo->unread_marker;
  int action = 1;
  
  /* Always put up a warning. */
//  WARNMS2(cinfo, JWRN_MUST_RESYNC, marker, desired);
  
  /* Outer loop handles repeated decision after scanning forward. */
  for (;;) {
    if (marker < (int) M_SOF0)
      action = 2;		/* invalid marker */
    else if (marker < (int) M_RST0 || marker > (int) M_RST7)
      action = 3;		/* valid non-restart marker */
    else {
      if (marker == ((int) M_RST0 + ((desired+1) & 7)) ||
	  marker == ((int) M_RST0 + ((desired+2) & 7)))
	action = 3;		/* one of the next two expected restarts */
      else if (marker == ((int) M_RST0 + ((desired-1) & 7)) ||
	       marker == ((int) M_RST0 + ((desired-2) & 7)))
	action = 2;		/* a prior restart, so advance */
      else
	action = 1;		/* desired restart or too far away */
    }
//    TRACEMS2(cinfo, 4, JTRC_RECOVERY_ACTION, marker, action);
    switch (action) {
    case 1:
      /* Discard marker and let entropy decoder resume processing. */
      cinfo->unread_marker = 0;
      return TRUE;
    case 2:
      /* Scan to the next marker, and repeat the decision loop. */
      if (! next_marker(cinfo))
	return FALSE;
      marker = cinfo->unread_marker;
      break;
    case 3:
      /* Return without advancing past this marker. */
      /* Entropy decoder will be forced to process an empty segment. */
      return TRUE;
    default:
      break;
    }
  } /* end loop */
}


/*
 * Reset marker processing state to begin a fresh datastream.
 */

void
reset_marker_reader (j_decompress_ptr cinfo)
{
  my_marker_ptr marker = (my_marker_ptr) cinfo->marker;

  cinfo->comp_info = null;		/* until allocated by get_sof */
  cinfo->input_scan_number = 0;		/* no SOS seen yet */
  cinfo->unread_marker = 0;		/* no pending marker */
  marker->pub.saw_SOI = FALSE;		/* set internal state too */
  marker->pub.saw_SOF = FALSE;
  marker->pub.discarded_bytes = 0;
  marker->cur_marker = null;
}


/*
 * Initialize the marker reader module.
 * This is called only once, when the decompression object is created.
 */

public void
jinit_marker_reader (j_decompress_ptr cinfo)
{
  my_marker_ptr marker;
  int i;

  /* Create subobject in permanent pool */
  marker = (my_marker_ptr)
    (cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				(my_marker_reader'size));

  cinfo->marker = (jpeg_marker_reader *) marker;
  /* Initialize public method pointers */
  marker->pub.reset_marker_reader = reset_marker_reader;
  marker->pub.read_markers = read_markers;
  marker->pub.read_restart_marker = read_restart_marker;
  /* Initialize COM/APPn processing.
   * By default, we examine and then discard APP0 and APP14,
   * but simply discard COM and all other APPn.
   */
  marker->process_COM = skip_variable;
  marker->length_limit_COM = 0;
  for (i = 0; i < 16; i++) {
    marker->process_APPn[i] = skip_variable;
    marker->length_limit_APPn[i] = 0;
  }
  marker->process_APPn[0] = get_interesting_appn;
  marker->process_APPn[14] = get_interesting_appn;
  /* Reset marker processing state */
  reset_marker_reader(cinfo);
}

#end unsafe

