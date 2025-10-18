
// elf.c

use ../strings;
use bionic;

//---------------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------------

byte g_global;

//---------------------------------------------------------------------

public
bool load_ressource (int name, int typ, out byte* ptr, out uint size)
{
  char[16]  namestr, typstr;
  byte*     p;
  long      addr;
  Dl_info   info;
  int4      offset_resources;
  int4*     pi;
  int4      count, i;

  packed struct ENTRY
  {
    int4 name;
    int4 typ;
    int4 data;
    int4 size;
  }

  ENTRY* pe;

  clear ptr, size;

  p = &g_global;
  addr'byte = p'byte;

  if (dladdr (addr, out info) == 0)
    return false;

  p'byte = info.dli_fbase'byte;

  offset_resources'byte = p[0x238+32:4];     // fix here if 0x238 changes in elf.c

  pi = (int4*)(p + offset_resources);

  count = pi[0];

  pe = (ENTRY*)(&pi[1]);

  sprintf (out namestr, "%d", name);
  sprintf (out typstr, "%d", typ);

  for (i=0; i<count; i++)   // could be improved using binary search instead of looping on all
  {
    ref ENTRY e = pe[i];

    if (strcmp (((char*)&e)[e.name:16], namestr) == 0 && strcmp (((char*)&e)[e.typ:16], typstr) == 0)
    {
      ptr = (byte*)&e + e.data;
      size = (uint)e.size;

      return true;
    }
  }

  return false;
}

//---------------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------------
