
// goptions.c : global compiler options

from std use strings, inifile;
use error;

/**********************************************************************************/

const string section = "options";

public
void load_global_options (string ini_filename)
{
  int  nth;
  char key[128], value[128], str[128];

  for (nth=0; ; nth++)
  {
    if (get_nth_key (ini_filename, section, nth, out key) < 0 ||
        get_nth_parameter (ini_filename, section, nth, out value) < 0)
      break;

    if (strcmp (key, "target") == 0)
    {
      if (g_target != UNDEFINED_TARGET)
      {
        sprintf (out str, "%.32s : multiple compiler targets are not allowed", ini_filename);
        config_error (str);
      }

      if (strcmp (value, "intel") == 0)
        g_target = INTEL;
      else if (strcmp (value, "android") == 0)
        g_target = ANDROID;
      else
      {
        sprintf (out str, "%.32s : illegal compiler target %.64s : intel or android expected", ini_filename, key);
        config_error (str);
      }
    }
    else if (strcmp (key, "memory_model") == 0)
    {
      if (address_size != 0)
      {
        sprintf (out str, "%.32s : multiple compiler memory_model are not allowed", ini_filename);
        config_error (str);
      }

      if (strcmp (value, "32") == 0)
      {
        address_size = 4;
      }
      else if (strcmp (value, "64") == 0)
      {
        address_size = 8;
      }
      else
      {
        sprintf (out str, "%.32s : illegal compiler option %.64s : only 32 or 64 supported", ini_filename, key);
        config_error (str);
      }
    }
    else if (strcmp (key, "pointer_check") == 0)
    {
      if (strcmp (value, "yes") == 0)
        pointer_checks_enabled = true;
      else if (strcmp (value, "no") == 0)
        pointer_checks_enabled = false;
      else
      {
        sprintf (out str, "%.32s : illegal compiler option %.64s : yes or no expected", ini_filename, key);
        config_error (str);
      }
    }
    else if (strcmp (key, "array_check") == 0)
    {
      if (strcmp (value, "yes") == 0)
        array_checks_enabled = true;
      else if (strcmp (value, "no") == 0)
        array_checks_enabled = false;
      else
      {
        sprintf (out str, "%.32s : illegal compiler option %.64s : yes or no expected", ini_filename, key);
        config_error (str);
      }
    }
    else if (strcmp (key, "assertion_check") == 0)
    {
      if (strcmp (value, "yes") == 0)
        assertion_checks_enabled = true;
      else if (strcmp (value, "no") == 0)
        assertion_checks_enabled = false;
      else
      {
        sprintf (out str, "%.32s : illegal compiler option %.64s : yes or no expected", ini_filename, key);
        config_error (str);
      }
    }
    else
    {
      sprintf (out str, "%.32s : illegal compiler option %.64s", ini_filename, key);
      config_error (str);
    }
  }

  if (g_target == UNDEFINED_TARGET)
    g_target = INTEL;

  if (address_size == 0)
    address_size = 8;

  if (g_target == ANDROID && address_size != 8)
  {
    sprintf (out str, "%.32s : illegal compiler option : 32-bit for android is not supported", ini_filename);
    config_error (str);
  }
}

/**********************************************************************************/
