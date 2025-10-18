
// system.h : obtain system information

//--------------------------------------------------------------------------

void message_box (string title, string message);

//--------------------------------------------------------------------------

const int MAX_COMPUTERNAME_LENGTH = 32;

void get_computer_name (out string(MAX_COMPUTERNAME_LENGTH) computer_name);

//--------------------------------------------------------------------------

const int MAX_EXECUTABLE_FILENAME_LENGTH = 260;

void get_executable_filename (out string(MAX_EXECUTABLE_FILENAME_LENGTH) executable_filename);
void wget_executable_filename (out wstring(MAX_EXECUTABLE_FILENAME_LENGTH) executable_filename);

//--------------------------------------------------------------------------

const int MAX_USERID_LENGTH = 256;

void get_login_userid (out string(MAX_USERID_LENGTH) userid);

//--------------------------------------------------------------------------

// returns how many milliseconds the user did not type or move the mouse on his pc
// does not work on Windows Hyper-V Server 2012 R2
uint idle_msec ();

//--------------------------------------------------------------------------

// physical RAM on the PC (in bytes)

long get_total_physical_RAM ();

//--------------------------------------------------------------------------

// returns free memory (in bytes)

long get_free_memory ();

//--------------------------------------------------------------------------

// A number between 0 and 100 that specifies the approximate percentage of physical memory 
// that is in use (0 indicates no memory use and 100 indicates full memory use).

int get_virtual_memory_load ();

//--------------------------------------------------------------------------

// nb of CPUs

int get_nb_of_cores ();

//--------------------------------------------------------------------------

// retrieve operating system directory ("c:\windows")

void get_operating_system_directory (out char dir[260]);

//--------------------------------------------------------------------------

// retrieve user's 2- or 3-character country code, from ISO3166-1 or UN M.49

void get_country (out wchar[3] country);

//--------------------------------------------------------------------------

// get user's language and sublanguage

void get_language (out uint lang, out uint sublang);

const uint LANG_ALBANIAN                  = 0x1c;
const uint LANG_ARABIC                    = 0x01;
const uint LANG_BAHASA                    = 0x21;
const uint LANG_BULGARIAN                 = 0x02;
const uint LANG_CATALAN                   = 0x03;
const uint LANG_CHINESE                   = 0x04;
const uint LANG_CZECH                     = 0x05;
const uint LANG_DANISH                    = 0x06;
const uint LANG_DUTCH                     = 0x13;
const uint LANG_ENGLISH                   = 0x09;
const uint LANG_FINNISH                   = 0x0b;
const uint LANG_FRENCH                    = 0x0c;
const uint LANG_GERMAN                    = 0x07;
const uint LANG_GREEK                     = 0x08;
const uint LANG_HEBREW                    = 0x0d;
const uint LANG_HUNGARIAN                 = 0x0e;
const uint LANG_ICELANDIC                 = 0x0f;
const uint LANG_ITALIAN                   = 0x10;
const uint LANG_JAPANESE                  = 0x11;
const uint LANG_KOREAN                    = 0x12;
const uint LANG_NORWEGIAN                 = 0x14;
const uint LANG_POLISH                    = 0x15;
const uint LANG_PORTUGUESE                = 0x16;
const uint LANG_RHAETO_ROMAN              = 0x17;
const uint LANG_ROMANIAN                  = 0x18;
const uint LANG_RUSSIAN                   = 0x19;
const uint LANG_SERBO_CROATIAN            = 0x1a;
const uint LANG_SLOVAK                    = 0x1b;
const uint LANG_SPANISH                   = 0x0a;
const uint LANG_SWEDISH                   = 0x1d;
const uint LANG_THAI                      = 0x1e;
const uint LANG_TURKISH                   = 0x1f;
const uint LANG_URDU                      = 0x20;


const uint SUBLANG_NEUTRAL                = 0x00;    /* language neutral */
const uint SUBLANG_CHINESE_TRADITIONAL    = 0x01;    /* Chinese (Traditional) */
const uint SUBLANG_CHINESE_SIMPLIFIED     = 0x02;    /* Chinese (Simplified) */
const uint SUBLANG_DUTCH                  = 0x01;    /* Dutch */
const uint SUBLANG_DUTCH_BELGIAN          = 0x02;    /* Dutch (Belgian) */
const uint SUBLANG_ENGLISH_US             = 0x01;    /* English (USA) */
const uint SUBLANG_ENGLISH_UK             = 0x02;    /* English (UK) */
const uint SUBLANG_ENGLISH_AUS            = 0x03;    /* English (Australian) */
const uint SUBLANG_ENGLISH_CAN            = 0x04;    /* English (Canadian) */
const uint SUBLANG_ENGLISH_NZ             = 0x05;    /* English (New Zealand) */
const uint SUBLANG_ENGLISH_EIRE           = 0x06;    /* English (Irish) */
const uint SUBLANG_FRENCH                 = 0x01;    /* French */
const uint SUBLANG_FRENCH_BELGIAN         = 0x02;    /* French (Belgian) */
const uint SUBLANG_FRENCH_CANADIAN        = 0x03;    /* French (Canadian) */
const uint SUBLANG_FRENCH_SWISS           = 0x04;    /* French (Swiss) */
const uint SUBLANG_GERMAN                 = 0x01;    /* German */
const uint SUBLANG_GERMAN_SWISS           = 0x02;    /* German (Swiss) */
const uint SUBLANG_GERMAN_AUSTRIAN        = 0x03;    /* German (Austrian) */
const uint SUBLANG_ITALIAN                = 0x01;    /* Italian */
const uint SUBLANG_ITALIAN_SWISS          = 0x02;    /* Italian (Swiss) */
const uint SUBLANG_NORWEGIAN_BOKMAL       = 0x01;    /* Norwegian (Bokmal) */
const uint SUBLANG_NORWEGIAN_NYNORSK      = 0x02;    /* Norwegian (Nynorsk) */
const uint SUBLANG_PORTUGUESE_BRAZILIAN   = 0x01;    /* Portuguese (Brazilian) */
const uint SUBLANG_PORTUGUESE             = 0x02;    /* Portuguese */
const uint SUBLANG_SERBO_CROATIAN_LATIN   = 0x01;    /* Croato-Serbian (Latin) */
const uint SUBLANG_SERBO_CROATIAN_CYRILLIC= 0x02;    /* Serbo-Croatian (Cyrillic) */
const uint SUBLANG_SPANISH                = 0x01;    /* Spanish */
const uint SUBLANG_SPANISH_MEXICAN        = 0x02;    /* Spanish (Mexican) */
const uint SUBLANG_SPANISH_MODERN         = 0x03;    /* Spanish (Modern) */

//--------------------------------------------------------------------------
