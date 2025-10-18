
// guigdi.h

use ../gui, guitree;

#begin unsafe


HBRUSH CreateSolidBrush (COLORREF color);
HFONT gdi_make_font (FONT font);

// returns previous one
HFONT    SelectFont   (HDC hdc, HFONT h);
HPEN     SelectPen    (HDC hdc, HPEN h);
HBRUSH   SelectBrush  (HDC hdc, HBRUSH h);
COLORREF SetTextColor (HDC hdc, COLORREF col);
COLORREF SetBkColor   (HDC hdc, COLORREF col);

void Polyline (HDC hdc, POINT* pts, int count);
void FillRect (HDC hdc, RECT* rect, HBRUSH brush_color);
void Ellipse (HDC hdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);
void Arc
  (HDC hdc,
   int nLeftRect, int nTopRect,
   int nRightRect, int nBottomRect,
   int nXStartArc, int nYStartArc,
   int nXEndArc, int nYEndArc);
   
void GetTextExtentPoint32W (HDC hdc, wchar* text, int length, SIZE* size);

const uint ETO_CLIPPED = 0x0004;
const uint ETO_OPAQUE  = 0x0002;

void ExtTextOutW (HDC    hdc,
                  int    x_left,
                  int    y_top,
                  uint   flags,
                  RECT*  rect,
                  wchar* text,
                  uint   text_length,
                  byte*  ptr);


const uint4 SRCCOPY = 0x00CC0020;
void BitBlt (HDC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight,
             HDC hdcSrc, int nXSrc, int nYSrc, uint4 dwRop);


// return NULLREGION if no more to draw
const int NULLREGION  = 1;
int ExcludeClipRect (HDC hdc, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);
 
#end unsafe
