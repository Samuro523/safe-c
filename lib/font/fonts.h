
// fonts.h (used for android that has not access to fonts in native apps)

use ../image;

// ------------------------------------------------------------------------------------------

// returns font_index (you can create max 16 fonts, then the old ones are deleted)
int create_font (string font_name, int height, uint style);

void delete_font (int font_index);

// ------------------------------------------------------------------------------------------

// background can be transparent or solid color, text will be mixed with it
// returns width in pixels

int draw_font_text (IMAGE_INFO image,
                    CLIP_INFO  clip,        // don't draw outside clipping rectangle
                    string     text,
                    int        font_index,  // from create_font (string font_name, int height, uint style)
                    int        left_x,      // lower left corner (0,0 is top left)
                    int        top_y,
                    uint       color);      // RGB text color (4th byte is ignored)

// ------------------------------------------------------------------------------------------

// background can be transparent or solid color, text will be mixed with it
// returns width in pixels

int draw_font_wtext (IMAGE_INFO image,
                     CLIP_INFO  clip,        // don't draw outside clipping rectangle
                     wstring    text,
                     int        font_index,  // from create_font (string font_name, int height, uint style)
                     int        left_x,      // lower left corner (0,0 is top left)
                     int        top_y,
                     uint       color);      // RGB text color (4th byte is ignored)

// ------------------------------------------------------------------------------------------

int width_of_font_text (string  text,
                        int     font_index);   // from create_font (string font_name, int height, uint style)

// ------------------------------------------------------------------------------------------

int width_of_font_wtext (wstring  text,
                         int     font_index);   // from create_font (string font_name, int height, uint style)

// ------------------------------------------------------------------------------------------

int height_of_font (int font_index);   // from create_font (string font_name, int height, uint style)

// ------------------------------------------------------------------------------------------
