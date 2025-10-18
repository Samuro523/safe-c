
// clipboard.c

#if WINDOWS
  use win/windows;
#endif  

/**********************************************************************/
#begin unsafe
/**********************************************************************/

public int save_data_to_clipboard (byte[] data, uint kind)
{
#if WINDOWS

  HANDLE h;
  uint    size, extra;
  byte   *p;

  if (OpenClipboard (0) == FALSE)
    return -1;

  if (EmptyClipboard () == FALSE)
  {
    CloseClipboard ();
    return -1;
  }

  size = data'size;
  switch (kind)
  {
    case CF_TEXT:
    case CF_OEMTEXT:
      extra = 1;
      break;

    case CF_UNICODETEXT:
      extra = 2;
      break;

    default:
      extra = 0;
      break;
  }

  h = GlobalAlloc (GMEM_MOVEABLE, size+extra);
  if (h == 0)
  {
    CloseClipboard ();
    return -1;
  }

  p = GlobalLock (h);
  if (p == null)
  {
    GlobalFree (h);
    CloseClipboard ();
    return -1;
  }

  p[0:size] = data;
  p[size:extra] = {all => 0};

  GlobalUnlock (h);

  if (SetClipboardData (kind, h) == 0)
  {
    CloseClipboard ();
    GlobalFree (h);
    return -1;
  }

  if (CloseClipboard () == FALSE)
  {
    GlobalFree (h);
    return -1;
  }

  GlobalFree (h);

  return 0;
#endif

#if ANDROID
  _unused data, kind;
  return 0;
#endif 
}

/**********************************************************************/

public byte[]^ load_data_from_clipboard (uint kind)
{
#if WINDOWS

  HANDLE  h;
  byte    *p;
  uint    size;
  byte[]^ result;
  
  if (OpenClipboard (0) == FALSE)
    return null;

  h = GetClipboardData (kind);
  if (h == 0)
  {
    CloseClipboard ();
    return null;
  }

  p = GlobalLock (h);
  if (p == null)
  {
    CloseClipboard ();
    return null;
  }

  size = (uint)GlobalSize(h);
  
  result = new byte[size];
  result^[0 : size] = p[0 : size];

  GlobalUnlock (h);

  if (CloseClipboard () == FALSE)
  {
    free result;
    return null;
  }

  return result;
#endif

#if ANDROID
  _unused kind;
  return null;
#endif  
}

/**********************************************************************/
#end unsafe
/**********************************************************************/
