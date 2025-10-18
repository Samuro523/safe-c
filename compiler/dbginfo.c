
// dbginfo : write debug info into file

from std use files, strings;
use error, front/unit;

/*****************************************************************************/

const uint SIZE = 4096;

struct BLOCK
{
  byte[SIZE] data;
  uint       len;
  BLOCK^     next;  
}

BLOCK^ first, last, move;

/*****************************************************************************/

void store (byte[] data)
{
  BLOCK^ n;

  if (last == null || last^.len + data'size > SIZE)
  {
    n = new BLOCK;

    if (first == null)
      first = n;
    else
      last^.next = n;

    last = n;
  }

  last^.data[last^.len : data'size] = data;
  last^.len += data'size;
}

/*****************************************************************************/

public void dbg_header ()
{
  uint4 data[2];

  clear data;
  data[0] = 0xF2761287;
  data[1] = 0xD5016423;

  store (data);
}

/*****************************************************************************/

public void dbg_new_unit (int unit)
{
  uint4 data[2];

  clear data;
  data[0] = 0;
  data[1] = (uint)unit;
  store (data);

  move = last;
}

/*****************************************************************************/

public void dbg_store_line (int line, int ip)
{
  uint4 data[2];
  int   nip = ip;
  
  if (nip == 0)  // ip == 0 is not ok, so set it to 1 (will be fixed below)
    nip = 1;

  clear data;
  data[0] = (uint)nip;
  data[1] = (uint)line;

  store (data);
}

/*****************************************************************************/

public void move_dbg_lines (int start_address, int rip_offset)
{
  BLOCK^ p;
  int    ofs;

  p = move;            // more or less last unit compiled
  while (p != null)
  {
    ofs = (int)p^.len;

    for (;;)
    {
      uint4 data[2];

      ofs -= 8;         // move all pairs (ip, line)
      if (ofs < 0)
        break;

      data'byte = p^.data[ofs:8];

      if (data[0] != 0)
      {
        if (data[0] < (uint)start_address)
          break;

        data[0] += (uint)rip_offset;

        p^.data[ofs:8] = data'byte;
      }
    }

    p = p^.next;
  }
}

/*****************************************************************************/

// used for android only, at the end

public void move_all_ip (int rip_offset)
{
  BLOCK^ p;

// (0xF2761287, 0xD5016423 : skip header
// (0,  0x80000000) -> end of couple list
// (0,  unit_nr)    -> new unit nr
// (ip, line)       -> couples ordered by ip
// (ip, 0)          -> means end of function

  p = first;
  while (p != null)
  {
    uint len = p^.len;
    uint i;
    
    for (i=0; i<len; i+=8)
    {
      uint4 data[2];
      data'byte = p^.data[i:8];
       
      if (data[1] == 0x80000000)
        return;
        
      if (data[0] != 0 && data[0] != 0xF2761287)
      {
        if (data[0] == 1)  // IP = 1 little arm fix  : set it to 0
          data[0] = 0;

        data[0] += (uint)rip_offset;

        p^.data[i:8] = data'byte;
      }
    }
    
    p = p^.next;
  }
}

/*****************************************************************************/

public void dbg_store_unit_name (int unit, string library, string source)
{
  int len;

  store (unit);

  len = strlen(library);
  store (len);
  store (library[0:len]);

  len = strlen(source);
  store (len);
  store (source[0:len]);
}

/*****************************************************************************/

public void dbg_save (string debug_filename)
{
  int    fd;
  BLOCK^ p;
  uint   mark = 0x80000000;

  dbg_new_unit ((int)mark);  // signals end of list
  list_units_for_debug ();   // store all unit names via callback dbg_store_unit_name()
  dbg_new_unit ((int)mark);  // signals end of list

  fd = create (debug_filename);
  if (fd < 0)
  {
    fatal_compiler_error0 ("cannot save debug file on disk");
    return;
  }

  p = first;
  while (p != null)
  {
    if (write (fd, p^.data[0:p^.len]) != (int)p^.len)
    {
      close (fd);
      delete_file (debug_filename);
      fatal_compiler_error0 ("cannot write debug file to disk");
      return;
    }

    p = p^.next;
  }

  if (close (fd) < 0)
  {
    delete_file (debug_filename);
    fatal_compiler_error0 ("cannot close debug file");
    return;
  }
}

/*****************************************************************************/
