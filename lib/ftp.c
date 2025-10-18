
// ftp.c

use calendar, files, tcpip, strings, thread, aes, text, random;

/*************************************************************************/
#begin unsafe
/*************************************************************************/

#define debug 0

/*************************************************************************/

package body FTP_CLIENT

/*************************************************************************/

struct FTP_DATA
{
  bool       used;
  TCP_CLIENT handle;
  IP         local_ip;
  bool       clib_caller;    // true = secure password and filesize check
  bool       is_open_dir;    // true = between ftp_open_dir() and ftp_close_dir(),
  A_TEXT     open_dir_text;  // valid if (is_open_dir)
}

const int MAX_CONNECTIONS = 10;

FTP_DATA ftp[MAX_CONNECTIONS];

const int HANDLE_OFFSET = 0x2356;

const int  DEFAULT_FTP_PORT = 21;
const uint TIMEOUT          = 10;   /* seconds */

/*************************************************************************/

int send_command (ref TCP_CLIENT handle, char[] request)
{
  int   rc;
  uint  length, index;
  TIMER timer;


#if debug
  trace ("REQUEST: %s\n", request);
#endif

  length = (uint)strlen(request);
  index = 0;
  set_timer (out timer, TIMEOUT);

  for (;;)
  {
    rc = TCP_send (ref handle, request[0:length], ref index);

    if (index == length)    /* all is sent */
      break;

    if (rc < 0)        /* network error */
      return rc;

    if (timer_elapsed (timer))
      return FTP_COMMAND_TIMEOUT;

    TCP_wait (ref handle, 1);
  }

  return 0;
}

/*************************************************************************/

int ext_get_response (ref TCP_CLIENT handle, out char reply[1024])
{
  int   rc;
  uint  length, len0, index;
  TIMER timer;

  clear reply;

  length = (uint)reply'length;
  index = 0;
  set_timer (out timer, TIMEOUT);

  for (;;)
  {
    len0 = index + 1;   /* accept only 1 char at a time */

    rc = TCP_receive (ref handle, ref reply[0:len0], ref index);

    /* check if reply is complete */
    if (index >= 5 && reply[index-1] == (char)0x0A)
    {
      if (reply[3] == ' ')           /* short reply */
        break;
      else if (reply[3] == '-')      /* long reply */
      {
        char fragment[8];

        clear fragment;
        fragment[0] = (char)0x0A;
        fragment[1:3] = reply[0:3];
        fragment[4] = ' ';

        if (strstr (reply, fragment) != -1)
          break;
      }
    }

    if (index == length)    /* buffer is full */
      return FTP_REPLY_BUFFER_FULL;

    if (rc < 0)        /* network error */
      return rc;

    if (timer_elapsed (timer))
      return FTP_REPLY_TIMEOUT;

    TCP_wait (ref handle, 1);
  }

#if debug
  trace ("REPLY: %s\n", reply);
#endif

  return 0;
}

/*************************************************************************/

int get_response (ref TCP_CLIENT handle, out char code[3])
{
  int  rc;
  char reply[1024];

  clear code;

  rc = ext_get_response (ref handle, out reply);
  if (rc < 0)
    return rc;

  code = reply[0:3];

  return 0;
}

/*************************************************************************/

int command (ref TCP_CLIENT handle,
                 string     request,
             out char       code[3])
{
  int rc;

  clear code;

  rc = send_command (ref handle, request);
  if (rc < 0)
    return rc;

  return get_response (ref handle, out code);
}

/*************************************************************************/

public int ftp_open (string host, int port, string user, string pasw)
{
  const byte key[16] = {0xFA, 0x86, 0x96, 0x74, 0x52, 0x92, 0x10, 0x37,
                        0x9A, 0x94, 0x11, 0xBD, 0xF8, 0xA2, 0x1C, 0x62};
  int           i, rc;
  FTP_DATA*     p;
  PEER          info;
  char          cmd[80];
  char          code[3];
  char          reply[1024];
  int           pstr;
  byte          stamp[16];
  char          buffer[1+32+1];
  int           j, k;
  bool          is_secure;

  /* select free entry */
  for (i=0; i<MAX_CONNECTIONS; i++)
  {
    if (!ftp[i].used)
      break;
  }
  if (i == MAX_CONNECTIONS)
    return FTP_TOO_MANY_CONNECTIONS;

  p = &ftp[i];

  rc = CS_connect (out p->handle, host, port == 0 ? DEFAULT_FTP_PORT : port, TIMEOUT);
  if (rc < 0)
    return FTP_CONNECTION_FAILED;

  rc = TCP_query_local_address (p->handle, out info);
  if (rc < 0)
  {
    TCP_hangup (ref p->handle);
    return FTP_CANNOT_QUERY_LOCAL_ADDRESS;
  }
  p->local_ip = info.ip;

  (void)TCP_no_delay (p->handle);


  /* GET SERVICE STATUS CODE */

  rc = ext_get_response (ref p->handle, out reply);
  if (rc < 0)
  {
    TCP_hangup (ref p->handle);
    return rc;
  }

  if (reply[0] != '2')
  {
    TCP_hangup (ref p->handle);
    return FTP_SERVICE_NOT_READY;
  }


  /* detect if the service is secure */

  p->clib_caller = false;
  is_secure = false;
  clear stamp;
  pstr = strchr (reply, '[');
  if (pstr != -1 && strlen(reply)-pstr >= 34 && reply[pstr+33] == ']')
  {
    is_secure = true;
    p->clib_caller = true;
    for (j=0; j<16; j++)
    {
      sscanf (reply[pstr+1+j*2:2], "%x", out k);
      stamp[j] = (byte)k;
    }
  }


  /* USER */

  sprintf (out cmd, "user %.60s\r\n", user);

  rc = command (ref p->handle, cmd, out code);
  if (rc < 0)
  {
    TCP_hangup (ref p->handle);
    return rc;
  }

  if (code[0] < '2' || code[0] > '3')
  {
    TCP_hangup (ref p->handle);
    return FTP_BAD_USER;
  }

  if (code[0] == '3')   /* password required */
  {
    /* PASSWORD */
    if (is_secure && strlen(pasw) <= 16)
    {
      for (j=0; j<strlen(pasw); j++)
        stamp[j] ^= (byte)pasw[j];

      /* encrypt with key */
      aes_encrypt (out stamp, stamp, key);

      /* expand into buffer */
      strcpy (out buffer, "$");
      for (j=0; j<16; j++)
        strcatf (ref buffer, "%02x", stamp[j]);

      sprintf (out cmd, "pass %.60s\r\n", buffer);
    }
    else
    {
      sprintf (out cmd, "pass %.60s\r\n", pasw);
    }

    rc = command (ref p->handle, cmd, out code);
    if (rc < 0)
    {
      TCP_hangup (ref p->handle);
      return rc;
    }

    if (code[0] != '2')
    {
      TCP_hangup (ref p->handle);
      return FTP_BAD_PASSWORD;
    }
  }

  p->used = true;

  return HANDLE_OFFSET + i;
}

/*************************************************************************/

package Q
  enum DIRECTION { FTP_PUT,
                   FTP_GET,
                   FTP_GET_SIZE };  /* like FTP_GET, but does not create a local file */
end Q;

/*************************************************************************/

int ftp_transfer (    int        ftp_handle,
                      string     local_filename,
                      string     remote_filename,
                      int        file_type,
                      DIRECTION  direction,  /* see constants above */
                  out uint       bytes_transferred)
{
  FTP_DATA*  p;
  char       cmd[512], code[3];
  int        rc, fd;
  int        port, tries;
  TIMER      timer;
  TCP_SERVER tcp_server;
  TCP_CLIENT tcp_client;


  bytes_transferred = 0;


  /* check ftp handle */

  if (ftp_handle < HANDLE_OFFSET ||
      ftp_handle >= HANDLE_OFFSET + MAX_CONNECTIONS)
    return FTP_BAD_FTP_HANDLE;

  p = &ftp[ftp_handle - HANDLE_OFFSET];

  if (!p->used)
    return FTP_BAD_FTP_HANDLE;


  /* check file type */

  if (file_type != TYPE_BINARY &&
      file_type != TYPE_ASCII  &&
      file_type != TYPE_EBCDIC)
    return FTP_BAD_FILE_TYPE;


  /* set TYPE */

  if (file_type == TYPE_BINARY)
    strcpy (out cmd, "type I\r\n");
  else if (file_type == TYPE_ASCII)
    strcpy (out cmd, "type A\r\n");
  else
    strcpy (out cmd, "type E\r\n");

  rc = command (ref p->handle, cmd, out code);
  if (rc < 0)
    return rc;

  if (code[0] != '2')
    return FTP_TYPE_SERVER_ERROR;


  /* open a data server */

  clear tcp_server;
  port = 0;
  for (tries=0; tries<1000; tries++)
  {
    port = rnd (5000, 65535);
    rc = TCP_create_server (out tcp_server,
                                (uint2)port,
                                is_ipv4(p->local_ip) ? MODE_IPV4 : MODE_IPV6);
    if (rc != TCP_PORT_IN_USE)
      break;
  }
  if (rc < 0)
    return FTP_CANNOT_CREATE_DATA_SERVER;


  /* set PORT */

  if (is_ipv4 (p->local_ip))
  {
    sprintf (out cmd, "port %u,%u,%u,%u,%d,%d\r\n",
             p->local_ip[12], p->local_ip[13], p->local_ip[14], p->local_ip[15], port >> 8, port & 255);

  }
  else
  {
    char can[48];
    ip_to_numericstr (p->local_ip, out can);
    sprintf (out cmd, "EPRT |2|%s|%d|\r\n", can, port);
  }

  rc = command (ref p->handle, cmd, out code);
  if (rc < 0)
  {
    TCP_close_server (ref tcp_server);
    return rc;
  }

  if (code[0] != '2')
  {
    TCP_close_server (ref tcp_server);
    return FTP_PORT_SERVER_ERROR;
  }


  /* RETR/STORE FILE */

  if (direction == FTP_PUT)
    sprintf (out cmd, "stor %.256s\r\n", remote_filename);
  else  /* FTP_GET or FTP_GET_SIZE */
    sprintf (out cmd, "retr %.256s\r\n", remote_filename);

  rc = command (ref p->handle, cmd, out code);
  if (rc < 0)
  {
    TCP_close_server (ref tcp_server);
    return rc;
  }

  if (code[0] != '1')
  {
    TCP_close_server (ref tcp_server);
    return FTP_BAD_REMOTE_FILENAME;
  }

  /* wait for incoming connection */

  clear tcp_client;
  set_timer (out timer, TIMEOUT);
  for (;;)
  {
    rc = TCP_incoming_call (tcp_server, out tcp_client);
    if (rc != TCP_WAITING)
      break;

    if (timer_elapsed (timer))
      break;

    TCP_wait_server (ref tcp_server, 1);
  }

  TCP_close_server (ref tcp_server);

  if (rc < 0)
    return FTP_SERVER_NO_DATA_CONNECTION;


  /* send/receive the file */

  if (direction == FTP_PUT)
  {
    fd = open (local_filename);
    if (fd < 0)
    {
      TCP_hangup (ref tcp_client);
      return FTP_BAD_LOCAL_FILENAME;
    }

    for (;;)
    {
      byte buffer[8192];
      uint size, index;

      rc = read (fd, out buffer);
      if (rc < 0)
      {
        close (fd);
        TCP_hangup (ref tcp_client);
        return FTP_FILE_READ_ERROR;
      }

      if (rc == 0)
        break;

      bytes_transferred += (uint)rc;

      size = (uint)rc;
      index = 0;
      set_timer (out timer, TIMEOUT);

      for (;;)
      {
        rc = TCP_send (ref tcp_client, buffer[0:size], ref index);

        if (index == size)    /* all is sent */
          break;

        if (rc < 0)        /* network error */
        {
          close (fd);
          TCP_hangup (ref tcp_client);
          return FTP_CONNECTION_BROKEN;
        }

        if (timer_elapsed (timer))
        {
          close (fd);
          TCP_hangup (ref tcp_client);
          return FTP_DATA_TIMEOUT;
        }

        TCP_wait (ref tcp_client, 1);
      }
    }

    close (fd);
    TCP_hangup (ref tcp_client);
  }
  else   /* direction == FTP_GET or FTP_GET_SIZE */
  {
    if (direction == FTP_GET)
    {
      fd = create (local_filename);
      if (fd < 0)
      {
        TCP_hangup (ref tcp_client);
        return FTP_BAD_LOCAL_FILENAME;
      }
    }
    else
    {
      fd = 0;
    }

    for (;;)
    {
      byte buffer[8192];
      uint size, index;

      size = buffer'size;
      index = 0;
      set_timer (out timer, TIMEOUT);
      clear buffer;

      for (;;)
      {
        rc = TCP_receive (ref tcp_client, ref buffer, ref index);

        if (index == size)    /* buffer is full */
          break;

        if (rc < 0)        /* network error */
        {
          if (rc == TCP_CONNECTION_BROKEN)
            break;

          if (direction == FTP_GET)
          {
            close (fd);
            delete_file (local_filename);
          }
          TCP_hangup (ref tcp_client);
          return FTP_CONNECTION_ERROR;
        }

        if (timer_elapsed (timer))
        {
          if (direction == FTP_GET)
          {
            close (fd);
            delete_file (local_filename);
          }
          TCP_hangup (ref tcp_client);
          return FTP_DATA_TIMEOUT;
        }

        TCP_wait (ref tcp_client, 1);
      }

      if (index == 0)   /* no further data */
        break;

      bytes_transferred += index;

      if (direction == FTP_GET)
      {
        rc = write (fd, buffer[0:index]);
        if (rc != (int)index)
        {
          close (fd);
          delete_file (local_filename);
          TCP_hangup (ref tcp_client);
          return FTP_FILE_WRITE_ERROR;
        }
      }
    }

    if (direction == FTP_GET)
      close (fd);
    TCP_hangup (ref tcp_client);
  }

  rc = get_response (ref p->handle, out code);
  if (rc < 0)
    return rc;

  if (code[0] != '2')
    return FTP_TRANSFER_FAILED;

  return 0;
}

/*************************************************************************/

/* retrieve size of last file transfer operation */

int ftp_get_size (int ftp_handle, out uint size)
{
  FTP_DATA* p;
  char      cmd[512], response[1024];
  char      resp1[3], resp2[4];
  int       rc;

  size = 0;

  if (ftp_handle < HANDLE_OFFSET ||
      ftp_handle >= HANDLE_OFFSET + MAX_CONNECTIONS)
    return FTP_BAD_FTP_HANDLE;

  p = &ftp[ftp_handle - HANDLE_OFFSET];

  if (!p->used)
    return FTP_BAD_FTP_HANDLE;


  sprintf (out cmd, "$siz\r\n");

  rc = send_command (ref p->handle, cmd);
  if (rc < 0)
    return rc;

  rc = ext_get_response (ref p->handle, out response);
  if (rc < 0)
    return rc;

  if (response[0] != '2')
    return FTP_INTERN_ERROR;

  if (sscanf (response, "%s %s %u", out resp1, out resp2, out size) < 0 ||
      stricmp (resp2, "size") != 0)
    return FTP_INTERN_ERROR;

  _unused resp1;

  return 0;
}

/*************************************************************************/

/* attention: any existing file is overwritten */

public int ftp_put (int    ftp_handle,
                    string local_filename,
                    string remote_filename,
                    int    file_type = TYPE_BINARY)    /* see constants above */
{
  int       rc;
  uint      size1, size2;
  FTP_DATA* p;

  rc = ftp_transfer (ftp_handle, local_filename, remote_filename,
                     file_type, FTP_PUT, out size1);
  if (rc < 0)
    return rc;

  p = &ftp[ftp_handle - HANDLE_OFFSET];
  if (p->clib_caller)
  {
    rc = ftp_get_size (ftp_handle, out size2);
    if (rc < 0)
      return rc;
  }
  else
  {
    rc = ftp_transfer (ftp_handle, local_filename, remote_filename,
                       file_type, FTP_GET_SIZE, out size2);
    if (rc < 0)
      return rc;
  }

  if (size1 != size2)
    return FTP_TRANSFER_FAILED;

  return 0;
}

/*************************************************************************/

/* attention: any existing file is overwritten */

public int ftp_get (int    ftp_handle,
                    string remote_filename,
                    string local_filename,
                    int    file_type = TYPE_BINARY)    /* see constants above */
{
  int       rc;
  uint      size1, size2;
  FTP_DATA* p;

  rc = ftp_transfer (ftp_handle, local_filename, remote_filename,
                     file_type, FTP_GET, out size1);
  if (rc < 0)
    return rc;

  p = &ftp[ftp_handle - HANDLE_OFFSET];
  if (p->clib_caller)
  {
    rc = ftp_get_size (ftp_handle, out size2);
    if (rc < 0)
      return rc;
  }
  else
  {
    rc = ftp_transfer (ftp_handle, local_filename, remote_filename,
                       file_type, FTP_GET_SIZE, out size2);
    if (rc < 0)
      return rc;
  }

  if (size1 != size2)
    return FTP_TRANSFER_FAILED;

  return 0;
}

/*************************************************************************/

/* attention: the new_filename is deleted if already existing */

public int ftp_rename (int    ftp_handle,
                       string remote_filename,
                       string new_remote_filename)
{
  FTP_DATA* p;
  char      cmd[512], code[3];
  int       rc;

  if (ftp_handle < HANDLE_OFFSET ||
      ftp_handle >= HANDLE_OFFSET + MAX_CONNECTIONS)
    return FTP_BAD_FTP_HANDLE;

  p = &ftp[ftp_handle - HANDLE_OFFSET];

  if (!p->used)
    return FTP_BAD_FTP_HANDLE;


  sprintf (out cmd, "rnfr %.256s\r\n", remote_filename);
  rc = command (ref p->handle, cmd, out code);
  if (rc < 0)
    return rc;

  if (code[0] != '3')
    return FTP_BAD_FILENAME;


  sprintf (out cmd, "rnto %.256s\r\n", new_remote_filename);
  rc = command (ref p->handle, cmd, out code);
  if (rc < 0)
    return rc;

  if (code[0] != '2')
    return FTP_BAD_NEW_FILENAME;

  return 0;
}

/*************************************************************************/

public int ftp_delete (int ftp_handle, string remote_filename)
{
  FTP_DATA* p;
  char      cmd[512], code[3];
  int       rc;

  if (ftp_handle < HANDLE_OFFSET ||
      ftp_handle >= HANDLE_OFFSET + MAX_CONNECTIONS)
    return FTP_BAD_FTP_HANDLE;

  p = &ftp[ftp_handle - HANDLE_OFFSET];

  if (!p->used)
    return FTP_BAD_FTP_HANDLE;


  sprintf (out cmd, "dele %.256s\r\n", remote_filename);
  rc = command (ref p->handle, cmd, out code);
  if (rc < 0)
    return rc;

  if (code[0] != '2')
    return FTP_BAD_FILENAME;

  return 0;
}

/*************************************************************************/

int _ftp_dir (int ftp_handle, string keyword, string dir)
{
  FTP_DATA* p;
  char      cmd[512], code[3];
  int       rc;

  if (ftp_handle < HANDLE_OFFSET ||
      ftp_handle >= HANDLE_OFFSET + MAX_CONNECTIONS)
    return FTP_BAD_FTP_HANDLE;

  p = &ftp[ftp_handle - HANDLE_OFFSET];

  if (!p->used)
    return FTP_BAD_FTP_HANDLE;


  sprintf (out cmd, "%s %.256s\r\n", keyword, dir);
  rc = command (ref p->handle, cmd, out code);
  if (rc < 0)
    return rc;

  if (code[0] != '2')
    return FTP_BAD_PATH;

  return 0;
}

/*************************************************************************/

public int ftp_set_remote_dir (int ftp_handle, string remote_dir)
{
  return _ftp_dir (ftp_handle, "cwd", remote_dir);
}

/*************************************************************************/

public int ftp_make_remote_dir (int ftp_handle, string remote_dir)
{
  return _ftp_dir (ftp_handle, "mkd", remote_dir);
}

/*************************************************************************/

public int ftp_remove_remote_dir (int ftp_handle, string remote_dir)
{
  return _ftp_dir (ftp_handle, "rmd", remote_dir);
}

/*************************************************************************/

package DATA

  struct FTP_SCAN_DIR_INFO
  {
    byte       buffer[8192];
    uint       index;
    uint       get_index;
    bool       eof;
  }

end DATA;

/*************************************************************************/

/* returns -1 if EOF */

int ftp_open_dir_get_char (ref TCP_CLIENT h, ref FTP_SCAN_DIR_INFO p)
{
  TIMER timer;
  int   rc;

  if (p.get_index < p.index)         /* chars left in buffer */
    return p.buffer[p.get_index++];

  if (p.eof)     /* no more tcp/ip chunks to receive */
    return -1;

  p.get_index = 0;
  p.index = 0;

  set_timer (out timer, TIMEOUT);

  for (;;)
  {
    rc = TCP_receive (ref h, ref p.buffer, ref p.index);

    if (p.index > 0)      /* something was received */
      break;

    if (rc < 0 || timer_elapsed (timer))    /* network error or timeout */
    {
      p.eof = true;
      break;
    }

    TCP_wait (ref h, 1);
  }

  if (p.get_index < p.index)
    return p.buffer[p.get_index++];

  return -1;
}

/*************************************************************************/

/* returns 0 if OK, or a negative error code. */

public int ftp_open_dir (int ftp_handle, string remote_dir)
{
  FTP_DATA*  p;
  char       cmd[512], code[3];
  int        rc, tries;
  int        port;
  TIMER      timer;
  TCP_SERVER tcp_server;
  TCP_CLIENT tcp_client;

  if (ftp_handle < HANDLE_OFFSET ||
      ftp_handle >= HANDLE_OFFSET + MAX_CONNECTIONS)
    return FTP_BAD_FTP_HANDLE;

  p = &ftp[ftp_handle - HANDLE_OFFSET];

  if (!p->used)
    return FTP_BAD_FTP_HANDLE;

  if (p->is_open_dir)
    return FTP_OPEN_DIR_WAS_NOT_CLOSED;


  /* open a data server */

  clear tcp_server;
  port = 0;
  rc = -1;
  for (tries=0; tries<1000; tries++)
  {
    port = rnd (5000, 65535);
    rc = TCP_create_server (out tcp_server,
                                (uint2)port,
                                is_ipv4(p->local_ip) ? MODE_IPV4 : MODE_IPV6);
    if (rc != TCP_PORT_IN_USE)
      break;
  }
  if (rc < 0)
    return FTP_CANNOT_CREATE_DATA_SERVER;


  /* set PORT */

  if (is_ipv4 (p->local_ip))
  {
    sprintf (out cmd, "port %u,%u,%u,%u,%d,%d\r\n",
             p->local_ip[12], p->local_ip[13], p->local_ip[14], p->local_ip[15], port >> 8, port & 255);

  }
  else
  {
    char can[48];
    ip_to_numericstr (p->local_ip, out can);
    sprintf (out cmd, "EPRT |2|%s|%d|\r\n", can, port);
  }

  rc = command (ref p->handle, cmd, out code);
  if (rc < 0)
  {
    TCP_close_server (ref tcp_server);
    return rc;
  }

  if (code[0] != '2')
  {
    TCP_close_server (ref tcp_server);
    return FTP_PORT_SERVER_ERROR;
  }


  /* send command to scan directory */

  sprintf (out cmd, "nlst %.256s\r\n", remote_dir);
  rc = command (ref p->handle, cmd, out code);
  if (rc < 0)
    return rc;

  if (code[0] != '1')
  {
    TCP_close_server (ref tcp_server);
    return FTP_BAD_REMOTE_FILENAME;
  }


  /* wait for incoming connection */

  clear tcp_client;
  set_timer (out timer, TIMEOUT);
  for (;;)
  {
    rc = TCP_incoming_call (tcp_server, out tcp_client);
    if (rc != TCP_WAITING)
      break;

    if (timer_elapsed (timer))
      break;

    TCP_wait_server (ref tcp_server, 1);
  }

  TCP_close_server (ref tcp_server);

  if (rc < 0)
  {
    (void)get_response (ref p->handle, out code);
    return FTP_SERVER_NO_DATA_CONNECTION;
  }


  /* receive the filename list */

  creat_text (out p->open_dir_text);

  {
    FTP_SCAN_DIR_INFO info;
    char              filename[FTP_MAX_FILENAME_LENGTH];
    int               ch;
    int               length;
    int               count;

    clear info;
    count = 0;

    for (;;)
    {
      /* retrieve a filename */

      clear filename;
      length = 0;
      for (;;)
      {
        ch = ftp_open_dir_get_char (ref tcp_client, ref info);

        if (ch >= 32)
        {
          if (length < filename'length)
            filename[length++] = (char)ch;
        }
        else  /* CR, LF or EOF */
        {
          break;
        }
      }

      if (length > 0)
      {
        if (strcmp (filename, ".") != 0 && strcmp (filename, "..") != 0)
        {
          insert_text_line (ref p->open_dir_text,
                                count + 1,
                                filename[0:strlen(filename)]);
          count++;
        }
      }

      if (ch < 0)     /* EOF */
        break;
    }
  }

  TCP_hangup (ref tcp_client);

  rc = get_response (ref p->handle, out code);
  if (rc < 0)
  {
    close_text (ref p->open_dir_text);
    return rc;
  }

  if (code[0] != '2')
  {
    close_text (ref p->open_dir_text);
    return FTP_TRANSFER_FAILED;
  }

  p->is_open_dir = true;

  return 0;
}

/*************************************************************************/

/* retrieve next filename from the directory */
/* returns 0 if OK, -1 if end-of-list.       */

public int ftp_read_dir (int ftp_handle, out char filename[FTP_MAX_FILENAME_LENGTH])
{
  FTP_DATA* p;
  int       size;

  clear filename;

  if (ftp_handle < HANDLE_OFFSET ||
      ftp_handle >= HANDLE_OFFSET + MAX_CONNECTIONS)
    return FTP_BAD_FTP_HANDLE;

  p = &ftp[ftp_handle - HANDLE_OFFSET];

  if (!p->used)
    return FTP_BAD_FTP_HANDLE;

  if (!p->is_open_dir)
    return FTP_NO_OPEN_DIR;


  /* retrieve first line of text, then delete it */

  if (nb_text_lines (p->open_dir_text) == 0)   /* no more lines */
    return -1;

  retrieve_text_line (ref p->open_dir_text,
                          1,
                      out filename,
                      out size);
  _unused size;

  delete_text_line (ref p->open_dir_text, 1);

  return 0;
}

/*************************************************************************/

public int ftp_close_dir (int ftp_handle)
{
  FTP_DATA* p;

  if (ftp_handle < HANDLE_OFFSET ||
      ftp_handle >= HANDLE_OFFSET + MAX_CONNECTIONS)
    return FTP_BAD_FTP_HANDLE;

  p = &ftp[ftp_handle - HANDLE_OFFSET];

  if (!p->used)
    return FTP_BAD_FTP_HANDLE;

  if (!p->is_open_dir)
    return FTP_NO_OPEN_DIR;

  close_text (ref p->open_dir_text);

  p->is_open_dir = false;

  return 0;
}

/*************************************************************************/

public int ftp_close (int ftp_handle)
{
  FTP_DATA* p;

  if (ftp_handle < HANDLE_OFFSET ||
      ftp_handle >= HANDLE_OFFSET + MAX_CONNECTIONS)
    return FTP_BAD_FTP_HANDLE;

  p = &ftp[ftp_handle - HANDLE_OFFSET];

  if (!p->used)
    return FTP_BAD_FTP_HANDLE;

  if (p->is_open_dir)
    close_text (ref p->open_dir_text);

  TCP_hangup (ref p->handle);
  p->used = false;

  return 0;
}

/*************************************************************************/

end FTP_CLIENT;

/*************************************************************************/

package body FTP_SERVER

/*************************************************************************/

const uint INACTIVITY_TIMEOUT = (15*60);  /* in seconds (15 minutes) */
const uint CONNECT_TIMEOUT    =       5;  /* in seconds */
const uint RECV_TIMEOUT       =      10;  /* in seconds */
const uint SEND_TIMEOUT       =      10;  /* in seconds */
const uint POLLING_TIMEOUT    =       1;  /* polling timeout for service stop */

/*************************************************************************/

struct FUNCTIONS
{
  FTP_PROFILE        ftp_profile;
  FTP_CHECK_PASSWORD ftp_check_password;
}

/*************************************************************************/

enum STATES { _START, _USERID_GIVEN, _LOGGED_IN };

struct STATE
{
  STATES     state;
  byte[16]   stamp;
  char[512]  userid;
  char[48]   client_ip;
  int        client_port;
  char[512]  home_dir;
  char[512]  access_dir;
  char[512]  current_dir;
  int        ftp_access_mode;
  char[512]  rename_from;
  char       type;             /* 'A' (ascii) or 'I' (image) */
  long       size;             /* size of last file transfer */
  bool       passive_mode;     /* true = passive mode, 0 = active mode */
  TCP_SERVER passive_server_h; /* tcp/ip server handle (if passive mode) */
}

/*************************************************************************/

/* counts bad passwords; service becomes unavailable    */
/* after MAX_BAD_ATTEMPTS bad attempts on the same day. */

DATE_TIME bad_password_date;
int       bad_password_count;

const int MAX_BAD_ATTEMPTS = 25;

/*************************************************************************/

void close_passive_mode (ref STATE state)
{
  if (state.passive_mode)
  {
    state.passive_mode = false;
    TCP_close_server (ref state.passive_server_h);
  }
}

/*************************************************************************/

int send_reply (ref HANDLER_DATA p, char[] reply)
{
  byte buf[512];

  strcpy (out buf, reply);
  strcat (ref buf, "\r\n");

#if debug
  trace ("%s\n", reply);
#endif

  if (write_tcpip_block (ref p, buf[0:strlen(buf)], SEND_TIMEOUT) < 0)
    return -1;   // close connection

  return 0;
}

/*************************************************************************/

int treat_cmd_user (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  FUNCTIONS* f;

  f'byte = p.user_data[0:f'size];   // retrieve functions pointer

  if (f->ftp_profile (    parameter,
                      out state.home_dir,
                      out state.access_dir,
                      out state.ftp_access_mode) < 0)
  {
    state.state = _START;
    close_passive_mode (ref state);
    return send_reply (ref p, "530 not logged in");
  }

  normalize_pathname (ref state.home_dir);
  normalize_pathname (ref state.access_dir);

  strcpy (out state.current_dir, state.home_dir);

  state.state = _USERID_GIVEN;
  strcpy (out state.userid, parameter);

  strcpy (out state.rename_from, "");

  state.type = 'A';   /* ASCII */

  return send_reply (ref p, "331 user needs password");
}

/*************************************************************************/

int treat_cmd_pass (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  const byte key[16] =
    {0xFA, 0x86, 0x96, 0x74, 0x52, 0x92, 0x10, 0x37,
     0x9A, 0x94, 0x11, 0xBD, 0xF8, 0xA2, 0x1C, 0x62};

  DATE_TIME  now;
  int        i, j, rc;
  byte       buffer[16];
  FUNCTIONS* f;

  f'byte = p.user_data[0:f'size];   // retrieve functions pointer

  /* reset bad password counter after 1 hour */
  get_datetime (out now);
  if (now.day   != bad_password_date.day ||
      now.month != bad_password_date.month ||
      now.hour  != bad_password_date.hour)
  {
    bad_password_date = now;
    bad_password_count = 0;
  }

  if (bad_password_count >= MAX_BAD_ATTEMPTS)
    return send_reply (ref p, "421 service unavailable, too many bad password attempts");

  if (state.state < _USERID_GIVEN)
    return send_reply (ref p, "503 bad sequence of commands");


  if (strlen(parameter) == 32+1 &&   /* probably an encrypted password */
      parameter[0] == '$')
  {
    /* decrypt the password */
    clear buffer;
    for (i=0; i<16; i++)
    {
      sscanf (parameter[1+i*2:2], "%2x", out j);
      buffer[i] = (byte)j;
    }

    /* decrypt with key */
    aes_decrypt (out buffer, buffer, key);

    /* XOR with stamp */
    for (i=0; i<16; i++)
      buffer[i] ^= state.stamp[i];

    rc = f->ftp_check_password (state.userid, buffer);
  }
  else
  {
    rc = f->ftp_check_password (state.userid, parameter);
  }

  if (rc < 0)  /* wrong password */
  {
    state.state = _START;
    close_passive_mode (ref state);
    sleep 5;               /* wait 5 seconds before retrying */
    bad_password_count++;
    return send_reply (ref p, "530 user not logged in");
  }

  state.state = _LOGGED_IN;
  return send_reply (ref p, "230 user logged in, proceed");
}

/*************************************************************************/

// client sends IP and PORT that server must connect to for data transfer

int treat_cmd_port (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  byte i1, i2, i3, i4, p1, p2;
  char c1, c2, c3, c4, c5;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  /* parse parameter */
  if (sscanf (parameter, "%u%c%u%c%u%c%u%c%u%c%u",
      out i1, out c1, out i2, out c2, out i3, out c3, out i4, out c4,
      out p1, out c5, out p2) != 0 ||
      c1!=',' || c2!=',' || c3!=',' || c4!=',' || c5!=',')
  {
    return send_reply (ref p, "501 syntax error in parameters");
  }

  sprintf (out state.client_ip, "%u.%u.%u.%u", i1, i2, i3, i4);
  state.client_port = (int)(p1 * 256 + p2);

#if debug
  trace ("ip=%s, port=%d\n", state.client_ip, state.client_port);
#endif

  return send_reply (ref p, "200 ok");
}

/*************************************************************************/

// client sends IP and PORT that server must connect to for data transfer

// EPRT |1|132.235.1.2|6275|
// EPRT |2|1080::8:800:200C:417A|5282|

int treat_cmd_extended_port (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  int   i, start;
  char  delim, c1;
  IP[]^ ip;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  /* parse parameter */
  if (parameter'length < 12)
    return send_reply (ref p, "501 syntax error in parameters");

  if (sscanf (parameter[0:3], "%c%d%c", out delim, out i, out c1) != 0 ||
      delim != c1 || i < 1 || i > 2)
    return send_reply (ref p, "501 syntax error in parameters");

  i = 3;
  start = i;
  while (i < parameter'length && parameter[i] != delim)
    i++;

  dns_to_ip (parameter[start:i-start], out ip);

  if (ip^'length == 0)    // illegal format
  {
    free ip;
    return send_reply (ref p, "501 syntax error in parameters");
  }

  ip_to_numericstr (ip^[0], out state.client_ip);
  free ip;

  if (i == parameter'length || parameter[i] != delim)
    return send_reply (ref p, "501 syntax error in parameters");

  i++;   // skip delimiter

  start = i;
  while (i < parameter'length && parameter[i] != delim)
    i++;

  if (sscanf (parameter[start:i-start], "%d", out state.client_port) != 0)
    return send_reply (ref p, "501 syntax error in parameters");

#if debug
  trace ("ip=%s, port=%d\n", state.client_ip, state.client_port);
#endif

  return send_reply (ref p, "200 ok");
}

/*************************************************************************/

// client requests server port it can connect to for data transfer

int treat_cmd_pasv (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  PEER           info;
  int            port;
  int            rc, tries;
  int            i1, i2, i3, i4;
  char           buffer[80];

  _unused parameter;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  close_passive_mode (ref state);

  rc = TCP_query_local_address (p.tcp_client, out info);
  if (rc < 0)
  {
#if debug
  trace ("error: TCP_query_local_address() returned %d\n", rc);
#endif
    return send_reply (ref p, "555 local processing error");
  }

  port = 0;
  for (tries=0; tries<1000; tries++)
  {
    port = rnd (5000, 65535);
    rc = TCP_create_server (out state.passive_server_h,
                                (uint2)port,
                                MODE_IPV4);
    if (rc != TCP_PORT_IN_USE)
      break;
  }
  if (rc < 0)
  {
#if debug
  trace ("error: TCP_create_server() returned %d\n", rc);
#endif
    return send_reply (ref p, "554 cannot create socket");
  }

  state.passive_mode = true;

  sscanf (info.ip, "%d%*c%d%*c%d%*c%d", out i1, out i2, out i3, out i4);
  sprintf (out buffer,
           "227 entering passive mode. %d,%d,%d,%d,%d,%d",
           i1, i2, i3, i4, port >> 8, port & 255);

#if debug
  trace ("reply: '%s'\n", buffer);
#endif

  return send_reply (ref p, buffer);
}

/*************************************************************************/

// client requests server port it can connect to for data transfer

// EPSV
// Entering Extended Passive Mode (|||6446|)

int treat_cmd_extended_pasv (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  int  port;
  int  rc, tries;
  char buffer[80];

  _unused parameter;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  close_passive_mode (ref state);

  port = 0;
  rc = 0;
  for (tries=0; tries<1000; tries++)
  {
    port = rnd (5000, 65535);
    rc = TCP_create_server (out state.passive_server_h,
                                (uint2)port,
                                MODE_DUAL);
    if (rc != TCP_PORT_IN_USE)
      break;
  }
  if (rc < 0)
  {
#if debug
  trace ("error: TCP_create_server() returned %d\n", rc);
#endif
    return send_reply (ref p, "554 cannot create socket");
  }

  state.passive_mode = true;

  sprintf (out buffer, "227 entering extended passive mode (|||%d|)", port);

#if debug
  trace ("reply: '%s'\n", buffer);
#endif

  return send_reply (ref p, buffer);
}

/*************************************************************************/

int treat_cmd_type (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  if (parameter'length > 0 && toupper(parameter[0]) == 'A')
  {
    state.type = 'A';
    return send_reply (ref p, "200 type is ascii (ok)");
  }
  else if (parameter'length > 0 && toupper(parameter[0]) == 'I') /* image */
  {
    state.type = 'I';
    return send_reply (ref p, "200 type is binary (ok)");
  }
  else
  {
    return send_reply (ref p, "501 unsupported type");
  }
}

/*************************************************************************/

int treat_cmd_pwd (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  char msg[1024];

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  if (strlen(parameter) > 0)
    return send_reply (ref p, "501 syntax error (no parameter expected)");

  sprintf (out msg, "257 \"%s\" is current directory", state.current_dir);
  return send_reply (ref p, msg);
}

/*************************************************************************/

int CMP_FILENAME (string a, string b, int len)
{
#if ANDROID
    return strncmp (a, b, len);
#elif WINDOWS
    return strnicmp (a, b, len);
#else
    error undefined os
#endif
}

/*************************************************************************/

int treat_cmd_cwd (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  char path[512], path2[512];
  char msg[1024];
  int  rc;
  FILE_INFO info;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

  strcpy (out path2, path);
//  strcat (ref path2, "/*");

  if (CMP_FILENAME (path2, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  rc = open_directory (out info, path2);
  if (rc != 0 && rc != NO_MORE_FILES)
    return send_reply (ref p, "550 directory was not found");
  close_directory (ref info);

  /* if path ends with "/.", modify into "/" */
  if (strlen(path) >= 2 && memcmp (path[strlen(path)-2:2], "/.") == 0)
    path[strlen(path)-1] = nul;

  /* if path does not end with "/", append "/" */
  if (path[0] != nul && path[strlen(path)-1] != '/')
    strcat (ref path, "/");

  strcpy (out state.current_dir, path);

  sprintf (out msg, "200 directory changed to \"%s\"", state.current_dir);
  return send_reply (ref p, msg);
}

/*************************************************************************/

int treat_cmd_cdup (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  if (strlen(parameter) > 0)
    return send_reply (ref p, "501 syntax error (no parameter expected)");

  return treat_cmd_cwd (ref p, "..", ref state);
}

/*************************************************************************/

int treat_cmd_list (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  FILE_INFO  info;
  int        rc;
  char       path[512], path2[512];
  DATE_TIME  now;
  TCP_CLIENT h;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

  strcpy (out path2, path);
//  strcat (ref path2, "/*.*");
  normalize_pathname (ref path2);

  if (CMP_FILENAME (path2, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  rc = open_directory (out info, path2);
  if (rc != 0 && rc != NO_MORE_FILES)
    rc = open_directory (out info, path);

  if (rc != 0 && rc != NO_MORE_FILES)
    return send_reply (ref p, "451 action aborted - local processing error");

  if (state.passive_mode)
    rc = TCP_incoming_call (state.passive_server_h, out h);
  else
    rc = CS_connect (out h, state.client_ip, state.client_port, CONNECT_TIMEOUT);

  if (rc < 0)
  {
    (void)close_directory (ref info);
    return send_reply (ref p, "425 cannot open data connection");
  }

  if (send_reply (ref p, "125 file transfer starting ...") < 0)
  {
    (void)close_directory (ref info);
    CS_close (ref h);
    return send_reply (ref p, "451 action aborted - local processing error");
  }


  get_datetime (out now);
 
  while (rc == 0)
  {
    DATE_TIME datetime;
    char xdir[16], xsize[64], xyeartime[64], line[512];
    const string xmonth[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    if (info.type == TYPE_REGULAR_FILE)
    {
      if (strlen(info.name) >= 4 && stricmp (info.name[strlen(info.name)-4:4], ".exe") == 0)
        strcpy (out xdir, "-rwxrwxrwx");
      else
        strcpy (out xdir, "-rw-rw-rw-");
      sprintf (out xsize, "%10d", info.size);
    }
    else
    {
      strcpy (out xdir, "drw-rw-rw-");
      strcpy (out xsize, "         0");
    }

    clock_to_datetime (info.time, 0, out datetime);
    
    if (datetime.year == now.year)
      sprintf (out xyeartime, "%02d:%02d", datetime.hour, datetime.min);
    else
      sprintf (out xyeartime, " %04d", datetime.year);

    sprintf (out line, "%s   1 owner    group  %s %s %2d %s %s\r\n",
             xdir, xsize, xmonth[(datetime.month-1)%12], datetime.day, xyeartime,
             info.name);

    if (CS_write (ref h, line[0:strlen(line)], SEND_TIMEOUT) < 0)
    {
      (void)close_directory (ref info);
      CS_close (ref h);
      return send_reply (ref p, "426 connection aborted");
    }

    rc = read_directory (ref info);
  }

  (void)close_directory (ref info);
  CS_close (ref h);

  if (rc != NO_MORE_FILES)
    return send_reply (ref p, "451 action aborted - local processing error");

  return send_reply (ref p, "250 action completed (ok)");
}

/*************************************************************************/

int treat_cmd_nlst (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  FILE_INFO  info;
  int        rc;
  char       path[512], path2[512];
  TCP_CLIENT h;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

  strcpy (out path2, path);
//  strcat (ref path2, "/*.*");
  normalize_pathname (ref path2);

  if (CMP_FILENAME (path2, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  rc = open_directory (out info, path2);
  if (rc != 0 && rc != NO_MORE_FILES)
    rc = open_directory (out info, path);

  if (rc != 0 && rc != NO_MORE_FILES)
    return send_reply (ref p, "451 action aborted - local processing error");

  if (state.passive_mode)
    rc = TCP_incoming_call (state.passive_server_h, out h);
  else
    rc = CS_connect (out h, state.client_ip, state.client_port, CONNECT_TIMEOUT);

  if (rc < 0)
  {
    (void)close_directory (ref info);
    return send_reply (ref p, "425 cannot open data connection");
  }

  if (send_reply (ref p, "125 file transfer starting ...") < 0)
  {
    (void)close_directory (ref info);
    CS_close (ref h);
    return send_reply (ref p, "451 action aborted - local processing error");
  }

  while (rc == 0)
  {
    char line[512];

    sprintf (out line, "%s\r\n", info.name);

    if (CS_write (ref h, line[0:strlen(line)], SEND_TIMEOUT) < 0)
    {
      (void)close_directory (ref info);
      CS_close (ref h);
      return send_reply (ref p, "426 connection aborted");
    }

    rc = read_directory (ref info);
  }

  (void)close_directory (ref info);
  CS_close (ref h);

  if (rc != NO_MORE_FILES)
    return send_reply (ref p, "451 action aborted - local processing error");

  return send_reply (ref p, "250 action completed (ok)");
}

/*************************************************************************/

int treat_cmd_rnfr (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  int  fd;
  char path[512];

  strcpy (out state.rename_from, "");

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  if ((state.ftp_access_mode & FTP_MODE_WRITE) == 0)
    return send_reply (ref p, "550 no write access rights");

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

  if (CMP_FILENAME (path, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  fd = open (path);
  if (fd < 0)
    return send_reply (ref p, "550 file not found");
  close (fd);

  strcpy (out state.rename_from, path);

  return send_reply (ref p, "350 file action pending ...");
}

/*************************************************************************/

int treat_cmd_rnto (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  char path[512];

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  if ((state.ftp_access_mode & FTP_MODE_WRITE) == 0)
    return send_reply (ref p, "550 no write access rights");

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

  if (CMP_FILENAME (path, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  if (move_file (state.rename_from, path) < 0)
    return send_reply (ref p, "553 rename failed");

  strcpy (out state.rename_from, "");

  return send_reply (ref p, "200 ok");
}

/*************************************************************************/

int treat_cmd_dele (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  char path[512];

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  if ((state.ftp_access_mode & FTP_MODE_WRITE) == 0)
    return send_reply (ref p, "550 no write access rights");

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

  if (CMP_FILENAME (path, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  if (delete_file (path) < 0)
    return send_reply (ref p, "553 file not found (delete failed)");

  return send_reply (ref p, "250 file deleted (ok)");
}

/*************************************************************************/

int treat_cmd_mkd (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  char path[512], msg[1024];

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  if ((state.ftp_access_mode & FTP_MODE_WRITE) == 0)
    return send_reply (ref p, "550 no write access rights");

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

  if (CMP_FILENAME (path, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  if (create_directory (path) < 0)
    return send_reply (ref p, "550 cannot create directory");

  sprintf (out msg, "257 \"%s\" directory created", path);

  return send_reply (ref p, msg);
}

/*************************************************************************/

int treat_cmd_rmd (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  char path[512], msg[1024];

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  if ((state.ftp_access_mode & FTP_MODE_WRITE) == 0)
    return send_reply (ref p, "550 no write access rights");

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

  if (CMP_FILENAME (path, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  if (remove_directory (path) < 0)
    return send_reply (ref p, "550 directory not found");

  sprintf (out msg, "250 \"%s\" directory removed", path);

  return send_reply (ref p, msg);
}

/*************************************************************************/

int treat_cmd_retr (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  int        rc, fd;
  char       path[512], buffer[8192];
  TCP_CLIENT h;

  state.size = -1;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  if ((state.ftp_access_mode & FTP_MODE_READ) == 0)
    return send_reply (ref p, "550 no read access rights");

  /* server calls client and copies file from server to client */

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

#if debug
  trace ("ftp: retr '%s'\n", path);
#endif

  if (CMP_FILENAME (path, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  fd = open (path);
  if (fd < 0)
    return send_reply (ref p, "550 file not found");

  if (state.passive_mode)
    rc = TCP_incoming_call (state.passive_server_h, out h);
  else
    rc = CS_connect (out h, state.client_ip, state.client_port, CONNECT_TIMEOUT);

  if (rc < 0)
  {
    close (fd);
    return send_reply (ref p, "425 cannot open data connection");
  }

  if (send_reply (ref p, "125 file transfer starting ...") < 0)
  {
    CS_close (ref h);
    close (fd);
    return send_reply (ref p, "426 command connection aborted");
  }

  state.size = 0;

  for (;;)
  {
    if (p.stop^)        // user requested service stop
    {
#if debug
      trace ("command aborted due to service shutdown\n");
#endif
      CS_close (ref h);
      close (fd);
      state.size = -1;
      return send_reply (ref p, "421 command aborted - service shutdown");
    }

    rc = read (fd, out buffer);
    if (rc == 0)
      break;
    if (rc < 0)
    {
#if debug
      trace ("error reading from file\n");
#endif
      CS_close (ref h);
      close (fd);
      state.size = -1;
      return send_reply (ref p, "451 action aborted - cannot read file");
    }

#if ANDROID
    {
      char buffer2[2*8192];
      int  i, size;

      if (state.type == 'A')       /* ASCII -> expand (10) to (13,10) */
      {
        size = 0;
        for (i=0; i<rc; i++)
        {
          if (buffer[i] == 10)  // unix newline
          {
            buffer2[size++] = 13;
            buffer2[size++] = 10;
          }
          else
          {
            buffer2[size++] = buffer[i];
          }
        }
      }
      else
      {
        buffer2[0:rc] = buffer[0:rc];
        size = rc;
      }

      if (CS_write (ref h, buffer2[0:size], SEND_TIMEOUT) < 0)
      {
#if debug
        trace ("error sending file data\n");
#endif
        CS_close (ref h);
        close (fd);
        state.size = -1;
        return send_reply (ref p, "426 data connection aborted");
      }

      state.size += size;
    }
#else   // not unix
    if (CS_write (ref h, buffer[0:rc], SEND_TIMEOUT) < 0)
    {
#if debug
      trace ("error sending file data\n");
#endif
      CS_close (ref h);
      close (fd);
      state.size = -1;
      return send_reply (ref p, "426 data connection aborted");
    }
    state.size += rc;
#endif
  }

  CS_close (ref h);

  if (close (fd) < 0)
  {
#if debug
    trace ("error closing file\n");
#endif
    state.size = -1;
  }

  return send_reply (ref p, "250 file action completed (ok)");
}

/*************************************************************************/

int treat_cmd_stor_appe (ref HANDLER_DATA p, string parameter, ref STATE state, bool mode_append)
{
  int   rc, fd;
  uint  index;
  bool  new_file;
  char  path[512], buffer[8192];
  TIMER timer;
  TCP_CLIENT h;

  state.size = -1;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  if ((state.ftp_access_mode & FTP_MODE_WRITE) == 0)
    return send_reply (ref p, "550 no write access rights");

  /* server calls client and copies file from client to server */

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

#if debug
  trace ("ftp: stor/appe '%s'\n", path);
#endif

  if (CMP_FILENAME (path, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  fd = -1;
  new_file = false;

  if (mode_append)
  {
    fd = open (path, READ+WRITE);
    if (fd >= 0)
    {
      if (lseek (fd, 0L, SEEK_END) < 0)
      {
        close (fd);
        return send_reply (ref p, "550 cannot seek in file");
      }
    }
  }

  if (fd < 0)
  {
    fd = create (path, READ+WRITE);
    if (fd < 0)
      return send_reply (ref p, "550 cannot create file");
    new_file = true;
  }

  if (state.passive_mode)
    rc = TCP_incoming_call (state.passive_server_h, out h);
  else
    rc = CS_connect (out h, state.client_ip, state.client_port, (uint)CONNECT_TIMEOUT);

  if (rc < 0)
  {
    close (fd);
    if (new_file)
      delete_file (path);
    return send_reply (ref p, "425 cannot open data connection");
  }

  if (send_reply (ref p, "125 file transfer starting ...") < 0)
  {
    CS_close (ref h);
    close (fd);
    if (new_file)
      delete_file (path);
    return send_reply (ref p, "426 command connection aborted");
  }

  set_timer (out timer, RECV_TIMEOUT);

  state.size = 0;

  clear buffer;

  for (;;)
  {
    if (p.stop^)        // user requested service stop
    {
#if debug
      trace ("command aborted due to service shutdown\n");
#endif
      CS_close (ref h);
      close (fd);
      state.size = -1;
      if (new_file)
        delete_file (path);
      return send_reply (ref p, "421 command aborted - service shutdown");
    }

    index = 0;
    rc = TCP_receive (ref h, ref buffer, ref index);

    if (index > 0)
    {
#if ANDROID
      if (state.type == 'A')     /* ascii -> remove all CR (13) */
      {
        int i, j;
        j = 0;
        for (i=0; i<(int)index; i++)
        {
          if (buffer[i] != 13)
            buffer[j++] = buffer[i];
        }
        index = (uint)j;
      }
#endif

      if (write (fd, buffer[0:index]) != (int)index)
      {
#if debug
        trace ("error writing to file '%s'\n", path);
#endif
        CS_close (ref h);
        close (fd);
        state.size = -1;
        if (new_file)
          delete_file (path);
        return send_reply (ref p, "451 action aborted - cannot write to file");
      }

      state.size += (int)index;

      /* re-arm timer */
      set_timer (out timer, RECV_TIMEOUT);
    }


    /* check for any network problem */

    if (rc < 0)
    {
      if (rc != TCP_CONNECTION_BROKEN)
      {
#if debug
        trace ("error %d in TCP_receive (handler %d)\n", rc, p.nr);
#endif
        CS_close (ref h);
        close (fd);
        state.size = -1;
        if (new_file)
          delete_file (path);
        return send_reply (ref p, "426 data connection aborted");
      }

      CS_close (ref h);

      if (close (fd) < 0)
      {
#if debug
        trace ("error closing file %s\n", path);
#endif
        state.size = -1;
        if (new_file)
          delete_file (path);
        return send_reply (ref p, "451 action aborted - cannot write to file");
      }

      return send_reply (ref p, "250 file action completed (ok)");
    }


    /* check for timeout */

    if (timer_elapsed (timer))
    {
#if debug
      trace ("handler %d : timeout receiving ftp data\n", p.nr);
#endif
      CS_close (ref h);
      close (fd);
      state.size = -1;
      if (new_file)
        delete_file (path);
      return send_reply (ref p, "426 data connection timed-out");
    }

    TCP_wait (ref h, 1);
  }
}

/*************************************************************************/

int treat_cmd_stor (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  return treat_cmd_stor_appe (ref p, parameter, ref state, mode_append => false);
}

/*************************************************************************/

int treat_cmd_appe (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  return treat_cmd_stor_appe (ref p, parameter, ref state, mode_append => true);
}

/*************************************************************************/

int treat_cmd_noop (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  _unused parameter;
  _unused state;
  return send_reply (ref p, "200 ok");
}

/*************************************************************************/

int treat_cmd_rein (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  _unused parameter;
  state.state = _START;
  close_passive_mode (ref state);
  return send_reply (ref p, "220 service ready for new user");
}

/*************************************************************************/

int treat_cmd_quit (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  _unused parameter;
  _unused state;
  (void)send_reply (ref p, "221 closing connection");
  return -1;   /* close connection */
}

/*************************************************************************/

int treat_cmd_size0 (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  char msg[80];

  _unused parameter;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  sprintf (out msg, "200 size %d", state.size);

  return send_reply (ref p, msg);
}

/*************************************************************************/

int treat_cmd_size (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  char path[512], outf[512];
  int  fd;
  long size;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  if ((state.ftp_access_mode & FTP_MODE_READ) == 0)
    return send_reply (ref p, "550 no read access rights");

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

  if (CMP_FILENAME (path, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  strcpy (out outf, "550 cannot read file size");

  fd = open (path);
  if (fd >= 0)
  {
    size = filesize (fd);
    if (size >= 0)
      sprintf (out outf, "213 %d", size);
    close (fd);
  }

  return send_reply (ref p, outf);
}

/*************************************************************************/

int treat_cmd_mdtm (ref HANDLER_DATA p, string parameter, ref STATE state)
{
  char      path[512], outf[512];
  int       fd;
  long      clockt;
  DATE_TIME d;

  if (state.state < _LOGGED_IN)
    return send_reply (ref p, "530 not logged in");

  if ((state.ftp_access_mode & FTP_MODE_READ) == 0)
    return send_reply (ref p, "550 no read access rights");

  clear path;
  if (expand_pathname (state.current_dir, parameter, out path[0:path'length-4]) < 0)
    return send_reply (ref p, "550 directory too long");

  if (CMP_FILENAME (path, state.access_dir, strlen(state.access_dir)) != 0)
    return send_reply (ref p, "550 no access rights");

  sprintf (out outf, "550 No file named \"%.260s\"", path);

  fd = open (path);
  if (fd >= 0)
  {
    get_ftime (fd, out clockt);
    clock_to_datetime (clockt, 0, out d);
    sprintf (out outf, "213 %04d%02d%02d%02d%02d%02d",
             d.year, d.month, d.day, d.hour, d.min, d.sec);
    close (fd);
  }

  return send_reply (ref p, outf);
}

/*************************************************************************/

package JUMP

  typedef int COMMAND (ref HANDLER_DATA p, string parameter, ref STATE state);

  struct VECTOR
  {
    char[4] cmd;
    COMMAND treat_cmd;
  }

  VECTOR[]^ vector;

  void init_vector ();

end JUMP;

/*************************************************************************/

package body JUMP

  public void init_vector ()
  {
    if (vector != null)
      return;
    vector = new VECTOR[] '
             {{"USER", treat_cmd_user},
              {"PASS", treat_cmd_pass},
              {"PORT", treat_cmd_port},
              {"EPRT", treat_cmd_extended_port},
              {"PASV", treat_cmd_pasv},
              {"EPSV", treat_cmd_extended_pasv},
              {"TYPE", treat_cmd_type},
              {"PWD\0",treat_cmd_pwd },
              {"XPWD", treat_cmd_pwd },
              {"CWD\0",treat_cmd_cwd },
              {"XCWD", treat_cmd_cwd },
              {"CDUP", treat_cmd_cdup},
              {"LIST", treat_cmd_list},
              {"NLST", treat_cmd_nlst},
              {"RETR", treat_cmd_retr},
              {"STOR", treat_cmd_stor},
              {"APPE", treat_cmd_appe},
              {"RNFR", treat_cmd_rnfr},
              {"RNTO", treat_cmd_rnto},
              {"DELE", treat_cmd_dele},
              {"MKD\0",treat_cmd_mkd },
              {"XMKD", treat_cmd_mkd },
              {"RMD\0",treat_cmd_rmd },
              {"XRMD", treat_cmd_rmd },
              {"NOOP", treat_cmd_noop},
              {"REIN", treat_cmd_rein},
              {"QUIT", treat_cmd_quit},
              {"$SIZ", treat_cmd_size0},
              {"SIZE", treat_cmd_size},
              {"MDTM", treat_cmd_mdtm}};
  }

end JUMP;

/*************************************************************************/

package CNT
  volatile int count = 0;
end CNT;

/*************************************************************************/

void ftp_connection_handler (ref HANDLER_DATA p)
{
  int         rc, tcp_rc, i, j;
  uint        request_index, request_index_before;
  char        command[512], reply[512], cmd[5], parameter[512];
  TIMER       timer;
  STATE       state;
  DATE_TIME   now;


  /* send greetings including a random stamp */

  clear state;

  get_datetime (out now);

  state.stamp[0] = (byte)(now.year & 255);
  state.stamp[1] = (byte)now.month;
  state.stamp[2] = (byte)now.day;
  state.stamp[3] = (byte)now.hour;
  state.stamp[4] = (byte)now.min;
  state.stamp[5] = (byte)now.sec;
  state.stamp[6] = (byte)(count >> 24);
  state.stamp[7] = (byte)(count >> 16);
  state.stamp[8] = (byte)(count >> 8);
  state.stamp[9] = (byte)(count & 255);

  aes_encrypt (out state.stamp, state.stamp, state.stamp);

  strcpy (out reply, "220 SECURE FTP service ready [");
  for (i=0; i<state.stamp'length; i++)
    strcatf (ref reply, "%02x", state.stamp[i]);
  strcat (ref reply, "]");

  if (send_reply (ref p, reply) < 0)
    return;   // close connection

  state.state        = _START;
  state.size         = 0;
  state.passive_mode = false;

  clear command;

  for (;;)
  {
    /* receive a request */

    set_timer (out timer, INACTIVITY_TIMEOUT);

    request_index = 0;      /* nb bytes received so far */

    for (;;)
    {
      if (p.stop^ && request_index == 0)  /* user requested service stop */
      {
        close_passive_mode (ref state);
        return;   /* close connection */
      }

      /* save request_index */
      request_index_before = request_index;


      tcp_rc = TCP_receive (ref p.tcp_client, ref command, ref request_index);


      /* check for command completion */

      if (request_index == command'size)  /* input buffer is full */
      {
        command[command'size-1] = nul;
        break;
      }

      if (request_index >= 2 && command[request_index-2] == (char)13   /* CR */
                             && command[request_index-1] == (char)10)  /* LF */
      {
        command[request_index-2] = nul;
        break;
      }


      /* check for any network problem */

      if (tcp_rc < 0)
      {
        if (tcp_rc != TCP_CONNECTION_BROKEN)
        {
#if debug
          trace ("error %d in TCP_receive (handler %d)\n", tcp_rc, p.nr);
#endif
        }
        close_passive_mode (ref state);
        return;   /* close connection */
      }


      /* re-arm timer with stricter timeout after having received */
      /* the first request bytes.                                 */

      if (request_index_before == 0 && request_index > 0)
        set_timer (out timer, RECV_TIMEOUT);


      /* check for timeout */

      if (timer_elapsed (timer))
      {
        if (request_index_before > 0)
        {
#if debug
          trace ("handler %d : timeout receiving ftp command\n", p.nr);
#endif
        }
        else
        {
#if debug
          trace ("handler %d : timeout after %u secs inactivity\n", p.nr, INACTIVITY_TIMEOUT);
#endif
        }
        close_passive_mode (ref state);
        return;   // close connection
      }

      TCP_wait (ref p.tcp_client, POLLING_TIMEOUT);
    }


    /* extract command and parameter from command string */

    /* skip trailing spaces */
    i = strlen(command);
    while (i > 0 && command[i-1] == ' ')
      i--;
    command[i] = nul;

    /* skip leading spaces */
    for (i=0; command[i] == ' '; i++)
      ;

    if (strlen(command) - i < 1)   /* command is too short */
    {
      strcpy (out reply, "500 syntax error\r\n");
      if (send_reply (ref p, reply) < 0)
      {
        close_passive_mode (ref state);
        return;   // close connection
      }
      continue;
    }

    clear cmd;
    for (j=0; j<(int)cmd'size-1; j++)
    {
      if (command[i] == ' ' || command[i] == nul)
        break;
      cmd[j] = command[i++];
    }

    /* skip spaces */
    while (command[i] == ' ')
      i++;

    strcpy (out parameter, command[i:strlen(command)-i]);

#if debug
    trace ("ftp command '%s' parameter '%s'\n", cmd, parameter);
#endif

    init_vector ();

    for (i=0;
         i < vector^'length && stricmp (cmd, vector^[i].cmd) != 0;
         i++)
    {
      ;
    }

    if (i == vector^'length)
    {
      strcpy (out reply, "502 command not implemented");
      if (send_reply (ref p, reply) < 0)
      {
        close_passive_mode (ref state);
        return;   // close connection
      }
      continue;
    }

    rc = vector^[i].treat_cmd (ref p, parameter, ref state);

    if (rc < 0)     /* error sending reply */
    {
      close_passive_mode (ref state);
      return;   // close connection
    }
  }
}

/*************************************************************************/

public int ftp_server (FTP_PROFILE        ftp_profile,
                       FTP_CHECK_PASSWORD ftp_check_password,
                       int                listener_port = 21,  // use 0 for default port 21
                       bool^              pstop = null)        // poll variable 'pstop^' used to stop server
{
  int       rc, port;
  FUNCTIONS functions, pfunctions*;
  byte[8]   user_data;
  bool^     ptrace_calls;

  port = (listener_port == 0) ? 21 : listener_port;

#if debug
  trace ("FTP server started - listening on port %d\n", port);
#endif

  functions.ftp_profile        = ftp_profile;
  functions.ftp_check_password = ftp_check_password;
  pfunctions = &functions;

  clear user_data;
  user_data[0:pfunctions'size] = pfunctions'byte;

#if debug
  ptrace_calls = new bool ' (true);
#else
  ptrace_calls = new bool ' (false);
#endif

  rc = serve_requests (port,
                       ftp_connection_handler,
                       max_handlers    => 512,
                       mode            => MODE_DUAL,
                       stop            => pstop,
                       trace_calls     => ptrace_calls,
                       server_is_ready => null,
                       incoming_call   => null,
                       user_data       => user_data);

  free ptrace_calls;

#if debug
  trace ("FTP server halted\n");
#endif

  if (rc < 0)
  {
#if debug
    trace ("error in ftp_server() : serve_requests() returned %d !\n", rc);
#endif
    return rc;
  }

  return 0;
}

/*************************************************************************/

end FTP_SERVER;

/*************************************************************************/
#end unsafe
/*************************************************************************/
