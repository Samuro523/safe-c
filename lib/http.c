
// http.c : Internet Client & Server

use calendar, files, strings, tracing, tcpip, thread, url;

//---------------------------------------------------------------------------

package body HTTP_CLIENT

  // note: we support HTTP version 1.0 and 1.1
  //       version 0.9 will time-out because there is no header.

  //-----------------------------------------------------------------------------------

  const bool DEBUG_HEADER  = true;  // trace reply header
  const bool DEBUG_CONNECT = true;  // trace selected connection + reconnections
  const bool DEBUG_REQUEST = true;  // trace request
  const bool DEBUG_WAIT    = true;
  const bool DEBUG_CHUNK   = true;  // trace chunk states
  const bool DEBUG_BYTES   = true;  // trace bytes received
  const bool DEBUG_IP_PORT = true;

  const uint CONNECT_TIMEOUT  =   5;   // seconds for connection
  const uint SEND_TIMEOUT     =   5;   // seconds without sending data
  const uint RECEIVE_TIMEOUT  = 120;   // seconds without receiving data

  SHARED_OBJECT shared;

  //-----------------------------------------------------------------------------------

  struct HTTP_CONNECTION
  {
    bool       is_open;
    bool       busy;                          // true = request currently busy
    char       host[URL_HOST_LENGTH];
    int        port;
    TCP_CLIENT handle;                        // from TCP_connect()
  }

  const int MAX_CONNECTIONS = 4;    // max simultaneous persistent connections

  HTTP_CONNECTION conn[MAX_CONNECTIONS];

  //-----------------------------------------------------------------------------------

  // wait until all requests are finished, then close all open connections

  public void reset_http_service ()
  {
    int i;

    enter_shared_object (ref shared);

    for (i=0; i<MAX_CONNECTIONS; i++)
    {
      while (conn[i].busy)
        sleep 0.25;

      if (conn[i].is_open)
      {
        TCP_hangup (ref conn[i].handle);
        conn[i].is_open = false;
      }
    }

    leave_shared_object (ref shared);
  }

  //-----------------------------------------------------------------------------------

  int unprotected_select_connection (string host, int port)
  {
    int i;

    for (;;)
    {
      // search first for an open and not busy connection with this host name

      for (i=0; i<MAX_CONNECTIONS; i++)
      {
        if ((!conn[i].busy) && conn[i].is_open && stricmp (host, conn[i].host) == 0 && port == conn[i].port)
          return i;
      }


      // search for any closed and not busy connection

      for (i=0; i<MAX_CONNECTIONS; i++)
      {
        if ((!conn[i].busy) && (!conn[i].is_open))
          return i;
      }


      // search for any non-busy connection

      for (i=0; i<MAX_CONNECTIONS; i++)
      {
        if (!conn[i].busy)
        {
          if (conn[i].is_open)
          {
            TCP_hangup (ref conn[i].handle);
            conn[i].is_open = false;
          }

          return i;
        }
      }


      // wait until some connection might not be busy anymore, then try again

      sleep 0.25;
    }
  }

  //-----------------------------------------------------------------------------------

  int select_connection (string host, int port)
  {
    int index;

    enter_shared_object (ref shared);

    index = unprotected_select_connection (host, port);

    conn[index].busy = true;
    strcpy (out conn[index].host, host);
    conn[index].port = port;

    leave_shared_object (ref shared);

    return index;
  }

  //-----------------------------------------------------------------------------------

  int open_connection (ref HTTP_CONNECTION h, URL url, HTTP_PARAMETERS param)
  {
    int  rc;
    uint connect_timeout;

    // connect to host

    if (DEBUG_CONNECT && param.trace)
      trace ("  connecting to %s port %d ...\n", url.host, url.port);


    if (param.connect_timeout <= 0)
      connect_timeout = CONNECT_TIMEOUT;
    else
      connect_timeout = (uint)param.connect_timeout;

    if (param.use_proxy)
      rc = CS_connect (out h.handle, param.proxy_ip, param.proxy_port, connect_timeout);
    else
      rc = CS_connect (out h.handle, url.host, url.port, connect_timeout);

    if (rc < 0)
    {
      if (param.trace)
      {
        trace ("error %d : cannot connect to %s port %d\n", rc, url.host, url.port);
        if (param.use_proxy)
          trace ("      through proxy server %s port %d\n", param.proxy_ip, param.proxy_port);
      }

      return HTTP_CONNECT_ERROR;
    }


    h.is_open = true;

    if (DEBUG_IP_PORT && param.trace)
    {
      PEER     a;
      char[48] ipstr;

      TCP_query_local_address (h.handle, out a);
      ip_to_numericstr (a.ip, out ipstr);
      trace ("local: %s port %u\n", ipstr, a.port);
      TCP_query_remote_address (h.handle, out a);
      ip_to_numericstr (a.ip, out ipstr);
      trace ("remote: %s port %u\n", ipstr, a.port);
    }

    return 0;
  }

  //-----------------------------------------------------------------------------------

  int get_gmt_file_time (string filename, out DATE_TIME gmt_time)
  {
    int  fd;
    long clocks;

    clear gmt_time;

    fd = open (filename, READ, READ+WRITE);
    if (fd < 0)
      return -1;

    get_ftime (fd, out clocks);

    close (fd);

    clock_to_datetime (clocks, 0, out gmt_time);

    return 0;
  }

  //-----------------------------------------------------------------------------------

  int set_gmt_file_time (string filename, DATE_TIME gmt_time)
  {
    long  clocks;
    int   fd;

    datetime_to_clock (gmt_time, 0, out clocks);

    fd = open (filename, READ+WRITE, READ+WRITE);
    if (fd < 0)
      return -1;

    set_ftime (fd, clocks);

    close (fd);

    return 0;
  }

  //-----------------------------------------------------------------------------------

  package DATA

    const string wkday[8] = {"", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    const string mtday[13] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  end DATA;

  //-----------------------------------------------------------------------------------

  // replace 0x0D and 0x0A by "<CR>" and "<LF>" respectively
  // to avoid line breaks in the trace file.

  void make_printable (string source, out string target)
  {
    int  i, j;
    char c;

    clear target;

    i = 0;
    j = 0;

    while (i < source'length)
    {
      c = source[i++];

      if (c == nul)
        break;

      if (c == '\r')
      {
        target[j++] = '<';
        target[j++] = 'C';
        target[j++] = 'R';
        target[j++] = '>';
      }
      else if (c == '\n')
      {
        target[j++] = '<';
        target[j++] = 'L';
        target[j++] = 'F';
        target[j++] = '>';
      }
      else
      {
        target[j++] = c;
      }
    }
  }

  //-----------------------------------------------------------------------------------

  int send_request (ref HTTP_CONNECTION h, URL url, URL base, string filename, HTTP_PARAMETERS param)
  {
    char      request[15+18+51+50+2+32+3*URL_HOST_LENGTH+URL_FILENAME_LENGTH], str[80];
    int       rc, len;
    DATE_TIME file_time;

    sprintf (out request, "GET %s HTTP/1.1\r\n", url.filename);  // 15 + HOST

    if (base.host[0] != nul)
    {
      sprintf (out str, "Referer: http://%s%s\r\n", base.host, base.filename);  // 18 + HOST + FILENAME
      strcat (ref request, str);
    }

    sprintf (out str, "User-Agent: Mozilla/4.5.1\r\nHost: %s\r\nAccept: */*\r\n", url.host); // 51 + HOST
    strcat (ref request, str);


    // try to open the local file and retrieve its date and add a time condition

    rc = get_gmt_file_time (filename, out file_time);
    if (rc == 0 &&
        is_valid_date (file_time.day, file_time.month, file_time.year))
    {
      sprintf (out str,
               "If-Modified-Since: %s, %02d %s %04d %02d:%02d:%02d GMT\r\n",   // 50
               wkday[day_of_week (file_time.day, file_time.month, file_time.year)],
               file_time.day, mtday[file_time.month],
               file_time.year, file_time.hour, file_time.min, file_time.sec);
      strcat (ref request, str);
    }

    strcat (ref request, "\r\n");   // 2


    len = strlen(request);

    if (DEBUG_REQUEST && param.trace)
    {
      char tempstr[request'size + 80];
      make_printable (request[0:len], out tempstr);
      trace ("%s\n", tempstr);
    }


    // send the request

    rc = CS_write (ref h.handle, request[0:len], SEND_TIMEOUT);
    if (rc < 0)
    {
      if (param.trace)
        trace ("error %d in CS_write() to host %s port %d\n", rc, url.host, url.port);

      return HTTP_SEND_ERROR;
    }

    return 0;
  }

  //-----------------------------------------------------------------------------------

  // request array limited by sentinel value 13.

  void analyze_http_header (ref string      reply,
                            uint            header_size,
                            URL             url,
                            HTTP_PARAMETERS param,
                            out HTTP_HEADER http)
  {
    uint i, j, param_start, param_len, value_len, saved_pos;
    int  major, minor;
    char par[20], value[256];

    clear http;

    http.version = -1;  // default values
    http.size    = -1;


    // "HTTP/1.1 200 OK"

    i = 5;   // skip "HTTP/"

    if (!isdigit(reply[i]))
      return;

    major = (int)reply[i++] - (int)'0';

    if (reply[i] != '.')
      return;
    i++;

    if (!isdigit(reply[i]))
      return;
    minor = (int)reply[i++] - (int)'0';

    http.version = 100*major + minor;

    while (reply[i] == ' ' || reply[i] == '\t')    // skip white space
      i++;

    for (j=0; j<3; j++)         // 3-digit http code
    {
      if (!isdigit(reply[i]))
      {
        clear http.code;
        return;
      }
      http.code[j] = reply[i];
      i++;
    }

    while (reply[i] == ' ' || reply[i] == '\t')    // skip white space
      i++;


    // retrieve error message

    j = 0;
    while (reply[i] != (char)13 && reply[i] != (char)10 && j < (uint)http.msg'length)
      http.msg[j++] = reply[i++];


    // skip to end of line

    while (reply[i] != (char)13 && reply[i] != (char)10)
      i++;


    if (DEBUG_HEADER && param.trace)
      trace ("  %s\n", reply[0:i]);


    for (;;)
    {
      // skip CR | LF | CR LF

      if (i < header_size && reply[i] == (char)13)
        i++;

      if (i < header_size && reply[i] == (char)10)
        i++;

      if (i == header_size)      // end of header
        break;

      param_start = i;      // start of parameter name

      while (i < header_size && reply[i] > ' ' && reply[i] != ':')
        i++;

      param_len = i - param_start;

      if (param_len > 0)
      {
        strncpy (out par, reply[param_start:param_len], par'length);

        while (i < header_size && (reply[i] == ' ' || reply[i] == '\t'))    // skip white space
          i++;

        if (i < header_size && reply[i] == ':')
        {
          i++;                  // skip ':'

          while (reply[i] == ' ' || reply[i] == '\t')   // skip white space
            i++;

          value_len = 0;
          clear value;

          for (;;)
          {
            while (reply[i] != (char)13 && reply[i] != (char)10 && value_len < (uint)value'length)
              value[value_len++] = reply[i++];

            // remove trailing white space
            while (value_len > 0 &&
                   (value[value_len-1] == ' ' || value[value_len-1] == '\t'))
              value_len--;

            while (reply[i] != (char)13 && reply[i] != (char)10)
              i++;

            saved_pos = i;

            // check if the value runs over multiple lines

            if (i < header_size && reply[i] == (char)13)
              i++;

            if (i < header_size && reply[i] == (char)10)
              i++;

            if (i == header_size || (reply[i] != ' ' && reply[i] != '\t'))
            {
              i = saved_pos;      // no more data
              break;
            }

            // folding

            // append a space
            if (value_len < (uint)value'length)
              value[value_len++] = ' ';

            // skip white space
            while (reply[i] == ' ' || reply[i] == '\t')
              i++;

            // loop to parse line continuation
          }

          // append nul
          if (value_len < (uint)value'length)
            value[value_len++] = nul;


          // evaluate (param,value)

          if (DEBUG_HEADER && param.trace)
            trace ("  %s = %s\n", par, value);

          if (stricmp (par, "SERVER") == 0)
          {
            strncpy (out http.server, value, http.server'length);
          }
          else if (stricmp (par, "CONNECTION") == 0)
          {
            strncpy (out http.connect, value, http.connect'length);
          }
          else if (stricmp (par, "LOCATION") == 0)
          {
            strncpy (out http.location, value, http.location'length);
          }
          else if (stricmp (par, "CONTENT-TYPE") == 0)
          {
            strncpy (out http.type, value, http.type'length);
          }
          else if (stricmp (par, "TRANSFER-ENCODING") == 0)
          {
            strncpy (out http.coding, value, http.coding'length);
          }
          else if (stricmp (par, "CONTENT-LENGTH") == 0)
          {
            sscanf (value, "%d", out http.size);
          }
          else if (stricmp (par, "LAST-MODIFIED") == 0 && value_len >= 29)
          {
            DATE_TIME date;
            char      month[3], dummy;

            /* Thu, 01 Dec 1994 16:00:00 GMT */

            clear date;
            month[0] = nul;
            if (sscanf (value, "%*s %d %3s %d %2d%c%2d%c%2d",
                        out date.day, out month, out date.year,
                        out date.hour, out dummy, out date.min, out dummy, out date.sec) >= 0)
            {
              _unused dummy;

              for (date.month=1; date.month<=12; date.month++)
              {
                if (memcmp (month, mtday[date.month]) == 0)
                  break;
              }

              if (date.year <= 99)   // year in 2 digits
              {
                DATE_TIME now;
                date.year += 1900;
                get_datetime (out now);
                while (date.year < now.year - 50)
                  date.year += 100;
              }

              if (is_valid_date (date.day, date.month, date.year) &&
                  date.hour >= 0 && date.hour <= 23 &&
                  date.min  >= 0 && date.min  <= 59 &&
                  date.sec  >= 0 && date.sec  <= 59)
              {
                http.date = date;
              }
            }
          }
        }
      }


      // skip to end of line

      while (reply[i] != (char)13 && reply[i] != (char)10)
        i++;
    }

    // small corrections :

    // 1) if Transfer-Encoding == "chunked", then the Content-Length value
    //    should not be used.

    if (stricmp (http.coding, "chunked") == 0)
      http.size = -1;

    // 2) in case of redirection (301/302) without new location, and the
    //    original URL has no ending slash, provide it.

    if ((strcmp (http.code, "301") == 0 || strcmp (http.code, "302") == 0) &&
        http.location[0] == nul &&     // no new location
        url.filename[0] != nul &&
        url.filename[strlen(url.filename)-1] != '/' &&
        7 + strlen(url.host) + 6 + strlen(url.filename) + 3 < http.location'length)
    {
      char portstr[10];
      strcpy (out http.location, "http://");
      strcat (ref http.location, url.host);
      if (url.port != 80)
      {
        sprintf (out portstr, ":%d", url.port);
        strcat (ref http.location, portstr);
      }
      strcat (ref http.location, url.filename);
      strcat (ref http.location, "/");
    }
  }

  //-----------------------------------------------------------------------------------

  // reply array limited by sentinel value 13.
  // returns header size, or 0 if no header

  uint check_header_is_complete (char reply[])
  {
    int  i;                 // index of end-of-header
    bool header_complete;

    // search for two CR, or two LF, or two CRLF, whatever comes first

    i = 0;
    for (;;)
    {
      // skip data
      while (reply[i] != (char)13 && reply[i] != (char)10)
        i++;


      // skip CR | LF | CR LF

      if (reply[i] == (char)13)
        i++;

      if (i < reply'length && reply[i] == (char)10)
        i++;

      if (i == reply'length)   // end of reply data reached
        return 0;


      // check if another CR LF follows

      header_complete = false;

      if (i < reply'length-1 && reply[i] == (char)13)
      {
        header_complete = true;
        i++;
      }

      if (i < reply'length-1 && reply[i] == (char)10)
      {
        header_complete = true;
        i++;
      }

      if (header_complete)
        return (uint)i;
    }
  }

  //-----------------------------------------------------------------------------------

  // write to file and check file limit not exceeded

  int write_file (int             fd,
                  byte[]          buffer,
                  HTTP_PARAMETERS param,
                  ref HTTP_HEADER http)
  {
    int  rc;

    if (param.max_file_size != 0 &&
        http.filesize + buffer'length > param.max_file_size)
      return -1;

    rc = write (fd, buffer);
    if (rc > 0)
      http.filesize += rc;

    return rc;
  }

  //-----------------------------------------------------------------------------------

  public int get_http_file (url.URL         url,       // URL declared in url.h
                            URL             base,      // referer URL - can be cleared
                            HTTP_PARAMETERS param,
                            string          filename,
                            out HTTP_HEADER http)
  {
    int         nr, rc, tcp_rc, outf, try;
    bool        was_open;
    char        reply[32768+1];    // 1-byte sentinel
    uint        reply_index, reply_size, reply_offset, to, header_size, size;
    TIMER       timer;
    int         chunk_state;
    uint        already_written_size, chunk_size = 0, chunk_rest_size = 0;


    clear http;


    // retry a connection twice

    for (try=1; try<=2; try++)
    {
      // open connection

      nr = select_connection (url.host, url.port);

      if (DEBUG_CONNECT && param.trace)
        trace ("  select connection %d\n", nr);

      {
        ref HTTP_CONNECTION h = conn[nr];

        was_open = h.is_open;

        if (!h.is_open)     // the connection to this host is not yet open
        {
          rc = open_connection (ref h, url, param);
          if (rc < 0)
          {
            h.busy = false;
            return HTTP_CONNECT_ERROR;
          }
        }


        rc = send_request (ref h, url, base, filename, param);
        if (rc < 0)
        {
          TCP_hangup (ref h.handle);
          h.is_open = false;

          if (try == 1 && was_open)
          {
            if (param.trace)
              trace ("info: reconnect ...\n");
            h.busy = false;
            continue;
          }

          h.busy = false;
          return HTTP_SEND_ERROR;
        }


        // receive the reply block

        clear reply;

        already_written_size = 0;
        outf = -1;   // the output file will only be created when data
                     // is received (maybe we keep the old file).

        reply_index  = 0;
        reply_size   = (uint)reply'length - 1;    // leave 1 byte for sentinel
        reply_offset = 0;                         // first char to consume

        chunk_state = 1;     // next : parse hex size

        if (param.receive_timeout <= 0)
          to = RECEIVE_TIMEOUT;
        else
          to = (uint)param.receive_timeout;
        set_timer (out timer, to);

        header_size = 0;

        for (;;)
        {
          if (reply_index == reply_size &&      // buffer is full
              reply_index - reply_offset < 128) // less than 128 bytes to consume
          {
            // move remaining bytes to beginning of buffer
            reply[0:reply_index - reply_offset] = reply[reply_offset:reply_index - reply_offset];
            reply_index -= reply_offset;
            reply_offset = 0;
          }

          if (reply_index == reply_size)   // data was not consumed ?
          {
            if (param.trace)
              trace ("error: intern_error : buffer is full\n");

            TCP_hangup (ref h.handle);
            h.is_open = false;

            if (outf >= 0)
            {
              close (outf);
              delete_file (filename);
            }

            h.busy = false;
            return HTTP_INTERN_ERROR;
          }


          /* receive next chunk */

          tcp_rc = TCP_receive (ref h.handle, ref reply[0:reply_size], ref reply_index);


          /* check if the header is complete and if yes, parse it */

          if (header_size == 0)
          {
            if (reply_index >= 8)
            {
              if (strnicmp (reply, "HTTP/", 5) != 0)   // bad syntax
              {
                if (param.trace)
                  trace ("error: http reply has bad syntax\n");
                TCP_hangup (ref h.handle);
                h.is_open = false;
                h.busy = false;
                return HTTP_NO_DATA;
              }

              reply[reply_index] = (char)13;    // put sentinel
              header_size = check_header_is_complete (reply[0:reply_index+1]);
            }

            if (header_size == 0)    // header not yet complete
            {
              if (reply_index == reply_size)   // buffer is full
              {
                if (param.trace)
                  trace ("error: http reply header is too long\n");
                TCP_hangup (ref h.handle);
                h.is_open = false;
                h.busy = false;
                return HTTP_NO_DATA;
              }
            }
            else   /* header is completely read-in : analyse it */
            {
              analyze_http_header (ref reply, header_size, url, param, out http);

              reply_offset = header_size;    // skip the header's data

              /* In HTTP 1.0, there is a body                 */
              /* - if the field CONTENT-LENGTH is present, or */
              /* - if data arrives just after the header      */
              /*   (the connection is then closed to indicate */
              /*    the end of the body).                     */

              /* In HTTP 1.1, there is a body                 */
              /* - if the field CONTENT-LENGTH is present, or */
              /* - if the field TRANSFER-ENCODING is present, */
              /* - or if "Connection : close"                 */

              /* handle the cases where we are sure there is no body */
              /* or where we don't want a body.                      */

              if ((http.version   >= 101 &&    /* version HTTP 1.1           */
                   http.size      == -1  &&    /* no field CONTENT-LENGTH    */
                   http.coding[0] == nul &&    /* no field TRANSFER-ENCODING */
                   stricmp (http.connect, "close") != 0) ||
                  (http.version == 100 &&      /* version HTTP 1.0           */
                   http.code[0] != '2'))       /* with error code.           */
              {
                if (param.trace)
                  trace ("note: no http reply body expected/wanted\n");

                if (http.version == 100 ||
                    stricmp (http.connect, "close") == 0)
                {
                  TCP_hangup (ref h.handle);
                  h.is_open = false;
                }

                if ((strcmp (http.code, "301") == 0 ||  // redirection
                     strcmp (http.code, "302") == 0) &&
                    http.location[0] != nul)
                {
                  h.busy = false;
                  return HTTP_REDIRECT;
                }

                if (strcmp (http.code, "304") == 0)     // not modified
                {
                  h.busy = false;
                  return 0;
                }

                if (http.code[0] != '2')
                {
                  h.busy = false;
                  return HTTP_ERROR_CODE;
                }

                h.busy = false;
                return HTTP_NO_DATA;
              }


              /* we expect a body to follow the header */

              if (http.code[0] == '2')   // OK : we will overwrite the file
              {
                outf = create (filename, WRITE);
                if (outf < 0)
                {
                  if (param.trace)
                    trace ("error: http : cannot create local file %s\n", filename);

                  /* we have to close tcp/ip because we have no opportunity */
                  /* to empty the data sent by the server.                  */

                  TCP_hangup (ref h.handle);
                  h.is_open = false;
                  h.busy = false;
                  return HTTP_FILE_CREATE_ERROR;
                }
              }

              /* we expect a body to follow, so we need to re-arm */
              /* the timer to prevent time-out.                   */

              set_timer (out timer, to);
            }
          }
          else   /* header_complete : we're receiving a block of the body */
          {
            if (stricmp (http.coding, "chunked") == 0)
            {
              /*  <white space>1000<white space><CR><LF>
                  (0x1000 data bytes following)<CR><LF>
                  <white space>2<white space><CR><LF>
                  (0x2 data bytes following)<CR><LF>
                  <white space>0<white space><CR><LF>
                  (0 data bytes following)<CR><LF>
                  {header_line CR LF}
                  CR LF
              */


              for (;;)   // consume a chunk
              {
                if (chunk_state == 1)   /* next : read hex size */
                {
                  uint i, start, endi;
                  int  sret;

                 reply[reply_index] = (char)13;    /* put sentinel */

                  start = reply_offset;
                  i     = reply_offset;

                  /* proceed until next CR */
                  while (reply[i] != (char)13)
                    i++;

                  endi = i;

                  if (i+1 < reply_index && reply[i] == (char)13 && reply[i+1] == (char)10)
                  {
                    i += 2;

                    reply[endi] = nul;   /* put hard terminator */
                    sret = sscanf (reply[start:reply_index-start], "\t%x", out chunk_size);
                    reply[endi] = (char)13;     /* remove hard terminator */

                    if (sret >= 0)
                    {
                      if (DEBUG_CHUNK && param.trace)
                        trace ("chunk_size : %u (0x%x)\n", chunk_size, chunk_size);

                      reply_offset = i;                 /* new start of data */
                      chunk_rest_size = chunk_size;
                      chunk_state = 2;     /* next : read 'chunk_size' bytes */

                      if (param.trace)
                        trace ("state 2\n");

                      set_timer (out timer, to);
                    }
                  }
                }

                if (chunk_state == 2)   /* read 'chunk_rest_size' bytes */
                {
                  /* consume received data */

                  if (reply_index > reply_offset)    /* we received some data */
                  {
                    size = reply_index - reply_offset;

                    if (size > chunk_rest_size)   /* constrain by chunk_rest_size */
                      size = chunk_rest_size;

                    if (DEBUG_BYTES && param.trace)
                      trace ("received %u bytes\n", size);

                    if (outf >= 0)
                    {
                      rc = write_file (outf, reply[reply_offset:size], param, ref http);
                      if ((uint)rc != size)
                      {
                        if (param.trace)
                          trace ("error: http : cannot write to local file %s\n", filename);
                        close (outf);
                        delete_file (filename);
                        TCP_hangup (ref h.handle);
                        h.is_open = false;
                        h.busy = false;
                        return HTTP_FILE_WRITE_ERROR;
                      }
                    }

                    reply_offset    += size;
                    chunk_rest_size -= size;

                    set_timer (out timer, to);
                  }

                  if (chunk_rest_size == 0)   /* we read the full data part */
                  {
                    chunk_state = 3;

                    if (DEBUG_CHUNK && param.trace)
                      trace ("state 3\n");
                  }
                }


                if (chunk_state == 3)
                {
                  if (chunk_size > 0)   /* more part are following */
                  {
                    uint i;

                    /* skip CR LF */
                    i = reply_offset;
                    if (i+1 < reply_index && reply[i] == (char)13 && reply[i+1] == (char)10)
                    {
                      reply_offset = i + 2;
                      chunk_state = 1;               /* restart in state 1 */

                      if (DEBUG_CHUNK && param.trace)
                        trace ("state 1\n");

                      set_timer (out timer, to);

                      continue;   /* consume next chunk */
                    }
                  }
                  else    /* no more parts following */
                  {
                    uint i;
                    bool done;

                    /* parse : {header-line CR LF} CR LF */

                    reply[reply_index] = (char)13;   /* put a sentinel */

                    done = false;
                    i = reply_offset;

                    for (;;)
                    {
                      if (i == reply_index)      /* not found */
                        break;

                      if (i + 1 < reply_index && reply[i] == (char)13 && reply[i+1] == (char)10)
                      {
                        reply_offset = i + 2;    /* after header */
                        done = true;                /* the end ! */
                        break;
                      }

                      /* skip to end of line, then skip CR LF */

                      while (reply[i] != (char)13)
                        i++;

                      if (i == reply_index)      /* not found */
                        break;

                      i++;     /* skip CR */

                      if (i == reply_index || reply[i] != (char)10)
                        break;

                      i++;     /* skip LF */
                    }

                    if (done)
                    {
                      if (reply_offset != reply_index)
                      {
                        if (param.trace)
                          trace ("warning : http : incorrect end of data in chunk\n");
                      }

                      if (outf >= 0)
                      {
                        if (close (outf) < 0)
                        {
                          if (param.trace)
                            trace ("error: cannot close file %s\n", filename);
                          delete_file (filename);
                          TCP_hangup (ref h.handle);
                          h.is_open = false;
                          h.busy = false;
                          return HTTP_FILE_CLOSE_ERROR;
                        }

                        /* set the file's date */
                        if (is_valid_date (http.date.day, http.date.month, http.date.year))
                          (void)set_gmt_file_time (filename, http.date);
                      }

                      if (stricmp (http.connect, "close") == 0)
                      {
                        TCP_hangup (ref h.handle);
                        h.is_open = false;
                      }

                      if ((strcmp (http.code, "301") == 0 ||  /* redirection */
                           strcmp (http.code, "302") == 0) &&
                           http.location[0] != nul)
                      {
                        h.busy = false;
                        return HTTP_REDIRECT;
                      }

                      if (strcmp (http.code, "304") == 0)   /* Not Modified */
                      {
                        h.busy = false;
                        return 0;
                      }

                      if (http.code[0] != '2')
                      {
                        h.busy = false;
                        return HTTP_ERROR_CODE;
                      }

                      h.busy = false;
                      return 0;
                    }
                  }
                }

                break;

              } // for consume a chunk
            }
            else   /* normal (non-chunked) data */
            {
              /* consume received data */

              if (reply_index > reply_offset)    /* we received some data */
              {
                size = reply_index - reply_offset;

                if (DEBUG_BYTES && param.trace)
                  trace ("received %u bytes\n", size);

                if (outf >= 0)
                {
                  rc = write_file (outf, reply[reply_offset:size], param, ref http);
                  if ((uint)rc != size)
                  {
                    if (param.trace)
                      trace ("error: http : cannot write to local file %s\n", filename);
                    close (outf);
                    delete_file (filename);
                    TCP_hangup (ref h.handle);
                    h.is_open = false;
                    h.busy = false;
                    return HTTP_FILE_WRITE_ERROR;
                  }
                }

                already_written_size += size;
                reply_offset = reply_index;

                /* we consumed data, so we need to re-arm */
                /* the timer to prevent time-out.         */
                set_timer (out timer, to);

                if (http.size != -1 && already_written_size > http.size)
                {
                  if (param.trace)
                    trace ("error: too much data received (received=%u,expected=%d)\n",
                           already_written_size, http.size);

                  if (outf >= 0)
                  {
                    close (outf);
                    delete_file (filename);
                  }

                  TCP_hangup (ref h.handle);
                  h.is_open = false;
                  h.busy = false;
                  return HTTP_LONG_DATA;
                }
              }


              /* check if the body is complete */

              if ((already_written_size == http.size) ||
                  (tcp_rc == TCP_CONNECTION_BROKEN && http.version == 100
                                                   && http.size == -1) ||
                  (tcp_rc == TCP_CONNECTION_BROKEN && http.size == -1
                                 && stricmp (http.connect, "close") == 0))
              {
                if (outf >= 0)
                {
                  if (close (outf) < 0)
                  {
                    if (param.trace)
                      trace ("error: cannot close file %s\n", filename);
                    delete_file (filename);
                    TCP_hangup (ref h.handle);
                    h.is_open = false;
                    h.busy = false;
                    return HTTP_FILE_CLOSE_ERROR;
                  }

                  /* set the file's date */
                  if (is_valid_date (http.date.day,
                                     http.date.month,
                                     http.date.year))
                    (void)set_gmt_file_time (filename, http.date);
                }
              }

              if (http.version == 100 || stricmp (http.connect, "close") == 0)
              {
                TCP_hangup (ref h.handle);
                h.is_open = false;
              }

              if ((strcmp (http.code, "301") == 0 ||  /* redirection */
                   strcmp (http.code, "302") == 0) &&
                  http.location[0] != nul)
              {
                h.busy = false;
                return HTTP_REDIRECT;
              }

              if (strcmp (http.code, "304") == 0)   /* Not Modified */
              {
                h.busy = false;
                return 0;
              }

              if (http.code[0] != '2')
              {
                h.busy = false;
                return HTTP_ERROR_CODE;
              }

              h.busy = false;
              return 0;
            }
          }  // if header complete


          /* check for any network problem */

          if (tcp_rc < 0)
          {
            if (param.trace)
              trace ("error %d in http : TCP_receive() from host %s port %d\n", tcp_rc, url.host, url.port);

            TCP_hangup (ref h.handle);
            h.is_open = false;

            if (outf >= 0)
            {
              close (outf);

              if (!param.accept_incomplete_files)
                delete_file (filename);
            }


            /* special case : if zero bytes were received and the */
            /* connection was initially open, retry the request.  */

            if (header_size == 0 && reply_index == 0 && try == 1 && was_open)
            {
              h.busy = false;
              continue;
            }

            if (param.accept_incomplete_files)
            {
              h.busy = false;
              return 0;
            }

            h.busy = false;
            return HTTP_RECEIVE_ERROR;
          }


          /* timeout */

          if (timer_elapsed (timer))      /* no data received for n seconds */
          {
            if (param.trace)
              trace ("warning: timeout in http : TCP_receive() from host %s port %d\n", url.host, url.port);

            TCP_hangup (ref h.handle);
            h.is_open = false;

            if (outf >= 0)
            {
              close (outf);
              delete_file (filename);
            }

            h.busy = false;
            return HTTP_TIMEOUT_ERROR;
          }

          TCP_wait (ref h.handle, nsecs => 1);
        }
      }  // end ref h

    }  // for (try=1; try<=2; try++)

    abort;   // we should never arrive here
  }

  //-----------------------------------------------------------------------------------

end HTTP_CLIENT;

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

package body HTTP_SERVER

  //---------------------------------------------------------------------------

  const bool SIMULATE_SLOW_CONNECTION = false;

  //---------------------------------------------------------------------------
#begin unsafe
  //---------------------------------------------------------------------------

  package POST_INFO

    const int POST_BUFFER_SIZE        = 64*1024;
    const int POST_SEPARATOR_MAX_SIZE = 1024;

    struct POST_PART_INFO
    {
      int             offset_part;      // offset within temp file
      string^         parameter_name;   // null or allocated on heap
      int             offset_data;      // offset within temp file
      int             size_data;        // size of data
      string^         filename;         // null or allocated on heap
      POST_PART_INFO^ next;
    }

  end POST_INFO;

  //---------------------------------------------------------------------------

  enum TYP
  {
    DATA_NONE,     // GET
    DATA_IN_RAM,   // POST
    DATA_IN_FILE,  // POST
  };

  //---------------------------------------------------------------------------

  struct ARG_INFO    // full type
  {
    // POST parameters
    TYP                  typ;            // see DATA_xx constants above
    string^              post_data;      // pointer to memory for DATA_IN_RAM
    string^              pfilename;      // temp filename with post data
    POST_PART_INFO^      part;           // list of each part of the temp file

    // HTTP parameters
    char*                request;        // request buffer

    // server data
    HTTP_SERVER_DATA*    server_data;    // possibly null
    URL*                 url;
  }

  const int REQUEST_SIZE = 8192;

  //---------------------------------------------------------------------------

  void compute_extension (string filename, out char[16] extension)
  {
    int i, j, len;

    clear extension;

    i = strrchr (filename, '.');
    if (i == -1)
      return;

    j = strrchr (filename, '/');
    if (j > i)
      return;

    i++;
    len = strlen (filename) - i;

    if (len > extension'length)
      len = extension'length;

    extension[0:len] = filename[i:len];
  }

  //---------------------------------------------------------------------------

  void trace_data_block (string s)
  {
    int i, pos;

    i = 0;
    for (;;)
    {
      pos = strchr (s[i:s'length-i], '\r');
      if (pos == -1)
      {
        if (i < s'length && s[i] != nul)
           trace ("   \"%s\"\n", s[i:s'length-i]);
        return;
      }

      trace ("   \"%s\"\n", s[i:pos]);

      i += pos;
      if (i < s'length && s[i] == '\r')
        i++;
      if (i < s'length && s[i] == '\n')
        i++;
    }
  }

  //---------------------------------------------------------------------------

  void free_part_list (POST_PART_INFO^ list)
  {
    POST_PART_INFO^ p, q;

    q = list;
    while (q != null)
    {
      p = q;
      q = q^.next;
      free p^.parameter_name;
      free p^.filename;
      free p;
    }
  }

  //---------------------------------------------------------------------------

  string^ get_param_string (string buffer, string token)
  {
    bool in_quote = false;
    int  start, i, len;
    char c;

    start = -1;   // start of word
    i     = 0;

    while (i < buffer'length)
    {
      c = buffer[i];

      if (c < ' ')   // control chars or zero
        return null;

      if (c == '\"')   // quotes
      {
        in_quote = !in_quote;
        start = -1;   // start of word
      }

      if (!in_quote)
      {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        {
          if (start == -1)
            start = i;
        }
        else  // end of word
        {
          if (start != -1)
          {
            if (stricmp (buffer[start:i-start], token) == 0)        // found !
            {
              while (i < buffer'length && buffer[i] >= ' ' && buffer[i] != '\"')
                i++;

              if (i == buffer'length || buffer[i] != '\"')
                return null;

              i++;  // skip "

              len = 0;
              while (i+len < buffer'length && buffer[i+len] >= ' ' && buffer[i+len] != '\"')
                len++;

              return new string ' (buffer[i:len]);
            }
            start = -1;
          }
        }
      }

      i++;
    }

    return null;
  }

  //---------------------------------------------------------------------------

  int setup_part_list (string              filename,
                       HTTP_SERVER_DATA    http_data,
                       out POST_PART_INFO^ result)
  {
    int             fd, rc, size, i, limit, j, separator_size, offset, body_ofs;
    byte            buffer[POST_BUFFER_SIZE], separator[POST_SEPARATOR_MAX_SIZE];
    POST_PART_INFO^ first_part, last_part, new_part;

    result = null;

    first_part = null;
    last_part = null;

    fd = open (filename, READ);
    if (fd < 0)
    {
      trace ("error: setup_part_list() : cannot open '%s'\n", filename);
      return -1;
    }

    size = 0;    // nb of loaded bytes in buffer
    offset = 0;  // file offset corresponding to buffer start

    clear buffer;  // to avoid warning
    clear separator;
    separator_size = 0;

    for (;;)
    {
      rc = read (fd, out buffer[size:buffer'length-size]);
      if (rc < 0)
      {
        free_part_list (first_part);
        close (fd);
        trace ("error: setup_part_list() : cannot read '%s'\n", filename);
        return -1;
      }

      if (rc == 0)
        break;

      size += rc;

      if (offset == 0)   // first loop : compute and store separator
      {
        for (j=0; j<size; j++)
          if (buffer[j] == 0x0D)
            break;
        if (j < 10 || j > POST_SEPARATOR_MAX_SIZE)     // cannot be separator
        {
          free_part_list (first_part);
          close (fd);
          if (http_data.tracing)
            trace ("warning: post data has no separator\n");
          return -1;
        }

        separator[0:j] = buffer[0:j];
        separator_size = j;
      }


      // search in buffer[0..size[

      limit = size - separator_size;
      for (i=0; i<=limit; i++)
      {
        if (buffer[i] == separator[0] &&
            buffer[i+separator_size-1] == separator[separator_size-1] &&
            memcmp (buffer[i:separator_size], separator[0:separator_size]) == 0)
        {
          // part found at (offset + i + separator_size + 2)
          new_part = new POST_PART_INFO;

          new_part^.offset_part = (offset + i + separator_size + 2);

          if (first_part == null)
            first_part = new_part;
          else
            last_part^.next = new_part;

          last_part = new_part;
        }
      }

      if (size < POST_BUFFER_SIZE)   // nothing more to read
        break;

      // copy (separator_size - 1) bytes to head of buffer
      size = separator_size - 1;
      buffer[0:size] = buffer[POST_BUFFER_SIZE-size : size];

      offset += (POST_BUFFER_SIZE - size);
    }


    // now, examine each part in detail

    if (first_part == null || first_part == last_part)   // 0 or 1 separator
    {
      free_part_list (first_part);
      close (fd);
      if (http_data.tracing)
        trace ("warning: empty post data\n");
      return -1;
    }

    new_part = first_part;
    while (new_part != null && new_part^.next != null)
    {
      if (lseek (fd, new_part^.offset_part, SEEK_SET) < 0)
      {
        free_part_list (first_part);
        close (fd);
        trace ("error: setup_part_list() : lseek() failed\n");
        return -1;
      }

      size = new_part^.next^.offset_part - new_part^.offset_part;
      if (size > 8*1024)   // 8K limit
        size = 8*1024;

      rc = read (fd, out buffer[0:size]);
      if (rc != size)
      {
        free_part_list (first_part);
        close (fd);
        trace ("error: setup_part_list() : read() failed\n");
        return -1;
      }

      body_ofs = strstr (buffer[0:size], "\r\n\r\n");
      if (body_ofs == -1)
      {
        free_part_list (first_part);
        close (fd);
        if (http_data.tracing)
          trace ("warning: setup_part_list() : bad format\n");
        return -1;
      }

      new_part^.offset_data = new_part^.offset_part + (body_ofs + 4);

      if (http_data.tracing)
        trace ("PART (offset=%d) :\n", new_part^.offset_part);


      // parse "name" and "filename"

      new_part^.parameter_name = get_param_string (buffer[0:body_ofs], "name");
      new_part^.filename       = get_param_string (buffer[0:body_ofs], "filename");

      if (new_part^.parameter_name == null)
      {
        free_part_list (first_part);
        close (fd);
        trace ("warning: setup_part_list() : missing parameter 'name'\n");
        return -1;
      }

      if (http_data.tracing)
      {
        trace ("header:\n");
        trace_data_block (buffer[0:body_ofs]);
        trace ("\n");
      }

      new_part = new_part^.next;
    }

    // check that last part is closed (two dashes)

    if (lseek (fd, new_part^.offset_part-2, SEEK_SET) < 0)
    {
      free_part_list (first_part);
      close (fd);
      trace ("error: setup_part_list() : lseek() failed\n");
      return -1;
    }

    size = 2;

    rc = read (fd, out buffer[0:size]);
    if (rc != size)
    {
      free_part_list (first_part);
      close (fd);
      trace ("error: setup_part_list() : read() failed\n");
      return -1;
    }

    if (buffer[0] != (byte)'-' || buffer[1] != (byte)'-')
    {
      free_part_list (first_part);
      close (fd);
      if (http_data.tracing)
        trace ("warning : missing double minus\n");
      return -1;
    }

    if (close(fd) < 0)
    {
      free_part_list (first_part);
      trace ("error: setup_part_list() : cannot close '%s'\n", filename);
      return -1;
    }


    // fill data size

    new_part = first_part;
    while (new_part != null && new_part^.next != null)
    {
      new_part^.size_data = new_part^.next^.offset_part - separator_size - 2
                          - new_part^.offset_data - 2;
      if (new_part^.size_data < 0)
      {
        free_part_list (first_part);
        if (http_data.tracing)
          trace ("error: setup_part_list() : negative body size\n");
        return -1;
      }

      new_part = new_part^.next;
    }

    result = first_part;
    return 0;
  }

  //---------------------------------------------------------------------------

  // used to retrieve query parameter and value  (ex: http://host/?parameter1=value1&parameter2=value2
  // returns 0 if found, -1 if not found

  public int get_query_string_pair (    ARG_INFO args,          // http arguments
                                        string   parameter_name,
                                    out string   value,
                                        string   default_value = "")
  {
    ref char[URL_PARAMETER_LENGTH] querystr = args.url->parameter;
    int i, len;
    char parameter[URL_PARAMETER_LENGTH];
    int  parameter_length;
    char the_value[URL_PARAMETER_LENGTH];
    int  the_value_length;

    strcpy (out value, default_value);

    if (querystr'length == 0 || querystr[0] != '?')
      return -1;

    i = 0;
    len = strchr (querystr, '#');
    if (len < 0)
      len = strlen(querystr);

    while (i < len)
    {
      i++;   // skip separator ? or &

      clear parameter;
      parameter_length = 0;
      while (i < len && querystr[i] != '=' && querystr[i] != '&')
      {
        char c = querystr[i];
        int  val = 0;

        i++;

        if (c == '+')
          c = ' ';
        
        if (c == '%' && i+1 < len && sscanf (querystr[i:2], "%x", out val) == 0)
        {
          c = (char)val;
          i += 2;
        }

        parameter[parameter_length++] = c;
      }

      clear the_value;
      the_value_length = 0;
      if (i < len && querystr[i] == '=')
      {
        i++;   // skip separator =
        
        while (i < len && querystr[i] != '&')
        {
          char c = querystr[i];
          int  val = 0;

          i++;

          if (c == '+')
            c = ' ';
          if (c == '%' && i+1 < len && sscanf (querystr[i:2], "%x", out val) == 0)
          {
            c = (char)val;
            i += 2;
          }

          the_value[the_value_length++] = c;
        }
      }

      if (strcmp (parameter, parameter_name) == 0)
      {
        strncpy (out value, the_value, value'length);
        return 0;
      }
      
      if (i == len || querystr[i] != '&')
        break;
    }

    return -1;  // not found
  }

  //---------------------------------------------------------------------------

  // used to retrieve http parameters (ex: "Referer", "Host", "Cookie")
  // all http parameters together are limited to 8K

  public void get_http_parameter (ARG_INFO     args,          // http arguments
                                  string       parameter_name,
                                  out string   value,
                                  string       default_value = "")
  {
    string^    fragment;
    int        i, j;
    ref char[] req = args.request[0:REQUEST_SIZE];

    clear value;

    if (value'size == 0)
      return;

    fragment = new string (strlen(parameter_name) + 3);

    sprintf (out fragment^, "\r\n%s:", parameter_name);

    i = stristr (req, fragment^);
    if (i == -1)
    {
      free fragment;
      strncpy (out value, default_value, value'length);
      return;
    }

    i += strlen(fragment^);

    free fragment;

    // skip white space
    while (req[i] == ' ' || req[i] == '\t')
      i++;

    j = strstr (req[i:req'length-i], "\r\n");
    if (j == -1)
    {
      strncpy (out value, default_value, value'length);
      return;
    }

    strncpy (out value, req[i:j], value'length);
  }

  //---------------------------------------------------------------------------

  // this function can be called when handling a is_virtual_page() call.
  // its purpose is to test if a physical file exists for the url.

  public bool get_http_physical_file_exists (ARG_INFO args)
  {
    char filename[260], filename2[260];
    int  fd;

    if (args.server_data == null || args.url == null)
      return false;

    if (convert_url_to_local_filename (*args.url, out filename) < 0)
      return false;

    if (strlen(args.server_data->html_directory) + strlen(filename) > filename2'length)
      return false;

    sprintf (out filename2, "%s%s", args.server_data->html_directory, filename[1:filename'length-1]);

    fd = open (filename2, READ);
    if (fd < 0)
      return false;

    close (fd);
    return true;
  }

  //---------------------------------------------------------------------------

  public void get_post_parameter (ARG_INFO   args,          // post arguments
                                  string     parameter_name,
                                  out string value,
                                  string     default_value = "")
  {
    clear value;

    if (value'length == 0)
      return;

    if (args.typ == DATA_IN_RAM)
    {
      ref string a = args.post_data^;

      int  length, i, j, code;
      char c;

      length = strlen (parameter_name);

      i = 0;

      for (;;)
      {
        if (i+length < a'length && a[i+length] == '=' &&
            stricmp (a[i:length], parameter_name) == 0)
          break;   // found

        while (i < a'length && a[i] != '&' && a[i] != nul)
          i++;

        if (i == a'length || a[i] == nul)
        {
          strncpy (out value, default_value, value'length);
          return;
        }

        i++;
      }

      i += (length+1);   // point to the parameter value

      j = 0;
      while (i < a'length && j < value'length)
      {
        c = a[i];

        if (c == '&' || c < ' ')
          break;

        i++;

        if (c == '+')
          c = ' ';
        
        code = 0;
        if (c == '%' && i+1 < a'length && sscanf (a[i:2], "%x", out code) == 0)
        {
          c = (char)code;
          i += 2;
        }

        value[j++] = c;
      }
    }
    else if (args.typ == DATA_IN_FILE)
    {
      POST_PART_INFO^ p;

      p = args.part;

      while (p != null && p^.parameter_name != null)
      {
        if (stricmp (parameter_name, p^.parameter_name^) == 0)  // found
        {
          int size, fd;

          size = value'length;
          if (size > p^.size_data)
            size = p^.size_data;

          fd = open (args.pfilename^, READ);
          if (fd < 0)
          {
            trace ("error: get_post_parameter() : open file failed\n");
            strncpy (out value, default_value, value'length);
            return;
          }

          if (lseek (fd, p^.offset_data, SEEK_SET) < 0)
          {
            close (fd);
            trace ("error: get_post_parameter() : lseek file failed\n");
            strncpy (out value, default_value, value'length);
            return;
          }

          if (read (fd, out value[0:size]) != size)
          {
            close (fd);
            trace ("error: get_post_parameter() : read file failed\n");
            strncpy (out value, default_value, value'length);
            return;
          }

          close (fd);

          return;     // done !
        }

        p = p^.next;
      }

      strncpy (out value, default_value, value'length);
    }
    else   // bad type
    {
      strncpy (out value, default_value, value'length);
    }
  }

  //---------------------------------------------------------------------------

  public int copy_post_parameter_to_file (ARG_INFO args,
                                          string   parameter_name,
                                          string   local_filename)
  {
    POST_PART_INFO^ p;

    if (args.typ != DATA_IN_FILE)
    {
      trace ("error: copy_post_parameter_to_file() : bad html option : multipart required !\n");
      return -1;
    }

    p = args.part;
    while (p != null && p^.parameter_name != null)
    {
      if (stricmp (parameter_name, p^.parameter_name^) == 0)    // found
      {
        int  fd1, fd2;
        int  rest, chunk_size;
        char buffer[8*1024];

        if (p^.size_data == 0)   // no data
          return -1;

         fd1 = open (args.pfilename^, READ);
         if (fd1 < 0)
         {
           trace ("error: copy_post_parameter_to_file() : open file failed\n");
           return -1;
         }

        if (lseek (fd1, p^.offset_data, SEEK_SET) < 0)
        {
          close (fd1);
          trace ("error: copy_post_parameter_to_file() : lseek file failed\n");
          return -1;
        }

        fd2 = create (local_filename, WRITE);
        if (fd2 < 0)
        {
          close (fd1);
          trace ("error: copy_post_parameter_to_file() : create file failed\n");
          return -1;
        }

        rest = p^.size_data;

        while (rest > 0)
        {
          if (rest > buffer'length)
            chunk_size = buffer'length;
          else
            chunk_size = rest;

          clear buffer;

          if (read (fd1, out buffer[0:chunk_size]) != chunk_size)
          {
            close (fd1);
            close (fd2);
            delete_file (local_filename);
            trace ("error: copy_post_parameter_to_file() : read file failed\n");
            return -1;
          }

          if (write (fd2, buffer[0:chunk_size]) != chunk_size)
          {
            close (fd1);
            close (fd2);
            delete_file (local_filename);
            trace ("error: copy_post_parameter_to_file() : write file failed\n");
            return -1;
          }

          rest -= chunk_size;
        }

        if (close (fd2) < 0)
        {
          close (fd1);
          delete_file (local_filename);
          trace ("error: copy_post_parameter_to_file() : close file failed\n");
          return -1;
        }

        close (fd1);

        return 0;     // done !
      }

      p = p^.next;
    }

    return -1;
  }

  //---------------------------------------------------------------------------

  public int get_post_parameter_filename (ARG_INFO   args,
                                          string     parameter_name,
                                          out string result_filename)
  {
    POST_PART_INFO^  p;

    clear result_filename;

    if (result_filename'size == 0)
    {
      trace ("error: get_post_parameter_filename() : zero buffer size\n");
      return -1;
    }

    if (args.typ != DATA_IN_FILE)
    {
      trace ("error: get_post_parameter_filename() : bad html option : multipart required !\n");
      return -1;
    }

    p = args.part;
    while (p != null && p^.parameter_name != null)
    {
      if (stricmp (parameter_name, p^.parameter_name^) == 0)    // found
      {
        if (p^.size_data == 0)    // no data
          return -1;

        if (p^.filename == null)  // no filename
          return -1;

        strncpy (out result_filename, p^.filename^, result_filename'length);
        return 0;
      }

      p = p^.next;
    }

    trace ("error: get_post_parameter_filename() : field not found\n");
    return -1;
  }

  //---------------------------------------------------------------------------

#end unsafe

  //---------------------------------------------------------------------------

  // assertion: 'in_length' is a multiple of 4.
  // out buffer should have the same size as the in buffer.

  void decode_base64 (string     in,
                      out string outb,
                      out int    out_length)
  {
    int i, j;

    clear outb;

    j = 0;

    for (i=0; i<in'length; i+=4)
    {
      int  count, k;
      uint w;
      char c;

      // convert 4 input bytes into 3 output bytes

      count = 3;
      w     = 0;

      for (k=0; k<4; k++)
      {
        c = in[i+k];
        if (c >= 'A' && c <= 'Z')
          c -= (int)'A';
        else if (c >= 'a' && c <= 'z')
          c -= ((int)'a' - 26);
        else if (c >= '0' && c <= '9')
          c -= ((int)'0' - 52);
        else if (c == '+')
          c = (char)62;
        else if (c == '/')
          c = (char)63;
        else if (c == '=')
        {
          c = nul;
          count--;
        }
        else    // illegal character
        {
          c = nul;
          count--;
        }

        w = (w << 6) + (uint)c;
      }

      while (count > 0)
      {
        outb[j++] = (char)(w >> 16);
        w <<= 8;
        count--;
      }
    }

    out_length = j;
  }

  //---------------------------------------------------------------------------

  package DATA

    struct MIME_COUPLE
    {
      string extension;
      string mime;
    }

    const MIME_COUPLE conv_mime[] = {
   {"APK",     "application/vnd.android.package-archive"},
   {"ASF",     "video/x-ms-asf"},
   {"ASX",     "video/x-ms-asf"},
   {"AU",      "audio/basic"},
   {"AVI",     "video/x-msvideo"},
   {"BMP",     "image/bmp"},
   {"CSS",     "text/css"},
   {"CSV",     "application/vnd.ms-excel"},
   {"DIF",     "video/x-dv"},
   {"DOC",     "application/msword"},
   {"DV",      "video/x-dv"},
   {"EXE",     "application/binary"},
   {"GIF",     "image/gif"},
   {"GZ",      "application/x-zip"},
   {"ICO",     "image/x-icon"},
   {"JPE",     "image/jpeg"},
   {"JPEG",    "image/jpeg"},
   {"JFIF",    "image/jpeg"},
   {"JIF",     "image/jpeg"},
   {"JPG",     "image/jpeg"},
   {"MP3",     "audio/wav"},
   {"MP4",     "video/mp4"},
   {"MPE",     "video/mpeg"},
   {"MPEG",    "video/mpeg"},
   {"MPG",     "video/mpeg"},
   {"HTM",     "text/html"},
   {"HTML",    "text/html"},
   {"MID",     "audio/mid"},
   {"MOV",     "video/quicktime"},
   {"PDF",     "application/pdf"},
   {"PNG",     "image/png"},
   {"PPS",     "application/vnd.ms-powerpoint"},
   {"PPT",     "application/vnd.ms-powerpoint"},
   {"QT",      "video/quicktime"},
   {"RA",      "audio/x-pn-realaudio"},
   {"RAM",     "audio/x-pn-realaudio"},
   {"RM",      "audio/x-pn-realaudio"},
   {"RV",      "video/vnd.rn-realvideo"},
   {"SND",     "audio/basic"},
   {"SWF",     "application/x-shockwave-flash"},
   {"TIF",     "image/tiff"},
   {"TIFF",    "image/tiff"},
   {"TXT",     "text/plain"},
   {"WAV",     "audio/wav"},
   {"WMA",     "audio/x-ms-wma"},
   {"WMV",     "video/x-ms-wmv"},
   {"XLS",     "application/vnd.ms-excel"},
   {"Z",       "application/x-zip"},
   {"ZIP",     "application/x-zip"}};

    //---------------------------------------------------------------------------

    const int VARIABLE_SIZE      = 64;      // max size of variable name
    const int VALUE_SIZE         = 1024;    // max size of variable value

    const uint READ_TIMEOUT       = 10;      // seconds
    const uint WRITE_TIMEOUT      = 10;      // seconds
    const uint DATA_READ_TIMEOUT  = (20*60); // nb seconds to read 1MB with delay
    const uint DATA_WRITE_TIMEOUT = (5*60);  // nb seconds to write 64K with delay

  end DATA;

  //---------------------------------------------------------------------------

  // handle one http call (tcp/ip connection must be open)

  public void handle_http_call (ref HANDLER_DATA      p,  // from tcp/ip server
                                HTTP_SERVER_DATA      h,  // http server config
                                ref HTTP_INITIAL_DATA i)  // in case some bytes already read
  {
    int     tcp_rc, fd, rc;
    uint    request_index, before;
    char    request[REQUEST_SIZE+1];
    char    protocol[32], request_url[512], version_http[16];
    TIMER   timeout;
    URL     url, base;

    clear request;

    for (;;)      // receive a request
    {
      request_index = 0;

      if (i.initial_received_data_size != 0)
      {
        assert (i.initial_received_data_size <= 8);
        request[0:8]'byte = i.initial_received_data;
        request_index = i.initial_received_data_size;
        i.initial_received_data_size = 0;
      }

      set_timer (out timeout, READ_TIMEOUT);

      for (;;)
      {
        if (p.stop^)
          return;

#if 0
#begin unsafe
        if (request_index == 8 && ((uint*)&buffer)[1] == REQUEST_NTIER)
        {
          treat_ntier (p, request[0:8]);   // other protocol
          return;
        }
#end unsafe
#endif

        before = request_index;

        tcp_rc = TCP_receive (ref p.tcp_client,
                              ref request[0:request_index < 8 ? 8 : REQUEST_SIZE],
                              ref request_index);

        if (request_index > before)               // some bytes received
          set_timer (out timeout, READ_TIMEOUT);  // re-arm timer


        // check for header completion

        if (strstr (request[0:request_index], "\r\n\r\n") >= 0)    // header complete
        {
          request[request_index] = nul;   // set sentinel
          break;
        }


        // check if input buffer is full

        if (request_index == request'size-1)
        {
          if (h.tracing)
          {
            trace ("handler %d : received request (%u bytes) :\n", p.nr, request_index);
            trace_data_block (request);
            trace ("handler %d : input buffer is full but header is incomplete - abort\n", p.nr);
          }
          return;
        }


        // check for any network problem

        if (tcp_rc != 0)
        {
          if (h.tracing)
          {
            if (request_index == 0)   // quiet broken connection
              trace ("handler %d : connection closed by client.\n", p.nr);
            else
            {
              trace ("handler %d : received request (%u bytes) :\n", p.nr, request_index);
              trace_data_block (request);
              trace ("handler %d : TCP_receive() returned %d\n", p.nr, tcp_rc);
            }
          }

          return;
        }


        // timeout

        if (timer_elapsed (timeout))
        {
          if (h.tracing)
            trace ("handler %d : timeout after %u seconds inactivity\n", p.nr, READ_TIMEOUT);
          return;
        }

        TCP_wait (ref p.tcp_client, nsecs => 1);
      }


      if (h.tracing)
      {
        trace ("handler %d : received request (%u bytes) :\n", p.nr, request_index);
        trace_data_block (request);
      }


      // analyze request

      if (sscanf (request, "%32s %512s %16s \r", out protocol, out request_url, out version_http) < 0)
      {
        if (h.tracing)
          trace ("handler %d : unrecognized http request\n", p.nr);
        return;
      }

      if (stricmp (protocol, "GET") != 0 && stricmp (protocol, "POST") != 0)
      {
        if (h.tracing)
          trace ("handler %d : unrecognized http command '%s'\n", p.nr, protocol);
        return;
      }

      rc = make_absolute_url ("http://H", out base);
      if (rc < 0)
      {
        if (h.tracing)
          trace ("handler %d : make_absolute_url() failed\n", p.nr);
        return;
      }
      if (base.port == 0)
        base.port = 80;

      rc = make_relative_url (request_url, base, out url);
      if (rc < 0)
      {
        if (h.tracing)
          trace ("handler %d : bad url '%s'\n", p.nr, request_url);
        return;
      }

      if (strcmp (url.filename, "/") == 0)    // home page
        strcpy (out url.filename, h.start_url);


      if (h.handle_login != null)
      {
        int  auth;
        char mode[10], base64[120], profile[120], realm[64];
        int  profile_length, ofs;

        clear profile;
        profile_length = 0;
        auth = stristr (request, "\r\nAuthorization: ");
        if (auth != -1)
        {
          if (sscanf (request[auth+17:(int)request_index-auth-17], "%10s %120s", out mode, out base64) >= 0 &&
              stricmp (mode, "Basic") == 0)
          {
            decode_base64 (base64[0:strlen(base64)], out profile, out profile_length);
          }
        }

        ofs = strchr (profile[0:profile_length], ':');

        clear realm;

        if (h.handle_login (p.nr, p.tcp_client, url.filename,
                            userid => (ofs == -1 ? "" : profile[0:ofs]),
                            password => (ofs == -1 ? "" : profile[ofs+1:profile_length-(ofs+1)]),
                            out realm) == false)
        {
          char reply[2048];
          int  len;

          len = sprintf (out reply,
                          "HTTP/1.1 401 LOGIN\r\n"
                          + "WWW-Authenticate: Basic realm=\"%s\"\r\n"
                          + "Connection: close\r\n"
                          + "\r\n"
                          + "<HTML><HEAD><TITLE>LOGIN</TITLE></HEAD>\r\n"
                          + "<BODY><H1><CENTER>Please login to %s</H1></BODY></HTML>\r\n",
                   realm, realm);

          if (h.tracing)
          {
            trace ("handler %d : send reply :\n", p.nr);
            trace_data_block (reply);
          }

          if (h.control_send_flux != null)
            h.control_send_flux (len);

          if (write_tcpip_block (ref p, reply[0:len], WRITE_TIMEOUT) < 0)
          {
            if (h.tracing)
              trace ("handler %d : error sending tcp/ip block\n", p.nr);
          }

          return;  // do not keep the connection open for further requests
        }
      }


      if (stricmp (protocol, "GET") == 0)     // protocol GET
      {
        char      filename[260], filename2[260];
        long      start_seek_offset;
        ARG_INFO  arg_info;

#begin unsafe
        clear arg_info;
        arg_info.request     = &request;
        arg_info.server_data = &h;
        arg_info.url         = &url;
#end unsafe

        /* check if it's a virtual page */

        if (h.handle_virtual_page != null && h.is_virtual_page != null &&
#begin unsafe
            h.is_virtual_page (p.nr, p.tcp_client, url.filename, arg_info))
#end unsafe
        {
          char extension[16], reply[1024+4096], type[32], headers[4096];
          int  k;

          compute_extension (url.filename, out extension);

          for (k=0; k<conv_mime'length; k++)
            if (stricmp (extension, conv_mime[k].extension) == 0)
              break;

          if (k < conv_mime'length)
            sprintf (out type, "%.31s", conv_mime[k].mime);
          else
            strcpy (out type, "text/html");

          sprintf (out reply, "HTTP/1.1 200 OK\r\n"
                              + "Content-Type: %s\r\n"
                              + "Connection: close\r\n",
                   type);

          if (h.add_reply_headers != null)
          {
#begin unsafe
            h.add_reply_headers (p.nr, p.tcp_client, url.filename, arg_info, out headers);
#end unsafe
            strcat (ref reply, headers);
          }
          strcat (ref reply, "\r\n");

          if (h.tracing)
          {
            trace ("handler %d : send reply :\n", p.nr);
            trace_data_block (reply);
          }

          if (h.control_send_flux != null)
            h.control_send_flux (strlen(reply));

          if (write_tcpip_block (ref p, reply[0:strlen(reply)], WRITE_TIMEOUT) < 0)
            return;

#begin unsafe
          h.handle_virtual_page (p.nr, ref p.tcp_client, url.filename, arg_info);
#end unsafe

          return;     // do not keep the connection open for further requests
        }


        // it's a real page on disk


        // check for a start seek offset

        {
          int start_range, r;

          start_seek_offset = 0L;

          start_range = stristr (request, "\r\nRange:");
          if (start_range != -1)
          {
            r = stristr (request[start_range:(int)request_index-start_range], "bytes=");
            if (r != -1)
            {
              start_range += (r + 6);
              sscanf (request[start_range:(int)request_index-start_range], "%d", out start_seek_offset);
            }
          }
        }


        rc = convert_url_to_local_filename (url, out filename);
        if (rc < 0)
        {
          if (h.tracing)
            trace ("handler %d : cannot convert url '%s' to filename\n", p.nr, url.filename);
          return;
        }

        if (strlen(h.html_directory) + strlen(filename) > filename2'length)
        {
          if (h.tracing)
            trace ("handler %d : url '%s' too long\n", p.nr, url.filename);
          return;
        }

        sprintf (out filename2, "%s%s", h.html_directory, filename[1:filename'length-1]);


        fd = open (filename2, READ);
        if (fd < 0)
        {
          const string msg = "HTTP/1.1 404 NOT FOUND\r\n\r\n"
                             + "<HTML><HEAD><TITLE>404 NOT FOUND</TITLE></HEAD>\r\n"
                             + "<BODY><H1>404 PAGE NOT FOUND</H1></BODY></HTML>\r\n";
          if (h.tracing)
          {
            trace ("handler %d : cannot open '%s'\n", p.nr, filename2);
            trace ("handler %d : send reply :\n", p.nr);
            trace_data_block (msg);
          }

          if (h.control_send_flux != null)
            h.control_send_flux (strlen(msg));

          if (write_tcpip_block (ref p, msg[0:strlen(msg)], WRITE_TIMEOUT) < 0)
          {
            if (h.tracing)
              trace ("handler %d : error sending tcp/ip block\n", p.nr);
          }

          return;  // do not keep the connection open for further requests
        }


        // send the file as reply (handle 'fd' is open)

        {
          char extension[16], str[1024], reply[64*1024], type[32], headers[4096];
          long size;
          int  k;
          bool contains_variables;

          size = filesize (fd);

          contains_variables = (h.must_translate_variables != null &&
                                h.translate_variable != null &&
#begin unsafe
                                h.must_translate_variables (p.nr, p.tcp_client, url.filename, arg_info));
#end unsafe

          if (contains_variables || start_seek_offset < 0L || start_seek_offset > size)
            start_seek_offset = 0L;   // no partial content allowed

          if (start_seek_offset == 0L)
            sprintf (out reply, "HTTP/1.1 200 OK\r\n");
          else
            sprintf (out reply, "HTTP/1.1 206 Partial content\r\n");


          compute_extension (filename, out extension);

          for (k=0; k<conv_mime'length; k++)
            if (stricmp (extension, conv_mime[k].extension) == 0)
              break;

          if (k < conv_mime'length)
            sprintf (out type, "%.31s", conv_mime[k].mime);
          else
            strcpy (out type, "text/html");

          sprintf (out str, "Content-Type: %s\r\n", type);
          strcat (ref reply, str);


          if (!contains_variables)
          {
            sprintf (out str, "Content-Length: %d\r\n", size - start_seek_offset);
          }
          else     /* page with variables has undefined size */
          {
            strcpy (out str, "Connection: close\r\n");
          }
          strcat (ref reply, str);


          if (start_seek_offset > 0)
          {
            sprintf (out str, "Content-Range: bytes %d-%d/%d\r\n", start_seek_offset, size-1, size);
            strcat (ref reply, str);
          }

          if (h.add_reply_headers != null)
          {
#begin unsafe
            h.add_reply_headers (p.nr, p.tcp_client, url.filename, arg_info, out headers);
#end unsafe
            strcat (ref reply, headers);
          }
          strcat (ref reply, "\r\n");

          if (h.tracing)
          {
            trace ("handler %d : send reply :\n", p.nr);
            trace_data_block (reply);
          }

          if (h.control_send_flux != null)
            h.control_send_flux (strlen(reply));

          if (write_tcpip_block (ref p, reply[0:strlen(reply)], WRITE_TIMEOUT) < 0)
          {
            close (fd);
            if (h.tracing)
              trace ("handler %d : error sending tcp/ip block\n", p.nr);
            return;
          }


          // send the file content

          if (h.tracing)
            trace ("handler %d : send file '%s'\n", p.nr, filename2);

          if (!contains_variables)
          {
            lseek (fd, start_seek_offset, SEEK_SET);

            for (;;)
            {
              rc = reply'length;   // 64K

              if (SIMULATE_SLOW_CONNECTION)
                rc = 4096;            // 4K

              if (h.control_send_flux != null)
                rc = 1024;          // 1K

              rc = read (fd, out reply[0:rc]);
              if (rc < 0)
              {
                close (fd);
                if (h.tracing)
                  trace ("handler %d : reading '%s' failed\n", p.nr, filename2);
                return;
              }

              if (rc == 0)
                break;

              if (h.control_send_flux != null)
                h.control_send_flux (rc);

              if (write_tcpip_block (ref p, reply[0:rc], DATA_WRITE_TIMEOUT) < 0)
              {
                close (fd);
                if (h.tracing)
                  trace ("handler %d : error sending tcp/ip block\n", p.nr);
                return;
              }

              if (h.tracing)
                trace ("handler %d : block sent\n", p.nr);

              if (p.stop^)
              {
                close (fd);
                trace ("handler %d : abort data sending due to service stop\n", p.nr);
                return;
              }

              if (SIMULATE_SLOW_CONNECTION)
                sleep (1);
            }

            if (h.tracing)
              trace ("handler %d : file sent : ok\n", p.nr);
          }
          else   // replace all $xxx$ variables
          {
            int  index, limit, x, y, x0, value_size;
            char variable[VARIABLE_SIZE], value[VALUE_SIZE];
            bool eof;

            clear value;

            index = 0;
            eof   = false;

            for (;;)
            {
              // fill up entire reply buffer
              rc = read (fd, out reply[index:reply'length-index]);
              if (rc < 0)
              {
                close (fd);
                if (h.tracing)
                  trace ("handler %d : reading '%s' failed\n", p.nr, filename2);
                return;
              }

              if (p.stop^)
              {
                close (fd);
                trace ("handler %d : abort data sending due to service stop\n", p.nr);
                return;
              }

              if (rc < reply'length-index)
                eof = true;

              index += rc;

              if (index == 0)   // no more data to convert
                break;


              // search for '$' in range [0 .. limit[

              if (eof)
                limit = index;
              else
                limit = index - (1+VARIABLE_SIZE);

              x0 = 0;   // start of chunk to write
              x  = 0;   // scan from position x

              while (x < limit)
              {
                if (reply[x] != '$')
                {
                  x++;
                  continue;
                }

                x++;   // skip '$'

                // check for variable name

                y = x;
                while (y < index && y - x < VARIABLE_SIZE && reply[y] != '$')
                  y++;

                if (y >= index || y - x == VARIABLE_SIZE)   // not found
                  continue;

                clear variable;
                variable[0:y-x] = reply[x:y-x];

                if (h.translate_variable != null)
                {
#begin unsafe
                  h.translate_variable (p.nr, p.tcp_client, url.filename, arg_info, variable, out value);
#end unsafe
                  value_size = strlen(value);
                }
                else
                {
                  value_size = 0;
                }


                // send chunk before initial '$'

                if ((x-1)-x0 > 0)
                {
                  uint size_to_write, offset, chunk_size;

                  size_to_write = (uint)((x-1)-x0);

                  for (offset=0; offset<size_to_write; offset+=1024)
                  {
                    chunk_size = size_to_write - offset;
                    if (chunk_size > 1024)
                      chunk_size = 1024;

                    if (h.control_send_flux != null)
                      h.control_send_flux ((int)chunk_size);

                    if (write_tcpip_block (ref p, reply[x0+(int)offset:chunk_size], DATA_WRITE_TIMEOUT) < 0)
                    {
                      close (fd);
                      if (h.tracing)
                        trace ("handler %d : error sending tcp/ip block\n", p.nr);
                      return;
                    }
                  }
                }


                // send variable value

                if (value_size > 0)
                {
                  if (h.control_send_flux != null)
                    h.control_send_flux (value_size);

                  if (write_tcpip_block (ref p, value[0:value_size], DATA_WRITE_TIMEOUT) < 0)
                  {
                    close (fd);
                    if (h.tracing)
                      trace ("handler %d : error sending tcp/ip block\n", p.nr);
                    return;
                  }
                }


                // continue after second '$'

                x = y + 1;
                x0 = x;
              }


              // send chunk [x0 .. x[

              if (x-x0 > 0)
              {
                uint size_to_write, offset, chunk_size;

                size_to_write = (uint)(x-x0);

                for (offset=0; offset<size_to_write; offset+=1024)
                {
                  chunk_size = size_to_write - offset;
                  if (chunk_size > 1024)
                    chunk_size = 1024;

                  if (h.control_send_flux != null)
                    h.control_send_flux ((int)chunk_size);

                  if (write_tcpip_block (ref p, reply[x0+(int)offset:chunk_size], DATA_WRITE_TIMEOUT) < 0)
                  {
                    close (fd);
                    if (h.tracing)
                      trace ("handler %d : error sending tcp/ip block\n", p.nr);
                    return;
                  }
                }
              }

              // move piece [x .. index[ back to begin of buffer

              reply[0:index-x] = reply[x:index-x];
              index -= x;
            }

            if (h.tracing)
              trace ("handler %d : file with variables sent : ok\n", p.nr);

            if (close (fd) < 0)
            {
              if (h.tracing)
                trace ("handler %d : error closing file '%s'\n", p.nr, filename2);
            }

            return;  // do not keep the connection open for further requests
          }
        }

        // file with variables sent : ok

        if (close (fd) < 0)
        {
          if (h.tracing)
            trace ("handler %d : error closing file '%s'\n", p.nr, filename2);
        }
      }
      else      // protocol POST
      {
        int       iContentLength, iContentType, ibody;
        uint      content_length, fixed_size, extra_size;
        bool      multipart_form;
        ARG_INFO  arg_info;
        char      reply[1024+4096], headers[4096];
        char[]^   body_buffer;

        if (h.handle_post_request == null)
          return;


        /* read "Content-Type" : "application/x-www-form-urlencoded" */
        /*                    or "multipart/form-data"               */

        multipart_form = false;
        iContentType = stristr (request, "\r\nContent-Type:");
        if (iContentType != -1)
        {
          iContentType += 15;
          while (iContentType < (int)request_index && request[iContentType] == ' ')
            iContentType++;
          if (strnicmp (request[iContentType:(int)request_index-iContentType], "multipart", 9) == 0)
            multipart_form = true;
        }


        /* read "Content-Length" */

        iContentLength = stristr (request, "\r\nContent-Length:");
        if (iContentLength == -1)
        {
          if (h.tracing)
            trace ("handler %d : error: attribute Content-Length was not found in request\n", p.nr);
          return;
        }
        content_length = 0;
        sscanf (request[iContentLength+17:(int)request_index-(iContentLength+17)], " %u", out content_length);


        /* get pointer to start of body */

        ibody = strstr (request, "\r\n\r\n");
        if (ibody == -1)   // should never occur as header was complete
          return;
        ibody += 4;        // points to start of body


        /* fixed_size = body that is already in-memory */

        fixed_size = request_index - (uint)ibody;
        if (fixed_size > content_length)  // can happen if additional CRLF
          fixed_size = content_length;


        // extra_size = body data that must still be received from network

        extra_size = content_length - fixed_size;


        /* policy : */
        /* "application/x-www-form-urlencoded" are treated in RAM (fast) */
        /*  with a limit of 32 Kbytes; */
        /* "multipart/form-data" are treated on file (slower) */
        /*  with a limit of 1 Gigabyte. */

        if (multipart_form)
        {
          const uint UPLOAD_BUF_SIZE = 8*1024;
          string^    pfilename;
          int        fd2;

          if (content_length > 1024*1024*1024)
          {
            if (h.tracing)
              trace ("handler %d : error: post data (%u MB) exceeds 1GB\n", p.nr, content_length / 1024 / 1024);
            return;
          }

          pfilename = new string (32);

          sprintf (out pfilename^, "%d.post.temp", p.nr);
          fd2 = create (pfilename^, WRITE);
          if (fd2 < 0)
          {
            if (h.tracing)
              trace ("handler %d : error: cannot create temp file\n", p.nr);
            free pfilename;
            return;
          }

          if (write (fd2, request[ibody:fixed_size]) != (int)fixed_size)
          {
            close (fd2);
            delete_file (pfilename^);
            free pfilename;
            trace ("handler %d : error: cannot write to temp file\n", p.nr);
            return;
          }

          if (extra_size > 0)
          {
            uint    rest, chunk_size;
            byte[]^ buf;

            if (h.tracing)
              trace ("handler %d : receive additional %u bytes\n", p.nr, extra_size);

            buf = new byte[UPLOAD_BUF_SIZE];

            rest = extra_size;
            while (rest > 0)
            {
              if (rest > UPLOAD_BUF_SIZE)
                chunk_size = UPLOAD_BUF_SIZE;
              else
                chunk_size = rest;

              if (read_tcpip_block (ref p, out buf^[0:chunk_size], DATA_READ_TIMEOUT) < 0)
              {
                free buf;
                close (fd2);
                delete_file (pfilename^);
                free pfilename;
                return;
              }

              if (p.stop^)
              {
                free buf;
                close (fd2);
                delete_file (pfilename^);
                free pfilename;
                trace ("handler %d : upload cancelled due to service stop\n", p.nr);
                return;
              }

              if (write (fd2, buf^[0:chunk_size]) != (int)chunk_size)
              {
                free buf;
                close (fd2);
                delete_file (pfilename^);
                free pfilename;
                trace ("handler %d : error: cannot write to temp file\n", p.nr);
                return;
              }

              rest -= chunk_size;
            }

            free buf;
          }

          if (close (fd2) < 0)
          {
            trace ("handler %d : error: cannot close temp file\n", p.nr);
            delete_file (pfilename^);
            free pfilename;
            return;
          }

#begin unsafe
          clear arg_info;
          arg_info.typ = DATA_IN_FILE;
          arg_info.pfilename = pfilename;
          arg_info.request = &request;
          arg_info.url = &url;
#end unsafe

          if (setup_part_list (pfilename^, h, out arg_info.part) < 0)
          {
            trace ("handler %d : scanning post data failed\n", p.nr);
            delete_file (pfilename^);
            free pfilename;
            return;
          }


          // send reply header

          {
            char extension[16], type[32];
            int  k;

            compute_extension (url.filename, out extension);

            for (k=0; k<conv_mime'length; k++)
              if (stricmp (extension, conv_mime[k].extension) == 0)
                break;

            if (k < conv_mime'length)
              sprintf (out type, "%.31s", conv_mime[k].mime);
            else
              strcpy (out type, "text/html");

            sprintf (out reply, "HTTP/1.1 200 OK\r\n"
                              + "Content-Type: %s\r\n"
                              + "Connection: close\r\n",
                     type);
          }

          if (h.add_reply_headers != null)
          {
#begin unsafe
            h.add_reply_headers (p.nr, p.tcp_client, url.filename, arg_info, out headers);
#end unsafe
            strcat (ref reply, headers);
          }
          strcat (ref reply, "\r\n");

          if (h.tracing)
          {
            trace ("handler %d : send reply :\n", p.nr);
            trace_data_block (reply);
          }

          if (h.control_send_flux != null)
            h.control_send_flux (strlen(reply));

          if (write_tcpip_block (ref p, reply[0:strlen(reply)], WRITE_TIMEOUT) < 0)
          {
            free_part_list (arg_info.part);
            delete_file (pfilename^);
            free pfilename;
            return;
          }


          // send reply body

#begin unsafe
          h.handle_post_request (p.nr, ref p.tcp_client, url.filename, arg_info);
#end unsafe

          free_part_list (arg_info.part);
          delete_file (pfilename^);
          free pfilename;

          return;   // force close connection because no size
                    // was specified in the reply header.
        }
        else   /* x-www-form-urlencoded */
        {
          if (content_length > 32*1024)
          {
            if (h.tracing)
              trace ("handler %d : error: post data (%uKB) exceeds 32KB\n", p.nr, content_length / 1024);
            return;
          }

          body_buffer = new char[content_length + 1];

          body_buffer^[0:fixed_size] = request[ibody:fixed_size];

          if (extra_size > 0)
          {
            if (h.tracing)
              trace ("handler %d : receive additional %u bytes\n", p.nr, extra_size);

            if (read_tcpip_block (ref p, out body_buffer^[fixed_size:extra_size], DATA_READ_TIMEOUT) < 0)
            {
              free body_buffer;
              return;
            }
          }

          body_buffer^[content_length] = nul;

          if (h.tracing)
          {
            trace ("handler %d : POST parameter block :\n", p.nr);
            trace_data_block (body_buffer^);
          }

          {
            char extension[16], type[32];
            int  k;

            compute_extension (url.filename, out extension);

            for (k=0; k<conv_mime'length; k++)
              if (stricmp (extension, conv_mime[k].extension) == 0)
                break;

            if (k < conv_mime'length)
              sprintf (out type, "%.31s", conv_mime[k].mime);
            else
              strcpy (out type, "text/html");

            sprintf (out reply, "HTTP/1.1 200 OK\r\n"
                              + "Content-Type: %s\r\n"
                              + "Connection: close\r\n",
                     type);
          }

#begin unsafe
          clear arg_info;
          arg_info.typ       = DATA_IN_RAM;
          arg_info.post_data = body_buffer;
          arg_info.request   = &request;
          arg_info.url       = &url;
#end unsafe

          if (h.add_reply_headers != null)
          {
#begin unsafe
            h.add_reply_headers (p.nr, p.tcp_client, url.filename, arg_info, out headers);
#end unsafe
            strcat (ref reply, headers);
          }
          strcat (ref reply, "\r\n");

          if (h.tracing)
          {
            trace ("handler %d : send reply :\n", p.nr);
            trace_data_block (reply);
          }

          if (h.control_send_flux != null)
            h.control_send_flux (strlen(reply));

          if (write_tcpip_block (ref p, reply[0:strlen(reply)], WRITE_TIMEOUT) < 0)
          {
            free body_buffer;
            return;
          }

#begin unsafe
          h.handle_post_request (p.nr, ref p.tcp_client, url.filename, arg_info);
#end unsafe

          free body_buffer;

          return;   // force close connection because no size
                    // was specified in the reply header.
        }
      }

      if (stricmp (version_http, "HTTP/1.0") == 0)  // HTTP 1.0
        return;     // do not keep the connection open for further requests
    }
  }

  //---------------------------------------------------------------------------

end HTTP_SERVER;

//---------------------------------------------------------------------------

package CONV_DATA

  struct HTML_COUPLE
  {
    char   source;
    string target;
  }

  const HTML_COUPLE[] couple =
    {{'<',  "&lt;" },
     {'>',  "&gt;" },
     {'&',  "&amp;"},
     {'\"', "&#34;"},
     {'\'', "&#39;"}};

end CONV_DATA;

//---------------------------------------------------------------------------

// replaces all <, >, ", & and ' characters by html items like "&lt;"
// to avoid confusion with the html syntax.
// note: the target string can become 5 times longer than the source.

public void expand_html (string source, out string target)
{
  char c;
  int  x, y, i, len;

  clear target;

  x = 0;
  y = 0;

  for (x=0; x<source'length; x++)
  {
    c = source[x];

    if (c == nul)
      return;

    for (i=0; i<couple'length; i++)
    {
      if (c == couple[i].source)
        break;
    }

    if (i < couple'length)   // found
    {
      len = couple[i].target'length;
      target[y:len] = couple[i].target;
      y += len;
    }
    else
    {
      target[y++] = c;
    }
  }
}

//---------------------------------------------------------------------------
