
// windows.c

#begin unsafe

public void SafeRelease (LPVOID **ppT)
{
  if (*ppT != null)
  {
    ((IUnknown*)(*ppT))->lpVtbl->Release((LPVOID)(*ppT));
    *ppT = null;
  }
}

public uint RGB (uint r, uint g, uint b)
{
  return ((BYTE)(r))
       | ((WORD)((BYTE)(g))<<8)
       | (((DWORD)(BYTE)(b))<<16);
}

public LRESULT SendMessagePtrA (HWND hwnd, UINT message, WPARAM wParam, char* lParam)
{
  LPARAM i;
  i'byte = lParam'byte;
  return SendMessageA (hwnd, message, wParam, i);
}

public LRESULT SendMessagePtrW (HWND hwnd, UINT message, WPARAM wParam, wchar* lParam)
{
  LPARAM i;
  i'byte = lParam'byte;
  return SendMessageW (hwnd, message, wParam, i);
}


#if MEM32
  public PVOID GetWindowLongPtrA (HWND hWnd, int nIndex)
  {
    LONG val = GetWindowLongA (hWnd, nIndex);
    PVOID ptr;
    ptr'byte = val'byte;
    return ptr;
  }
  
  public PVOID SetWindowLongPtrA (HWND hWnd, int nIndex, PVOID dwNewLong)
  {
    LONG val;
    val'byte = dwNewLong'byte;
    SetWindowLongA (hWnd, nIndex, val);
    return dwNewLong;
  }
#endif

public int negative_last_windows_error()
{
  int rc = (int)GetLastError ();
  
  if (rc > 0)
    return -rc;   // make negative
  
  if (rc == 0)
    rc = -1;    // don't return zero as it would signal no error
  
  return rc;
}


#end unsafe
