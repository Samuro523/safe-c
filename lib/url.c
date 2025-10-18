
// url.c

use strings;

//---------------------------------------------------------------------------

// return uppercase hex char

char hex_digit (int n)
{
  assert n >= 0 && n <= 15;

  if (n < 10)
    return '0' + n;
  else
    return 'A' + (n - 10);
}

//---------------------------------------------------------------------------

// return hex value

int hex_value (char c)
{
  if (c >= '0' && c <= '9')
    return (int)c - (int)'0';
  else if (c >= 'A' && c <= 'F')
    return (int)c - ((int)'A' - 10);
  else if (c >= 'a' && c <= 'f')
    return (int)c - ((int)'a' - 10);
  else
    abort;
}

//---------------------------------------------------------------------------

// If the host or the filename is undefined, it is an empty string.

// The host and filename structures are not checked at all.
// (we check only for the presence of spaces and non-printable control characters).

// The filename can contain a query part and parameters.

int split_url (string href, out URL url)
{
  int   i, j, start, len, depth;
  char  c;
  uint2 port;

  clear url;


  // distinguish absolute from relative URLs :
  // try to find a first colon or slash.

  c = nul;
  for (i=0; i<href'length; i++)
  {
    c = href[i];
    if (c == nul || c == ':' || c == '/')
      break;
  }

  if (c == ':')   // we found a colon : it's an absolute URL
  {
    if (i > URL_PROTOCOL_LENGTH)
      return URL_PROTOCOL_TOO_LONG;
    
    url.protocol[0:i] = href[0:i];

    i++;           // skip ':'

    if (i+1 >= href'length || href[i] != '/' || href[i+1] != '/')
      return URL_BAD_SYNTAX;
    i += 2;        // skip "//"


    // parse the hostname delimited by '/', '\', ':', ';', '?', '#'

    start = i;
    depth = 0;

    while (i < href'length)
    {
      c = href[i];

      if (c == '[')
        depth++;
      if (c == ']')
        depth--;
        
      if (c == nul || 
          (depth == 0 && 
              (c == '/' || c == '\\' || c == ':' || c == ';' || c == '?' || c == '#')))
        break;

      if (c <= ' ' || c == (char)127)
        return URL_BAD_HOSTNAME;

      i++;
    }

    len = i - start;

    if (len == 0)
      return URL_BAD_HOSTNAME;

    if (len > url.host'length)
      return URL_HOSTNAME_TOO_LONG;

    // retrieve the host name, and convert it to lower case

    for (j=0; j<len; j++)
      url.host[j] = tolower(href[start+j]);


    // parse the port number

    if (i < href'length && href[i] == ':')
    {
      i++;         // skip ':'

      start = i;

      while (i < href'length && isdigit(href[i]))
        i++;

      len = i - start;

      if (sscanf (href[start:len], "%u", out port) != 0)
        return URL_BAD_PORT_NUMBER;

      url.port = port;
    }


    // check that the string ends here or continues with a slash.

    if (i < href'length && href[i] != nul && href[i] != '/')
      return URL_BAD_SYNTAX;
  }
  else
  {
    i = 0;   // parse absolute or relative filename from start
  }


  // check that the rest of the filename does not contain any control characters.
  // and translate any %xx forms.

  j = 0;

  while (i < href'length)
  {
    c = href[i];

    if (c == nul || c == ';' || c == '?' || c == '#')
      break;

    if (c == '%' && i+2 < href'length && isxdigit (href[i+1]) && isxdigit (href[i+2]))
    {
      c = (char)((hex_value (href[i+1]) << 4) + hex_value (href[i+2]));
      i += 2;
    }

    if (c < ' ' || c == (char)127)
      return URL_BAD_SYNTAX;

    if (j == url.filename'length)
      return URL_FILENAME_TOO_LONG;

    url.filename[j++] = c;

    i++;
  }


  // finally, copy the parameters which must only contain printable characters.

  j = 0;

  while (i < href'length)
  {
    c = href[i];

    if (c == nul)
      break;

    if (c < ' ' || c == (char)127)
      return URL_BAD_SYNTAX;

    if (j == url.parameter'length)
      return URL_PARAMETER_TOO_LONG;

    url.parameter[j++] = c;

    i++;
  }

  return 0;
}

//---------------------------------------------------------------------------

// verifies that the path is absolute, removes all ".." and "." components.

int normalize_absolute_path (ref string filename)
{
  int  i, start, len, q;
  char c;

  if (filename'length == 0 || filename[0] != '/')    // not absolute path ?
    return URL_NOT_ABSOLUTE;

  i = 0;

  for (;;)
  {
    // skip '/'
    i++;

    start = i;      // start of path component

    // go til next component or end of path
    c = nul;
    while (i < filename'length)
    {
      c = filename[i];
      if (c == nul || c == '/')
        break;
      i++;
    }

    len = i - start;

    if (len == 0)   // an empty component
    {
      if (c == nul)    // ok (path ends with slash)
      {
        break;   // OK (finished)
      }

      return URL_BAD_SYNTAX;     //  "//" not allowed
    }
    else if (len == 2 && filename[start] == '.' && filename[start+1] == '.')    // ".."
    {
      if (i < filename'length && c != '/' && c != nul)
        return URL_BAD_SYNTAX;

      // find a slash before start-1 (if any)

      q = start - 2;    // q will point at earlier slash

      while (q >= 0 && filename[q] != '/')
        q--;

      if (q < 0)
        return URL_CANNOT_NORMALIZE;

      // here, q points to a slash.

      len = strlen (filename[i:filename'length-i]);

      filename[q:len] = filename[i:len];
      filename[q+len] = nul;

      // if this is the end of string, we must keep one slash
      // because it was a directory.
      if (len == 0)
      {
        strcat (ref filename, "/");
        return 0;
      }

      i = q;
    }
    else if (len == 1 && filename[start] == '.')   // "."
    {
      if (i < filename'length && c != '/' && c != nul)
        return URL_BAD_SYNTAX;

      q = start - 1;    // now, q points to earlier slash

      len = strlen (filename[i:filename'length-i]);

      filename[q:len] = filename[i:len];
      filename[q+len] = nul;

      // if this is the end of string, we must keep one slash
      // because it was a directory.
      if (len == 0)
      {
        strcat (ref filename, "/");
        return 0;
      }

      i = q;
    }
    else   // a normal component
    {
      if (c == nul)
        break;   // OK (finished)
    }
  }

  return 0;
}

//---------------------------------------------------------------------------

// make the URL absolute (if possible), check the path syntax.
// returns an error if the URL cannot be made absolute.

int force_absolute_url (ref URL url)
{
  if (url.host[0] == nul)
    return URL_INCOMPLETE;

  if (url.filename[0] == nul)        // no filename
  {
    strcpy (out url.filename, "/");      // set default one
    return 0;
  }

  return normalize_absolute_path (ref url.filename);
}

//---------------------------------------------------------------------------

// expand the 'url' in case it's relative,
// normalize its path (removing all "." and ".." components)

int expand_url (URL base, ref URL url)
{
  // check base url

  if (base.host[0] == nul || base.filename[0] != '/')
    return URL_NOT_ABSOLUTE;


  if (url.host[0] == nul)     // no host, no port
  {
    strcpy (out url.host, base.host);   // use host+port from base URL
    url.port = base.port;

    if (url.filename[0] == nul)   // empty filename
    {
      strcpy (out url.filename, base.filename);   // use base's filename
      return 0;
    }
  }
  else     // a host name was given
  {
    if (url.filename[0] == nul)   // empty filename
    {
      strcpy (out url.filename, "/");   // use root
      return 0;
    }
  }


  // check if the url's filename is absolute or relative

  if (url.filename[0] == '/')    // absolute path : just normalize it
  {
    return normalize_absolute_path (ref url.filename);
  }


  // relative filename

  {
    char filename[2 * url.filename'length];
    int  t, rc;

    strcpy (out filename, base.filename);      // begin with absolute path


    // remove last directory name of base, if any.

    t = strrchr (filename, '/');     // find last occurence of '/'
    if (t != -1 && t+1 < filename'length)
      filename[t+1] = nul;


    // append relative filename

    strcat (ref filename, url.filename);

    rc = normalize_absolute_path (ref filename);
    if (rc < 0)
      return rc;

    if (strlen (filename) > url.filename'length)
      return URL_FILENAME_TOO_LONG;

    strcpy (out url.filename, filename);
  }

  return 0;
}

//---------------------------------------------------------------------------

// parse a href that is always absolute (http://www.site.com/dir)

public int make_absolute_url (string href, out URL url)
{
  int rc;

  rc = split_url (href, out url);
  if (rc < 0)
    return rc;

  rc = force_absolute_url (ref url);
  if (rc < 0)
    return rc;

  return 0;
}

//---------------------------------------------------------------------------

// parse an undefined href : either absolute or relative to a base URL

public int make_relative_url (string href, URL base, out URL url)
{
  int rc;

  rc = split_url (href, out url);
  if (rc < 0)
    return rc;

  rc = expand_url (base, ref url);
  if (rc < 0)
    return rc;

  return 0;
}

//---------------------------------------------------------------------------

//  Windows Rules for simple filenames :
//  ------------------------------------
//  . all leading spaces or dots are not allowed -> replace by %xx.
//  . device names "con", "aux", "prn", "nul" are not allowed,
//    either alone or followed by a dot -> replace first char.
//  . device names "com", "lpt" followed by a digit are not allowed,
//    either alone or followed by a dot -> replace first char.
//  . the chars \/:*?"<>| are not allowed -> to be replaced.
//  . ascii codes 1 .. 31 are not allowed -> to be replaced.
//  . all trailing spaces or dots are not allowed -> replace all.
//  -> the target string can become up to 3 times larger.

void normalize_simple_name (string source, out string target)
{
  int  i, j, i9;
  bool device;
  char c;

  clear target;

  i = 0;
  j = 0;


  // compute i9 : first trailing space or dot

  i9 = strlen(source);
  while (i9 > 0 && (source[i9-1] == ' ' || source[i9-1] == '.'))
    i9--;


  // all leading spaces or dots are not allowed -> replace all.

  while (i<i9 && (source[i] == ' ' || source[i] == '.'))
  {
    target[j++] = '%';
    target[j++] = hex_digit (((int)source[i]) >> 4);
    target[j++] = hex_digit (((int)source[i]) & 15);
    i++;
  }


  // "con", "aux", "prn", "nul" are not allowed,
  // either alone or followed by a dot -> replace first char.

  device = false;

  if (i9-i == 3 || (i9-i >= 4 && source[i+3] == '.'))
  {
    if (stricmp (source[i:3], "con") == 0 ||
        stricmp (source[i:3], "aux") == 0 ||
        stricmp (source[i:3], "prn") == 0 ||
        stricmp (source[i:3], "nul") == 0)
    {
      target[j++] = '%';
      target[j++] = hex_digit (((int)source[i]) >> 4);
      target[j++] = hex_digit (((int)source[i]) & 15);
      i++;
      device = true;
    }
  }


  // "com", "lpt" followed by a digit are not allowed,
  // either alone or followed by a dot -> replace all chars.

  if ((!device)
      && (i9-i == 4 || (i9-i >= 5 && source[i+4] == '.'))
      && isdigit (source[i+3]))
  {
    if (stricmp (source[i:3], "com") == 0 ||
        stricmp (source[i:3], "lpt") == 0)
    {
      target[j++] = '%';
      target[j++] = hex_digit (((int)source[i]) >> 4);
      target[j++] = hex_digit (((int)source[i]) & 15);
      i++;
    }
  }


  while (i < i9)
  {
    c = source[i];
    if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' ||
        c == '"'  || c == '<' || c == '>' || c == '|' || c < ' ')
    {
      target[j++] = '%';
      target[j++] = hex_digit (((int)c) >> 4);
      target[j++] = hex_digit (((int)c) & 15);
    }
    else
    {
      target[j++] = c;
    }

    i++;
  }


  i9 = strlen(source);
  while (i < i9)
  {
    target[j++] = '%';
    target[j++] = hex_digit (((int)source[i]) >> 4);
    target[j++] = hex_digit (((int)source[i]) & 15);
    i++;
  }
}

//---------------------------------------------------------------------------

// url.filename is a list of simple filenames separated by '/'.
// the function modifies each simple filename (enclosed by '/').
// finally it appends a default filename if it ends with '/'.

public int convert_url_to_local_filename (URL           url,
                                          out char[260] filename,
                                          string        default_filename = "dir.html")
{
  int  i, start, len, j, i9;
  char name[url.filename'length*3];

  clear filename;

  if (url.filename[0] != '/')
    return CNV_NOT_ABSOLUTE;

  i = 1;
  j = 0;

  i9 = strlen(url.filename);

  for (;;)
  {
    if (j + 1 > filename'length)
      return CNV_TOO_LONG;
    j = strcat (ref filename, "/");

    if (i == i9)
      break;

    start = i;   // start of component

    while (i < i9 && url.filename[i] != '/')
      i++;

    len = i - start;   // length of component

    // note: components of length 0, ex: "//", are ignored.

    normalize_simple_name (url.filename[start:len], out name);
    if (j + strlen(name) > filename'length)
      return CNV_TOO_LONG;
    j = strcat (ref filename, name);

    if (i == i9)
      break;

    i++;   // skip '/'
  }

  if (filename[j-1] == '/')   // ends with '/'
  {
    if (j + default_filename'length > filename'length)
      return CNV_TOO_LONG;
    strcat (ref filename, default_filename);
  }

  return 0;
}

//---------------------------------------------------------------------------
