
/* inifile.h : read .ini file */

//--------------------------------------------------------------------------

/* retrieve parameter from .ini file                     */
/* returns 0 or a negative error code.                   */
/* in case of error, the default value (if any) is used. */

int get_parameter (    string  filename,        /* param.ini    */
                       string  section,         /* section name */
                       string  key,             /* key name     */
                   out string  parameter,       /* out buffer   */
                       string  default_value = "");

//--------------------------------------------------------------------------

/* retrieve nth section of the file.   */
/* returns 0 or a negative error code. */

int get_nth_section (    string  filename,         /* ini filename  */
                         int     nth,              /* 0 .. ?        */
                     out string  section);         /* out buffer    */

//--------------------------------------------------------------------------

/* retrieve nth key within given section of the file. */
/* returns 0 or a negative error code.                */

int get_nth_key (    string  filename,  /* ini filename  */
                     string  section,   /* section name  */
                     int     nth,       /* 0 .. ?        */
                 out string  key);      /* out buffer    */

//--------------------------------------------------------------------------

/* retrieve nth parameter within given section of the file. */
/* returns 0 or a negative error code.                      */

int get_nth_parameter (    string  filename,   /* ini filename  */
                           string  section,    /* section name  */
                           int     nth,        /* 0 .. ?        */
                       out string  parameter); /* out buffer    */

//--------------------------------------------------------------------------

/* signal that initialization file was changed and must be reloaded. */

void signal_ini_file_changed ();

//--------------------------------------------------------------------------

const int E_INI_FILE_NOT_FOUND    = (-1);  /* cannot open .ini file */
const int E_INI_SECTION_NOT_FOUND = (-2);
const int E_INI_KEY_NOT_FOUND     = (-3);
const int E_INI_BUFFER_TOO_SMALL  = (-4);  /* output value was truncated   */
const int E_INI_BAD_INDEX         = (-5);  /* parameter 'nth' out of range */

//--------------------------------------------------------------------------

/* format of .ini file :

# this is a comment !

[section1]
key1 = parameter1
key2 = parameter2   ; comment

[section2]
key1 =             ; empty
test = a;;b        ; value "a;b"

Notes:
.  max line length in the file is 4096 chars.
. '#' or ';' characters are used to start a comment.
. '#' or ';' characters must be doubled within a parameter value
             to indicate the corresponding single character.
. keys are case insensitive, parameter values are not.

*/

//--------------------------------------------------------------------------
