
// smtp.c

use base64, files, strings, system, tcpip, thread, tracing;

struct SMTP
{
  bool       is_open;
  TCP_CLIENT handle;
  int        error;
  bool       tracing;
} 

/**************************************************************************/

enum CMD_TYP {
  UNDEFINED,
  SMTP_CMD,   /* max 510 chars */
  SMTP_TEXT,  /* max 998 chars, duplicate any leading dot */
}; 

/**************************************************************************/

/* 'line' : line without CRLF */

void smtp_put_line (ref SMTP    s,
                        string  line,
                        CMD_TYP mode)  /* SMTP_CMD or SMTP_TEXT */
{
  int  length, size;
  char buffer[1001];

  if (s.error != 0 || !s.is_open)       // error or session not open
    return;

  if (s.tracing)
    trace ("info: smtp send: ('%s')\n", line);

  length = strlen(line);

  if (mode == SMTP_TEXT)
  {
    if (length > 998)
    {
      trace ("error: smtp_put_line() : line is too long\n");
      s.error = -2;
      return;
    }
  }
  else  // SMTP_CMD
  {
    if (length > 510)
    {
      trace ("error: smtp_put_line() : line is too long\n");
      s.error = -2;
      return;
    }
  }

  size = 0;
  clear buffer;
  
  if (mode == SMTP_TEXT && length > 0 && line[0] == '.')
    buffer[size++] = '.';

  buffer[size : length] = line[0 : length];
  size += length;

  buffer[size++] = '\r';
  buffer[size++] = '\n';

  if (CS_write (ref s.handle, buffer[0 : size], 5) < 0)
    s.error = -1;
}

/**************************************************************************/

/* returns 0 if OK, or -1 if tcp/ip error. */
/* if OK, 'code' will be filled with the 3-digit SMTP return code. */

void smtp_get_reply_code (ref SMTP s, out char code[3])
{
  TIMER timer;
  int   rc;
  uint  index, k, i;
  char  buffer[512];

  if (s.error != 0 || !s.is_open)   // error or no session
  {
    code = "999";
    return;
  }

  clear code;
  set_timer (out timer, 180);
  index = 0;
  k = 0;
  clear buffer;
  
  for (;;)
  {
    rc = TCP_receive (ref s.handle, ref buffer, ref index);

    // trace incoming string
    if (s.tracing)
    {
      if (index > k)
      {
        char outb[512*5];
        int  o;
        uint j;
        
        o = 0;
        clear outb;
        for (j=k; j<index; j++)
        {
          if (buffer[j] >= ' ')
            outb[o++] = buffer[j];
          else
          {
            sprintf (out outb[o:5], "<%d>", (int)buffer[j]);
            o += strlen(outb[o:5]);
          }
        }
        trace ("info: smtp recv: ('%s')\n", outb);
      }
      k = index;
    }

    if (index >= 6 && buffer[index-2] == '\r' && buffer[index-1] == '\n')
    {
      i = 0;
      while (i+5 < index)
      {
        if (isdigit(buffer[i]) && isdigit(buffer[i+1]) && isdigit(buffer[i+2]) && buffer[i+3] == ' ')
        {
          code = buffer[i:3];
          return;
        }

        while (i < index && buffer[i] != '\r')
          i++;
        if (i < index && buffer[i] == '\r')
          i++;
        if (i < index && buffer[i] == '\n')
          i++;
      }
    }

    if (index == buffer'size)    /* buffer is full */
    {
      if (s.tracing)
        trace ("error: smtp input buffer full\n");
      break;
    }

    if (rc < 0)                     /* tcp/ip problem */
    {
      if (s.tracing)
        trace ("error: smtp tcp/ip error %d\n", rc);
      break;
    }

    if (timer_elapsed (timer))
    {
      if (s.tracing)
        trace ("error: smtp receive timeout\n");
      break;
    }

    TCP_wait (ref s.handle, 1);
  }

  s.error = -1;
}

/**************************************************************************/

public int smtp_open (out SMTP s, string host, int port)
{
  char code[3];
  int  port2 = port;
  
  if (port2 == 0)
    port2 = 25;

  clear s;

  if (CS_connect (out s.handle, host, port2, 5) < 0)
  {
    clear s;
    return -1;
  }

  s.is_open = true;
  
  smtp_get_reply_code (ref s, out code);
  if (code[0] != '2')
  {
    CS_close (ref s.handle);
    s.is_open = false;
    clear s;
    return -2;
  }

  return 0;
}

/**************************************************************************/

public void smtp_close (ref SMTP s)
{
  char code[3];

  if (s.tracing)
    trace ("info: smtp_close ...\n");

  if (s.error == 0 && s.is_open)     // no error, valid handle
  {
    smtp_put_line (ref s, "QUIT", SMTP_CMD);
    smtp_get_reply_code (ref s, out code);
    _unused code;
  }

  CS_close (ref s.handle);
  clear s;
}

/**************************************************************************/

void smtp_command (ref SMTP s, string cmd, char correct_code)
{
  char code[3];

  smtp_put_line (ref s, cmd, SMTP_CMD);
  smtp_get_reply_code (ref s, out code);

  if (code[0] != correct_code)
    s.error = -2;
}

/**************************************************************************/

public void smtp_begin_mail (ref SMTP s)
{
  char computer[MAX_COMPUTERNAME_LENGTH], line[MAX_COMPUTERNAME_LENGTH + 20];
  get_computer_name (out computer);
  sprintf (out line, "HELO %.64s", computer);
  smtp_command (ref s, line, '2');
}

/**************************************************************************/

public void smtp_from (ref SMTP s, string originator)
{
  char line[256+32];
  sprintf (out line, "MAIL FROM:<%.256s>", originator);
  smtp_command (ref s, line, '2');
}

/**************************************************************************/

public void smtp_to (ref SMTP s, string recipient)
{
  char line[256+32];
  sprintf (out line, "RCPT TO:<%.256s>", recipient);
  smtp_command (ref s, line, '2');
}

/**************************************************************************/

public void smtp_begin_header (ref SMTP s)
{
  smtp_command (ref s, "DATA", '3');
}

/**************************************************************************/

public void smtp_text (ref SMTP s, string line)
{
  smtp_put_line (ref s, line, SMTP_TEXT);
}

/**************************************************************************/

public void smtp_trace (ref SMTP s, bool flag)
{
  s.tracing = flag;
}

/**************************************************************************/

public void smtp_end_header (ref SMTP s)
{
  smtp_text (ref s, "MIME-Version: 1.0");
  smtp_text (ref s, "Content-Type: multipart/mixed; boundary=\"next\"");
  smtp_text (ref s, "");
}

/**************************************************************************/

void smtp_send_base64 (ref SMTP s, byte[] p)
{
  byte[]^ q = new byte[((p'length + 2) / 3 * 4)];
  int     len2, i, chunk, rest;

  encode_block_base64 (p, out q^, out len2);

  i = 0;
  rest = len2;
  while (rest > 0)
  {
    if (rest > 996)  // output blocks can only be cut at offsets multiple of 4
      chunk = 996;
    else
      chunk = rest;

    smtp_put_line (ref s, q^[i:chunk], SMTP_TEXT);

    i += chunk;
    rest -= chunk;
  }

  free q;
}

/**************************************************************************/

void smtp_send_base64_file (ref SMTP s, string filename)
{
  int     fd, size, rc;
  byte[]^ p;

  fd = open (filename);
  if (fd < 0)
  {
    trace ("error: smtp_send_base64_file() : cannot open '%s'\n", filename);
    s.error = -1;
    return;
  }

  size = (int)lseek (fd, 0L, SEEK_END);
  if (size < 0)
  {
    trace ("error: smtp_send_base64_file() : lseek() failed\n");
    close (fd);
    s.error = -1;
    return;
  }

  rc = (int)lseek (fd, 0L, SEEK_SET);
  if (rc < 0)
  {
    trace ("error: smtp_send_base64_file() : lseek() failed\n");
    close (fd);
    s.error = -1;
    return;
  }

  p = new byte[size];

  if (read (fd, out p^) != size)
  {
    trace ("error: smtp_send_base64_file() : read() failed\n");
    close (fd);
    free p;
    s.error = -1;
    return;
  }

  close (fd);

  smtp_send_base64 (ref s, p^);

  free p;
}

/**************************************************************************/

public void smtp_body (ref SMTP s, string mime, string text)
{
  char buffer[1024];

  smtp_text (ref s, "--next");

  sprintf (out buffer, "Content-Type: %.500s", mime);
  smtp_text (ref s, buffer);

  smtp_text (ref s, "Content-Transfer-Encoding: Base64");
  smtp_text (ref s, "");
  smtp_send_base64 (ref s, text[0 : strlen(text)]);
  smtp_text (ref s, "");
}

/**************************************************************************/

public void smtp_attach_file (ref SMTP s, string mime, string name, string filename)
{
  char buffer[1024];

  smtp_text (ref s, "--next");

  sprintf (out buffer, "Content-Type: %.500s", mime);
  smtp_text (ref s, buffer);

  sprintf (out buffer, "Content-Disposition: attachment; filename=\"%.260s\"", name);
  smtp_text (ref s, buffer);

  smtp_text (ref s, "Content-Transfer-Encoding: Base64");
  smtp_text (ref s, "");
  smtp_send_base64_file (ref s, filename);
  smtp_text (ref s, "");
}

/**************************************************************************/

public int smtp_end_mail (ref SMTP s)
{
  if (s.error != 0 || !s.is_open)     // error or no handle
    return -1;

  smtp_text (ref s, "--next--");
  smtp_command (ref s, ".", '2');

  if (s.error != 0)
    return -1;

  return 0;
}

/**************************************************************************/
