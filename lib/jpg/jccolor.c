/*
 * jccolor.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains input colorspace conversion routines.
 */

use jpeglib, jmorecfg;

#begin unsafe


/* Private subobject */

struct  my_color_converter {
  jpeg_color_converter pub; /* public fields */

  /* Private state for RGB->YCC conversion */
  INT32 * rgb_ycc_tab;    /* => table for RGB to YCbCr conversion */
}

typedef my_color_converter * my_cconvert_ptr;


/**************** RGB -> YCbCr conversion: most common case **************/

/*
 * YCbCr is defined per CCIR 601-1, except that Cb and Cr are
 * normalized to the range 0..MAXJSAMPLE rather than -0.5 .. 0.5.
 * The conversion equations to be implemented are therefore
 *  Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
 *  Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B  + CENTERJSAMPLE
 *  Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B  + CENTERJSAMPLE
 * (These numbers are derived from TIFF 6.0 section 21, dated 3-June-92.)
 * Note: older versions of the IJG code used a zero offset of MAXJSAMPLE/2,
 * rather than CENTERJSAMPLE, for Cb and Cr.  This gave equal positive and
 * negative swings for Cb/Cr, but meant that grayscale values (Cb=Cr=0)
 * were not represented exactly.  Now we sacrifice exact representation of
 * maximum red and maximum blue in order to get exact grayscales.
 *
 * To avoid floating-point arithmetic, we represent the fractional constants
 * as integers scaled up by 2^16 (about 4 digits precision); we have to divide
 * the products by 2^16, with appropriate rounding, to get the correct answer.
 *
 * For even more speed, we avoid doing any multiplications in the inner loop
 * by precalculating the constants times R,G,B for all possible values.
 * For 8-bit JSAMPLEs this is very reasonable (only 256 entries per table);
 * for 12-bit samples it is still acceptable.  It's not very reasonable for
 * 16-bit samples, but if you want lossless storage you shouldn't be changing
 * colorspace anyway.
 * The CENTERJSAMPLE offsets and the rounding fudge-factor of 0.5 are included
 * in the tables to save adding them separately in the inner loop.
 */

const int SCALEBITS = 16; /* speediest right-shift on some machines */
const int CBCR_OFFSET =((INT32) CENTERJSAMPLE << SCALEBITS);
const int ONE_HALF  =((INT32) 1 << (SCALEBITS-1));

/* We allocate one big table and divide it up into eight parts, instead of
 * doing eight alloc_small requests.  This lets us use a single table base
 * address, which can be held in a register in the inner loops on many
 * machines (more than can hold all eight addresses, anyway).
 */

const int  R_Y_OFF  = 0;      /* offset to R => Y section */
const int  G_Y_OFF  = (1*(MAXJSAMPLE+1)); /* offset to G => Y section */
const int  B_Y_OFF  = (2*(MAXJSAMPLE+1)); /* etc. */
const int  R_CB_OFF =(3*(MAXJSAMPLE+1));
const int  G_CB_OFF =(4*(MAXJSAMPLE+1));
const int  B_CB_OFF =(5*(MAXJSAMPLE+1));
const int  R_CR_OFF = B_CB_OFF;   /* B=>Cb, R=>Cr are the same */
const int  G_CR_OFF =(6*(MAXJSAMPLE+1));
const int  B_CR_OFF =(7*(MAXJSAMPLE+1));
const int  TABLE_SIZE =(8*(MAXJSAMPLE+1));

int FIX (double x)
{
  return ((INT32) ( (x * (double)(1<<SCALEBITS)) + 0.5));
}

/*
 * Initialize for RGB->YCC colorspace conversion.
 */

void
rgb_ycc_start (j_compress_ptr cinfo)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  INT32 * rgb_ycc_tab;
  INT32 i;

  /* Allocate and fill in the conversion tables. */
  rgb_ycc_tab = (INT32 *)
    (cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
        ((uint)TABLE_SIZE * (INT32'size)));
  cconvert->rgb_ycc_tab = rgb_ycc_tab;

  for (i = 0; i <= MAXJSAMPLE; i++) {
    rgb_ycc_tab[i+R_Y_OFF] = FIX(0.29900) * i;
    rgb_ycc_tab[i+G_Y_OFF] = FIX(0.58700) * i;
    rgb_ycc_tab[i+B_Y_OFF] = FIX(0.11400) * i     + ONE_HALF;
    rgb_ycc_tab[i+R_CB_OFF] = (-FIX(0.16874)) * i;
    rgb_ycc_tab[i+G_CB_OFF] = (-FIX(0.33126)) * i;
    /* We use a rounding fudge-factor of 0.5-epsilon for Cb and Cr.
     * This ensures that the maximum output will round to MAXJSAMPLE
     * not MAXJSAMPLE+1, and thus that we don't have to range-limit.
     */
    rgb_ycc_tab[i+B_CB_OFF] = FIX(0.50000) * i    + CBCR_OFFSET + ONE_HALF-1;
/*  B=>Cb and R=>Cr tables are the same
    rgb_ycc_tab[i+R_CR_OFF] = FIX(0.50000) * i    + CBCR_OFFSET + ONE_HALF-1;
*/
    rgb_ycc_tab[i+G_CR_OFF] = (-FIX(0.41869)) * i;
    rgb_ycc_tab[i+B_CR_OFF] = (-FIX(0.08131)) * i;
  }
}

/*
 * Initialize for RGBA->YCCA colorspace conversion.
 */

void
rgba_ycca_start (j_compress_ptr cinfo)
{
  rgb_ycc_start (cinfo);
}


/*
 * Convert some rows of samples to the JPEG colorspace.
 *
 * Note that we change from the application's interleaved-pixel format
 * to our internal noninterleaved, one-plane-per-component format.
 * The input buffer is therefore three times as wide as the output buffer.
 *
 * A starting row offset is provided only for the output buffer.  The caller
 * can easily adjust the passed input_buf value to accommodate any row
 * offset required on that side.
 */

void
rgb_ycc_convert (j_compress_ptr cinfo,
     JSAMPARRAY input_buf0, JSAMPIMAGE output_buf,
     JDIMENSION output_row0, int num_rows0)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  int r, g, b;
  INT32 * ctab = cconvert->rgb_ycc_tab;
  JSAMPROW inptr;
  JSAMPROW outptr0, outptr1, outptr2;
  JDIMENSION col;
  JDIMENSION num_cols = cinfo->image_width;
 
  JSAMPARRAY input_buf = input_buf0;
  JDIMENSION output_row = output_row0;
  int num_rows = num_rows0;

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    outptr1 = output_buf[1][output_row];
    outptr2 = output_buf[2][output_row];
    output_row++;
    for (col = 0; col < num_cols; col++) {
      r = GETJSAMPLE(inptr[RGB_RED]);
      g = GETJSAMPLE(inptr[RGB_GREEN]);
      b = GETJSAMPLE(inptr[RGB_BLUE]);
      inptr += RGB_PIXELSIZE;
      /* If the inputs are 0..MAXJSAMPLE, the outputs of these equations
       * must be too; we do not need an explicit range-limiting operation.
       * Hence the value being shifted is never negative, and we don't
       * need the general RIGHT_SHIFT macro.
       */
      /* Y */
      outptr0[col] = (JSAMPLE)
    ((ctab[r+R_Y_OFF] + ctab[g+G_Y_OFF] + ctab[b+B_Y_OFF])
     >> SCALEBITS);
      /* Cb */
      outptr1[col] = (JSAMPLE)
    ((ctab[r+R_CB_OFF] + ctab[g+G_CB_OFF] + ctab[b+B_CB_OFF])
     >> SCALEBITS);
      /* Cr */
      outptr2[col] = (JSAMPLE)
    ((ctab[r+R_CR_OFF] + ctab[g+G_CR_OFF] + ctab[b+B_CR_OFF])
     >> SCALEBITS);
    }
  }
}


/*
 * Convert some rows of samples to the JPEG colorspace.
 *
 * Note that we change from the application's interleaved-pixel format
 * to our internal noninterleaved, one-plane-per-component format.
 * The input buffer is therefore four times as wide as the output buffer.
 *
 * A starting row offset is provided only for the output buffer.  The caller
 * can easily adjust the passed input_buf value to accommodate any row
 * offset required on that side.
 */

void
rgba_ycca_convert (j_compress_ptr cinfo,
     JSAMPARRAY input_buf0, JSAMPIMAGE output_buf,
     JDIMENSION output_row0, int num_rows0)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  int r, g, b, a;
  INT32 * ctab = cconvert->rgb_ycc_tab;
  JSAMPROW inptr;
  JSAMPROW outptr0, outptr1, outptr2, outptr3;
  JDIMENSION col;
  JDIMENSION num_cols = cinfo->image_width;
 
  JSAMPARRAY input_buf = input_buf0;
  JDIMENSION output_row = output_row0;
  int num_rows = num_rows0;

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    outptr1 = output_buf[1][output_row];
    outptr2 = output_buf[2][output_row];
    outptr3 = output_buf[3][output_row];
    output_row++;
    for (col = 0; col < num_cols; col++) {
      r = GETJSAMPLE(inptr[RGB_RED]);
      g = GETJSAMPLE(inptr[RGB_GREEN]);
      b = GETJSAMPLE(inptr[RGB_BLUE]);
      a = GETJSAMPLE(inptr[RGB_ALPHA]);
      inptr += RGBA_PIXELSIZE;
      /* If the inputs are 0..MAXJSAMPLE, the outputs of these equations
       * must be too; we do not need an explicit range-limiting operation.
       * Hence the value being shifted is never negative, and we don't
       * need the general RIGHT_SHIFT macro.
       */
      /* Y */
      outptr0[col] = (JSAMPLE)((ctab[r+R_Y_OFF] + ctab[g+G_Y_OFF] + ctab[b+B_Y_OFF]) >> SCALEBITS);
      
      /* Cb */
      outptr1[col] = (JSAMPLE)((ctab[r+R_CB_OFF] + ctab[g+G_CB_OFF] + ctab[b+B_CB_OFF]) >> SCALEBITS);
      
      /* Cr */
      outptr2[col] = (JSAMPLE)((ctab[r+R_CR_OFF] + ctab[g+G_CR_OFF] + ctab[b+B_CR_OFF]) >> SCALEBITS);
      
      /* A */
      outptr3[col] = (JSAMPLE)a;
    }
  }
}


/**************** Cases other than RGB -> YCbCr **************/


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles RGB->grayscale conversion, which is the same
 * as the RGB->Y portion of RGB->YCbCr.
 * We assume rgb_ycc_start has been called (we only use the Y tables).
 */

void
rgb_gray_convert (j_compress_ptr cinfo,
      JSAMPARRAY input_buf0, JSAMPIMAGE output_buf,
      JDIMENSION output_row0, int num_rows0)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  int r, g, b;
  INT32 * ctab = cconvert->rgb_ycc_tab;
  JSAMPROW inptr;
  JSAMPROW outptr;
  JDIMENSION col;
  JDIMENSION num_cols = cinfo->image_width;

  JSAMPARRAY input_buf = input_buf0;
  JDIMENSION output_row = output_row0;
  int num_rows = num_rows0;

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr = output_buf[0][output_row];
    output_row++;
    for (col = 0; col < num_cols; col++) {
      r = GETJSAMPLE(inptr[RGB_RED]);
      g = GETJSAMPLE(inptr[RGB_GREEN]);
      b = GETJSAMPLE(inptr[RGB_BLUE]);
      inptr += RGB_PIXELSIZE;
      
      /* Y */
      outptr[col] = (JSAMPLE) ((ctab[r+R_Y_OFF] + ctab[g+G_Y_OFF] + ctab[b+B_Y_OFF]) >> SCALEBITS);
    }
  }
}


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles Adobe-style CMYK->YCCK conversion,
 * where we convert R=1-C, G=1-M, and B=1-Y to YCbCr using the same
 * conversion as above, while passing K (black) unchanged.
 * We assume rgb_ycc_start has been called.
 */

void
cmyk_ycck_convert (j_compress_ptr cinfo,
       JSAMPARRAY input_buf0, JSAMPIMAGE output_buf,
       JDIMENSION output_row0, int num_rows0)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  int r, g, b;
  INT32 * ctab = cconvert->rgb_ycc_tab;
  JSAMPROW inptr;
  JSAMPROW outptr0, outptr1, outptr2, outptr3;
  JDIMENSION col;
  JDIMENSION num_cols = cinfo->image_width;

  JSAMPARRAY input_buf = input_buf0;
  JDIMENSION output_row = output_row0;
  int num_rows = num_rows0;

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    outptr1 = output_buf[1][output_row];
    outptr2 = output_buf[2][output_row];
    outptr3 = output_buf[3][output_row];
    output_row++;
    for (col = 0; col < num_cols; col++) {
      r = MAXJSAMPLE - GETJSAMPLE(inptr[0]);
      g = MAXJSAMPLE - GETJSAMPLE(inptr[1]);
      b = MAXJSAMPLE - GETJSAMPLE(inptr[2]);
      /* K passes through as-is */
      outptr3[col] = inptr[3];  /* don't need GETJSAMPLE here */
      inptr += 4;
      /* If the inputs are 0..MAXJSAMPLE, the outputs of these equations
       * must be too; we do not need an explicit range-limiting operation.
       * Hence the value being shifted is never negative, and we don't
       * need the general RIGHT_SHIFT macro.
       */
      /* Y */
      outptr0[col] = (JSAMPLE)
    ((ctab[r+R_Y_OFF] + ctab[g+G_Y_OFF] + ctab[b+B_Y_OFF])
     >> SCALEBITS);
      /* Cb */
      outptr1[col] = (JSAMPLE)
    ((ctab[r+R_CB_OFF] + ctab[g+G_CB_OFF] + ctab[b+B_CB_OFF])
     >> SCALEBITS);
      /* Cr */
      outptr2[col] = (JSAMPLE)
    ((ctab[r+R_CR_OFF] + ctab[g+G_CR_OFF] + ctab[b+B_CR_OFF])
     >> SCALEBITS);
    }
  }
}


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles grayscale output with no conversion.
 * The source can be either plain grayscale or YCbCr (since Y == gray).
 */

void
grayscale_convert (j_compress_ptr cinfo,
       JSAMPARRAY input_buf0, JSAMPIMAGE output_buf,
       JDIMENSION output_row0, int num_rows0)
{
  JSAMPROW inptr;
  JSAMPROW outptr;
  JDIMENSION col;
  JDIMENSION num_cols = cinfo->image_width;
  int instride = cinfo->input_components;

  JSAMPARRAY input_buf = input_buf0;
  JDIMENSION output_row = output_row0;
  int num_rows = num_rows0;

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr = output_buf[0][output_row];
    output_row++;
    for (col = 0; col < num_cols; col++) {
      outptr[col] = inptr[0]; /* don't need GETJSAMPLE() here */
      inptr += instride;
    }
  }
}


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles multi-component colorspaces without conversion.
 * We assume input_components == num_components.
 */

void
null_convert (j_compress_ptr cinfo,
        JSAMPARRAY input_buf0, JSAMPIMAGE output_buf,
        JDIMENSION output_row0, int num_rows0)
{
  JSAMPROW inptr;
  JSAMPROW outptr;
  JDIMENSION col;
  int ci;
  int nc = cinfo->num_components;
  JDIMENSION num_cols = cinfo->image_width;

  JSAMPARRAY input_buf = input_buf0;
  JDIMENSION output_row = output_row0;
  int num_rows = num_rows0;

  while (--num_rows >= 0) {
    /* It seems fastest to make a separate pass for each component. */
    for (ci = 0; ci < nc; ci++) {
      inptr = *input_buf;
      outptr = output_buf[ci][output_row];
      for (col = 0; col < num_cols; col++) {
  outptr[col] = inptr[ci]; /* don't need GETJSAMPLE() here */
  inptr += nc;
      }
    }
    input_buf++;
    output_row++;
  }
}


/*
 * Empty method for start_pass.
 */

void
null_method (j_compress_ptr cinfo)
{
  _unused cinfo;
  /* no work needed */
}


/*
 * Module initialization routine for input colorspace conversion.
 */

public int
jinit_color_converter (j_compress_ptr cinfo)
{
  my_cconvert_ptr cconvert;

  cconvert = (my_cconvert_ptr)
    (cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
        (my_color_converter'size));
  cinfo->cconvert = (jpeg_color_converter *) cconvert;
  /* set start_pass to null method until we find out differently */
  cconvert->pub.start_pass = null_method;

  /* Make sure input_components agrees with in_color_space */
  switch (cinfo->in_color_space) {
  case JCS_GRAYSCALE:
    if (cinfo->input_components != 1)
      return JERR_BAD_IN_COLORSPACE; // cERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    break;

  case JCS_RGB:
//#if RGB_PIXELSIZE != 3
    if (cinfo->input_components != RGB_PIXELSIZE)
      return JERR_BAD_IN_COLORSPACE; // cERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    break;
//#endif /* else share code with YCbCr */

  case JCS_YCbCr:
    if (cinfo->input_components != 3)
      return JERR_BAD_IN_COLORSPACE; // cERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    break;

  case JCS_CMYK:
  case JCS_YCCK:
  case JCS_RGBA:
  case JCS_YCbCrA:
    if (cinfo->input_components != 4)
      return JERR_BAD_IN_COLORSPACE; // cERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    break;

  default:      /* JCS_UNKNOWN can be anything */
    if (cinfo->input_components < 1)
      return JERR_BAD_IN_COLORSPACE;  // cERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    break;
  }

  /* Check num_components, set conversion method based on requested space */
  switch (cinfo->jpeg_color_space)
  {
  case JCS_GRAYSCALE:
    if (cinfo->num_components != 1)
      return JERR_BAD_J_COLORSPACE; // cERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    if (cinfo->in_color_space == JCS_GRAYSCALE)
      cconvert->pub.color_convert = grayscale_convert;
    else if (cinfo->in_color_space == JCS_RGB) {
      cconvert->pub.start_pass = rgb_ycc_start;
      cconvert->pub.color_convert = rgb_gray_convert;
    } else if (cinfo->in_color_space == JCS_YCbCr)
      cconvert->pub.color_convert = grayscale_convert;
    else
      return JERR_CONVERSION_NOTIMPL; // cERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_RGB:
    if (cinfo->num_components != 3)
      return JERR_BAD_J_COLORSPACE; // cERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    if (cinfo->in_color_space == JCS_RGB && RGB_PIXELSIZE == 3)
      cconvert->pub.color_convert = null_convert;
    else
      return JERR_CONVERSION_NOTIMPL; // cERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_YCbCr:
    if (cinfo->num_components != 3)
      return JERR_BAD_J_COLORSPACE; // cERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    if (cinfo->in_color_space == JCS_RGB) {
      cconvert->pub.start_pass = rgb_ycc_start;
      cconvert->pub.color_convert = rgb_ycc_convert;
    } else if (cinfo->in_color_space == JCS_YCbCr)
      cconvert->pub.color_convert = null_convert;
    else
      return JERR_CONVERSION_NOTIMPL; // cERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_CMYK:
    if (cinfo->num_components != 4)
      return JERR_BAD_J_COLORSPACE; // cERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    if (cinfo->in_color_space == JCS_CMYK)
      cconvert->pub.color_convert = null_convert;
    else
      return JERR_CONVERSION_NOTIMPL; // cERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_YCCK:
    if (cinfo->num_components != 4)
      return JERR_BAD_J_COLORSPACE; // cERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    if (cinfo->in_color_space == JCS_CMYK) {
      cconvert->pub.start_pass = rgb_ycc_start;
      cconvert->pub.color_convert = cmyk_ycck_convert;
    } else if (cinfo->in_color_space == JCS_YCCK)
      cconvert->pub.color_convert = null_convert;
    else
      return JERR_CONVERSION_NOTIMPL; // cERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_YCbCrA:
    if (cinfo->num_components != 4)
      return JERR_BAD_J_COLORSPACE; // cERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    if (cinfo->in_color_space == JCS_RGBA)
    {
      cconvert->pub.start_pass = rgba_ycca_start;
      cconvert->pub.color_convert = rgba_ycca_convert;
    }
    else if (cinfo->in_color_space == JCS_YCbCrA)
    {
      cconvert->pub.color_convert = null_convert;
    }
    else
      return JERR_CONVERSION_NOTIMPL; // cERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  default:      /* allow null conversion of JCS_UNKNOWN */
    if (cinfo->jpeg_color_space != cinfo->in_color_space ||
        cinfo->num_components != cinfo->input_components)
      return JERR_CONVERSION_NOTIMPL; // cERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    cconvert->pub.color_convert = null_convert;
    break;
  }

  return 0;
}


#end unsafe
