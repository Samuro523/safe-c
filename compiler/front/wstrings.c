
// wstrings.c

from std use strings;

/**********************************************************************/

public void wstring_to_string (wstring w, out string s)
{
  int i;
  
  clear s;
  i = 0;
  
  while (i < w'length)
  {
    wchar wc = w[i];

    if (wc == Lnul)
      break;

    if (wc <= (wchar)255)
      s[i] = (char)(int)wc;
    else
      s[i] = '?';
      
    i++;
  }
}

/**********************************************************************/

public void string_to_wstring (string s, out wstring w)
{
  int i;
  
  clear w;
  i = 0;
  
  while (i < s'length)
  {
    char c = s[i];

    if (c == nul)
      break;

    w[i] = (wchar)(int)c;

    i++;
  }
}

/**********************************************************************/

public bool is_all_in_lowercase (wstring s)
{
  int i = 0;

  while (i < s'length)
  {
    wchar wc = s[i];

    if (wc == Lnul)
      break;

    if (wc >= L'A' && wc <= L'Z')
      return false;
      
    i++;
  }

  return true;
}

/**********************************************************************************/

public bool is_all_in_uppercase (wstring s)
{
  int i = 0;

  while (i < s'length)
  {
    wchar wc = s[i];

    if (wc == Lnul)
      break;

    if (wc >= L'a' && wc <= L'z')
      return false;
      
    i++;
  }

  return true;
}

/**********************************************************************************/

public bool has_at_least_one_letter (wstring s)
{
  int i = 0;

  while (i < s'length)
  {
    wchar wc = s[i];

    if (wc == Lnul)
      break;

    if (wc >= L'a' && wc <= L'z')
      return true;
    if (wc >= L'A' && wc <= L'Z')
      return true;
      
    i++;
  }
  
  return false;
}

/**********************************************************************************/

public void to_lower_case (ref string s)
{
  int i, len;
  len = strlen(s);
  for (i=0; i<len; i++)
  {
    ref char c = s[i];
    c = tolower (c);
  }
}

/**********************************************************************************/
