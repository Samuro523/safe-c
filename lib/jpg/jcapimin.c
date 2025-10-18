/*
 * jcapimin.c
 *
 * Copyright (C) 1994-1998, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains application interface code for the compression half
 * of the JPEG library.  These are the "minimum" API routines that may be
 * needed in either the normal full-compression case or the transcoding-only
 * case.
 *
 * Most of the routines intended to be called directly by an application
 * are in this file or in jcapistd.c.  But also see jcparam.c for
 * parameter-setup helper routines, jcomapi.c for routines shared by
 * compression and decompression, and jctrans.c for the transcoding case.
 */

use jpeglib, jmorecfg, jutils, jmemmgr, jcomapi;


#begin unsafe

/*
 * Initialization of a JPEG compression object.
 * The error manager must already be set up (in case memory manager fails).
 */

public int
jpeg_CreateCompress (j_compress_ptr cinfo, int version, size_t structsize)
{
  int i;

  /* Guard against version mismatches between library and caller. */
  cinfo->mem = null;		/* so jpeg_destroy knows mem mgr not called */
  if (version != JPEG_LIB_VERSION)
    return JERR_BAD_LIB_VERSION; // cERREXIT2(cinfo, JERR_BAD_LIB_VERSION, JPEG_LIB_VERSION, version);
  if (structsize != (jpeg_compress_struct'size))
    return JERR_BAD_STRUCT_SIZE; // cERREXIT2(cinfo, JERR_BAD_STRUCT_SIZE, (int) (jpeg_compress_struct'size), (int) structsize);

  /* For debugging purposes, we zero the whole master structure.
   * But the application has already set the err pointer, and may have set
   * client_data, so we have to save and restore those fields.
   * Note: if application hasn't set client_data, tools like Purify may
   * complain here.
   */
  {
    jpeg_error_mgr * err = cinfo->err;
    byte * client_data = cinfo->client_data; /* ignore Purify complaint here */
    jzero_far ((byte *)cinfo, (jpeg_compress_struct'size));
    cinfo->err = err;
    cinfo->client_data = client_data;
  }
  cinfo->is_decompressor = FALSE;

  /* Initialize a memory manager instance for this object */
  jinit_memory_mgr((j_common_ptr) cinfo);

  /* Zero out pointers to permanent structures. */
  cinfo->progress = null;
  cinfo->dest = null;

  cinfo->comp_info = null;

  for (i = 0; i < NUM_QUANT_TBLS; i++)
    cinfo->quant_tbl_ptrs[i] = null;

  for (i = 0; i < NUM_HUFF_TBLS; i++) {
    cinfo->dc_huff_tbl_ptrs[i] = null;
    cinfo->ac_huff_tbl_ptrs[i] = null;
  }

  cinfo->script_space = null;

  cinfo->input_gamma = 1.0;	/* in case application forgets */

  /* OK, I'm ready */
  cinfo->global_state = CSTATE_START;

  return 0;
}



/*
 * Forcibly suppress or un-suppress all quantization and Huffman tables.
 * Marks all currently defined tables as already written (if suppress)
 * or not written (if !suppress).  This will control whether they get emitted
 * by a subsequent jpeg_start_compress call.
 *
 * This routine is exported for use by applications that want to produce
 * abbreviated JPEG datastreams.  It logically belongs in jcparam.c, but
 * since it is called by jpeg_start_compress, we put it here --- otherwise
 * jcparam.o would be linked whether the application used it or not.
 */

public void
jpeg_suppress_tables (j_compress_ptr cinfo, boolean suppress)
{
  int i;
  JQUANT_TBL * qtbl;
  JHUFF_TBL * htbl;

  for (i = 0; i < NUM_QUANT_TBLS; i++)
  {
    qtbl = cinfo->quant_tbl_ptrs[i];
    if (qtbl != null)
      qtbl->sent_table = suppress;
  }

  for (i = 0; i < NUM_HUFF_TBLS; i++)
  {
    htbl = cinfo->dc_huff_tbl_ptrs[i];
    if (htbl != null)
      htbl->sent_table = suppress;
    htbl = cinfo->ac_huff_tbl_ptrs[i];
    if (htbl != null)
      htbl->sent_table = suppress;
  }
}


/*
 * Finish JPEG compression.
 *
 * If a multipass operating mode was selected, this may do a great deal of
 * work including most of the actual output.
 */

public int
jpeg_finish_compress (j_compress_ptr cinfo)
{
  JDIMENSION iMCU_row;
  int rc;

  if (cinfo->global_state == CSTATE_SCANNING ||
      cinfo->global_state == CSTATE_RAW_OK) {

    /* Terminate first pass */
    if (cinfo->next_scanline < cinfo->image_height)
      return JERR_TOO_LITTLE_DATA; // cERREXIT(cinfo, JERR_TOO_LITTLE_DATA);

    cinfo->master->finish_pass (cinfo);

  } else if (cinfo->global_state != CSTATE_WRCOEFS)
    return JERR_BAD_STATE; // cERREXIT1(cinfo, JERR_BAD_STATE, cinfo->global_state);

  /* Perform any remaining passes */
  while (! cinfo->master->is_last_pass) {
    rc = cinfo->master->iprepare_for_pass (cinfo);
    if (rc != 0)
      return rc;

    for (iMCU_row = 0; iMCU_row < cinfo->total_iMCU_rows; iMCU_row++) {
      if (cinfo->progress != null) {
	cinfo->progress->pass_counter = (int) iMCU_row;
	cinfo->progress->pass_limit = (int) cinfo->total_iMCU_rows;
	cinfo->progress->progress_monitor ((j_common_ptr) cinfo);
      }
      /* We bypass the main controller and invoke coef controller directly;
       * all work is being done from the coefficient buffer.
       */
      if (! cinfo->coef->compress_data (cinfo, (JSAMPIMAGE) null))
	return JERR_CANT_SUSPEND; // cERREXIT(cinfo, JERR_CANT_SUSPEND);
    }
    cinfo->master->finish_pass (cinfo);
  }
  /* Write EOI, do final cleanup */
  cinfo->marker->write_file_trailer (cinfo);
  if (cinfo->dest->iterm_destination (cinfo) == FALSE)
    return JERR_CANT_SUSPEND;

  /* We can use jpeg_abort to release memory and reset global_state */
  jpeg_abort((j_common_ptr) cinfo);

  return 0;
}


/*
 * Write a special marker.
 * This is only recommended for writing COM or APPn markers.
 * Must be called after jpeg_start_compress() and before
 * first call to jpeg_write_scanlines() or jpeg_write_raw_data().
 */

package P
  typedef void WRITE_MARKER_BYTE (j_compress_ptr info, int val);
end P;

public void
jpeg_write_marker (j_compress_ptr cinfo, int marker,
		   JOCTET *dataptr, uint datalen)
{
  WRITE_MARKER_BYTE write_marker_byte;
  uint datalen2 = datalen;
  JOCTET *dataptr2 = dataptr;

  if (cinfo->next_scanline != 0 ||
      (cinfo->global_state != CSTATE_SCANNING &&
       cinfo->global_state != CSTATE_RAW_OK &&
       cinfo->global_state != CSTATE_WRCOEFS))
    abort; //  cERREXIT1(cinfo, JERR_BAD_STATE, cinfo->global_state);

  cinfo->marker->write_marker_header (cinfo, marker, datalen2);
  write_marker_byte = cinfo->marker->write_marker_byte;	/* copy for speed */
  while (datalen2-- > 0) {
    write_marker_byte (cinfo, *dataptr2);
    dataptr2++;
  }
}

/* Same, but piecemeal. */

public void
jpeg_write_m_header (j_compress_ptr cinfo, int marker, uint datalen)
{
  if (cinfo->next_scanline != 0 ||
      (cinfo->global_state != CSTATE_SCANNING &&
       cinfo->global_state != CSTATE_RAW_OK &&
       cinfo->global_state != CSTATE_WRCOEFS))
    abort; // cERREXIT1(cinfo, JERR_BAD_STATE, cinfo->global_state);

  cinfo->marker->write_marker_header (cinfo, marker, datalen);
}

public void
jpeg_write_m_byte (j_compress_ptr cinfo, int val)
{
  cinfo->marker->write_marker_byte (cinfo, val);
}

#end unsafe
