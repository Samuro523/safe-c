
// mk.c : Safe-C compiler, linker, and make utility.

from std use console, exception, strings, files, thread, tracing;

use front/gsymb, front/glibpaths, front/entities, front/unit, front/libunit;
use front/packages, front/wstrings;

use intel/pe, intel/init86, intel/as86;
use arm/elf, arm/asm_arm, arm/initarm;

use common, fixup, goptions, error, pool, makelib0, dbginfo, exeout, blob2;
use codgen;

//=================================================================================

const string DEFAULT_CONFIG_FILE = "mk.cfg";

//=================================================================================

void get_current_dir (out char current_dir[MAX_FILENAME_LENGTH])   // always ends with '/'
{
  char c;

  get_current_directory (out current_dir);

  if (current_dir[0] != nul)  // make sure it ends with a '/'
  {
    c = current_dir[strlen(current_dir)-1];   // last char
    if (c != '\\' && c != '/')
      strcat (ref current_dir, "/");
  }

  normalize_pathname (ref current_dir);
}

//=================================================================================

// final : when true, display any warnings (if no errors)

void display_errors (bool final)
{
  if (nb_of_compilation_errors() > 0)
  {
    printf ("%d error%s", nb_of_compilation_errors(),  nb_of_compilation_errors() == 1 ? "" : "s");

    if (nb_of_compilation_warnings() > 0)
      printf (", %d warning%s", nb_of_compilation_warnings(),  nb_of_compilation_warnings() == 1 ? "" : "s");

    printf ("\n");
    exit (-1);
  }

  if (final)
  {
    printf ("ok");
    if (nb_of_compilation_warnings() > 0)
      printf (" (%d warning%s)", nb_of_compilation_warnings(),  nb_of_compilation_warnings() == 1 ? "" : "s");
    printf ("\n");
  }
}

//=================================================================================

int delete_file_and_confirm (string filename)
{
  int fd;
  fd = open (filename);
  if (fd < 0)
    return 0;
  close (fd);
  if (delete_file (filename) < 0)
    return -1;
  return 0;
}

//=================================================================================

void print_usage ()
{
  printf ("\n");
  printf ("Safe-C Compiler v1.%u\n", LIB_VERSION);
  printf ("usage: mk source.c [cfg=%s] [rc=<file>.rc] [prod] [listing] [crc] \n", DEFAULT_CONFIG_FILE);
  printf ("cfg     : specify config file\n");
  printf ("rc      : specify resource file\n");
  printf ("prod    : merge identical pool constants (slower compilation but smaller exe)\n");
  printf ("crc     : display checksum and code size\n");
  printf ("listing : generate listing\n");
  printf ("\n");
}

//=================================================================================

int main (string[] arg)
{
  char[MAX_FILENAME_LENGTH] current_dir, filename, filename_ext_c, simple_filename;
  char[MAX_FILENAME_LENGTH] exec_filename, debug_filename, res_filename;
  int                       nb_args = arg'length;
  int                       k;
  bool                      option_crc, option_prod, explicit_option_cfg, explicit_option_rc;
  string^                   pconfig_filename;


  arm_exception_handler ();

  g_tracing = false;
  option_crc = false;
  option_prod = false;
  explicit_option_cfg = false;
  pconfig_filename = new string ' (DEFAULT_CONFIG_FILE);
  explicit_option_rc = false;
  clear res_filename;
  
  for (k=2; k<nb_args; k++)
  {
    ref string a = arg[k];

    if (strcmp (a, "listing") == 0)
      g_tracing = true;
    else if (strcmp (a, "crc") == 0)
      option_crc = true;
    else if (strcmp (a, "prod") == 0)
      option_prod = true;
    else if (strnicmp (a, "cfg=", 4) == 0)
    {
      free pconfig_filename;
      pconfig_filename = new string ' (a[4:strlen(a)-4]);

      if (!files.exists (pconfig_filename^))
      {
        printf ("error: invalid option '%s' : config file '%s' not found\n", a, pconfig_filename^);
        return -1;
      }

      if (explicit_option_cfg)
      {
        printf ("error: duplicate option cfg=\n");
        return -1;
      }

      explicit_option_cfg = true;
    }
    else if (strnicmp (a, "rc=", 3) == 0)
    {
      strcpy (out res_filename, a[3:strlen(a)-3]);

      if (!files.exists (res_filename))
      {
        printf ("error: invalid option '%s' : resource file '%s' not found\n", a, res_filename);
        return -1;
      }

      if (explicit_option_rc)
      {
        printf ("error: duplicate option rc=\n");
        return -1;
      }

      explicit_option_rc = true;
    }
    else
    {
      print_usage ();
      return -1;
    }
  }

  if (g_tracing)
  {
    delete_file ("listing.txt");
    open_trace ("listing.txt", 256*1024*1024, date => false, time => false);
  }


  load_global_options       (pconfig_filename^);  // MUST BE FIRST to load address_size
  load_global_symbols       (pconfig_filename^);  // MUST BE SECOND because uses address_size
  load_global_library_paths (pconfig_filename^);

  free pconfig_filename;
  pconfig_filename = null;


  if (nb_args < 2)  // no filename
  {
    print_usage ();
    return -1;
  }

  sprintf (out filename, "%.255s", arg[1]);

  if (strlen(filename) < 2 || stricmp (filename[strlen(filename)-2 : 2], ".c") != 0)
    strcat (ref filename, ".c");

  get_current_dir (out current_dir);
  clear filename_ext_c;
  if (expand_pathname (current_dir, filename, out filename_ext_c[0 : filename_ext_c'length-5]) < 0)
  {
    printf ("fatal error: cannot create pathname for '%s'\n", filename);
    return -1;
  }

  to_lower_case (ref filename_ext_c);

  {
    int i, j;  // i included, j excluded

    i = strlen(filename_ext_c);
    while (i-1 >= 0 && filename_ext_c[i-1] != '/' && filename_ext_c[i-1] != '\\')
      i--;
    // i points on character just after last slash, or =0

    j = strlen(filename_ext_c);
    while (j-1 >= 0 && filename_ext_c[j-1] != '/' && filename_ext_c[j-1] != '\\' &&
           filename_ext_c[j-1] != '.')
      j--;
    // j points on character just after last slash or last dot, or =0

    strcpy (out simple_filename, filename_ext_c[i:j-i]);

    switch (g_target)
    {
      case INTEL:
        sprintf (out exec_filename,  "%s%s", simple_filename, "exe");
        break;

      case ANDROID:
        sprintf (out exec_filename,  "%s%s", simple_filename, "so");
        break;

      default:
        abort;
    }


    if (!explicit_option_rc)
      sprintf (out res_filename, "%s%s", simple_filename, "rc");
      
    sprintf (out debug_filename, "%s%s", simple_filename, "dbg");
  }

  // deleting any previous .exe and .dbg
  if (delete_file_and_confirm (exec_filename) < 0)
  {
    printf ("error: cannot create %s\n", exec_filename);
    return -1;
  }
  if (delete_file_and_confirm (debug_filename) < 0)
  {
    printf ("error: cannot create %s\n", debug_filename);
    return -1;
  }


  init_constant_pool ();          // do this before creating predefined entities
  create_predefined_entities ();  // this creates also the root region

  init_unit_tree ();
  init_library_unit_tree ();
  init_fixup_structures ();


  if (compile_main_unit (filename_ext_c) != 0)
    return -1;

  display_errors (final => false);  // this will stop in case of errors


  // instantiate all generic package bodies

  instantiate_all_pending_package_bodies ();

  display_errors (final => false);  // this will stop in case of errors


  // perform function inlining

// $


  // parse resource file

  switch (g_target)
  {
    case INTEL:
      pe.parse_resource_file (current_dir, res_filename);
      break;

    case ANDROID:
      elf.parse_resource_file (current_dir, res_filename);
      break;

    default:
      abort;
  }

  display_errors (final => false);   // this will stop in case of errors


  // check main (not for android shared object)

  switch (g_target)
  {
    case INTEL:
      if (codgen.g_func_main == null)
        semantic_error ("missing start function main()", TEXT_POSITION'{1,1});
      break;

    case ANDROID:
      if (codgen.g_func_main != null)
        semantic_error ("main() is not allowed for android", TEXT_POSITION'{1,1});
      if (codgen.g_entry_function_list == null)
        semantic_error ("missing [entry] entry points", TEXT_POSITION'{1,1});
      break;

    default:
      abort;
  }

  display_errors (final => false);   // this will stop in case of errors


  // generate code

  blob2.init_blobs ();  // init code and data blobs

  {
    const int func_label_to_main           = 1;
    const int func_label_to_init_constants = 2;

    switch (g_target)
    {
      case INTEL:
        
        codgen.g_global_offset = INTEL_BEGIN_GLOBAL_OFFSET;
      
        pe       .pe_init_image ();
        dbginfo  .dbg_header ();

        init86 . generate_bootstrap_86_code (func_label_to_main,
                                             func_label_to_init_constants,
                                             codgen . g_function_main_has_parameter_array_of_string);

        // all the application's functions, starting with main
        g_func_main^.the_function_declaration.func_label_nr = func_label_to_main;

        codgen   . generate_code (codgen . g_func_main,
                                  as86   . generate_asm_for_function);

        // for 32-bit : extra functions for mult/div/mod int8
        // extra functions for allocate/free tombstones
        // 64-bit debug function for testing stack alignment at M16
        init86 . extra_86_code ();

        codgen   . generate_code_to_init_global_variables (func_label_to_init_constants,
                                                           as86 . generate_asm_for_function,
                                                           stack_size => 4*1024*1024);   // unused

        pe.exe_finish_code ();

        break;

      case ANDROID:

        codgen.g_global_offset = ANDROID_BEGIN_GLOBAL_OFFSET;

        elf      . elf_init_image ();
        dbginfo  .dbg_header ();

        // generate code for all entry points
        {
          int i;
          for (i=0; i<codgen.g_entry_function_list^'length; i++)
          {
            PENTITY func = codgen.g_entry_function_list^[i];
            uint    code_ip;

            // generate code if no one called this function yet
            codgen   . generate_code (func,
                                      asm_arm . generate_asm_for_function);

            code_ip = fixup.function_get_ip (func^.the_function_declaration.func_label_nr);

            fixup.export_set_code_ip (name    => func^.identifier_or_null^,
                                      code_ip => code_ip);
          }
        }

        codgen   . generate_code_to_init_global_variables (func_label_to_init_constants,
                                                           asm_arm . generate_asm_for_function,
                                                           stack_size => 4*1024*1024);    // 4 MB


        initarm . extra_arm_code ();  // code for tombstone allocate/free
        
        fixup . fix_function_calls ();
        break;

      default:
        abort;
    }


    display_errors (final => false);   // this will stop in case of errors


    // data: constants pool, switch jump tables, dll import tables.

    // optional (can take a long time)
    if (option_prod)
      pool.merge_pool_subtrings ();

    switch (g_target)
    {
      case INTEL:
        // write pool constants in exe
        pool.intel_flush_pool_subtrings (cte_segment_load_address => pe.LOAD_ADDRESS);

        // writes also DLL import table and resources.
        pe.exe_finish_data (res_filename, g_global_offset);
        break;

      case ANDROID:
        // write pool constants in data blob
        pool.android_flush_pool_subtrings ();
        
        // write resources
        elf.write_resources_in_data_blob ();
        break;

      default:
        abort;
    }


    // bss: global variables


    switch (g_target)
    {
      case INTEL:
        if (pe.current_RIP() > 2*1023*1024*1024 - codgen.g_global_offset)
          code_generator_error ("program code+data+bss exceeds 2GB");

        pe.exe_terminate_image
           (/* bss         = */ (uint4)(4096 * ((codgen.g_global_offset + 4095) / 4096)),   // M4K
            /* stack       = */ (address_size == 4) ? 1024*1024 : 4*1024*1024,  // must be multiple of 4K PAGE !
            /* console_app = */ is_console_app);
        break;

      case ANDROID:
        {
          uint out_start_of_bss_mem;
          int8 bss_size = (codgen.g_global_offset + 4095) & -4096;    // M4K

          elf.elf_terminate_image
             (    exe_simple_filename  => exec_filename,
                  bss_size             => (uint)bss_size,
                  func_label_to_init_constants,
              out out_start_of_bss_mem => out_start_of_bss_mem);

          if (out_start_of_bss_mem > 2*1023*1024*1024 - bss_size)    // exceeds 2 GB - 1 MB
            code_generator_error ("program code+data+bss exceeds 2GB");
        }
        break;

      default:
        abort;
    }

  }

  if (error.nb_of_compilation_errors() == 0)
  {
    exeout  . exe_save (exec_filename);
    dbginfo . dbg_save (debug_filename);

    if (option_crc)
    {
      switch (g_target)
      {
        case INTEL:
          pe . print_crc ();
          break;

        case ANDROID:
          elf . print_crc ();
          break;

        default:
          abort;
      }
    }
  }

  display_errors (final => true);
  return 0;
}

//=================================================================================
