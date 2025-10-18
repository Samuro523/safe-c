
// mkfont.c : generate a serie of model raster fonts of height 64 pixels

from std use console, exception, image, draw, files, strings;

const string[] FONTS = {"Arial", "Courier New", "Microsoft Sans Serif", "Tahoma", "Verdana"};

const int HEIGHT = 64;
const int WIDTH = HEIGHT/2;
const int NB_CHARS = 256 - 32;

void main ()
{
  byte[]^      p;
  DRAW_CONTEXT dc;
  int          rc;
  IMAGE_INFO   img, img2;
  IMAGE_CREATION_PARAMETERS param;
  wchar[256]   str;
  int          i;
  CLIP_INFO    clip;
  int          offset[257];
  int          max_w;
  int          font_nr;
  FILE         fp;
  IMAGE_INFO img3;

  arm_exception_handler ();

  clear img3;
  img3.width = (uint)HEIGHT;
  img3.height = (uint)HEIGHT;
  img3.pixel = new byte[4*HEIGHT*HEIGHT];
  init_draw (out dc, img3.pixel, img3.width, img3.height);

  p = new byte[4*WIDTH*HEIGHT*NB_CHARS];
  
  fcreate (out fp, "fontbitmaps.h", ANSI);

  fprintf (ref fp, "\n// fontbitmaps.h\n\n");

  fprintf (ref fp, "// DO NOT EDIT - THIS FILE IS GENERATED AUTOMATICALLY\n\n");

  fprintf (ref fp, "// fonts are :");
  for (font_nr=0; font_nr<FONTS'length; font_nr++)
    fprintf (ref fp, " \"%s\"", FONTS[font_nr]);
  fprintf (ref fp, "\n\n");

  fprintf (ref fp, "struct TYPE_FONT_DATA\n");
  fprintf (ref fp, "{\n");
  fprintf (ref fp, "  string    name;\n");
  fprintf (ref fp, "  int       max_width;\n");
  fprintf (ref fp, "  int2[257] x_offset;\n");
  fprintf (ref fp, "  int       bitmap_width;\n");  
  fprintf (ref fp, "  int       bitmap_height;\n");  
  fprintf (ref fp, "  byte[]    bitmap;\n");
  fprintf (ref fp, "}\n\n");
 
  fprintf (ref fp, "const TYPE_FONT_DATA FONT_DATA[%d] =\n", FONTS'length);
  fprintf (ref fp, "{\n");
  
  for (font_nr=0; font_nr<FONTS'length; font_nr++)
  {
    fprintf (ref fp, "  {\n");
    fprintf (ref fp, "    name      => \"%s\",\n", FONTS[font_nr]);
    
    p^ = {all => 255};

    clear img;
    img.pixel = p;
    img.width = (uint)(NB_CHARS*WIDTH);
    img.height = (uint)HEIGHT;

    clear str, offset;
    rc = 0;
    for (i=0; i<NB_CHARS; i++)
    {
      CLIP_INFO source_clip, target_clip;
      
      img3.pixel^ = {all => 255};

      str[0] = (wchar)(i+32);

      rc = draw_wtext (ref dc,
                       str,
                       FONTS[font_nr],
                       font_height => (int)img.height,
                       x => 0,          /* lower left corner */
                       y => HEIGHT-1,
                       color => 0,
                       style => 0);         /* = 0 */
      if (rc < 0)
        rc = 0;

      clear source_clip;
      source_clip.offset_x = 0;
      source_clip.offset_y = 0;
      source_clip.size_x = (uint)rc;
      source_clip.size_y = (uint)HEIGHT;

      clear target_clip;
      target_clip.offset_x = (uint)offset[i+32];
      target_clip.offset_y = 0;
      target_clip.size_x = (uint)rc;
      target_clip.size_y = (uint)HEIGHT;

  
      assert replace_image_rectangle (img3, source_clip, img, target_clip) == 0;
                             
      
      offset[i+32+1] = offset[i+32] + rc;
    }


    // convert to greyscale

    for (i=0; i<p^'length; i+=4)
    {
      byte[4] pixel;
      uint av;

      pixel'byte = p^[i:4];

      av = (30*pixel[0] + 59*pixel[1] + 11*pixel[2]) / 100;

      p^[i:3] = {(byte)av, (byte)av,(byte)av};
    }



    // cutout real width

    clear clip;
    clip.size_x = (uint)offset[256];
    clip.size_y = (uint)HEIGHT;

    rc = copy_image_rectangle (img, clip, out img2);
    assert rc == 0;


    // generate bitmap

    max_w = 0;
    for (i=0; i<256; i++)
      if (offset[i+1] - offset[i] > max_w)
        max_w = offset[i+1] - offset[i];
    fprintf (ref fp, "    max_width => %d,\n", max_w);

    fprintf (ref fp, "    x_offset  => int2[257] ' {\n");
    for (i=0; i<257; i++)
    {
      fprintf (ref fp, "%4d", offset[i]);
      if (i+1 < 257)
        fprintf (ref fp, ",");
      if ((i & 15) == 15)
        fprintf (ref fp, "\n");
    }
    fprintf (ref fp, "},\n");

    fprintf (ref fp, "    bitmap_width  => %u,\n", clip.size_x);
    fprintf (ref fp, "    bitmap_height => %u,\n", clip.size_y);
    
    fprintf (ref fp, "    bitmap => byte[%u] ' {  // %s : %u x %u byte pixels\n", clip.size_y*clip.size_x, FONTS[font_nr], clip.size_x, clip.size_y);
    for (i=0; i<(int)(4*clip.size_y*clip.size_x); i+=4)
    {
      fprintf (ref fp, "%3u,", img2.pixel^[i]);
      if (((i>>2) & 31) == 31)
        fprintf (ref fp, "\n");
    }
    fprintf (ref fp, "},\n");

    fprintf (ref fp, "  },\n");




    // save

if (false)
    {
      char filename[260];
      
      sprintf (out filename, "%s.png", FONTS[font_nr]);
      
      clear param;
      param.format = FORMAT_PNG;
      param.use_transparency = 2;

      rc = save_image (img2, filename, param);
      printf ("save rc = %d\n", rc);
    }
    
  }


  fprintf (ref fp, "};\n");
  fclose (ref fp);
}
