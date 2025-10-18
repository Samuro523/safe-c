
// exception.c

#if WINDOWS

// exception.c

use calendar, thread, files, strings, system, win/windows;

bool _gf_create_crash_report_file;
bool _gf_display_fatal_error_message_box;

#begin unsafe

//----------------------------------------------------------------------------------------

const string CRASH_REPORT = "CRASH-REPORT.TXT";

uint g_var;

struct EXCEPTION_TYPE
{
  uint   ExceptionCode;
  string msg;
}

const EXCEPTION_TYPE exception_type[] =
  {{0x80000001, "GUARD_PAGE_VIOLATION"},
   {0x80000002, "DATATYPE_MISALIGNMENT"},
   {0xC0000005, "ACCESS_VIOLATION"},
   {0xC0000017, "OUT_OF_HEAP_MEMORY"},
   {0xC000001D, "ILLEGAL_INSTRUCTION"},
   {0xC0000094, "DIVIDE_BY_ZERO"},
   {0xC0000096, "PRIV_INSTRUCTION"},
   {0xC00000FD, "STACK_OVERFLOW"}};

//----------------------------------------------------------------------------------------

struct RIP_REPEAT_INFO
{
  int     count, index, repeat;
  INT_PTR history[128];
}

//----------------------------------------------------------------------------------------

package Read_DBG

  packed struct Couple
  {
    uint a;
    uint b;
  }

  Couple cpl;
  uint   v;
  char   ch;
  int    fd;

  void r();
  void gv();
  void gch();

end Read_DBG;

//----------------------------------------------------------------------------------------

package body Read_DBG

  public void r()
  {
    if (read (fd, out cpl) != (int)cpl'size)
      cpl.b = 0x80000000;
  }

  public void gv()
  {
    if (read (fd, out v) != (int)v'size)
      v = 0;
  }

  public void gch()
  {
    if (read (fd, out ch) != (int)ch'size)
      ch = nul;
  }

end Read_DBG;

//----------------------------------------------------------------------------------------

void retrieve_location (long ip, out char[32] location, out int line_nr)
{
  char filename[MAX_EXECUTABLE_FILENAME_LENGTH];
  uint len, unit, line, current_unit, i;
  bool found;

  clear location;
  line_nr = 0;

  get_executable_filename (out filename);
  len = (uint)strlen(filename);
  if (len < 4)
    return;

  filename[len-4] = nul;
  strcat (ref filename, ".dbg");

  Read_DBG.fd = open (filename);
  if (Read_DBG.fd < 0)
    return;

  r();

  if (cpl.a != 0xF2761287 || cpl.b != 0xD5016423)   // check header
    return;

  r();
  found = false;
  unit = 0;
  line = 0;
  current_unit = 0;

  while (cpl.b != 0x80000000)
  {
    if (!found)
    {
      if (cpl.a == 0)  // new unit (file)
        current_unit = cpl.b;
      else if (ip >= cpl.a && cpl.b != 0)
      {
        unit = current_unit;
        line = cpl.b;
        r();
        if (ip < cpl.a)   // ip between 2 records
          found = true;
        else
          continue;
      }
    }
    r();
  }

  if (!found)
  {
    close (Read_DBG.fd);
    return;
  }

  clear filename;

  gv();
  while (v != 0)
  {
    current_unit = v;

    gv();  // get length of library
    len = v;

    for (i=0; i<len; i++)
      gch();

    gv();  // get length of source file
    len = v;

    for (i=0; i<len; i++)
    {
      gch();
      if (current_unit == unit && i < (uint)filename'length)
        filename[i] = ch;
    }

    gv();
  }

  close (Read_DBG.fd);

  {
    int length, offset;

    // cut filename
    length = strlen(filename);
    offset = length - location'length;
    if (offset < 0)   // short filename
    {
      offset = 0;
    }
    else   // long filename
    {
      offset = length - location'length;
      length = location'length;
    }

    sprintf (out location, "%s", filename[offset : length]);
  }

  line_nr = (int)line;
}

//----------------------------------------------------------------------------------------

void trace_newline (int fd)
{
  write (fd, "\r\n");
}

//----------------------------------------------------------------------------------------

void trace_register_32 (int fd, char[3] register_name, int value)
{
  char[32]   str;
  char[32+2] str2;

  sprintf (out str, "%s=%8x", register_name, value);
  if (value != 0)
    strcatf (ref str, "   (= %d)", value);
  sprintf (out str2, "  %-32.32s", str);

  write (fd, str2);
}

//----------------------------------------------------------------------------------------

void trace_register (int fd, char[3] register_name, long value)
{
  char[48]   str;
  char[48+4] str2;
  long v = value;

  if (value >= 0 && value <= uint'max)  // seems int4 value zero-extended to int8
    v = (int)v;  // sign-extend

  sprintf (out str, "%s=%16x", register_name, value);
  if (v != 0)
    strcatf (ref str, "   (= %d)", v);
  sprintf (out str2, "    %-48.48s", str);

  write (fd, str2);
}

//----------------------------------------------------------------------------------------

void load_module_name (long ip, out char[32] location)
{
  HANDLE        hModuleSnap;
  MODULEENTRY32 me32;

  clear location;

  hModuleSnap = CreateToolhelp32Snapshot (TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, 0);
  if (hModuleSnap == INVALID_HANDLE_VALUE)
    return;

  me32.dwSize = MODULEENTRY32'size;

  if (Module32First (hModuleSnap, &me32) == 0)
  {
    CloseHandle (hModuleSnap);
    return;
  }

  for (;;)
  {
    INT_PTR base;
    base'byte = me32.modBaseAddr'byte;

    if (ip >= base && ip < base + (long)me32.modBaseSize)
    {
      strncpy (out location, me32.szModule, location'length);
      CloseHandle (hModuleSnap);
      return;
    }

    if (Module32Next (hModuleSnap, &me32) == 0)
      break;
  }

  CloseHandle (hModuleSnap);
}

//----------------------------------------------------------------------------------------

void list_all_DLLs (int fd)
{
  HANDLE        hModuleSnap;
  MODULEENTRY32 me32;

  hModuleSnap = CreateToolhelp32Snapshot (TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, 0);
  if (hModuleSnap == INVALID_HANDLE_VALUE)
    return;

  me32.dwSize = MODULEENTRY32'size;

  if (Module32First (hModuleSnap, &me32) == 0)
  {
    CloseHandle (hModuleSnap);
    return;
  }

  trace_newline (fd);
  write (fd, "Memory modules:\r\n");

  for (;;)
  {
    char str[512];
    INT_PTR base;
    base'byte = me32.modBaseAddr'byte;

#if MEM32
    sprintf (out str, "  %8x to %8x %s\r\n", base, base + (long)me32.modBaseSize, me32.szModule);
#else
    sprintf (out str, "%16x to %16x %s\r\n", base, base + me32.modBaseSize, me32.szModule);
#endif

    write (fd, str[0:strlen(str)]);

    if (Module32Next (hModuleSnap, &me32) == 0)
      break;
  }

  CloseHandle (hModuleSnap);
}

//----------------------------------------------------------------------------------------

bool in_main_module (INT_PTR address)
{
  byte*   data_ptr = (byte*)&_gf_create_crash_report_file;  // address of some global
  INT_PTR last;

  last'byte = data_ptr'byte;

  return address >= 0x401000 && address < last;
}

//----------------------------------------------------------------------------------------

void compute_address_details (    INT_PTR  address,
                              out char[32] location,
                              out int      line_nr)
{
  if (in_main_module (address))
    retrieve_location (address, out location, out line_nr);
  else
  {
    load_module_name (address, out location);
    line_nr = 0;
  }
}

//----------------------------------------------------------------------------------------

void build_short_address_details (    INT_PTR  address,
                                      char[32] location,
                                      int      line_nr,
                                  out char[64] buffer)
{
  if (line_nr > 0)   // translated by .dbg file
    sprintf (out buffer, "%s:%d", location, line_nr);
  else if (location[0] > nul)  // in DLL
    sprintf (out buffer, "%x (%s)", address, location);
  else   // untranslated
    sprintf (out buffer, "%x", address);
}

//----------------------------------------------------------------------------------------

void build_medium_address_details (    INT_PTR  address,
                                       char[32] location,
                                       int      line_nr,
                                   out char[80] buffer)
{
  if (line_nr > 0)
    sprintf (out buffer, "%x  %s line %d", address, location, line_nr);
  else if (location[0] > nul)
    sprintf (out buffer, "%x (%s)", address, location);
  else
    sprintf (out buffer, "%x", address);
}

//----------------------------------------------------------------------------------------

void build_large_address_details (    INT_PTR  address,
                                      char[32] location,
                                      int      line_nr,
                                  out char[80] buffer)
{
  if (line_nr > 0)
  {
    #if MEM32
      sprintf (out buffer, "  %8x  %s line %d", address, location, line_nr);
    #else
      sprintf (out buffer, "%16x  %s line %d", address, location, line_nr);
    #endif
  }
  else if (location[0] > nul)
  {
    #if MEM32
      sprintf (out buffer, "  %8x  (%s)", address, location);
    #else
      sprintf (out buffer, "%16x  (%s)", address, location);
    #endif
  }
  else
  {
    #if MEM32
      sprintf (out buffer, "  %8x", address);
    #else
      sprintf (out buffer, "%16x", address);
    #endif
  }
}

//----------------------------------------------------------------------------------------

void output_rip (INT_PTR rip, int fd)
{
  char[32] location;
  int      line_nr;
  char[80] buffer;

  compute_address_details (rip, out location, out line_nr);
  build_large_address_details (rip, location, line_nr, out buffer);

  write (fd, buffer[0:strlen(buffer)]);
  trace_newline (fd);
}

//----------------------------------------------------------------------------------------

void terminate_repeating_rip (    int             fd,
                              ref RIP_REPEAT_INFO r)
{
  if (r.repeat > 1)
  {
    char[128] line_buffer;

    if (r.repeat == 2 && r.count == 1)
    {
      output_rip (r.history[0], fd);
    }
    else
    {
      strcpy (out line_buffer, "    (previous ");

      if (r.count == 1)
        strcat (ref line_buffer, "line");
      else
        strcatf (ref line_buffer, "%d lines", r.count);

      strcatf (ref line_buffer, " repeated %d time", r.repeat-1);

      if (r.repeat > 2)
        strcat (ref line_buffer, "s");

      strcat (ref line_buffer, ")");

      write (fd, line_buffer[0:strlen(line_buffer)]);
      trace_newline (fd);
    }
  }

  if (r.index > 0)  // last part was not fully repeated : output the remaining lines
  {
    int k;
    for (k=0; k<r.index; k++)
      output_rip (r.history[k], fd);
  }
}

//----------------------------------------------------------------------------------------

// display rip or record it in repeat structure

void display_or_record_rip (    INT_PTR         rip,
                                int             fd,
                            ref int             callstack_lines_displayed,
                            ref RIP_REPEAT_INFO r)
{
  if (r.repeat == 0)   // no recursive call history active
  {
    int j;

    // check if repeated rip
    for (j=0; j<r.count; j++)
    {
      if (rip == r.history[j])
        break;
    }

    if (j == r.count)   // not found
    {
      // store rip
      if (r.count == r.history'length)
      {
        r.history[0 : r.history'length-1] = r.history[1 : r.history'length-1];
        r.count--;
      }
      r.history[r.count++] = rip;

      output_rip (rip, fd);
    }
    else   // found a repeating rip : go in repeat mode
    {
      r.count -= j;
      r.history [0 : r.count] = r.history [j : r.count];
      r.index = 1;
      r.repeat = 1;

      if (r.count == 1)  // it's 1 line repeat sequence, it was already repeated twice now
      {
        r.index = 0;
        r.repeat = 2;
        callstack_lines_displayed--;
      }
    }
  }
  else  // we are repeating a sequence of recursive calls
  {
    if (rip == r.history [r.index])  // match
    {
      r.index++;

      if (r.index == r.count)  // whole sequence repeated
      {
        r.index = 0;
        r.repeat++;
        callstack_lines_displayed -= r.count;
      }
    }
    else  // sequence does not match anymore : we need to produce some output
    {
      terminate_repeating_rip (fd, ref r);

      // leave repeat mode
      clear r;

      // output the new sequence
      output_rip (rip, fd);

      // store as new initial history
      r.history[r.count++] = rip;
    }
  }
}

//----------------------------------------------------------------------------------------

void handle_the_exception (EXCEPTION_POINTERS e)
{
  // we have a guard page (4K) of stack space available in case of exception STACK OVERFLOW.
  char[MAX_EXECUTABLE_FILENAME_LENGTH] exe_filename;
  char[128]                            str, dialog_box_buffer, line_buffer;

  RIP_REPEAT_INFO          rip_info;  // 1K
  MEMORY_BASIC_INFORMATION mem;

  ref EXCEPTION_RECORD  r = *e.ExceptionRecord;
  ref CONTEXT           c = *e.ContextRecord;

  int              i, callstack_lines_displayed;
  INT_PTR          rbp, rip;
  uint             pu*, u;
  int              fd;
  DATE_TIME        now;
  char             *pstr;
  bool             dialog_full, crash_in_dll;

  // report only the first crash
  if (InterlockedExchange (ref g_var, 1) != 0)
    sleep 86400;  // other threads wait forever


  get_executable_filename (out exe_filename);

  if (_gf_create_crash_report_file)
  {
    char[MAX_EXECUTABLE_FILENAME_LENGTH + 32] crash_report_filename;

    strcpy (out crash_report_filename, exe_filename);
    i = strrchr (crash_report_filename, '\\');
    crash_report_filename[i+1] = nul;
    strcat (ref crash_report_filename, CRASH_REPORT);

    fd = open (crash_report_filename, READ+WRITE, 0);
    if (fd > 0)   // exists
    {
      lseek (fd, 0L, SEEK_END);
    }
    else  // new file
    {
      fd = create (crash_report_filename, WRITE, 0);
    }
  }
  else
  {
    fd = -1;
  }

  get_gmt_datetime (out now);
  sprintf (out str, "crash date   : %02d/%02d/%04d %02d:%02d:%02d (gmt)\r\napplication  : ", now.day, now.month, now.year, now.hour, now.min, now.sec);
  write (fd, str[0:strlen(str)]);

  write (fd, exe_filename[0:strlen(exe_filename)]);

  write (fd, "\r\ncommand line : ");
  pstr = GetCommandLineA();
  write (fd, pstr[0:strlen(pstr[0:260])]);

  rip = 0x400088;
  pu'byte = rip'byte;
  u = *pu;

  now.day = 1;
  now.month = 1;
  now.year = 1970;
  add_days (ref now, (int)(u / 86400));
  u %= 86400;
  now.hour = (tiny)(u / 3600);
  u -= 3600 * (uint)now.hour;
  now.min = (tiny)(u / 60);
  u -= 60 * (uint)now.min;
  now.sec = (tiny)u;
  sprintf (out str, "\r\ncompile date : %02d/%02d/%04d %02d:%02d:%02d\r\n\r\n", now.day, now.month, now.year, now.hour, now.min, now.sec);
  write (fd, str[0:strlen(str)]);

  for (i=0; i<exception_type'length; i++)
  {
    if (exception_type[i].ExceptionCode == r.ExceptionCode)
      break;
  }

  if (i < exception_type'length)
    sprintf (out str, "%s", exception_type[i].msg);
  else
    sprintf (out str, "Ex %08x", r.ExceptionCode);

  strcpy (out line_buffer, str);
  write (fd, line_buffer[0:strlen(line_buffer)]);

  strcat (ref str, " at ");
  strcpy (out dialog_box_buffer, str);

  {
    char[32] location;
    int      line_nr;
    char[64] short_buffer;

    compute_address_details (r.ExceptionAddress, out location, out line_nr);
    build_short_address_details (r.ExceptionAddress, location, line_nr, out short_buffer);

    strcat (ref dialog_box_buffer, short_buffer);

    crash_in_dll = (location[0] != nul) && (line_nr == 0);
  }

  if (r.ExceptionCode == 0xC0000005)  // ACCESS_VIOLATION
  {
    sprintf (out line_buffer, " attempted ");

    switch ((int)r.ExceptionInformation[0])
    {
      case 0:
        strcat (ref line_buffer, "READ");
        break;

      case 1:
        strcat (ref line_buffer, "WRITE");
        break;

      case 8:
        strcat (ref line_buffer, "EXECUTE");
        break;

      default:
        strcat (ref line_buffer, "ACCESS");
        break;
    }

    strcatf (ref line_buffer, " at address %x\r\n", r.ExceptionInformation[1]);
    write (fd, line_buffer[0:strlen(line_buffer)]);
  }
  trace_newline (fd);

  write (fd, "Call stack:\r\n");

  output_rip (r.ExceptionAddress, fd);

  // determine range of valid stack pages
  VirtualQuery (c.Rsp, &mem, mem'size);

  dialog_full = false;
  clear rip_info;

  rbp = c.Rbp;

  callstack_lines_displayed = 0;

  if (crash_in_dll)   // find some intermediate return addresses
  {
    INT_PTR Rsp = c.Rsp;

    if (rbp < mem.BaseAddress || rbp >= mem.BaseAddress + (INT_PTR)mem.RegionSize)
      rbp = mem.BaseAddress + (INT_PTR)mem.RegionSize - (int)(2*Rsp'size);

    #if MEM64
      Rsp = ((Rsp - 8) & -16) | 8;  // align at 16 + 8
    #endif

    {
      const string GUESS = "    --- begin guessed ips ---";
      write (fd, GUESS);
      trace_newline (fd);
    }

    for (; callstack_lines_displayed<1024; callstack_lines_displayed++)
    {
      INT_PTR p*, RetAdr;

      if (Rsp + (int)Rsp'size > mem.BaseAddress + (INT_PTR)mem.RegionSize)
        break;

      if (Rsp >= rbp)
        break;

      p'byte = Rsp'byte;
      RetAdr = p[0] - 2;      // possible return address

      if (in_main_module (RetAdr))
        display_or_record_rip (RetAdr, fd, ref callstack_lines_displayed, ref rip_info);

      #if MEM32
        Rsp += 4;
      #else
        Rsp += 16;
      #endif
    }

    {
      const string GUESS = "    --- end guessed ips ---";
      write (fd, GUESS);
      trace_newline (fd);
    }
  }

  for (; callstack_lines_displayed<1024; callstack_lines_displayed++)
  {
    INT_PTR *p;

    if (rbp < mem.BaseAddress || rbp+(int)(2*rbp'size) > mem.BaseAddress + (INT_PTR)mem.RegionSize)
      break;

    p'byte = rbp'byte;

    rbp = p[0];      // load old rbp
    rip = p[1] - 2;  // load return address,  -2 because we don't want the IP after the call

    if (!dialog_full)
    {
      char[32] location;
      int      line_nr;
      char[64] short_buffer;

      compute_address_details (rip, out location, out line_nr);
      build_short_address_details (rip, location, line_nr, out short_buffer);

      if (strlen(dialog_box_buffer) + 3 + strlen(short_buffer) < dialog_box_buffer'length)
      {
        strcat (ref dialog_box_buffer, ", ");
        strcat (ref dialog_box_buffer, short_buffer);
      }
      else
      {
        dialog_full = true;
      }
    }

    display_or_record_rip (rip, fd, ref callstack_lines_displayed, ref rip_info);
  }

  terminate_repeating_rip (fd, ref rip_info);

  trace_newline (fd);
  write (fd, "Registers:\r\n");

#if MEM32
  trace_register_32 (fd, "EAX", c.Rax);
  trace_register_32 (fd, "RSI", c.Rsi);
  trace_newline (fd);

  trace_register_32 (fd, "EBX", c.Rbx);
  trace_register_32 (fd, "RDI", c.Rdi);
  trace_newline (fd);

  trace_register_32 (fd, "ECX", c.Rcx);
  trace_register_32 (fd, "RBP", c.Rbp);
  trace_newline (fd);

  trace_register_32 (fd, "EDX", c.Rdx);
  trace_register_32 (fd, "RSP", c.Rsp);
  trace_newline (fd);
#else
  trace_register (fd, "RAX", c.Rax);
  trace_register (fd, "R8 ", c.R8);
  trace_newline (fd);

  trace_register (fd, "RBX", c.Rbx);
  trace_register (fd, "R9 ", c.R9);
  trace_newline (fd);

  trace_register (fd, "RCX", c.Rcx);
  trace_register (fd, "R10", c.R10);
  trace_newline (fd);

  trace_register (fd, "RDX", c.Rdx);
  trace_register (fd, "R11", c.R11);
  trace_newline (fd);

  trace_register (fd, "RSI", c.Rsi);
  trace_register (fd, "R12", c.R12);
  trace_newline (fd);

  trace_register (fd, "RDI", c.Rdi);
  trace_register (fd, "R13", c.R13);
  trace_newline (fd);

  trace_register (fd, "RBP", c.Rbp);
  trace_register (fd, "R14", c.R14);
  trace_newline (fd);

  trace_register (fd, "RSP", c.Rsp);
  trace_register (fd, "R15", c.R15);
  trace_newline (fd);
#endif

  if (_gf_create_crash_report_file)
  {
    flush (fd);
    list_all_DLLs (fd);
  }

  {
    clear str;
    i = 100;
    str[0:i] = {all=>'-'};
    write (fd, str[0:100]);
    write (fd, "\r\n");
  }

  close (fd);

  if (_gf_display_fatal_error_message_box)
    FatalAppExitA (0, &dialog_box_buffer);     // !! does not support a string longer than 260 !!
  else
    ExitProcess (0xFFFFFFFF);
}

//----------------------------------------------------------------------------------------

[callback]
LONG pvectored_exception_handler (EXCEPTION_POINTERS e)
{
  ref EXCEPTION_RECORD r = *e.ExceptionRecord;
  int                  i;

  for (i=0; i<exception_type'length; i++)
  {
    if (exception_type[i].ExceptionCode == r.ExceptionCode)
      handle_the_exception (e);
  }

  // let this exception travel up the chain, otherwise APIs like GetOpenFileNameA() fail.
  return 0;  // EXCEPTION_CONTINUE_SEARCH (0x0)
}

//----------------------------------------------------------------------------------------

public void arm_exception_handler (bool create_crash_report_file        = true,
                                   bool display_fatal_error_message_box = true)
{
  _gf_create_crash_report_file        = create_crash_report_file;
  _gf_display_fatal_error_message_box = display_fatal_error_message_box;

  AddVectoredExceptionHandler (0, pvectored_exception_handler);  // 0 = last handler to be called
}

//----------------------------------------------------------------------------------------

public void log_in_crash_report (string format, object[] arg)
{
  char[MAX_EXECUTABLE_FILENAME_LENGTH]      exe_filename;
  char[MAX_EXECUTABLE_FILENAME_LENGTH + 32] crash_report_filename;
  int                                       i;
  FILE                                      file;

  get_executable_filename (out exe_filename);
  strcpy (out crash_report_filename, exe_filename);
  i = strrchr (crash_report_filename, '\\');
  crash_report_filename[i+1] = nul;
  strcat (ref crash_report_filename, CRASH_REPORT);

  fappend (out file, crash_report_filename, encoding => ANSI);
  fprintf (ref file, format, arg);
  fclose (ref file);
}

//---------------------------------------------------------------------
#end unsafe

#endif // WINDOWS

#if ANDROID

use arithm, calendar, files, logging, strings, thread;
use android/bionic, android/android;

//--------------------------------------------------------------------------
#begin unsafe
//--------------------------------------------------------------------------

const string CRASH_REPORT = "CRASH-REPORT.TXT";

//--------------------------------------------------------------------------

bool _gf_create_crash_report_file;
int  g_fd;
uint g_var;

//--------------------------------------------------------------------------

struct FRAME
{
  int8 frame_pointer;
  int8 return_address;
}

//---------------------------------------------------------------------

struct SIGNAL_NAME
{
  int code;
  string name;
}

const SIGNAL_NAME[] SIG = {{4, "ILL"}, {7, "BUS"}, {8, "FPE"}, {11, "SEGV"}};

byte*  g_dbg;
int    g_dbg_len;

long   g_my_base;

//---------------------------------------------------------------------

void wr (int fd, string s)
{
  int len = strlen(s);

  log_fatal_error ("%s", s[0:len]);

  if (_gf_create_crash_report_file)
  {
    files.write (fd, s[0:len]);
    files.write (fd, "\n");
  }
}

//--------------------------------------------------------------------------

bool is_memory_valid (long ptr, int length)
{
  int  bits = ilog2 ((int)sysconf (_SC_PAGESIZE));
  long m1, m2, m;

  m1 = (ptr >> bits) << bits;
  m2 = ((ptr+length-1) >> bits) << bits;

  for (m=m1; m<=m2; m++)
  {
    if (msync (m, 1024, MS_ASYNC) != 0)
      return false;
  }

  return true;
}

//---------------------------------------------------------------------

/*
void wr_block (int fd, byte[] block)
{
  uint  offset, i;
  byte  ch;
  char  line[80], item[32];
  byte* ptr;
  long  lptr;

  ptr = &block;
  lptr'byte = ptr'byte;

  for (offset=0; offset<(uint)block'length; offset+=16)
  {
    sprintf (out line, "%06x |", (offset & 0xFFFFFF));  // 8 chars

    if (is_memory_valid (ptr => lptr + offset, length => 16))
    {
      for (i=0; i<16; i++)        // 16 x 3 = 48 chars
      {
        if (offset + i < (uint)block'length)
          sprintf (out item, " %02x", block[offset+i]);
        else
          strcpy (out item, "   ");
        strcat (ref line, item);
      }

      strcat (ref line, " | ");    // 3 chars

      for (i=0; i<16; i++)       // 16 chars
      {
        if (offset + i < (uint)block'length)
        {
          ch = block[offset+i];
          if (ch >= 32 && ch != 127)
            sprintf (out item, "%c", (char)ch);
          else
            strcpy (out item, ".");
          strcat (ref line, item);
        }
        else
        {
          strcat (ref line, " ");
        }
      }
    }

    wr (fd, line);
  }
}
*/

//----------------------------------------------------------------------------

void load_debug_info ()
{
  byte*   p;
  long    addr;
  Dl_info info;
  char    dbg_filename[512];
  int     pos, pos2;

  p = (byte *)&g_dbg;
  addr'byte = p'byte;

  if (dladdr (addr, out info) == 0)   // address not inside any module
    return;

  sprintf (out dbg_filename, "%s", info.dli_fname[0:512]);

  // replace extension .so by .dbg
  pos = strrchr (dbg_filename, '.');
  if (pos >= 1)
    dbg_filename[pos:5] = ".dbg\0";

  // keep only last name
  pos2 = strrchr (dbg_filename, '/');
  if (pos2 >= 1)
  {
    int len = (pos+5) - (pos2+1);
    dbg_filename[0 : len] = dbg_filename[pos2+1 : len];
  }

  g_dbg = android_load_asset2 (dbg_filename, out g_dbg_len);

  {
    const uint[2] HEADER = {0xF2761287, 0xD5016423};

    if (g_dbg == null || g_dbg_len < 16 || memcmp (g_dbg[0:HEADER'size], HEADER) != 0)
    {
      freem(g_dbg);
      g_dbg = null;
    }
  }
}

//---------------------------------------------------------------------

bool get_addr_info (long addr, out long base, out string object_name)
{
  Dl_info info;

  if (dladdr (addr, out info) == 0)
  {
    clear base, object_name;
    return false;
  }
  else
  {
    base = info.dli_fbase;

    if (base == g_my_base && g_dbg != null)  // search in debug info
    {
      long ip = addr - base;
      struct INFO {uint ip; uint line; }
      ref INFO[] dbg = ((INFO*)g_dbg) [0 : (uint)g_dbg_len / INFO'size];
      bool found = false;
      uint current_unit = 0;
      int ofs = 1;  // skip header
      uint unit = 0;
      uint line = 0;
      uint len;

      // record: <a4>  <b4>
      //   (0,  0x80000000) -> end of couple list
      //   (0,  unit_nr)    -> new unit nr
      //   (ip, line)       -> couples ordered by ip

      while (dbg[ofs].line != 0x80000000 && ofs+1 < dbg'length)
      {
        if (dbg[ofs].ip == 0)
          current_unit = dbg[ofs].line;
        else if (ip >= dbg[ofs].ip && ip < dbg[ofs+1].ip && dbg[ofs].line != 0)
        {
          found = true;
          unit = current_unit;
          line = dbg[ofs].line;
        }

        ofs++;
      }

      if (!found)
      {
        strcpy (out object_name, "<not found>");
      }
      else
      {
        uint* l = (uint*)&dbg[ofs+1];   // skip (0, 0x80000000)
        string(200) library;
        string(300) filename;

        clear library, filename, object_name;

        while (((byte*)l) + 4 <= g_dbg + g_dbg_len && *l != 0)
        {
          // <unit_nr4>  <len4_library_name ; library_name>   <len4_source_filename ; source_filename>
          // ends with 2 longs : (0, 0x80000000)

          found = (*l == unit);
          l++;

          len = *l & 1023;
          l++;
          if (found)
            sprintf (out library, "%.199s", ((char*)l)[0:len]);
          l = (uint*)(((char*)l) + len);

          len = *l & 1023;
          l++;
          if (found)
            sprintf (out filename, "%.299s", ((char*)l)[0:len]);
          l = (uint*)(((char*)l) + len);
        }

        if (library[0] != nul)
          sprintf (out object_name, "[%s] ", library);
        strcatf (ref object_name, "%s line %u", filename, line);
      }
    }
    else  // display only module name
    {
      sprintf (out object_name, "%s", info.dli_fname[0:object_name'length]);
    }

    return true;
  }
}

//---------------------------------------------------------------------

void log_address_info (int fd, long addr)
{
  long         base;
  string (512) object_name;
  string (550) str;

  if (get_addr_info (addr, out base, out object_name))
  {
    sprintf (out str, "> %06x %s", addr - base, object_name);
  }
  else
  {
    sprintf (out str, "> %016x", addr);
  }

  wr (fd, str);
}

//---------------------------------------------------------------------

[callback]
int list_modules (dl_phdr_info info, long size, byte *data)
{
  char line[512];
  _unused size, data;
  sprintf (out line, "%016x : %s", info.dlpi_addr, info.dlpi_name[0:400]);
  wr (g_fd, line);
  return 0;
}

//---------------------------------------------------------------------

void log_separator (int fd, char sep)
{
  char buf[140];
  int i;
  clear buf;
  for (i=0; i<buf'length; i++)
    buf[i] = sep;
  wr (fd, buf);
}

//---------------------------------------------------------------------

void wr_heap_block (int fd, long addr, long ptr[4096/8])
{
  char buffer[80];
  int  i;
  long lk, ad;
  bool chain[4096/16];
  bool error[4096/16];

  sprintf (out buffer, "  next: %x", ptr[0]);
  wr (fd, buffer);

  sprintf (out buffer, "  free: %x", ptr[1]);
  wr (fd, buffer);

  clear error;
  
  lk = ptr[1];
  if ((lk & -4096) != addr || (lk & 15) != 0 || (lk & 4095) < 16)
    wr (fd, "error: bad free ptr");
  else
  {
    clear chain;
    ad = 0;
    while (lk != 0)
    {
      if ((lk & -4096) != addr || (lk & 15) != 0 || (lk & 4095) < 16)
      {
        error[(int)ad/2] = true;
        break;
      }

      lk = (lk - addr) >> 3;

      if (chain[(int)lk/2])   // chain cycle
      {
        error[(int)lk/2] = true;
        break;
      }

      chain[(int)lk/2] = true;

      if (ptr[(int)lk] != 0)      // count or type non-zero
        error[(int)lk/2] = true;

      ad = lk;
      lk = ptr[(int)lk+1];
    }
  }

  for (i=2; i<4096/8; i+=2)
  {
    sprintf (out buffer, "  %x : count=%3d type=%3d mem=%x",
                 addr + i*8,
                 ptr[i] & (long)uint'max,
                 (ptr[i] >> 32) & (long)uint'max,
                 ptr[i+1]);
    wr (fd, buffer);
    
    if (error[i/2])
      wr (fd, "error");
  }
}

//---------------------------------------------------------------------

void log_heap (int fd)
{
  long* ptr;
  byte* pt;
  long  bss_addr, addr, tail, link;
  char  buffer[80];
  byte[8] zero = {all => 0};

  bss_addr = g_my_base + 64 + 56*4 + 16;
  ptr'byte = bss_addr'byte;
  bss_addr = g_my_base + *ptr;       // load virtual address of bss
  pt'byte = bss_addr'byte;


  addr'byte = pt[16:8];
  sprintf (out buffer, "tombstone head = %x\n", addr);
  wr (fd, buffer);

  tail'byte = pt[24:8];
  sprintf (out buffer, "tombstone tail = %x\n", tail);
  wr (fd, buffer);

  ptr'byte = pt[16:8];   // head
  while (memcmp (ptr, zero) != 0)
  {
    if (!is_memory_valid (ptr => addr, length => 4096))
    {
      wr (fd, "error: bad address");
      break;
    }

    addr'byte = ptr'byte;
    sprintf (out buffer, "4K-block at %x\n", addr);
    wr (fd, buffer);

    link = ptr[0];
    if (link == 0 && memcmp (addr, tail) != 0)
    {
      sprintf (out buffer, "error: tombstone tail does not match");
      wr (fd, buffer);
    }

    wr_heap_block (fd, addr, ptr[0:4096/8]);

    ptr'byte = link'byte;
  }
}

//---------------------------------------------------------------------

[callback]
void handler (int sig, siginfo si, ucontext context)
{
  // we have a guard page (4K) of stack space available in case of exception STACK OVERFLOW.
  DATE_TIME now;
  char      name[128];
  char      line[256];
  FRAME     *pframe;
  int8      fp;
  int       i, fd;

  // report only the first crash
  if (InterlockedExchange (ref g_var, 1) != 0)
    sleep 86400;  // other threads wait forever

  if (_gf_create_crash_report_file)
  {
    fd = files.open (CRASH_REPORT, WRITE, 0);
    if (fd > 0)   // exists
    {
      files.lseek (fd, 0L, SEEK_END);
    }
    else  // new file
    {
      fd = files.create (CRASH_REPORT, WRITE, 0);
    }
  }
  else
  {
    fd = -1;
  }

  log_separator (fd, '=');
  get_gmt_datetime (out now);
  sprintf (out line, "crash date  : %02d/%02d/%04d %02d:%02d:%02d (gmt)", now.day, now.month, now.year, now.hour, now.min, now.sec);
  wr (fd, line);

  {
    byte*     p;
    long      addr, t;
    Dl_info   info;
    DATE_TIME dt;

    p = (byte*)&g_dbg;
    addr'byte = p'byte;

    if (dladdr (addr, out info) != 0)
    {
      g_my_base = info.dli_fbase;

      p'byte = g_my_base'byte;
      t'byte = p[0x238+56:8];     // fix here if 0x238 changes in elf.c
      clock_to_datetime (t, 0, out dt);
      sprintf (out line, "compile date: %02d/%02d/%04d %02d:%02d:%02d (gmt)", dt.day, dt.month, dt.year, dt.hour, dt.min, dt.sec);
      wr (fd, line);

      sprintf (out line, "shared lib  : %s", info.dli_fname[0:512]);
      wr (fd, line);
    }
  }

  if (android.g_app != null)
  {
    sprintf (out line, "storage dir : %s", android.g_app->activity->internalDataPath[0:512]);
    wr (fd, line);
    sprintf (out line, "disk space  : %d GB left", free_disk_space(".") >> 30);
    wr (fd, line);
  }

  for (i=0; i<SIG'length; i++)
  {
    if (SIG[i].code == sig)
      break;
  }
  if (i<SIG'length)
  {
    sprintf (out name, "SIG%s (%d)", SIG[i].name, sig);
  }
  else
  {
    sprintf (out name, "%d", sig);
  }

  sprintf (out line, "fatal error : signal %s  signo %d  code %d", name, si.si_signo, si.si_code);
  wr (fd, line);

  load_debug_info ();

  sprintf (out line, "crash at PC : %016x", context.uc_mcontext.pc);

  if (sig != SIGILL)
    strcatf (ref line, "  illegal memory access at %x", context.uc_mcontext.fault_address);

  wr (fd, line);
  log_separator (fd, '-');

  wr (fd, "Stack Trace");

  log_address_info (fd, context.uc_mcontext.pc);

  fp = context.uc_mcontext.regs[29];   // points to last frame

  for (i=0; i<256; i++)
  {
    if (!is_memory_valid (fp, 16))
      break;

    pframe'byte = fp'byte;

    if (pframe->return_address == 0)
      break;

    log_address_info (fd, pframe->return_address - 2);  // -2 because we don't want the IP after the call

    fp = pframe->frame_pointer;
  }

  log_separator (fd, '-');
  wr (fd, "Stack Trace 2");

  fp = context.uc_mcontext.sp;   // points to sp
  for (i=0; i<1024; i++)
  {
    if (!is_memory_valid (fp, 16))
      break;

    pframe'byte = fp'byte;

    if (pframe->return_address >= g_my_base && pframe->return_address < g_my_base + 16*1024*1024)
    {
      for (i=0; i<256; i++)
      {
        if (!is_memory_valid (fp, 16))
          break;

        pframe'byte = fp'byte;

        if (pframe->return_address == 0)
          break;

        log_address_info (fd, pframe->return_address - 2);  // -2 because we don't want the IP after the call

        fp = pframe->frame_pointer;
      }
      break;
    }
    fp += 16;
  }

  log_separator (fd, '-');

  wr (fd, "Registers");

  for (i=0; i<27; i+=4)
  {
    sprintf (out line, "X%-2d = %016x  X%-2d = %016x  X%-2d = %016x  X%-2d = %016x",
               i, context.uc_mcontext.regs[i],
               i+1, context.uc_mcontext.regs[i+1],
               i+2, context.uc_mcontext.regs[i+2],
               i+3, context.uc_mcontext.regs[i+3]);
    wr (fd, line);
  }

  sprintf (out line, "X%d = %016x  X%d = %016x  X%d = %016x  SP  = %016x",
             i, context.uc_mcontext.regs[i],
             i+1, context.uc_mcontext.regs[i+1],
             i+2, context.uc_mcontext.regs[i+2],
             context.uc_mcontext.sp);
  wr (fd, line);

  if (_gf_create_crash_report_file)
    files.flush (fd);

#if 1
  log_separator (fd, '-');
  wr (fd, "Loaded Objects");
  g_fd = fd;
  dl_iterate_phdr (list_modules, null);
#endif

#if 0
  log_separator (fd, '-');
  wr (fd, "Heap");
  log_heap (fd);
#endif

  log_separator (fd, '=');
  if (_gf_create_crash_report_file)
    files.close (fd);

  // restore default action for this signal, so pass the signal to android
  {
    sigaction_t sa;
    clear sa;
    sa.sa_flags = SA_SIGINFO;
    sigaction (signum => sig, act => sa);
  }
}

//---------------------------------------------------------------------

// enables exception handler that shows the error location after a crash.

public
void arm_exception_handler (bool create_crash_report_file        = true,
                            bool display_fatal_error_message_box = true)
{
  sigaction_t sa;
  int         i;

  _gf_create_crash_report_file = create_crash_report_file;
  _unused display_fatal_error_message_box;

  clear sa;
  sa.sa_flags     = SA_SIGINFO;
  sa.sa_sigaction = handler;
  sa.sa_mask      = {0};

  for (i=0; i<SIG'length; i++)
    sigaction (signum => SIG[i].code,  act => sa);
}

//---------------------------------------------------------------------

public
void log_in_crash_report (string format, object[] arg)
{
  FILE file;

  fappend (out file, CRASH_REPORT, encoding => ANSI);
  fprintf (ref file, format, arg);
  fclose (ref file);
}

//--------------------------------------------------------------------------
#end unsafe
//--------------------------------------------------------------------------

#endif // ANDROID
