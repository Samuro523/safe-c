
// glibpaths.c : global library paths

from std use inifile, files, strings;
use ../error;

//=================================================================================

typedef string^ PSTRING;   // max 260 chars

PSTRING[]^ g_library_path;
int        g_nb_lib_paths;

//=================================================================================

string^ new_string (string str)
{
  return new string ' (str[0 : strlen(str)]);
}

//=================================================================================

public
void load_global_library_paths (string ini_filename)
{
  int          nth;
  const string section = "library";
  char         key[260], value[260], str[64+64+260];

  g_nb_lib_paths = 0;
  for (nth=0; ; nth++)
  {
    if (get_nth_key (ini_filename, section, nth, out key) < 0 ||
        get_nth_parameter (ini_filename, section, nth, out value) < 0)
      break;

    if (strcmp (key, "dir") == 0)
    {
      if (value[0] == nul || value[strlen(value)-1] != '/')
      {
        sprintf (out str, "%.64s : [library] dir path '%.260s' must end with /", ini_filename, value);
        config_error (str);
      }
      else
      {
        g_nb_lib_paths++;
      }
    }
    else
    {
      sprintf (out str, "%.64s : illegal compiler [library] key %.64s", ini_filename, key);
      config_error (str);
    }
  }

  g_library_path = new PSTRING [g_nb_lib_paths];

  for (nth=0; ; nth++)
  {
    if (get_nth_key (ini_filename, section, nth, out key) < 0 ||
        get_nth_parameter (ini_filename, section, nth, out value) < 0)
      break;
    g_library_path^[nth] = new_string (value);
  }
}

//=================================================================================

public
void find_library (    string    unit_name,          // "std"
                   out char[260] library_filename)   // "std.lib" or "c:/libraries/std.lib"
{
  char filename[600];
  int  i;
  bool found;

  found = false;

  // default path is current directory
  sprintf (out filename, "%s%s", unit_name, ".lib");
  if (exists (filename))
    found = true;
  else
  {
    // otherwise look in all paths provided in library section
    for (i=0; i<g_nb_lib_paths; i++)
    {
      sprintf (out filename, "%s%s%s", g_library_path^[i]^, unit_name, ".lib");  // 260 + 260 + 4
      if (exists (filename))
      {
        found = true;
        break;
      }
    }
  }
  
  if (found)
  {
    if (strlen(filename) > 260)
    {
      char msg[700];
      sprintf (out msg, "path is too long : %s", filename);
      config_error (msg);
      stop_compilation ();
    }
    
    strcpy (out library_filename, filename);
  }
  else   // library not found
  {
    sprintf (out filename, "cannot find library %s.lib", unit_name);
    config_error (filename);
    stop_compilation ();
    clear library_filename;
  }
}

//=================================================================================
