
// gsymb.c : global symbols

from std use inifile, strings;
use ../error, symbols, ../goptions, wstrings;

/**********************************************************************************/

public
void load_global_symbols (string ini_filename)
{
  const string section = "symbols";

  int   nth, rc;
  char  key[128], value[128], str[128];
  wchar wkey[128];
  bool  b;

  create_symbol_btree (out global_symbols);

  // predefined symbols
  insert_symbol (ref global_symbols, L"WINDOWS", g_target == INTEL);
  insert_symbol (ref global_symbols, L"ANDROID", g_target == ANDROID);

  insert_symbol (ref global_symbols, L"MEM32", (address_size == 4));
  insert_symbol (ref global_symbols, L"MEM64", (address_size == 8));

  for (nth=0; ; nth++)
  {
    if (get_nth_key (ini_filename, section, nth, out key) < 0 ||
        get_nth_parameter (ini_filename, section, nth, out value) < 0)
      break;

    string_to_wstring (key, out wkey);

    if (!is_all_in_uppercase (wkey))
    {
      sprintf (out str, "%.32s : symbol '%.32S' must be all in UPPER CASE",
               ini_filename, wkey);
      config_error (str);
    }

    if (!has_at_least_one_letter (wkey))
    {
      sprintf (out str, "%.32s : symbol '%.32S' must contain at least one letter A-Z",
               ini_filename, wkey);
      config_error (str);
    }

    if (strcmp (value, "0") != 0 && strcmp (value, "1") != 0)
    {
      sprintf (out str, "%.32s : symbol '%.32S' must have value 0 or 1", ini_filename, wkey);
      config_error (str);
    }

    b = (strcmp (value, "1") == 0);

    rc = insert_symbol (ref global_symbols, wkey, b);

    if (rc < 0)
    {
      fatal_out_of_memory_error ("load_global_symbols() : insert_symbol() failed");
    }
    else if (rc == +1)
    {
      sprintf (out str, "%.32s : symbol '%.32S' is defined twice", ini_filename, wkey);
      config_error (str);
    }
  }
}

/**********************************************************************************/
