
// pdf.h : create PDF file

//=====================================================================================

struct PDF;

//=====================================================================================

int create_pdf (out PDF pdf, string filename);
int close_pdf (ref PDF pdf);

//=====================================================================================

// size of A4 page in PDF :
const int PDF_WIDTH  = 595;
const int PDF_HEIGHT = 842;

//=====================================================================================

void pdf_set_page_size (ref PDF pdf,
                            int width,    // default is PDF_WIDTH
                            int height);  // default is PDF_HEIGHT

//=====================================================================================

// style constants
const uint _STYLE_ITALIC = 0x0002;
const uint _STYLE_BOLD   = 0x0004;

enum FONT {PDF_FONT_TIMES_NEW_ROMAN, PDF_FONT_HELVETICA, PDF_FONT_COURIER};

void pdf_text (ref PDF pdf, string text, FONT font, int font_height,
               int x, int y,   // lower left corner
               uint color, uint style);

void pdf_jpeg (ref PDF pdf, string filename, int x, int y, int width, int height);

void new_page (ref PDF pdf);

//=====================================================================================
