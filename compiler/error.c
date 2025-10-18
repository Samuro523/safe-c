
// error.c

from std use console, strings, thread;
use common, front/unit;

//-------------------------------------------------------------------------------------

char[260] g_library_file, g_source_file, g_previous_library_file, g_previous_source_file;

int g_code_generator_source_line;
int g_previous_unit_key;

const int MAX_ERRORS = 25;
long g_nb_warnings, g_nb_errors;

//-------------------------------------------------------------------------------------

// called by unit.c to set current source file

public void set_current_error_source_file (string lib, string file, int key)
{
  strcpy (out g_library_file, lib);
  strcpy (out g_source_file, file);
  g_unit_key = key;   // used by entities.c when creating new entities
}

//-------------------------------------------------------------------------------------

// called by as86.c and codgen.c to set current source line

public void error_set_code_generator_source_line (int line)
{
  g_code_generator_source_line = line;
}

//-------------------------------------------------------------------------------------

// called by as86.c and codgen.c to set current unit key and load filenames

public void error_set_code_generator_unit_key (int key)
{

  if (key != g_previous_unit_key)
  {
    retrieve_library_and_source_names (key, out g_library_file, out g_source_file);
    g_previous_unit_key = key;
  }
}

//-------------------------------------------------------------------------------------

void display_source_filename ()
{
  if (g_nb_errors == MAX_ERRORS)
  {
    printf ("number of errors >= %d\n", MAX_ERRORS);
    stop_compilation ();
  }

  if (strcmp (g_previous_library_file, g_library_file) != 0 ||
      strcmp (g_previous_source_file, g_source_file) != 0)
  {
    strcpy (out g_previous_library_file, g_library_file);
    strcpy (out g_previous_source_file, g_source_file);

    printf ("\n");
    if (g_library_file[0] == '\0')
      printf ("compiling %s:\n", g_source_file);
    else
      printf ("compiling %s: [%s]\n", g_source_file, g_library_file);
  }
}

//-------------------------------------------------------------------------------------

public void get_current_source_filename (out char[528] filename)
{
  if (g_library_file[0] == '\0')
    sprintf (out filename, "%s", g_source_file);
  else
    sprintf (out filename, "%s: [%s]", g_source_file, g_library_file);
}

//-------------------------------------------------------------------------------------

public void warning (string s, TEXT_POSITION pos)
{
  display_source_filename ();
  printf ("line %d col %d : warning : %s\n", pos.line, pos.col, s);
  g_nb_warnings++;
}

//-------------------------------------------------------------------------------------

public void config_error (string s)
{
  display_source_filename ();
  printf ("config: %s\n", s);
  g_nb_errors++;
  stop_compilation ();
}

//-------------------------------------------------------------------------------------

public void lexical_error (string s, TEXT_POSITION pos)
{
  display_source_filename ();
  printf ("line %d col %d : lexical : %s\n", pos.line, pos.col, s);
  g_nb_errors++;
}

//-------------------------------------------------------------------------------------

public void syntax_error (string s, TEXT_POSITION pos)
{
  display_source_filename ();
  printf ("line %d col %d : syntax : %s\n", pos.line, pos.col, s);
  g_nb_errors++;
}

//-------------------------------------------------------------------------------------

public void semantic_error  (string s, TEXT_POSITION pos)
{
  display_source_filename ();
  printf ("line %d col %d : semantic : %s\n", pos.line, pos.col, s);
  g_nb_errors++;
}

//-------------------------------------------------------------------------------------

// fatal errors before code generation :

public void fatal_compiler_error (string s, TEXT_POSITION pos)
{
  display_source_filename ();
  printf ("line %d col %d : FATAL COMPILER ERROR : %s\n", pos.line, pos.col, s);
  g_nb_errors++;
  stop_compilation ();
}

//-------------------------------------------------------------------------------------

// for resource file

public void resource_error (string resource_filename, string msg, int line_nr, int col)
{
  printf ("compiling %s:\n", resource_filename);
  printf ("line %d col %d : syntax : %s\n", line_nr, col, msg);
  stop_compilation ();
}

//-------------------------------------------------------------------------------------

// for code generator :

public void code_generator_error (string s)
{
  display_source_filename ();
  printf ("\n");
  printf ("line %d : code generator : %s\n", g_code_generator_source_line, s);
  g_nb_errors++;
}

//-------------------------------------------------------------------------------------

// fatal errors during code generation or in pool.c :

public void fatal_compiler_error0 (string s)
{
  display_source_filename ();
  printf ("line %d : FATAL COMPILER ERROR : %s\n", g_code_generator_source_line, s);
  g_nb_errors++;
  stop_compilation ();
}

//-------------------------------------------------------------------------------------

public void fatal_out_of_memory_error (string s)
{
  display_source_filename ();
  printf ("line %d : out of memory : %s\n", g_code_generator_source_line, s);
  g_nb_errors++;
  stop_compilation ();
}

//-------------------------------------------------------------------------------------

public long nb_of_compilation_errors ()
{
  return g_nb_errors;
}

//-------------------------------------------------------------------------------------

public long nb_of_compilation_warnings ()
{
  return g_nb_warnings;
}

//-------------------------------------------------------------------------------------

public void stop_compilation ()
{
  printf ("stop compilation\n");
  exit (-1);
}

//-------------------------------------------------------------------------------------
