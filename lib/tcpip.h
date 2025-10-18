
// tcpip.h : tcp/ip network protocol : - calling a remote computer
//                                     - accepting calls from remote computers

//---------------------------------------------------------------------------
// A SERVER waits for incoming calls on a (port nr).                       //
// A CLIENT connects to a remote SERVER defined by (ip address, port nr).  //
//---------------------------------------------------------------------------

/* for Android, add following line :

<manifest xlmns:android...>
 ...
 <uses-permission android:name="android.permission.INTERNET" />
 <application ...

*/
 
 
//---------------------------------------------------------------------------
// 1) Client functions for Client-Server communication
//---------------------------------------------------------------------------

struct TCP_CLIENT;

//---------------------------------------------------------------------------

// open a tcp/ip connection with timeout.
// returns 0 if OK or a negative error code.

int CS_connect (out TCP_CLIENT tcp_client,
                    string     ip,         // ip address or host name
                    int        port,       // port number (1 .. 65535)
                    uint       nsecs);     // timeout in seconds

//---------------------------------------------------------------------------

// read/write data with nsecs timeout.
// returns 0 if OK or a negative error code.

int CS_read  (ref TCP_CLIENT tcp_client, out byte[] buffer, uint nsecs);
int CS_write (ref TCP_CLIENT tcp_client, byte[] buffer, uint nsecs);

//---------------------------------------------------------------------------

// close tcp/ip connection.
// a runtime error occurs if tcp_client is invalid.

void CS_close (ref TCP_CLIENT tcp_client);

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// 2) Server functions for Client-Server communication
//---------------------------------------------------------------------------


struct TCP_SERVER;

//---------------------------------------------------------------------------

enum IPV_MODE
{
  MODE_IPV6,  // accept only IPV6 calls
  MODE_IPV4,  // accept only IPV4 calls
  MODE_DUAL,  // accept IPV6 and IPV4 calls
};

//---------------------------------------------------------------------------

// this structure is available within the user-written request handler

struct HANDLER_DATA
{
  TCP_CLIENT  tcp_client;  // client connection (can be used to query ip & port)
  int         nr;          // unique nr (0 .. max_handlers-1)
  bool        trace_calls;
  bool^       stop;        // ptr to 'stop' variable, set to true if we want to stop server
  byte[8]     user_data;   // can be used freely by the user
}

//---------------------------------------------------------------------------

typedef void SERVER_IS_READY (TCP_SERVER tcp_server);

// must return 0 if OK, -1 if we want to cancel the incoming call.
typedef int INCOMING_CALL (TCP_CLIENT tcp_client, int nr);

typedef void REQUEST_HANDLER (ref HANDLER_DATA p);

//---------------------------------------------------------------------------

// the following functions can be used inside a request handler to send/receive data:

// returns 0 if OK, -1 in case of a network problem or after nsecs timeout
int read_tcpip_block  (ref HANDLER_DATA p, out byte[] buffer, uint nsecs);
int write_tcpip_block (ref HANDLER_DATA p, byte[] buffer, uint nsecs);

//---------------------------------------------------------------------------

// This function creates a server that accepts remote calls on a port.
//
// It returns only in case of a network problem, or if the
//   variable 'stop^' is set to true.
//
// 'server_is_ready' is an optional user-written function that is called
//   once when the server has correctly initialized.
//
// when the server receives an incoming tcpip call :
//  . first it calls the function 'incoming_call',
//  . then it starts a handler thread on the function 'request_handler'.
//
// returns 0 if OK, or a negative tcp/ip error code.
//
// setting stop^ to true will make the server refuse new calls,
// it will wait until all handlers are closed and then the function returns.

int serve_requests (int             port,                        // port to listen on
                    REQUEST_HANDLER request_handler,             // user-define function to handle a call
                    uint            max_handlers    = 512,       // max open connections
                    IPV_MODE        mode            = MODE_IPV6, // accepted protocols
                    bool^           stop            = null,      // poll variable 'stop' used to stop server
                    bool^           trace_calls     = null,
                    SERVER_IS_READY server_is_ready = null,
                    INCOMING_CALL   incoming_call   = null,
                    byte[8]         user_data       = {all=>0});

//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// 3) Low-Level tcp/ip functions
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

// all IP addresses are stored in 16 bytes.

typedef byte[16] IP;

//---------------------------------------------------------------------------

// IPV4 addresses are stored in the last 4 bytes of the IP structure,
// with the 12 first bytes containing IPV4_PREFIX.

const byte[12] IPV4_PREFIX = {0,0,0,0,0,0,0,0,0,0,0xFF,0xFF};

//---------------------------------------------------------------------------

// converts a DNS name into an array of IP.

// the array 'ip' is allocated on heap and must be freed by the caller after use;
// it contains 0 elements if the name is invalid or was not found.

void dns_to_ip (string name, out IP[]^ ip);

//---------------------------------------------------------------------------

// convert an IP to a host name.
// returns 0 if a name was found, -1 if none was found
// BEWARE: this function is very slow (4 to 15 seconds !)

int ip_to_dns (IP ip, out char[1024] name);

//---------------------------------------------------------------------------

// convert an IP to a numeric text representation in canonical form.

void ip_to_numericstr (IP ip, out char[48] ipstr);

//---------------------------------------------------------------------------

// tests if ip begins with 12-byte IPV4_PREFIX.

bool is_ipv4 (IP ip);

//---------------------------------------------------------------------------

// tests if ip does not begin with 12-byte IPV4_PREFIX.

bool is_ipv6 (IP ip);

//---------------------------------------------------------------------------

// test if ip denotes this computer.
// (IPV6 ::1 or IPV4 127.x.x.x)

bool is_loopback (IP ip);

//---------------------------------------------------------------------------

// tests if ip is private.
// private IP's are usable only on the local LAN, not on the internet.
// (IPV4 10.x.x.x, 169.254.x.x, 172.16-31.x.x, 192.168.x.x, IPV6 fc00::/7, fe80::/10)

bool is_private (IP ip);

//---------------------------------------------------------------------------

// Connect to a remote server.
//
// Returns a client handle >= 0 if ok, or a negative error code :
//   TCP_INVALID_PORT        : port must not be zero.
//   TCP_INVALID_IP_ADDRESS  : invalid ip address.
//   TCP_NO_SERVER_LISTENING : no server listening on port.
//   TCP_CONNECTION_FAILED   : could not connect.
//   TCP_UNREACHABLE         : remote server is unreachable.
//   or any implementation-dependant error.

int TCP_connect (out TCP_CLIENT tcp_client,
                     IP         ip,        // IP address
                     int        port,      // port number (1 .. 65535)
                     uint       nsecs);    // timeout in seconds

//---------------------------------------------------------------------------

packed struct PEER
{
  IP     ip;    // IPV6, or IPV4 with IPV4_PREFIX.
  uint2  port;
}

//---------------------------------------------------------------------------

// Retrieve ip address and port number of local or remote peer.
//
// These functions are only allowed after a connection was established,
// otherwise a runtime error occurs.
//
// Returns 0 if ok, or a negative error code.
//
// Parameters:
//   tcp_client : returned by TCP_connect() or TCP_incoming_call().
//
// Bug:
// . TCP_query_local_address() is not reliable and should be avoided.

int TCP_query_local_address (TCP_CLIENT tcp_client, out PEER info);
int TCP_query_remote_address (TCP_CLIENT tcp_client, out PEER info);

//---------------------------------------------------------------------------

// Set the NO_DELAY option to turn off bufferization of sent data.
//
// By default, TCP/IP is delaying the sending of data as
// much as possible to be able to send large-size packets.
// Setting this option can be interesting if small amounts
// of data must be sent in real-time (without delay),
// for example a mouse position.
//
// This function is only allowed after a connection was established,
// otherwise a runtime error occurs.
//
// Parameters:
//   tcp_client : returned by TCP_connect() or TCP_incoming_call().
//
// Returns 0 if ok, or a negative error code.

int TCP_no_delay (TCP_CLIENT tcp_client);

//---------------------------------------------------------------------------

// TCP_send ()
//
// Send 'buffer' from index 'buffer_index' to 'buffer'size-1'.
// The function always returns immediately.
//
// Parameters:
//   tcp_client   : returned by TCP_connect() or TCP_incoming_call().
//   buffer       : buffer to send
//   buffer_index : index within buffer (read/write parameter)
//
// Note:
//   buffer_index is a read/write parameter. It must be set to 0 before
//   the call. The function will increase buffer_index so that it always
//   indicates the index of the remaining buffer bytes to be sent.
//   The function will possibly send only part of the buffer or even
//   no bytes at all. It should then be called again and again until
//   buffer_index equals buffer_size, or until the caller doesn't want to
//   send the remaining buffer data (timeout, ..)
//
// Returns 0 if ok, or a negative error code :
//   TCP_INVALID_BUFFER_INDEX : violation of 'buffer_index < buffer'size'
//   TCP_CONNECTION_BROKEN    : connection was broken by remote computer
//
// Note:
// . a zero return code is no guarantee that the sent blocks have arrived
//   correctly.

int TCP_send (ref TCP_CLIENT tcp_client,
                  byte[]     buffer,
              ref uint       buffer_index);

//---------------------------------------------------------------------------

//
// TCP_receive()
//
// Receive data into 'buffer' from index 'buffer_index' to 'buffer'size-1'.
// The function always returns immediately.
//
// Parameters:
//   tcp_client   : returned by TCP_connect() or TCP_incoming_call().
//   buffer       : buffer for received data
//   buffer_index : index within buffer (read/write parameter)
//
// Note:
//   buffer_index is a read/write parameter. It must be set to 0 before
//   the call. The function will increase buffer_index so that it always
//   indicates the index of the first free byte of the buffer.
//   The function will possibly receive only part of the buffer or even
//   no bytes at all. It should then be called again and again until
//   buffer_index equals buffer_size, or until the caller doesn't want
//   to receive the remaining data (timeout, short block, ..)
//
// Returns 0 if ok, or a negative error code :
//   TCP_INVALID_HANDLE       : invalid parameter 'handle'
//   TCP_INVALID_BUFFER_INDEX : violation of '*buffer_index < buffer_size'
//   TCP_NOT_CONNECTED        : connection was not established
//   TCP_CONNECTION_BROKEN    : connection was broken by remote computer

int TCP_receive (ref TCP_CLIENT tcp_client,
                 ref byte[]     buffer,
                 ref uint       buffer_index);

//---------------------------------------------------------------------------

void TCP_wakeup (ref TCP_CLIENT tcp_client);

//---------------------------------------------------------------------------

// waits until a network event occurs,
// for example data arrives or the connection was broken.
// returns 0 if client had an event, -1 if timeout.

int TCP_wait (ref TCP_CLIENT tcp_client, uint nsecs);

//---------------------------------------------------------------------------

// Close the connection and free the client resources
//
// Parameters:
//   tcp_client : returned by TCP_connect() or TCP_incoming_call().
//
// Note:
// . tcp_client becomes invalid after the call.

void TCP_hangup (ref TCP_CLIENT tcp_client);

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

//
// Create a server that waits for incoming client calls.
//
// The function returns immediately.
//
// TCP_incoming_call() must be called regularly to check
// if there are new incoming calls.
//
// Parameters:
//   server_port : port number to listen on (usually between 1024 and 4999)
//   mode        : see IPV_MODE above.
//
// Returns a server handle >= 0 if ok, or a negative error code :
//   TCP_INVALID_PORT     : port 0 is not allowed.
//   TCP_PORT_IN_USE      : port is already in use.
//   or any implementation-dependant error.

int TCP_create_server (out TCP_SERVER tcp_server,
                           uint2      port,             // port 1 to 65535
                           IPV_MODE   mode = MODE_IPV6);

//---------------------------------------------------------------------------

//
// Check if there are incoming calls on a server.
//
// The function returns immediately.
//
// Parameter:
//   tcp_server : returned by TCP_create_server().
//   tcp_client : is initialized in case of incoming call.
//
// Returns 0 in case of an incoming call, or a negative error code:
//   TCP_WAITING : if there are no incoming calls (this is not really an error),
//   or any implementation-dependant error.

int TCP_incoming_call (TCP_SERVER tcp_server, out TCP_CLIENT tcp_client);

//---------------------------------------------------------------------------

// waits until the server receives a call or some other event, or timeout occurs.
// returns 0 if server had an event, -1 if timeout.

int TCP_wait_server (ref TCP_SERVER tcp_server, uint nsecs);

//---------------------------------------------------------------------------

// move the TCP_CLIENT structure from 'source' to 'target'.
// 'source' is cleared.

void transfer_tcp_client (ref TCP_CLIENT source, out TCP_CLIENT target);

//---------------------------------------------------------------------------

//
// Close a server.
//
// Parameter:
//   tcp_server : returned by TCP_create_server().
//
// Returns 0 if ok, or a negative error code.
//
// Note:
// . server_handle becomes invalid after the call.

void TCP_close_server (ref TCP_SERVER tcp_server);

//---------------------------------------------------------------------------

/**************************************************************************/
/* The following functions are implemented through udp datagrams.         */
/* They are completely distinct from the client/server operations         */
/* described above.                                                       */
/**************************************************************************/

//---------------------------------------------------------------------------

struct UDP_ENDPOINT;

struct UDP_LOCAL  // info about local peer IP (useful in case of several interfaces)
{
  byte[40] buf;
  uint4    len;
}

//---------------------------------------------------------------------------

// port is usually between 1 and 65535,
// or use 0 to let the network layer decide (usually for clients).

int UDP_create_endpoint (out UDP_ENDPOINT udp,
                             uint2        port = 0,
                             IPV_MODE     mode = MODE_IPV6);

//---------------------------------------------------------------------------

// return 0 if a block arrived or a negative error code.
// returns TCP_WAITING if no new block arrived : you must then retry later.
// buffer'size should be at least 548 bytes for IPV4, or 1232 bytes for IPV6.

int UDP_receive (ref UDP_ENDPOINT  udp,
                 out byte[]        buffer,
                 out uint          actual_buffer_length,
                 out PEER          origin,
                 out UDP_LOCAL     local);

//---------------------------------------------------------------------------

// returns 0 if ok, or a negative error code.

int UDP_set_destination (ref UDP_ENDPOINT udp,
                             PEER         peer,    // with port between 1 and 65535
                             UDP_LOCAL    local);

//---------------------------------------------------------------------------

// returns 0 if ok, or a negative error code.
// returns TCP_WAITING if the output buffers are full : you must then retry later.
// buffer'size should not exceed 548 bytes for IPV4, or 1232 bytes for IPV6.

int UDP_send (ref UDP_ENDPOINT udp,
                  byte[]       buffer);

//---------------------------------------------------------------------------

// wait until we can send or receive further blocks
// returns 0 if further send or receive are possible, -1 if timeout.

int UDP_wait (ref UDP_ENDPOINT udp, uint millisecs);

//---------------------------------------------------------------------------

// signal UDP layer that we have further blocks to send
// (wakes up UDP_wait() if output buffers are not full)

void UDP_wakeup (ref UDP_ENDPOINT udp);

//---------------------------------------------------------------------------

void UDP_close (ref UDP_ENDPOINT udp);

//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// cryption
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// notes :
// 1) cryption slows down tcp/ip transfer, especially on 100Mbit/s LANs.
// 2) a new key should be computed dynamically for each new connection.
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

// enable/disable cryption
void TCP_enable_crypt (ref TCP_CLIENT tcp_client, bool on);

//---------------------------------------------------------------------------

bool TCP_is_crypted (ref TCP_CLIENT tcp_client);

//---------------------------------------------------------------------------

void TCP_set_key (ref TCP_CLIENT tcp_client, byte[16] key);

//---------------------------------------------------------------------------

// must be called just before TCP_send

void TCP_encrypt (ref TCP_CLIENT tcp_client, ref byte[] data);

//---------------------------------------------------------------------------

// must be called just after TCP_receive

void TCP_decrypt (ref TCP_CLIENT tcp_client, ref byte[] data);

//---------------------------------------------------------------------------

const int TCP_WAITING              = -20000;  // connection not yet established
const int TCP_INVALID_PORT         = -20001;  // port 0 is not allowed
const int TCP_PORT_IN_USE          = -20003;  // port is already used
const int TCP_DRIVER_PROBLEM       = -20004;  // missing WS2_32.dll
const int TCP_NO_IP6               = -20005;  // no ipv6 supported
const int TCP_NO_IP4               = -20006;  // no ipv4 supported
const int TCP_NO_SERVER_LISTENING  = -20008;  // nobody listens on port
const int TCP_CONNECTION_FAILED    = -20009;  // connection attempt failed
const int TCP_INVALID_BUFFER_INDEX = -20012;  // invalid param. 'buffer_index'
const int TCP_CONNECTION_BROKEN    = -20013;  // remote peer broke connection
const int TCP_INVALID_IP_ADDRESS   = -20014;  // bad remote server ip address
const int TCP_UNREACHABLE          = -20015;  // server is not reachable (ping)
const int TCP_BUFFER_TOO_LARGE     = -20016;  // size should not exceed 512
const int TCP_BUFFER_TOO_SMALL     = -20017;  // reception buffer was too small
// other return codes defined in <errno.h> or <winsock2.h> are possible.

//---------------------------------------------------------------------------
