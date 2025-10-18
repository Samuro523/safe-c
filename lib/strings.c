
// strings.c

use arithm, strformat;

//---------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------

package WriteIntoString

  packed struct ContextWriteIntoString
  {
    char *buffer;
    int  ofs, len;
  }

  int Put (PUT_CONTEXT context, string s);

end WriteIntoString;

//---------------------------------------------------------------

package body WriteIntoString

  public int Put (PUT_CONTEXT context, string s)
  {
    ref ContextWriteIntoString c = *((ContextWriteIntoString *)context);

    if (s'length > c.len - c.ofs)
      return -1;

    c.buffer[c.ofs : s'length] = s;
    c.ofs += s'length;

    return 0;
  }

end WriteIntoString;

//---------------------------------------------------------------

// causes a runtime error if the target buffer is too small
// or if the format string has a bad format.
// returns the actual length of the result.

public int sprintf (out string buffer, string format, object[] arg)
{
  ContextWriteIntoString c = {&buffer, 0, buffer'length};

  clear buffer;
  assert (format_string ((byte *)&c, Put, format, arg) == 0);
  return c.ofs;
}

//---------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------


//---------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------

package ReadFromString

  packed struct ContextReadFromString
  {
    char *buffer;
    int  ofs, len;
  }

  int Get (GET_CONTEXT context, out char ch);
  void UnGet (GET_CONTEXT context);

end ReadFromString;

//---------------------------------------------------------------

package body ReadFromString

  // user function that must return 0 if OK, -1 if read error, -2 if end-of-stream.

  public int Get (PUT_CONTEXT context, out char ch)
  {
    ref ContextReadFromString c = *((ContextReadFromString *)context);

    if (c.ofs < c.len)
    {
      ch = c.buffer[c.ofs++];
      return 0;
    }

    clear ch;
    return -2;
  }

  public void UnGet (PUT_CONTEXT context)
  {
    ref ContextReadFromString c = *((ContextReadFromString *)context);
    c.ofs--;
  }

end ReadFromString;

//---------------------------------------------------------------

public int sscanf (string buffer, string format, out object[] arg)
{
  ContextReadFromString c = {&buffer, 0, strlen(buffer)};
  int rc;

  rc = scan_string ((byte *)&c, Get, UnGet, format, out arg);
  if (rc < 0)
    return rc;

  if (c.ofs != c.len)
    return +1;   // warning: extra characters remain in the buffer.

  return 0;
}

//---------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------

// copy a string
// causes a runtime error if the target buffer is too small.

public int strcpy (out string dest, string src)
{
  int  i;
  char c;

  clear dest;

  for (i=0; i<src'length; i++)
  {
    c = src[i];
    if (c == nul)
      break;
    dest[i] = c;
  }

  return i;
}

//---------------------------------------------------------------

// copy a string but stop after 'len' chars.
// causes a runtime error if the target buffer is too small.
// returns the actual length of the result.
// a runtime occurs if len < 0.

public int strncpy (out string dest, string src, int len)
{
  int  i, length;
  char c;

  clear dest;

  assert (len >= 0);

  length = src'length;
  if (len < length)
    length = len;

  for (i=0; i<length; i++)
  {
    c = src[i];
    if (c == nul)
      break;
    dest[i] = c;
  }

  return i;
}

//---------------------------------------------------------------

// append a string to another string
// causes a runtime error if the target buffer is too small.

public int strcat (ref string dest, string src)
{
  int  i, j;
  char c;

  for (i=0; i<dest'length && dest[i] != nul; i++)
    ;

  for (j=0; j<src'length; j++)
  {
    c = src[j];
    if (c == nul)
      break;
    dest[i++] = c;
  }

  if (i < dest'length)
    dest[i] = nul;

  return i;
}

//---------------------------------------------------------------

// returns active length of string

public int strlen (string s)
{
  int i;
  for (i=0; i<s'length && s[i] != nul; i++)
    ;
  return i;
}

//---------------------------------------------------------------

public int strcatf (ref string buffer, string format, object[] arg)
{
  int  i;

  for (i=0; i<buffer'length && buffer[i] != nul; i++)
    ;

  return sprintf (out buffer[i:buffer'length-i], format, arg);
}

//---------------------------------------------------------------

// compare two strings
// returns -1 if a<b, 0 if equal, +1 if a>b

public int strcmp (string a, string b)
{
  int  len, i;
  char ca, cb;

  len = (a'length < b'length) ? a'length : b'length;

  for (i=0; i<len; i++)
  {
    ca = a[i];
    cb = b[i];

    if (ca == cb && ca != nul)
      continue;

    if (ca < cb)
      return -1;
    if (ca > cb)
      return +1;
    if (ca == nul)  // both nuls
      return 0;
  }

  if (a'length < b'length)
    return b[i] == nul ? 0 : -1;

  if (a'length > b'length)
    return a[i] == nul ? 0 : +1;

  return 0;
}

//---------------------------------------------------------------

// compare two strings with insensitive case
// returns -1 if a<b, 0 if equal, +1 if a>b

public int stricmp (string a, string  b)
{
  int  i, len;
  char ca, cb;

  len = (a'length < b'length) ? a'length : b'length;

  for (i=0; i<len; i++)
  {
    ca = a[i];
    cb = b[i];

    if (ca == cb && ca != nul)
      continue;

    if (ca >= 'a' && ca <= 'z')
      ca = (char)((int)ca + ((int)'A' - (int)'a'));
    if (cb >= 'a' && cb <= 'z')
      cb = (char)((int)cb + ((int)'A' - (int)'a'));

    if (ca < cb)
      return -1;
    if (ca > cb)
      return +1;

    if (ca == nul)  // both nuls
      return 0;
  }

  if (a'length < b'length)
    return b[i] == nul ? 0 : -1;

  if (a'length > b'length)
    return a[i] == nul ? 0 : +1;

  return 0;
}

//---------------------------------------------------------------

// compare two strings but stop after 'maxlen' chars
// returns -1 if a<b, 0 if equal, +1 if a>b

public int strncmp (string a, string b, int maxlen)
{
  int  len, i;
  char ca, cb;

  assert (maxlen >= 0);

  len = (a'length < b'length) ? a'length : b'length;
  if (len > maxlen)
    len = maxlen;

  for (i=0; i<len; i++)
  {
    ca = a[i];
    cb = b[i];

    if (ca == cb && ca != nul)
      continue;

    if (ca < cb)
      return -1;
    if (ca > cb)
      return +1;
    if (ca == nul)  // both nuls
      return 0;
  }

  if (len == maxlen)
    return 0;

  if (a'length < b'length)
    return b[i] == nul ? 0 : -1;

  if (a'length > b'length)
    return a[i] == nul ? 0 : +1;

  return 0;
}

//---------------------------------------------------------------

// compare two strings with insensitive case but stop after 'maxlen' chars
// returns -1 if a<b, 0 if equal, +1 if a>b

public int strnicmp (string a, string  b, int maxlen)
{
  int  i, len;
  char ca, cb;

  assert (maxlen >= 0);

  len = (a'length < b'length) ? a'length : b'length;
  if (len > maxlen)
    len = maxlen;

  for (i=0; i<len; i++)
  {
    ca = a[i];
    cb = b[i];

    if (ca == cb && ca != nul)
      continue;

    if (ca >= 'a' && ca <= 'z')
      ca = (char)((int)ca + ((int)'A' - (int)'a'));
    if (cb >= 'a' && cb <= 'z')
      cb = (char)((int)cb + ((int)'A' - (int)'a'));

    if (ca < cb)
      return -1;
    if (ca > cb)
      return +1;

    if (ca == nul)  // both nuls
      return 0;
  }

  if (len == maxlen)
    return 0;

  if (a'length < b'length)
    return b[i] == nul ? 0 : -1;

  if (a'length > b'length)
    return a[i] == nul ? 0 : +1;

  return 0;
}

//---------------------------------------------------------------

// compare two memory area
// returns -1 if a<b, 0 if equal, +1 if a>b
// a runtime error occurs if both areas don't have the same length

public int memcmp (byte[] a, byte[] b)
{
  int i;

  assert a'length == b'length;

#begin unsafe
  i = 0;

  if (a'length >= 8)
  {
    int count = a'length >> 3;
    int j;
    for (j=0; j<count; j++)
    {
      if (((int8*)&a)[j] != ((int8*)&b)[j])
        break;
    }

    i = j << 3;
  }

  for (; i<a'length; i++)
  {
    if (((byte*)&a)[i] == ((byte*)&b)[i])
      continue;

    if (((byte*)&a)[i] < ((byte*)&b)[i])
      return -1;
    else
      return +1;
  }
#end unsafe

  return 0;
}

//---------------------------------------------------------------

// find a char in a string
// returns position or -1 if not found

public int strchr (string s, char c)
{
  int  i;
  char ch;

  for (i=0; i<s'length; i++)
  {
    ch = s[i];
    if (ch == nul)
      break;
    if (ch == c)
      return i;
  }

  return -1;
}

//---------------------------------------------------------------

// find last occurence of char in a string
// returns position or -1 if not found

public int strrchr (string s, char c)
{
  int i, len;

  len = strlen(s);

  for (i=len-1; i>=0; i--)
  {
    if (s[i] == c)
      return i;
  }

  return -1;
}

//---------------------------------------------------------------

// find a fragment in a text
// returns position, or -1 if not found

public int strstr (string text, string fragment)
{
  int len, ofs, i, j, k;

  len = strlen(fragment);
  ofs = strlen(text) - len;

  for (i=0; i<=ofs; i++)
  {
    for (j=0,k=i; j<len; j++,k++)
    {
      if (text[k] != fragment[j])
        break;
    }

    if (j == len)
      return i;
  }

  return -1;
}

//---------------------------------------------------------------

// find a fragment in a text with insensitive case
// returns position, or -1 if not found

public int stristr (string text, string fragment)
{
  int  len, ofs, i, j, k;
  char ca, cb;

  len = strlen(fragment);
  ofs = strlen(text) - len;

  for (i=0; i<=ofs; i++)
  {
    for (j=0,k=i; j<len; j++,k++)
    {
      if (text[k] == fragment[j])
        continue;

      ca = text[k];
      cb = fragment[j];

      if (ca >= 'a' && ca <= 'z')
        ca = (char)((int)ca + ((int)'A' - (int)'a'));

      if (cb >= 'a' && cb <= 'z')
        cb = (char)((int)cb + ((int)'A' - (int)'a'));

      if (ca != cb)
        break;
    }

    if (j == len)
      return i;
  }

  return -1;
}

//---------------------------------------------------------------

public char tolower (char c)
{
  if (c < 'A' || c > 'Z')
    return c;
  return (char)((int)c + ((int)'a' - (int)'A'));
}

//---------------------------------------------------------------

public char toupper (char c)
{
  if (c < 'a' || c > 'z')
    return c;
  return (char)((int)c + ((int)'A' - (int)'a'));
}

//---------------------------------------------------------------

public void trim (ref string s)    // remove leading and trailing white space (<= 32)
{
  int len = strlen(s);
  int i, j;

  for (i=len-1; i>=0; i--)
  {
    if (s[i] > ' ')
      break;
  }

  i++;
  if (i < len)
  {
    s[i] = nul;
    len = i;
  }

  for (i=0; i<len; i++)
  {
    if (s[i] > ' ')
      break;
  }

  if (i > 0)
  {
    j = 0;
    while (i < len)
      s[j++] = s[i++];
    s[j] = nul;
  }
}

//---------------------------------------------------------------

public void ltrim (ref string s)   // remove leading white space (<= 32)
{
  int len = strlen(s);
  int i, j;

  for (i=0; i<len; i++)
  {
    if (s[i] > ' ')
      break;
  }

  if (i > 0)
  {
    j = 0;
    while (i < len)
      s[j++] = s[i++];
    s[j] = nul;
  }
}

//---------------------------------------------------------------

public void rtrim (ref string s)   // remove trailing white space (<= 32)
{
  int len = strlen(s);
  int i;

  for (i=len-1; i>=0; i--)
  {
    if (s[i] > ' ')
      break;
  }

  i++;
  if (i < len)
    s[i] = nul;
}

//---------------------------------------------------------------------------------

public bool isalpha (char c)   // A-Z a-z
{
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

//---------------------------------------------------------------

public bool isdigit (char c)   // 0-9
{
  return c >= '0' && c <= '9';
}

//---------------------------------------------------------------

public bool isxdigit (char c)  // 0-9 A-F a-f
{
  return (c >= '0' && c <= '9') ||
         (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

//---------------------------------------------------------------

// return cost to convert string a to string b by inserting, deleting or replacing characters.
// returns 0 if strings are identical.

public
uint2 levenshtein_distance (string a,
                            string b,
                            uint2  cost_insert  = 1,
                            uint2  cost_delete  = 1,
                            uint2  cost_replace = 1)
{
  int      a_len = strlen(a);
  int      b_len = strlen(b);
  int      dim = b_len + 1;
  int      i, j;
  uint2    cost, dist;
  uint2[]^ pd = new uint2[2*dim];

  {
    ref uint2[] d = pd^;

    for (j=0; j<=b_len; j++)  // first line
      d[j] = (uint2)((uint)j * cost_insert);

    for (i=1; i<=a_len; i++)
    {
      d[(i & 1)*dim] = (uint2)i;  // col 0
      for (j=1; j<=b_len; j++)
      {
        cost = (uint2)(cost_replace * (uint2)(a[i-1] != b[j-1]));
        d[(i&1)*dim + j] = (uint2)umin3 (d[((i-1)&1)*dim + j  ] + cost_delete,  // delete
                                         d[(i&1)    *dim + j-1] + cost_insert,  // insert
                                         d[((i-1)&1)*dim + j-1] + cost);        // replace
      }
    }

    dist = d[(a_len & 1)*dim + b_len];
  }

  free pd;

  return dist;
}

//---------------------------------------------------------------

// return similarity coefficient [0.0 .. 1.0] between the set of characters in strings a and b.
// note: this function evaluates only letters [A..Z], upper and lower case, possibly with accents,
// and ignores all other symbols, i.e. space and punctuation.
// when asymetric is true, the function checks only if the letters of A exist in B, not if those in B exist in A.

public
float jaccard_coefficient (string a, string b, bool asymetric = false)
{
  char c;
  int  i, num, den;
  int[26] fa, fb;

  clear fa, fb;

  for (i=0; i<a'length && a[i]!=nul; i++)
  {
    c = toupper (normalize_extended_character (a[i]));
    if (c >= 'A' && c <= 'Z')
      fa[(uint)c - 65]++;
  }

  for (i=0; i<b'length && b[i]!=nul; i++)
  {
    c = toupper (normalize_extended_character (b[i]));
    if (c >= 'A' && c <= 'Z')
      fb[(uint)c - 65]++;
  }

  num = 0;
  for (i=0; i<fa'length; i++)
    num += min (fa[i], fb[i]);

  den = 0;
  if (asymetric)
  {
    for (i=0; i<fa'length; i++)
      den += fa[i];
  }
  else
  {
    for (i=0; i<fa'length; i++)
      den += max (fa[i], fb[i]);
  }

  if (den == 0)
    return 0.0;

  return (float)num / (float)den;
}

//---------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------

package WWriteIntoString

  packed struct WContextWriteIntoString
  {
    wchar *buffer;
    int   ofs, len;
  }

  int WPut (WPUT_CONTEXT context, wstring s);

end WWriteIntoString;

//---------------------------------------------------------------

package body WWriteIntoString

  public int WPut (WPUT_CONTEXT context, wstring s)
  {
    ref WContextWriteIntoString c = *((WContextWriteIntoString *)context);

    if (s'length > c.len - c.ofs)
      return -1;

    c.buffer[c.ofs : s'length] = s;
    c.ofs += s'length;

    return 0;
  }

end WWriteIntoString;

//---------------------------------------------------------------

public int wsprintf (out wstring buffer, wstring format, object[] arg)
{
  WContextWriteIntoString c = {&buffer, 0, buffer'length};

  clear buffer;
  assert (wformat_string ((short *)&c, WPut, format, arg) == 0);
  return c.ofs;
}

//---------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------


//---------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------

package WReadFromString

  packed struct WContextReadFromString
  {
    wchar *buffer;
    int   ofs, len;
  }

  int WGet (WGET_CONTEXT context, out wchar ch);
  void WUnGet (WGET_CONTEXT context);

end WReadFromString;

//---------------------------------------------------------------

package body WReadFromString

  // user function that must return 0 if OK, -1 if read error, -2 if end-of-stream.

  public int WGet (WPUT_CONTEXT context, out wchar ch)
  {
    ref WContextReadFromString c = *((WContextReadFromString *)context);

    if (c.ofs < c.len)
    {
      ch = c.buffer[c.ofs++];
      return 0;
    }

    clear ch;
    return -2;
  }

  public void WUnGet (WPUT_CONTEXT context)
  {
    ref WContextReadFromString c = *((WContextReadFromString *)context);
    c.ofs--;
  }

end WReadFromString;

//---------------------------------------------------------------

public int wsscanf (wstring buffer, wstring format, out object[] arg)
{
  WContextReadFromString c = {&buffer, 0, wstrlen(buffer)};
  int rc;

  rc = wscan_string ((short *)&c, WGet, WUnGet, format, out arg);
  if (rc < 0)
    return rc;

  if (c.ofs != c.len)
    return +1;   // warning: extra characters remain in the buffer.

  return 0;
}

//---------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------

// copy a string
// causes a runtime error if the target buffer is too small.

public int wstrcpy (out wstring dest, wstring src)
{
  int   i;
  wchar c;

  clear dest;

  for (i=0; i<src'length; i++)
  {
    c = src[i];
    if (c == Lnul)
      break;
    dest[i] = c;
  }

  return i;
}

//---------------------------------------------------------------

// copy a string but stop after 'len' chars.
// causes a runtime error if the target buffer is too small.
// returns the actual length of the result.
// a runtime occurs if len < 0.

public int wstrncpy (out wstring dest, wstring src, int len)
{
  int   i, length;
  wchar c;

  clear dest;

  assert (len >= 0);

  length = src'length;
  if (len < length)
    length = len;

  for (i=0; i<length; i++)
  {
    c = src[i];
    if (c == Lnul)
      break;
    dest[i] = c;
  }

  return i;
}

//---------------------------------------------------------------

// append a string to another string
// causes a runtime error if the target buffer is too small.

public int wstrcat (ref wstring dest, wstring src)
{
  int   i, j;
  wchar c;

  for (i=0; i<dest'length && dest[i] != Lnul; i++)
    ;

  for (j=0; j<src'length; j++)
  {
    c = src[j];
    if (c == Lnul)
      break;
    dest[i++] = c;
  }

  if (i < dest'length)
    dest[i] = Lnul;

  return i;
}

//---------------------------------------------------------------

// returns active length of string

public int wstrlen (wstring s)
{
  int i;
  for (i=0; i<s'length && s[i] != Lnul; i++)
    ;
  return i;
}

//---------------------------------------------------------------

public int wstrcatf (ref wstring buffer, wstring format, object[] arg)
{
  int  i;

  for (i=0; i<buffer'length && buffer[i] != Lnul; i++)
    ;

  return wsprintf (out buffer[i:buffer'length-i], format, arg);
}

//---------------------------------------------------------------

// compare two strings
// returns -1 if a<b, 0 if equal, +1 if a>b

public int wstrcmp (wstring a, wstring b)
{
  int   len, i;
  wchar ca, cb;

  len = (a'length < b'length) ? a'length : b'length;

  for (i=0; i<len; i++)
  {
    ca = a[i];
    cb = b[i];

    if (ca == cb && ca != Lnul)
      continue;

    if (ca < cb)
      return -1;
    if (ca > cb)
      return +1;
    if (ca == Lnul)  // both nuls
      return 0;
  }

  if (a'length < b'length)
    return b[i] == Lnul ? 0 : -1;

  if (a'length > b'length)
    return a[i] == Lnul ? 0 : +1;

  return 0;
}

//---------------------------------------------------------------

// compare two strings with insensitive case
// returns -1 if a<b, 0 if equal, +1 if a>b

public int wstricmp (wstring a, wstring  b)
{
  int   i, len;
  wchar ca, cb;

  len = (a'length < b'length) ? a'length : b'length;

  for (i=0; i<len; i++)
  {
    ca = a[i];
    cb = b[i];

    if (ca == cb && ca != Lnul)
      continue;

    if (ca >= L'a' && ca <= L'z')
      ca = (wchar)((int)ca + ((int)L'A' - (int)L'a'));
    if (cb >= L'a' && cb <= L'z')
      cb = (wchar)((int)cb + ((int)L'A' - (int)L'a'));

    if (ca < cb)
      return -1;
    if (ca > cb)
      return +1;

    if (ca == Lnul)  // both nuls
      return 0;
  }

  if (a'length < b'length)
    return b[i] == Lnul ? 0 : -1;

  if (a'length > b'length)
    return a[i] == Lnul ? 0 : +1;

  return 0;
}

//---------------------------------------------------------------

// compare two strings but stop after 'maxlen' chars
// returns -1 if a<b, 0 if equal, +1 if a>b

public int wstrncmp (wstring a, wstring b, int maxlen)
{
  int   len, i;
  wchar ca, cb;

  assert (maxlen >= 0);

  len = (a'length < b'length) ? a'length : b'length;
  if (len > maxlen)
    len = maxlen;

  for (i=0; i<len; i++)
  {
    ca = a[i];
    cb = b[i];

    if (ca == cb && ca != Lnul)
      continue;

    if (ca < cb)
      return -1;
    if (ca > cb)
      return +1;
    if (ca == Lnul)  // both nuls
      return 0;
  }

  if (len == maxlen)
    return 0;

  if (a'length < b'length)
    return b[i] == Lnul ? 0 : -1;

  if (a'length > b'length)
    return a[i] == Lnul ? 0 : +1;

  return 0;
}

//---------------------------------------------------------------

// compare two strings with insensitive case but stop after 'maxlen' chars
// returns -1 if a<b, 0 if equal, +1 if a>b

public int wstrnicmp (wstring a, wstring  b, int maxlen)
{
  int   i, len;
  wchar ca, cb;

  assert (maxlen >= 0);

  len = (a'length < b'length) ? a'length : b'length;
  if (len > maxlen)
    len = maxlen;

  for (i=0; i<len; i++)
  {
    ca = a[i];
    cb = b[i];

    if (ca == cb && ca != Lnul)
      continue;

    if (ca >= L'a' && ca <= L'z')
      ca = (wchar)((int)ca + ((int)L'A' - (int)L'a'));
    if (cb >= L'a' && cb <= L'z')
      cb = (wchar)((int)cb + ((int)L'A' - (int)L'a'));

    if (ca < cb)
      return -1;
    if (ca > cb)
      return +1;

    if (ca == Lnul)  // both nuls
      return 0;
  }

  if (len == maxlen)
    return 0;

  if (a'length < b'length)
    return b[i] == Lnul ? 0 : -1;

  if (a'length > b'length)
    return a[i] == Lnul ? 0 : +1;

  return 0;
}

//---------------------------------------------------------------

// find a char in a string
// returns position or -1 if not found

public int wstrchr (wstring s, wchar c)
{
  int   i;
  wchar ch;

  for (i=0; i<s'length; i++)
  {
    ch = s[i];
    if (ch == Lnul)
      break;
    if (ch == c)
      return i;
  }

  return -1;
}

//---------------------------------------------------------------

// find last occurence of char in a string
// returns position or -1 if not found

public int wstrrchr (wstring s, wchar c)
{
  int i, len;

  len = wstrlen(s);

  for (i=len-1; i>=0; i--)
  {
    if (s[i] == c)
      return i;
  }

  return -1;
}

//---------------------------------------------------------------

// find a fragment in a text
// returns position, or -1 if not found

public int wstrstr (wstring text, wstring fragment)
{
  int len, ofs, i, j, k;

  len = wstrlen(fragment);
  ofs = wstrlen(text) - len;

  for (i=0; i<=ofs; i++)
  {
    for (j=0,k=i; j<len; j++,k++)
    {
      if (text[k] != fragment[j])
        break;
    }

    if (j == len)
      return i;
  }

  return -1;
}

//---------------------------------------------------------------

// find a fragment in a text with insensitive case
// returns position, or -1 if not found

public int wstristr (wstring text, wstring fragment)
{
  int   len, ofs, i, j, k;
  wchar ca, cb;

  len = wstrlen(fragment);
  ofs = wstrlen(text) - len;

  for (i=0; i<=ofs; i++)
  {
    for (j=0,k=i; j<len; j++,k++)
    {
      if (text[k] == fragment[j])
        continue;

      ca = text[k];
      cb = fragment[j];

      if (ca >= L'a' && ca <= L'z')
        ca = (wchar)((int)ca + ((int)L'A' - (int)L'a'));

      if (cb >= L'a' && cb <= L'z')
        cb = (wchar)((int)cb + ((int)L'A' - (int)L'a'));

      if (ca != cb)
        break;
    }

    if (j == len)
      return i;
  }

  return -1;
}

//---------------------------------------------------------------

public wchar wtolower (wchar c)
{
  if (c < L'A' || c > L'Z')
    return c;
  return (wchar)((int)c + ((int)L'a' - (int)L'A'));
}

//---------------------------------------------------------------

public wchar wtoupper (wchar c)
{
  if (c < L'a' || c > L'z')
    return c;
  return (wchar)((int)c + ((int)L'A' - (int)L'a'));
}

//---------------------------------------------------------------

public void wtrim (ref wstring s)    // remove leading and trailing white space (<= 32)
{
  int len = wstrlen(s);
  int i, j;

  for (i=len-1; i>=0; i--)
  {
    if (s[i] > L' ')
      break;
  }

  i++;
  if (i < len)
  {
    s[i] = Lnul;
    len = i;
  }

  for (i=0; i<len; i++)
  {
    if (s[i] > L' ')
      break;
  }

  if (i > 0)
  {
    j = 0;
    while (i < len)
      s[j++] = s[i++];
    s[j] = Lnul;
  }
}

//---------------------------------------------------------------

public void wltrim (ref wstring s)   // remove leading white space (<= 32)
{
  int len = wstrlen(s);
  int i, j;

  for (i=0; i<len; i++)
  {
    if (s[i] > L' ')
      break;
  }

  if (i > 0)
  {
    j = 0;
    while (i < len)
      s[j++] = s[i++];
    s[j] = Lnul;
  }
}

//---------------------------------------------------------------

public void wrtrim (ref wstring s)   // remove trailing white space (<= 32)
{
  int len = wstrlen(s);
  int i;

  for (i=len-1; i>=0; i--)
  {
    if (s[i] > L' ')
      break;
  }

  i++;
  if (i < len)
    s[i] = Lnul;
}

//---------------------------------------------------------------------------------

public bool wisalpha (wchar c)   // A-Z a-z
{
  return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
}

//---------------------------------------------------------------

public bool wisdigit (wchar c)   // 0-9
{
  return c >= L'0' && c <= L'9';
}

//---------------------------------------------------------------

public bool wisxdigit (wchar c)  // 0-9 A-F a-f
{
  return (c >= L'0' && c <= L'9') ||
         (c >= L'A' && c <= L'F') ||
         (c >= L'a' && c <= L'f');
}

//---------------------------------------------------------------

// remove all accents, i.e. change all 'é', 'è', 'ê' into 'e'

public wchar wnormalize_extended_character (wchar c)
{
  wchar r;

  switch (c)
  {
    case L'â':
    case L'ä':
    case L'à':
    case L'å':
    case L'á':
      r = L'a';
      break;

    case L'Ä':
    case L'Å':
    case L'À':
      r = L'A';
      break;

    case L'ç':
      r = L'c';
      break;

    case L'Ç':
      r = L'C';
      break;

    case L'é':
    case L'ê':
    case L'ë':
    case L'è':
      r = L'e';
      break;

    case L'É':
      r = L'E';
      break;

    case L'ï':
    case L'î':
    case L'ì':
    case L'í':
      r = L'i';
      break;

    case L'ñ':
      r = L'n';
      break;

    case L'Ñ':
      r = L'N';
      break;

    case L'ô':
    case L'ö':
    case L'ò':
    case L'ó':
      r = L'o';
      break;

    case L'Ö':
      r = L'O';
      break;

    case L'ß':
      r = L's';
      break;

    case L'ü':
    case L'û':
    case L'ù':
    case L'ú':
      r = L'u';
      break;

    case L'Ü':
      r = L'U';
      break;

    case L'ÿ':
      r = L'y';
      break;

    default:
      r = c;
      break;
  }

  return r;
}

//---------------------------------------------------------------

public char normalize_extended_character (char c)
{
  return (char)(int)wnormalize_extended_character((wchar)(int)c);
}

//---------------------------------------------------------------

public void normalize_extended_characters (string source, out string target)
{
  int i;
  clear target;
  i = 0;
  while (i < source'length && source[i] != nul)
  {
    target[i] = normalize_extended_character (source[i]);
    i++;
  }
}

//---------------------------------------------------------------

public void wnormalize_extended_characters (wstring source, out wstring target)
{
  int i;
  clear target;
  i = 0;
  while (i < source'length && source[i] != Lnul)
  {
    target[i] = wnormalize_extended_character (source[i]);
    i++;
  }
}

//---------------------------------------------------------------

// return cost to convert string a to string b by inserting, deleting or replacing characters.
// 0 = strings are identical.
public
uint2 wlevenshtein_distance (wstring a,
                             wstring b,
                             uint2   cost_insert  = 1,
                             uint2   cost_delete  = 1,
                             uint2   cost_replace = 1)
{
  int      a_len = wstrlen(a);
  int      b_len = wstrlen(b);
  int      dim = b_len + 1;
  int      i, j;
  uint2    cost, dist;
  uint2[]^ pd = new uint2[2*dim];

  {
    ref uint2[] d = pd^;

    for (j=0; j<=b_len; j++)  // first line
      d[j] = (uint2)((uint)j * cost_insert);

    for (i=1; i<=a_len; i++)
    {
      d[(i & 1)*dim] = (uint2)i;  // col 0
      for (j=1; j<=b_len; j++)
      {
        cost = (uint2)(cost_replace * (uint2)(a[i-1] != b[j-1]));
        d[(i&1)*dim + j] = (uint2)umin3 (d[((i-1)&1)*dim + j  ] + cost_delete,  // delete
                                         d[(i&1)    *dim + j-1] + cost_insert,  // insert
                                         d[((i-1)&1)*dim + j-1] + cost);        // replace
      }
    }

    dist = d[(a_len & 1)*dim + b_len];
  }

  free pd;

  return dist;
}

//---------------------------------------------------------------

public
float wjaccard_coefficient (wstring a, wstring b, bool asymetric = false)
{
  wchar   c;
  int     i, num, den;
  int[26] fa, fb;

  clear fa, fb;

  for (i=0; i<a'length && a[i]!=Lnul; i++)
  {
    c = wtoupper (wnormalize_extended_character (a[i]));
    if (c >= L'A' && c <= L'Z')
      fa[(uint)c - 65]++;
  }

  for (i=0; i<b'length && b[i]!=Lnul; i++)
  {
    c = wtoupper (wnormalize_extended_character (b[i]));
    if (c >= L'A' && c <= L'Z')
      fb[(uint)c - 65]++;
  }

  num = 0;
  for (i=0; i<fa'length; i++)
    num += min (fa[i], fb[i]);

  den = 0;
  if (asymetric)
  {
    for (i=0; i<fa'length; i++)
      den += fa[i];
  }
  else
  {
    for (i=0; i<fa'length; i++)
      den += max (fa[i], fb[i]);
  }

  if (den == 0)
    return 0.0;

  return (float)num / (float)den;
}

//---------------------------------------------------------------
