/*
 * jmemnobs.c
 *
 * Copyright (C) 1992-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file provides a really simple implementation of the system-
 * dependent portion of the JPEG memory manager.  This implementation
 * assumes that no backing-store files are needed: all required space
 * can be obtained from malloc().
 * This is very portable in the sense that it'll compile on almost anything,
 * but you'd better have lots of main memory (or virtual memory) if you want
 * to process big images.
 * Note that the max_memory_to_use option is ignored by this implementation.
 */

use jpeglib;


#begin unsafe

/*
 * Memory allocation and freeing are controlled by the regular library
 * routines malloc() and free().
 */

/*
public byte *
jpeg_get_small (j_common_ptr cinfo, size_t sizeofobject)
{
  return (void *) malloc(sizeofobject);
}

public void
jpeg_free_small (j_common_ptr cinfo, void * object, size_t sizeofobject)
{
  free(object);
}
*/


/*
 * "Large" objects are treated the same as "small" ones.
 * NB: although we include FAR keywords in the routine declarations,
 * this file won't actually work in 80x86 small/medium model; at least,
 * you probably won't be able to process useful-size images in only 64KB.
 */


/*
public void FAR *)
jpeg_get_large (j_common_ptr cinfo, size_t sizeofobject)
{
  return (void FAR *) malloc(sizeofobject);
}

GLOBAL(void)
jpeg_free_large (j_common_ptr cinfo, void FAR * object, size_t sizeofobject)
{
  free(object);
}
*/

/*
 * This routine computes the total memory space available for allocation.
 * Here we always say, "we got all you want bud!"
 */

public uint
jpeg_mem_available (j_common_ptr cinfo, uint min_bytes_needed,
		    uint max_bytes_needed, uint already_allocated)
{
  _unused cinfo;
  _unused min_bytes_needed;
  _unused already_allocated;

  return max_bytes_needed;
}


/*
 * Backing store (temporary file) management.
 * Since jpeg_mem_available always promised the moon,
 * this should never be called and we can just error out.
 */

public void
jpeg_open_backing_store (j_common_ptr cinfo, backing_store_ptr info,
			 uint total_bytes_needed)
{
  _unused cinfo;
  _unused info;
  _unused total_bytes_needed;

  abort; // cERREXIT((j_compress_ptr)cinfo, JERR_NO_BACKING_STORE);
}


/*
 * These routines take care of any system-dependent initialization and
 * cleanup required.  Here, there isn't any.
 */

public int
jpeg_mem_init (j_common_ptr cinfo)
{
_unused cinfo;
  return 0;			/* just set max_memory_to_use to 0 */
}

public void
jpeg_mem_term (j_common_ptr cinfo)
{
  _unused cinfo;
  /* no work */
}

#end unsafe
