
// pe.c : generate PE structure

from std use bintree, calendar, console, crc, files, strings, tracing;
use ../error, ../goptions, ../pool, ../dbginfo, ../exeout;

/************************************************************************/

const uint PAGE    =  0x1000;       // 4K
const uint HEADER  =  PAGE;         // mz_header + pe_header + fillers

/************************************************************************/

packed struct IMAGE_DATA_DIR
{
  uint4 addr;      // virtual address
  uint4 size;
}

/************************************************************************/

packed struct SECTION  // 40 bytes
{
  char   name[8];
  uint4  size_in_memory;
  uint4  addr;
  uint4  SizeOnDisk;            // multiple of FileAlignment  (0 for .bss section)
  uint4  PointerToDisk;         // ptr into section data in file, relative to optional header (0 for .bss)
  uint4  PointerToRelocations;  // 0
  uint4  PointerToLinenumbers;  // 0
  uint2  NumberOfRelocations;   // 0
  uint2  NumberOfLinenumbers;   // 0
  uint4  Characteristics;       // contains executable code    = 0x00000020,
                                // contains initialized data   = 0x00000040,
                                // contains uninitialized data = 0x00000080,
                                // access rights exec  = 0x20000000,
                                // access rights read  = 0x40000000,
                                // access rights write = 0x80000000.
                                //
                                // .text -> code + exec
                                // .data -> initialized + read
                                // .bss  -> uninitialized + read + write
}

/************************************************************************/

packed struct PE32_PART1
{
  uint4 signature;        // 0x50 0x45 0x00 0x00
  uint2 machineType;      // 0x014C for i386, 0x8664 for x64.
  uint2 nb_sections;      // 3 (text, data, bss) (see at the end of this struct)
  uint4 created_dtf;      // file creation dtf (nb secs since 1970)
  uint4 symbols;          // 0
  uint4 nb_symbols;       // 0
  uint2 opt_header_size;  // 00E0  (= .code - .magic)
  uint2 characteristics;  // 0x0103  (0x0100=32-bit, 0x020 = large addr aware, 0x0002=file valid, 0x0001=no reloc)
}

packed struct PE32_PART2_32BIT
{
  uint2 magic;                   // 0x010B=PE32, 0x20B=PE32+)
  byte  MajorLinkerVersion;      // 0x06(32 bit) or 0x0C or 0x0E
  byte  MinorLinkerVersion;      // 0x00
  uint4 SizeOfCode;              // size of .text section
  uint4 SizeOfInitializedData;   // size of .data section
  uint4 SizeOfUninitializedData; // size of .bss section (often zero)
  uint4 EntryPoint;              // RVA of PC start address
  uint4 BaseOfCode;              // 0x00001000  (.text loaded at ImageBase+BaseOfCode = 0x00401000)
  uint4 BaseOfData;              // 0x00005000  (.data loaded at ImageBase+BaseOfData)

  // optional header - windows-specific fields
  uint4 ImageBase;               // 0x00400000  (MS-DOS header loaded at this address)
  uint4 SectionAlignment;        // 0x00001000
  uint4 FileAlignment;           // 0x00001000
  uint2 MajorOSVersion;          // 4 or 5
  uint2 MinorOSVersion;          // 0 or 2
  uint2 MajorImageVersion;       // 0
  uint2 MinorImageVersion;       // 0
  uint2 MajorSubsystemVersion;   // 4 (Win32 4.0) or 5
  uint2 MinorSubsystemVersion;   // 0 or 2
  uint4 Win32VersionValue;       // 0
  uint4 SizeOfImage;             // size of image to be loaded in memory, incl. headers (multiple of SectionAlignment)
  uint4 SizeOfHeaders;           // 0x1000
  uint4 CheckSum;                // 0
  uint2 Subsystem;               // 2=WIN_GUI, 3=DOS_CONSOLE
  uint2 DllCharacteristics;      // 0
  uint4 SizeOfStackReserve;      // 0x00100000 (1 MB stack)
  uint4 SizeOfStackCommit;       // 0x00001000 (4 KB pour démarrer)
  uint4 SizeOfHeapReserve;       // 0x00100000 (1 MB heap)
  uint4 SizeOfHeapCommit;        // 0x00001000 (4 KB pour démarrer)
  uint4 LoaderFlags;             // 0
  uint4 NumberOfDirectories;     // 0x10 (16)
}

/************************************************************************/

packed struct PE32_PART2_64BIT
{
  uint2 magic;                   // 0x010B=PE32, 0x20B=PE32+)
  byte  MajorLinkerVersion;      // 0x06(32 bit) or 0x0C or 0x0E
  byte  MinorLinkerVersion;      // 0x00
  uint4 SizeOfCode;              // size of .text section
  uint4 SizeOfInitializedData;   // size of .data section
  uint4 SizeOfUninitializedData; // size of .bss section (often zero)
  uint4 EntryPoint;              // RVA of PC start address
  uint4 BaseOfCode;              // 0x00001000  (.text loaded at ImageBase+BaseOfCode = 0x00401000)

  // optional header - windows-specific fields
  int8  ImageBase64;             // 0x0140000000  (MS-DOS header loaded at this address)
  uint4 SectionAlignment;        // 0x00001000
  uint4 FileAlignment;           // 0x00001000
  uint2 MajorOSVersion;          // 4 or 5
  uint2 MinorOSVersion;          // 0 or 2
  uint2 MajorImageVersion;       // 0
  uint2 MinorImageVersion;       // 0
  uint2 MajorSubsystemVersion;   // 4 (Win32 4.0) or 5
  uint2 MinorSubsystemVersion;   // 0 or 2
  uint4 Win32VersionValue;       // 0
  uint4 SizeOfImage;             // size of image to be loaded in memory, incl. headers (multiple of SectionAlignment)
  uint4 SizeOfHeaders;           // 0x1000
  uint4 CheckSum;                // 0
  uint2 Subsystem;               // 2=WIN_GUI, 3=DOS_CONSOLE
  uint2 DllCharacteristics;      // 0
  int8 SizeOfStackReserve;       // 0x00100000 (1 MB stack)
  int8 SizeOfStackCommit;        // 0x00001000 (4 KB pour démarrer)
  int8 SizeOfHeapReserve;        // 0x00100000 (1 MB heap)
  int8 SizeOfHeapCommit;         // 0x00001000 (4 KB pour démarrer)
  uint4 LoaderFlags;             // 0
  uint4 NumberOfDirectories;     // 0x10 (16)
}

/************************************************************************/

packed struct PE32_PART3
{
  // optional header - data directories (16 x 8 bytes = 128 bytes)
  IMAGE_DATA_DIR  export;
  IMAGE_DATA_DIR  import;
  IMAGE_DATA_DIR  resource;
  IMAGE_DATA_DIR  exception;
  IMAGE_DATA_DIR  certificate;      // must be last section - 1
  IMAGE_DATA_DIR  base_relocation;
  IMAGE_DATA_DIR  debug;            // must be last section
  IMAGE_DATA_DIR  architecture;
  IMAGE_DATA_DIR  global_ptr;
  IMAGE_DATA_DIR  tls;
  IMAGE_DATA_DIR  load_config;
  IMAGE_DATA_DIR  bound_import;
  IMAGE_DATA_DIR  iat;
  IMAGE_DATA_DIR  delay_import_descriptor;
  IMAGE_DATA_DIR  clr_runtime;
  IMAGE_DATA_DIR  reserved;
}

/************************************************************************/

packed struct PE32_PART4
{
  // section table
  SECTION  code;   // code
  SECTION  data;   // constants pool, switch jump tables, dll import tables.
  SECTION  bss;    // global variables.
}

/************************************************************************/

uint    ofs_pe32_part1;
uint    ofs_pe32_part2;
uint    ofs_pe32_part3;
uint    ofs_pe32_part4;

uint    g_code_size;   // must be multiple of PAGE !
uint    g_data_size;   // must be multiple of PAGE !

uint    g_image_index_start_code, g_image_index_end_code;
uint    g_image_index_start_data, g_image_index_end_data;
uint    g_image_index_start_bss,  g_image_index_end_bss;

/************************************************************************/

// btree of dll_names, btree of function_names, list of relocations.

struct RELOC_INFO
{
  uint4       addr;        // offset in g_image where offset32 should be stored
  bool        absolute;    // absolute or relative
  RELOC_INFO^ next_reloc;  // next reloc for this DLL function
  RELOC_INFO^ next_move;   // next reloc for this generated function (to fix when block of code is moved)
}

struct FUNC_INFO
{
  string^     func_name;
  uint4       vector_addr;   // indirect call vector's address
  uint4       hint_name_addr_fill;
  uint4       func_name_addr_fill;
  RELOC_INFO^ first;
  RELOC_INFO^ last;
}

package B1 = new BALANCED_BINARY_TREE (ELEMENT => FUNC_INFO, USER_INFO => bool);

struct DLL_INFO
{
  string^         dll_name;
  uint4           iat_start_addr;
  uint4           lookup_addr_fill;
  uint4           dll_name_fill;
  B1.BINARY_TREE^ funcs;
}

package B2 = new BALANCED_BINARY_TREE (ELEMENT => DLL_INFO, USER_INFO => bool);

bool            dll_tree_created;
B2.BINARY_TREE  dll_tree;
RELOC_INFO^     dll_move_chain;   // in reverse address order


/************************************************************************/

public uint4 current_RIP ()
{
  return LOAD_ADDRESS + exeout.exe_current_ptr ();
}

/************************************************************************/

// called when generating code for new function

public void exe_reset_dll_move_chain ()
{
  dll_move_chain = null;
}

/************************************************************************/

// move all dll relocations addresses of a function starting at a given address
// (used when moving chunks of code for longer branch instructions)

void exe_move_dll_chain (uint4 start_addr, int4 offset)
{
  RELOC_INFO^ r = dll_move_chain;
  while (r != null && r^.addr + LOAD_ADDRESS >= start_addr)
  {
    r^.addr += (uint4)offset;      // move offset in g_image to fix
    r = r^.next_move;
  }
}

/************************************************************************/

void add_reloc_in_func_node (ref FUNC_INFO func_info, bool absolute, uint4 reloc)
{
  RELOC_INFO^ n;

  n = new RELOC_INFO;

  n^.addr       = reloc;             // offset in g_image to fix
  n^.absolute   = absolute;
  n^.next_reloc = null;
  n^.next_move  = dll_move_chain;    // link in move chain
  dll_move_chain = n;

  if (func_info.last == null)
    func_info.first = n;
  else
    func_info.last^.next_reloc = n;
  func_info.last = n;
}

/************************************************************************/

int compare_dll (bool^    user,
                 DLL_INFO a,
                 DLL_INFO b)
{
  _unused user;
  return stricmp (a.dll_name^, b.dll_name^);  // case insensitive compare
}

/************************************************************************/

int compare_func (bool^     user,
                  FUNC_INFO a,
                  FUNC_INFO b)
{
  _unused user;
  return strcmp (a.func_name^, b.func_name^);  // case sensitive compare
}

/************************************************************************/

void add_func_in_dll_node (ref DLL_INFO dll_info,
                               string   func,
                               bool     absolute,
                               uint4    reloc)   // offset in image to fix
{
  string^      func_name;
  FUNC_INFO    func_info;
  int          rc;

  func_name = new string ' (func[0 : strlen(func)]);

  clear(func_info);
  func_info.func_name = func_name;

  rc = B1.retrieve_btree (dll_info.funcs^, ref func_info, BT_EQUAL);

  if (rc != 0 && rc != BT_KEY_NOT_FOUND)
    fatal_compiler_error0 ("add_func_in_dll_node2");

  if (rc == BT_KEY_NOT_FOUND)
  {
    clear(func_info);
    func_info.func_name = func_name;
    func_info.first     = null;
    func_info.last      = null;

    add_reloc_in_func_node (ref func_info, absolute, reloc);

    rc = B1.insert_btree (ref dll_info.funcs^, func_info);
    if (rc < 0)
      fatal_compiler_error0 ("add_func_in_dll_node3");
  }
  else    // func exists
  {
    free func_name;

    add_reloc_in_func_node (ref func_info, absolute, reloc);

    rc = B1.update_btree (ref dll_info.funcs^, func_info);
    if (rc < 0)
      fatal_compiler_error0 ("add_func_in_dll_node4");
  }
}

/************************************************************************/

public void exe_dll_reference_written (string dll, string func, bool absolute)
{
  uint4        reloc = exe_current_ptr() - 4;
  string^      dll_name;
  DLL_INFO     dll_info;
  int          rc;

  if (!dll_tree_created)
  {
    bool^ b = null;
    B2.create_btree (out dll_tree, b, compare_dll);
    dll_tree_created = true;
  }

  dll_name = new string ' (dll[0 : strlen(dll)]);

  clear dll_info;
  dll_info.dll_name = dll_name;

  rc = B2.retrieve_btree (dll_tree, ref dll_info, BT_EQUAL);

  if (rc != 0 && rc != BT_KEY_NOT_FOUND)
    fatal_compiler_error0 ("exe_dll_reference_written(2)");

  if (rc == BT_KEY_NOT_FOUND)
  {
    bool^ b = null;
    clear dll_info;
    dll_info.dll_name = dll_name;
    dll_info.funcs = new B1.BINARY_TREE;

    B1.create_btree (out dll_info.funcs^, b, compare_func);

    add_func_in_dll_node (ref dll_info, func, absolute, reloc);

    rc = B2.insert_btree (ref dll_tree, dll_info);
    if (rc < 0)
      fatal_compiler_error0 ("exe_dll_reference_written(3)");
  }
  else    // dll exists
  {
    free dll_name;

    add_func_in_dll_node (ref dll_info, func, absolute, reloc);

    rc = B2.update_btree (ref dll_tree, dll_info);
    if (rc < 0)
      fatal_compiler_error0 ("exe_dll_reference_written(4)");
  }
}

/************************************************************************/

package HEADER_BYTES

// it's possible to add 72 extra bytes to the 128, to have a large header.

const byte mz_header_size = 128;

byte mz_header[mz_header_size] =
{ 0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
  0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, mz_header_size, 0x00, 0x00, 0x00,
  0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,

/* original "This program cannot be run in DOS mode" */
/*
  0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x54, 0x68,
  0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72,
  0x61, 0x6D, 0x20, 0x63, 0x61, 0x6E, 0x6E, 0x6F,
  0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E,
  0x20, 0x69, 0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20,
  0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A,
*/

/* new : "generated by SAFE-C Compiler for Windows" */
  0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x67, 0x65,
  0x6E, 0x65, 0x72, 0x61, 0x74, 0x65, 0x64, 0x20,
  0x62, 0x79, 0x20, 0x53, 0x41, 0x46, 0x45, 0x2D,
  0x43, 0x20, 0x43, 0x6F, 0x6D, 0x70, 0x69, 0x6C,
  0x65, 0x72, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x57,
  0x69, 0x6E, 0x64, 0x6F, 0x77, 0x73, 0x0D, 0x0A,

  0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

end HEADER_BYTES;

/************************************************************************/

public void pe_init_image ()
{
  exeout.exe_init_image (initial_size => 2*PAGE);

  ofs_pe32_part1 = mz_header'size;
  ofs_pe32_part2 = ofs_pe32_part1 + PE32_PART1'size;

  if (address_size == 4)
    ofs_pe32_part3 = ofs_pe32_part2 + PE32_PART2_32BIT'size;
  else
    ofs_pe32_part3 = ofs_pe32_part2 + PE32_PART2_64BIT'size;

  ofs_pe32_part4 = ofs_pe32_part3 + PE32_PART3'size;

  exeout.exe_set_current_ptr (ofs_pe32_part4 + PE32_PART4'size);

  align_at (PAGE);

  g_image_index_start_code = exe_current_ptr();
}

/************************************************************************/

void write_dll_tables_in_exec_image ()
{
  uint2        mode1, mode2;
  DLL_INFO     dll_info;
  FUNC_INFO    func_info;
  RELOC_INFO^  r;
  int          rc, i;

  if (!dll_tree_created)
    return;

  align_at (256);

  {
    // 1) write dummy IAT table with all zeroes

#begin unsafe
    ref PE32_PART3 p = *(PE32_PART3 *)exe_ptr(ofs_pe32_part3);
#end unsafe
    p.iat.addr = exe_current_ptr();

    mode1 = BT_FIRST;
    clear dll_info;
    for (;;)
    {
      rc = B2.retrieve_btree (dll_tree, ref dll_info, mode1);
      if (rc != 0 && rc != BT_KEY_NOT_FOUND)
        fatal_compiler_error0 ("write_dll_tables_in_exec_image1");

      if (rc == BT_KEY_NOT_FOUND)
        break;

      dll_info.iat_start_addr = exe_current_ptr();   // store start of IAT vectors

      mode2 = BT_FIRST;
      clear func_info;
      for (;;)
      {
        rc = B1.retrieve_btree (dll_info.funcs^, ref func_info, mode2);
        if (rc != 0 && rc != BT_KEY_NOT_FOUND)
          fatal_compiler_error0 ("write_dll_tables_in_exec_image2");

        if (rc == BT_KEY_NOT_FOUND)
          break;

        func_info.vector_addr = exe_current_ptr();

        rc = B1.update_btree (ref dll_info.funcs^, func_info);
        if (rc < 0)
          fatal_compiler_error0 ("write_dll_tables_in_exec_image3");

        r = func_info.first;
        while (r != null)
        {
          uint val;

          if (r^.absolute)
            val = LOAD_ADDRESS + exe_current_ptr();
          else
            val = LOAD_ADDRESS + exe_current_ptr() - (r^.addr + LOAD_ADDRESS + 4);

          exe_patch (r^.addr, val);

          r = r^.next_reloc;
        }

        write_vector (0);  // add a dummy zero vector

        mode2 = BT_LARGER;
      }

      rc = B2.update_btree (ref dll_tree, dll_info);
      if (rc < 0)
        fatal_compiler_error0 ("write_dll_tables_in_exec_image4");

      write_vector (0);  // add a trailing zero vector

      mode1 = BT_LARGER;
    }
  }

  {
#begin unsafe
    ref PE32_PART3 p = *(PE32_PART3 *)exe_ptr(ofs_pe32_part3);
#end unsafe

    p.iat.size = exe_current_ptr() - p.iat.addr;
  }


  {
    // 2) write IMPORT DIRECTORY ENTRIES

#begin unsafe
    ref PE32_PART3 p = *(PE32_PART3 *)exe_ptr(ofs_pe32_part3);
#end unsafe

    p.import.addr = exe_current_ptr();


    mode1 = BT_FIRST;
    for (;;)
    {
      rc = B2.retrieve_btree (dll_tree, ref dll_info, mode1);
      if (rc != 0 && rc != BT_KEY_NOT_FOUND)
        fatal_compiler_error0 ("write_dll_tables_in_exec_image5");

      if (rc == BT_KEY_NOT_FOUND)
        break;

      dll_info.lookup_addr_fill = exe_current_ptr();

      exe_write_int4 (0);  // lookup table
      exe_write_int4 (0);    // dtf
      exe_write_int4 (-1);   // forwarder

      dll_info.dll_name_fill = exe_current_ptr();
      exe_write_int4 (0);  // dll name

      exe_write_int4 ((int4)dll_info.iat_start_addr);  // iat address

      rc = B2.update_btree (ref dll_tree, dll_info);
      if (rc < 0)
        fatal_compiler_error0 ("write_dll_tables_in_exec_image6");

      mode1 = BT_LARGER;
    }

    // write trailing entry with all zeroes

    exe_write_int4  (0);  // lookup table
    exe_write_int4  (0);  // dtf
    exe_write_int4  (0);  // forwarder
    exe_write_int4  (0);  // dll name
    exe_write_int4  (0);  // iat address
  }



  // 3) write LOOKUP table

  align_at ((uint)address_size);

  mode1 = BT_FIRST;
  for (;;)
  {
    rc = B2.retrieve_btree (dll_tree, ref dll_info, mode1);
    if (rc != 0 && rc != BT_KEY_NOT_FOUND)
      fatal_compiler_error0 ("write_dll_tables_in_exec_image7");

    if (rc == BT_KEY_NOT_FOUND)
      break;

    exe_patch (dll_info.lookup_addr_fill, exe_current_ptr());

    mode2 = BT_FIRST;
    clear func_info;
    for (;;)
    {
      rc = B1.retrieve_btree (dll_info.funcs^, ref func_info, mode2);
      if (rc != 0 && rc != BT_KEY_NOT_FOUND)
        fatal_compiler_error0 ("write_dll_tables_in_exec_image8");

      if (rc == BT_KEY_NOT_FOUND)
        break;

      func_info.hint_name_addr_fill = exe_current_ptr();
      write_vector (0);  // lookup table

      rc = B1.update_btree (ref dll_info.funcs^, func_info);
      if (rc < 0)
        fatal_compiler_error0 ("write_dll_tables_in_exec_image9");

      mode2 = BT_LARGER;
    }

    rc = B2.update_btree (ref dll_tree, dll_info);
    if (rc < 0)
      fatal_compiler_error0 ("write_dll_tables_in_exec_image10");

    write_vector (0);  // add a trailing zero vector

    mode1 = BT_LARGER;
  }


  // 4) write dll names + hint/name tables

  mode1 = BT_FIRST;
  for (;;)
  {
    rc = B2.retrieve_btree (dll_tree, ref dll_info, mode1);
    if (rc != 0 && rc != BT_KEY_NOT_FOUND)
      fatal_compiler_error0 ("write_dll_tables_in_exec_image11");

    if (rc == BT_KEY_NOT_FOUND)
      break;

    align_at (2);
    exe_patch (dll_info.dll_name_fill, exe_current_ptr());

    for (i=0; i<dll_info.dll_name^'length; i++)
      exe_write_int1 ((int1)dll_info.dll_name^[i]);
    exe_write_int1 (0);   // trailing zero

    align_at (2);

    mode2 = BT_FIRST;
    clear func_info;
    for (;;)
    {
      rc = B1.retrieve_btree (dll_info.funcs^, ref func_info, mode2);
      if (rc != 0 && rc != BT_KEY_NOT_FOUND)
        fatal_compiler_error0 ("write_dll_tables_in_exec_image12");

      if (rc == BT_KEY_NOT_FOUND)
        break;

      align_at (2);

      exe_patch (func_info.hint_name_addr_fill, exe_current_ptr());
      exe_patch (func_info.vector_addr, exe_current_ptr());

      exe_write_int2 (0);  // ordinal always zero

      // ascii string of function name
      for (i=0; i<func_info.func_name^'length; i++)
        exe_write_int1 ((int1)func_info.func_name^[i]);
      exe_write_int1 (0);   // trailing zero

      align_at (2);

      mode2 = BT_LARGER;
    }

    mode1 = BT_LARGER;
  }

  {
#begin unsafe
    ref PE32_PART3 p = *(PE32_PART3 *)exe_ptr(ofs_pe32_part3);
#end unsafe
    p.import.size = exe_current_ptr() - p.import.addr;
  }
}

/************************************************************************/

package RELOCATION_DATA

  // func label relocation (filled at end-of-program)

  /* a tree */

  struct RELOC_FUNC_NODE
  {
    int        func_label_nr;
    uint4      func_addr;      // RIP of function
  }

  package B3 = new BALANCED_BINARY_TREE (ELEMENT => RELOC_FUNC_NODE, USER_INFO => bool);

  bool            func_reloc_tree_created;
  B3.BINARY_TREE  func_reloc_tree;


  /* a list of addresses to backfill, in decreasing backfill order */

  struct RELOC_FUNC_INFO
  {
    uint4            backfill_addr;    // RIP of 4-byte address to fill
    int              func_label_nr;
    bool             absolute;         // absolute or relative
    RELOC_FUNC_INFO^ next;
  }

  RELOC_FUNC_INFO^ func_reloc_list;   // in reverse backfill address order

end RELOCATION_DATA;

/************************************************************************/

int func_reloc_tree_compare (bool^           user,
                             RELOC_FUNC_NODE a,
                             RELOC_FUNC_NODE b)
{
  _unused user;
  if (a.func_label_nr < b.func_label_nr)
    return -1;
  if (a.func_label_nr > b.func_label_nr)
    return +1;
  return 0;
}

/************************************************************************/

public void exe_add_func_label (int func_label_nr)
{
  RELOC_FUNC_NODE n;
  int             rc;

  if (g_tracing)
    trace ("    declare entry func #%d\n", func_label_nr);

  if (!func_reloc_tree_created)
  {
    bool^ b = null;
    B3.create_btree (out func_reloc_tree, b, func_reloc_tree_compare);
    func_reloc_tree_created = true;
  }

  clear n;
  n.func_label_nr = func_label_nr;
  n.func_addr     = current_RIP();

  rc = B3.insert_btree (ref func_reloc_tree, n);
  if (rc < 0)
    fatal_compiler_error0 ("exe_add_func_label(1)");
}

/************************************************************************/

public void exe_func_backfill_addr4_written (int func_label_nr, bool absolute)
{
  RELOC_FUNC_INFO^ n;

  n = new RELOC_FUNC_INFO;
  n^.backfill_addr = current_RIP() - 4;
  n^.func_label_nr = func_label_nr;
  n^.absolute      = absolute;
  n^.next = func_reloc_list;

  func_reloc_list = n;
}

/************************************************************************/

void move_all_func_backfills (uint4 start_addr, int4 offset)
{
  RELOC_FUNC_INFO^ n = func_reloc_list;

  while (n != null && n^.backfill_addr >= start_addr)
  {
    n^.backfill_addr += (uint)offset;
    n = n^.next;
  }
}

/************************************************************************/

void fill_all_func_backfills ()
{
  RELOC_FUNC_INFO^ n = func_reloc_list;
  int              rc;
  RELOC_FUNC_NODE  nd;
  uint4            target_address;

  while (n != null)
  {
    uint val;

    clear nd;
    nd.func_label_nr = n^.func_label_nr;

    rc = B3.retrieve_btree (func_reloc_tree, ref nd, BT_EQUAL);
    if (rc < 0)
      fatal_compiler_error0 ("fill_all_func_backfills()");

    target_address = nd.func_addr;

    if (n^.absolute)
    {
      val = target_address;
    }
    else
    {
      val = target_address - (n^.backfill_addr + 4);
    }

    exe_patch (n^.backfill_addr - LOAD_ADDRESS, val);

    n = n^.next;
  }
}

/************************************************************************/
/************************************************************************/
/************************************************************************/

package RELOCATE_GLOBALS

  /* a list of addresses to backfill, in decreasing backfill order */

  struct RELOC_GLOBAL_INFO
  {
    uint4               backfill_addr;    // RIP of 4-byte address to fill
    int                 global_address;   // global variable's address, relative to start of bss segment
    bool                absolute;         // absolute or relative
    RELOC_GLOBAL_INFO^  next;
  }

  RELOC_GLOBAL_INFO^ global_reloc_list;   // in reverse backfill address order

end RELOCATE_GLOBALS;

/************************************************************************/

public void exe_global_backfill_addr4_written (int relative_global_addr, bool absolute)
{
  RELOC_GLOBAL_INFO^ n;

  n = new RELOC_GLOBAL_INFO;

  n^.backfill_addr = current_RIP() - 4;
  n^.global_address = relative_global_addr;
  n^.absolute      = absolute;
  n^.next = global_reloc_list;

  global_reloc_list = n;
}

/************************************************************************/

void move_all_global_backfills (uint4 start_addr, int4 offset)
{
  RELOC_GLOBAL_INFO^ n = global_reloc_list;

  while (n != null && n^.backfill_addr >= start_addr)
  {
    n^.backfill_addr += (uint)offset;
    n = n^.next;
  }
}

/************************************************************************/

// to be called once bss segment's address is fixed

void fill_all_global_backfills ()
{
  RELOC_GLOBAL_INFO^ n = global_reloc_list;
  uint4              target;

  while (n != null)
  {
    uint val;

    target = LOAD_ADDRESS + exe_current_ptr() + (uint)n^.global_address;

    if (n^.absolute)
    {
      val = target;
    }
    else
    {
      val = target - (n^.backfill_addr + 4);
    }

    exe_patch (n^.backfill_addr - LOAD_ADDRESS, val);

    n = n^.next;
  }
}

/************************************************************************/
/************************************************************************/
/************************************************************************/

package JUMP_TABLE

  /* a list of jump table constants to fix (convert each label_nr -> code_address) */

  struct RELOC_JUMPTABLE_INFO
  {
    int8                  serial_nr;
    RELOC_JUMPTABLE_INFO^ next;
  }

  RELOC_JUMPTABLE_INFO^ reloc_jumptable_list;

end JUMP_TABLE;

/************************************************************************/

// to be called for each jump table

public void exe_mark_jump_table (int8 pool_nr)
{
  RELOC_JUMPTABLE_INFO^ n;

  n = new RELOC_JUMPTABLE_INFO;
  n^.serial_nr = pool_nr;
  n^.next      = reloc_jumptable_list;

  reloc_jumptable_list = n;
}

/************************************************************************/
/************************************************************************/
/************************************************************************/

package LABELS

  int[]^ pnear_labels;          // contains 0 or IP of label
  int    nb_near_labels;

  /************************************************************************/

  /* a list of near addresses to backfill, in decreasing backfill order */

  struct RELOC_NEAR_INFO
  {
    uint4            backfill_addr;    // RIP of 1-byte or 4-byte address to fill
    int              label_nr;         // target label nr
    bool             conditional;
    bool             was_expanded;
    RELOC_NEAR_INFO^ next;
  }

  RELOC_NEAR_INFO^ reloc_near_list;   // in reverse backfill address order

end LABELS;

/************************************************************************/

// to be called after p-code for a function was generated,
// before generating asm.

public void exe_allocate_near_label_table (int nb_labels)
{
  free (pnear_labels);

  pnear_labels = new int [ nb_labels ];

  nb_near_labels = nb_labels;
}

public void exe_declare_near_label (int label_nr)
{
  if (label_nr < 0 || label_nr >= nb_near_labels)
    fatal_compiler_error0 ("exe_declare_near_label(1)");

  pnear_labels^[label_nr] = (int)current_RIP();

  if (g_tracing)
    trace ("    dcl label #%d\n", label_nr);
}

void move_near_labels (uint4 start_addr, int4 offset)
{
  int              i;
  RELOC_NEAR_INFO^ p;

  for (i=0; i<nb_near_labels; i++)
  {
    if (pnear_labels^[i] >= (int)start_addr)
      pnear_labels^[i] += offset;
  }

  p = reloc_near_list;
  while (p != null && p^.backfill_addr >= start_addr)
  {
    p^.backfill_addr += (uint)offset;
    p = p^.next;
  }
}

// we just wrote a 1-byte branch
public void exe_near_branch_written (int label_nr, bool conditional)
{
  RELOC_NEAR_INFO^ n;

  n = new RELOC_NEAR_INFO;
  n^.backfill_addr = current_RIP() - 1;
  n^.label_nr      = label_nr;
  n^.conditional   = conditional;
  n^.was_expanded  = false;
  n^.next          = reloc_near_list;

  reloc_near_list = n;
}


public void exe_insert_code_sequence (uint4 backfill_addr, uint4 offset_size_increase)
{
  uint size;

  move_all_func_backfills   (backfill_addr, (int)offset_size_increase);
  exe_move_dll_chain        (backfill_addr, (int)offset_size_increase);
  move_all_pool_backfills   (backfill_addr, (int)offset_size_increase);
  move_all_global_backfills (backfill_addr, (int)offset_size_increase);
  move_near_labels          (backfill_addr, (int)offset_size_increase);
  move_dbg_lines            ((int)backfill_addr, (int)offset_size_increase);

  probe (offset_size_increase);
  size = exe_current_ptr() - (backfill_addr - LOAD_ADDRESS);

  exe_move_byte_sequence (origin      => backfill_addr - LOAD_ADDRESS,
                          destination => (backfill_addr - LOAD_ADDRESS) + offset_size_increase, 
                          size        => size);

  exec_advance_ptr (offset_size_increase);
}


public void exe_update_code_sequence (uint4 backfill_addr, byte[] seq)
{
  exe_patch (backfill_addr - LOAD_ADDRESS, seq);
}


// to be called after generating asm for each function

void expand_branch_offsets ()
{
  bool changes_done;

  for (;;)
  {
    RELOC_NEAR_INFO^ n;
    int              offset, offset_size_increase;
    uint4            backfill_addr;

    changes_done = false;

    for (n=reloc_near_list; n!=null; n=n^.next)
    {
      if (n^.was_expanded)
        continue;

      offset = pnear_labels^[n^.label_nr] - (int)(n^.backfill_addr + 1);
      if (offset >= -128 && offset <= 127)    // 1 byte is enough
        continue;

      // we need to increase the offset size

      offset_size_increase = (n^.conditional) ? 4 : 3;
      backfill_addr = n^.backfill_addr;
      n^.was_expanded = true;

      // +1 to avoid that the backfill address itself be relocated
      exe_insert_code_sequence (backfill_addr+1, (uint)offset_size_increase);

      changes_done = true;
    }

    if (!changes_done)
      break;
  }
}


#begin unsafe

// to be called after generating asm for each function

void backfill_all_near_branch_offsets ()
{
  RELOC_NEAR_INFO^ n;
  int              offset;

  for (n=reloc_near_list; n!=null; n=n^.next)
  {
    if (n^.was_expanded)
    {
      if (n^.conditional)
      {
        uint val;

        // c_jcond() generates either 2 or 6 bytes of code
        // before: 0x7n i1
        // after:  0x0F 0x8n i4)
        //               ^ (backfill_addr)
        *(byte*)exe_ptr (n^.backfill_addr - LOAD_ADDRESS) =
          (byte)((*(byte*)exe_ptr (n^.backfill_addr - LOAD_ADDRESS - 1)) + 0x10);

        *(byte*)exe_ptr (n^.backfill_addr - LOAD_ADDRESS - 1) = 0x0F;

        val = (uint)pnear_labels^[n^.label_nr] - (n^.backfill_addr + 5);
        *(uint*)exe_ptr (n^.backfill_addr - LOAD_ADDRESS + 1) = val;
      }
      else
      {
        uint val;

        // c_jump_relative() generates either 2 or 5 bytes of code
        // before: 0xEB i1
        // after:  0xE9 i4
        //              ^ (backfill_addr)

        *(byte*)exe_ptr (n^.backfill_addr - LOAD_ADDRESS - 1) = 0xE9;

        val = (uint)pnear_labels^[n^.label_nr] - (n^.backfill_addr + 4);
        *(uint*)exe_ptr (n^.backfill_addr - LOAD_ADDRESS) = val;
      }
    }
    else   // 1-byte offset
    {
      offset = pnear_labels^[n^.label_nr] - (int)(n^.backfill_addr + 1);
      *(byte*)exe_ptr (n^.backfill_addr - LOAD_ADDRESS) = (byte)offset;
    }
  }
}

#end unsafe

// to be called after generating asm for each function

void free_near_list ()
{
  RELOC_NEAR_INFO^ n, prev;

  n = reloc_near_list;

  while (n != null)
  {
    prev = n;
    n = n^.next;
    free prev;
  }

  reloc_near_list = null;
}

/************************************************************************/

void reloc_jumptable_data ()
{
  RELOC_JUMPTABLE_INFO^ n;

  n = reloc_jumptable_list;

  while (n != null)
  {
    relocate_jumptable_pool_constant (n^.serial_nr, pnear_labels^);
    n = n^.next;
  }
}

/************************************************************************/

void free_jumptable ()
{
  RELOC_JUMPTABLE_INFO^ n, prev;

  n = reloc_jumptable_list;

  while (n != null)
  {
    prev = n;
    n = n^.next;
    free prev;
  }

  reloc_jumptable_list = null;
}

/************************************************************************/

// to be called after generating asm for each function

public void relocate_all_near_labels ()
{
  expand_branch_offsets ();
  backfill_all_near_branch_offsets ();
  free_near_list ();

  reloc_jumptable_data ();
  free_jumptable ();
}

/************************************************************************/
/************************************************************************/
/************************************************************************/

public void exe_finish_code ()
{
  fill_all_func_backfills ();

  g_image_index_end_code = exe_current_ptr();    // for printing CODE end only

  align_at (PAGE);   // 4K
  g_image_index_start_data = exe_current_ptr();   // for printing DATA start only

  g_code_size = exe_current_ptr() - HEADER;    // M4096 aligned code size
}

/************************************************************************/

public uint4 exe_store_pool_constant (byte[] cte, uint4 align)
{
  return LOAD_ADDRESS + exe_write_byte_sequence (cte, align);
}

/************************************************************************/
void write_resources_in_exec_image (string res_filename);
/************************************************************************/

public void exe_finish_data (string res_filename, int8 global_offset)
{
  write_resources_in_exec_image (res_filename);

  g_image_index_end_data = exe_current_ptr();     // for printing DATA end only

  write_dll_tables_in_exec_image ();

  align_at (PAGE);   // 4K
  g_image_index_start_bss = exe_current_ptr();                                    // for printing BSS start only
  g_image_index_end_bss = (uint)((int)exe_current_ptr() + (int)global_offset);    // for printing BSS end only

  g_data_size = exe_current_ptr() - (HEADER + g_code_size);   // M4096 aligned data size

  // to be called once bss segment's address is fixed
  fill_all_global_backfills ();
}

/************************************************************************/

public
void exe_terminate_image (uint4 bss_size,    // must be multiple of PAGE !
                          uint4 stack_size,  // must be multiple of PAGE !
                          bool  console_app)
{
  DATE_TIME  now;

#begin unsafe
  exe_ptr(0)[0 : mz_header'size] = mz_header;
#end unsafe  

  {
#begin unsafe
    ref PE32_PART1 p1 = *(PE32_PART1 *)exe_ptr(ofs_pe32_part1);
#end unsafe

    p1.signature   = 0x4550;     // "PE" 0x00 0x00
    p1.machineType = (uint2)((address_size == 4) ? 0x014C : 0x8664);     // 0x014C for i386, 0x8664 for x64.
    p1.nb_sections = 3;          // 3 (text, data, bss) (see at the end of this struct)

    // file creation dtf (nb secs since 1/1/1970)
    get_datetime (out now);
    p1.created_dtf = (uint4)(86400 * (nb_days_since_1901 (now.day, now.month, now.year) - nb_days_since_1901 (1, 1, 1970))
                              + now.hour * 3600 + now.min * 60 + now.sec);
    p1.symbols     = 0;
    p1.nb_symbols  = 0;

    p1.opt_header_size = (uint2)(ofs_pe32_part4 - ofs_pe32_part2);     // 00E0 or 00F0
    p1.characteristics = (uint2)((address_size == 4) ? 0x103 : 0x023);  // (0x0100=32-bit, 0x020 = large addr aware, 0x0002=file valid, 0x0001=no reloc)
  }

  if (address_size == 4)
  {
#begin unsafe
    ref PE32_PART2_32BIT p2 = *(PE32_PART2_32BIT *)exe_ptr(ofs_pe32_part2);
#end unsafe

    // optional header - standard fields
    p2.magic                   = 0x010B;  // 0x010B=PE32, 0x020B=PE32+)
    p2.MajorLinkerVersion      = 0x06;
    p2.MinorLinkerVersion      = 0x00;
    p2.SizeOfCode              = g_code_size;
    p2.SizeOfInitializedData   = g_data_size;
    p2.SizeOfUninitializedData = bss_size;
    p2.EntryPoint              = HEADER;        // PC start address (skip MZ header)
    p2.BaseOfCode              = HEADER;        // (.text loaded at ImageBase+BaseOfCode = 0x00401000)
    p2.BaseOfData              = p2.BaseOfCode + g_code_size;   // warning: this field does not exist for PE32+

    // optional header - windows-specific fields
    p2.ImageBase               = LOAD_ADDRESS;    // (MZ_header loaded at this address)
    p2.SectionAlignment        = PAGE;
    p2.FileAlignment           = PAGE;
    p2.MajorOSVersion          = 4;
    p2.MinorOSVersion          = 0;
    p2.MajorImageVersion       = 0;
    p2.MinorImageVersion       = 0;
    p2.MajorSubsystemVersion   = 4;   // 4 (Win32 4.0)
    p2.MinorSubsystemVersion   = 0;
    p2.Win32VersionValue       = 0;
    p2.SizeOfImage             = HEADER + g_code_size + g_data_size + bss_size;  // size of g_image to be loaded in memory, incl. headers (multiple of SectionAlignment)
    p2.SizeOfHeaders           = HEADER;
    p2.CheckSum                = 0;
    p2.Subsystem               = (uint2)(console_app ? 3 : 2);     // 2=WINDOWS_APP, 3=CONSOLE_APP
    p2.DllCharacteristics      = 0;           // 0
    p2.SizeOfStackReserve      = stack_size;  // (1 MB stack : 0x02FFFF to 0x12FFFF)
                                              // (2 MB stack : 0x02FFFF to 0x22FFFF)
                                              // (3 MB stack : 0x02FFFF to 0x32FFFF)
                                              // (4 MB stack : 0x40FFFF to 0x80FFFF)
    p2.SizeOfStackCommit       = PAGE;        // (4 KB pour démarrer)
    p2.SizeOfHeapReserve       = 0x00100000;  // (1 MB heap)
    p2.SizeOfHeapCommit        = PAGE;        // (4 KB pour démarrer)
    p2.LoaderFlags             = 0;           // 0
    p2.NumberOfDirectories     = 16;
  }
  else   // 64 bit
  {
#begin unsafe
    ref PE32_PART2_64BIT p2 = *(PE32_PART2_64BIT *)exe_ptr(ofs_pe32_part2);
#end unsafe

    // optional header - standard fields
    p2.magic                   = 0x020B;  // 0x010B=PE32, 0x020B=PE32+)
    p2.MajorLinkerVersion      = 0x06;
    p2.MinorLinkerVersion      = 0x00;
    p2.SizeOfCode              = g_code_size;
    p2.SizeOfInitializedData   = g_data_size;
    p2.SizeOfUninitializedData = bss_size;
    p2.EntryPoint              = HEADER;        // PC start address (skip MZ header)
    p2.BaseOfCode              = HEADER;        // (.text loaded at ImageBase+BaseOfCode = 0x00401000)
    p2.ImageBase64             = LOAD_ADDRESS;    // (MZ_header loaded at this address)
    p2.SectionAlignment        = PAGE;
    p2.FileAlignment           = PAGE;
    p2.MajorOSVersion          = 6;   // Vista
    p2.MinorOSVersion          = 0;
    p2.MajorImageVersion       = 0;
    p2.MinorImageVersion       = 0;
    p2.MajorSubsystemVersion   = 6;   // 5.0=Win2000, 5.1=Xp, 6.0=Vista, 6.1=Win7
    p2.MinorSubsystemVersion   = 0;
    p2.Win32VersionValue       = 0;
    p2.SizeOfImage             = HEADER + g_code_size + g_data_size + bss_size;  // size of image to be loaded in memory, incl. headers (multiple of SectionAlignment)
    p2.SizeOfHeaders           = HEADER;
    p2.CheckSum                = 0;
    p2.Subsystem               = (uint2)(console_app ? 3 : 2);     // 2=WINDOWS_APP, 3=CONSOLE_APP
    p2.DllCharacteristics      = 0;           // 0
    p2.SizeOfStackReserve      = stack_size;  // (1 MB stack : 0x02FFFF to 0x12FFFF)
                                              // (2 MB stack : 0x02FFFF to 0x22FFFF)
                                              // (3 MB stack : 0x02FFFF to 0x32FFFF)
                                              // (4 MB stack : 0x40FFFF to 0x80FFFF)
    p2.SizeOfStackCommit       = PAGE;        // (4 KB pour démarrer)
    p2.SizeOfHeapReserve       = 0x00100000;  // (1 MB heap)
    p2.SizeOfHeapCommit        = PAGE;        // (4 KB pour démarrer)
    p2.LoaderFlags             = 0;           // 0
    p2.NumberOfDirectories     = 16;
  }


  // section table

  {
#begin unsafe
    ref PE32_PART4 p4 = *(PE32_PART4 *)exe_ptr(ofs_pe32_part4);
#end unsafe

    // .text (code)
    strcpy (out p4.code.name, ".text");
    p4.code.size_in_memory       = g_code_size;
    p4.code.addr                 = HEADER;
    p4.code.SizeOnDisk           = g_code_size;   // multiple of FileAlignment
    p4.code.PointerToDisk        = HEADER;      // ptr into section data in file, relative to optional header
    p4.code.PointerToRelocations = 0;
    p4.code.PointerToLinenumbers = 0;
    p4.code.NumberOfRelocations  = 0;
    p4.code.NumberOfLinenumbers  = 0;
    p4.code.Characteristics      = 0x20000020;   // code + exec (most exe's use 60000020 for readable too)

    // .data (constants pool, switch jump tables, dll import tables)
    strcpy (out p4.data.name, ".data");
    p4.data.size_in_memory       = g_data_size;
    p4.data.addr                 = HEADER + g_code_size;
    p4.data.SizeOnDisk           = g_data_size;          // multiple of FileAlignment  (0 for .bss section)
    p4.data.PointerToDisk        = HEADER + g_code_size; // ptr into section data in file, relative to optional header
    p4.data.PointerToRelocations = 0;
    p4.data.PointerToLinenumbers = 0;
    p4.data.NumberOfRelocations  = 0;
    p4.data.NumberOfLinenumbers  = 0;
    p4.data.Characteristics      = 0x40000040;      // init + read (initialized read-only data)

    // .bss (global variables)
    strcpy (out p4.bss.name, ".bss");
    p4.bss.size_in_memory       = bss_size;
    p4.bss.addr                 = HEADER + g_code_size + g_data_size;
    p4.bss.SizeOnDisk           = 0;  // 0 for .bss section
    p4.bss.PointerToDisk        = 0;
    p4.bss.PointerToRelocations = 0;
    p4.bss.PointerToLinenumbers = 0;
    p4.bss.NumberOfRelocations  = 0;
    p4.bss.NumberOfLinenumbers  = 0;
    p4.bss.Characteristics      = 0xC0000080;       // uninit + read + write
  }
}

/************************************************************************/

public void print_crc ()
{
  uint crc, ofs;

  // compute crc of executable, but skip 4 bytes at offset (ofs_pe32_part1 + 8)
  ofs = ofs_pe32_part1 + 8;

  crc = 0;
#begin unsafe
  update_crc (ref crc, exe_ptr(0)[0:ofs]);
  update_crc (ref crc, exe_ptr(0)[ofs + 4 : exe_current_ptr() - (ofs + 4)]);
#end unsafe  

  printf ("CRC=%08x CODE=%u bytes DATA=%u bytes BSS=%u bytes\n",
         crc,
         g_image_index_end_code - g_image_index_start_code,
         g_image_index_end_data - g_image_index_start_data,
         g_image_index_end_bss  - g_image_index_start_bss);
}

/************************************************************************/

package RESOURCE_STRUCTS

  packed struct RES_DIR_HEADER
  {
    uint    charac;
    uint    datetime;
    uint2   major;
    uint2   minor;
    uint2   nb_names;
    uint2   nb_ids;
  }

  packed struct RES_DIR_ENTRY
  {
    uint    name_or_id;
    uint    next;    // dir has high bit=1, leaf has not.
  }

  packed struct RES_STRING_ENTRY      // aligned at 2 !!
  {
    uint2   len;
    // followed by unicode string
  }

  packed struct RES_NODE
  {
    uint    res_nva;
    uint    res_size;
    uint    codepage;
    uint    unused;
  }


  struct RES_DATA
  {
    string^   name;
    uint      id;
    uint      ientry;   // ptr to RES_DIR_ENTRY
  }

  // each RES_INFO is sorted by type/id/lang,
  // and at each level first the name's then the id's.

  struct RES_INFO
  {
    RES_DATA  type;
    RES_DATA  id;
    RES_DATA  lang;
    string^   resource_filename;
    uint      inode;    // ptr to RES_NODE
  }

  typedef RES_INFO^ PRES_INFO;

  package B5 = new BALANCED_BINARY_TREE (ELEMENT => PRES_INFO, USER_INFO => bool);

  B5.BINARY_TREE res_tree;             // tree of PRES_INFO
  bool           res_tree_created;

end RESOURCE_STRUCTS;

/**********************************************************************************/

int uintcmp (uint a, uint b)
{
  if (a < b)
    return -1;
  if (a > b)
    return +1;
  return 0;
}

/**********************************************************************************/

int cmp_res_data (RES_DATA a, RES_DATA b)
{
  int cmp;

  if (a.name != null && b.name == null)
    return -1;

  if (a.name == null && b.name != null)
    return +1;

  if (a.name != null && b.name != null)
    cmp = stricmp (a.name^, b.name^);
  else
    cmp = uintcmp (a.id, b.id);

  return cmp;
}

/**********************************************************************************/

int res_compare (bool^      user,
                 PRES_INFO  a,
                 PRES_INFO  b)
{
  int cmp;

  _unused user;

  cmp = cmp_res_data (a^.type, b^.type);
  if (cmp != 0)
    return cmp;

  cmp = cmp_res_data (a^.id, b^.id);
  if (cmp != 0)
    return cmp;

  cmp = cmp_res_data (a^.lang, b^.lang);
  return cmp;
}

/**********************************************************************************/

string^ new_string (string s)
{
  return new string ' (s[0 : strlen(s)]);
}

/**********************************************************************************/

void fill_res_data (string value, ref RES_DATA p)
{
  if (isdigit(value[0]))
    sscanf (value, "%u", out p.id);
  else
    p.name = new_string (value);
}

/**********************************************************************************/

// returns 0 if OK, -1 if duplicate key

int insert_resource_in_tree (string namestr, string typstr, string filename)
{
  PRES_INFO p;
  int       rc;

  if (!res_tree_created)
  {
    bool^ b = null;
    B5.create_btree (out res_tree, b, res_compare);
    res_tree_created = true;
  }

  p = new RES_INFO;

  fill_res_data (typstr,  ref p^.type);
  fill_res_data (namestr, ref p^.id);
  fill_res_data ("0",     ref p^.lang);

  p^.resource_filename = new_string (filename);

  rc = B5.insert_btree (ref res_tree, p);
  if (rc == BT_DUPLICATE_KEY)
    return -1;
  if (rc != 0)
    fatal_out_of_memory_error ("insert_resource_in_tree(2)");

  return 0;
}

/**********************************************************************************/

public void parse_resource_file (string current_dir, string res_filename)
{
  FILE fp;
  int  line_nr, col, len, res_col;
  char line[512], namestr[64+1], typstr[64+1], filename[200+1];
  char absolute_filename[260], msg[512];

  if (fopen (out fp, res_filename) < 0)  // no resource file provided
    return;

  line_nr = 0;
  while (fgets (ref fp, out line) == 0)
  {
    line_nr++;
    col = 0;

    // skip blanks
    while (line[col] == ' ')
      col++;

    if (line[col] == '\n' || line[col] == nul)
      continue;

    clear namestr;
    len = 0;
    while (len < namestr'length-1 && line[col] > ' ')
      namestr[len++] = line[col++];
    namestr[len] = nul;
    if (len == 0)
      resource_error (res_filename, "resource ID expected", line_nr, col);
    if (len == namestr'length)
      resource_error (res_filename, "resource ID is too long", line_nr, col);

    // skip blanks
    while (line[col] == ' ')
      col++;

    clear typstr;
    len = 0;
    while (len < typstr'length-1 && line[col] > ' ')
      typstr[len++] = line[col++];
    typstr[len] = nul;
    if (len == 0)
      resource_error (res_filename, "resource TYPE expected", line_nr, col);
    if (len == typstr'length)
      resource_error (res_filename, "resource TYPE is too long", line_nr, col);

    // skip blanks
    while (line[col] == ' ')
      col++;

    res_col = col;

    clear filename;
    len = 0;
    while (len < filename'length-1 && line[col] > ' ')
      filename[len++] = line[col++];
    filename[len] = nul;
    if (len == 0)
      resource_error (res_filename, "resource FILENAME expected", line_nr, col);
    if (len == filename'length)
      resource_error (res_filename, "resource FILENAME is too long", line_nr, col);

    // skip blanks
    while (line[col] == ' ')
      col++;

    if (line[col] != '\n' && line[col] != nul)
      resource_error (res_filename, "end of line expected", line_nr, col);

    clear absolute_filename;
    if (expand_pathname (current_dir, filename, out absolute_filename[0 : absolute_filename'length-5]) < 0)
      resource_error (res_filename, "cannot create resource filename", line_nr, res_col);

    if (!exists (absolute_filename))
    {
      sprintf (out msg, "cannot open resource %s", absolute_filename);
      resource_error (res_filename, msg, line_nr, res_col);
    }

    if (insert_resource_in_tree (namestr, typstr, filename) < 0)
      resource_error (res_filename, "resource give twice", line_nr, 1);
  }

  fclose (ref fp);
}

/************************************************************************/

bool entries_different (RES_DATA a, RES_DATA b)
{
  if (a.name != null && b.name != null)
    return stricmp (a.name^, b.name^) != 0;

  if (a.name == null || b.name == null)
    return a.id != b.id;

  return true;
}

/************************************************************************/

void write_resources_in_exec_image (string res_filename)
{
#begin unsafe
  uint2           mode;
  RES_INFO^       info, prev_info;
  int             rc;
  uint            ihead, ient=0, size;
  RES_DIR_HEADER* phead;
  RES_DIR_ENTRY*  pent, tmpent;

  if (!res_tree_created)
    return;

  // 1) write TYPE directory

  align_at (16);

  {
    ref PE32_PART3 p = *(PE32_PART3 *)exe_ptr(ofs_pe32_part3);
    p.resource.addr = exe_current_ptr();


    // send TYPE directory

    size = RES_DIR_HEADER'size;
    probe (size);
    ihead = exe_current_ptr();
    phead = (RES_DIR_HEADER *)exe_ptr(ihead);
    clear *phead;
    exec_advance_ptr (size);

    prev_info = null;
    clear info;

    mode = BT_FIRST;
    for (;;)
    {
      rc = B5.retrieve_btree (res_tree, ref info, mode);
      if (rc != 0 && rc != BT_KEY_NOT_FOUND)
        fatal_compiler_error0 ("write_resources_in_exec_image()");

      if (rc == BT_KEY_NOT_FOUND)
        break;

      if (prev_info == null || entries_different (prev_info^.type, info^.type))
      {
        size = RES_DIR_ENTRY'size;
        probe (size);
        ient = exe_current_ptr();
        pent = (RES_DIR_ENTRY *)exe_ptr(ient);
        clear *pent;
        exec_advance_ptr (size);

        if (info^.type.name == null)
          pent->name_or_id = info^.type.id;


        // update counters

        phead = (RES_DIR_HEADER *)exe_ptr(ihead);
        if (info^.type.name != null)  // it's a name
          phead->nb_names++;
        else     // it's numeric
          phead->nb_ids++;

        prev_info = info;
      }

      info^.type.ientry = ient;

      mode = BT_LARGER;
    }


    // send ID directory

    prev_info = null;

    mode = BT_FIRST;
    for (;;)
    {
      rc = B5.retrieve_btree (res_tree, ref info, mode);
      if (rc != 0 && rc != BT_KEY_NOT_FOUND)
        fatal_compiler_error0 ("write_resources_in_exec_image()");

      if (rc == BT_KEY_NOT_FOUND)
        break;

      if (prev_info == null || entries_different (prev_info^.type, info^.type))
      {
        // start new ID directory table (all the ID's of this type)
        size = RES_DIR_HEADER'size;
        probe (size);
        ihead = exe_current_ptr();
        phead = (RES_DIR_HEADER *)exe_ptr(ihead);
        clear *phead;
        exec_advance_ptr (size);

        tmpent = (RES_DIR_ENTRY *)exe_ptr(info^.type.ientry);
        tmpent->next = ((exe_current_ptr() - size) - p.resource.addr) | 0x80000000;

        prev_info = null;
      }

      if (prev_info == null || entries_different (prev_info^.id, info^.id))
      {
        size = RES_DIR_ENTRY'size;
        probe (size);
        ient = exe_current_ptr();
        pent = (RES_DIR_ENTRY *)exe_ptr(ient);
        clear *pent;
        exec_advance_ptr (size);

        if (info^.id.name == null)
          pent->name_or_id = info^.id.id;


        // update counters

        phead = (RES_DIR_HEADER *)exe_ptr(ihead);
        if (info^.id.name != null)  // it's a name
          phead->nb_names++;
        else     // it's numeric
          phead->nb_ids++;

        prev_info = info;
      }

      info^.id.ientry = ient;

      mode = BT_LARGER;
    }


    // send LANG directory

    prev_info = null;

    mode = BT_FIRST;
    for (;;)
    {
      rc = B5.retrieve_btree (res_tree, ref info, mode);
      if (rc != 0 && rc != BT_KEY_NOT_FOUND)
        fatal_compiler_error0 ("write_resources_in_exec_image()");

      if (rc == BT_KEY_NOT_FOUND)
        break;

      if (prev_info == null ||
          entries_different (prev_info^.type, info^.type) ||
          entries_different (prev_info^.id,   info^.id))
      {
        // start new LANG directory table (all the LANG's of this type+id)
        size = RES_DIR_HEADER'size;
        probe (size);
        ihead = exe_current_ptr();
        phead = (RES_DIR_HEADER *)exe_ptr(ihead);
        clear *phead;
        exec_advance_ptr (size);

        tmpent = (RES_DIR_ENTRY *)exe_ptr(info^.id.ientry);
        tmpent->next = ((exe_current_ptr() - size) - p.resource.addr) | 0x80000000;
      }

      {
        size = RES_DIR_ENTRY'size;
        probe (size);
        ient = exe_current_ptr();
        pent = (RES_DIR_ENTRY *)exe_ptr(ient);
        clear *pent;
        exec_advance_ptr (size);

        if (info^.lang.name == null)
          pent->name_or_id = info^.lang.id;


        // update counters

        phead = (RES_DIR_HEADER *)exe_ptr(ihead);
        if (info^.lang.name != null)  // it's a name
          phead->nb_names++;
        else     // it's numeric
          phead->nb_ids++;

        info^.lang.ientry = ient;
      }

      prev_info = info;


      mode = BT_LARGER;
    }


    // send unicode strings

    align_at (2);

    prev_info = null;

    mode = BT_FIRST;
    for (;;)
    {
      rc = B5.retrieve_btree (res_tree, ref info, mode);
      if (rc != 0 && rc != BT_KEY_NOT_FOUND)
        fatal_compiler_error0 ("write_resources_in_exec_image()");

      if (rc == BT_KEY_NOT_FOUND)
        break;

      if (info^.type.name != null)   // a string
      {
        if (prev_info == null || entries_different (prev_info^.type, info^.type))
        {
          uint2* q;
          int    len, i;

          tmpent = (RES_DIR_ENTRY *)exe_ptr(info^.type.ientry);
          tmpent->name_or_id = (exe_current_ptr() - p.resource.addr) | 0x80000000;

          len = strlen(info^.type.name^);
          size = (uint)(2 * (1 + len));
          probe (size);
          q = (uint2 *)exe_ptr(exe_current_ptr());
          *q++ = (uint2)len;
          for (i=0; i<len; i++)
            *q++ = (uint2)info^.type.name^[i];

           exec_advance_ptr (size);
        }
      }

      if (info^.id.name != null)   // a string
      {
        if (prev_info == null || entries_different (prev_info^.type, info^.type)
                              || entries_different (prev_info^.id,   info^.id))
        {
          uint2* q;
          int    len, i;

          tmpent = (RES_DIR_ENTRY *)exe_ptr(info^.id.ientry);
          tmpent->name_or_id = (exe_current_ptr() - p.resource.addr) | 0x80000000;

          len = strlen(info^.id.name^);
          size = (uint)(2 * (1 + len));
          probe (size);
          q = (uint2 *)exe_ptr(exe_current_ptr());
          *q++ = (uint2)len;
          for (i=0; i<len; i++)
            *q++ = (uint2)info^.id.name^[i];

          exec_advance_ptr (size);
        }
      }

      if (info^.lang.name != null)   // a string
      {
        {
          uint2* q;
          int    len, i;

          tmpent = (RES_DIR_ENTRY *)exe_ptr(info^.lang.ientry);
          tmpent->name_or_id = (exe_current_ptr() - p.resource.addr) | 0x80000000;

          len = strlen(info^.lang.name^);
          size = (uint)(2 * (1 + len));
          probe (size);
          q = (uint2 *)exe_ptr(exe_current_ptr());
          *q++ = (uint2)len;
          for (i=0; i<len; i++)
            *q++ = (uint2)info^.lang.name^[i];

          exec_advance_ptr (size);
        }
      }

      prev_info = info;

      mode = BT_LARGER;
    }


    // send resource nodes

    align_at (16);

    mode = BT_FIRST;
    for (;;)
    {
      rc = B5.retrieve_btree (res_tree, ref info, mode);
      if (rc != 0 && rc != BT_KEY_NOT_FOUND)
        fatal_compiler_error0 ("write_resources_in_exec_image()");

      if (rc == BT_KEY_NOT_FOUND)
        break;

      {
        RES_NODE *r;
        uint     inode;

        size = RES_NODE'size;
        probe (size);
        inode = exe_current_ptr();
        r = (RES_NODE *)exe_ptr(inode);
        clear *r;
        exec_advance_ptr (size);

        tmpent = (RES_DIR_ENTRY *)exe_ptr(info^.lang.ientry);
        tmpent->next = inode - p.resource.addr;

        info^.inode = inode;
      }

      mode = BT_LARGER;
    }


    // send resources

    mode = BT_FIRST;
    for (;;)
    {
      rc = B5.retrieve_btree (res_tree, ref info, mode);
      if (rc != 0 && rc != BT_KEY_NOT_FOUND)
        fatal_compiler_error0 ("write_resources_in_exec_image()");

      if (rc == BT_KEY_NOT_FOUND)
        break;

      {
        int      fd;
        RES_NODE *r;

        fd = open (info^.resource_filename^);
        if (fd < 0)
        {
          char msg[512];
          sprintf (out msg, "cannot open resource %.260s", info^.resource_filename^);
          resource_error (res_filename, msg, 0, 1);
        }

        size = (uint)lseek (fd, 0L, SEEK_END);
        lseek (fd, 0L, SEEK_SET);

        r = (RES_NODE *)exe_ptr(info^.inode);
        r->res_nva = exe_current_ptr();
        r->res_size = size;

        probe (size);
        
        
        if (read (fd, out exe_ptr(0)[exe_current_ptr():size]) != (int)size)
        {
          char msg[512];
          sprintf (out msg, "cannot read resource %.260s", info^.resource_filename^);
          resource_error (res_filename, msg, 0, 1);
        }
        exec_advance_ptr (size);

        close (fd);
      }

      mode = BT_LARGER;
    }
  }

  {
    ref PE32_PART3 p = *(PE32_PART3 *)exe_ptr(ofs_pe32_part3);
    p.resource.size = exe_current_ptr() - p.resource.addr;
  }
#end unsafe
}

/************************************************************************/
