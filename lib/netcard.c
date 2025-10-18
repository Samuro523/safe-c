
// netcard.c

use win/windows;

/****************************************************************************/
#begin unsafe
/****************************************************************************/

// list must be freed by caller after use.

public void get_list_of_network_cards (out NETCARD[]^ list)
{
  const string dll_name = "iphlpapi.dll\0";
  const string proc_name = "GetIfTable\0";
  HMODULE      hinstLib;
  byte[]^      table;
  PVOID        func;
  GETIFTABLE   GetIfTable;
  uint         size, count;

  list = null;

  hinstLib = LoadLibraryA (&dll_name);
  if (hinstLib == 0)
    return;

  func = GetProcAddress (hinstLib, &proc_name);
  GetIfTable = *(GETIFTABLE*)&func;
  if (GetIfTable == null)
  {
    FreeLibrary (hinstLib);
    return;
  }

  size = 0;

  if (GetIfTable (null, &size, 0) != 122 ||   // ERROR_INSUFFICIENT_BUFFER
      size < 4)
  {
    FreeLibrary (hinstLib);
    return;
  }

  table = new byte[size];

  if (GetIfTable (&table^[0], &size, 1) != 0)
  {
    free table;
    FreeLibrary (hinstLib);
    return;
  }

  count'byte = table^[0:4];      // first 4 bytes contains nb of entries.

  if (count*NETCARD'size > size-4)   // invalid count (buffer is too small)
  {
    free table;
    FreeLibrary (hinstLib);
    return;
  }

  list = new NETCARD [count];

  list^'byte = table^[4 : count*NETCARD'size];

  free table;

  FreeLibrary (hinstLib);
}

/****************************************************************************/
#end unsafe
/****************************************************************************/
