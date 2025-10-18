
// clipboard.h

// kind :
const uint _CF_TEXT         =   1;
const uint _CF_BITMAP       =   2;
const uint _CF_TIFF         =   6;
const uint _CF_OEMTEXT      =   7;
const uint _CF_DIB          =   8;
const uint _CF_WAVE         =  12;
const uint _CF_UNICODETEXT  =  13;
const uint _CF_HDROP        =  15;

int save_data_to_clipboard (byte[] data,
                            uint   kind);    // _CF_TEXT, _CF_OEMTEXT, _CF_UNICODETEXT

byte[]^ load_data_from_clipboard (uint kind);  // _CF_TEXT, _CF_OEMTEXT, _CF_UNICODETEXT
