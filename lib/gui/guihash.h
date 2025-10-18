
// guihash.h

use ../gui;

#if WINDOWS
  use ../win/windows;
#elif ANDROID
  use guitree;
#else
  bad
#endif  

// + do a postmessage create !
void treat_create_dialog (DIALOG_ID id);

// when returning false, you must DESTROY the window.
bool treat_init_window (DIALOG_ID id, HWND hwnd);

// post a destroy message if return value is not zero
HWND treat_delete_dialog (DIALOG_ID id);

// returns 0 if not found
HWND find_hwnd_of_dialog (DIALOG_ID id);
