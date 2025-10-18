
// ftp.h : file transfer protocol (client and server)

package FTP_CLIENT

  //--------------------------------------------------------------------------

  /* 'port' : can be set to 0 for default port 21. */
  /* returns a ftp_handle or a negative error.     */
  int ftp_open (string host, int port, string user, string pasw);

  /* returns 0 if OK, or a negative error */
  int ftp_close (int ftp_handle);

  //--------------------------------------------------------------------------

  // file types:
  const int TYPE_BINARY = 0x20;
  const int TYPE_ASCII  = 0x21;
  const int TYPE_EBCDIC = 0x22;

  //--------------------------------------------------------------------------

  /* attention: 1) any existing file is overwritten.                        */
  /*            2) the FTP protocol does not check for truncated files      */
  /*               so the transferred file is verified by reading it again. */
  /*            3) the connection should be closed after an error.          */

  int ftp_put (int    ftp_handle,
               string local_filename,
               string remote_filename,
               int    file_type = TYPE_BINARY);    /* see constants above */

  int ftp_get (int    ftp_handle,
               string remote_filename,
               string local_filename,
               int    file_type = TYPE_BINARY);    /* see constants above */

  //--------------------------------------------------------------------------

  /* note: on some systems, the new_remote_filename is deleted */
  /*       if already existing.                                */

  int ftp_rename (int    ftp_handle,
                  string remote_filename,
                  string new_remote_filename);

  int ftp_delete (int ftp_handle, string remote_filename);

  //--------------------------------------------------------------------------

  /* select current remote directory */
  int ftp_set_remote_dir (int ftp_handle, string remote_dir);

  int ftp_make_remote_dir (int ftp_handle, string remote_dir);
  int ftp_remove_remote_dir (int ftp_handle, string remote_dir);

  //--------------------------------------------------------------------------

  const int FTP_MAX_FILENAME_LENGTH  = 260;

  /* returns 0 if OK, or a negative error code. */
  int ftp_open_dir (int ftp_handle, string remote_dir);

  /* retrieve next file or directory name from the directory */
  /* returns 0 if OK, -1 if end-of-list.                     */
  int ftp_read_dir (int ftp_handle, out char filename[FTP_MAX_FILENAME_LENGTH]);

  int ftp_close_dir (int ftp_handle);

  //--------------------------------------------------------------------------

  /* possible error return codes : */
  const int FTP_TOO_MANY_CONNECTIONS       = (-1);
  const int FTP_CONNECTION_FAILED          = (-2);
  const int FTP_SERVICE_NOT_READY          = (-3);
  const int FTP_NO_FTP_SERVICE             = (-4);
  const int FTP_BAD_USER                   = (-5);
  const int FTP_BAD_PASSWORD               = (-6);
  const int FTP_CANNOT_ENABLE_EVENTS       = (-7);
  const int FTP_CANNOT_QUERY_LOCAL_ADDRESS = (-8);
  const int FTP_REPLY_BUFFER_FULL          = (-9);
  const int FTP_COMMAND_TIMEOUT            =(-10);
  const int FTP_REPLY_TIMEOUT              =(-11);
  const int FTP_BAD_FTP_HANDLE             =(-12);
  const int FTP_BAD_FILE_TYPE              =(-13);
  const int FTP_TYPE_SERVER_ERROR          =(-14);
  const int FTP_MODE_SERVER_ERROR          =(-15);
  const int FTP_CANNOT_CREATE_DATA_SERVER  =(-16);
  const int FTP_CANNOT_QUERY_PORT          =(-17);
  const int FTP_PORT_SERVER_ERROR          =(-18);
  const int FTP_BAD_REMOTE_FILENAME        =(-19);
  const int FTP_SERVER_NO_DATA_CONNECTION  =(-20);
  const int FTP_BAD_LOCAL_FILENAME         =(-21);
  const int FTP_FILE_READ_ERROR            =(-22);
  const int FTP_TRANSFER_FAILED            =(-23);
  const int FTP_DATA_TIMEOUT               =(-24);
  const int FTP_CONNECTION_BROKEN          =(-25);
  const int FTP_CONNECTION_ERROR           =(-26);
  const int FTP_FILE_WRITE_ERROR           =(-27);
  const int FTP_BAD_FILENAME               =(-28);
  const int FTP_BAD_NEW_FILENAME           =(-29);
  const int FTP_BAD_PATH                   =(-30);
  const int FTP_INTERN_ERROR               =(-31);
  const int FTP_OPEN_DIR_WAS_NOT_CLOSED    =(-32);   /* must call ftp_close_dir */
  const int FTP_NO_OPEN_DIR                =(-33);   /* must call ftp_open_dir */
  const int FTP_OUT_OF_MEMORY              =(-34);

  //--------------------------------------------------------------------------

end FTP_CLIENT;


package FTP_SERVER

  //--------------------------------------------------------------------------

  // 'home_directory'   : current directory at login; should end with '/';
  //                      must not exceed 511 bytes.
  // 'access_directory' : user cannot leave this directory; can be empty
  //                      string; must not exceed 511 bytes.
  // 'access_mode       : FTP_MODE_READ | FTP_MODE_WRITE or both.

  const int FTP_MODE_READ  = 1;
  const int FTP_MODE_WRITE = 2;

  // function that must be provided by the user.
  // must return 0 if ok, -1 if the user does not exist.
  typedef int FTP_PROFILE (    string user,
                           out string home_directory,
                           out string access_directory,
                           out int    access_mode);

  //--------------------------------------------------------------------------

  // function that must be provided by the user.
  // must return 0 if ok, -1 if the user does not exist or if the password is incorrect.

  typedef int FTP_CHECK_PASSWORD (string user,
                                  string password);

  //--------------------------------------------------------------------------

  // a variable 'bool^ pstop_service = new bool'(false); must be declared.
  // the function returns only when pstop_service^ is set to true.

  int ftp_server (FTP_PROFILE        ftp_profile,
                  FTP_CHECK_PASSWORD ftp_check_password,
                  int                listener_port = 21,  // use 0 for default port 21
                  bool^              pstop = null);       // poll variable 'pstop^' used to stop server

  //--------------------------------------------------------------------------

end FTP_SERVER;
