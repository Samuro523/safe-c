
// elf.c

from std use bintree, calendar, console, crc, files, strings;
use ../exeout, ../blob, ../blob2, ../fixup, ../dbginfo, ../error;
use elf0;

//---------------------------------------------------------------------

const uint PAGE = 0x10000;   // 64 K
 // à partir d'Android 15 (disponible publiquement depuis le 16 octobre 2024),
 // AOSP prend en charge appareils configurés pour utiliser une taille de page de 16 Ko

//---------------------------------------------------------------------

Elf64_Ehdr                        g_hdr;

const int MAX_PROGRAM_HEADERS = 9;
Elf64_Phdr[MAX_PROGRAM_HEADERS]   g_ph;

const int MAX_SECTIONS_HEADERS = 23;
int                               g_sh_count;   // nb of entries in section header table
Elf64_Shdr[MAX_SECTIONS_HEADERS]  g_sh;

uint g_start_of_code_in_file;
uint g_start_of_code_in_memory;
uint g_size_of_code;

uint g_start_of_data_in_file;
uint g_start_of_data_in_memory;
uint g_size_of_data;

uint g_start_of_bss_in_memory;
uint g_size_of_bss;

uint g_start_of_dynamic_table_in_data_in_file;
uint g_start_of_dynamic_table_in_data_in_memory;
uint g_size_of_dynamic_table_in_data;

uint g_symbol_table_in_exe_file;
uint g_end_symbol_table_in_exe_file;

uint g_string_table_in_exe_file;
uint g_end_of_string_table_in_exe_file;
uint g_string_table_count;                // used for hash table

uint g_relocation_table_in_exe_file;
uint g_relocation_table_size;

uint g_gotplt_pos_in_file;
uint g_gotplt_address;
uint g_gotplt_size;

uint g_info_in_exe_file;
uint g_resource_offset_in_exe_file;
uint g_resource_offset_in_data_blob;
uint g_program_table_in_exe_file;
uint g_section_table_in_exe_file;

uint g_hash_in_exe_file;
uint g_end_hash_in_exe_file;

uint g_initialize_function_address;

//---------------------------------------------------------------------

BLOB g_blob_string_table;    // import and export symbol table
BLOB g_blob_dynamic_linking;
BLOB g_blob_section_name_string_table;

const int TEXT_SECTION_NR = 1;
int g_section_names_section_index;

//---------------------------------------------------------------------

void init_elf_header ()
{
  g_hdr.e_ident[0:4] = {0x7F, 0x45, 0x4C, 0x46};
  g_hdr.e_ident[4] = 2;  // 64 bit
  g_hdr.e_ident[5] = 1;  // LSB first
  g_hdr.e_ident[6] = 1;  // version 1

  g_hdr.e_type    = 3;     /* (2=exe, 3=shared lib) */
  g_hdr.e_machine = 0xB7;  /* Architecture (android EM_AARCH64 = B7) */

  g_hdr.e_version = 1;   /* Object file version (=1) */

//  g_hdr.e_flags = 0;    /* Processor-specific flags (=0) */

  g_hdr.e_ehsize = (uint2)Elf64_Ehdr'size;   /* ELF header size in bytes (=sizeof(Elf64_Ehdr) */
  assert Elf64_Ehdr'size == 64;

  g_hdr.e_phentsize = (uint2)Elf64_Phdr'size; /* Program header table entry size (=sizeof(Elf64_Phdr)) */
  assert Elf64_Phdr'size == 56;

  g_hdr.e_shentsize = (uint2)Elf64_Shdr'size;  /* Section header table entry size (=sizeof(Elf64_Shdr)) */
  assert Elf64_Shdr'size == 64;

//  g_hdr.e_entry   = 0;    /* Entry point virtual address (=0 for shared lib) */

  g_hdr.e_phoff = g_program_table_in_exe_file;
  g_hdr.e_phnum = (uint2)MAX_PROGRAM_HEADERS;  /* Program header table entry count (= 10) */

  g_hdr.e_shoff = g_section_table_in_exe_file;    /* Section header table file offset */
  g_hdr.e_shnum = (uint2)g_sh_count;    /* Section header table entry count  (= 22)*/

  g_hdr.e_shstrndx = (Elf64_Half)g_section_names_section_index; /* Section header string table index */
             // index of the section header table entry that contains the section names (= 21)  so last one
}

//---------------------------------------------------------------------

void init_program_headers ()
{
  int ph_count = 0;   // nb of entries in program header table

  {
    ref Elf64_Phdr ph = g_ph[ph_count++];

    ph.p_type = PT_PHDR;   // seg 0 : containing program header table itself
    ph.p_flags = PF_R;
    ph.p_offset = Elf64_Ehdr'size;   // in file
    ph.p_vaddr  = Elf64_Ehdr'size;   // in memory
    ph.p_paddr  = Elf64_Ehdr'size;
    ph.p_filesz = g_ph'size;     // size in file
    ph.p_memsz = g_ph'size;      // size in memory
    ph.p_align = 8;
  }

  {
    ref Elf64_Phdr ph = g_ph[ph_count++];

    ph.p_type = PT_LOAD;   // seg 1 : start of file, til executable code
    ph.p_flags = PF_R;
//    ph.p_offset = 0;   // in file
//    ph.p_vaddr  = 0;   // in memory
//    ph.p_paddr  = 0;
    ph.p_filesz = g_start_of_code_in_file;     // size in file
    ph.p_memsz = g_start_of_code_in_file;      // size in memory
    ph.p_align = PAGE;
  }

  {
    ref Elf64_Phdr ph = g_ph[ph_count++];

    ph.p_type = PT_LOAD;   // seg 2 : CODE
    ph.p_flags = PF_X;
    ph.p_offset = g_start_of_code_in_file;     // in file
    ph.p_vaddr  = g_start_of_code_in_memory;   // in memory (has its own page)
    ph.p_paddr  = g_start_of_code_in_memory;
    ph.p_filesz = g_size_of_code;    // size in file
    ph.p_memsz  = g_size_of_code;    // size in memory
    ph.p_align = PAGE;
  }

  {
    ref Elf64_Phdr ph = g_ph[ph_count++];

    ph.p_type = PT_LOAD;   // seg 3 : DATA
    ph.p_flags = PF_R + PF_W;  // must be writable, otherwise linker can't fill got.plt table. Will be set read-only later in .relro
    ph.p_offset = g_start_of_data_in_file;     // in file
    ph.p_vaddr  = g_start_of_data_in_memory;   // in memory (has its own page)
    ph.p_paddr  = g_start_of_data_in_memory;
    ph.p_filesz = g_size_of_data;     // size in file
    ph.p_memsz = g_size_of_data;      // size in memory
    ph.p_align = PAGE;
  }

  {
    ref Elf64_Phdr ph = g_ph[ph_count++];

assert ph_count == 5;  // if this assertion fails, we must adapt the library file exception.c, function log_heap() bss_addr=

    ph.p_type = PT_LOAD;   // seg 4 : BSS
    ph.p_flags = PF_R + PF_W;
    ph.p_offset = g_start_of_data_in_file + g_size_of_data;    // in file
    ph.p_vaddr  = g_start_of_bss_in_memory;   // in memory (has its own page)
    ph.p_paddr  = g_start_of_bss_in_memory;
    ph.p_filesz = 0;                          // size in file
    ph.p_memsz  = g_size_of_bss;              // size in memory
    ph.p_align  = PAGE;
  }

  {
    ref Elf64_Phdr ph = g_ph[ph_count++];

    ph.p_type   = PT_DYNAMIC;    // seg 5 : Dynamic linking information
    ph.p_flags  = PF_R;

    ph.p_offset = g_start_of_dynamic_table_in_data_in_file;     // in file
    ph.p_vaddr  = g_start_of_dynamic_table_in_data_in_memory;   // in memory
    ph.p_paddr  = g_start_of_dynamic_table_in_data_in_memory;
    ph.p_filesz = g_size_of_dynamic_table_in_data;     // size in file
    ph.p_memsz  = g_size_of_dynamic_table_in_data;     // size in memory
    ph.p_align  = 8;
  }

  // location and size of a segment which may be made read-only after relocations have been processed.
  {
    ref Elf64_Phdr ph = g_ph[ph_count++];

    ph.p_type   = PT_GNU_RELRO;    // seg 6 : read-only after relocation
    ph.p_flags  = PF_R;

    ph.p_offset = g_start_of_data_in_file;     // in file
    ph.p_vaddr  = g_start_of_data_in_memory;   // in memory (has its own page)
    ph.p_paddr  = g_start_of_data_in_memory;
    ph.p_filesz = g_size_of_data;      // size in file
    ph.p_memsz  = g_size_of_data;      // size in memory
    ph.p_align  = 1;
  }

  // The p_flags member specifies the permissions on the segment containing
  // the stack and is used to indicate wether the stack should be executable.
  // The absense of this header indicates that the stack will be executable.
  {
    ref Elf64_Phdr ph = g_ph[ph_count++];

    ph.p_type   = PT_GNU_STACK;    // seg 7 : indicates stack should be non-exec
    ph.p_flags  = PF_R;
  }

  {
    ref Elf64_Phdr ph = g_ph[ph_count++];

    ph.p_type   = PT_NOTE;
    ph.p_flags  = PF_R;

    ph.p_offset = g_info_in_exe_file;     // in file
    ph.p_vaddr  = g_info_in_exe_file;   // in memory (has its own page)
    ph.p_paddr  = g_info_in_exe_file;
    ph.p_filesz = 64;      // size in file
    ph.p_memsz  = 64;      // size in memory
    ph.p_align  = 1;
  }

  assert ph_count == MAX_PROGRAM_HEADERS;
}

//---------------------------------------------------------------------

uint store_string_in_section_name_string_table (string name)
{
  int i, idx;

  idx = blob_index (g_blob_section_name_string_table);

  for (i=0; i<name'length; i++)
    blob_put_byte (ref g_blob_section_name_string_table, (byte)name[i]);
  blob_put_byte (ref g_blob_section_name_string_table, 0);

  return (uint)idx;
}

//---------------------------------------------------------------------

void elf_write_section_table ()
{
  int dynstr_index, dynsym_index, got_plt_index;

  blob_create (out g_blob_section_name_string_table);
  blob_put_byte (ref g_blob_section_name_string_table, 0);

  {
    g_sh_count++;   // first entry is SHT_NULL
  }

  assert g_sh_count == TEXT_SECTION_NR;
  {
    ref Elf64_Shdr sh = g_sh[g_sh_count++];
    sh.sh_name   = store_string_in_section_name_string_table (".text");
    sh.sh_type   = SHT_PROGBITS;
    sh.sh_flags  = SHF_ALLOC | SHF_EXECINSTR;
    sh.sh_addr   = g_start_of_code_in_memory;
    sh.sh_offset = g_start_of_code_in_file;
    sh.sh_size   = g_size_of_code;
    sh.sh_link      = 0;  /* Link to another section */
    sh.sh_info      = 0;  /* Additional section information (low=type, high=bind) */
    sh.sh_addralign = 4;  /* Section alignment. This field must be a power of two. */
    sh.sh_entsize   = 0;  /* Entry size if section holds table. Otherwise zero. */
  }

  {
    ref Elf64_Shdr sh = g_sh[g_sh_count++];
    sh.sh_name   = store_string_in_section_name_string_table (".data.rel.ro");
    sh.sh_type   = SHT_PROGBITS;
    sh.sh_flags  = SHF_ALLOC | SHF_WRITE;
    sh.sh_addr   = g_start_of_data_in_memory;
    sh.sh_offset = g_start_of_data_in_file;
    sh.sh_size   = g_size_of_data;
    sh.sh_link      = 0;  /* Link to another section */
    sh.sh_info      = 0;  /* Additional section information (low=type, high=bind) */
    sh.sh_addralign = 8;  /* Section alignment. This field must be a power of two. */
    sh.sh_entsize   = 0;  /* Entry size if section holds table. Otherwise zero. */
  }

  {
    ref Elf64_Shdr sh = g_sh[g_sh_count++];
    sh.sh_name   = store_string_in_section_name_string_table (".bss");
    sh.sh_type   = SHT_NOBITS;
    sh.sh_flags  = SHF_WRITE | SHF_ALLOC;
    sh.sh_addr   = g_start_of_bss_in_memory;
    sh.sh_offset = g_start_of_data_in_file + g_size_of_data;
    sh.sh_size   = 0;     /* Section size in the file, in bytes. May be 0. */
    sh.sh_link      = 0;  /* Link to another section */
    sh.sh_info      = 0;  /* Additional section information (low=type, high=bind) */
    sh.sh_addralign = 8;  /* Section alignment. This field must be a power of two. */
    sh.sh_entsize   = 4;  /* Entry size if section holds table. Otherwise zero. */
  }

  dynstr_index = g_sh_count;
  {
    ref Elf64_Shdr sh = g_sh[g_sh_count++];
    sh.sh_name   = store_string_in_section_name_string_table (".dynstr");
    sh.sh_type   = SHT_STRTAB;
    sh.sh_flags  = SHF_ALLOC;
    sh.sh_addr   = g_string_table_in_exe_file;
    sh.sh_offset = g_string_table_in_exe_file;
    sh.sh_size   = g_end_of_string_table_in_exe_file - g_string_table_in_exe_file;     /* Section size in the file, in bytes. May be 0. */
    sh.sh_link      = 0;  /* Link to another section */
    sh.sh_info      = 0;  /* Additional section information (low=type, high=bind) */
    sh.sh_addralign = 1;  /* Section alignment. This field must be a power of two. */
    sh.sh_entsize   = 0;  /* Entry size if section holds table. Otherwise zero. */
  }

  {
    ref Elf64_Shdr sh = g_sh[g_sh_count++];
    sh.sh_name   = store_string_in_section_name_string_table (".dynamic");  // table with "DT_NEEDED"
    sh.sh_type   = SHT_DYNAMIC;
    sh.sh_flags  = SHF_WRITE | SHF_ALLOC;
    sh.sh_addr   = g_start_of_dynamic_table_in_data_in_memory;
    sh.sh_offset = g_start_of_dynamic_table_in_data_in_file;
    sh.sh_size   = g_size_of_dynamic_table_in_data;     /* Section size in the file, in bytes. May be 0. */
    sh.sh_link   = (Elf64_Word)dynstr_index;  /* Link to another section */
//  sh.sh_info      = 0;  /* Additional section information (low=type, high=bind) */
    sh.sh_addralign = 8;  /* Section alignment. This field must be a power of two. */
    sh.sh_entsize   = Elf64_Dyn'size;  /* Entry size if section holds table. Otherwise zero. */
  }

  dynsym_index = g_sh_count;
  {
    ref Elf64_Shdr sh = g_sh[g_sh_count++];
    sh.sh_name   = store_string_in_section_name_string_table (".dynsym");
    sh.sh_type   = SHT_DYNSYM;
    sh.sh_flags  = SHF_ALLOC;
    sh.sh_addr   = g_symbol_table_in_exe_file;  // before .text
    sh.sh_offset = g_symbol_table_in_exe_file;
    sh.sh_size   = g_end_symbol_table_in_exe_file - g_symbol_table_in_exe_file;   /* Section size in the file, in bytes. May be 0. */
    sh.sh_link      = (Elf64_Word)dynstr_index;  /* Link to another section */
    sh.sh_info      = 1;  /* Additional section information (low=type, high=bind) first global symbol entry has index 1 (after initial 0) */
    sh.sh_addralign = 8;  /* Section alignment. This field must be a power of two. */
    sh.sh_entsize   = Elf64_Sym'size;  /* Entry size if section holds table. Otherwise zero. */
  }

  got_plt_index = g_sh_count;
  {
    ref Elf64_Shdr sh = g_sh[g_sh_count++];
    sh.sh_name   = store_string_in_section_name_string_table (".got.plt");
    sh.sh_type   = SHT_PROGBITS;
    sh.sh_flags  = SHF_WRITE | SHF_ALLOC;
    sh.sh_addr   = g_gotplt_address;  // before .text
    sh.sh_offset = g_gotplt_pos_in_file;
    sh.sh_size   = g_gotplt_size;   /* Section size in the file, in bytes. May be 0. */
    sh.sh_link      = 0;  /* Link to another section */
    sh.sh_info      = 0;  /* Additional section information (low=type, high=bind) first entry has index 1 (after initial 0) */
    sh.sh_addralign = 8;  /* Section alignment. This field must be a power of two. */
    sh.sh_entsize   = 0;  /* Entry size if section holds table. Otherwise zero. */
  }

  {
    ref Elf64_Shdr sh = g_sh[g_sh_count++];
    sh.sh_name   = store_string_in_section_name_string_table (".rela.plt");  // ".rela.plt" SHT_RELA  Relocation entries with addends
    sh.sh_type   = SHT_RELA;
    sh.sh_flags  = SHF_ALLOC | SHF_INFO_LINK;
    sh.sh_addr   = g_relocation_table_in_exe_file;  // before .text
    sh.sh_offset = g_relocation_table_in_exe_file;
    sh.sh_size   = g_relocation_table_size;   /* Section size in the file, in bytes. May be 0. */
    sh.sh_link      = (Elf64_Word)dynsym_index;  /* Link to another section */
    sh.sh_info      = (Elf64_Word)got_plt_index;  /* Additional section information (low=type, high=bind) first entry has index 1 (after initial 0) */
    sh.sh_addralign = 8;  /* Section alignment. This field must be a power of two. */
    sh.sh_entsize   = Elf64_Rela'size;  /* Entry size if section holds table. Otherwise zero. */
  }

  g_section_names_section_index = g_sh_count;
  {
    ref Elf64_Shdr sh = g_sh[g_sh_count++];
    sh.sh_name   = store_string_in_section_name_string_table (".hash");
    sh.sh_type   = SHT_HASH;
    sh.sh_flags  = SHF_ALLOC;
    sh.sh_addr   = g_hash_in_exe_file;
    sh.sh_offset = g_hash_in_exe_file;
    sh.sh_size   = g_end_hash_in_exe_file - g_hash_in_exe_file;
    sh.sh_link      = (Elf64_Word)dynsym_index;  /* Link to another section */
    sh.sh_info      = 0;  /* Additional section information (low=type, high=bind) */
    sh.sh_addralign = 8;  /* Section alignment. This field must be a power of two. */
    sh.sh_entsize   = 0;  /* Entry size if section holds table. Otherwise zero. */
  }


  {
    ref Elf64_Shdr sh = g_sh[g_sh_count++];
    sh.sh_name   = store_string_in_section_name_string_table (".note.datetime");  // special string table giving section names
    sh.sh_type   = SHT_NOTE;
    sh.sh_flags  = SHF_ALLOC;
    sh.sh_addr   = g_info_in_exe_file;
    sh.sh_offset = g_info_in_exe_file;
    sh.sh_size   = 64;

    sh.sh_link      = 0;  /* Link to another section */
    sh.sh_info      = 0;  /* Additional section information (low=type, high=bind) */
    sh.sh_addralign = 1;  /* Section alignment. This field must be a power of two. */
    sh.sh_entsize   = 0;  /* Entry size if section holds table. Otherwise zero. */
  }


  g_section_names_section_index = g_sh_count;
  {
    ref Elf64_Shdr sh = g_sh[g_sh_count++];
    sh.sh_name   = store_string_in_section_name_string_table (".shstrtab");  // special string table giving section names
  (void)store_string_in_section_name_string_table ("");   // final zero byte
    sh.sh_type   = SHT_STRTAB;
    sh.sh_flags  = 0;
    sh.sh_addr   = 0;
    sh.sh_offset = exe_current_ptr();
    sh.sh_size   = blob_size (g_blob_section_name_string_table) - 1;   /* Section size in the file, in bytes. May be 0. */
                                     // suppress final zero byte
    sh.sh_link      = 0;  /* Link to another section */
    sh.sh_info      = 0;  /* Additional section information (low=type, high=bind) */
    sh.sh_addralign = 1;  /* Section alignment. This field must be a power of two. */
    sh.sh_entsize   = 0;  /* Entry size if section holds table. Otherwise zero. */
  }

#begin unsafe
  exe_write_byte_sequence (blob_ptr(g_blob_section_name_string_table)[0 : blob_size(g_blob_section_name_string_table)], align => 1);
#end unsafe

  g_section_table_in_exe_file = exe_write_byte_sequence (g_sh[0 : g_sh_count], align => 8);
}

//---------------------------------------------------------------------

public
void elf_init_image ()
{
  exeout.exe_init_image (initial_size => 64);

  exe_write_byte_sequence (g_hdr, align => 1);

  g_program_table_in_exe_file = exe_current_ptr ();
  exe_write_byte_sequence (g_ph, align => 1);

  g_info_in_exe_file = exe_current_ptr ();

  // if this assertion fails, we must adapt the library file exception.c (search for 0x238)
  assert g_info_in_exe_file == 0x238;

  {
    DATE_TIME now;
    char      str[64];
    long      t;

    get_gmt_datetime (out now);

    // 28 chars "compiled 00/00/0000 00:00:00" + 2 nul = 30 chars
    sprintf (out str, "compiled %02d/%02d/%04d %02d:%02d:%02d", now.day, now.month, now.year, now.hour, now.min, now.sec);

    g_resource_offset_in_exe_file = g_info_in_exe_file + 32;
    // str[32:4] are reserved for offset to resource dictionary

    datetime_to_clock (now, 0, out t);
    str[56:8]'byte = t'byte;

    exe_write_byte_sequence (str, align => 1);
  }
}

//---------------------------------------------------------------------

uint store_string_in_table (string name)
{
  int i, len, idx;

  idx = blob_index (g_blob_string_table);

  len = strlen(name);
  for (i=0; i<len; i++)
    blob_put_byte (ref g_blob_string_table, (byte)name[i]);
  blob_put_byte (ref g_blob_string_table, 0);

  g_string_table_count++;

  return (uint)idx;
}

//---------------------------------------------------------------------

uint wstore_string_in_table (wstring name)
{
  int i, len, idx;

  idx = blob_index (g_blob_string_table);

  len = wstrlen(name);
  for (i=0; i<len; i++)
    blob_put_byte (ref g_blob_string_table, (byte)name[i]);
  blob_put_byte (ref g_blob_string_table, 0);

  g_string_table_count++;

  return (uint)idx;
}

//---------------------------------------------------------------------

void treat_so_name2 (int str_table_index)
{
  Elf64_Dyn dyn = {d_tag => DT_NEEDED, d_val => str_table_index};
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);
}

//---------------------------------------------------------------------

// generate linking info for prefix it to data

void generate_dynamic_linking_info (uint so_index_in_string_table)
{
  Elf64_Dyn dyn;

  blob_create (out g_blob_dynamic_linking);

  // loop on all import library names, like "libc.so", "LIBC.libm.so", "libdl.so"
  import_loop_so_names2 (treat_so_name2);

  dyn = {d_tag => DT_SONAME, d_val => so_index_in_string_table};  // 'libmygame2.so'
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_FLAGS, d_val => DF_BIND_NOW};     // No lazy binding for this object
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_FLAGS_1, d_val => DF_1_NOW};     // resolve all symbols now, not lazilly later
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_RELA, d_val => g_relocation_table_in_exe_file};  // address of relocation table Elf64_Rela
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_RELASZ, d_val => g_relocation_table_size};
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_RELAENT, d_val => Elf64_Rela'size};
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  // seems optional
  dyn = {d_tag => DT_PLTRELSZ, d_val => (uint)nb_of_imported_funcs() * Elf64_Rela'size};
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_PLTGOT, d_val => (int)g_gotplt_address};
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  // type of relocation entry to which the PLT refers
  dyn = {d_tag => DT_PLTREL, d_val => DT_RELA};     // DT_RELA 7   or  DT_REL 17 (not supported by android)
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_SYMTAB, d_val => g_symbol_table_in_exe_file};
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_SYMENT, d_val => Elf64_Sym'size};
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_STRTAB, d_val => g_string_table_in_exe_file};
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_STRSZ, d_val => g_end_of_string_table_in_exe_file - g_string_table_in_exe_file}; // size of string table
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_HASH, d_val => g_hash_in_exe_file};
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_INIT, d_val => g_initialize_function_address};  // init function to initialize all global variables
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);

  dyn = {d_tag => DT_NULL, d_val => 0};    // final entry
  blob_put_sequence (ref g_blob_dynamic_linking, dyn);
}

//---------------------------------------------------------------------

void treat_so_names1 (string so_name, out int str_table_index)
{
  str_table_index = (int)store_string_in_table (so_name);
  g_string_table_count--;  // should probably not be in hash table (crashes otherwise)
}

//---------------------------------------------------------------------

void treat_import_function1 (string func_name, int seqnr, int slot_nr)
{
  Elf64_Sym sym;

  _unused seqnr, slot_nr;

  clear sym;
  sym.st_name = (Elf64_Word)store_string_in_table (func_name);
  sym.st_info = (byte)((STB_GLOBAL << 4) + STT_FUNC);   // global function
//  sym.st_shndx = 0;  // must be zero
//  sym.st_size = 0;  // must be zero
  exe_write_byte_sequence (sym, align => 1);
}

// ------------------------------------------------

void treat_export_function1 (wstring name)
{
  Elf64_Sym sym;
  uint      ofs;

  clear sym;
  sym.st_name = (Elf64_Word)wstore_string_in_table (name);
  sym.st_info = (byte)((STB_GLOBAL << 4) + STT_FUNC);   // global function
  sym.st_shndx = (Elf64_Section)TEXT_SECTION_NR;
//  sym.st_value = 0;  // gets filled later
  sym.st_size = 8;
  ofs = exe_write_byte_sequence (sym, align => 1);

#begin unsafe
  export_set_fix_offset_in_file (name                 => name,
                                 fixup_offset_in_file => ofs + ((byte*)&sym.st_value - (byte*)&sym));
#end unsafe
}

// ------------------------------------------------

// returns index of exe_simple_filename in g_blob_string_table

uint write_elf_symbol_table (string exe_simple_filename)
{
  uint so_index_in_string_table;


  blob_create (out g_blob_string_table);

  blob_put_byte (ref g_blob_string_table, 0);
  g_string_table_count = 1;      // used for hash table

  // The specific symbols included in the DT_HASH hash table are all the externally visible symbols,
  // which are typically found in the .dynsym section.


  g_symbol_table_in_exe_file = exeout.exe_current_ptr ();

  {
    Elf64_Sym empty;
    clear empty;
    exe_write_byte_sequence (empty, align => 1);
  }

  // import functions
  fixup.import_loop_import_funcs1 (treat_import_function1);

  // export functions
  fixup.loop1_on_export_functions (treat_export_function1);

  g_end_symbol_table_in_exe_file = exeout.exe_current_ptr ();



  // import so
  fixup.import_loop_so_names1 (treat_so_names1);   // allocate strings of imported libraries in string table only

  // so name
  so_index_in_string_table = store_string_in_table (exe_simple_filename);
  g_string_table_count--;  // should probably not be in hash table (crashes otherwise)

  blob_put_byte (ref g_blob_string_table, 0);   // close string table (OPTIONAL)


  return so_index_in_string_table;
}

//---------------------------------------------------------------------

void operate_export_function2 (uint fixup_offset_in_file, uint code_ip)
{
  uint saved_pos = exeout.exe_current_ptr ();

  exe_set_current_ptr (fixup_offset_in_file);
  exe_write_int8 (g_start_of_code_in_memory + code_ip);

  exe_set_current_ptr (saved_pos);
}

//---------------------------------------------------------------------

#begin unsafe
uint elf_hash (byte* name0)
{
  byte* name = name0;
  uint h = 0, g;
  for (; *name != 0; name++)
  {
    h = (h << 4) + *name;
    g = h & 0xf0000000;
    if (g != 0)
      h ^= g >> 24;
    h &= ~g;
  }
  return h;
}
#end unsafe

//---------------------------------------------------------------------

void write_hash_table ()
{
#begin unsafe
  byte* ptr = blob_ptr (g_blob_string_table);
  byte* ps;
  uint* tbuckets, tchains;
  uint  hvalue, chain_count, nb_buckets;

  nb_buckets = g_string_table_count;  // arbitrary

  exe_write_int4 ((int)nb_buckets);              // nb buckets
  exe_write_int4 ((int)g_string_table_count);    // nb chains

  probe (uint'size * (nb_buckets + g_string_table_count));   // make sure bucket and chain tables are allocated

  tbuckets = (uint*)exe_ptr(exe_current_ptr());
  exec_advance_ptr (uint'size * nb_buckets);

  tchains = (uint*)exe_ptr(exe_current_ptr());
  exec_advance_ptr (uint'size * g_string_table_count);

  chain_count = 0;

  while (chain_count < g_string_table_count)
  {
    ps = ptr;

    while (*ptr != 0)
      ptr++;

    hvalue = elf_hash (ps) % nb_buckets;

    tchains[chain_count] = tbuckets [hvalue];
    tbuckets [hvalue] = chain_count;
    chain_count++;

    ptr++;
  }
#end unsafe
}

//---------------------------------------------------------------------

void treat_import_function2_for_relocation_table (string func_name, int seqnr, int slot_nr)
{
  _unused func_name;
#begin unsafe
  {
    Elf64_Rela rel;

    clear rel;
    rel.r_offset  = slot_nr * 8;            // got.plt table entry
    rel.r_type    = R_AARCH64_JUMP_SLOT;    // relocation type
    rel.r_sym     = (Elf64_Word)seqnr + 1;  // symbol index
    rel.r_addend  = 0;

    exe_write_byte_sequence (rel, align => 1);
  }
#end unsafe
}

//---------------------------------------------------------------------

void add_base_address_of_vector_table_entries ()
{
#begin unsafe
  Elf64_Rela* p = (Elf64_Rela*)exe_ptr(g_relocation_table_in_exe_file);
  int         i, count;

  count = nb_of_imported_funcs();
  for (i=0; i<count; i++)
  {
    p->r_offset += g_gotplt_address;
    p++;
  }
#end unsafe
}

//---------------------------------------------------------------------

public
void elf_terminate_image (    string exe_simple_filename,
                              uint4  bss_size,
                              int    func_label_to_init_constants,
                          out uint   out_start_of_bss_mem)
{
  uint so_index_in_string_table;
  uint gotplt_size_m16;
  uint exe_pos_relocation_table;
  uint start_of_data_blob_mem;
  uint pos_end_of_exe;

// ********** so, after writing empty g_hdr, g_ph ... **********

// ********** symbol table : list of import/export functions **********

  // returns index of exe_simple_filename in g_blob_string_table
  so_index_in_string_table = write_elf_symbol_table (exe_simple_filename);  // create string table and write symbol table in exe file

// ********** string table : exec name, "libc.so", "LIBC.libm.so", "libdl.so", import & export strings **********

  g_string_table_in_exe_file = exeout.exe_current_ptr ();
#begin unsafe
  exe_write_byte_sequence (blob_ptr(g_blob_string_table)[0 : blob_size(g_blob_string_table)], align => 1);
#end unsafe
  g_end_of_string_table_in_exe_file = exeout.exe_current_ptr ();

// ********** hash table **********

  exeout.align_at (mod => 4);
  g_hash_in_exe_file = exeout.exe_current_ptr ();
  write_hash_table ();  // read string table and compute hash structure
  g_end_hash_in_exe_file = exeout.exe_current_ptr ();

// ********** relocation table **********

  exeout.align_at (mod => 16);
  g_relocation_table_in_exe_file = exeout.exe_current_ptr ();

  fixup.import_loop_import_funcs1 (treat_import_function2_for_relocation_table);  // write R_AARCH64_JUMP_SLOT entries in exe

  exe_pos_relocation_table = exeout.exe_current_ptr ();
  {
    int  nb_reloc;
    uint size;
    fixup.get_elf_relocation_table_count (out nb_reloc);
    size = (uint)nb_reloc * Elf64_Rela'size;              // reserve space for relocations of type R_AARCH64_RELATIVE
    exeout.probe  (size);
    exeout.exec_advance_ptr (size);
  }

  g_relocation_table_size = exeout.exe_current_ptr () - g_relocation_table_in_exe_file;

// ********** code **********

  exeout.align_at (mod => 16);
  g_start_of_code_in_file = exeout.exe_current_ptr ();
  g_start_of_code_in_memory = g_start_of_code_in_file + PAGE;   // code segment has own page
  g_size_of_code = (uint)blob_size(g_blob_code);
#begin unsafe
  exe_write_byte_sequence (blob_ptr(g_blob_code)[0 : g_size_of_code], align => 1); // will be rewritten below
#end unsafe

  g_initialize_function_address = g_start_of_code_in_memory + function_get_ip (func_label_to_init_constants);

// ********** data (  gotplt + filler + dynamic_linking_info + data ) **********

  exeout.align_at (mod => 16);
  g_start_of_data_in_file = exeout.exe_current_ptr ();
  g_start_of_data_in_memory = g_start_of_data_in_file + 2*PAGE;   // data segment has own page

  g_gotplt_pos_in_file = g_start_of_data_in_file;
  g_gotplt_address = g_start_of_data_in_memory;
  g_gotplt_size    = 8 * (uint)nb_of_imported_funcs();   // 8-byte vector per imported function
  gotplt_size_m16 = (g_gotplt_size + 15) & (uint'max - 15);  // align at M16

  // generate dynamic linking info (entries Elf64_Dyn) (exec name, imported libraries, various options)
  generate_dynamic_linking_info (so_index_in_string_table => so_index_in_string_table);

  g_size_of_data = gotplt_size_m16 + (uint)(blob_size(g_blob_dynamic_linking) + blob_size(g_blob_data));

  // reserve got/plt table
  probe (gotplt_size_m16);
  exec_advance_ptr (gotplt_size_m16);   // multiple of 8 bytes, rounded up to multiple of 16 bytes


  // dynamic linking table (must be at multiple of 8 bytes)
  g_start_of_dynamic_table_in_data_in_file = exeout.exe_current_ptr ();
  g_start_of_dynamic_table_in_data_in_memory = g_start_of_dynamic_table_in_data_in_file + 2*PAGE;
  g_size_of_dynamic_table_in_data = (uint)blob_size(g_blob_dynamic_linking);
#begin unsafe
  // multiple of 16 bytes
  exe_write_byte_sequence (blob_ptr(g_blob_dynamic_linking)[0 : blob_size(g_blob_dynamic_linking)], align => 1);
#end unsafe

  // data (at multiple of 16 bytes)
#begin unsafe
  start_of_data_blob_mem = exeout.exe_current_ptr () + 2*PAGE;

  {
    ELF_REL[]^ elf_table;
    int        elf_table_count, i;
    byte*      pdata = blob_ptr (g_blob_data);
    byte*      pexe = exe_ptr(0);

    get_elf_relocation_table (out elf_table, out elf_table_count);

    {
      ref ELF_REL[] tab = elf_table^[0 : elf_table_count];
      for (i=0; i<tab'length; i++)
      {
        ref ELF_REL e = tab[i];
        int address;

        // load address
        address'byte = pdata[e.fill_position:4];

        if (e.code_blob)
          address += (int)g_start_of_code_in_memory;   // jump table
        else
          address += (int)start_of_data_blob_mem;      // constant jagged reference

        // store entry in elf relocation table
        {
          Elf64_Rela rel;

          clear rel;
          rel.r_offset = (Elf64_Addr)(start_of_data_blob_mem + (uint)e.fill_position);
          rel.r_type   = R_AARCH64_RELATIVE;     // relocation type
          rel.r_sym    = 0;
          rel.r_addend = address;

          pexe[exe_pos_relocation_table + (uint)i*Elf64_Rela'size : rel'size] = rel'byte;
        }
      }
    }
  }

  exe_write_byte_sequence (blob_ptr(g_blob_data)[0 : blob_size(g_blob_data)], align => 1);
#end unsafe

  g_start_of_bss_in_memory = (uint)((int)(exeout.exe_current_ptr () + 3*PAGE) & -(int)PAGE);   // bss segment aligned at page start

  // in elf, fix export table
  loop2_on_export_functions (operate_export_function2);

  add_base_address_of_vector_table_entries ();

  g_size_of_bss = bss_size;

  fixup.backfill_globals_pools_references_in_code (start_of_code_mem      => g_start_of_code_in_memory,
                                                   start_of_data_blob_mem => start_of_data_blob_mem,
                                                   start_of_bss_mem       => g_start_of_bss_in_memory,
                                                   got_plt_base_address   => g_gotplt_address);
// ********** sections **********

  elf_write_section_table ();

  pos_end_of_exe = exeout.exe_current_ptr ();


// ********** fix resource ptr **********

  if (g_resource_offset_in_data_blob != 0)
  {
    exe_set_current_ptr (g_resource_offset_in_exe_file);
    exe_write_int4 ((int4)(start_of_data_blob_mem + g_resource_offset_in_data_blob));
  }


// ********** rewrite code **********

  exe_set_current_ptr (g_start_of_code_in_file);
#begin unsafe
  exe_write_byte_sequence (blob_ptr(g_blob_code)[0 : g_size_of_code], align => 1); // second write of code
#end unsafe

  dbginfo . move_all_ip (rip_offset => (int)g_start_of_code_in_memory);


  // rewrite headers

  exe_set_current_ptr (0);

  init_program_headers ();
  init_elf_header ();

  exe_write_byte_sequence (g_hdr, align => 1);

  exe_set_current_ptr (g_program_table_in_exe_file);
  exe_write_byte_sequence (g_ph,  align => 1);


  exe_set_current_ptr (pos_end_of_exe);

  out_start_of_bss_mem = g_start_of_bss_in_memory;
}

//---------------------------------------------------------------------

public void print_crc ()
{
  uint crc, ofs;

  // compute crc of executable, but skip 64 bytes
  ofs = g_info_in_exe_file;

  crc = 0;
#begin unsafe
  update_crc (ref crc, exe_ptr(0)[0:ofs]);
  update_crc (ref crc, exe_ptr(0)[ofs + 64 : exe_current_ptr() - (ofs + 64)]);
#end unsafe

  printf ("CRC=%08x CODE=%u bytes DATA=%u bytes BSS=%u bytes\n",
         crc,
         g_size_of_code,
         g_size_of_data,
         g_size_of_bss);
}

//---------------------------------------------------------------------

package RESOURCE_STRUCTS

  struct RES_INFO
  {
    string^  namestr;
    string^  typstr;
    string^  resource_filename;
  }

  package BT5 = new BALANCED_BINARY_TREE (ELEMENT => RES_INFO, USER_INFO => bool);

  BT5.BINARY_TREE g_res_tree;             // tree of RES_INFO
  bool            g_res_tree_created;
  uint4           g_nb_resources;
  int             g_pos_catalog_entry;

  // returns -1 if same resource (namestr, typstr) is stored twice
  int insert_resource_in_tree (string namestr, string typstr, string filename);

end RESOURCE_STRUCTS;

//---------------------------------------------------------------------

string^ new_string (string s)
{
  return new string ' (s[0 : strlen(s)]);
}

//---------------------------------------------------------------------

package body RESOURCE_STRUCTS

  int cmp_res_info (bool^ bf, RES_INFO a, RES_INFO b)
  {
    int cmp = stricmp (a.namestr^, b.namestr^);
    _unused bf;
    if (cmp != 0)
      return cmp;
    return stricmp (a.typstr^, b.typstr^);
  }

  // returns -1 if same resource (namestr, typstr) is stored twice
  public int insert_resource_in_tree (string namestr, string typstr, string filename)
  {
    RES_INFO r;
    int      rc;

    if (!g_res_tree_created)
    {
      bool^ b = null;
      BT5.create_btree (out g_res_tree, b, cmp_res_info);
      g_res_tree_created = true;
    }

    clear r;
    r.namestr           = new_string (namestr);
    r.typstr            = new_string (typstr);
    r.resource_filename = new_string (filename);

    rc = BT5.insert_btree (ref g_res_tree, r);
    if (rc == BT_DUPLICATE_KEY)
      return -1;
    if (rc != 0)
      fatal_out_of_memory_error ("insert_resource_in_tree(a)");

    g_nb_resources++;
    return 0;
  }

end RESOURCE_STRUCTS;

//---------------------------------------------------------------------

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

//---------------------------------------------------------------------

int write_resource (bool^ user, RES_INFO data)
{
  int  pos_name, pos_typ, pos_data;
  int  fd;
  uint size;

  _unused user;

  pos_name = blob_index (g_blob_data) - g_pos_catalog_entry;
  blob_put_sequence (ref g_blob_data, seq => data.namestr^);
  blob_put_byte (ref g_blob_data, value => 0);

  pos_typ = blob_index (g_blob_data) - g_pos_catalog_entry;
  blob_put_sequence (ref g_blob_data, seq => data.typstr^);
  blob_put_byte (ref g_blob_data, value => 0);

  blob_align (ref g_blob_data, mod => 16);
  pos_data = blob_index (g_blob_data) - g_pos_catalog_entry;

  fd = open (data.resource_filename^);
  if (fd < 0)
  {
    char msg[512];
    sprintf (out msg, "cannot open resource %.260s", data.resource_filename^);
    resource_error ("", msg, 0, 1);
  }

  size = (uint)lseek (fd, 0L, SEEK_END);
  lseek (fd, 0L, SEEK_SET);

  blob_insert_bytes (ref blob     => g_blob_data,
                         pos      => blob_index (g_blob_data),
                         nb_bytes => (int)size);

#begin unsafe

  if (read (fd, out blob_ptr(g_blob_data)[blob_index (g_blob_data):size]) != (int)size)
  {
    char msg[512];
    sprintf (out msg, "cannot read resource %.260s", data.resource_filename^);
    resource_error ("", msg, 0, 1);
  }

  blob_set_index (ref g_blob_data, blob_index (g_blob_data) + (int)size);

  close (fd);

  // pointer relative to start of catalog entry
  blob_ptr(g_blob_data)[g_pos_catalog_entry   :4] = pos_name'byte;
  blob_ptr(g_blob_data)[g_pos_catalog_entry+ 4:4] = pos_typ'byte;
  blob_ptr(g_blob_data)[g_pos_catalog_entry+ 8:4] = pos_data'byte;
  blob_ptr(g_blob_data)[g_pos_catalog_entry+12:4] = size'byte;

  g_pos_catalog_entry += 16;
  return 0;

#end unsafe
}

//---------------------------------------------------------------------

public void write_resources_in_data_blob ()
{
  if (!g_res_tree_created)
    return;

  blob_align (ref g_blob_data, mod => 16);

  g_resource_offset_in_data_blob = (uint)blob_index (g_blob_data);
//4 <count>
  blob_put_uint4 (ref g_blob_data, value => g_nb_resources);

//4 <name str>  (pointer relative to this catalog entry)
//4 <typ str>   (")
//4 <addr >     (")
//4 <size >

  g_pos_catalog_entry = blob_index (g_blob_data);

  blob_insert_bytes (ref blob     => g_blob_data,
                         pos      => blob_index (g_blob_data),
                         nb_bytes => (int)(g_nb_resources * 16));

  blob_set_index (ref g_blob_data, blob_index (g_blob_data) + (int)(g_nb_resources * 16));

  assert BT5.traverse_btree (g_res_tree, write_resource, order => +1) == 0;

  blob_align (ref g_blob_data, mod => 16);
}

//---------------------------------------------------------------------
