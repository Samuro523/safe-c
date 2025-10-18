
// guidraw.h

#if WINDOWS
  use ../win/windows;
#elif ANDROID
#else
  bad
#endif  
  
use guitree;

void redraw_dialogs (HWND hwnd, HDC hdc, wstring title, PAINTSTRUCT ps, DIALOG_INFO d);
void update_caret (HWND hwnd, DIALOG_INFO d);
