
// tcpip.c

use aes, strings, thread, tracing;

#if WINDOWS
  use win/windows;
#endif

#if ANDROID
  use android/bionic;
#endif

//-----------------------------------------------------------------------------------------
#begin unsafe
//-----------------------------------------------------------------------------------------

struct TCP_CRYPT_DATA
{
  bool crypted;
  byte key[16];
  uint send_offset; // offset within stream (bit 31=client send)
  uint recv_offset; // offset within stream (bit 31=server recv)
}

struct TCP_CLIENT
{
  SOCKET         s;       // socket handle

#if WINDOWS
  WSAEVENT       v;       // event handle
#endif

#if ANDROID
  int  evfd;   // event descriptor
  int  epfd;   // epoll descriptor
  bool bpollout_set;
  bool bsend_called;
#endif

  bool           is_ipv6;
  TCP_CRYPT_DATA crypt;
}

struct TCP_SERVER
{
  SOCKET   s[2];  // socket handle (first is IPV6, second is IPV4)

#if WINDOWS
  WSAEVENT v;     // event handle
#endif

#if ANDROID
  int evfd;   // event descriptor
  int epfd;   // epoll descriptor
#endif
}

struct UDP_ENDPOINT
{
  SOCKET   s[2];                // socket handle (0=ipv6, 1=ipv4)

#if WINDOWS
  WSAEVENT v;                   // event handle
#endif

#if ANDROID
  int  evfd;   // event descriptor
  int  epfd;   // epoll descriptor
  bool bpollout_set[2];
  bool bsend_called[2];
#endif

  bool         send_ipv6;
  sockaddr_in6 a6;
  sockaddr_in  a;
  bool         sending_buffer_full;   // sending is still possible
  UDP_LOCAL    local;   // info about local peer IP (useful in case of several interfaces)
}

//-----------------------------------------------------------------------------------------

#if WINDOWS
  volatile int  g_winsock_state = 0;    // 0 = closed, +1 = open, -1 = error
  volatile uint g_winsock_init = 0;
#endif

volatile int  g_order_recv_udp;

//-----------------------------------------------------------------------------------------

#if WINDOWS
  WSARecvmsg  g_WsaRecvMsg;
  WSASendMsg  g_WsaSendMsg;
#endif

//-----------------------------------------------------------------------------------------

#if WINDOWS
int init_socket_library ()
{
  WSADATA  wsaData;

  if (g_winsock_state > 0)    // +1 -> WSA already open
    return 0;

  if (InterlockedExchange (ref g_winsock_init, 1) == 0)  // first call
  {
    if (WSAStartup (0x0202, &wsaData) != 0)   // version 2.2
    {
      g_winsock_state = -1;  // WSA error
      return TCP_DRIVER_PROBLEM;
    }
    else
    {
      g_winsock_state = +1;  // WSA open
    }
  }
  else   // not first thread, but WSA is not yet open
  {
    while (g_winsock_state == 0)  // wait until WSA initialization done
      sleep 0;

    if (g_winsock_state < 0)
      return TCP_DRIVER_PROBLEM;
  }

  return 0;
}
#endif

//-----------------------------------------------------------------------------------------

int set_stream_socket_options (SOCKET s)
{
  // set 4 MB send and receive buffers.
  {
    uint size;

    size = 4*1024*1024;
    if (setsockopt (s, SOL_SOCKET, SO_SNDBUF, (byte *)&size, size'size) == -1)
#if WINDOWS
      return negative_last_windows_error();
#endif
#if ANDROID
      return negative_errno ();
#endif

    size = 4*1024*1024;
    if (setsockopt (s, SOL_SOCKET, SO_RCVBUF, (byte *)&size, size'size) == -1)
#if WINDOWS
      return negative_last_windows_error();
#endif
#if ANDROID
      return negative_errno ();
#endif
  }


  // set the SO_KEEPALIVE option to detect a broken connection
  // after 1 or 2 minutes of inactivity.

  {
    int flag = 1;
    if (setsockopt (s, SOL_SOCKET, SO_KEEPALIVE, (byte *)&flag, flag'size) == -1)
#if WINDOWS
      return negative_last_windows_error();
#endif
#if ANDROID
      return negative_errno ();
#endif
  }

  return 0;
}

//-----------------------------------------------------------------------------------------

int set_datagram_socket_options (SOCKET s,
                                 int    af)  // AF_INET or AF_INET6
{
  // set 4 MB send and receive buffers.
  {
    uint size;

    size = 4*1024*1024;
    if (setsockopt (s, SOL_SOCKET, SO_SNDBUF, (byte *)&size, size'size) == -1)
#if WINDOWS
      return negative_last_windows_error();
#endif
#if ANDROID
      return negative_errno ();
#endif

    size = 4*1024*1024;
    if (setsockopt (s, SOL_SOCKET, SO_RCVBUF, (byte *)&size, size'size) == -1)
#if WINDOWS
      return negative_last_windows_error();
#endif
#if ANDROID
      return negative_errno ();
#endif
  }

#if WINDOWS
  {
    int option = 1;  // enable return of packet information by g_WsaRecvMsg()
    if (af == (int)AF_INET)
      (void)setsockopt (s, IPPROTO_IP,   IP_PKTINFO,   (byte *)&option, option'size);
    else
      (void)setsockopt (s, IPPROTO_IPV6, IPV6_PKTINFO, (byte *)&option, option'size);
  }
#endif

#if ANDROID
  {
    int option = 1;  // Set delivery of the IP_PKTINFO/IPV6_PKTINFO control message on incoming datagrams
    if (af == (int)AF_INET)
      (void)setsockopt (s, IPPROTO_IP,   IP_PKTINFO,       (byte *)&option, option'size);
    else
      (void)setsockopt (s, IPPROTO_IPV6, IPV6_RECVPKTINFO, (byte *)&option, option'size);
  }
#endif

#if WINDOWS
  if (g_WsaRecvMsg == null)
  {
    DWORD count;
    (void)WSAIoctl (s, SIO_GET_EXTENSION_FUNCTION_POINTER,
                    (LPVOID)&WSAID_WSARECVMSG, WSAID_WSARECVMSG'size,
                    (LPVOID)&g_WsaRecvMsg, g_WsaRecvMsg'size, &count);
  }

  if (g_WsaSendMsg == null)
  {
    DWORD count;
    (void)WSAIoctl (s, SIO_GET_EXTENSION_FUNCTION_POINTER,
                    (LPVOID)&WSAID_WSASENDMSG, WSAID_WSASENDMSG'size,
                    (LPVOID)&g_WsaSendMsg, g_WsaSendMsg'size, &count);
  }
#endif

  return 0;
}

//-----------------------------------------------------------------------------------------

// returns a socket handle or a negative error

SOCKET create_socket (int  af,         // AF_INET     or AF_INET6
                      int  type,       // SOCK_STREAM or SOCK_DGRAM
                      int  protocol)   // IPPROTO_TCP or IPPROTO_UDP
{
  int    rc;
  SOCKET s;

#if WINDOWS
  rc = init_socket_library ();
  if (rc < 0)
    return rc;
#endif

#if WINDOWS
  s = socket (af, type, protocol);
#endif

#if ANDROID
  s = socket (af, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
#endif

  if (s == -1)
#if WINDOWS
      return negative_last_windows_error();
#endif
#if ANDROID
      return negative_errno ();
#endif

  if (type == SOCK_STREAM)
  {
    rc = set_stream_socket_options (s);
    if (rc < 0)
      return rc;
  }

  if (type == SOCK_DGRAM)
  {
    rc = set_datagram_socket_options (s, af);
    if (rc < 0)
      return rc;
  }

  return s;
}

//-----------------------------------------------------------------------------------------

public bool is_ipv4 (IP ip)
{
  return memcmp (ip[0:12], IPV4_PREFIX) == 0;
}

//-----------------------------------------------------------------------------------------

public bool is_ipv6 (IP ip)
{
  return memcmp (ip[0:12], IPV4_PREFIX) != 0;
}

//-----------------------------------------------------------------------------------------

// IPV4 beginning with 127, or IPV6 ::1

public bool is_loopback (IP ip)
{
  const byte[13] IPV4_LOOPBACK_PREFIX = {0,0,0,0,0,0,0,0,0,0,0xFF,0xFF, 127};
  const IP       IPV6_LOOPBACK = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
  return memcmp (ip[0:13], IPV4_LOOPBACK_PREFIX) == 0 || memcmp (ip, IPV6_LOOPBACK) == 0;
}

//-----------------------------------------------------------------------------------------

// private IP's are usable only on the local LAN, not on the internet.
// IPV4 10., 169.254., 172.16-31., 192.168., IPV6 fc00::/7, fe80::/10.

public bool is_private (IP ip)
{
  if (is_ipv6 (ip))
  {
    return ((ip[0] & 0xFE) == 0xFC) || (ip[0] == 0xFE && (ip[1] & 0xC0) == 0x80);
  }
  else
  {
    if (ip[12] == 10)
      return true;

    if (ip[12] == 169 && ip[13] == 254)
      return true;

    if (ip[12] == 172 && (ip[13] >= 16 && ip[13] <= 31))
      return true;

    if (ip[12] == 192 && ip[13] == 168)
      return true;

    return false;
  }
}

//-----------------------------------------------------------------------------------------

// port 0 is not allowed.

public int TCP_connect (out TCP_CLIENT tcp_client,
                            IP             ip,
                            int            port,      // port 0 is not allowed
                            uint           nsecs)     // timeout in msecs
{
  int      rc;
  SOCKET   s;
  int      msecs = (int)((nsecs <= (uint'max/1000)) ? (nsecs * 1000) : uint'max);

#if WINDOWS
  WSAEVENT v;
#endif

#if ANDROID
  int evfd;   // event descriptor
  int epfd;   // epoll descriptor
#endif

  bool     ipv6;
  uint2    hport;

  clear tcp_client;

  if (port == 0)
    return TCP_INVALID_PORT;

  ipv6 = is_ipv6 (ip);

  s = create_socket ((int)(ipv6 ? AF_INET6 : AF_INET), SOCK_STREAM, IPPROTO_TCP);
  if (s < 0)
    return (int)s;

  hport = (uint2)((port << 8) + (port >> 8));

#if WINDOWS
    v = WSACreateEvent();
    if (v == 0)
    {
      rc = negative_last_windows_error();
      closesocket (s);
      return rc;
    }

    // this will also set socket to non-blocking mode
    if (WSAEventSelect(s, v, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE) != 0)
    {
      rc = negative_last_windows_error();
      closesocket (s);
      WSACloseEvent (v);
      return rc;
    }
#endif

#if ANDROID
  {
    epoll_event event;

    evfd = bionic.eventfd (initval => 0);
    epfd = bionic.epoll_create1 ();

    assert evfd != -1;
    assert epfd != -1;

    clear event;
    event.events = (uint)POLLIN;
    event.data.g64 = 1;  // user event
    assert bionic.epoll_ctl (epfd, EPOLL_CTL_ADD, evfd, event) == 0;

    clear event;
    event.events = (EPOLLIN | EPOLLOUT | EPOLLRDHUP);   // EPOLLOUT will confirm that connect() was done
    event.data.g64 = 2;  // socket
    assert epoll_ctl (epfd, EPOLL_CTL_ADD, s, event) == 0;
  }
#endif  // ANDROID


  if (ipv6)
  {
    sockaddr_in6 a = {AF_INET6, hport, 0, ip, 0};
    rc = connect (s, (byte *)&a, a'size);
  }
  else
  {
    sockaddr_in a = {AF_INET, hport, ip[12:4], {all=>0}};
    rc = connect (s, (byte *)&a, a'size);
  }

  if (rc != 0)   // some error occured
  {
#if WINDOWS
    rc = negative_last_windows_error();
#endif
#if ANDROID
    rc = negative_errno ();
#endif


#if WINDOWS
    if (rc == -EWOULDBLOCK)   // not really an error
    {
      // waiting for connection
      rc = (int)WSAWaitForMultipleEvents (1, &v, 0, (uint)msecs, 0);
      if (rc == WSA_WAIT_TIMEOUT)
      {
        closesocket (s);
        WSACloseEvent (v);
        return TCP_CONNECTION_FAILED;
      }
    }
#endif

#if ANDROID
    if (rc == -EWOULDBLOCK || rc == -EAGAIN || rc == -EALREADY || rc == -EINPROGRESS)   // not really an error
    {
      // waiting for connection
      epoll_event event;
      rc = bionic.epoll_wait (epfd, &event, maxevents => 1, timeout => msecs);

//log ("epoll_wait rc = %d", rc);

      if (rc == 1)   // socket or user event
      {
//log ("event.events = 0x%x", event.events);
//log ("event.data.g64 = %d", event.data.g64);

        if (event.data.g64 == 1)  // user event
        {
          long value;
          assert bionic.read (evfd, (byte*)&value, 8) == 8;
        }
      }
      else if (rc == 0)   // timeout
      {
        close (s);
        close (evfd);
        close (epfd);
        return TCP_CONNECTION_FAILED;
      }
      else
      {
        return TCP_CONNECTION_FAILED;  // some error
      }
    }
#endif

    else     // the connection failed
    {
      switch (rc)
      {
        case -EADDRNOTAVAIL:
        case -EADDRINUSE:
        case -EINVAL:
        case -EDESTADDRREQ:
          rc = TCP_INVALID_IP_ADDRESS;
          break;

        case -ENETDOWN:
        case -ENETUNREACH:
        case -EHOSTDOWN:
        case -EHOSTUNREACH:
        case -ECONNREFUSED:
        case -ETIMEDOUT:
          rc = TCP_UNREACHABLE;
          break;

        default:   // keep original rc
          break;
      }

#if WINDOWS
      closesocket (s);
      WSACloseEvent (v);
#endif

#if ANDROID
      close (s);
      close (evfd);
      close (epfd);
#endif

      return rc;
    }
  }


  // try to send a first empty block

//log ("try send");

  if (send (s, (byte *)&s, len => 0, flags => 0) == -1)
  {
#if WINDOWS
    closesocket (s);
    WSACloseEvent (v);
#endif

#if ANDROID
    close (s);
    close (evfd);
    close (epfd);
#endif

    return TCP_NO_SERVER_LISTENING;
  }


  // remove EPOLLOUT
#if ANDROID
  {
    epoll_event event;
    clear event;
    event.events = (EPOLLIN | EPOLLRDHUP);
    event.data.g64 = 2;  // socket
    assert epoll_ctl (epfd, EPOLL_CTL_MOD, s, event) == 0;
  }
#endif


//log ("open");

  tcp_client = {s       => s,

#if WINDOWS
                v       => v,
#endif

#if ANDROID
                evfd         => evfd,
                epfd         => epfd,
                bpollout_set => false,
                bsend_called => false,
#endif

                is_ipv6 => ipv6,
                crypt   => {crypted     => false,
                            key         => {all => 0},
                            send_offset => 0x80000000,
                            recv_offset => 0}};
  return 0;
}

//-----------------------------------------------------------------------------------------

// waits until a network event occurs, for example data arrives
// or the connection is closing.
// returns 0 if client had an event, -1 if timeout.

public int TCP_wait (ref TCP_CLIENT tcp_client, uint nsecs)
{
  int rc;
  int msecs = (int)((nsecs <= (uint'max/1000)) ? (nsecs * 1000) : uint'max);

#if WINDOWS
  assert (tcp_client.v > 0);

  rc = (int)WSAWaitForMultipleEvents (1, &tcp_client.v, 0, (uint)msecs, 0);
  if (rc == 0)
  {
    WSAResetEvent (tcp_client.v);
    return 0;
  }

  return -1;
#endif

#if ANDROID
  epoll_event event;

//log ("");
//log ("tcp wait %d msec", msecs);

  if (tcp_client.bsend_called != tcp_client.bpollout_set)
  {
    tcp_client.bpollout_set = tcp_client.bsend_called;

/*
if (tcp_client.bsend_called)
  log ("adding EPOLLOUT");
else
  log ("removing POLLOUT");
*/

    clear event;
    event.events = (EPOLLIN | EPOLLRDHUP) | (EPOLLOUT * (uint)tcp_client.bsend_called);
    event.data.g64 = 2;  // socket
    assert epoll_ctl (tcp_client.epfd, EPOLL_CTL_MOD, tcp_client.s, event) == 0;
  }
  tcp_client.bsend_called = false;

  rc = bionic.epoll_wait (tcp_client.epfd, &event, maxevents => 1, timeout => msecs);

//log ("tcp wait epoll_wait rc = %d", rc);

  if (rc == 1)   // socket or user event
  {
//log ("tcp wait event.data.g64 = %d", event.data.g64);
//log ("tcp wait events = 0x%x", event.events);
//log (" ");

    if (event.data.g64 == 1)  // user event
    {
      long value;
      assert bionic.read (tcp_client.evfd, (byte*)&value, 8) == 8;
    }
    return 0;
  }
  else if (rc == 0)   // timeout
    return -1;
  else
    return -1;  // some error
#endif
}

//-----------------------------------------------------------------------------------------

public void TCP_wakeup (ref TCP_CLIENT tcp_client)
{
#if WINDOWS
  assert (tcp_client.v > 0);
  assert WSASetEvent (tcp_client.v) == 1;
#endif

#if ANDROID
  long value = 1;
  assert bionic.write (tcp_client.evfd, (byte*)&value, 8) == 8;
#endif
}

//-----------------------------------------------------------------------------------------

public int TCP_send (ref TCP_CLIENT tcp_client,
                         byte[]     buffer,
                     ref uint       buffer_index)
{
  int  rc;
  uint chunk_size;


  // check if tcp_client is valid

  assert (tcp_client.s > 0);


  // check if 'buffer_index' is valid (avoid sending 0 bytes)

  if (buffer_index >= buffer'size)
    return TCP_INVALID_BUFFER_INDEX;

#if ANDROID
  tcp_client.bsend_called = true;
#endif

  
  // try to send part of the block

  chunk_size = buffer'size - buffer_index;

  if (chunk_size > 5840)     // avoid error ENOBUFS
    chunk_size = 5840;

  rc = send (tcp_client.s, &buffer[buffer_index], chunk_size, 0);
  if (rc == -1)
  {
#if WINDOWS
    rc = negative_last_windows_error();
#endif
#if ANDROID
    rc = negative_errno ();
#endif

#if WINDOWS
    if (rc == -EWOULDBLOCK || rc == -ENOMEM)   // output buffers are full
#endif
#if ANDROID
    if (rc == -EWOULDBLOCK || rc == -EAGAIN)   // output buffers are full
#endif
    {
      return 0;
    }
    else       // some unexpected error : close connection
    {
      if (rc == -ENETRESET    ||    // broken connection
          rc == -ENETDOWN     || rc == -ENETUNREACH ||
          rc == -ECONNABORTED || rc == -ECONNRESET)
        return TCP_CONNECTION_BROKEN;
      else
        return rc;    // some implementation-dependant error code
    }
  }

  buffer_index += (uint)rc;
  tcp_client.crypt.send_offset += (uint)rc;

#if WINDOWS
  // make sure the following TCP_wait() works.
  if (WSASetEvent (tcp_client.v) == 0)
    return negative_last_windows_error();
#endif

  return 0;
}

//-----------------------------------------------------------------------------------------

public int TCP_receive (ref TCP_CLIENT tcp_client,
                        ref byte[]     buffer,
                        ref uint       buffer_index)
{
  int  rc;
  uint chunk_size;

  // check if tcp_client is valid

//log ("TCP_receive");


  assert (tcp_client.s > 0);


  // check if 'buffer_index' is valid
  // (avoid trying to receive 0 bytes
  // as it normally means the connection
  // was broken).

  if (buffer_index >= buffer'size)
    return TCP_INVALID_BUFFER_INDEX;


  // try to receive part of the block

  chunk_size = buffer'size - buffer_index;

  if (chunk_size > 5840)     // avoid error ENOBUFS
    chunk_size = 5840;

  rc = recv (tcp_client.s, &buffer[buffer_index], chunk_size, 0);

  if (rc == -1)   // an error occured
  {
#if WINDOWS
    rc = negative_last_windows_error();
#endif
#if ANDROID
    rc = negative_errno ();
#endif

//log ("TCP_receive rc = %d", rc);

#if WINDOWS
    if (rc == -EWOULDBLOCK || rc == -ENOMEM)   // no bytes received
#endif
#if ANDROID
    if (rc == -EWOULDBLOCK || rc == -EAGAIN)   // no bytes received
#endif
    {
      return 0;
    }
    else       // some unexpected error : close connection
    {
      if (rc == -ENETRESET    || // broken connection
          rc == -ENETDOWN     || rc == -ENETUNREACH ||
          rc == -ECONNABORTED || rc == -ECONNRESET)
        return TCP_CONNECTION_BROKEN;
      else
        return rc;     // some implementation-dependant error code
    }
  }

  if (rc == 0)     // remote computer closed the connection
  {
    return TCP_CONNECTION_BROKEN;
  }

  buffer_index += (uint)rc;
  tcp_client.crypt.recv_offset += (uint)rc;

  return 0;
}

//-----------------------------------------------------------------------------------------

public void TCP_hangup (ref TCP_CLIENT tcp_client)
{
  // check if tcp_client is valid
  assert (tcp_client.s > 0);

#if WINDOWS
  closesocket (tcp_client.s);
  WSACloseEvent (tcp_client.v);
#endif

#if ANDROID
  close (tcp_client.s);
  close (tcp_client.evfd);
  close (tcp_client.epfd);
#endif

  clear tcp_client;
}

//-----------------------------------------------------------------------------------------

int intern_query_address (TCP_CLIENT        tcp_client,
                          bool              local,      // true=local, false=remote
                          out PEER          info)
{
  uint size;
  int  rc;

  // check if tcp_client is valid
  assert (tcp_client.s > 0);

  clear info;


  // retrieve socket addr

  if (tcp_client.is_ipv6)
  {
    sockaddr_in6 a;

    size = a'size;

    if (local)
      rc = getsockname (tcp_client.s, (byte *)&a, &size);
    else
      rc = getpeername (tcp_client.s, (byte *)&a, &size);

    info = {ip => a.sin6_addr, port => (uint2)((a.sin6_port >> 8) + (a.sin6_port << 8))};
  }
  else
  {
    sockaddr_in a;

    size = a'size;

    if (local)
      rc = getsockname (tcp_client.s, (byte *)&a, &size);
    else
      rc = getpeername (tcp_client.s, (byte *)&a, &size);

    clear info;
    info.ip[0:12] = IPV4_PREFIX;
    info.ip[12:4] = a.sin_addr;
    info.port = (uint2)((a.sin_port >> 8) + (a.sin_port << 8));
  }

  if (rc == -1)
#if WINDOWS
    return negative_last_windows_error();
#endif
#if ANDROID
    return negative_errno ();
#endif

  return 0;
}

//-----------------------------------------------------------------------------------------

public int TCP_query_local_address (TCP_CLIENT tcp_client, out PEER info)
{
  return intern_query_address (tcp_client, true, out info);
}

//-----------------------------------------------------------------------------------------

public int TCP_query_remote_address (TCP_CLIENT tcp_client, out PEER info)
{
  return intern_query_address (tcp_client, false, out info);
}

//-----------------------------------------------------------------------------------------

public int TCP_no_delay (TCP_CLIENT tcp_client)
{
  // check if tcp_client is valid
  assert (tcp_client.s > 0);

  // set no_delay flag
  {
    int flag = 1;
    if (setsockopt (tcp_client.s, IPPROTO_TCP, TCP_NODELAY, (byte *)&flag, flag'size) == -1)
#if WINDOWS
      return negative_last_windows_error();
#endif
#if ANDROID
      return negative_errno ();
#endif
  }

  return 0;
}

//-----------------------------------------------------------------------------------------

// returns 0 if OK, or a negative error code

public int TCP_create_server (out TCP_SERVER tcp_server,
                                  uint2      port,             // port 0 is not allowed.
                                  IPV_MODE   mode = MODE_IPV6)
{
  uint2    hport;
  int      rc;
  SOCKET   s;

#if WINDOWS
  WSAEVENT v;
#endif

  clear tcp_server;

  if (port == 0)
    return TCP_INVALID_PORT;

  hport = (uint2)((port << 8) + (port >> 8));

#if WINDOWS
  v = WSACreateEvent();
  if (v == 0)
    return negative_last_windows_error();
#endif  // WINDOWS

  if (mode == MODE_IPV6 || mode == MODE_DUAL)
  {
    s = create_socket (AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (s > 0)
    {
#if WINDOWS
      if (WSAEventSelect (s, v, FD_ACCEPT | FD_CLOSE) != 0)
      {
        rc = negative_last_windows_error();
        closesocket (s);
        WSACloseEvent (v);
        return rc;
      }
#endif

      // bind to local port number
      {
        sockaddr_in6 a = {AF_INET6, hport, 0, {all=>0}, 0};

        if (bind (s, (byte *)&a, a'size) == -1)
        {
#if WINDOWS
          rc = negative_last_windows_error();
          closesocket (s);
          WSACloseEvent (v);
#endif
#if ANDROID
          rc = negative_errno ();
          close (s);
#endif

          if (rc == -EADDRINUSE)  // another server already listens on this port
            return TCP_PORT_IN_USE;
          else
            return rc;
        }
      }

      // make the socket 'passive'

      if (listen (s, backlog => 64) == -1)
      {
#if WINDOWS
        rc = negative_last_windows_error();
        closesocket (s);
        WSACloseEvent (v);
#endif
#if ANDROID
        rc = negative_errno ();
        close (s);
#endif

        return rc;
      }

      tcp_server.s[0] = s;
    }
  }


  if (mode == MODE_IPV4 || mode == MODE_DUAL)
  {
    s = create_socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s > 0)
    {
#if WINDOWS
      if (WSAEventSelect(s, v, FD_ACCEPT | FD_CLOSE) != 0)
      {
        rc = negative_last_windows_error();
        closesocket (s);
        WSACloseEvent (v);
        if (tcp_server.s[0] > 0)
          closesocket (tcp_server.s[0]);
        clear tcp_server;
        return rc;
      }
#endif

      // bind to local port number

      {
        sockaddr_in a = {AF_INET, hport, {all=>0}, {all=>0}};

        if (bind (s, (byte *)&a, a'size) == -1)
        {
#if WINDOWS
          rc = negative_last_windows_error();
          closesocket (s);
          WSACloseEvent (v);
          if (tcp_server.s[0] > 0)
            closesocket (tcp_server.s[0]);
#endif
#if ANDROID
          rc = negative_errno ();
          close (s);
          if (tcp_server.s[0] > 0)
            close (tcp_server.s[0]);
#endif

          clear tcp_server;
          if (rc == -EADDRINUSE)  // another server already listens on this port
            return TCP_PORT_IN_USE;
          else
            return rc;
        }
      }

      // make the socket 'passive'

      if (listen (s, backlog => 64) == -1)
      {
#if WINDOWS
        rc = negative_last_windows_error();
        closesocket (s);
        WSACloseEvent (v);
        if (tcp_server.s[0] > 0)
          closesocket (tcp_server.s[0]);
#endif
#if ANDROID
        rc = negative_errno ();
        close (s);
        if (tcp_server.s[0] > 0)
          close (tcp_server.s[0]);
#endif

        clear tcp_server;
        return rc;
      }

      tcp_server.s[1] = s;
    }
  }

  if (tcp_server.s[0] == 0 && tcp_server.s[1] == 0)
  {
#if WINDOWS
    rc = negative_last_windows_error();
    WSACloseEvent (v);
#endif
#if ANDROID
    rc = negative_errno ();
#endif

    return rc;
  }

#if WINDOWS
  tcp_server.v = v;
#endif

#if ANDROID
  {
    int evfd;   // event descriptor
    int epfd;   // epoll descriptor
    epoll_event event;

    evfd = bionic.eventfd (initval => 0);
    assert evfd != -1;

    epfd = bionic.epoll_create1 ();
    assert epfd != -1;

    clear event;
    event.events = (uint)POLLIN;
    event.data.g64 = 1;  // user event
    assert bionic.epoll_ctl (epfd, EPOLL_CTL_ADD, evfd, event) == 0;

    if (tcp_server.s[0] > 0)
    {
      clear event;
      event.events = (EPOLLIN | EPOLLRDHUP);
      event.data.g64 = 2;  // socket
      assert epoll_ctl (epfd, EPOLL_CTL_ADD, tcp_server.s[0], event) == 0;
    }

    if (tcp_server.s[1] > 0)
    {
      clear event;
      event.events = (EPOLLIN | EPOLLRDHUP);
      event.data.g64 = 3;  // socket
      assert epoll_ctl (epfd, EPOLL_CTL_ADD, tcp_server.s[1], event) == 0;
    }

    tcp_server.evfd = evfd;
    tcp_server.epfd = epfd;
  }
#endif  // ANDROID

  return 0;
}

//-----------------------------------------------------------------------------------------

public void TCP_close_server (ref TCP_SERVER tcp_server)
{
#if WINDOWS
  assert (tcp_server.v > 0);

  if (tcp_server.s[0] > 0)
    closesocket (tcp_server.s[0]);

  if (tcp_server.s[1] > 0)
    closesocket (tcp_server.s[1]);

  WSACloseEvent (tcp_server.v);
#endif

#if ANDROID
  close (tcp_server.evfd);

  close (tcp_server.epfd);

  if (tcp_server.s[0] > 0)
    close (tcp_server.s[0]);

  if (tcp_server.s[1] > 0)
    close (tcp_server.s[1]);
#endif

  clear tcp_server;
}

//-----------------------------------------------------------------------------------------

public int TCP_incoming_call (TCP_SERVER tcp_server, out TCP_CLIENT tcp_client)
{
  uint     size;
  int      rc;
  SOCKET   s;

#if WINDOWS
  WSAEVENT v;
#endif

#if ANDROID
  int evfd;   // event descriptor
  int epfd;   // epoll descriptor
#endif

  clear tcp_client;

#if WINDOWS
  assert (tcp_server.v > 0);
#endif

  if (tcp_server.s[0] > 0)
  {
    sockaddr_in6 a;

    size = a'size;
    s = accept (tcp_server.s[0], (byte *)&a, &size);

    if (s != -1)      // we have an incoming connection
    {
      rc = set_stream_socket_options (s);
      if (rc < 0)
      {
        trace ("warning: setsockopt() returned %d\n", rc);
#if WINDOWS
        closesocket(s);
#endif

#if ANDROID
        close(s);
#endif

        return rc;
      }

#if WINDOWS
      v = WSACreateEvent();
      if (v == 0)
      {
        rc = negative_last_windows_error();
        closesocket (s);
        return rc;
      }

      // this will also set socket to non-blocking mode
      if (WSAEventSelect(s, v, FD_READ | FD_WRITE | FD_CLOSE) != 0)
      {
        rc = negative_last_windows_error();
        trace ("warning: WSAEventSelect() returned %d\n", rc);
        closesocket (s);
        WSACloseEvent (v);
        return rc;
      }
#endif

#if ANDROID
      {
        epoll_event event;

        evfd = bionic.eventfd (initval => 0);
        epfd = bionic.epoll_create1 ();

        assert evfd != -1;
        assert epfd != -1;

        clear event;
        event.events = (uint)POLLIN;
        event.data.g64 = 1;  // user event
        assert bionic.epoll_ctl (epfd, EPOLL_CTL_ADD, evfd, event) == 0;

        clear event;
        event.events = (EPOLLIN | EPOLLRDHUP);
        event.data.g64 = 2;  // socket
        assert epoll_ctl (epfd, EPOLL_CTL_ADD, s, event) == 0;
      }
#endif

      tcp_client = {s       => s,

#if WINDOWS
                    v       => v,
#endif

#if ANDROID
                   evfd     => evfd,
                   epfd     => epfd,
                   bpollout_set => false,
                   bsend_called => false,
#endif

                    is_ipv6 => true,
                    crypt   => {crypted     => false,
                                key         => {all=>0},
                                send_offset => 0,
                                recv_offset => 0x80000000}};
      return 0;
    }

#if WINDOWS
    rc = negative_last_windows_error();
#endif
#if ANDROID
    rc = negative_errno ();
#endif

    if (rc != -EWOULDBLOCK)
      return rc;
  }


  if (tcp_server.s[1] > 0)
  {
    sockaddr_in a;

    size = a'size;

    s = accept (tcp_server.s[1], (byte *)&a, &size);

    if (s != -1)      // we have an incoming connection
    {
      rc = set_stream_socket_options (s);
      if (rc < 0)
      {
        trace ("warning: setsockopt() returned %d\n", rc);
#if WINDOWS
        closesocket(s);
#endif

#if ANDROID
        close(s);
#endif
        return rc;
      }

#if WINDOWS
      v = WSACreateEvent();
      if (v == 0)
      {
        rc = negative_last_windows_error();
        closesocket (s);
        return rc;
      }

      // this will also set socket to non-blocking mode
      if (WSAEventSelect(s, v, FD_READ | FD_WRITE | FD_CLOSE) != 0)
      {
        rc = negative_last_windows_error();
        trace ("warning: WSAEventSelect() returned %d\n", rc);
        closesocket (s);
        WSACloseEvent (v);
        return rc;
      }
#endif

#if ANDROID
      {
        epoll_event event;

        evfd = bionic.eventfd (initval => 0);
        epfd = bionic.epoll_create1 ();

        assert evfd != -1;
        assert epfd != -1;

        clear event;
        event.events = (uint)POLLIN;
        event.data.g64 = 1;  // user event
        assert bionic.epoll_ctl (epfd, EPOLL_CTL_ADD, evfd, event) == 0;

        clear event;
        event.events = (EPOLLIN | EPOLLRDHUP);
        event.data.g64 = 2;  // socket
        assert epoll_ctl (epfd, EPOLL_CTL_ADD, s, event) == 0;
      }
#endif

      tcp_client = {s       => s,

#if WINDOWS
                    v       => v,
#endif

#if ANDROID
                   evfd     => evfd,
                   epfd     => epfd,
                   bpollout_set => false,
                   bsend_called => false,
#endif
                    is_ipv6 => false,
                    crypt   => {crypted     => false,
                                key         => {all=>0},
                                send_offset => 0,
                                recv_offset => 0x80000000}};
      return 0;
    }

#if WINDOWS
    rc = negative_last_windows_error();
#endif
#if ANDROID
    rc = negative_errno ();
#endif

    if (rc != -EWOULDBLOCK)
      return rc;
  }

  return TCP_WAITING;
}

//-----------------------------------------------------------------------------------------

// returns 0 if server had an event, -1 if timeout.

public int TCP_wait_server (ref TCP_SERVER tcp_server, uint nsecs)
{
  int rc;
  int msecs = (int)((nsecs <= (uint'max/1000)) ? (nsecs * 1000) : uint'max);

#if WINDOWS
  assert (tcp_server.v > 0);

  rc = (int)WSAWaitForMultipleEvents (1, &tcp_server.v, 0, (uint)msecs, 0);

  if (rc == 0)
  {
    WSAResetEvent (tcp_server.v);
    return 0;
  }

  return -1;
#endif


#if ANDROID
  epoll_event event;

  rc = bionic.epoll_wait (tcp_server.epfd, &event, maxevents => 1, timeout => msecs);

  if (rc == 1)   // socket or user event
  {
    if (event.data.g64 == 1)  // user event
    {
      long value;
      assert bionic.read (tcp_server.evfd, (byte*)&value, 8) == 8;
    }
    return 0;
  }
  else if (rc == 0)   // timeout
    return -1;
  else
    return -1;        // some error
#endif
}

//-----------------------------------------------------------------------------------------

// copy the TCP_CLIENT structure from 'from' to 'to'.
// 'from' is cleared.

public void transfer_tcp_client (ref TCP_CLIENT source, out TCP_CLIENT target)
{
  target = source;
  clear source;
}

//-----------------------------------------------------------------------------------------

// port is usually between 1 and 65535,
// or use 0 to let the network layer decide (usually for clients).

public
int UDP_create_endpoint (out UDP_ENDPOINT udp,
                             uint2        port = 0,
                             IPV_MODE     mode = MODE_IPV6)
{
  int      rc;
  SOCKET   s;

#if WINDOWS
  WSAEVENT v;
#endif

  uint2    hport;

  hport = (uint2)((port << 8) + (port >> 8));

  clear udp;

#if WINDOWS
  v = WSACreateEvent();
  if (v == 0)
    return negative_last_windows_error();
#endif

  if (mode == MODE_IPV6 || mode == MODE_DUAL)
  {
    s = create_socket (AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s > 0)
    {

#if WINDOWS
      // this will also set socket to non-blocking mode
      if (WSAEventSelect(s, v, FD_READ|FD_WRITE) != 0)
      {
        rc = negative_last_windows_error();
        closesocket (s);
        WSACloseEvent (v);
        return rc;
      }
#endif

      // bind to local port number
      {
        sockaddr_in6 a = {AF_INET6, hport, 0, {all=>0}, 0};

        if (bind (s, (byte *)&a, a'size) == -1)
        {
#if WINDOWS
          rc = negative_last_windows_error();
          closesocket (s);
          WSACloseEvent (v);
#endif
#if ANDROID
          rc = negative_errno ();
          close (s);
#endif

          if (rc == -EADDRINUSE)  // someone already listens on this port
            return TCP_PORT_IN_USE;
          else
            return rc;
        }
      }

      udp.s[0] = s;
    }
  }

  if (mode == MODE_IPV4 || mode == MODE_DUAL)
  {
    s = create_socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s > 0)
    {
#if WINDOWS
      // this will also set socket to non-blocking mode
      if (WSAEventSelect(s, v, FD_READ|FD_WRITE) != 0)
      {
        rc = negative_last_windows_error();
        closesocket (s);
        WSACloseEvent (v);

        if (udp.s[0] > 0)
          closesocket (udp.s[0]);
        clear udp;

        return rc;
      }
#endif

      // bind to local port number
      {
        sockaddr_in a = {AF_INET, hport, {all=>0}, {all=>0}};

        if (bind (s, (byte *)&a, a'size) == -1)
        {
#if WINDOWS
          rc = negative_last_windows_error();
          closesocket (s);
          WSACloseEvent (v);
          if (udp.s[0] > 0)
            closesocket (udp.s[0]);
#endif
#if ANDROID
          rc = negative_errno ();
          close (s);
          if (udp.s[0] > 0)
            close (udp.s[0]);
#endif

          clear udp;

          if (rc == -EADDRINUSE)  // someone already listens on this port
            return TCP_PORT_IN_USE;
          else
            return rc;
        }
      }

      udp.s[1] = s;
    }
  }

  if (udp.s[0] == 0 && udp.s[1] == 0)    // neither socket was created
  {
#if WINDOWS
    rc = negative_last_windows_error();
    WSACloseEvent (v);
#endif
#if ANDROID
    rc = negative_errno ();
#endif
    return rc;
  }

#if WINDOWS
  udp.v = v;
#endif

#if ANDROID
  {
    int evfd;   // event descriptor
    int epfd;   // epoll descriptor
    epoll_event event;

    evfd = bionic.eventfd (initval => 0);
    assert evfd != -1;

    epfd = bionic.epoll_create1 ();
    assert epfd != -1;

    clear event;
    event.events = (uint)POLLIN;
    event.data.g64 = 1;  // user event
    assert bionic.epoll_ctl (epfd, EPOLL_CTL_ADD, evfd, event) == 0;

    if (udp.s[0] > 0)
    {
      clear event;
      event.events = (EPOLLIN | EPOLLRDHUP);
      event.data.g64 = 2;  // socket
      assert epoll_ctl (epfd, EPOLL_CTL_ADD, udp.s[0], event) == 0;
    }

    if (udp.s[1] > 0)
    {
      clear event;
      event.events = (EPOLLIN | EPOLLRDHUP);
      event.data.g64 = 3;  // socket
      assert epoll_ctl (epfd, EPOLL_CTL_ADD, udp.s[1], event) == 0;
    }

    udp.evfd = evfd;
    udp.epfd = epfd;  
  }
#endif  // ANDROID

  return 0;
}

//-----------------------------------------------------------------------------------------

// return 0 if a block arrived or a negative error code.
// returns TCP_WAITING if no new block arrived : you must then retry later.
// buffer'size should be at least 548 bytes for IPV4, or 1232 bytes for IPV6.

public
int UDP_receive (ref UDP_ENDPOINT  udp,
                 out byte[]        buffer,
                 out uint          actual_buffer_length,
                 out PEER          origin,
                 out UDP_LOCAL     local)
{
  byte* ptr1 = &buffer;   // avoids clearing buffer
  PEER* ptr2 = &origin;   // and origin

#if WINDOWS
  uint len;
#endif

  int  rc;

  _unused ptr1;
  _unused ptr2;

  clear actual_buffer_length, local;

#if WINDOWS
  assert (udp.v > 0);
#endif

  g_order_recv_udp = 1 - g_order_recv_udp;

  if (g_order_recv_udp == 0)
  {
    if (udp.s[0] > 0)
    {
      sockaddr_in6 a;

#if WINDOWS
      if (g_WsaRecvMsg == null)
      {
        len = a'size;
        rc = recvfrom (udp.s[0], (byte *)&buffer, buffer'size, 0, (byte *)&a, &len);
        clear local;
      }
      else
      {
        WSAMSG msg;
        WSABUF buf;

        clear buf;
        buf.len = buffer'size;
        buf.buf = &buffer;

        clear msg;
        msg.name = (byte*)&a;
        msg.namelen = a'size;
        msg.lpBuffers = &buf;
        msg.dwBufferCount = 1;
        msg.Control.len = local.buf'size;
        msg.Control.buf = &local.buf;

        if (g_WsaRecvMsg (udp.s[0], &msg, (uint4*)&rc) != 0)
          rc = -1;
        else
          local.len = msg.Control.len;
      }
#endif

#if ANDROID
      {
        msghdr msg;
        iovec  buf;

        clear buf;
        buf.iov_base = &buffer;
        buf.iov_len = buffer'size;

        clear msg;
        msg.msg_name       = (byte*)&a;
        msg.msg_namelen    = a'size;
        msg.msg_iov        = &buf;
        msg.msg_iovlen     = 1;
        msg.msg_control    = &local.buf;
        msg.msg_controllen = local.buf'size;

        rc = recvmsg (udp.s[0], &msg, flags => 0);
        if (rc != -1)
          local.len = (uint4)msg.msg_controllen;
      }
#endif

      if (rc >= 0)
      {
// log ("$ recvmsg() rc=%d", rc);

        actual_buffer_length = (uint)rc;
        origin.ip = a.sin6_addr;
        origin.port = (uint2)((a.sin6_port >> 8) + (a.sin6_port << 8));
        return 0;
      }

#if WINDOWS
      rc = negative_last_windows_error();
#endif
#if ANDROID
      rc = negative_errno ();
#endif


#if WINDOWS
      if (rc != -EWOULDBLOCK)
        return rc;
#endif

#if ANDROID
// log ("$recvmsg() rc=%d", rc);
      if (rc != -EWOULDBLOCK && rc != -EAGAIN)
        return rc;
#endif
    }


    if (udp.s[1] > 0)
    {
      sockaddr_in a;

#if WINDOWS
      if (g_WsaRecvMsg == null)
      {
        len = a'size;
        rc = recvfrom (udp.s[1], (byte *)&buffer, buffer'size, 0, (byte *)&a, &len);
        clear local;
      }
      else
      {
        WSAMSG msg;
        WSABUF buf;

        clear buf;
        buf.len = buffer'size;
        buf.buf = &buffer;

        clear msg;
        msg.name = (byte*)&a;
        msg.namelen = a'size;
        msg.lpBuffers = &buf;
        msg.dwBufferCount = 1;
        msg.Control.len = local.buf'size;
        msg.Control.buf = &local.buf;

        if (g_WsaRecvMsg (udp.s[1], &msg, (uint4*)&rc) != 0)
          rc = -1;
        else
          local.len = msg.Control.len;
      }
#endif

#if ANDROID
      {
        msghdr msg;
        iovec  buf;

        clear buf;
        buf.iov_base = &buffer;
        buf.iov_len = buffer'size;

        clear msg;
        msg.msg_name       = (byte*)&a;
        msg.msg_namelen    = a'size;
        msg.msg_iov        = &buf;
        msg.msg_iovlen     = 1;
        msg.msg_control    = &local.buf;
        msg.msg_controllen = local.buf'size;

        rc = recvmsg (udp.s[1], &msg, flags => 0);
        if (rc != -1)
          local.len = (uint4)msg.msg_controllen;
      }
#endif

      if (rc >= 0)
      {
// log ("$ recvmsg() rc=%d", rc);
        actual_buffer_length = (uint)rc;
        origin.ip[0:12] = IPV4_PREFIX;
        origin.ip[12:4] = a.sin_addr;
        origin.port = (uint2)((a.sin_port >> 8) + (a.sin_port << 8));
        return 0;
      }

#if WINDOWS
      rc = negative_last_windows_error();
#endif
#if ANDROID
      rc = negative_errno ();
#endif

#if WINDOWS
      if (rc != -EWOULDBLOCK)
        return rc;
#endif

#if ANDROID
// log ("$recvmsg() rc=%d", rc);
      if (rc != -EWOULDBLOCK && rc != -EAGAIN)
        return rc;
#endif
    }
  }
  else
  {
    if (udp.s[1] > 0)
    {
      sockaddr_in a;

#if WINDOWS
      if (g_WsaRecvMsg == null)
      {
        len = a'size;
        rc = recvfrom (udp.s[1], (byte *)&buffer, buffer'size, 0, (byte *)&a, &len);
        clear local;
      }
      else
      {
        WSAMSG msg;
        WSABUF buf;

        clear buf;
        buf.len = buffer'size;
        buf.buf = &buffer;

        clear msg;
        msg.name = (byte*)&a;
        msg.namelen = a'size;
        msg.lpBuffers = &buf;
        msg.dwBufferCount = 1;
        msg.Control.len = local.buf'size;
        msg.Control.buf = &local.buf;

        if (g_WsaRecvMsg (udp.s[1], &msg, (uint4*)&rc) != 0)
          rc = negative_last_windows_error();
        else
          local.len = msg.Control.len;
      }
#endif

#if ANDROID
      {
        msghdr msg;
        iovec  buf;

        clear buf;
        buf.iov_base = &buffer;
        buf.iov_len = buffer'size;

        clear msg;
        msg.msg_name       = (byte*)&a;
        msg.msg_namelen    = a'size;
        msg.msg_iov        = &buf;
        msg.msg_iovlen     = 1;
        msg.msg_control    = &local.buf;
        msg.msg_controllen = local.buf'size;

        rc = recvmsg (udp.s[1], &msg, flags => 0);
        if (rc != -1)
          local.len = (uint4)msg.msg_controllen;
      }
#endif

      if (rc >= 0)
      {
// log ("$recvmsg() rc=%d", rc);
        actual_buffer_length = (uint)rc;
        origin.ip[0:12] = IPV4_PREFIX;
        origin.ip[12:4] = a.sin_addr;
        origin.port = (uint2)((a.sin_port >> 8) + (a.sin_port << 8));
        return 0;
      }

#if WINDOWS
      rc = negative_last_windows_error();
#endif
#if ANDROID
      rc = negative_errno ();
#endif

#if WINDOWS
      if (rc != -EWOULDBLOCK)
        return rc;
#endif

#if ANDROID
// log ("$recvmsg() rc=%d", rc);
      if (rc != -EWOULDBLOCK && rc != -EAGAIN)
        return rc;
#endif
    }

    if (udp.s[0] > 0)
    {
      sockaddr_in6 a;

#if WINDOWS
      if (g_WsaRecvMsg == null)
      {
        len = a'size;
        rc = recvfrom (udp.s[0], (byte *)&buffer, buffer'size, 0, (byte *)&a, &len);
        clear local;
      }
      else
      {
        WSAMSG msg;
        WSABUF buf;

        clear buf;
        buf.len = buffer'size;
        buf.buf = &buffer;

        clear msg;
        msg.name = (byte*)&a;
        msg.namelen = a'size;
        msg.lpBuffers = &buf;
        msg.dwBufferCount = 1;
        msg.Control.len = local.buf'size;
        msg.Control.buf = &local.buf;

        if (g_WsaRecvMsg (udp.s[0], &msg, (uint4*)&rc) != 0)
          rc = negative_last_windows_error();
        else
          local.len = msg.Control.len;
      }
#endif

#if ANDROID
      {
        msghdr msg;
        iovec  buf;

        clear buf;
        buf.iov_base = &buffer;
        buf.iov_len = buffer'size;

        clear msg;
        msg.msg_name       = (byte*)&a;
        msg.msg_namelen    = a'size;
        msg.msg_iov        = &buf;
        msg.msg_iovlen     = 1;
        msg.msg_control    = &local.buf;
        msg.msg_controllen = local.buf'size;

        rc = recvmsg (udp.s[0], &msg, flags => 0);
        if (rc != -1)
          local.len = (uint4)msg.msg_controllen;
      }
#endif

      if (rc >= 0)
      {
// log ("$ recvmsg() rc=%d", rc);
        actual_buffer_length = (uint)rc;
        origin.ip = a.sin6_addr;
        origin.port = (uint2)((a.sin6_port >> 8) + (a.sin6_port << 8));
        return 0;
      }

#if WINDOWS
      rc = negative_last_windows_error();
#endif
#if ANDROID
      rc = negative_errno ();
#endif

#if WINDOWS
      if (rc != -EWOULDBLOCK)
        return rc;
#endif

#if ANDROID
// log ("$recvmsg() rc=%d", rc);
      if (rc != -EWOULDBLOCK && rc != -EAGAIN)
        return rc;
#endif
    }
  }

  return TCP_WAITING;
}

//-----------------------------------------------------------------------------------------

// returns 0 if ok, or a negative error code.

public
int UDP_set_destination (ref UDP_ENDPOINT udp,
                             PEER         peer,     // with port between 1 and 65535
                             UDP_LOCAL    local)
{
  uint2 hport;
  bool  send_ipv6;

#if WINDOWS
  assert (udp.v > 0);
#endif

  if (peer.port == 0)
    return TCP_INVALID_PORT;

  hport = (uint2)((peer.port << 8) + (peer.port >> 8));

  send_ipv6 = is_ipv6 (peer.ip);

  if (send_ipv6)
  {
    if (udp.s[0] <= 0)
      return TCP_NO_IP6;
    udp.a6 = {AF_INET6, hport, 0, peer.ip, 0};
  }
  else   // IPV4
  {
    if (udp.s[1] <= 0)
      return TCP_NO_IP4;
    udp.a = {AF_INET, hport, peer.ip[12:4], {all=>0}};
  }

  udp.send_ipv6 = send_ipv6;
  udp.local = local;

  return 0;
}

//-----------------------------------------------------------------------------------------

// returns 0 if ok, or a negative error code.
// returns TCP_WAITING if the output buffers are full : you must then retry later.
// buffer'size should not exceed 548 bytes for IPV4, or 1232 bytes for IPV6.

public
int UDP_send (ref UDP_ENDPOINT udp,
                  byte[]       buffer)
{
  int rc;

#if WINDOWS
  assert (udp.v > 0);
#endif

  if (udp.send_ipv6)
  {
#if WINDOWS
    if (g_WsaSendMsg == null)
    {
      uint len = udp.a6'size;
      rc = sendto (udp.s[0], &buffer, buffer'size, 0, (byte *)&udp.a6, &len);
    }
    else
    {
      DWORD  NumberOfBytesSent;
      WSAMSG msg;
      WSABUF buf;

      clear buf;
      buf.len = buffer'size;
      buf.buf = &buffer;

      clear msg;
      msg.name = (byte*)&udp.a6;
      msg.namelen = udp.a6'size;
      msg.lpBuffers = &buf;
      msg.dwBufferCount = 1;

      if (udp.local.len > 0)
      {
        msg.Control.len = udp.local.len;
        msg.Control.buf = &udp.local.buf;
      }

      rc = 0;
      if (g_WsaSendMsg (udp.s[0], &msg, 0, &NumberOfBytesSent) != 0)
        rc = -1;
    }
#endif

#if ANDROID
    {
      msghdr msg;
      iovec  buf;

      udp.bsend_called[0] = true;

      clear buf;
      buf.iov_base = &buffer;
      buf.iov_len = buffer'size;

      clear msg;
      msg.msg_name       = (byte*)&udp.a6;
      msg.msg_namelen    = udp.a6'size;
      msg.msg_iov        = &buf;
      msg.msg_iovlen     = 1;

      if (udp.local.len > 0)
      {
        msg.msg_control    = &udp.local.buf;
        msg.msg_controllen = udp.local.len;
      }

      rc = sendmsg (udp.s[0], &msg, flags => 0);
    }
#endif
  }
  else    // ipv4
  {
#if WINDOWS
    if (g_WsaSendMsg == null)
    {
      uint len = udp.a'size;
      rc = sendto (udp.s[1], &buffer, buffer'size, 0, (byte *)&udp.a, &len);
    }
    else
    {
      DWORD  NumberOfBytesSent;
      WSAMSG msg;
      WSABUF buf;

      clear buf;
      buf.len = buffer'size;
      buf.buf = &buffer;

      clear msg;
      msg.name = (byte*)&udp.a;
      msg.namelen = udp.a'size;
      msg.lpBuffers = &buf;
      msg.dwBufferCount = 1;

      if (udp.local.len > 0)
      {
        msg.Control.len = udp.local.len;
        msg.Control.buf = &udp.local.buf;
      }

      rc = 0;
      if (g_WsaSendMsg (udp.s[1], &msg, 0, &NumberOfBytesSent) != 0)
        rc = -1;
    }
#endif

#if ANDROID
    {
      msghdr msg;
      iovec  buf;

      udp.bsend_called[1] = true;

      clear buf;
      buf.iov_base = &buffer;
      buf.iov_len = buffer'size;

      clear msg;
      msg.msg_name       = (byte*)&udp.a;
      msg.msg_namelen    = udp.a'size;
      msg.msg_iov        = &buf;
      msg.msg_iovlen     = 1;

      if (udp.local.len > 0)
      {
        msg.msg_control    = &udp.local.buf;
        msg.msg_controllen = udp.local.len;
      }

      rc = sendmsg (udp.s[1], &msg, flags => 0);
    }
#endif
  }

  if (rc == -1)   // some error occured
  {
#if WINDOWS
    rc = negative_last_windows_error();
#endif
#if ANDROID
    rc = negative_errno ();
#endif

#if WINDOWS
    if (rc == -EWOULDBLOCK)     // not really an error
#endif
#if ANDROID
// log ("UDP_send() : sendmsg() errno = %d", rc);
    if (rc == -EWOULDBLOCK || rc == -EAGAIN)
#endif
    {
      udp.sending_buffer_full = true;
      return TCP_WAITING;
    }

    udp.sending_buffer_full = false;
    return rc;
  }

  udp.sending_buffer_full = false;
  return 0;
}

//-----------------------------------------------------------------------------------------

// wait until we can send or receive further blocks
// returns 0 if further send or receive are possible, -1 if timeout.

public
int UDP_wait (ref UDP_ENDPOINT udp, uint millisecs)
{
  int rc;

#if WINDOWS
  assert (udp.v > 0);

  // wait for read or write event, or extern wakeup

  rc = (int)WSAWaitForMultipleEvents (1, &udp.v, 0, millisecs, 0);
  if (rc != 0)
    return -1;   // timeout

  assert WSAResetEvent (udp.v) == 1;

  return 0;
#endif

#if ANDROID
  epoll_event event;

  if (udp.bsend_called[0] != udp.bpollout_set[0])
  {
    udp.bpollout_set[0] = udp.bsend_called[0];

    clear event;
    event.events = (EPOLLIN | EPOLLRDHUP) | (EPOLLOUT * (uint)udp.bsend_called[0]);
    event.data.g64 = 2;  // socket
    assert epoll_ctl (udp.epfd, EPOLL_CTL_MOD, udp.s[0], event) == 0;
  }
  udp.bsend_called[0] = false;

  if (udp.bsend_called[1] != udp.bpollout_set[1])
  {
    udp.bpollout_set[1] = udp.bsend_called[1];

    clear event;
    event.events = (EPOLLIN | EPOLLRDHUP) | (EPOLLOUT * (uint)udp.bsend_called[1]);
    event.data.g64 = 3;  // socket
    assert epoll_ctl (udp.epfd, EPOLL_CTL_MOD, udp.s[1], event) == 0;
  }
  udp.bsend_called[1] = false;

//$
//log ("IN udp_wait (pollout = %u %u  msec = %u)", udp.bpollout_set[0], udp.bpollout_set[1], millisecs);

  rc = bionic.epoll_wait (udp.epfd, &event, maxevents => 1, timeout => (int)millisecs);

//log ("OUT udp_wait rc = %d", rc);

  if (rc == 1)   // socket or user event
  {
    if (event.data.g64 == 1)  // user event
    {
      long value;
//log ("OUT udp_wait user event");
      assert bionic.read (udp.evfd, (byte*)&value, 8) == 8;
    }
    else
    {
//log ("OUT udp_wait socket event index %d flags 0x%x  (1=read, 4=write)", event.data.g64 - 2, event.events);
    }
    return 0;
  }
  else if (rc == 0)   // timeout
  {
//log ("OUT udp_wait user timeout");
    return -1;
  }
  else
  {
//log ("OUT udp_wait user error");
    return -1;
  }
#endif
}

//-----------------------------------------------------------------------------------------

// signal UDP layer that we have further blocks to send
// (wakes up UDP_wait() if output buffers are not full)

public
void UDP_wakeup (ref UDP_ENDPOINT udp)
{
  if (udp.sending_buffer_full)   // ignore wake up : buffers are full anyway
    return;

#if WINDOWS
  assert (udp.v > 0);
  assert WSASetEvent (udp.v) == 1;
#endif

#if ANDROID
  {
    long value = 1;
    assert bionic.write (udp.evfd, (byte*)&value, 8) == 8;
  }
#endif
}

//-----------------------------------------------------------------------------------------

public void UDP_close (ref UDP_ENDPOINT udp)
{
#if WINDOWS
  assert (udp.v > 0);

  if (udp.s[0] > 0)
    closesocket (udp.s[0]);

  if (udp.s[1] > 0)
    closesocket (udp.s[1]);

  WSACloseEvent (udp.v);
#endif

#if ANDROID
  if (udp.s[0] > 0)
    close (udp.s[0]);

  if (udp.s[1] > 0)
    close (udp.s[1]);

  close (udp.evfd);
  close (udp.epfd);
#endif

  clear udp;
}

//-----------------------------------------------------------------------------------------

void TCP_crypt_block (out byte[] data_out,
                      byte[]     data_in,
                      uint       offset,   // offset within stream
                      byte[16]   key)
{
  byte seq_data[16], xor_data[16];
  uint start, rest, pos, chunk_size, i, ofs;

  assert (data_out'size == data_in'size);

  assert (&data_out != null);   // this removes a warning

  pos = offset;

  ofs = offset & 15;
  chunk_size = 16 - ofs;
  if (chunk_size > data_in'size)
    chunk_size = data_in'size;

  clear seq_data;
  seq_data[0:4]'byte = pos'byte;
  seq_data[0] &= 240;

  aes_encrypt (out xor_data, seq_data, key);

  for (i=0; i<chunk_size; i++)
    data_out[i] = (byte)(data_in[i] ^ xor_data[ofs + i]);

  start = chunk_size;
  rest = data_in'size - chunk_size;
  pos += chunk_size;

  while (rest >= 16)     // more 16-byte blocks to encrypt
  {
    seq_data[0:4]'byte = pos'byte;
    seq_data[0] &= 240;

    aes_encrypt (out xor_data, seq_data, key);

    *((uint *)&data_out[start+0])  = *((uint *)&data_in[start+0])  ^ *((uint *)&xor_data[0]);
    *((uint *)&data_out[start+4])  = *((uint *)&data_in[start+4])  ^ *((uint *)&xor_data[4]);
    *((uint *)&data_out[start+8])  = *((uint *)&data_in[start+8])  ^ *((uint *)&xor_data[8]);
    *((uint *)&data_out[start+12]) = *((uint *)&data_in[start+12]) ^ *((uint *)&xor_data[12]);

    start += 16;
    rest  -= 16;
    pos   += 16;
  }

  if (rest > 0)      // some rest data to encrypt
  {
    seq_data[0:4]'byte = pos'byte;
    seq_data[0] &= 240;

    aes_encrypt (out xor_data, seq_data, key);

    for (i=0; i<rest; i++)
      data_out[start+i] = (byte)(data_in[start+i] ^ xor_data[i]);
  }
}

//-----------------------------------------------------------------------------------------

// enable/disable cryption

public void TCP_enable_crypt (ref TCP_CLIENT tcp_client, bool on)
{
  assert (tcp_client.s > 0);
  tcp_client.crypt.crypted = on;
}

//-----------------------------------------------------------------------------------------

public bool TCP_is_crypted (ref TCP_CLIENT tcp_client)
{
  assert (tcp_client.s > 0);
  return tcp_client.crypt.crypted;
}

//-----------------------------------------------------------------------------------------

public void TCP_set_key (ref TCP_CLIENT tcp_client, byte[16] key)
{
  assert (tcp_client.s > 0);
  tcp_client.crypt.key = key;
}

//-----------------------------------------------------------------------------------------

// must be called just before TCP_send

public void TCP_encrypt (ref TCP_CLIENT tcp_client, ref byte[] data)
{
  assert (tcp_client.s > 0);
  TCP_crypt_block (out data, data, tcp_client.crypt.send_offset, tcp_client.crypt.key);
}

//-----------------------------------------------------------------------------------------

// must be called just after TCP_receive

public void TCP_decrypt (ref TCP_CLIENT tcp_client, ref byte[] data)
{
  assert (tcp_client.s > 0);
  TCP_crypt_block (out data, data, tcp_client.crypt.recv_offset - data'size, tcp_client.crypt.key);
}

//-----------------------------------------------------------------------------------------

// only IPV4

#if WINDOWS
void old_dns_to_ip (string namez, out IP[]^ ip)
{
  if (namez[0] >= '0' && namez[0] <= '9')   // numeric ipv4
  {
    uint addr = inet_addr (&namez);

    if (addr != 0 && addr != 0xFFFFFFFF)
    {
      ip = new IP[1];
      ip^[0][0:12] = IPV4_PREFIX;
      ip^[0][12:4]'byte = addr'byte;
      return;
    }
  }
  else
  {
    hostent* p;
    byte**   list, l;
    int      count;

    p = gethostbyname (&namez);
    if (p != null && p->h_addrtype == AF_INET)
    {
      list = p->h_addr_list;

      l = list;
      count = 0;
      while (l[count] != null)
        count++;

      ip = new IP[count];

      l = list;
      count = 0;
      while (l[count] != null)
      {
        ip^[count][0:12] = IPV4_PREFIX;
        ip^[count][12:4] = l[count][0:4];
        count++;
      }

      return;
    }
  }

  ip = new IP[0];  // default
}
#endif

//-----------------------------------------------------------------------------------------

#if WINDOWS
public void dns_to_ip (string name, out IP[]^ ip)
{
  string^      namez;
  const string DLLNAME = "ws2_32.dll\0";
  const string F1      = "getaddrinfo\0";
  const string F2      = "freeaddrinfo\0";
  LPARAM*      func1, func2;
  GETADDRINFO  getaddrinfo;
  FREEADDRINFO freeaddrinfo;
  ADDRINFOA*   p, list;
  int          rc;
  HMODULE      h;
  uint         i, count;

  rc = init_socket_library ();
  if (rc < 0)
  {
    ip = new IP[0];
    return;
  }

  namez = new string (strlen(name)+1);
  strcpy (out namez^, name);

  h = GetModuleHandleA (&DLLNAME);

  func1 = (LPARAM *)GetProcAddress (h, &F1);
  func2 = (LPARAM *)GetProcAddress (h, &F2);

  if (func1 == null || func2 == null)     // function not available before windows XP
  {
    old_dns_to_ip (namez^, out ip);
    free namez;
    return;
  }

  *((LPARAM **)&getaddrinfo)  = func1;
  *((LPARAM **)&freeaddrinfo) = func2;

  rc = getaddrinfo (&namez^, null, null, &list);

  free namez;

  if (rc != 0)
  {
    ip = new IP[0];
    return;
  }

  count = 0;
  p = list;
  while (p != null)
  {
    if (p->ai_family == AF_INET || p->ai_family == AF_INET6)
      count++;
    p = p->ai_next;
  }

  ip = new IP[count];

  p = list;
  i = 0;
  while (p != null)
  {
    if (p->ai_family == AF_INET)
    {
      ip^[i][0:12] = IPV4_PREFIX;
      ip^[i][12:4] = p->ai_addr[4:4];
      i++;
    }
    else if (p->ai_family == AF_INET6)
    {
      ip^[i] = p->ai_addr[8:16];
      i++;
    }

    p = p->ai_next;
  }

  freeaddrinfo (list);
}
#endif

#if ANDROID
public void dns_to_ip (string name, out IP[]^ ip)
{
  string^      namez;
  addrinfo*    p, list;
  int          len, rc;
  uint         i, count;

  len = strlen(name);
  namez = new string (len+1);

  if (len >= 3 && name[0] == '[' && name[len-1] == ']')   // in brackets
    strcpy (out namez^, name[1:len-2]);   // remove brackets
  else
    strcpy (out namez^, name);

  rc = getaddrinfo (&namez^, null, null, &list);

  free namez;

  if (rc != 0)
  {
    ip = new IP[0];
    return;
  }

  count = 0;
  p = list;
  while (p != null)
  {
    if (p->ai_family == AF_INET || p->ai_family == AF_INET6)
      count++;
    p = p->ai_next;
  }

  ip = new IP[count];

  p = list;
  i = 0;
  while (p != null)
  {
    if (p->ai_family == AF_INET)
    {
      ip^[i][0:12] = IPV4_PREFIX;
      ip^[i][12:4] = p->ai_addr[4:4];
      i++;
    }
    else if (p->ai_family == AF_INET6)
    {
      ip^[i] = p->ai_addr[8:16];
      i++;
    }

    p = p->ai_next;
  }

  freeaddrinfo (list);
}
#endif

//-----------------------------------------------------------------------------------------

// only IPV4
// BEWARE: this function is very slow (4 to 15 seconds !)

#if WINDOWS
int old_ip_to_dns (IP ip, out char[1024] name)
{
  hostent *p;

  clear name;

  if (is_ipv6 (ip))
    return -1;

  p = gethostbyaddr (&ip[12], 4, AF_INET);
  if (p == null)
    return -1;

  strcpy (out name, p->h_name[0:1024]);
  return 0;
}
#endif

//-----------------------------------------------------------------------------------------

// returns 0 if a dns name was found, -1 if none was found
// BEWARE: this function is very slow (4 to 15 seconds !)

#if WINDOWS
public int ip_to_dns (IP ip, out char[1024] name)
{
  const string DLLNAME = "ws2_32.dll\0";
  const string F1      = "getnameinfo\0";
  uint*        func1;
  GETNAMEINFO  getnameinfo;
  int          rc;
  HMODULE      h;
  char         hostname[1025];

  rc = init_socket_library ();
  if (rc < 0)
  {
    clear name;
    return -1;
  }

  h = GetModuleHandleA (&DLLNAME);

  func1 = (uint *)GetProcAddress (h, &F1);

  if (func1 == null)     // function not available before windows XP
    return old_ip_to_dns (ip, out name);


  *((uint **)&getnameinfo) = func1;

  if (is_ipv6 (ip))
  {
    sockaddr_in6 a = {AF_INET6, 0, 0, ip, 0};
    rc = getnameinfo
          (addr => (byte *)&a,
           addr_len => (socklen_t)a'size,
           host => &hostname,
           hostlen => hostname'size,
           serv => null,
           servlen => 0,
           flags => 0);
  }
  else
  {
    sockaddr_in a = {AF_INET, 0, ip[12:4], {all=>0}};
    rc = getnameinfo
         (addr => (byte *)&a,
          addr_len => (socklen_t)a'size,
          host => &hostname,
          hostlen => hostname'size,
          serv => null,
          servlen => 0,
          flags => 0);
  }

  if (rc != 0)
  {
    clear name;
    return -1;
  }

  strcpy (out name, hostname);
  return 0;
}
#endif


// returns 0 if a dns name was found, -1 if none was found
// BEWARE: this function is very slow (4 to 15 seconds !)

#if ANDROID
public int ip_to_dns (IP ip, out char[1024] name)
{
  int  rc;
  char hostname[1025];

  if (is_ipv6 (ip))
  {
    sockaddr_in6 a = {AF_INET6, 0, 0, ip, 0};
    rc = getnameinfo
          (addr     => (byte *)&a,
           addrlen  => a'size,
           host     => &hostname,
           hostlen  => hostname'size,
           serv     => null,
           servlen  => 0,
           flags    => 0);
  }
  else
  {
    sockaddr_in a = {AF_INET, 0, ip[12:4], {all=>0}};
    rc = getnameinfo
         (addr     => (byte *)&a,
          addrlen  => a'size,
          host     => &hostname,
          hostlen  => hostname'size,
          serv     => null,
          servlen  => 0,
          flags    => 0);
  }

  if (rc != 0)
  {
    clear name;
    return -1;
  }

  strcpy (out name, hostname);
  return 0;
}
#endif

//-----------------------------------------------------------------------------------------

public void ip_to_numericstr (IP ip, out char[48] ipstr)
{
  if (is_ipv4 (ip))
  {
    sprintf (out ipstr, "%u.%u.%u.%u", ip[12], ip[13], ip[14], ip[15]);
  }
  else
  {
    int  i, start, len, best_start, best_len;
    char hex[4];

    best_start = 0;
    best_len   = 0;
    for (i=0; i<8; i++)
    {
      if (ip[i*2] + ip[i*2+1] != 0)
        continue;

      start = i;   // start of zero word
      len = 1;
      while (i+1 < 8 && (ip[(i+1)*2] + ip[(i+1)*2+1] == 0))  // next words are zero too
      {
        len++;
        if (len > best_len)
        {
          best_start = start;
          best_len = len;
        }
        i++;
      }
    }

    if (best_len == 0)
      best_start = -1;   // no ::

    if (best_len == 8)
    {
      strcpy (out ipstr, "[::]");
      return;
    }

    strcpy (out ipstr, "[");

    i = 0;
    if (best_start == 0)
    {
      strcat (ref ipstr, "::");
      i = best_len;
    }

    for (;;)
    {
      sprintf (out hex, "%x", (ip[i*2]<<8) + ip[i*2+1]);
      strcat (ref ipstr, hex);
      if (i == 7)
        break;
      strcat (ref ipstr, ":");
      i++;
      if (i == best_start)
      {
        strcat (ref ipstr, ":");
        i += best_len;
        if (i == 8)
          break;
      }
    }

    strcat (ref ipstr, "]");
  }
}

//-----------------------------------------------------------------------------------------

public int CS_connect (out TCP_CLIENT tcp_client,
                       string         ip,
                       int            port,
                       uint           nsecs)
{
  IP[]^ ipt;
  int   rc, i;

  clear tcp_client;

  dns_to_ip (ip, out ipt);

  rc = TCP_CONNECTION_FAILED;
  for (i=0; i<ipt^'length; i++)
  {
    rc = TCP_connect (out tcp_client, ipt^[i], port, nsecs);
    if (rc == 0)
      break;
  }

  if (rc < 0)
    trace ("error %d in CS_connect()\n", rc);

  free ipt;

  return rc;
}

//-----------------------------------------------------------------------------------------

public int CS_read (ref TCP_CLIENT tcp_client, out byte[] buffer, uint nsecs)
{
  uint    request_index;
  TIMER   timeout;
  int     tcp_rc;

  request_index = 0;

  set_timer (out timeout, nsecs);

  for (;;)
  {
    tcp_rc = TCP_receive (ref tcp_client, ref (&buffer)[0:buffer'length], ref request_index);

    // check for block completion
    if (request_index == buffer'size)
      break;

    // check for any network problem
    if (tcp_rc < 0)
    {
      if (tcp_rc == TCP_CONNECTION_BROKEN)
        trace ("error: CS_read() cancelled by broken connection\n");
      else
        trace ("error %d in CS_read()\n", tcp_rc);
      return -1;
    }

    /* check for timeout */
    if (timer_elapsed (timeout))    /* no data received for n secs */
    {
      trace ("error: CS_read() cancelled after waiting %u secs\n", nsecs);
      return -1;
    }

    TCP_wait (ref tcp_client, nsecs);
  }

  if (tcp_client.crypt.crypted)
    TCP_decrypt (ref tcp_client, ref buffer);

  return 0;
}

//-----------------------------------------------------------------------------------------

public int CS_write (ref TCP_CLIENT tcp_client, byte[] buffer, uint nsecs)
{
  uint    reply_index;
  TIMER   timeout;
  int     tcp_rc, rc;
  byte[]^ buf;
  byte    *pbuf;

  if (tcp_client.crypt.crypted)
  {
    buf = new byte[] ' (buffer);
    TCP_encrypt (ref tcp_client, ref buf^);
    pbuf = &buf^;
  }
  else
  {
    buf = null;  // avoids warning
    pbuf = &buffer;
  }


  reply_index = 0;

  set_timer (out timeout, nsecs);

  for (;;)
  {
    tcp_rc = TCP_send (ref tcp_client, pbuf[0:buffer'length], ref reply_index);

    // check for block completion
    if (reply_index == buffer'size)
    {
      rc = 0;
      break;
    }

    // check for any network problem
    if (tcp_rc < 0)
    {
      if (tcp_rc == TCP_CONNECTION_BROKEN)
        trace ("error: CS_write() cancelled by broken connection\n");
      else
        trace ("error: CS_write() returned error %d\n", tcp_rc);

      rc = -1;
      break;
    }

    // check for timeout
    if (timer_elapsed (timeout))    // no data sent for n secs
    {
      trace ("error: CS_write() cancelled after waiting %u secs\n", nsecs);
      rc = -1;
      break;
    }

    TCP_wait (ref tcp_client, nsecs);
  }

  if (tcp_client.crypt.crypted)
    free (buf);

  return rc;
}

//-----------------------------------------------------------------------------------------

public void CS_close (ref TCP_CLIENT tcp_client)
{
  TCP_hangup (ref tcp_client);
}

//-----------------------------------------------------------------------------------------

package TCPS

  typedef BASE;

  struct HANDLER
  {
    TCP_CLIENT  tcp_client;  // connection
    BASE*       base;        // null = slot unused.
  }

  struct BASE     // base variables for the server
  {
    HANDLER[]^      table;       // table of handler data
    REQUEST_HANDLER handler;
    bool^           stop;
    bool^           trace_calls;
    byte[8]         user_data;
  }

end TCPS;

//-----------------------------------------------------------------------------------------

void connection_handler (HANDLER *h)
{
  int          nr;
  HANDLER_DATA data;

  nr = (int)(h - &h->base->table^[0]);       // index of handler

  clear data;
  transfer_tcp_client (ref h->tcp_client, out data.tcp_client);
  data.nr          = nr;
  data.trace_calls = h->base->trace_calls^;
  data.stop        = h->base->stop;
  data.user_data   = h->base->user_data;

  h->base->handler (ref data);

  if (data.trace_calls)
    trace ("handler %d : closing connection\n", nr);

  TCP_hangup (ref data.tcp_client);
  h->base = null;
}

//-----------------------------------------------------------------------------------------

void wait_for_handler_completion (BASE base)
{
  int  i, count;
  bool all_closed;

  count = base.table^'length;

  for (;;)
  {
    all_closed = true;

    for (i=0; i<count; i++)
    {

#if WINDOWS
      WSAEVENT v = base.table^[i].tcp_client.v;
      if (v > 0)
      {
        (void) WSASetEvent (v);   // wake up handler
        all_closed = false;
      }
#endif

#if ANDROID
      int evfd = base.table^[i].tcp_client.evfd;
      if (evfd > 0)
      {
        long value = 1;
        assert bionic.write (evfd, (byte*)&value, 8) == 8;
        all_closed = false;
      }
#endif

    }

    if (all_closed)   // all handlers have terminated
      return;

    sleep 0.5;
  }
}

//-----------------------------------------------------------------------------------------

public int serve_requests (int             port,
                           REQUEST_HANDLER request_handler,
                           uint            max_handlers    = 512,
                           IPV_MODE        mode            = MODE_IPV6,
                           bool^           stop            = null,
                           bool^           trace_calls     = null,
                           SERVER_IS_READY server_is_ready = null,
                           INCOMING_CALL   incoming_call   = null,
                           byte[8]         user_data       = {all=>0})
{
  BASE          base;
  TCP_SERVER    tcp_server;
  int           rc;
  int           i;
  TCP_CLIENT    tcp_client;
  PEER          addr;
  char[48]      ipstr;

  clear base;
  base.table     = new HANDLER [max_handlers];
  base.handler   = request_handler;
  base.user_data = user_data;

  if (stop == null)
    base.stop = new bool ' (false);
  else
    base.stop = stop;

  if (trace_calls == null)
    base.trace_calls = new bool ' (false);
  else
    base.trace_calls = trace_calls;

  rc = TCP_create_server (out tcp_server, (uint2)port, mode);
  if (rc < 0)
  {
    if (base.trace_calls^)
      trace ("error: TCP_create_server (port=%d) returned %d\n", port, rc);
    return rc;
  }

  if (server_is_ready != null)
  {
    server_is_ready (tcp_server);
  }

  for (;;)
  {
    for (;;)
    {
      if (base.stop^)        // user requested service stop
      {
        TCP_close_server (ref tcp_server);
        wait_for_handler_completion (base);
        return 0;
      }

      rc = TCP_incoming_call (tcp_server, out tcp_client);

      if (rc == 0)                    // an incoming call
        break;

      if (rc != TCP_WAITING)
      {
        trace ("warning: TCP_incoming_call() returned %d\n", rc);
//        TCP_close_server (ref tcp_server);
//        wait_for_handler_completion (base);
//        return rc;
      }

      TCP_wait_server (ref tcp_server, nsecs => 1);
    }

    TCP_query_remote_address (tcp_client, out addr);
    ip_to_numericstr (addr.ip, out ipstr);

    // find a free handler slot

    for (i=0; i<(int)max_handlers; i++)
    {
      if (base.table^[i].base == null)
        break;
    }

    if (i == (int)max_handlers)
    {
      if (base.trace_calls^)
        trace ("error: refusing incoming call from %s port %u (handler table is full)\n",
               ipstr, addr.port);
      TCP_hangup (ref tcp_client);
      continue;
    }

    transfer_tcp_client (ref tcp_client, out base.table^[i].tcp_client);
    base.table^[i].base = &base;

    if (base.trace_calls^)
      trace ("handler %d : incoming call from %s port %u\n", i, ipstr, addr.port);

    if (incoming_call != null)
      incoming_call (tcp_client, i);


    // start a new thread that will handle the connection

    if (run connection_handler (&base.table^[i]) < 0)
    {
      trace ("error: cannot start thread for handler %d\n", i);
      TCP_hangup (ref base.table^[i].tcp_client);
      base.table^[i].base = null;
      continue;
    }
  }
}

//-----------------------------------------------------------------------------------------

public int read_tcpip_block (ref HANDLER_DATA p, out byte[] buffer, uint nsecs)
{
  uint    request_index;
  TIMER   timeout;
  int     tcp_rc;

  request_index = 0;

  set_timer (out timeout, nsecs);

  for (;;)
  {
    tcp_rc = TCP_receive (ref p.tcp_client, ref (&buffer)[0:buffer'length], ref request_index);

    // check for block completion
    if (request_index == buffer'size)
      break;

    // check for any network problem
    if (tcp_rc < 0)
    {
      if (p.trace_calls)
      {
        if (tcp_rc == TCP_CONNECTION_BROKEN)
          trace ("handler %d : read cancelled by broken connection\n", p.nr);
        else
          trace ("error: handler %d : TCP_receive() returned %d\n", p.nr, tcp_rc);
      }
      return -1;
    }

    /* check for timeout */
    if (timer_elapsed (timeout))    /* no data received for n secs */
    {
      if (p.trace_calls)
        trace ("handler %d : read cancelled after waiting %u secs\n", p.nr, nsecs);
      return -1;
    }

    TCP_wait (ref p.tcp_client, nsecs);
  }

  if (p.tcp_client.crypt.crypted)
    TCP_decrypt (ref p.tcp_client, ref buffer);

  return 0;
}

//-----------------------------------------------------------------------------------------

public int write_tcpip_block (ref HANDLER_DATA p, byte[] buffer, uint nsecs)
{
  uint    reply_index;
  TIMER   timeout;
  int     tcp_rc, rc;
  byte[]^ buf;
  byte    *pbuf;

  if (p.tcp_client.crypt.crypted)
  {
    buf = new byte[] ' (buffer);
    TCP_encrypt (ref p.tcp_client, ref buf^);
    pbuf = &buf^;
  }
  else
  {
    buf = null;  // avoids warning
    pbuf = &buffer;
  }


  reply_index = 0;

  set_timer (out timeout, nsecs);

  for (;;)
  {
    tcp_rc = TCP_send (ref p.tcp_client, pbuf[0:buffer'length], ref reply_index);

    // check for block completion
    if (reply_index == buffer'size)
    {
      rc = 0;
      break;
    }

    // check for any network problem
    if (tcp_rc < 0)
    {
      if (p.trace_calls)
      {
        if (tcp_rc == TCP_CONNECTION_BROKEN)
          trace ("handler %d : write cancelled by broken connection\n", p.nr);
        else
          trace ("error: handler %d : write_tcpip_block() returned %d\n", p.nr, tcp_rc);
      }
      rc = -1;
      break;
    }

    // check for timeout
    if (timer_elapsed (timeout))    // no data sent for n secs
    {
      if (p.trace_calls)
        trace ("handler %d : write cancelled after waiting %u secs\n", p.nr, nsecs);
      rc = -1;
      break;
    }

    TCP_wait (ref p.tcp_client, nsecs);
  }

  if (p.tcp_client.crypt.crypted)
    free (buf);

  return rc;
}

//-----------------------------------------------------------------------------------------
#end unsafe
//-----------------------------------------------------------------------------------------
