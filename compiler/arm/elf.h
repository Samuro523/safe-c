
// elf.h

void elf_init_image ();

void elf_terminate_image (    string exe_simple_filename,
                              uint4  bss_size,
                              int    func_label_to_init_constants,
                          out uint   out_start_of_bss_mem);

void print_crc ();

void parse_resource_file (string current_dir, string res_filename);
void write_resources_in_data_blob ();

/*
 structure of ELF in memory :

. elf header
. 8 program headers

. dynamic linker symbol table
  . begins with 24 zero bytes
  . import and export symbols
      symbol 4 : st_name = 5A "exit"  12 = GLOBAL, FUNC
      symbol 5 : st_name = 2F "export_test" 12 = GLOBAL, FUNC ; section 12 (text)

. string table  (g_blob_string_table)
  . executable name
  . "libc.so", "LIBC.libm.so", "libdl.so"
  . "exit" "export_test"

. hash_table

. relocation_table

    typedef struct
    {
      Elf64_Addr    r_offset;   Address
      Elf64_Xword   r_info;     Relocation type and symbol index
      Elf64_Sxword  r_addend;   Addend
    } Elf64_Rela;

    00000510 | 08 28 00 00 00 00 00 00  (2808) typ 1027 sym 0  (171C)  START OF CODE + 18
               03 04 00 00 00 00 00 00
    00000520 | 1C 17 00 00 00 00 00 00  R_AARCH64_RELATIVE  1027   Adjust by program base.

. code (g_blob_code)

. gotplt vector table
. dynamic_linking_info (g_blob_dynamic_linking) (entries Elf64_Dyn) (exec name, imported libraries, various options)
. data (g_blob_data)

. section table

*/

