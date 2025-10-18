
// extract resources from an EXE file

from std use console, files, strings, win/windows;


#begin unsafe

bool IS_INTRESOURCE (INT_PTR x)
{
  return (uint)x < 65536;
}

char* INT_PTR_TO_PTR (INT_PTR x)
{
  char* p;
  p'byte = x'byte;
  return p;
}


[callback]
BOOL EnumResLang(
  HMODULE hModule,
  INT_PTR lpszType,
  INT_PTR lpszName,
  WORD    wIDLanguage,
  DWORD   lParam)
{
  HRSRC r;
  HGLOBAL h;
  LPVOID lpResLock;
  DWORD size;
  char filename[260];
  int fd;

   _unused lParam;

  r = FindResourceExA (hModule, lpszType, lpszName, wIDLanguage);
  if (r == 0)
  {
    printf ("      error: cannot find resource\n");
    return TRUE;
  }

  h = LoadResource (hModule, r);
  if (h == 0)
  {
    printf ("      error: cannot load resource\n");
    return TRUE;
  }

  lpResLock = LockResource (h);
  if (lpResLock == null)
    return -1;

  size = SizeofResource (hModule, r);

  sprintf (out filename, "");

  if (IS_INTRESOURCE(lpszName))
    strcatf (ref filename, "%d", lpszName);
  else
    strcatf (ref filename, "%s", INT_PTR_TO_PTR (lpszName)[0:250]);
  strcat (ref filename, ".");

  if (IS_INTRESOURCE(lpszType))
    strcatf (ref filename, "%d", lpszType);
  else
    strcatf (ref filename, "%s", INT_PTR_TO_PTR (lpszType)[0:250]);
  strcat (ref filename, ".");

  strcatf (ref filename, "%04x", wIDLanguage);

  strcat (ref filename, ".resource");


  if (IS_INTRESOURCE(lpszName))
    printf ("%-20d", lpszName);
  else
    printf ("%-20s", INT_PTR_TO_PTR(lpszName)[0:250]);
  printf ("  ");

  if (IS_INTRESOURCE(lpszType))
    printf ("%-20d", lpszType);
  else
    printf ("%-20s", INT_PTR_TO_PTR(lpszType)[0:250]);
  printf ("  ");

  printf ("%s\n", filename);


  fd = create (filename);
  if (fd < 0)
  {
    printf ("        error: creating %s failed\n", filename);
    return TRUE;
  }

  if (write (fd, lpResLock[0:size]) != (int)size)
  {
    close (fd);
    delete_file (filename);
    printf ("        error: writing to %s failed\n", filename);
    return TRUE;
  }

  if (close (fd) < 0)
  {
    printf ("        error: closing %s failed\n", filename);
    return TRUE;
  }

  return TRUE;
}

[callback]
BOOL EnumResNames(
  HMODULE hModule,
  INT_PTR lpszType,
  INT_PTR  lpszName,
  LONG_PTR   lParam)
{
  _unused lParam;

#if 0
  if (IS_INTRESOURCE(lpszName))
    printf ("  name: %d\n", lpszName);
  else
    printf ("  name: %s\n", lpszName);
#endif

  EnumResourceLanguagesA (hModule, lpszType, lpszName, EnumResLang, 0);
  return TRUE;
}

[callback]
BOOL EnumResType (
  HMODULE hModule,
  INT_PTR lpszType,
  LONG_PTR  lParam)
{
  _unused lParam;

#if 0
  if (IS_INTRESOURCE(lpszType))
    printf ("type: %d\n", lpszType);
  else
    printf ("type: %s\n", lpszType);
#endif

  EnumResourceNamesA (hModule, lpszType, EnumResNames, 0);
  return TRUE;
}

int main (string[] arg)
{
  char    filename[260+1];
  HMODULE h;

  if (arg'length != 2)
  {
    printf ("extract resources from an EXE file\n");
    printf ("extract <file.exe>\n");
    return -1;
  }

  strcpy (out filename, arg[1]);

  h = LoadLibraryExA (&filename, 
                      0,
                      0x00000002 | 0x00000020);  // LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE
  if (h == 0)
  {
    printf ("error: cannot open %s\n", arg[1]);
    return -1;
  }

  EnumResourceTypesA (h, EnumResType, 0);

  return 0;
}

#end unsafe
