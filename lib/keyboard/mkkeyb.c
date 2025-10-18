
// mkkeyb.c : generate keyboard jpg images as data

from std use console, draw, exception, image, files, strings;
use keyboard_pos;

//-----------------------------------------------------------------------------------------------

byte[]^ compress (ref IMAGE_INFO img, int set)
{
  IMAGE_CREATION_PARAMETERS param;
  byte[]^      p;
  int          rc;
  
  clear param;
  param.format = FORMAT_JPEG;
  param.quality = 80;

  rc = compress_image (img, param, out p);
  printf ("save rc = %d\n", rc);


if (false)
{
  char filename[256];
  int fd;
  sprintf (out filename, "temp%d.jpg", set);
  fd = create (filename);
  write (fd, p^);
  close (fd);
}

  free_image (ref img);

  return p;
}

//-----------------------------------------------------------------------------------------------

void patch_keys (IMAGE_INFO img, int set)
{
  int l, w;
  DRAW_CONTEXT dc;
  
  init_draw (out dc, buffer => img.pixel, width => img.width, height => img.height);
                    
  for (l=0; l<KEYBOARD_CODES'length; l++)
  {
    ref KEY_LINE line = KEYBOARD_CODES[l];
    int y = line.y;
    
    for (w=0; w<line.keys'length; w++)
    {
      ref KEY_CODE kc = line.keys[w];
      int x = kc.x;
      char c = kc.letter[set];
      int len;
      
      if (x > 450)
        x -= (x-450) / 100;
      else
        x += (450-x) / 100;
      
      len = draw_text (ref dc,
                 text => {c},
                 font_name => "Verdana",
                 font_height => 90,
                 x       => 4000,
                 y       => 0,
                 color   => 0xFFFFFFFF,
                 style   => _STYLE_BOLD);
      
      draw_text (ref dc,
                 text => {c},
                 font_name => "Verdana",
                 font_height => 90,
                 x       => x - len/2,
                 y       => y+45,
                 color   => 0xFFC1BEB6, // 182, 190, 193   
                 style   => _STYLE_BOLD);
    }
  }
}

//-----------------------------------------------------------------------------------------------

void main ()
{
  IMAGE_INFO   img;
  FILE         fp;
  int          rc, i, set;
  byte[]^      p;
  
  arm_exception_handler ();

  fcreate (out fp, "keyboard.h", ANSI);

  fprintf (ref fp, "\n// keyboard.h\n\n");

  fprintf (ref fp, "// DO NOT EDIT - THIS FILE IS GENERATED AUTOMATICALLY\n\n");

  fprintf (ref fp, "const byte[] KEYBOARD_JPG[3] = {\n");

  for (set=0; set<3; set++)
  {
    rc = load_image (out img, "keyboard v10.png");
    assert rc == 0;

    patch_keys (img, set);
    
    p = compress (ref img, set);

    fprintf (ref fp, "{\n");
    
    for (i=0; i<p^'length; i++)
    {
      fprintf (ref fp, "%3u,", p^[i]);
      if ((i & 15) == 15)
      fprintf (ref fp, "\n");
    }
    fprintf (ref fp, "},\n\n");

    free p;
  }

  fprintf (ref fp, "};\n\n");
  fclose (ref fp);

  printf ("ok\n");
}

//-----------------------------------------------------------------------------------------------
