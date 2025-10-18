/*
 * jmemmgr.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains the JPEG system-independent memory management
 * routines.  This code is usable across a wide variety of machines; most
 * of the system dependencies have been isolated in a separate file.
 * The major functions provided here are:
 *   * pool-based allocation and freeing of memory;
 *   * policy decisions about how to divide available memory among the
 *     virtual arrays;
 *   * control logic for swapping virtual arrays between main memory and
 *     backing storage.
 * The separate system-dependent file provides the actual backing-storage
 * access code, and it contains the policy decision about how much total
 * main memory to use.
 * This file is system-dependent in the sense that some of its functions
 * are unnecessary in some systems.  For example, if there is enough virtual
 * memory so that backing storage will never be used, much of the virtual
 * array control logic could be removed.  (Of course, if you have that much
 * memory then you shouldn't care about a little bit of unused code...)
 */

use jpeglib, jmorecfg, jmemsys, jutils; 	/* import the system-dependent declarations */

#begin unsafe


/*
 * Some important notes:
 *   The allocation routines provided here must never return null.
 *   They should exit to error_exit if unsuccessful.
 *
 *   It's not a good idea to try to merge the sarray and barray routines,
 *   even though they are textually almost the same, because samples are
 *   usually stored as bytes while coefficients are shorts or ints.  Thus,
 *   in machines where byte pointers have a different representation from
 *   word pointers, the resulting machine code could not be the same.
 */


/*
 * Many machines require storage alignment: longs must start on 4-byte
 * boundaries, doubles on 8-byte boundaries, etc.  On such machines, malloc()
 * always returns pointers that are multiples of the worst-case alignment
 * requirement, and we had better do so too.
 * There isn't any really portable way to determine the worst-case alignment
 * requirement.  This module assumes that the alignment requirement is
 * multiples of sizeof(ALIGN_TYPE).
 * By default, we define ALIGN_TYPE as double.  This is necessary on some
 * workstations (where doubles really do need 8-byte alignment) and will work
 * fine on nearly everything.  If your machine has lesser alignment needs,
 * you can save a few bytes by making ALIGN_TYPE smaller.
 * The only place I know of where this will NOT work is certain Macintosh
 * 680x0 compilers that define double as a 10-byte IEEE extended float.
 * Doing 10-byte alignment is counterproductive because longwords won't be
 * aligned well.  Put "#define ALIGN_TYPE long" in jconfig.h if you have
 * such a compiler.
 */



/*
 * Allocation of "small" objects.
 *
 * For these, we use pooled storage.  When a new pool must be created,
 * we try to get enough space for the current request plus a "slop" factor,
 * where the slop will be the amount of leftover space in the new pool.
 * The speed vs. space tradeoff is largely determined by the slop values.
 * A different slop value is provided for each pool class (lifetime),
 * and we also distinguish the first pool of a class from later ones.
 * NOTE: the values given work fairly well on both 16- and 32-bit-int
 * machines, but may be too small if longs are 64 bits or more.
 */

const size_t first_pool_slop[JPOOL_NUMPOOLS] = 
{
	1600,			/* first PERMANENT pool */
	16000			/* first IMAGE pool */
};

const size_t extra_pool_slop[JPOOL_NUMPOOLS] = 
{
	0,			/* additional PERMANENT pools */
	5000			/* additional IMAGE pools */
};

const uint MIN_SLOP = 50;		/* greater than 0 to avoid futile looping */



void
out_of_memory (j_common_ptr cinfo, int which)
/* Report an out-of-memory error and stop execution */
/* If we compiled MEM_STATS support, report alloc requests before dying */
{
  _unused cinfo;
  _unused which;
  abort;  // dERREXIT1((j_decompress_ptr) cinfo, JERR_OUT_OF_MEMORY, which);
}


byte *
alloc_small (j_common_ptr cinfo, int pool_id, size_t sizeofobject0)
/* Allocate a "small" object */
{
  my_mem_ptr mem = (my_mem_ptr) cinfo->mem;
  small_pool_ptr hdr_ptr, prev_hdr_ptr;
  char * data_ptr;
  size_t odd_bytes, min_request, slop;
  size_t sizeofobject = sizeofobject0;

  /* Check for unsatisfiable request (do now to ensure no overflow below) */
  if (sizeofobject > (size_t) (MAX_ALLOC_CHUNK-(small_pool_hdr'size)))
    out_of_memory(cinfo, 1);	/* request exceeds malloc's ability */

  /* Round up the requested size to a multiple of SIZEOF(ALIGN_TYPE) */
  odd_bytes = sizeofobject % (ALIGN_TYPE'size);
  if (odd_bytes > 0)
    sizeofobject += (ALIGN_TYPE'size) - odd_bytes;

  /* See if space is available in any existing pool */
  if (pool_id < 0 || pool_id >= JPOOL_NUMPOOLS)
    abort; // dERREXIT1((j_decompress_ptr)cinfo, JERR_BAD_POOL_ID, pool_id);	/* safety check */
  prev_hdr_ptr = null;
  hdr_ptr = mem->small_list[pool_id];
  while (hdr_ptr != null) {
    if (hdr_ptr->hdr.bytes_left >= sizeofobject)
      break;			/* found pool with enough space */
    prev_hdr_ptr = hdr_ptr;
    hdr_ptr = hdr_ptr->hdr.next;
  }

  /* Time to make a new pool? */
  if (hdr_ptr == null) {

    byte[]^ p;

    /* min_request is what we need now, slop is what will be leftover */
    min_request = sizeofobject + (small_pool_hdr'size);
    if (prev_hdr_ptr == null)	/* first pool in class? */
      slop = first_pool_slop[pool_id];
    else
      slop = extra_pool_slop[pool_id];
    /* Don't ask for more than MAX_ALLOC_CHUNK */
    if (slop > (size_t) (MAX_ALLOC_CHUNK-min_request))
      slop = (size_t) (MAX_ALLOC_CHUNK-min_request);
    /* Try to get space, if fail reduce slop and try again */
    {
      p = new byte[min_request + slop];

      hdr_ptr = (small_pool_ptr)&(p^);  // (small_pool_ptr) jpeg_get_small(cinfo, min_request + slop);

#if 0
      if (hdr_ptr != null)
	break;
      slop /= 2;
      if (slop < MIN_SLOP)	/* give up when it gets real small */
	out_of_memory(cinfo, 2); /* jpeg_get_small failed */
#endif
    }
    mem->total_space_allocated += (int)(min_request + slop);
    /* Success, initialize the new pool header and add to end of list */
    hdr_ptr->hdr.next = null;
    hdr_ptr->hdr.bytes_used = 0;
    hdr_ptr->hdr.bytes_left = sizeofobject + slop;
    hdr_ptr->hdr.this = p;

    if (prev_hdr_ptr == null)	/* first pool in class? */
      mem->small_list[pool_id] = hdr_ptr;
    else
      prev_hdr_ptr->hdr.next = hdr_ptr;
  }

  /* OK, allocate the object from the current pool */
  data_ptr = (char *) (hdr_ptr + 1); /* point to first data byte in pool */

  data_ptr += hdr_ptr->hdr.bytes_used; /* point to place for object */
  hdr_ptr->hdr.bytes_used += sizeofobject;
  hdr_ptr->hdr.bytes_left -= sizeofobject;

  return (byte *) data_ptr;
}


/*
 * Allocation of "large" objects.
 *
 * The external semantics of these are the same as "small" objects,
 * except that FAR pointers are used on 80x86.  However the pool
 * management heuristics are quite different.  We assume that each
 * request is large enough that it may as well be passed directly to
 * jpeg_get_large; the pool management just links everything together
 * so that we can free it all on demand.
 * Note: the major use of "large" objects is in JSAMPARRAY and JBLOCKARRAY
 * structures.  The routines that create these structures (see below)
 * deliberately bunch rows together to ensure a large request size.
 */

byte *
alloc_large (j_common_ptr cinfo, int pool_id, size_t sizeofobject0)
/* Allocate a "large" object */
{
  my_mem_ptr mem = (my_mem_ptr) cinfo->mem;
  large_pool_ptr hdr_ptr;
  size_t odd_bytes;
  size_t sizeofobject = sizeofobject0;
  byte[]^ p;

  /* Check for unsatisfiable request (do now to ensure no overflow below) */
  if (sizeofobject > (size_t) (MAX_ALLOC_CHUNK-(large_pool_hdr'size)))
    out_of_memory(cinfo, 3);	/* request exceeds malloc's ability */

  /* Round up the requested size to a multiple of SIZEOF(ALIGN_TYPE) */
  odd_bytes = sizeofobject % (ALIGN_TYPE'size);
  if (odd_bytes > 0)
    sizeofobject += (ALIGN_TYPE'size) - odd_bytes;

  /* Always make a new pool */
  if (pool_id < 0 || pool_id >= JPOOL_NUMPOOLS)
    abort; // dERREXIT1((j_decompress_ptr)cinfo, JERR_BAD_POOL_ID, pool_id);	/* safety check */

  p = new byte[sizeofobject + (large_pool_hdr'size)];
  hdr_ptr = (large_pool_ptr)&p^;
//  hdr_ptr = (large_pool_ptr) jpeg_get_large(cinfo, sizeofobject + (large_pool_hdr'size));

//  if (hdr_ptr == null)
//    out_of_memory(cinfo, 4);	/* jpeg_get_large failed */

  mem->total_space_allocated += (int)(sizeofobject + (large_pool_hdr'size));

  /* Success, initialize the new pool header and add to list */
  hdr_ptr->hdr.next = mem->large_list[pool_id];
  /* We maintain space counts in each pool header for statistical purposes,
   * even though they are not needed for allocation.
   */
  hdr_ptr->hdr.bytes_used = sizeofobject;
  hdr_ptr->hdr.bytes_left = 0;
  hdr_ptr->hdr.this = p;
  mem->large_list[pool_id] = hdr_ptr;

  return (byte *) (hdr_ptr + 1); /* point to first data byte in pool */
}


/*
 * Creation of 2-D sample arrays.
 * The pointers are in near heap, the samples themselves in FAR heap.
 *
 * To minimize allocation overhead and to allow I/O of large contiguous
 * blocks, we allocate the sample rows in groups of as many rows as possible
 * without exceeding MAX_ALLOC_CHUNK total bytes per allocation request.
 * NB: the virtual array control routines, later in this file, know about
 * this chunking of rows.  The rowsperchunk value is left in the mem manager
 * object so that it can be saved away if this sarray is the workspace for
 * a virtual array.
 */

JSAMPARRAY
alloc_sarray (j_common_ptr cinfo, int pool_id,
	      JDIMENSION samplesperrow, JDIMENSION numrows)
/* Allocate a 2-D sample array */
{
  my_mem_ptr mem = (my_mem_ptr) cinfo->mem;
  JSAMPARRAY result;
  JSAMPROW workspace;
  JDIMENSION rowsperchunk, currow, i;
  uint ltemp;

  /* Calculate max # of rows allowed in one allocation chunk */
  ltemp = (MAX_ALLOC_CHUNK-(large_pool_hdr'size)) /
	  ((uint) samplesperrow * (JSAMPLE'size));
  if (ltemp <= 0)
    abort;  // should never occur ; dERREXIT((j_decompress_ptr)cinfo, JERR_WIDTH_OVERFLOW);
  if (ltemp < (uint) numrows)
    rowsperchunk = (JDIMENSION) ltemp;
  else
    rowsperchunk = numrows;
  mem->last_rowsperchunk = rowsperchunk;

  /* Get space for row pointers (small object) */
  result = (JSAMPARRAY) alloc_small(cinfo, pool_id,
				    (size_t) (numrows * (JSAMPROW'size)));

  /* Get the rows themselves (large objects) */
  currow = 0;
  while (currow < numrows) {
    rowsperchunk = MIN(rowsperchunk, numrows - currow);
    workspace = (JSAMPROW) alloc_large(cinfo, pool_id,
	(size_t) ((size_t) rowsperchunk * (size_t) samplesperrow
		  * (JSAMPLE'size)));
    for (i = rowsperchunk; i > 0; i--) {
      result[currow++] = workspace;
      workspace += samplesperrow;
    }
  }

  return result;
}


/*
 * Creation of 2-D coefficient-block arrays.
 * This is essentially the same as the code for sample arrays, above.
 */

JBLOCKARRAY
alloc_barray (j_common_ptr cinfo, int pool_id,
	      JDIMENSION blocksperrow, JDIMENSION numrows)
/* Allocate a 2-D coefficient-block array */
{
  my_mem_ptr mem = (my_mem_ptr) cinfo->mem;
  JBLOCKARRAY result;
  JBLOCKROW workspace;
  JDIMENSION rowsperchunk, currow, i;
  uint ltemp;

  /* Calculate max # of rows allowed in one allocation chunk */
  ltemp = (MAX_ALLOC_CHUNK-(large_pool_hdr'size)) /
	  ((uint) blocksperrow * (JBLOCK'size));
  if (ltemp <= 0)
    abort;  // should never occur ; cERREXIT((j_compress_ptr)cinfo, JERR_WIDTH_OVERFLOW);
  if (ltemp < (uint) numrows)
    rowsperchunk = (JDIMENSION) ltemp;
  else
    rowsperchunk = numrows;
  mem->last_rowsperchunk = rowsperchunk;

  /* Get space for row pointers (small object) */
  result = (JBLOCKARRAY) alloc_small(cinfo, pool_id,
				     (size_t) (numrows * (JBLOCKROW'size)));

  /* Get the rows themselves (large objects) */
  currow = 0;
  while (currow < numrows) {
    rowsperchunk = MIN(rowsperchunk, numrows - currow);
    workspace = (JBLOCKROW) alloc_large(cinfo, pool_id,
	(size_t) ((size_t) rowsperchunk * (size_t) blocksperrow
		  * (JBLOCK'size)));
    for (i = rowsperchunk; i > 0; i--) {
      result[currow++] = workspace;
      workspace += blocksperrow;
    }
  }

  return result;
}


/*
 * About virtual array management:
 *
 * The above "normal" array routines are only used to allocate strip buffers
 * (as wide as the image, but just a few rows high).  Full-image-sized buffers
 * are handled as "virtual" arrays.  The array is still accessed a strip at a
 * time, but the memory manager must save the whole array for repeated
 * accesses.  The intended implementation is that there is a strip buffer in
 * memory (as high as is possible given the desired memory limit), plus a
 * backing file that holds the rest of the array.
 *
 * The request_virt_array routines are told the total size of the image and
 * the maximum number of rows that will be accessed at once.  The in-memory
 * buffer must be at least as large as the maxaccess value.
 *
 * The request routines create control blocks but not the in-memory buffers.
 * That is postponed until realize_virt_arrays is called.  At that time the
 * total amount of space needed is known (approximately, anyway), so free
 * memory can be divided up fairly.
 *
 * The access_virt_array routines are responsible for making a specific strip
 * area accessible (after reading or writing the backing file, if necessary).
 * Note that the access routines are told whether the caller intends to modify
 * the accessed strip; during a read-only pass this saves having to rewrite
 * data to disk.  The access routines are also responsible for pre-zeroing
 * any newly accessed rows, if pre-zeroing was requested.
 *
 * In current usage, the access requests are usually for nonoverlapping
 * strips; that is, successive access start_row numbers differ by exactly
 * num_rows = maxaccess.  This means we can get good performance with simple
 * buffer dump/reload logic, by making the in-memory buffer be a multiple
 * of the access height; then there will never be accesses across bufferload
 * boundaries.  The code will still work with overlapping access requests,
 * but it doesn't handle bufferload overlaps very efficiently.
 */


jvirt_sarray_ptr
request_virt_sarray (j_common_ptr cinfo, int pool_id, boolean pre_zero,
		     JDIMENSION samplesperrow, JDIMENSION numrows,
		     JDIMENSION maxaccess)
/* Request a virtual 2-D sample array */
{
  my_mem_ptr mem = (my_mem_ptr) cinfo->mem;
  jvirt_sarray_ptr result;

  /* Only IMAGE-lifetime virtual arrays are currently supported */
  if (pool_id != JPOOL_IMAGE)
    abort; // cERREXIT1((j_compress_ptr)cinfo, JERR_BAD_POOL_ID, pool_id);	/* safety check */

  /* get control block */
  result = (jvirt_sarray_ptr) alloc_small(cinfo, pool_id,
					  (jvirt_sarray_control'size));

  result->mem_buffer = null;	/* marks array not yet realized */
  result->rows_in_array = numrows;
  result->samplesperrow = samplesperrow;
  result->maxaccess = maxaccess;
  result->pre_zero = pre_zero;
  result->b_s_open = FALSE;	/* no associated backing-store object */
  result->next = mem->virt_sarray_list; /* add to list of virtual arrays */
  mem->virt_sarray_list = result;

  return result;
}


jvirt_barray_ptr
request_virt_barray (j_common_ptr cinfo, int pool_id, boolean pre_zero,
		     JDIMENSION blocksperrow, JDIMENSION numrows,
		     JDIMENSION maxaccess)
/* Request a virtual 2-D coefficient-block array */
{
  my_mem_ptr mem = (my_mem_ptr) cinfo->mem;
  jvirt_barray_ptr result;

  /* Only IMAGE-lifetime virtual arrays are currently supported */
  if (pool_id != JPOOL_IMAGE)
    abort; // cERREXIT1((j_compress_ptr)cinfo, JERR_BAD_POOL_ID, pool_id);	/* safety check */

  /* get control block */
  result = (jvirt_barray_ptr) alloc_small(cinfo, pool_id,
					  (jvirt_barray_control'size));

  result->mem_buffer = null;	/* marks array not yet realized */
  result->rows_in_array = numrows;
  result->blocksperrow = blocksperrow;
  result->maxaccess = maxaccess;
  result->pre_zero = pre_zero;
  result->b_s_open = FALSE;	/* no associated backing-store object */
  result->next = mem->virt_barray_list; /* add to list of virtual arrays */
  mem->virt_barray_list = result;

  return result;
}


void
realize_virt_arrays (j_common_ptr cinfo)
/* Allocate the in-memory buffers for any unrealized virtual arrays */
{
  my_mem_ptr mem = (my_mem_ptr) cinfo->mem;
  uint space_per_minheight, maximum_space, avail_mem;
  uint minheights, max_minheights;
  jvirt_sarray_ptr sptr;
  jvirt_barray_ptr bptr;

  /* Compute the minimum space needed (maxaccess rows in each buffer)
   * and the maximum space needed (full image height in each buffer).
   * These may be of use to the system-dependent jpeg_mem_available routine.
   */
  space_per_minheight = 0;
  maximum_space = 0;
  for (sptr = mem->virt_sarray_list; sptr != null; sptr = sptr->next) {
    if (sptr->mem_buffer == null) { /* if not realized yet */
      space_per_minheight += (uint) sptr->maxaccess *
			     (uint) sptr->samplesperrow * (JSAMPLE'size);
      maximum_space += (uint) sptr->rows_in_array *
		       (uint) sptr->samplesperrow * (JSAMPLE'size);
    }
  }
  for (bptr = mem->virt_barray_list; bptr != null; bptr = bptr->next) {
    if (bptr->mem_buffer == null) { /* if not realized yet */
      space_per_minheight += (uint) bptr->maxaccess *
			     (uint) bptr->blocksperrow * (JBLOCK'size);
      maximum_space += (uint) bptr->rows_in_array *
		       (uint) bptr->blocksperrow * (JBLOCK'size);
    }
  }

  if (space_per_minheight <= 0)
    return;			/* no unrealized arrays, no work */

  /* Determine amount of memory to actually use; this is system-dependent. */
  avail_mem = jpeg_mem_available(cinfo, space_per_minheight, maximum_space,
				 (uint)mem->total_space_allocated);

  /* If the maximum space needed is available, make all the buffers full
   * height; otherwise parcel it out with the same number of minheights
   * in each buffer.
   */
  if (avail_mem >= maximum_space)
    max_minheights = 1000000000;
  else {
    max_minheights = avail_mem / space_per_minheight;
    /* If there doesn't seem to be enough space, try to get the minimum
     * anyway.  This allows a "stub" implementation of jpeg_mem_available().
     */
    if (max_minheights <= 0)
      max_minheights = 1;
  }

  /* Allocate the in-memory buffers and initialize backing store as needed. */

  for (sptr = mem->virt_sarray_list; sptr != null; sptr = sptr->next) {
    if (sptr->mem_buffer == null) { /* if not realized yet */
      minheights = ((uint) sptr->rows_in_array - 1) / sptr->maxaccess + 1;
      if (minheights <= max_minheights) {
	/* This buffer fits in memory */
	sptr->rows_in_mem = sptr->rows_in_array;
      } else {
	/* It doesn't fit in memory, create backing store. */
	sptr->rows_in_mem = (JDIMENSION) (max_minheights * sptr->maxaccess);
	jpeg_open_backing_store(cinfo, & sptr->b_s_info,
				(uint) sptr->rows_in_array *
				(uint) sptr->samplesperrow *
				(uint) (JSAMPLE'size));
	sptr->b_s_open = TRUE;
      }
      sptr->mem_buffer = alloc_sarray(cinfo, JPOOL_IMAGE,
				      sptr->samplesperrow, sptr->rows_in_mem);
      sptr->rowsperchunk = mem->last_rowsperchunk;
      sptr->cur_start_row = 0;
      sptr->first_undef_row = 0;
      sptr->dirty = FALSE;
    }
  }

  for (bptr = mem->virt_barray_list; bptr != null; bptr = bptr->next) {
    if (bptr->mem_buffer == null) { /* if not realized yet */
      minheights = ((uint) bptr->rows_in_array - 1) / bptr->maxaccess + 1;
      if (minheights <= max_minheights) {
	/* This buffer fits in memory */
	bptr->rows_in_mem = bptr->rows_in_array;
      } else {
	/* It doesn't fit in memory, create backing store. */
	bptr->rows_in_mem = (JDIMENSION) (max_minheights * bptr->maxaccess);
	jpeg_open_backing_store(cinfo, & bptr->b_s_info,
				(uint) bptr->rows_in_array *
				(uint) bptr->blocksperrow *
				(uint) (JBLOCK'size));
	bptr->b_s_open = TRUE;
      }
      bptr->mem_buffer = alloc_barray(cinfo, JPOOL_IMAGE,
				      bptr->blocksperrow, bptr->rows_in_mem);
      bptr->rowsperchunk = mem->last_rowsperchunk;
      bptr->cur_start_row = 0;
      bptr->first_undef_row = 0;
      bptr->dirty = FALSE;
    }
  }
}


void
do_sarray_io (j_common_ptr cinfo, jvirt_sarray_ptr ptr, boolean writing)
/* Do backing store read or write of a virtual sample array */
{
  uint bytesperrow, file_offset, byte_count, rows, thisrow, i;

  bytesperrow = (uint) ptr->samplesperrow * (JSAMPLE'size);
  file_offset = ptr->cur_start_row * bytesperrow;
  /* Loop to read or write each allocation chunk in mem_buffer */
  for (i = 0; i < (uint) ptr->rows_in_mem; i += ptr->rowsperchunk) {
    /* One chunk, but check for short chunk at end of buffer */
    rows = MIN((uint) ptr->rowsperchunk, (uint) ptr->rows_in_mem - i);
    /* Transfer no more than is currently defined */
    thisrow = (uint) ptr->cur_start_row + i;
    rows = MIN(rows, (uint) ptr->first_undef_row - thisrow);
    /* Transfer no more than fits in file */
    rows = MIN(rows, (uint) ptr->rows_in_array - thisrow);
    if (rows <= 0)		/* this chunk might be past end of file! */
      break;
    byte_count = rows * bytesperrow;

_unused cinfo;
_unused writing;

/*
    if (writing)
      ptr->b_s_info.write_backing_store (cinfo, & ptr->b_s_info,
					    (byte *) ptr->mem_buffer[i],
					    file_offset, byte_count);
    else
      ptr->b_s_info.read_backing_store (cinfo, & ptr->b_s_info,
					   (byte *) ptr->mem_buffer[i],
					   file_offset, byte_count);
*/

    file_offset += byte_count;
  }
}


void
do_barray_io (j_common_ptr cinfo, jvirt_barray_ptr ptr, boolean writing)
/* Do backing store read or write of a virtual coefficient-block array */
{
  uint bytesperrow, file_offset, byte_count, rows, thisrow, i;

  bytesperrow = (uint) ptr->blocksperrow * (JBLOCK'size);
  file_offset = ptr->cur_start_row * bytesperrow;
  /* Loop to read or write each allocation chunk in mem_buffer */
  for (i = 0; i < (uint) ptr->rows_in_mem; i += ptr->rowsperchunk) {
    /* One chunk, but check for short chunk at end of buffer */
    rows = MIN((uint) ptr->rowsperchunk, (uint) ptr->rows_in_mem - i);
    /* Transfer no more than is currently defined */
    thisrow = (uint) ptr->cur_start_row + i;
    rows = MIN(rows, (uint) ptr->first_undef_row - thisrow);
    /* Transfer no more than fits in file */
    rows = MIN(rows, (uint) ptr->rows_in_array - thisrow);
    if (rows <= 0)		/* this chunk might be past end of file! */
      break;
    byte_count = rows * bytesperrow;

_unused cinfo;
_unused writing;

/*
    if (writing)
      (*ptr->b_s_info.write_backing_store) (cinfo, & ptr->b_s_info,
					    (void FAR *) ptr->mem_buffer[i],
					    file_offset, byte_count);
    else
      (*ptr->b_s_info.read_backing_store) (cinfo, & ptr->b_s_info,
					   (void FAR *) ptr->mem_buffer[i],
					   file_offset, byte_count);
*/

    file_offset += byte_count;
  }
}


JSAMPARRAY
access_virt_sarray (j_common_ptr cinfo, jvirt_sarray_ptr ptr,
		    JDIMENSION start_row, JDIMENSION num_rows,
		    boolean writable)
/* Access the part of a virtual sample array starting at start_row */
/* and extending for num_rows rows.  writable is true if  */
/* caller intends to modify the accessed area. */
{
  JDIMENSION end_row = start_row + num_rows;
  JDIMENSION undef_row;

  /* debugging check */
//  if (end_row > ptr->rows_in_array || num_rows > ptr->maxaccess ||
//      ptr->mem_buffer == null)
//    cERREXIT((j_compress_ptr)cinfo, JERR_BAD_VIRTUAL_ACCESS);

  /* Make the desired part of the virtual array accessible */
  if (start_row < ptr->cur_start_row ||
      end_row > ptr->cur_start_row+ptr->rows_in_mem) {
    if (! ptr->b_s_open)
      abort; // cERREXIT((j_compress_ptr)cinfo, JERR_VIRTUAL_BUG);
    /* Flush old buffer contents if necessary */
    if (ptr->dirty) {
      do_sarray_io(cinfo, ptr, TRUE);
      ptr->dirty = FALSE;
    }
    /* Decide what part of virtual array to access.
     * Algorithm: if target address > current window, assume forward scan,
     * load starting at target address.  If target address < current window,
     * assume backward scan, load so that target area is top of window.
     * Note that when switching from forward write to forward read, will have
     * start_row = 0, so the limiting case applies and we load from 0 anyway.
     */
    if (start_row > ptr->cur_start_row) {
      ptr->cur_start_row = start_row;
    } else {
      /* use long arithmetic here to avoid overflow & unsigned problems */
      int ltemp;

      ltemp = (int) end_row - (int) ptr->rows_in_mem;
      if (ltemp < 0)
	ltemp = 0;		/* don't fall off front end of file */
      ptr->cur_start_row = (JDIMENSION) ltemp;
    }
    /* Read in the selected part of the array.
     * During the initial write pass, we will do no actual read
     * because the selected part is all undefined.
     */
    do_sarray_io(cinfo, ptr, FALSE);
  }
  /* Ensure the accessed part of the array is defined; prezero if needed.
   * To improve locality of access, we only prezero the part of the array
   * that the caller is about to access, not the entire in-memory array.
   */
  if (ptr->first_undef_row < end_row) {
    if (ptr->first_undef_row < start_row) {
      if (writable)		/* writer skipped over a section of array */
	abort; // cERREXIT((j_compress_ptr)cinfo, JERR_BAD_VIRTUAL_ACCESS);
      undef_row = start_row;	/* but reader is allowed to read ahead */
    } else {
      undef_row = ptr->first_undef_row;
    }
    if (writable)
      ptr->first_undef_row = end_row;
    if (ptr->pre_zero) {
      size_t bytesperrow = (size_t) ptr->samplesperrow * (JSAMPLE'size);
      undef_row -= ptr->cur_start_row; /* make indexes relative to buffer */
      end_row -= ptr->cur_start_row;
      while (undef_row < end_row) {
	jzero_far((byte *) ptr->mem_buffer[undef_row], bytesperrow);
	undef_row++;
      }
    } else {
      if (! writable)		/* reader looking at undefined data */
	abort; // cERREXIT((j_compress_ptr)cinfo, JERR_BAD_VIRTUAL_ACCESS);
    }
  }
  /* Flag the buffer dirty if caller will write in it */
  if (writable)
    ptr->dirty = TRUE;
  /* Return address of proper part of the buffer */
  return ptr->mem_buffer + (start_row - ptr->cur_start_row);
}


JBLOCKARRAY
access_virt_barray (j_common_ptr cinfo, jvirt_barray_ptr ptr,
		    JDIMENSION start_row, JDIMENSION num_rows,
		    boolean writable)
/* Access the part of a virtual block array starting at start_row */
/* and extending for num_rows rows.  writable is true if  */
/* caller intends to modify the accessed area. */
{
  JDIMENSION end_row = start_row + num_rows;
  JDIMENSION undef_row;

  /* debugging check */
//  if (end_row > ptr->rows_in_array || num_rows > ptr->maxaccess ||
//      ptr->mem_buffer == null)
//    cERREXIT((j_compress_ptr)cinfo, JERR_BAD_VIRTUAL_ACCESS);

  /* Make the desired part of the virtual array accessible */
  if (start_row < ptr->cur_start_row ||
      end_row > ptr->cur_start_row+ptr->rows_in_mem) {
    if (! ptr->b_s_open)
      abort; /// cERREXIT((j_compress_ptr)cinfo, JERR_VIRTUAL_BUG);
    /* Flush old buffer contents if necessary */
    if (ptr->dirty) {
      do_barray_io(cinfo, ptr, TRUE);
      ptr->dirty = FALSE;
    }
    /* Decide what part of virtual array to access.
     * Algorithm: if target address > current window, assume forward scan,
     * load starting at target address.  If target address < current window,
     * assume backward scan, load so that target area is top of window.
     * Note that when switching from forward write to forward read, will have
     * start_row = 0, so the limiting case applies and we load from 0 anyway.
     */
    if (start_row > ptr->cur_start_row) {
      ptr->cur_start_row = start_row;
    } else {
      /* use long arithmetic here to avoid overflow & unsigned problems */
      int ltemp;

      ltemp = (int) end_row - (int) ptr->rows_in_mem;
      if (ltemp < 0)
	ltemp = 0;		/* don't fall off front end of file */
      ptr->cur_start_row = (JDIMENSION) ltemp;
    }
    /* Read in the selected part of the array.
     * During the initial write pass, we will do no actual read
     * because the selected part is all undefined.
     */
    do_barray_io(cinfo, ptr, FALSE);
  }
  /* Ensure the accessed part of the array is defined; prezero if needed.
   * To improve locality of access, we only prezero the part of the array
   * that the caller is about to access, not the entire in-memory array.
   */
  if (ptr->first_undef_row < end_row) {
    if (ptr->first_undef_row < start_row) {
      if (writable)		/* writer skipped over a section of array */
	abort; // cERREXIT((j_compress_ptr)cinfo, JERR_BAD_VIRTUAL_ACCESS);
      undef_row = start_row;	/* but reader is allowed to read ahead */
    } else {
      undef_row = ptr->first_undef_row;
    }
    if (writable)
      ptr->first_undef_row = end_row;
    if (ptr->pre_zero) {
      size_t bytesperrow = (size_t) ptr->blocksperrow * (JBLOCK'size);
      undef_row -= ptr->cur_start_row; /* make indexes relative to buffer */
      end_row -= ptr->cur_start_row;
      while (undef_row < end_row) {
	jzero_far((byte *) ptr->mem_buffer[undef_row], bytesperrow);
	undef_row++;
      }
    } else {
      if (! writable)		/* reader looking at undefined data */
	abort; // cERREXIT((j_compress_ptr)cinfo, JERR_BAD_VIRTUAL_ACCESS);
    }
  }
  /* Flag the buffer dirty if caller will write in it */
  if (writable)
    ptr->dirty = TRUE;
  /* Return address of proper part of the buffer */
  return ptr->mem_buffer + (start_row - ptr->cur_start_row);
}


/*
 * Release all objects belonging to a specified pool.
 */

void
free_pool (j_common_ptr cinfo, int pool_id)
{
  my_mem_ptr mem = (my_mem_ptr) cinfo->mem;
  small_pool_ptr shdr_ptr;
  large_pool_ptr lhdr_ptr;
  size_t space_freed;

  if (pool_id < 0 || pool_id >= JPOOL_NUMPOOLS)
    abort; // cERREXIT1((j_compress_ptr)cinfo, JERR_BAD_POOL_ID, pool_id);	/* safety check */


  /* If freeing IMAGE pool, close any virtual arrays first */
  if (pool_id == JPOOL_IMAGE) {
    jvirt_sarray_ptr sptr;
    jvirt_barray_ptr bptr;

    for (sptr = mem->virt_sarray_list; sptr != null; sptr = sptr->next) {
      if (sptr->b_s_open) {	/* there may be no backing store */
	sptr->b_s_open = FALSE;	/* prevent recursive close if error */
//	(*sptr->b_s_info.close_backing_store) (cinfo, & sptr->b_s_info);
      }
    }
    mem->virt_sarray_list = null;
    for (bptr = mem->virt_barray_list; bptr != null; bptr = bptr->next) {
      if (bptr->b_s_open) {	/* there may be no backing store */
	bptr->b_s_open = FALSE;	/* prevent recursive close if error */
//	(*bptr->b_s_info.close_backing_store) (cinfo, & bptr->b_s_info);
      }
    }
    mem->virt_barray_list = null;
  }

  /* Release large objects */
  lhdr_ptr = mem->large_list[pool_id];
  mem->large_list[pool_id] = null;

  while (lhdr_ptr != null) {
    large_pool_ptr next_lhdr_ptr = lhdr_ptr->hdr.next;
    space_freed = lhdr_ptr->hdr.bytes_used +
		  lhdr_ptr->hdr.bytes_left +
		  (large_pool_hdr'size);

//    jpeg_free_large(cinfo, (byte *) lhdr_ptr, space_freed);
    free lhdr_ptr->hdr.this;

    mem->total_space_allocated -= (int)space_freed;
    lhdr_ptr = next_lhdr_ptr;
  }

  /* Release small objects */
  shdr_ptr = mem->small_list[pool_id];
  mem->small_list[pool_id] = null;

  while (shdr_ptr != null) {
    small_pool_ptr next_shdr_ptr = shdr_ptr->hdr.next;
    space_freed = shdr_ptr->hdr.bytes_used +
		  shdr_ptr->hdr.bytes_left +
		  (small_pool_hdr'size);

//    jpeg_free_small(cinfo, (byte *) shdr_ptr, space_freed);
    free shdr_ptr->hdr.this;

    mem->total_space_allocated -= (int)space_freed;
    shdr_ptr = next_shdr_ptr;
  }
}


/*
 * Close up shop entirely.
 * Note that this cannot be called unless cinfo->mem is non-null.
 */

void
self_destruct (j_common_ptr cinfo)
{
  int pool;

  /* Close all backing store, release all memory.
   * Releasing pools in reverse order might help avoid fragmentation
   * with some (brain-damaged) malloc libraries.
   */
  for (pool = JPOOL_NUMPOOLS-1; pool >= JPOOL_PERMANENT; pool--) {
    free_pool(cinfo, pool);
  }

  /* Release the memory manager control block too. */

//  jpeg_free_small(cinfo, (byte *) cinfo->mem, (my_memory_mgr'size));
  free cinfo->pmem;

  cinfo->mem = null;		/* ensures I will be called only once */

  jpeg_mem_term(cinfo);		/* system-dependent cleanup */
}


/*
 * Memory manager initialization.
 * When this is called, only the error manager pointer is valid in cinfo!
 */

public void
jinit_memory_mgr (j_common_ptr cinfo)
{
  my_mem_ptr mem;
  int max_to_use;
  int pool;
  size_t test_mac;

  cinfo->mem = null;		/* for safety if init fails */

  /* Check for configuration errors.
   * SIZEOF(ALIGN_TYPE) should be a power of 2; otherwise, it probably
   * doesn't reflect any real hardware alignment requirement.
   * The test is a little tricky: for X>0, X and X-1 have no one-bits
   * in common if and only if X is a power of 2, ie has only one one-bit.
   * Some compilers may give an "unreachable code" warning here; ignore it.
   */
  if (((ALIGN_TYPE'size) & ((ALIGN_TYPE'size)-1)) != 0)
    abort; // cERREXIT((j_compress_ptr)cinfo, JERR_BAD_ALIGN_TYPE);
  /* MAX_ALLOC_CHUNK must be representable as type size_t, and must be
   * a multiple of SIZEOF(ALIGN_TYPE).
   * Again, an "unreachable code" warning may be ignored here.
   * But a "constant too large" warning means you need to fix MAX_ALLOC_CHUNK.
   */
  test_mac = (size_t) MAX_ALLOC_CHUNK;
  if ((uint) test_mac != MAX_ALLOC_CHUNK ||
      (MAX_ALLOC_CHUNK % (ALIGN_TYPE'size)) != 0)
    abort; // cERREXIT((j_compress_ptr)cinfo, JERR_BAD_ALLOC_CHUNK);

  max_to_use = jpeg_mem_init(cinfo); /* system-dependent initialization */

  /* Attempt to allocate memory manager's control block */
  cinfo->pmem = new my_memory_mgr;

  mem = &cinfo->pmem^; // (my_mem_ptr) jpeg_get_small(cinfo, (my_memory_mgr'size));

//  if (mem == null) {
//    jpeg_mem_term(cinfo);	/* system-dependent cleanup */
//    cERREXIT1((j_compress_ptr)cinfo, JERR_OUT_OF_MEMORY, 0);
//  }

  /* OK, fill in the method pointers */
  mem->pub.alloc_small = alloc_small;
  mem->pub.alloc_large = alloc_large;
  mem->pub.alloc_sarray = alloc_sarray;
  mem->pub.alloc_barray = alloc_barray;
  mem->pub.request_virt_sarray = request_virt_sarray;
  mem->pub.request_virt_barray = request_virt_barray;
  mem->pub.realize_virt_arrays = realize_virt_arrays;
  mem->pub.access_virt_sarray = access_virt_sarray;
  mem->pub.access_virt_barray = access_virt_barray;
  mem->pub.free_pool = free_pool;
  mem->pub.self_destruct = self_destruct;

  /* Make MAX_ALLOC_CHUNK accessible to other modules */
  mem->pub.max_alloc_chunk = (int)MAX_ALLOC_CHUNK;

  /* Initialize working state */
  mem->pub.max_memory_to_use = (int)max_to_use;

  for (pool = JPOOL_NUMPOOLS-1; pool >= JPOOL_PERMANENT; pool--) {
    mem->small_list[pool] = null;
    mem->large_list[pool] = null;
  }
  mem->virt_sarray_list = null;
  mem->virt_barray_list = null;

  mem->total_space_allocated = (int)(my_memory_mgr'size);

  /* Declare ourselves open for business */
  cinfo->mem = & mem->pub;
}

#end unsafe

