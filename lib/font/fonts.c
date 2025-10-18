
// fonts.c

use ../image, ../strings, ../thread;
use fontbitmaps;

/**************************************************************************/

struct CACHED_FONT
{
  int        name_index;  // index into FONT_DATA

  int2[257]^ x_offset;

  int        bitmap_width;
  int        bitmap_height;      // 6 to 64
  byte[]^    bitmap;

  uint       last_tick;
}

/**************************************************************************/

const int MAX_CACHED_FONTS = 16;
CACHED_FONT g_cached_font[MAX_CACHED_FONTS];

/**************************************************************************/

// IMAGE_INFO denotes a BYTE pixel image (1 greyscale byte per pixel)

void stretch_down_font (byte[]     source_pixel,
                        uint       source_width,
                        uint       source_height,   // always 64
                        CLIP_INFO  source_clip,
                        IMAGE_INFO target,          // small font (height <= 64)
                        CLIP_INFO  target_clip)
{
  const uint LSHIFTS = 3;    // 1 = bad quality, 4 = best quality
  uint[]^    ptab_x, ptab_y;

  _unused source_height;


  /* allocate conversion tables */

  ptab_x = new uint [(target_clip.size_x << LSHIFTS)];
  ptab_y = new uint [(target_clip.size_y << LSHIFTS)];


  {
    ref uint[] tab_x = ptab_x^;
    ref uint[] tab_y = ptab_y^;

    uint       i, i9, increment, limit, sum, x, y, x9, y9;


    /* fill conversion tables : "tab_x[target_x << LSHIFTS] -> source_x" */

    x         = 0;
    sum       = 0;
    increment = source_clip.size_x;
    limit     = target_clip.size_x;
    i9        = limit << LSHIFTS;

    for (i=0; i<i9; i++)
    {
      tab_x[i] = x >> LSHIFTS;
      sum += increment;
      while (sum >= limit)
      {
        sum -= limit;
        x++;
      }
    }

    y         = 0;
    sum       = 0;
    increment = source_clip.size_y;
    limit     = target_clip.size_y;
    i9        = limit << LSHIFTS;

    for (i=0; i<i9; i++)
    {
      tab_y[i] = y >> LSHIFTS;
      sum += increment;
      while (sum >= limit)
      {
        sum -= limit;
        y++;
      }
    }


    /* copy, line by line */

#begin unsafe
    {
      byte* base_source, base_target, psource, ptarget;
      byte* ptarget_y;
      uint* ptrtab_y, ptrtab_x;

      base_source = (byte *)&source_pixel[(uint)source_clip.offset_x + (uint)source_clip.offset_y * source_width];
      base_target = (byte *)&target.pixel^[(uint)target_clip.offset_x + (uint)target_clip.offset_y * target.width];

      psource = base_source;
      ptarget = base_target;

      /* compute average greyscale tone of each new pixel */

      x9        = target_clip.size_x;
      y9        = target_clip.size_y;
      ptarget_y = base_target;

      ptrtab_y = &tab_y[0];

      for (y=0; y<y9; y++)       // for each target pixel on y
      {
        ptrtab_x = &tab_x[0];

        ptarget = ptarget_y;

        for (x=0; x<x9; x++)      // for each target pixel on x
        {
          uint yy, xx;
          uint col;

          col = 0;

          for (yy=0; yy<(1<<LSHIFTS); yy++)       // 8 samples on y
          {
            psource = base_source + (source_width * ptrtab_y[yy]);

            for (xx=0; xx<(1<<LSHIFTS); xx++)       // 8 samples on x
            {
              col += psource[ptrtab_x[xx]];         // sum 64 samples
            }
          }

          *ptarget++ = (byte)(col >> (2*LSHIFTS));  // store value divided by 2^6

          ptrtab_x += (1<<LSHIFTS);

        }  // for x

        ptarget_y += target.width;
        ptrtab_y += (1<<LSHIFTS);

      }  // for y
    }
#end unsafe
  }

  /* free all */

  free ptab_x;
  free ptab_y;
}

/**************************************************************************/

void stretch_font (TYPE_FONT_DATA f, ref CACHED_FONT c)
{
  int       i;
  CLIP_INFO source_clip, target_clip;
  
  assert f.bitmap_height == 64;
  c.bitmap_width = (f.bitmap_width * c.bitmap_height) >> 6;

  c.bitmap = new byte[c.bitmap_width * c.bitmap_height];    // 1 byte per pixel

  c.x_offset = new int2[257];

  clear source_clip, target_clip;
  source_clip.size_y = (uint)f.bitmap_height;
  target_clip.size_y = (uint)c.bitmap_height;

  {
    ref int2[257] ofs = c.x_offset^;
    for (i=32; i<256; i++)
    {
      ofs[i+1] = (int2)(ofs[i] + ((f.x_offset[i+1] - f.x_offset[i]) * c.bitmap_width / f.bitmap_width));

      if (ofs[i+1] > 0)
      {
        source_clip.offset_x = f.x_offset[i];
        source_clip.size_x   = (uint)f.x_offset[i+1] - (uint)source_clip.offset_x;
        
        target_clip.offset_x = ofs[i];
        target_clip.size_x   = (uint)ofs[i+1] - (uint)target_clip.offset_x;
        
        if (target_clip.size_x > 0)
        {
          stretch_down_font (source_pixel  => f.bitmap,
                             source_width  => (uint)f.bitmap_width,
                             source_height => (uint)f.bitmap_height,
                             source_clip   => source_clip,
                             target        => {pixel  => c.bitmap,
                                               width  => (uint)c.bitmap_width,
                                               height => (uint)c.bitmap_height},
                             target_clip   => target_clip);
        }
      }
    }
  }
}

/**************************************************************************/

public
void delete_font (int font_index)
{
  ref CACHED_FONT c = g_cached_font[font_index];
  free c.x_offset, c.bitmap;
  clear c;
}

/**************************************************************************/

// returns font_index into table g_cached_font
public int create_font (string font_name, int height, uint style)
{
  int  adapted_height;
  int  font_nr, i;
  uint now = ticks();
  uint best_elapsed;
  int  best_index;

  _unused style;   // for later

  if (height < 8)
    adapted_height = 8;
  else if (height > 128)
    adapted_height = 128;
  else
    adapted_height = height;

  if (stristr (text => font_name, fragment => "arial") != -1)
    font_nr = 0;
  else if (stristr (text => font_name, fragment => "cour") != -1)
    font_nr = 1;
  else if (stristr (text => font_name, fragment => "sans ser") != -1)
    font_nr = 2;
  else if (stristr (text => font_name, fragment => "taho") != -1)
    font_nr = 3;
  else // if (stristr (text => font_name, fragment => "verda") != -1)
    font_nr = 4;   // Verdana by default

  // search in cache if we have this font with this height
  for (i=0; i<MAX_CACHED_FONTS; i++)
  {
    ref CACHED_FONT c = g_cached_font[i];
    if (c.name_index == font_nr && c.bitmap_height == adapted_height)
    {
      c.last_tick = now;
      return i;   // found !
    }
  }

  // we need to evict the oldest font

  best_elapsed = 0;
  best_index = 0;
  for (i=0; i<MAX_CACHED_FONTS; i++)
  {
    ref CACHED_FONT c = g_cached_font[i];
    uint diff = now - c.last_tick;   // elapsed time since last use

    if (diff > best_elapsed)
    {
      best_elapsed = diff;
      best_index = i;
    }
  }

  i = best_index;


  // evict cache font i

  delete_font (i);


  // generate a new font

  {
    ref CACHED_FONT c = g_cached_font[i];

    c.name_index = font_nr;
    c.bitmap_height = adapted_height;

    stretch_font (FONT_DATA[font_nr], ref c);

    c.last_tick = now;
  }

  return i;
}

/**************************************************************************/

// background can be transparent or solid color, text will be mixed with it
// returns width in pixels

public
int draw_font_text (IMAGE_INFO image,
                    CLIP_INFO  clip,        // don't draw outside clipping rectangle
                    string     text,
                    int        font_index,  // from create_font (string font_name, int height, uint style)
                    int        left_x,      // lower left corner (0,0 is top left)
                    int        top_y,
                    uint       color)       // RGB text color (4th byte is ignored)
{
#begin unsafe
  ref CACHED_FONT c = g_cached_font[font_index];
  int   target_x0, target_y0, target_stride, x, y, i, dy;
  uint  greyscale;
  byte  col[4];
  byte* image_bitmap = &image.pixel^;
  byte* font_bitmap  = &c.bitmap^;

  target_x0 = left_x;
  target_y0 = top_y;
  target_stride = 4*(int)image.width;
  dy = c.bitmap_height;

  col = color'byte;
  col[3] = 255;

  for (i=0; i<text'length; i++)
  {
    byte ch = (byte)text[i];
    int fx = c.x_offset^[ch];
    int dx = c.x_offset^[ch+1] - fx;

    // check if symbol will be visible in rectangle
    if (target_x0+dx-1 >= clip.offset_x && target_x0 < clip.offset_x + (int)clip.size_x &&
        target_y0+dy-1 >= clip.offset_y && target_y0 < clip.offset_y + (int)clip.size_y)
    {
      for (y=0; y<dy; y++)
      {
        if (target_y0+y >= clip.offset_y && target_y0+y < clip.offset_y + (int)clip.size_y)
        {
          int target_ofs = (target_y0+y) * target_stride + target_x0 * 4;
          byte* font_bitmap2 = &font_bitmap[y * c.bitmap_width + fx];
          
          for (x=0; x<dx; x++)
          {
            if (target_x0+x >= clip.offset_x && target_x0+x < clip.offset_x + (int)clip.size_x)
            {
              ref byte[4] pix = image_bitmap[target_ofs:4];
              byte[4]     b = pix;
              uint        ngreyscale;

              greyscale = font_bitmap2[x];
              if (greyscale == 255)
                greyscale = 256;
              ngreyscale = 256 - greyscale;
               
              // greyscale = 0 -> full color col
              // greyscale = 255 -> should keep original b
              b[0] = (byte)(((b[0] * greyscale) + (ngreyscale * col[0])) >> 8);
              b[1] = (byte)(((b[1] * greyscale) + (ngreyscale * col[1])) >> 8);
              b[2] = (byte)(((b[2] * greyscale) + (ngreyscale * col[2])) >> 8);
              b[3] = (byte)(((b[3] * greyscale) + (ngreyscale * col[3])) >> 8);

              pix = b;
            }

            target_ofs += 4;
          }
        }
      }
    }
    
    target_x0 += dx;
  }
  
  return target_x0 - left_x;
#end unsafe
}

/**************************************************************************/

// background can be transparent or solid color, text will be mixed with it
// returns width in pixels

public
int draw_font_wtext (IMAGE_INFO image,
                     CLIP_INFO  clip,        // don't draw outside clipping rectangle
                     wstring    text,
                     int        font_index,  // from create_font (string font_name, int height, uint style)
                     int        left_x,      // lower left corner (0,0 is top left)
                     int        top_y,
                     uint       color)       // RGB text color (4th byte is ignored)
{
#begin unsafe
  ref CACHED_FONT c = g_cached_font[font_index];
  int   target_x0, target_y0, target_stride, x, y, i, dy;
  uint  greyscale;
  byte  col[4];
  byte* image_bitmap = &image.pixel^;
  byte* font_bitmap  = &c.bitmap^;

  target_x0 = left_x;
  target_y0 = top_y;
  target_stride = 4*(int)image.width;
  dy = c.bitmap_height;

  col = color'byte;
  col[3] = 255;

  for (i=0; i<text'length; i++)
  {
    uint ch = (uint)text[i];
    
    if (ch == 961)   // planet currency sign
      ch = 112;      // p
      
    ch = (byte)ch;
      
    {
      int fx = c.x_offset^[ch];
      int dx = c.x_offset^[ch+1] - fx;

      // check if symbol will be visible in rectangle
      if (target_x0+dx-1 >= clip.offset_x && target_x0 < clip.offset_x + (int)clip.size_x &&
          target_y0+dy-1 >= clip.offset_y && target_y0 < clip.offset_y + (int)clip.size_y)
      {
        for (y=0; y<dy; y++)
        {
          if (target_y0+y >= clip.offset_y && target_y0+y < clip.offset_y + (int)clip.size_y)
          {
            int target_ofs = (target_y0+y) * target_stride + target_x0 * 4;
            byte* font_bitmap2 = &font_bitmap[y * c.bitmap_width + fx];
            
            for (x=0; x<dx; x++)
            {
              if (target_x0+x >= clip.offset_x && target_x0+x < clip.offset_x + (int)clip.size_x)
              {
                ref byte[4] pix = image_bitmap[target_ofs:4];
                byte[4]     b = pix;
                uint        ngreyscale;

                greyscale = font_bitmap2[x];
                if (greyscale == 255)
                  greyscale = 256;
                ngreyscale = 256 - greyscale;
                 
                // greyscale = 0 -> full color col
                // greyscale = 255 -> should keep original b
                b[0] = (byte)(((b[0] * greyscale) + (ngreyscale * col[0])) >> 8);
                b[1] = (byte)(((b[1] * greyscale) + (ngreyscale * col[1])) >> 8);
                b[2] = (byte)(((b[2] * greyscale) + (ngreyscale * col[2])) >> 8);
                b[3] = (byte)(((b[3] * greyscale) + (ngreyscale * col[3])) >> 8);

                pix = b;
              }

              target_ofs += 4;
            }
          }
        }
      }
     
      target_x0 += dx;
    }
  }
  
  return target_x0 - left_x;
#end unsafe
}

/**************************************************************************/

public
int width_of_font_text (string  text,
                        int     font_index)  // from create_font (string font_name, int height, uint style)
{
  ref CACHED_FONT c = g_cached_font[font_index];
  ref int2[257]   ofs = c.x_offset^;
  int i, w;

  w = 0;

  for (i=0; i<text'length; i++)
  {
    byte ch = (byte)text[i];
    w += (ofs[ch+1] - ofs[ch]);
  }
  
  return w;
}

/**************************************************************************/

public
int width_of_font_wtext (wstring  text,
                         int     font_index)  // from create_font (string font_name, int height, uint style)
{
  ref CACHED_FONT c = g_cached_font[font_index];
  ref int2[257]   ofs = c.x_offset^;
  int i, w;

  w = 0;

  for (i=0; i<text'length; i++)
  {
    byte ch = (byte)text[i];
    w += (ofs[ch+1] - ofs[ch]);
  }
  
  return w;
}

/**************************************************************************/

public
int height_of_font (int font_index)   // from create_font (string font_name, int height, uint style)
{
  ref CACHED_FONT c = g_cached_font[font_index];
  return c.bitmap_height;
}

/**************************************************************************/

#if 0

void main()
{
  IMAGE_INFO img;
  int idx, x, y, ofs;
  CLIP_INFO  clip;
  char str[2];

  arm_exception_handler ();

  clear img;
  img.width = 1000;
  img.height = 1000;
  img.pixel = new byte[4 * img.width * img.height] ' {all => 255};

  clear clip;
  clip.size_x = img.width;
  clip.size_y = img.height;

// idx = create_font ("sans serif", height => 37, style => 0);
 idx = create_font ("arial", height => 64, style => 0);

  clear str;
  for (y=2; y<16; y++)
  {
    ofs = 50;
    for (x=0; x<16; x++)
    {
      str[0] = (char)(x+y*16);
      
      ofs += draw_font_text (img, clip, str, idx, left_x => ofs, bottom_y => -64 + y*64, color => 0x0);
    }
  }


/*
 draw_font_text (img, clip, "wiwiwiwiwi", idx, left_x => 20, bottom_y => (int)img.height-1, color => 0x8080FF);
                     


 idx = create_font ("arial", height => 37, style => 0);
 draw_font_text (img, clip, "wiwiwiwiwi", idx, left_x => 20, bottom_y => (int)img.height-1 - 37, color => 0x80FF80);

 idx = create_font ("courier new", height => 37, style => 0);
 draw_font_text (img, clip, "wiwiwiwiwi", idx, left_x => 20, bottom_y => (int)img.height-1 - 37-37, color => 0xFF8080);
*/

  {
    IMAGE_CREATION_PARAMETERS param;
    int rc;
    
    clear param;
    param.format = FORMAT_PNG;
    param.use_transparency = 2;

    rc = save_image (img, "test.png", param);
    printf ("save rc = %d\n", rc);
  }



if (false)
  {
    IMAGE_CREATION_PARAMETERS param;
    int rc;

    idx = create_font ("arial", height => 12, style => 0);

    img = {new byte[4*g_cached_font[idx].bitmap_width*g_cached_font[idx].bitmap_height], 
                    (uint)g_cached_font[idx].bitmap_width, (uint)g_cached_font[idx].bitmap_height};

    for (rc=0; rc<(int)(g_cached_font[idx].bitmap_width*g_cached_font[idx].bitmap_height); rc++)
    {
       img.pixel^[4*rc] = g_cached_font[idx].bitmap^[rc];
       img.pixel^[4*rc+1] = g_cached_font[idx].bitmap^[rc];
       img.pixel^[4*rc+2] = g_cached_font[idx].bitmap^[rc];
       img.pixel^[4*rc+3] = 255;
    }    
    
    clear param;
    param.format = FORMAT_PNG;
    param.use_transparency = 2;

    rc = save_image (img, "arial12.png", param);
    printf ("save rc = %d\n", rc);
  }
}

#endif

/**************************************************************************/
