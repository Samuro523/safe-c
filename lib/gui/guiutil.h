
// guiutil.h

#if WINDOWS
use ../win/windows;
#endif

use ../gui;
use guitree;


#if WINDOWS

// font need not be freed
// function is not multithread-safe
HFONT get_cached_font (FONT font);

// hdc need not be freed
// function is not multithread-safe
HDC get_cached_hdc ();

#endif



void compute_scrollbox_data
      (int height,           // must be (nb_lines*text_height)
       int scrollbar_arrow_box_height,
       int current_page_nr,
       int nb_listbox_lines,
       int nb_screen_lines,
   out int scrollbox_y_offset,
   out int scrollbox_height);

// for editbox:   
void recompute_font_width         (ref CONTROL_INFO o);
void compute_editbox_scroll_field (ref CONTROL_INFO o);
void compute_editbox_page         (ref CONTROL_INFO o);

int intern_text_width_of2 (string text, FONT font);
int intern_wtext_width_of2 (wstring text, FONT font);
