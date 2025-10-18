/*
 * jutils.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains tables and miscellaneous utility routines needed
 * for both compression and decompression.
 * Note we prefix all global names with "j" to minimize conflicts with
 * a surrounding application.
 */

use jpeglib, jmorecfg;


#begin unsafe

/*
 * Arithmetic utilities
 */

public int
jdiv_round_up (int a, int b)
/* Compute a/b rounded up to next integer, ie, ceil(a/b) */
/* Assumes a >= 0, b > 0 */
{
  return (a + b - 1) / b;
}


public int
jround_up (int a, int b)
/* Compute a rounded up to next multiple of b, ie, ceil(a/b)*b */
/* Assumes a >= 0, b > 0 */
{
  int aa = a;
  aa += b - 1;
  return aa - (aa % b);
}


/* On normal machines we can apply MEMCOPY() and MEMZERO() to sample arrays
 * and coefficient-block arrays.  This won't work on 80x86 because the arrays
 * are FAR and we're assuming a small-pointer memory model.  However, some
 * DOS compilers provide far-pointer versions of memcpy() and memset() even
 * in the small-model libraries.  These will be used if USE_FMEM is defined.
 * Otherwise, the routines below do it the hard way.  (The performance cost
 * is not all that great, because these routines aren't very heavily used.)
 */



public void
jcopy_sample_rows (JSAMPARRAY input_array, int source_row,
		   JSAMPARRAY output_array, int dest_row,
		   int num_rows, JDIMENSION num_cols)
/* Copy some rows of samples from one place to another.
 * num_rows rows are copied from input_array[source_row++]
 * to output_array[dest_row++]; these areas may overlap for duplication.
 * The source and destination arrays must be at least as wide as num_cols.
 */
{
  JSAMPARRAY input_array2 = input_array;
  JSAMPARRAY output_array2 = output_array;
  JSAMPROW inptr, outptr;
  JDIMENSION count;
  int row;

  input_array2 += source_row;
  output_array2 += dest_row;

  for (row = num_rows; row > 0; row--) {
    inptr = *input_array2++;
    outptr = *output_array2++;
    for (count = num_cols; count > 0; count--)
      *outptr++ = *inptr++;	/* needn't bother with GETJSAMPLE() here */
  }
}


public void
jcopy_block_row (JBLOCKROW input_row, JBLOCKROW output_row,
		 JDIMENSION num_blocks)
/* Copy a row of coefficient blocks from one place to another. */
{
  JCOEFPTR inptr, outptr;
  int count;

  inptr = (JCOEFPTR) input_row;
  outptr = (JCOEFPTR) output_row;
  for (count = (int) num_blocks * DCTSIZE2; count > 0; count--) {
    *outptr++ = *inptr++;
  }
}


public void
jzero_far (byte* target, size_t bytestozero)
/* Zero out a chunk of FAR memory. */
/* This might be sample-array data, block-array data, or alloc_large data. */
{
  byte* ptr = (byte *) target;
  size_t count;

  for (count = bytestozero; count > 0; count--) {
    *ptr++ = 0;
  }
}

public void
jcopy (byte* target, byte *source, size_t bytes)
{
  size_t count = bytes;
  byte *fr = source;
  byte *to = target;

  while (count-- != 0)
    *to++ = *fr++;
}

#end unsafe
