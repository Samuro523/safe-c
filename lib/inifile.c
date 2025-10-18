
/* inifile.c : read .ini file */

use strings, files, thread, bintree;

/************************************************************************/

struct ITEM
{
  string^  key;
  string^  value;
}

package ITEM_TREE = new BALANCED_BINARY_TREE (ELEMENT => ITEM^, USER_INFO => int);

typedef ITEM^[] ITEMS;

struct SECTION
{
  string^               section;
  ITEMS^                item;         // used for sequential search
  ITEM_TREE.BINARY_TREE tree;         // used for named search
}

package SECTION_TREE = new BALANCED_BINARY_TREE (ELEMENT => SECTION^, USER_INFO => int);

typedef SECTION^[] SECTIONS;

struct INI_DATA
{
  string^                  filename;
  int                      rc;        // 0 if OK, or E_INI_FILE_NOT_FOUND
  SECTIONS^                section;   // used for sequential search
  SECTION_TREE.BINARY_TREE tree;      // used for named search
}

package INI_TREE = new BALANCED_BINARY_TREE (ELEMENT => INI_DATA^, USER_INFO => int);

struct INI_INFO
{
  INI_TREE.BINARY_TREE tree;
  bool                 initialized;
}

INI_INFO ini;

SHARED_OBJECT so;

/************************************************************************/

int compare_item (int^ user, ITEM^ data1, ITEM^ data2)
{
  _unused user;
  return stricmp (data1^.key^, data2^.key^);
}

/************************************************************************/

int compare_section (int^ user, SECTION^ data1, SECTION^ data2)
{
  _unused user;
  return stricmp (data1^.section^, data2^.section^);
}

/************************************************************************/

int compare_ini_data (int^ user, INI_DATA^ data1, INI_DATA^ data2)
{
  _unused user;
  return stricmp (data1^.filename^, data2^.filename^);
}

/************************************************************************/

string^ new_string (string s)
{
  return new string ' (s[0:strlen(s)]);
}

/************************************************************************/

ITEM^ new_item (string key, string value)
{
  return new ITEM ' {key => new_string(key), value => new_string(value)};
}

/************************************************************************/

SECTION^ new_section (string section)
{
  SECTION^ s = new SECTION;

  s^.section = new_string (section);
  s^.item    = new ITEMS (0);
  ITEM_TREE.create_btree (out s^.tree, null, compare_item);

  return s;
}

/************************************************************************/

INI_DATA^ new_ini_data (string filename, int rc)
{
  INI_DATA^ s = new INI_DATA;

  s^.filename = new_string (filename);
  s^.rc       = rc;
  s^.section  = new SECTIONS (0);
  SECTION_TREE.create_btree (out s^.tree, null, compare_section);

  return s;
}

/************************************************************************/

void add_item (ref SECTION section, string key, string value)
{
  ITEM^  i   = new_item (key => key, value => value);
  ITEMS^ old = section.item;
  int    len = old^'length;

  // append in array
  section.item = new ITEMS (len+1);
  section.item^[0:len] = old^;
  section.item^[len] = i;
  free old;

  // append in tree
  (void)ITEM_TREE.insert_btree (ref section.tree, i);
}

/************************************************************************/

SECTION^ add_section (ref INI_DATA ini_data, string section)
{
  SECTION^  s   = new_section (section);
  SECTIONS^ old = ini_data.section;
  int       len = old^'length;

  // append in array
  ini_data.section = new SECTIONS (len+1);
  ini_data.section^[0:len] = old^;
  ini_data.section^[len] = s;
  free old;

  // append in tree
  (void)SECTION_TREE.insert_btree (ref ini_data.tree, s);

  return s;
}

/************************************************************************/

INI_DATA^ add_ini_data (ref INI_INFO ini_info, string filename, int rc)
{
  INI_DATA^ i = new_ini_data (filename, rc);

  // append in tree
  assert INI_TREE.insert_btree (ref ini_info.tree, i) == 0;

  return i;
}

/************************************************************************/

ITEM^ get_item (SECTION^ section, string key)
{
  ITEM^  i, j;
  int    rc;

  i = new ITEM;
  i^.key = new_string (key);

  j = i;

  rc = ITEM_TREE.retrieve_btree (section^.tree, ref j, BT_EQUAL);

  free i^.key;
  free i;

  if (rc == 0)
    return j;

  if (rc == BT_KEY_NOT_FOUND)
    return null;

  abort;
}

/************************************************************************/

SECTION^ get_section (INI_DATA^ ini_data, string section)
{
  SECTION^  i, j;
  int       rc;

  i = new SECTION;
  i^.section = new_string (section);

  j = i;

  rc = SECTION_TREE.retrieve_btree (ini_data^.tree, ref j, BT_EQUAL);

  free i^.section;
  free i;

  if (rc == 0)
    return j;

  if (rc == BT_KEY_NOT_FOUND)
    return null;

  abort;
}

/************************************************************************/

INI_DATA^ get_ini_data (ref INI_INFO ini_info, string filename)
{
  INI_DATA^ i, j;
  int       rc;

  i = new INI_DATA;
  i^.filename = new_string (filename);

  j = i;

  rc = INI_TREE.retrieve_btree (ini_info.tree, ref j, BT_EQUAL);

  free i^.filename;
  free i;

  if (rc == 0)
    return j;

  if (rc == BT_KEY_NOT_FOUND)
    return null;

  abort;
}

/************************************************************************/

// does never return null

INI_DATA^ load_ini_data (ref INI_INFO ini_info, string filename)
{
  INI_DATA^ ini_data;
  SECTION^  section;
  FILE      fp;
  char      line[4097];
  int       size, i, j, k, l;

  if (!ini_info.initialized)
  {
    INI_TREE.create_btree (out ini_info.tree, null, compare_ini_data);
    ini_info.initialized = true;
  }

  ini_data = get_ini_data (ref ini_info, filename);
  if (ini_data != null)    // already loaded
    return ini_data;

  if (fopen (out fp, filename) != 0)
    return add_ini_data (ref ini_info, filename, E_INI_FILE_NOT_FOUND);

  ini_data = add_ini_data (ref ini_info, filename, 0);

  section = null;

  for (;;)
  {
    if (fgets (ref fp, out line) != 0)   /* end-of-file or error */
      break;

    /* 'size' = the active line length */
    size = strlen(line);

    /* delete trailing '\n' character */
    if (size > 0 && line[size-1] == '\n')
      size--;

    /* compact doubled '#' or ';' characters and also find      */
    /* non-doubled '#' or ';' character, and cut the line then. */

    i = 0;    /* source index */
    j = 0;    /* target index */

    while (i < size)
    {
      if (line[i] == '#' || line[i] == ';')   /* special symbol */
      {
        if (line[i] == line[i+1])   /* doubled */
        {
          line[j] = line[i];
          i += 2;
          j += 1;
        }
        else     /* non-doubled -> end of line */
        {
          break;
        }
      }
      else   /* normal symbol */
      {
        line[j++] = line[i++];
      }
    }

    size = j;


    /* delete all trailing white space */
    while (size > 0 && line[size-1] <= ' ')
      size--;

    /* skip empty line */
    if (size == 0)
      continue;


    /* test whether '[' or '=' comes first */

    for (i=0; i<size; i++)
    {
      if (line[i] == '[' || line[i] == '=')
        break;
    }

    /* skip line having incorrect format */
    if (i == size)
      continue;

    if (line[i] == '[')   /* it's a section */
    {
      i = 0;

      while (i<size && line[i]<=' ')
        i++;

      if (i==size || line[i] != '[')
        continue;

      i++;    /* skip '[' */

      while (i<size && line[i]<=' ')
        i++;

      j = i;

      while (j<size && line[j]!=']')
        j++;

      while (j>i && line[j-1]<=' ')
        j--;

      if (i == j)  /* empty */
        continue;

      section = add_section (ref ini_data^, line[i:j-i]);
    }
    else    /* it's an assignment "item = value" */
    {
      i = 0;

      while (i<size && line[i]<=' ')
        i++;

      j = i;

      while (j<size && line[j]!='=')
        j++;

      k = j;  /* points to '=' */

      while (j>i && line[j-1]<=' ')
        j--;

      if (i == j)  /* empty */
        continue;

      line[j] = '\0';

      k++;
      while (k<size && line[k]<=' ')
        k++;

      l = size;
      while (l>k && line[l-1]<=' ')
        l++;

      if (section != null)
      {
        add_item (ref section^, key=>line[i:j-i], value=>line[k:l-k]);
      }
    }
  }

  fclose (ref fp);
  return ini_data;
}

/************************************************************************/

int copy_value (string value, out string buffer)
{
  clear buffer;

  if (strlen(value) > buffer'length)
  {
    strncpy (out buffer, value, buffer'length);
    return E_INI_BUFFER_TOO_SMALL;
  }
  else
  {
    strcpy (out buffer, value);
    return 0;
  }
}

/************************************************************************/

public int get_parameter (string       filename,        /* param.ini    */
                          string       section,         /* section name */
                          string       key,             /* key name     */
                          out string   parameter,       /* out buffer   */
                          string       default_value = "")
{
  INI_DATA^ p;
  SECTION^  ps;
  ITEM^     pi;
  int       rc;

  enter_shared_object (ref so);

  p = load_ini_data (ref ini, filename);
  if (p^.rc != 0)
  {
    copy_value (default_value, out parameter);
    rc = p^.rc;
    leave_shared_object (ref so);
    return rc;
  }

  /* find matching section */

  ps = get_section (p, section);
  if (ps == null)
  {
    copy_value (default_value, out parameter);
    leave_shared_object (ref so);
    return E_INI_SECTION_NOT_FOUND;
  }

  pi = get_item (ps, key);
  if (pi == null)
  {
    copy_value (default_value, out parameter);
    leave_shared_object (ref so);
    return E_INI_KEY_NOT_FOUND;
  }

  rc = copy_value (pi^.value^, out parameter);
  leave_shared_object (ref so);
  return rc;
}

/************************************************************************/

/* retrieve nth section of the file.   */
/* returns 0 or a negative error code. */

public int get_nth_section (string     filename,         /* ini filename  */
                            int        nth,              /* 0 .. ?        */
                            out string section)          /* out buffer    */
{
  INI_DATA^ p;
  SECTIONS^ s;
  int       rc;

  clear section;

  if (nth < 0)
    return E_INI_BAD_INDEX;

  enter_shared_object (ref so);

  p = load_ini_data (ref ini, filename);
  if (p^.rc != 0)
  {
    rc = p^.rc;
    leave_shared_object (ref so);
    return rc;
  }

  /* find matching section */

  s = p^.section;
  if (nth >= s^'length)
  {
    leave_shared_object (ref so);
    return E_INI_BAD_INDEX;
  }

  rc = copy_value (s^[nth]^.section^, out section);
  leave_shared_object (ref so);
  return rc;
}

/************************************************************************/

/* retrieve nth key within given section of the file. */
/* returns 0 or a negative error code.                */

public int get_nth_key (string       filename,  /* ini filename  */
                        string       section,   /* section name  */
                        int          nth,       /* 0 .. ?        */
                        out string   key)       /* out buffer    */
{
  INI_DATA^ p;
  SECTION^  ps;
  ITEMS^    pi;
  int       rc;

  clear key;

  if (nth < 0)
    return E_INI_BAD_INDEX;

  enter_shared_object (ref so);

  p = load_ini_data (ref ini, filename);
  if (p^.rc != 0)
  {
    rc = p^.rc;
    leave_shared_object (ref so);
    return rc;
  }

  /* find matching section */

  ps = get_section (p, section);
  if (ps == null)
  {
    leave_shared_object (ref so);
    return E_INI_SECTION_NOT_FOUND;
  }

  pi = ps^.item;
  if (nth >= pi^'length)
  {
    leave_shared_object (ref so);
    return E_INI_BAD_INDEX;
  }

  rc = copy_value (pi^[nth]^.key^, out key);
  leave_shared_object (ref so);
  return rc;
}

/************************************************************************/

/* retrieve nth parameter within given section of the file. */
/* returns 0 or a negative error code.                      */

public int get_nth_parameter (string      filename,   /* ini filename  */
                              string      section,    /* section name  */
                              int         nth,        /* 0 .. ?        */
                              out string  parameter)  /* out buffer    */
{
  INI_DATA^ p;
  SECTION^  ps;
  ITEMS^    pi;
  int       rc;

  clear parameter;

  if (nth < 0)
    return E_INI_BAD_INDEX;

  enter_shared_object (ref so);

  p = load_ini_data (ref ini, filename);
  if (p^.rc != 0)
  {
    rc = p^.rc;
    leave_shared_object (ref so);
    return rc;
  }

  /* find matching section */

  ps = get_section (p, section);
  if (ps == null)
  {
    leave_shared_object (ref so);
    return E_INI_SECTION_NOT_FOUND;
  }

  pi = ps^.item;
  if (nth >= pi^'length)
  {
    leave_shared_object (ref so);
    return E_INI_BAD_INDEX;
  }

  rc = copy_value (pi^[nth]^.value^, out parameter);
  leave_shared_object (ref so);
  return rc;
}

/************************************************************************/

void deallocate_item (ITEM^ item)
{
  free item^.key;
  free item^.value;
  free item;
}

/************************************************************************/

void deallocate_section (SECTION^ section)
{
  ITEMS^  s;
  int     len, i;

  s = section^.item;

  len = s^'length;

  for (i=0; i<len; i++)
    deallocate_item (s^[i]);

  ITEM_TREE.close_btree (ref section^.tree);

  free s;
  free section^.section;
  free section;
}

/************************************************************************/

int deallocate_ini_data (int^ user, INI_DATA^ data)
{
  SECTIONS^ s;
  int       len, i;

  _unused user;

  s = data^.section;

  len = s^'length;

  for (i=0; i<len; i++)
    deallocate_section (s^[i]);

  SECTION_TREE.close_btree (ref data^.tree);

  free s;
  free data^.filename;
  free data;

  return 0;
}

/************************************************************************/

void deallocate_ini_info (ref INI_INFO ini_info)
{
  if (ini_info.initialized)
  {
    (void)INI_TREE.traverse_btree (ini_info.tree, deallocate_ini_data, +1);
    INI_TREE.close_btree (ref ini_info.tree);
    ini_info.initialized = false;
  }
}

/************************************************************************/

/* signal that initialization file was changed and must be reloaded. */

public void signal_ini_file_changed ()
{
  enter_shared_object (ref so);
  deallocate_ini_info (ref ini);
  leave_shared_object (ref so);
}

/************************************************************************/
