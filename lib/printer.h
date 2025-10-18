
// printer.h

//--------------------------------------------------------------------------

// returns nb of jobs currently in printer spool or -1 in case of error.
// 'printer_name' can be set to "" to specify the default local printer.

int nb_jobs_in_spool (string printer_name = "");

//--------------------------------------------------------------------------

// 'printer_name' can be set to "" to specify the default local printer.
// returns 0 if OK, -1 in case of error.
// all further functions apply to the currently open printer.

int open_printer (string printer_name = "", string document_name = "Document");
void close_printer ();
void close_printer_and_cancel_job ();

//--------------------------------------------------------------------------

void printer_new_page ();

//--------------------------------------------------------------------------

// this function can only be called immediately after open_printer() or printer_new_page().
// it will reset all font sizes to defaults.
// default is PORTRAIT

enum ORIENTATION {PORTRAIT, LANDSCAPE};

int printer_set_orientation (ORIENTATION orientation);

//--------------------------------------------------------------------------

void get_printer_resolution (out int x_res, out int y_res);

// returns margin sizes in pixels (margin areas are unprintable)
void get_printer_margins (out int top, out int bottom, out int left, out int right);

//--------------------------------------------------------------------------

// 1) print text in "Courier New", with automatic new page.

const int DEFAULT_PRINTER_COLUMNS = 80;     // for portrait orientation
const int DEFAULT_PRINTER_LINES   = 64;     // for portrait orientation

// set font size by specifying nb of columns and lines
void printer_set_text_size (int max_cols, int max_lines);

// set current position on the page
void printer_set_column (int col);       // starting at 1
void printer_set_line   (int line);      // starting at 1

// get current position on the page
int printer_column ();              // returns current column
int printer_line   ();              // returns current line

// print text, starts automatically a new page when needed.
void printerf (string format, object[] arg);
void wprinterf (wstring format, object[] arg);

//--------------------------------------------------------------------------

// 2) letter-quality printing

const string ARIAL                = "Arial";
const string COURIER              = "Courier";
const string COURIER_NEW          = "Courier New";
const string MICROSOFT_SANS_SERIF = "Microsoft Sans Serif";
const string TIMES_NEW_ROMAN      = "Times New Roman";

const int MAX_FONT_NAME_LENGTH = 32;

const uint UNDERLINED  = 1;
const uint ITALIC      = 2;
const uint BOLD        = 4;

packed struct FONT
{
  char name[MAX_FONT_NAME_LENGTH]; // ARIAL, COURIER, ..
  int  height;                     // in pixels
  uint style;                      // a combination of UNDERLINED, ITALIC, BOLD
  uint color;                      // rgb color of text
}

// compute width of a text, in pixels, using specified font.
// these functions are not multi-thread safe.
int text_width_of2 (string text, FONT font);
int wtext_width_of2 (wstring text, FONT font);

// printed text must not contain control characters
void ext_printerf (int      x,   // top/left coordinates
                   int      y,
                   FONT     font,
                   string   format,
                   object[] arg);

// printed text must not contain control characters
void ext_wprinterf (int      x,   // top/left coordinates
                    int      y,
                    FONT     font,
                    wstring  format,
                    object[] arg);

// print an image.
// 'raster' : image as an array [size_y] [size_x] of pixels.
//   Each pixel is stored in 4-byte RGBs indicating the amount
//   of RED,GREEN,BLUE; 's' is reserved for future use.
//   Image pixels are stored from top to bottom, line by line.

void printer_put_raster (int    x,
                         int    y,
                         int    size_x,
                         int    size_y,
                         byte[] raster);

//--------------------------------------------------------------------------
