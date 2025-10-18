
// http.h : http protocol, client and server

use calendar, tcpip, url;

//-----------------------------------------------------------------------------------

package HTTP_CLIENT

  //-----------------------------------------------------------------------------------

  struct HTTP_PARAMETERS
  {
    bool use_proxy;
    char proxy_ip[64];
    int  proxy_port;
    int  connect_timeout;           // 0 = default value of 5 secs
    int  receive_timeout;           // 0 = default value of 120 secs
    bool accept_incomplete_files;
    bool trace;
    long max_file_size;   // returns HTTP_FILE_WRITE_ERROR if file limit exceeded (0=no limit)
  }

  struct HTTP_HEADER
  {
    int                version;       // HTTP version (100*major+minor) (100 or 101)
    char               code[3];       // "200"=OK,"301"or"302"=redirect,"400"= not found,..
    char               msg[31];       // error message in clear text. ex: "OK"
    char               server[48];    // "Microsoft-IIS/4.0", ...
    char               connect[15];   // "close"
    char               location[256]; // redirection URL (in case of code 301/302)
    char               type[32];      // "text/html", "image/gif", "image/jpeg", ..
    char               coding[32];    // "chunked"
    calendar.DATE_TIME date;          // file's GMT date (or zeroes).
    long               size;          // file's size as indicated in header (or -1).
    long               filesize;      // actual size of file
  }

  // Caution ! The above fields are for information only. There is no
  // guarantee that any of them will be initialized by the server.
  // If unavailable, char fields are set to an empty string, 'version'
  // and 'size' are set to -1, date components are set to zero.

  //-----------------------------------------------------------------------------------

  // In case 'filename' already exists, its creation date is sent
  // to the server who will send a file only if it's more recent.

  // In case HTTP_REDIRECT is returned, the caller should follow the
  // redirection URL specified in http.location

  // this function is thread-safe.

  int get_http_file (    url.URL         url,       // URL declared in url.h
                         URL             base,      // referer URL - can be cleared
                         HTTP_PARAMETERS param,
                         string          filename,
                     out HTTP_HEADER     http);

  //-----------------------------------------------------------------------------------

  void reset_http_service ();       // close all open connections

  //-----------------------------------------------------------------------------------

  const int HTTP_CONNECT_ERROR     =  -1;   // cannot connect to server
  const int HTTP_SEND_ERROR        =  -2;   // error sending data to server
  const int HTTP_RECEIVE_ERROR     =  -3;   // error during receive
  const int HTTP_TIMEOUT_ERROR     =  -4;   // timeout during receive
  const int HTTP_WRONG_SIZE        =  -5;   // truncated data block received
  const int HTTP_ERROR_CODE        =  -6;   // see http.code
  const int HTTP_NO_DATA           =  -7;   // no data received -> no file
  const int HTTP_FILE_WRITE_ERROR  =  -8;   // error writing to file
  const int HTTP_LONG_DATA         =  -9;   // too much data received
  const int HTTP_INTERN_ERROR      = -10;   // intern error
  const int HTTP_FILE_CREATE_ERROR = -11;   // cannot create file
  const int HTTP_FILE_CLOSE_ERROR  = -12;   // error closing file
  const int HTTP_BAD_CHUNK         = -13;   // bad chunk format
  const int HTTP_REDIRECT          = -14;   // redirect to URL http.location

  //-----------------------------------------------------------------------------------

end HTTP_CLIENT;

//---------------------------------------------------------------------------


//---------------------------------------------------------------------------

package HTTP_SERVER

  //---------------------------------------------------------------------------

  struct ARG_INFO;   // opaque type

  //---------------------------------------------------------------------------

  // user must test the url and return true if the page is virtual

  typedef bool IS_VIRTUAL_PAGE (int        nr,
                                TCP_CLIENT tcp_client,
                                string     url,      // virtual url
                                ARG_INFO   args);    // http arguments

  //---------------------------------------------------------------------------

  // user must send virtual html page on tcp_client connection

  typedef void HANDLE_VIRTUAL_PAGE (    int        nr,
                                    ref TCP_CLIENT tcp_client,
                                        string     url,      // virtual url
                                        ARG_INFO   args);    // http arguments

  //---------------------------------------------------------------------------

  // user must test if variables must be translated for this page

  typedef bool MUST_TRANSLATE_VARIABLES (int        nr,
                                         TCP_CLIENT tcp_client,
                                         string     url,      // virtual url
                                         ARG_INFO   args);    // http arguments

  //---------------------------------------------------------------------------

  typedef void TRANSLATE_VARIABLE (    int        nr,
                                       TCP_CLIENT tcp_client,
                                       string     url,      // virtual url
                                       ARG_INFO   args,     // http arguments
                                       char[64]   variable,
                                   out char[1024] value);

  //---------------------------------------------------------------------------

  // user must send virtual html page on tcp_client connection

  typedef void HANDLE_POST_REQUEST (    int        nr,
                                    ref TCP_CLIENT tcp_client,
                                        string     url,      // virtual url, ex: "/getlist"
                                        ARG_INFO   args);    // post & http arguments

  //---------------------------------------------------------------------------

  // user must test if (userid, password) is valid for this url
  // and fill realm if not.

  typedef bool HANDLE_LOGIN (    int        nr,
                                 TCP_CLIENT tcp_client,
                                 string     url,      // virtual url, ex: "/home.html"
                                 string     userid,
                                 string     password,
                             out char[64]   realm);   // must be completed if returning false

  //---------------------------------------------------------------------------

  // user must sleep to slowdown throughput

  typedef void CONTROL_SEND_FLUX (int nb_bytes);

  //---------------------------------------------------------------------------

  typedef void ADD_REPLY_HEADERS (    int        nr,         // handler nr
                                      TCP_CLIENT tcp_client,
                                      string     url,        // virtual url, ex: "/getlist"
                                      ARG_INFO   args,       // post & http arguments
                                  out char[4096] headers);   // must be filled with parameters

  //---------------------------------------------------------------------------

  struct HTTP_SERVER_DATA
  {
    // directory containing html pages (must end with /)

    char html_directory[256];     // ex: "c:/html/"


    // start URL

    char start_url[64];           // ex: "/home.htm"


    // force some pages to be virtual and thus treated by the next function;
    // returns true if the page is to be treated as virtual,
    // or false for the usual standard treatment (display file in www tree)

    IS_VIRTUAL_PAGE      is_virtual_page;


    // treat pages in virtual url directory "/virtual/..."

    HANDLE_VIRTUAL_PAGE  handle_virtual_page;


    // this function is called for all non-virtual files;
    // it must return true if $var$ variables must be handled, false if not.

    MUST_TRANSLATE_VARIABLES   must_translate_variables;


    // this function is called for each variable of the form $VAR$
    // that needs to be translated into a value.

    TRANSLATE_VARIABLE    translate_variable;


    // treat post requests triggered by buttons.

    HANDLE_POST_REQUEST   handle_post_request;


    // login handler
    // must return true if (userid, password) are accepted,
    // must return false if login refused (+ complete realm)

    HANDLE_LOGIN    handle_login;


    // tracing flag

    bool    tracing;


    // flux regulator (to limit sent bytes)
    // will be called before sending byte chunks

    CONTROL_SEND_FLUX  control_send_flux;


    // add user-defined parameters at the end of the http reply header
    // example: "Set-Cookie: x=y\r\nCache-Control: max-age=3600\r\n"
    // several parameters can be concatenated, each one must end with "\r\n"

    ADD_REPLY_HEADERS   add_reply_headers;


    // user-data

    int user_data;
  }

  //---------------------------------------------------------------------------

  struct HTTP_INITIAL_DATA
  {
    // initial request data (in case some bytes already received)

    uint    initial_received_data_size;   // 0 .. 8, set to 0 by call
    byte[8] initial_received_data;
  }

  //---------------------------------------------------------------------------

  // handle one http call (tcp/ip connection must be open)

  void handle_http_call (ref HANDLER_DATA      p,   // from tcp/ip server
                             HTTP_SERVER_DATA  h,   // http server config
                         ref HTTP_INITIAL_DATA i);  // in case some bytes already read

  //---------------------------------------------------------------------------

  // used to retrieve query parameter and value  (ex: http://host/?parameter1=value1&parameter2=value2
  // returns 0 if found, -1 if not found

  int get_query_string_pair (    ARG_INFO args,          // http arguments
                                 string   parameter_name,
                             out string   value,
                                 string   default_value = "");

  //---------------------------------------------------------------------------

  // used to retrieve http parameters (ex: "Referer", "Host", "Cookie")
  // all http parameters together are limited to 8K

  void get_http_parameter (    ARG_INFO args,          // http arguments
                               string   parameter_name,
                           out string   value,
                               string   default_value = "");

  //---------------------------------------------------------------------------

  // this function can be called when handling a is_virtual_page() call.
  // its purpose is to test if a physical file exists for the url.

  bool get_http_physical_file_exists (ARG_INFO args);

  //---------------------------------------------------------------------------

  // used to retrieve form fields when handling a post request

  void get_post_parameter (    ARG_INFO args,          // post arguments
                               string   parameter_name,
                           out string   value,
                               string   default_value = "");

  //---------------------------------------------------------------------------

  // used to copy large form results (uploaded files or textareas)
  // to a local file when handling a post request.
  // returns 0 if OK, -1 if parameter not found or file error.

  int copy_post_parameter_to_file (ARG_INFO args,
                                   string   parameter_name,
                                   string   local_filename);

  //---------------------------------------------------------------------------

  // used to retrieve original client filename for file uploads (Type="File")
  // returns 0 if OK, -1 if no file parameter found.
  // note: the filename should be carefully modified before any use.

  int get_post_parameter_filename (    ARG_INFO args,
                                       string   parameter_name,
                                   out string   result_filename);

  //---------------------------------------------------------------------------

  /*
  Features :

  1. Variables
  ----------------
  html pages can contain variables of the form $VAR$.
  These are replaced by their corresponding value by providing a
  handler for 'translate_variable'.


  2. Virtual Pages
  ----------------
  Virtual URL references (as defined by is_virtual()) can be handled
  by the special handler 'handle_virtual_page'. The user must write
  a handler that sends, using CS_write() defined in tcpc.h,
  a document matching the extension of the virtual url.

  For example the handler must send :

     "<HTML><HEAD><TITLE>MY TITLE</TITLE></HEAD>\r\n"
     "<BODY>"
     " ... some text ..."
     "</BODY></HTML>"

  note: if "... some text ..." can contain the characters <, >, &, ', ",
        it should first be converted into html items like "&lt;", "&gt;"
        by calling expand_html().

  For "xxx.gif", a GIF image must be sent.
  For "xxx.jpg", a JPEG image must be sent.


  3. POST requests
  ----------------
  Here is the HTML syntax for a field "PAR1" and a button "SEARCH" :

      <FORM Method="POST" Action="/getlist">
      <INPUT Type="text" Name="PAR1" Size="10" MaxLength="10" Value="">
      <INPUT Type="submit" Value=" SEARCH ">
      </FORM>

  Pushing the button triggers a POST request that can be handled
  by the special handler 'handle_post_request'. This handler must :

    a) query the value of the field "PAR1" as follows :

         get_post_parameter (args, "PAR1", out par1);

    b) send, using CS_write() defined in tcpc.h, a reply HTML document :

         "<HTML><HEAD><TITLE>MY TITLE</TITLE></HEAD>\r\n"
         "<BODY>"
         " ... some text ..."
         "</BODY></HTML>"

  If the form data exceeds 32K or contains user files for upload or textareas,
  the following syntax must be used :

  <FORM Method="POST" Enctype="Multipart/Form-data" Action="/upload.html">
  Your name :   <INPUT Name="user-name" Type="Text" Size="32" MaxLength="32" Value="">
  Send file 1 : <INPUT Name="user-file1" Type="File">
  Send file 2 : <INPUT Name="user-file2" Type="File">
  Your Comment : <TEXTAREA Name="mon-texte" Rows="10" Cols="80"></TEXTAREA>
  <INPUT Type="Submit" Value="envoyez-moi tout">
  </FORM>

  Large result fields (uploaded files or TEXTAREAS) must be retrieved in files as follows :

    copy_post_parameter_to_file (args, "user-file1", "c:/local-user-file");

  For file uploads (Type="File"), the original client filename can be retrieved as follows :

    get_post_parameter_filename (args, "user-file1", out buf);


  4. LOGIN
  --------
  HTML directory trees can be protected by requiring a user/password
  login. When providing such a handler, it is called for each client
  request. The handler must check the profile (userid, password)
  according to the requested url and return 0 if OK, -1 if not OK.
  In case of -1, 'realm' must be filled with the application name
  to be presented to the user.
  Different userid/password profiles can co-exist for different urls.

  */

end HTTP_SERVER;

//---------------------------------------------------------------------------

// replaces all <, >, ", & and ' characters by html items like "&lt;"
// to avoid confusion with the html syntax.
// note: the target string can become 5 times longer than the source.

void expand_html (string source, out string target);

//---------------------------------------------------------------------------
