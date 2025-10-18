
// smtp.h : send e-mails using SMTP (Simple Mail Transfer Procotol) (rfc 821)

//--------------------------------------------------------------------------

struct SMTP;

//--------------------------------------------------------------------------

// 'host' : name or ip address of SMTP server.
// 'port' : port of SMTP server (specify 0 for default port 25).

// return codes :
//  >=0 : OK : a valid tcp/ip handle.
//   -1 : tcp/ip connection failed.
//   -2 : SMTP service is not available.

int smtp_open (out SMTP s, string host, int port);

//--------------------------------------------------------------------------

void smtp_close (ref SMTP s);

//--------------------------------------------------------------------------

// for debugging
// 'flag': true = enable tracing, false = disable tracing (default)

void smtp_trace (ref SMTP s, bool flag);

//--------------------------------------------------------------------------

// note: some mail servers require that a POP3 connection is
//       opened before opening a SMTP connection.

// once the connection with the SMTP server is established,
// the following commands must be executed in the given order
// for each mail that is to be sent :

//--------------------------------------------------------------------------

// start sending a new mail.

void smtp_begin_mail (ref SMTP s);

//--------------------------------------------------------------------------

// specify mail originator (for example "Smith@mail.com") (max 256 chars)

void smtp_from (ref SMTP s, string originator);

//--------------------------------------------------------------------------

// specify mail recipient (for example "Paul@wabadou.net") (max 256 chars)
// repeat this command to specify several recipients.

void smtp_to (ref SMTP s, string recipient);

//--------------------------------------------------------------------------

// begin the email's header

void smtp_begin_header (ref SMTP s);

//--------------------------------------------------------------------------

// specify the email header text.
// only printable 7-bit ascii characters (32 to 126) are allowed.
// this command can be repeated to specify several lines of text.

// The mail header must have a standard rfc822 form :

//    "Date:    Sat, 18 May 2002 06:02:42 +0200"   (date is optional)
//    "From:    Rik Smith <Smith@mail.com>"
//    "Subject: test"
//    "To:      Paul Wari <Paul@wabadou.net>"
//    "Cc:      erison <erik@ss.com>,john <wayne@ss.com>"

void smtp_text (ref SMTP s, string line);

//--------------------------------------------------------------------------

// end the email's header

void smtp_end_header (ref SMTP s);

//--------------------------------------------------------------------------

// specify only one body, in ascii text or in html
// this is the message of the email
// use "\r\n" as line separator, terminate with "\0"

const string MIME_TEXT = "text/plain; charset=\"iso-8859-1\"";
const string MIME_HTML = "text/html; charset=\"iso-8859-1\"";

void smtp_body (ref SMTP s, string mime, string text);

//--------------------------------------------------------------------------

// attach a file
// name:     "a.pdf"  (the name is visible in the received email)
// filename: "c:/temp/a.pdf"
// this function can be called several times.

const string MIME_PDF = "application/pdf";
const string MIME_JPG = "image/jpeg";

void smtp_attach_file (ref SMTP s, string mime, string name, string filename);

//--------------------------------------------------------------------------

// end the email and send it.
// returns 0 if OK, a negative value in case of error.

int smtp_end_mail (ref SMTP s);

//--------------------------------------------------------------------------

#if 0
// example.c

from std use tracing;
use smtp;

int send_email_on_smtp (ref SMTP h)
{
  int rc;

  smtp_begin_mail (ref h);

  smtp_from (ref h, "donotreply@mail.com");      // originator
  smtp_to   (ref h, "rec1@mail.com");   // recipient 1
  smtp_to   (ref h, "rec2@mail.com");   // recipient 2

  smtp_begin_header (ref h);
  smtp_text (ref h, "From: Automatic Generator <donotreply@mail.com>");
  smtp_text (ref h, "Subject: smtp test");
  smtp_text (ref h, "To: Alan Richardson <alan.richardson@mail.com>");
  smtp_text (ref h, "Cc: Rik Cuder <rik.cuder@mail.com>");
  smtp_end_header (ref h);

  smtp_body (ref h, MIME_HTML, "Hello !<br><br>"
                             + "<font size=5 color=red>THIS IS A VERY IMPORTANT MESSAGE !</font>");

  smtp_attach_file (ref h, MIME_PDF, "a4.pdf", "A4.PDF");
  smtp_attach_file (ref h, MIME_JPG, "a4.jpg", "a4.jpg");

  rc = smtp_end_mail (ref h);
  if (rc < 0)
  {
    trace ("error: smtp_end_mail() returned %d\n", rc);
    return -1;
  }

  return 0;
}

int main()
{
  int  rc;
  SMTP h;

  open_trace ("test.tra", 64*1024*1024);

  rc = smtp_open (out h, "smtp.netpost", 0);
  if (rc < 0)
  {
    trace ("error: smtp_open() returned %d\n", rc);
    return -1;
  }

  smtp_trace (ref h, true);   // enable tracing

  rc = send_email_on_smtp (ref h);
  if (rc < 0)
  {
    trace ("error: send_email_on_smtp() returned %d\n", rc);
    smtp_close (ref h);
    return -1;
  }

  smtp_close (ref h);

  return 0;
}
#endif
