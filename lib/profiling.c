
// profiling.c

use console, files, system, strings, win/windows;

//----------------------------------------------------------------------

struct IP_COUPLE
{
  uint    ip;
  uint    line_nr;
  uint    unit_nr;
  uint    index;
  long    count;      // incremented when profiled
}

IP_COUPLE[]^ g_ip_couple;

long g_largest;

//----------------------------------------------------------------------

struct UNIT_COUPLE
{
  uint    unit_nr;
  string^ library;
  string^ source_filename;
}

UNIT_COUPLE[]^ g_unit_couple;

//----------------------------------------------------------------------

void load_debug_info ()
{
  wstring(MAX_EXECUTABLE_FILENAME_LENGTH)  name;
  int     fd, count1, count2;
  long    size;
  byte[]^ p;
  uint    current_unit_nr, i, j;
  
  wget_executable_filename (out name);
  name[wstrlen(name)-3] = Lnul;
  wstrcat (ref name, L"dbg");

  fd = wopen (name);
  if (fd < 0)
    return;

  size = filesize (fd);
  p = new byte[(int)size];
  assert read (fd, out p^) == (int)size;
  close (fd);

  i = 8;
  count1 = 0;
  
  for (;;)  
  {
    uint a, b;
    
    a'byte = p^[i:4];
    i += 4;
    b'byte = p^[i:4];
    i += 4;
    
    if (b == 0x80000000)
      break;

    if (a != 0)
      count1++;
  }

  g_ip_couple = new IP_COUPLE [count1];
  
  i = 8;
  count1 = 0;
  current_unit_nr = 0;
  
  for (;;)  
  {
    uint a, b;
    
    a'byte = p^[i:4];
    i += 4;
    b'byte = p^[i:4];
    i += 4;
    
    if (b == 0x80000000)
      break;

    if (a != 0)
      g_ip_couple^[count1++] = {a, b, current_unit_nr, 0, 0};
    else
      current_unit_nr = b;
  }
  
  j = i;
  count2 = 0;

  for (;;)  
  {
    uint b, c;
    
    i += 4;
    b'byte = p^[i:4];
    i += 4;
    
    if (b == 0x80000000)
      break;

    i += b;
    
    c'byte = p^[i:4];
    i += 4;

    i += c;
    count2++;
  }
  
  g_unit_couple = new UNIT_COUPLE [count2];
  
  i = j;
  count2 = 0;
  
  for (;;)  
  {
    uint a, b, c;
    
    a'byte = p^[i:4];
    i += 4;
    b'byte = p^[i:4];
    i += 4;
    
    if (b == 0x80000000)
      break;

    g_unit_couple^[count2].unit_nr = a;
    
    g_unit_couple^[count2].library = new string(b);
    g_unit_couple^[count2].library^'byte = p^[i:b];
    i += b;
    
    c'byte = p^[i:4];
    i += 4;

    g_unit_couple^[count2].source_filename = new string(c);
    g_unit_couple^[count2].source_filename^'byte = p^[i:c];
    i += c;
    
    count2++;
  }

  free p;
  
  {
    uint unit_nr = uint'max;
    uint index   = 0;
    
    for (i=0; i<(uint)g_ip_couple^'length; i++)
    {
      if (g_ip_couple^[i].unit_nr != unit_nr)
      {
        unit_nr = g_ip_couple^[i].unit_nr;
        for (index=0; index<(uint)g_unit_couple^'length; index++)
        {
          if (g_unit_couple^[index].unit_nr == unit_nr)
            break;
        }
      }
      
      g_ip_couple^[i].index = index;
    }
  }
}

//----------------------------------------------------------------------

void half_counts ()
{
  int i;
  for (i=0; i<g_ip_couple^'length; i++)
    g_ip_couple^[i].count >>= 1;
}

//----------------------------------------------------------------------

void display_line (IP_COUPLE c)
{
  printf ("[%s] %s line %u\n", 
          g_unit_couple^[c.index].library^,
          g_unit_couple^[c.index].source_filename^,
          c.line_nr);
}

//----------------------------------------------------------------------

void increment_ip (long ip)
{
  int a, b, mid;
  
  a = 0;                       // included
  b = g_ip_couple^'length;     // excluded
  
  if (ip < g_ip_couple^[a].ip || ip >= g_ip_couple^[b-1].ip)
    return;

  while (b - a > 1)
  {
    mid = a + ((b - a) >> 1);
    if (ip < g_ip_couple^[mid].ip)
      b = mid;
    else
      a = mid;
  }
  
  {
    ref IP_COUPLE c = g_ip_couple^[a];
    long count = c.count + 1;

    c.count = count;
        
    if (count > g_largest)
    {
      g_largest = count;
      display_line (c);
      
      if (g_largest > 10)
      {
        half_counts ();
        g_largest >>= 1;
      }
    }
  }
}

//----------------------------------------------------------------------

#begin unsafe

void profiling_thread ()
{
  HANDLE        hModuleSnap;
  THREADENTRY32 te32;
  DWORD         pid = GetCurrentProcessId();
  
  for (;;)
  {
    hModuleSnap = CreateToolhelp32Snapshot (TH32CS_SNAPTHREAD, 0);
    if (hModuleSnap == INVALID_HANDLE_VALUE)
      return;

    te32.dwSize = THREADENTRY32'size;

    if (Thread32First (hModuleSnap, &te32) == 0)
    {
      assert CloseHandle (hModuleSnap) != 0;
      return;
    }

    for (;;)
    {
      if (te32.th32OwnerProcessID == pid && te32.th32ThreadID != GetCurrentThreadId())
      {
        HANDLE thandle = OpenThread (0x000F0000 | 0x00100000 | 0xFFFF, FALSE, te32.th32ThreadID);
        if (thandle != 0)
        {
          CONTEXT context;
          assert SuspendThread (thandle) != -1;

          clear context;
          context.ContextFlags = 0x100000 | 1;

          assert GetThreadContext (thandle, &context) != 0;
          assert ResumeThread (thandle) != -1;
          assert CloseHandle (thandle) != 0;

          increment_ip (context.Rip);
        }
      }

      if (Thread32Next (hModuleSnap, &te32) == 0)
        break;
    }

    assert CloseHandle (hModuleSnap) != 0;
    sleep 0;
  }
}

#end unsafe

//----------------------------------------------------------------------

public void start_profiling ()
{
  int rc;
  
  load_debug_info ();
  if (g_ip_couple == null)
    return;

  rc = run profiling_thread ();
  _unused rc;
}

//----------------------------------------------------------------------
