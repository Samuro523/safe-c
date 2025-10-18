
// url.h : parse URL's

//---------------------------------------------------------------------------

const int URL_PROTOCOL_LENGTH  =  256;
const int URL_HOST_LENGTH      =  256;
const int URL_FILENAME_LENGTH  =  260;
const int URL_PARAMETER_LENGTH = 1024;

struct URL
{
  char protocol[URL_PROTOCOL_LENGTH];
  char host[URL_HOST_LENGTH];
  int  port;
  char filename[URL_FILENAME_LENGTH];    // empty or begins with '/'
  char parameter[URL_PARAMETER_LENGTH];  // empty or begins with '#', ';' or '?'
}

// example1: "http://www.host.com:80/dir1/dir2/file.cgi?a=1&b+c=dir/f.txt"
// example2: "http://www.host.com/dir1/dir2/file.htm#Here"
// example3: "http://www.host.com/dir1/dir2/file.htm/subdir/a.txt"


//---------------------------------------------------------------------------
// part 1 : convert "http://site/page.html" string to url.
//---------------------------------------------------------------------------

// parses a href that is always absolute, ex: (http://www.site.com/dir)
// note: removes all ".." and "." components.
// note: %xx in filename and parameter are translated into simple characters.
//       but values below %20 or equal %7f return an error.
//
// returns 0 if OK or a negative error code.

int make_absolute_url (string href, out URL url);

//---------------------------------------------------------------------------

// parse a href that is either absolute, or relative to a base URL.
// note: removes all ".." and "." components.
// note: %xx in filename and parameter are translated into simple characters.
//       but values below %20 or equal %7f return an error.
//
// returns 0 if OK or a negative error code.

int make_relative_url (string href, URL base, out URL url);

//---------------------------------------------------------------------------

const int URL_PROTOCOL_TOO_LONG      = -1;
const int URL_BAD_SYNTAX             = -2;
const int URL_BAD_HOSTNAME           = -3;
const int URL_HOSTNAME_TOO_LONG      = -4;
const int URL_BAD_PORT_NUMBER        = -5;
const int URL_FILENAME_TOO_LONG      = -6;
const int URL_INCOMPLETE             = -7;   // missing host or port
const int URL_NOT_ABSOLUTE           = -8;   // is not an absolute URL
const int URL_CANNOT_NORMALIZE       = -9;   // cannot remove ".."
const int URL_PARAMETER_TOO_LONG     = -10;

//---------------------------------------------------------------------------
// part 2 : convert absolute url filename to local filename.
//---------------------------------------------------------------------------

// converts an internet url.filename into a filename
// that is safe to be used on the local filesystem.
// The result filename always begins with '/'.
//
// The function converts each simple filename (enclosed by '/') as follows :
//  . all leading spaces or dots are not allowed -> replace by %xx.
//  . device names "con", "aux", "prn", "nul" are not allowed,
//    either alone or followed by a dot -> replace first char.
//  . device names "com", "lpt" followed by a digit are not allowed,
//    either alone or followed by a dot -> replace first char.
//  . the chars \/:*?"<>| are not allowed -> to be replaced.
//  . ascii codes 1 .. 31 are not allowed -> to be replaced.
//  . all trailing spaces or dots are not allowed -> replace all.
// finally it appends default_filename if it ends with '/'.
//
// returns 0 if OK or a negative error code.

int convert_url_to_local_filename (URL           url,
                                   out char[260] filename,
                                   string        default_filename = "dir.html");

//---------------------------------------------------------------------------

const int CNV_NOT_ABSOLUTE = -1;
const int CNV_TOO_LONG     = -2;

//---------------------------------------------------------------------------
