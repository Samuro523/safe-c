
// xml.c : read XML file

use arithm, files, strings;

//-------------------------------------------------------------------------

// scanning information

const uint BUFFER_SIZE      = 64*1024;
const uint MAX_NAME_LENGTH  = 4*1024;

const uint AHEAD            = MAX_NAME_LENGTH + 16;  // look-ahead buffer must contain token

enum  XMODE { MODE_TAG, MODE_STRING, MODE_CDATA };

struct SCAN
{
  int      fd;
  bool     eof;                    // true = fd reached end-of-file
  int      col, line;              // of buffer index
  byte     buffer[BUFFER_SIZE+1];  // sentinel 0x00 at last pos
  uint     active_size;            // active size of buffer
  uint     index;                  // index of next char to read in buffer
  int      ch;                     // current character, or -1 if eof
  int      delimiter;              // string delimiter : apostrophe or quote
  XMODE    mode;
}

//-------------------------------------------------------------------------

// token information

enum TOKEN_TYP { TOKEN_PRAGMA, TOKEN_COMMENT, TOKEN_BEGIN_CDATA, TOKEN_DECL, TOKEN_OPEN_TAG,
                 TOKEN_END_TAG, TOKEN_CLOSE_TAG, TOKEN_EMPTY_CLOSE_TAG, TOKEN_NAME,
                 TOKEN_BEGIN_STRING, TOKEN_CHARACTER, TOKEN_END_STRING, TOKEN_END_CDATA,
                 /* last token is one of these : */ TOKEN_END_OF_FILE, TOKEN_ERROR };

struct TOKEN
{
  int       col, line;     // column & line of token, start at (1,1)

  TOKEN_TYP typ;

  // for TOKEN_CHARACTER :
  char      ch;

  // for TOKEN_DECL, TOKEN_OPEN_TAG, TOKEN_END_TAG and TOKEN_NAME :
  char      name[MAX_NAME_LENGTH];
}

//-------------------------------------------------------------------------

// parsing information

const int MAX_ATTRIBUTE_VALUE_SIZE = 4096;
const int MAX_TAG_VALUE_SIZE       = 16*1024*1024;  // 16 MB

struct LEVEL_INFO
{
  LEVEL_INFO^ next;
  string^     name;           // tag name, allocated on heap
  string^     value;          // tag value, allocated on heap
}

struct PARSE
{
  LEVEL_INFO^  info;             // null = starting
  int          level;
  bool         in_attribute;     // true = inside attribute
}

const int MAX_LEVELS = 1000;   // will use 4MB

//-------------------------------------------------------------------------

// xml structure : scanning + token information

const uint MAGIC  = 0x673AB3B2;

struct XML
{
  uint    magic;
  int     error;          // 0 (or -1 if error)
  char    error_msg[80];  // filled if error is -1
  SCAN    scan;           // scanning information
  TOKEN   token;          // token information
  PARSE   parse;          // parsing information
}

//-------------------------------------------------------------------------

// fill buffer with fresh data,
// at least AHEAD characters (size of a token) if possible
// must be called before scanning a token !
// returns 0 if OK, -1 if read error

int fill_buffer (ref SCAN s)
{
  uint  left;
  int   rc;

  if (s.eof)   // nothing more to read
    return 0;


  // nb characters left in buffer

  left = s.active_size - s.index;

  if (left > AHEAD)  // enough for scanning a token
    return 0;


  // move remaining characters to start of buffer

  s.buffer[0:left] = s.buffer[s.index:left];
  s.index = 0;
  s.active_size = left;


  // read fresh data

  rc = read (s.fd, out s.buffer[s.active_size : BUFFER_SIZE - s.active_size]);
  if (rc < 0)
  {
    s.buffer[s.active_size] = 0x00;    // sentinel 0x00 signals end of chunk
    s.eof = true;
    return -1;    // read error
  }

  s.active_size += (uint)rc;

  s.buffer[s.active_size] = 0x00;    // sentinel 0x00 signals end of chunk

  if (rc < (int)(BUFFER_SIZE - s.active_size))  // could not fill buffer
    s.eof = true;

  if (s.col == 0 && s.line == 1)   // first call : skip UFT8 header if any
  {
    const byte[] BOOM_UTF8 = {0xEF, 0xBB, 0xBF};
    if (s.active_size >= 3 && memcmp (s.buffer[0:3], BOOM_UTF8) == 0)
    {
      s.index += 3;
      rc -= 3;
    }
  }

  return 0;
}

//-------------------------------------------------------------------------

// load next character (or -1 if end-of-buffer) in x.scan.ch

void next_ch (ref SCAN s)
{
  if (s.index == s.active_size)   // end-of-buffer
    s.ch = -1;
  else
  {
    if (s.ch == 10)   // new-line
    {
      s.col = 0;
      s.line++;
    }

    s.ch = s.buffer[s.index++];
    s.col++;
  }
}

//-------------------------------------------------------------------------
int next_token (ref XML x, bool is_content);
//-------------------------------------------------------------------------

public int wopen_xml (out XML xml, wstring filename)
{
  int fd;

  clear xml;

  fd = wopen (filename);
  if (fd < 0)
  {
    strcpy (out xml.error_msg, "cannot open file");
    return -1;
  }

  xml.magic = MAGIC;

  xml.scan.fd          = fd;
  xml.scan.col         = 0;
  xml.scan.line        = 1;
  xml.scan.active_size = 0;
  xml.scan.index       = 0;
  xml.scan.ch          = 32;
  xml.scan.mode        = MODE_TAG;

  xml.token.col  = xml.scan.col;
  xml.token.line = xml.scan.line;

  if (fill_buffer (ref xml.scan) < 0)
  {
    xml.token.typ = TOKEN_ERROR;
    xml.error = -1;
    strcpy (out xml.error_msg, "file read error");
    return 0;   // error will be returned later
  }

  next_ch (ref xml.scan);

  if (next_token (ref xml, false) < 0)
    return 0;   // error will be returned later

  return 0;
}

//-------------------------------------------------------------------------

void wcstrcpy (out wstring dest, string src)
{
  int i;

  clear dest;

  for (i=0; i<src'length && src[i] != nul; i++)
    dest[i] = (wchar)(uint)src[i];
}

//----------------------------------------------------------------------------

public int open_xml (out XML xml, string filename)
{
  wchar ws[MAX_FILENAME_LENGTH];
  wcstrcpy (out ws, filename);
  return wopen_xml (out xml, ws);
}

//-------------------------------------------------------------------------

public int close_xml (ref XML xml)
{
  LEVEL_INFO^ p, q;

  if (xml.magic != MAGIC)
    return -1;

  close (xml.scan.fd);

  p = xml.parse.info;
  while (p != null)
  {
    q = p;
    p = p^.next;
    free q^.name;
    free q^.value;
    free q;
  }

  clear xml;

  return 0;
}

//-------------------------------------------------------------------------

bool is_base_char (int c)
{
  return (c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A) ||
         (c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6) ||
         (c >= 0xF8 && c <= 0xFF) || (c == (int)'_') || (c == (int)':');
}

//-------------------------------------------------------------------------

bool is_next_char (int c)
{
  return (is_base_char (c)
          || (c >= 0x30 && c <= 0x39)
          || (c == (int)'.') || (c == (int)'-') || (c == 0xB7));
}

//-------------------------------------------------------------------------

// returns 0 if OK, -1 if name is too long

int scan_name (ref XML x)
{
  int len;

  len = 0;

  x.token.name[len++] = (char)x.scan.ch;

  for (;;)
  {
    next_ch (ref x.scan);

    if (!is_next_char (x.scan.ch))
      break;

    if (len == (int)MAX_NAME_LENGTH)  // already full
    {
      x.token.name[0] = nul;
      return -1;
    }

    x.token.name[len++] = (char)x.scan.ch;
  }

  if (len < (int)MAX_NAME_LENGTH)
    x.token.name[len] = nul;

  return 0;
}

//-------------------------------------------------------------------------

int next_token (ref XML x, bool is_content)
{
  if (x.error != 0)
    return -1;

  x.token.typ     = TOKEN_ERROR;
  x.token.ch      = nul;
  x.token.name[0] = nul;
  x.token.col     = x.scan.col;
  x.token.line    = x.scan.line;

  if (fill_buffer (ref x.scan) < 0)
  {
    x.token.typ = TOKEN_ERROR;
    x.error = -1;
    strcpy (out x.error_msg, "file read error");
    return -1;
  }

  if (x.scan.mode == MODE_TAG)
  {
    if (x.scan.ch == (int)'<')
    {      
      next_ch (ref x.scan);

      if (x.scan.ch == (int)'?')   // pragma
      {
        next_ch (ref x.scan);

        for (;;)
        {
          if (x.scan.ch < 0)   // end-of-file
          {
            x.token.typ = TOKEN_ERROR;
            x.error = -1;
            strcpy (out x.error_msg, "unexpected end-of-file");
            return -1;
          }

          if (x.scan.ch == (int)'?' &&
              x.scan.buffer[x.scan.index] == (byte)'>')  // "?>"
            break;

          if (fill_buffer (ref x.scan) < 0)
          {
            x.token.typ = TOKEN_ERROR;
            x.error = -1;
            strcpy (out x.error_msg, "file read error");
            return -1;
          }
          next_ch (ref x.scan);
        }

        next_ch (ref x.scan);   // skip '?'
        next_ch (ref x.scan);   // skip '>'

        x.token.typ = TOKEN_PRAGMA;
        return 0;
      }

      if (x.scan.ch == (int)'!')
      {
        next_ch (ref x.scan);    // skip '!'

        if (x.scan.ch == (int)'-')   // comment
        {
          next_ch (ref x.scan);     // skip '-'

          if (x.scan.ch != (int)'-')
          {
            x.token.typ = TOKEN_ERROR;
            x.error = -1;
            strcpy (out x.error_msg, "<!-- expected");
            return -1;
          }

          next_ch (ref x.scan);   // skip second '-'

          for (;;)
          {
            if (x.scan.ch < 0)   // end-of-file
            {
              x.token.typ = TOKEN_ERROR;
              x.error = -1;
              strcpy (out x.error_msg, "unexpected end-of-file");
              return -1;
            }

            if (x.scan.ch == (int)'-' &&
                x.scan.buffer[x.scan.index] == (byte)'-' &&
                x.scan.buffer[x.scan.index+1] == (byte)'>')  // "-->"
              break;

            if (fill_buffer (ref x.scan) < 0)
            {
              x.token.typ = TOKEN_ERROR;
              x.error = -1;
              strcpy (out x.error_msg, "file read error");
              return -1;
            }
            next_ch (ref x.scan);
          }

          next_ch (ref x.scan);   // skip '-'
          next_ch (ref x.scan);   // skip '-'
          next_ch (ref x.scan);   // skip '>'

          x.token.typ = TOKEN_COMMENT;
          return 0;
        }
        else if (x.scan.ch == (int)'[')   // <![CDATA[
        {
          if (memcmp (x.scan.buffer[x.scan.index:6], "CDATA[") != 0)
          {
            x.token.typ = TOKEN_ERROR;
            x.error = -1;
            strcpy (out x.error_msg, "<![CDATA[ expected");
            return -1;
          }

          next_ch (ref x.scan);     // skip '['
          next_ch (ref x.scan);     // skip 'C'
          next_ch (ref x.scan);     // skip 'D'
          next_ch (ref x.scan);     // skip 'A'
          next_ch (ref x.scan);     // skip 'T'
          next_ch (ref x.scan);     // skip 'A'
          next_ch (ref x.scan);     // skip '['

          x.scan.mode = MODE_CDATA;

          x.token.typ = TOKEN_BEGIN_CDATA;
          return 0;
        }
        else if (is_base_char (x.scan.ch))
        {
          if (scan_name (ref x) < 0)
          {
            x.token.typ = TOKEN_ERROR;
            x.error = -1;
            strcpy (out x.error_msg, "<!name is too long");
            return -1;
          }

          x.token.typ = TOKEN_DECL;
          return 0;
        }
        else
        {
          x.token.typ = TOKEN_ERROR;
          x.error = -1;
          strcpy (out x.error_msg, "<!-- , <![CDATA or [!name expected");
          return -1;
        }
      }

      if (x.scan.ch == (int)'/')
      {
        next_ch (ref x.scan);   // skip '/'

        if (!is_base_char (x.scan.ch))
        {
          x.token.typ = TOKEN_ERROR;
          x.error = -1;
          strcpy (out x.error_msg, "</name expected");
          return -1;
        }

        if (scan_name (ref x) < 0)
        {
          x.token.typ = TOKEN_ERROR;
          x.error = -1;
          strcpy (out x.error_msg, "</name is too long");
          return -1;
        }

        x.token.typ = TOKEN_END_TAG;
        return 0;
      }


      if (is_base_char (x.scan.ch))
      {
        if (scan_name (ref x) < 0)
        {
          x.token.typ = TOKEN_ERROR;
          x.error = -1;
          strcpy (out x.error_msg, "<name is too long");
          return -1;
        }

        x.token.typ = TOKEN_OPEN_TAG;
        return 0;
      }

      x.token.typ = TOKEN_ERROR;
      x.error = -1;
      strcpy (out x.error_msg, "illegal tag");
      return -1;
    }


    if (!is_content)
    {
      if (x.scan.ch == (int)'>')
      {
        next_ch (ref x.scan);   // skip '>'

        x.token.typ = TOKEN_CLOSE_TAG;
        return 0;
      }

      if (x.scan.ch == (int)'/')
      {
        next_ch (ref x.scan);   // skip '/'

        if (x.scan.ch != (int)'>')
        {
          x.token.typ = TOKEN_ERROR;
          x.error = -1;
          strcpy (out x.error_msg, "/> expected");
          return -1;
        }

        next_ch (ref x.scan);   // skip '>'

        x.token.typ = TOKEN_EMPTY_CLOSE_TAG;
        return 0;
      }

      if (is_base_char (x.scan.ch))
      {
        if (scan_name (ref x) < 0)
        {
          x.token.typ = TOKEN_ERROR;
          x.error = -1;
          strcpy (out x.error_msg, "name is too long");
          return -1;
        }

        x.token.typ = TOKEN_NAME;
        return 0;
      }

      if (x.scan.ch == (int)'\'' || x.scan.ch == (int)'\"')
      {
        x.scan.delimiter = x.scan.ch;

        next_ch (ref x.scan);   // skip apos or quote

        x.scan.mode = MODE_STRING;

        x.token.typ = TOKEN_BEGIN_STRING;
        return 0;
      }
    }

    if (x.scan.ch >= 0 && x.scan.ch <= 32)  // white space
    {
      for (;;)
      {
        next_ch (ref x.scan);

        if (x.scan.ch < 0 || x.scan.ch > 32)
          break;

        if (fill_buffer (ref x.scan) < 0)
        {
          x.token.typ = TOKEN_ERROR;
          x.error = -1;
          strcpy (out x.error_msg, "file read error");
          return -1;
        }
      }

      x.token.typ = TOKEN_CHARACTER;
      x.token.ch = (char)32;
      return 0;
    }

    if (x.scan.ch < 0)
    {
      x.token.typ = TOKEN_END_OF_FILE;
      return 0;
    }

    x.token.typ = TOKEN_CHARACTER;
    x.token.ch  = (char)x.scan.ch;

    next_ch (ref x.scan);

    return 0;
  }
  else if (x.scan.mode == MODE_STRING)
  {
    if (x.scan.ch == x.scan.delimiter)
    {
      x.scan.delimiter = x.scan.ch;

      next_ch (ref x.scan);   // skip delimiter

      x.scan.mode = MODE_TAG;

      x.token.typ = TOKEN_END_STRING;
      return 0;
    }

    if (x.scan.ch >= 0 && x.scan.ch <= 32)  // white space
    {
      for (;;)
      {
        next_ch (ref x.scan);

        if (x.scan.ch < 0 || x.scan.ch > 32)
          break;

        if (fill_buffer (ref x.scan) < 0)
        {
          x.token.typ = TOKEN_ERROR;
          x.error = -1;
          strcpy (out x.error_msg, "file read error");
          return -1;
        }
      }

      x.token.typ = TOKEN_CHARACTER;
      x.token.ch = (char)32;
      return 0;
    }

    if (x.scan.ch < 0)
    {
      x.token.typ = TOKEN_END_OF_FILE;
      return 0;
    }

    x.token.typ = TOKEN_CHARACTER;
    x.token.ch  = (char)x.scan.ch;

    next_ch (ref x.scan);

    return 0;
  }
  else if (x.scan.mode == MODE_CDATA)
  {
    if (x.scan.ch == (int)']' &&
        x.scan.buffer[x.scan.index] == (byte)']' &&
        x.scan.buffer[x.scan.index+1] == (byte)'>')
    {
      next_ch (ref x.scan);
      next_ch (ref x.scan);
      next_ch (ref x.scan);

      x.scan.mode = MODE_TAG;

      x.token.typ = TOKEN_END_CDATA;
      return 0;
    }

    if (x.scan.ch == (int)'&' &&
        x.scan.buffer[x.scan.index] == (byte)'g' &&
        x.scan.buffer[x.scan.index+1] == (byte)'t' &&
        x.scan.buffer[x.scan.index+2] == (byte)';')
    {
      next_ch (ref x.scan);
      next_ch (ref x.scan);
      next_ch (ref x.scan);
      next_ch (ref x.scan);

      x.token.typ = TOKEN_CHARACTER;
      x.token.ch  = '>';
      return 0;
    }

    if (x.scan.ch == 13)
    {
      next_ch (ref x.scan);

      if (x.scan.ch == 10)
        next_ch (ref x.scan);

      x.token.typ = TOKEN_CHARACTER;
      x.token.ch  = (char)10;
      return 0;
    }

    if (x.scan.ch < 0)
    {
      x.token.typ = TOKEN_END_OF_FILE;
      return 0;
    }

    x.token.typ = TOKEN_CHARACTER;
    x.token.ch  = (char)(x.scan.ch == 0 ? 32 : x.scan.ch);   // no byte 0

    next_ch (ref x.scan);

    return 0;
  }
  else
  {
    x.token.typ = TOKEN_ERROR;
    x.error = -1;
    strcpy (out x.error_msg, "intern error");
    return -1;
  }
}

//-------------------------------------------------------------------------

int parse_misc (ref XML x)
{
  int depth;

  for (;;)
  {
    if ((x.token.typ == TOKEN_CHARACTER && x.token.ch == (char)32) ||
         x.token.typ == TOKEN_PRAGMA ||
         x.token.typ == TOKEN_COMMENT)
    {
      if (next_token (ref x, false) < 0)
        return -1;
    }
    else if (x.token.typ == TOKEN_DECL)
    {
      depth = 1;

      for (;;)
      {
        if (next_token (ref x, false) < 0)
          return -1;

        if (x.token.typ == TOKEN_DECL)
        {
          depth++;
        }
        else if (x.token.typ == TOKEN_CLOSE_TAG)
        {
          depth--;
        }
        else if (x.token.typ == TOKEN_END_OF_FILE)
        {
          x.error = -1;
          strcpy (out x.error_msg, "unexpected end of file");
          return -1;
        }
        else if (x.token.typ == TOKEN_ERROR)
        {
          return -1;
        }

        if (depth == 0)
          break;
      }

      if (next_token (ref x, false) < 0)
        return -1;
    }
    else
    {
      return 0;
    }
  }
}

//-------------------------------------------------------------------------

int skip_white_space_token (ref XML x)
{
  while (x.token.typ == TOKEN_CHARACTER && x.token.ch == (char)32)
  {
    if (next_token (ref x, false) < 0)
      return -1;
  }

  return 0;
}

//-------------------------------------------------------------------------

// replace &quot; &apos; &amp; &gt; &lt; &#62; &#x5A;

package REPLACEMENTS

  struct XML_REF
  {
    string str;
    char   symbol;
  }

  const XML_REF xml_ref[] =
           {{"&gt;",'>'},
            {"&lt;",'<'},
            {"&amp;",'&'},
            {"&apos;",'\''},
            {"&quot;",'\"'}};

end REPLACEMENTS;

//-------------------------------------------------------------------------

void translate_xml_references (ref string s,
                                   int    length0,
                               out int    new_length)
{
  int length = length0;
  int i, j, len;

  for (i=0; i<length; i++)
  {
    if (s[i] != '&')
      continue;

    for (j=0; j<xml_ref'length; j++)
    {
      if (i + strlen(xml_ref[j].str) <= length &&
          memcmp (s[i : strlen(xml_ref[j].str)], xml_ref[j].str) == 0)
        break;
    }

    if (j<xml_ref'length)  // found
    {
      s[i] = xml_ref[j].symbol;
      len = length - (i+strlen(xml_ref[j].str));
      s[i+1 : len] = s[i+strlen(xml_ref[j].str) : len];
      length -= (strlen(xml_ref[j].str) - 1);
    }
    else if (i+1 < length && s[i+1] == '#')
    {
      int  k, digit=0, count, base;
      uint n;

      if (i+2 < length && s[i+2] == 'x')   // hex string
      {
        base = 16;
        k = i+3;
      }
      else   // decimal string
      {
        base = 10;
        k = i+2;
      }

      n = 0;
      count = 0;

      for (;;)
      {
        if (k >= length)
          break;

        if (s[k] >= '0' && s[k] <= '9')
          digit = (int)s[k] - (int)'0';
        else if (s[k] >= 'a' && s[k] <= 'f')
          digit = (int)s[k] - (int)'a' + 10;
        else if (s[k] >= 'A' && s[k] <= 'F')
          digit = (int)s[k] - (int)'A' + 10;
        else
          break;

        if (digit >= base)
          break;

        n = n * (uint)base + (uint)digit;
        if (n > 255)
          break;

        count++;
        k++;
      }

      if (count >= 1 && n > 0 && digit < base && n <= 255 &&
          k < length && s[k] == ';')
      {
        s[i] = (char)n;
        k++;
        s[i+1 : length-k] = s[k : length-k];
        length -= ((k - i) - 1);
      }
    }
  }

  new_length = length;
}

//-------------------------------------------------------------------------

// get next attribute name and value

public int get_xml_attribute (ref XML xml, out string name, out string value)
{
  char value0[MAX_ATTRIBUTE_VALUE_SIZE];
  int  value0_len, len;

  clear name, value;

  if (xml.magic != MAGIC)
    return -1;

  if (!xml.parse.in_attribute)
  {
    xml.error = -1;
    strcpy (out xml.error_msg, "get_xml_attribute() : incorrect call order");
    return -1;
  }

  if (xml.token.typ != TOKEN_NAME)
    return +1;     // no more attributes !

  sprintf (out name, "%.*s", name'length, xml.token.name);

  if (next_token (ref xml, false) < 0)
    return -1;

  if (skip_white_space_token (ref xml) < 0)
    return -1;

  if (xml.token.typ != TOKEN_CHARACTER && xml.token.ch != '=')
  {
    xml.error = -1;
    strcpy (out xml.error_msg, "'=' expected");
    return -1;
  }

  if (next_token (ref xml, false) < 0)
    return -1;

  if (skip_white_space_token (ref xml) < 0)
    return -1;

  if (xml.token.typ != TOKEN_BEGIN_STRING)
  {
    xml.error = -1;
    strcpy (out xml.error_msg, "\" or \' expected");
    return -1;
  }

  value0_len = 0;
  clear value0;

  for (;;)
  {
    if (next_token (ref xml, false) < 0)
      return -1;

    if (xml.token.typ == TOKEN_CHARACTER)
    {
      if (value0_len >= MAX_ATTRIBUTE_VALUE_SIZE)
      {
        xml.error = -1;
        strcpy (out xml.error_msg, "value is too long");
        return -1;
      }

      if (xml.token.ch != (char)32 ||
          (value0_len > 0 && value0[value0_len-1] != (char)32))
      {
        value0[value0_len++] = xml.token.ch;
      }
    }
    else if (xml.token.typ == TOKEN_END_STRING)
    {
      break;
    }
    else if (xml.token.typ == TOKEN_END_OF_FILE)
    {
      xml.error = -1;
      strcpy (out xml.error_msg, "unexpected end of file");
      return -1;
    }
    else if (xml.token.typ == TOKEN_ERROR)
    {
      return -1;
    }
    else
    {
      xml.error = -1;
      strcpy (out xml.error_msg, "intern error : unexpected token");
      return -1;
    }
  }

  // remove trailing space if any
  if (value0_len > 0 && value0[value0_len-1] == (char)32)
    value0_len--;

  translate_xml_references (ref value0, value0_len, out value0_len);

  len = min (value'length, value0_len);
  sprintf (out value, "%.*s", len, value0);

  if (next_token (ref xml, false) < 0)
    return -1;

  if (skip_white_space_token (ref xml) < 0)
    return -1;

  return 0;
}

//-------------------------------------------------------------------------

// obtain name of next element

public int get_xml_begin_element (ref XML xml, out string name)
{
  LEVEL_INFO^ p;
  string^     hname;

  clear name;

  if (xml.magic != MAGIC)
    return -1;

  if (xml.parse.info == null)    // outer level
  {
    if (parse_misc (ref xml) < 0)
      return -1;

    if (xml.token.typ == TOKEN_END_OF_FILE)
      return +1;     // no more tag found

    if (xml.token.typ == TOKEN_ERROR)
      return -1;
  }
  else    // nested level
  {
    if (xml.parse.in_attribute)   // still in attributes
    {
      // skip first the remaining attributes

      for (;;)
      {
        int  rc;
        char name2[8], value2[8];

        rc = get_xml_attribute (ref xml, out name2, out value2);
        if (rc == +1)
          break;
        if (rc < 0)
          return rc;

        _unused name2;
        _unused value2;
      }

      xml.parse.in_attribute = false;

      if (xml.token.typ == TOKEN_EMPTY_CLOSE_TAG)
      {
        return +1;   // no further nested tag
      }
      else if (xml.token.typ == TOKEN_CLOSE_TAG)
      {
        // rule: content !
        if (next_token (ref xml, is_content => true) < 0)
          return -1;
      }
      else
      {
        xml.error = -1;
        strcpy (out xml.error_msg, "> or /> expected");
        return -1;
      }
    }


    // no longer in attributes

    if (xml.token.typ == TOKEN_EMPTY_CLOSE_TAG ||
        xml.token.typ == TOKEN_END_TAG)
    {
      return +1;      // no further nested tag
    }


    // parse rule "content"

    for (;;)
    {
      if (xml.token.typ == TOKEN_CHARACTER)
      {
        XMODE   mode;
        string^ value0;
        int     value0_len;

        mode = xml.scan.mode;

        value0 = new string(0);
        value0_len = 0;

        while (xml.token.typ == TOKEN_CHARACTER)
        {
          if (value0_len == MAX_TAG_VALUE_SIZE)
          {
            xml.error = -1;
            strcpy (out xml.error_msg, "value is too long");
            free value0;
            return -1;
          }

          if (xml.token.ch != (char)32 ||
              (value0_len > 0 && value0^[value0_len-1] != (char)32) ||
              (value0_len == 0 && xml.parse.info^.value^'length > 0 &&
               xml.parse.info^.value^[xml.parse.info^.value^'length-1] != (char)32) ||
              xml.scan.mode == MODE_CDATA)
          {
            if (value0_len == value0^'length)  // value0 is full
            {
              string^ nval = new string (2*value0^'length+1);

              nval^[0:value0_len] = value0^;

              free value0;

              value0 = nval;
            }

            value0^[value0_len++] = xml.token.ch;
          }

          if (next_token (ref xml, true) < 0)
            return -1;
        }

        if (mode != MODE_CDATA)
        {
          // remove trailing space if any

          if (value0_len > 0 && value0^[value0_len-1] == (char)32)
            value0_len--;

          translate_xml_references (ref value0^, value0_len, out value0_len);
        }

        if (value0_len > 0)
        {
          string^ nval;

          if (xml.parse.info^.value^'length + value0_len > MAX_TAG_VALUE_SIZE)
          {
            xml.error = -1;
            strcpy (out xml.error_msg, "value is too long");
            free value0;
            return -1;
          }

          nval = new string (xml.parse.info^.value^'length + value0_len);
          nval^[0:xml.parse.info^.value^'length] = xml.parse.info^.value^;
          nval^[xml.parse.info^.value^'length:value0_len] = value0^[0 : value0_len];

          free xml.parse.info^.value;
          xml.parse.info^.value = nval;
        }

        free value0;
      }
      else if (xml.token.typ == TOKEN_BEGIN_CDATA ||
               xml.token.typ == TOKEN_END_CDATA)
      {
        if (next_token (ref xml, is_content => true) < 0)
          return -1;
      }
      else if (xml.token.typ == TOKEN_PRAGMA || xml.token.typ == TOKEN_COMMENT)
      {
        if (next_token (ref xml, is_content => true) < 0)
          return -1;
      }
      else if (xml.token.typ == TOKEN_OPEN_TAG)
      {
        break;
      }
      else if (xml.token.typ == TOKEN_END_TAG)
      {
        return +1;      // no further nested tag
      }
      else if (xml.token.typ == TOKEN_END_OF_FILE)
      {
        xml.error = -1;
        strcpy (out xml.error_msg, "unexpected end of file");
        return -1;
      }
      else if (xml.token.typ == TOKEN_ERROR)
      {
        return -1;
      }
      else
      {
        xml.error = -1;
        strcpy (out xml.error_msg, "intern error : unexpected token");
        return -1;
      }
    }
  }


  // open tag

  if (xml.token.typ != TOKEN_OPEN_TAG)
  {
    xml.error = -1;
    strcpy (out xml.error_msg, "<tag expected");
    return -1;
  }


  // add a new node to the parsing structure

  if (MAX_LEVELS == xml.parse.level)
  {
    xml.error = -1;
    strcpy (out xml.error_msg, "too many nested tags");
    return -1;
  }

  p = new LEVEL_INFO;

  hname = new string (strlen(xml.token.name));
  strcpy (out hname^, xml.token.name);

  p^.next = xml.parse.info;
  p^.name = hname;
  p^.value = new string(0);

  xml.parse.info = p;
  xml.parse.level++;

  if (next_token (ref xml, false) < 0)
    return -1;


  if (skip_white_space_token (ref xml) < 0)
    return -1;

  sprintf (out name, "%.*s", name'length, hname^);


  xml.parse.in_attribute = true;

  return 0;
}

//-------------------------------------------------------------------------

int parse_end_tag (ref XML x)
{
  LEVEL_INFO^ p;

  if (x.token.typ == TOKEN_EMPTY_CLOSE_TAG)
  {
    if (next_token (ref x, is_content => true) < 0)
      return -1;
  }
  else if (x.token.typ == TOKEN_END_TAG)
  {
    // parse name of end-tag and check name
    if (strcmp (x.parse.info^.name^, x.token.name) != 0)
    {
      x.error = -1;
      strcpy (out x.error_msg, "name of end-tag does not match");
      return -1;
    }

    if (next_token (ref x, false) < 0)
      return -1;

    if (skip_white_space_token (ref x) < 0)
      return -1;

    if (x.token.typ != TOKEN_CLOSE_TAG)
    {
      x.error = -1;
      strcpy (out x.error_msg, "</name> expected");
      return -1;
    }

    if (next_token (ref x, is_content => true) < 0)
      return -1;
  }
  else if (x.token.typ == TOKEN_ERROR)
  {
    return -1;
  }
  else
  {
    x.error = -1;
    strcpy (out x.error_msg, "end-tag expected");
    return -1;
  }

  free x.parse.info^.name;
  free x.parse.info^.value;
  p = x.parse.info;
  x.parse.info = x.parse.info^.next;
  free p;
  x.parse.level--;

  return 0;
}

//-------------------------------------------------------------------------

// obtain value of element and close element

public int get_xml_end_element (ref XML xml, out string value)
{
  LEVEL_INFO^ p;
  int         rc;

  clear value;

  if (xml.magic != MAGIC)
    return -1;

  if (xml.parse.info == null)    // outer level
  {
    xml.error = -1;
    strcpy (out xml.error_msg, "get_xml_end_element() : incorrect call order");
    return -1;
  }

  p = xml.parse.info;   // save current level pointer

  for (;;)
  {
    char name[8];

    // skip attributes, all content and possibly intern open tag
    rc = get_xml_begin_element (ref xml, out name);
    if (rc < 0)
      return -1;
    if (rc == +1)  // no further open tag
    {
      if (p == xml.parse.info)   // same level as before
        break;

      // we're nested in intern tags

      rc = parse_end_tag (ref xml);
      if (rc < 0)
        return -1;
    }

    _unused name;
  }

  if (xml.parse.info^.value^'length > value'length)
  {
    xml.error = -1;
    strcpy (out xml.error_msg, "get_xml_end_element() : tag value is too long");
    return -1;
  }

  sprintf (out value, "%s", xml.parse.info^.value^);

  rc = parse_end_tag (ref xml);
  if (rc < 0)
    return -1;

  return 0;
}

//-------------------------------------------------------------------------

// obtain value of element and close element

public string^ get_xml_end_element_value (ref XML xml)
{
  LEVEL_INFO^ p;
  int         rc;
  string^     ps;
  
  if (xml.magic != MAGIC)
    return null;

  if (xml.parse.info == null)    // outer level
  {
    xml.error = -1;
    strcpy (out xml.error_msg, "get_xml_end_element() : incorrect call order");
    return null;
  }

  p = xml.parse.info;   // save current level pointer

  for (;;)
  {
    char name[8];

    // skip attributes, all content and possibly intern open tag
    rc = get_xml_begin_element (ref xml, out name);
    if (rc < 0)
      return null;
    if (rc == +1)  // no further open tag
    {
      if (p == xml.parse.info)   // same level as before
        break;

      // we're nested in intern tags

      rc = parse_end_tag (ref xml);
      if (rc < 0)
        return null;
    }

    _unused name;
  }

  ps = new string ' (xml.parse.info^.value^);

  rc = parse_end_tag (ref xml);
  if (rc < 0)
  {
    free ps;
    return null;
  }

  return ps;
}

//-------------------------------------------------------------------------

// get message about last xml error

public void get_xml_error (XML xml, out string error_message)
{
  char msg[100];

  if (xml.magic != MAGIC)
  {
    strcpy (out msg, "xml not open");
  }
  else if (xml.error == 0)
  {
    sprintf (out msg, "line %d col %d : ", xml.token.line, xml.token.col);
  }
  else
  {
    sprintf (out msg, "line %d col %d : %s", xml.token.line, xml.token.col, xml.error_msg);
  }

  sprintf (out error_message, "%.*s", error_message'length, msg);
}

//-------------------------------------------------------------------------
