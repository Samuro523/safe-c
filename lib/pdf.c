
// pdf.c

use files, image, strings, tracing;

//--------------------------------------------------------------------------------------------------------------

struct PDF
{
  // pages (kids)
  int nb_pages;
  int max_pages;
  int[]^ pages;

  // xref
  int nb_objects;
  int max_objects;
  int[]^ pobject_offset;

  // object_nr
  int unique_object_nr;    // used as ++unique_object_nr

  // text chunk
  int text_len;
  int text_size;
  char[]^ text;

  // image nr
  int nb_images;
  int max_images;
  int[]^ images;

  // font object nr
  int font_object_nr[12];

  // font nr
  int nb_fonts;
  int max_fonts;
  int[]^ fonts;

  // media box size
  int page_width, page_height;

  // file
  int fd;
  int error;  // 0 = OK, -1 = error

}

//--------------------------------------------------------------------------------------------------------------

const string font_name[12] = {"Times-Roman",  "Times-Italic",      "Times-Bold",      "Times-BoldItalic",
                              "Helvetica",    "Helvetica-Oblique", "Helvetica-Bold",  "Helvetica-BoldOblique",
                              "Courier",      "Courier-Oblique",   "Courier-Bold",    "Courier-BoldOblique"};

//--------------------------------------------------------------------------------------------------------------

void put (ref PDF pdf, byte[] s)
{
  if (pdf.error != 0)
    return;

  if (pdf.fd <= 0)
  {
    trace ("error: put() : pdf not open\n");
    pdf.error = 1;
    return;
  }

  if (write (pdf.fd, s) != s'length)
  {
    trace ("error: put() : cannot write to pdf\n");
    pdf.error = 1;
    return;
  }
}

//--------------------------------------------------------------------------------------------------------------

void putstr (ref PDF pdf, string s)
{
  put (ref pdf, s[0 : strlen(s)]);
}

//--------------------------------------------------------------------------------------------------------------

void start_object (ref PDF pdf, int object_nr)
{
  int[]^ p;

  while (object_nr >= pdf.max_objects)
  {
    pdf.max_objects *= 2;

    p = new int[pdf.max_objects];
    if (pdf.nb_objects != 0)
      p^[0 : pdf.nb_objects] = pdf.pobject_offset^[0 : pdf.nb_objects];

    free pdf.pobject_offset;
    pdf.pobject_offset = p;
  }

  pdf.pobject_offset^[object_nr] = (int)lseek (pdf.fd, 0L, SEEK_CUR);

  if (object_nr >= pdf.nb_objects)
    pdf.nb_objects = object_nr+1;
}

//--------------------------------------------------------------------------------------------------------------

void append_text (ref PDF pdf, string text)
{
  char[]^ p;
  int len = strlen(text);

  while (pdf.text_len + len > pdf.text_size)
  {
    pdf.text_size *= 2;

    p = new char[pdf.text_size];
    if (pdf.text_len != 0)
      p^[0 : pdf.text_len] = pdf.text^[0 : pdf.text_len];

    free pdf.text;
    pdf.text = p;
  }

  pdf.text^[pdf.text_len : len] = text[0 : len];
  pdf.text_len += len;
}

//--------------------------------------------------------------------------------------------------------------

void flush_text (ref PDF pdf)
{
  put (ref pdf, pdf.text^[0 : pdf.text_len]);
  pdf.text_len = 0;
}

//--------------------------------------------------------------------------------------------------------------

// kids

void add_page (ref PDF pdf, int page_obj_nr)
{
  int[]^ p;

  if (pdf.nb_pages == pdf.max_pages)
  {
    pdf.max_pages = (pdf.max_pages + 1) * 2;

    p = new int[pdf.max_pages];
    if (pdf.nb_pages != 0)
      p^[0 : pdf.nb_pages] = pdf.pages^[0 : pdf.nb_pages];

    free pdf.pages;
    pdf.pages = p;
  }

  pdf.pages^[pdf.nb_pages++] = page_obj_nr;
}

//--------------------------------------------------------------------------------------------------------------

public void new_page (ref PDF pdf)
{
  int page_nr    = ++pdf.unique_object_nr;
  int content_nr = ++pdf.unique_object_nr;
  char buffer[1024];
  int  i;

  // part 1 : kid page

  add_page (ref pdf, page_nr);  // add in kids list

  start_object (ref pdf, page_nr);

  sprintf (out buffer,
      "%d 0 obj\r\n"
    + "<<\r\n"
    + "/Type /Page\r\n"
    + "/Parent 2 0 R\r\n"
    + "/MediaBox [0 0 %d %d]\r\n"
    + "/Resources <<\r\n"
    + "  /ProcSet [/PDF /Text /ImageB /ImageC /ImageI]\r\n",
     page_nr, pdf.page_width, pdf.page_height);

  putstr (ref pdf, buffer);


  // referenced images

  if (pdf.nb_images > 0)
  {
    sprintf (out buffer, "  /XObject << \r\n");
    putstr (ref pdf, buffer);

    for (i=0; i<pdf.nb_images; i++)
    {
      sprintf (out buffer, "   /Im%d %d 0 R\r\n", pdf.images^[i], pdf.images^[i]);
      putstr (ref pdf, buffer);
    }

    sprintf (out buffer, "  >>\r\n");
    putstr (ref pdf, buffer);

    pdf.nb_images = 0;
  }


  // referenced fonts

  if (pdf.nb_fonts > 0)
  {
    sprintf (out buffer, "  /Font <<\r\n");
    putstr (ref pdf, buffer);

    for (i=0; i<pdf.nb_fonts; i++)
    {
      sprintf (out buffer, "   /F%d %d 0 R\r\n", pdf.fonts^[i], pdf.font_object_nr[pdf.fonts^[i]]);
      putstr (ref pdf, buffer);
    }

    sprintf (out buffer, "  >>\r\n");
    putstr (ref pdf, buffer);

    pdf.nb_fonts = 0;
  }

  sprintf (out buffer,
      ">>\r\n"
    + "/Contents [%d 0 R]\r\n"
    + ">>\r\n"
    + "endobj\r\n"
    + "\r\n",
     content_nr);

  putstr (ref pdf, buffer);


  // part 2 : flush page content

  start_object (ref pdf, content_nr);

  sprintf (out buffer,
      "%d 0 obj\r\n"
    + "<< /Length %d >>\r\n"
    + "stream\r\n",
     content_nr, pdf.text_len);

  putstr (ref pdf, buffer);


  flush_text (ref pdf);


  sprintf (out buffer,
    "endstream\r\n"
  + "endobj\r\n"
  + "\r\n");

  putstr (ref pdf, buffer);
}

//--------------------------------------------------------------------------------------------------------------

public int create_pdf (out PDF pdf, string filename)
{
  int  rc;
  char buffer[1024];

  clear pdf;

  rc = create (filename);
  if (rc < 0)
  {
    trace ("error: create_pdf() : cannot create %s\n", filename);
    return -1;
  }

  pdf.fd = rc;
  pdf.max_objects = 1;
  pdf.text_size = 1;
  pdf.page_width = PDF_WIDTH;
  pdf.page_height = PDF_HEIGHT;
  pdf.unique_object_nr = 2;    // 2 first objects are reserved

  sprintf (out buffer, "%%PDF-1.7\r\n%%ÿÿÿÿ\r\n\r\n");
  putstr (ref pdf, buffer);

  start_object (ref pdf, 1);

  sprintf (out buffer,
    "1 0 obj\r\n"
  + "<<\r\n"
  + " /Type /Catalog\r\n"
  + " /Pages 2 0 R\r\n"
  + ">>\r\n"
  + "endobj\r\n"
  + "\r\n");
  putstr (ref pdf, buffer);

  return 0;
}

//--------------------------------------------------------------------------------------------------------------

public void pdf_set_page_size (ref PDF pdf,
                                   int width,    // default is PDF_WIDTH
                                   int height)   // default is PDF_HEIGHT
{
  pdf.page_width = width;
  pdf.page_height = height;
}

//--------------------------------------------------------------------------------------------------------------

public int close_pdf (ref PDF pdf)
{
  int  rc, xref_offset, i;
  char buffer[1024];

  if (pdf.fd == 0)
  {
    trace ("error: close_pdf() : pdf not open\n");
    return -1;
  }


  new_page (ref pdf);


  // write Pages

  start_object (ref pdf, 2);
  sprintf (out buffer,
    "2 0 obj\r\n"
  + "<< \r\n"
  + "/Type /Pages\r\n"
  + "/Kids [\r\n");
  putstr (ref pdf, buffer);

  for (i=0; i<pdf.nb_pages; i++)
  {
    sprintf (out buffer,
      " %d 0 R\r\n", pdf.pages^[i]);
    putstr (ref pdf, buffer);
  }

  sprintf (out buffer,
    " ]\r\n"
  + "/Count %d\r\n"
  + ">>\r\n"
  + "endobj\r\n"
  + "\r\n",
    pdf.nb_pages);
  putstr (ref pdf, buffer);


  // write xref and trailer

  xref_offset = (int)lseek (pdf.fd, 0L, SEEK_CUR);

  sprintf (out buffer,
    "xref\r\n"
  + "0 %d\r\n"
  + "0000000000 65535 n\r\n",
      pdf.nb_objects);

  putstr (ref pdf, buffer);

  for (i=1; i<pdf.nb_objects; i++)
  {
    sprintf (out buffer, "%010d 00000 n\r\n", pdf.pobject_offset^[i]);
    putstr (ref pdf, buffer);
  }

  sprintf (out buffer,
    "\r\n"
  + "trailer\r\n"
  + "<<\r\n"
  + " /Size %d\r\n"
  + " /Root 1 0 R\r\n"
  + ">>\r\n"
  + "startxref\r\n"
  + "%d\r\n"
  + "%%EOF",
      pdf.nb_objects,
      xref_offset);

  putstr (ref pdf, buffer);


  // free all

  free pdf.pobject_offset;
  free pdf.text;
  free pdf.pages;
  free pdf.images;
  free pdf.fonts;

  rc = close (pdf.fd);
  if (rc < 0)
  {
    trace ("error: close_pdf() : close() failed\n");
    clear pdf;
    return -1;
  }

  if (pdf.error != 0)
  {
    trace ("error: close_pdf() : pdf has errors\n");
    clear pdf;
    return -1;
  }

  clear pdf;

  return 0;
}

//--------------------------------------------------------------------------------------------------------------

void add_font (ref PDF pdf, int font_obj_nr)
{
  int[]^ p;

  if (pdf.nb_fonts == pdf.max_fonts)
  {
    pdf.max_fonts = (pdf.max_fonts + 1) * 2;

    p = new int[pdf.max_fonts];
    if (pdf.nb_fonts != 0)
      p^[0 : pdf.nb_fonts] = pdf.fonts^[0 : pdf.nb_fonts];

    free pdf.fonts;
    pdf.fonts = p;
  }

  pdf.fonts^[pdf.nb_fonts++] = font_obj_nr;
}

//--------------------------------------------------------------------------------------------------------------

void use_font (ref PDF pdf, int font_nr)
{
  if (pdf.font_object_nr[font_nr] == 0)
  {
    char buffer[1024];

    pdf.font_object_nr[font_nr] = ++pdf.unique_object_nr;

    start_object (ref pdf, pdf.font_object_nr[font_nr]);

    sprintf (out buffer,
        "%d 0 obj\r\n"
      + "<<\r\n"
      + "/Type /Font\r\n"
      + "/Subtype /Type1\r\n"
      + "/Encoding /WinAnsiEncoding\r\n"
      + "/BaseFont /%s\r\n"
      + ">>\r\n"
      + "endobj\r\n"
      + "\r\n",
      pdf.font_object_nr[font_nr],
      font_name[font_nr]);

    putstr (ref pdf, buffer);
  }

  // add in list of fonts used by this page
  add_font (ref pdf, font_nr);
}

//--------------------------------------------------------------------------------------------------------------

public void pdf_text (
  ref PDF pdf,
  string text,
  FONT font,
  int font_height,
  int x,              // lower left corner
  int y,
  uint color,
  uint style)
{
  int len = strlen(text);
  string^ ptext = new char[2*len+1];
  string^ pbuffer = new char[1024 + 2*len+1];

  uint r = color & 255;
  uint g = (color >> 8) & 255;
  uint b = (color >> 16) & 255;

  int i, j, font2, font_nr;
  
  j = 0;
  for (i=0; i<len; i++)
  {
    if (text[i] == '(' || text[i] == ')' || text[i] == '\\')
      ptext^[j++] = '\\';
    ptext^[j++] = text[i];
  }
  ptext^[j] = '\0';

  font2 = (int)font;
  if (font2 >= 3)
  {
    trace ("error: pdf_text() : font is >= 3\n");
    font2 = 0;
  }

  font_nr = font2 * 4 + (int)((style & _STYLE_ITALIC) != 0) + 2 * (int)((style & _STYLE_BOLD) != 0);

  use_font (ref pdf, font_nr);

  sprintf (out pbuffer^,
    "q\r\n"
  + "BT\r\n"
  + "/F%d %d Tf\r\n"
  + "%d %d Td\r\n"
  + "%u.%02u %u.%02u %u.%02u rg\r\n"
  + "(%s) Tj\r\n"
  + "ET\r\n"
  + "Q\r\n",
      font_nr, font_height,
      x, pdf.page_height - y,
      r / 255, (r * 100 / 255) % 100,
      g / 255, (g * 100 / 255) % 100,
      b / 255, (b * 100 / 255) % 100,
      ptext^);

  append_text (ref pdf, pbuffer^);

  free ptext;
  free pbuffer;
}

//--------------------------------------------------------------------------------------------------------------

void add_image (ref PDF pdf, int image_obj_nr)
{
  int[]^ p;

  if (pdf.nb_images == pdf.max_images)
  {
    pdf.max_images = (pdf.max_images + 1) * 2;

    p = new int[pdf.max_images];
    if (pdf.nb_images != 0)
      p^[0 : pdf.nb_images] = pdf.images^[0 : pdf.nb_images];

    free pdf.images;
    pdf.images = p;
  }

  pdf.images^[pdf.nb_images++] = image_obj_nr;
}

//--------------------------------------------------------------------------------------------------------------

public void pdf_jpeg (ref PDF pdf, string filename, int x, int y, int width, int height)
{
  IMAGE    image;
  uint     jpeg_width, jpeg_height;
  int      rc, in, rest, chunk;
//  byte     head[2];
  int      jpeg_size;
  char     buffer[1024];
  int      jpeg_nr = ++pdf.unique_object_nr;

  add_image (ref pdf, jpeg_nr);

  rc = open_image (out image, filename);
  if (rc != 0)
  {
    trace ("error: pdf_jpeg() : cannot open image '%s'\n", filename);
    pdf.error = 1;
    return;
  }

  get_image_size (image, out jpeg_width, out jpeg_height);

  if (close_image (ref image) < 0)
  {
    trace ("error: pdf_jpeg() : cannot close image '%s'\n", filename);
    pdf.error = 1;
    return;
  }

  in = open (filename);
  if (in < 0)
  {
    trace ("error: pdf_jpeg() : cannot open %s\n", filename);
    pdf.error = 1;
    return;
  }

/*
  if (read (in, out head) != 2 || head[0] != 0xFF || head[1] != 0xD8)
  {
    trace ("error: pdf_jpeg() : %s must be jpeg file\n", filename);
    close (in);
    pdf.error = 1;
    return;
  }
*/

  jpeg_size = (int)lseek (in, 0L, SEEK_END);
  if (jpeg_size < 0)
  {
    trace ("error: pdf_jpeg() : lseek() on %s failed\n", filename);
    close (in);
    pdf.error = 1;
    return;
  }

  if (lseek (in, 0L, SEEK_SET) != 0)
  {
    trace ("error: pdf_jpeg() : lseek() on %s failed\n", filename);
    close (in);
    pdf.error = 1;
    return;
  }

  start_object (ref pdf, jpeg_nr);

  sprintf (out buffer,
    "%d 0 obj\r\n"
  + "<<\r\n"
  + "/Type /XObject\r\n"
  + "/Subtype /Image\r\n"
  + "/Name /Im%d\r\n"
  + "/BitsPerComponent 8\r\n"
  + "/Width %u\r\n"
  + "/Height %u\r\n"
  + "/ColorSpace /DeviceRGB\r\n"
  + "/Filter /DCTDecode\r\n"
  + "/Length %d\r\n"
  + ">>\r\n"
  + "stream\r\n",
        jpeg_nr, jpeg_nr, jpeg_width, jpeg_height, jpeg_size);

  putstr (ref pdf, buffer);


  rest = jpeg_size;
  while (rest > 0)
  {
    if (rest >= (int)buffer'size)
      chunk = (int)buffer'size;
    else
      chunk = rest;

    if (read (in, out buffer[0 : chunk]) != chunk)
    {
      trace ("error: pdf_jpeg() : cannot read %s\n", filename);
      close (in);
      pdf.error = 1;
      return;
    }

    put (ref pdf, buffer[0 : chunk]);

    rest -= chunk;
  }

  close (in);

  sprintf (out buffer,
    "\r\n"
  + "endstream\r\n"
  + "endobj\r\n"
  + "\r\n");

  putstr (ref pdf, buffer);


  sprintf (out buffer,
    "q\r\n"
  + "%d 0 0 %d %d %d cm\r\n"
  + "/Im%d Do\r\n"
  + "Q\r\n",
    width, height, x, pdf.page_height - y - height, jpeg_nr);

  append_text (ref pdf, buffer);
}

//--------------------------------------------------------------------------------------------------------------
