
// odbc.c : interface to SQL database using odbc 3.0

use arithm, strings, thread, tracing, win/windows;

#begin unsafe
/**********************************************************************/

struct SQL
{
  int     state;      // connect state (0 to 5)
  HDBC    hdbc;
  HSTMT   hstmt;
  bool    trace;
  BIND[]^ bind;
  byte*   ptr;
  uint    row_size;
}

HENV          henv;              // only 1 handle per process
int           nb_henv_users;     // nb users using henv
SHARED_OBJECT o;                 // protects henv

SQLLEN g_indicator;      // dummy field receiving the NULL/length indicator

/**********************************************************************/

void trace_message (SWORD typhandle, SQLHANDLE handle, string cmd)
{
  SQLCHAR       SqlState[6], Msg[512];
  SQLINTEGER    NativeError;
  SQLSMALLINT   i, MsgLen;
  int           rc;

  i = 1;
  for (;;)
  {
    rc = SQLGetDiagRecA (typhandle, handle, i, &SqlState, &NativeError, &Msg, (SQLSMALLINT)Msg'size, &MsgLen);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
    {
      if (i == 1)
        trace ("SQL ERROR in '%s'\n", cmd);
      break;
    }
    trace ("SQL ERROR in '%s' : state=%s, nativeerror=%d, msg=%.*s", cmd, SqlState, NativeError, (int)MsgLen, Msg);
    i++;
  }
}

/**********************************************************************/

void trace_env_error (string cmd)
{
  trace_message (SQL_HANDLE_ENV, henv, cmd);
}

/**********************************************************************/

void trace_dbc_error (ref SQL sql, string cmd)
{
  trace_message (SQL_HANDLE_DBC, sql.hdbc, cmd);
}

/**********************************************************************/

void trace_stmt_error (ref SQL sql, string cmd)
{
  trace_message (SQL_HANDLE_STMT, sql.hstmt, cmd);
}

/**********************************************************************/

int free_envir_handle ()
{
  RETCODE retcode;
  int     ret;

  enter_shared_object (ref o);

  ret = 0;

  nb_henv_users--;

  if (nb_henv_users == 0 && henv != 0)  // 'henv' can be freed
  {
    retcode = SQLFreeHandle (SQL_HANDLE_ENV, henv);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
    {
      trace ("error: SQLFreeHandle(SQL_HANDLE_ENV) failed\n");
      ret = E_SQLFreeHandle_FAILED;
    }

    henv = 0;
  }

  leave_shared_object (ref o);
  return ret;
}

/**********************************************************************/

int alloc_envir_handle ()
{
  RETCODE retcode;
  int     ret;

  enter_shared_object (ref o);

  ret = 0;

  nb_henv_users++;

  if (henv == 0)   // not yet allocated
  {
    retcode = SQLAllocHandle (SQL_HANDLE_ENV, 0, &henv);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
    {
      trace_env_error ("SQLAllocHandle(SQL_HANDLE_ENV) failed");
      free_envir_handle ();
      ret = E_SQLAllocHandle_FAILED;
    }
    else
    {
      const int SQL_OV_ODBC3 = 3;
      byte*   ptr;
      INT_PTR value;

      value = SQL_OV_ODBC3;
      ptr'byte = value'byte;
      retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, ptr, 0);
      if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
      {
        trace_env_error ("SQLSetEnvAttr(SQL_ATTR_ODBC_VERSION, SQL_OV_ODBC3) failed");
        free_envir_handle ();
        ret = E_SQLSetEnvAttr_FAILED;
      }
    }
  }

  leave_shared_object (ref o);
  return ret;
}

/**********************************************************************/

string^ allocate_zstring (string s)
{
  int len = strlen(s);
  string^ pstr = new string (len+1);
  pstr^[0:len] = s[0:len];
  return pstr;
}

/**********************************************************************/

int intern_connect (ref SQL     sql,
                        string  source,
                        string  user,
                        string  password,
                        INT_PTR login_timeout_seconds)
{
  RETCODE retcode;

  sql.state = 1;

  retcode = SQLAllocHandle (SQL_HANDLE_DBC, henv, &sql.hdbc);
  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
  {
    trace_stmt_error (ref sql, "SQLAllocHandle(SQL_HANDLE_DBC)");
    return E_SQLAllocHandle_FAILED;
  }


  {
    byte* ptr;
    ptr'byte = login_timeout_seconds'byte;
    retcode = SQLSetConnectAttr(sql.hdbc, SQL_LOGIN_TIMEOUT, ptr, 0);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
    {
      trace_env_error ("SQLSetConnectAttr(SQL_LOGIN_TIMEOUT) failed");
    }
  }

  sql.state = 2;

  {
    string^ psource, puser, ppassword;

    psource   = allocate_zstring (source);
    puser     = allocate_zstring (user);
    ppassword = allocate_zstring (password);

    retcode = SQLConnectA (sql.hdbc, &psource^, SQL_NTS, &puser^, SQL_NTS, &ppassword^, SQL_NTS);

    free psource;
    free puser;
    free ppassword;
  }

  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
  {
    trace_dbc_error (ref sql, "SQLConnect()");
    return E_SQLConnect_FAILED;
  }

  sql.state = 3;

  retcode = SQLAllocHandle (SQL_HANDLE_STMT, sql.hdbc, &sql.hstmt);
  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
  {
    trace_dbc_error (ref sql, "SQLAllocHandle(SQL_HANDLE_STMT)");
    return E_SQLAllocHandle_FAILED;
  }

  sql.state = 4;

  {
    INT_PTR flag = SQL_AUTOCOMMIT_OFF;
    byte* ptr;
    ptr'byte = flag'byte;
    retcode = SQLSetConnectAttr (sql.hdbc, SQL_AUTOCOMMIT, ptr, 0);
  }
  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
  {
    trace_dbc_error (ref sql, "SQLSetConnectAttr(SQL_AUTOCOMMIT_OFF)");
    return E_CANNOT_SET_AUTOCOMMIT_OFF;
  }

  sql.state = 5;

  return 0;
}

/**********************************************************************/

int intern_disconnect (ref SQL sql)
{
  RETCODE retcode;

  free sql.bind;
  sql.bind = null;

  if (sql.state >= 5)
  {
    retcode = SQLEndTran (SQL_HANDLE_DBC, sql.hdbc, SQL_ROLLBACK);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
    {
      trace_dbc_error (ref sql, "SQLEndTran(SQL_ROLLBACK)");
      return E_SQL_ROLLBACK_FAILED;
    }
    sql.state = 4;
  }

  if (sql.state >= 4)
  {
    retcode = SQLFreeHandle (SQL_HANDLE_STMT, sql.hstmt);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
    {
      trace_stmt_error (ref sql, "SQLFreeHandle(SQL_HANDLE_STMT)");
      return E_SQLFreeHandle_FAILED;
    }
    sql.state = 3;
  }

  if (sql.state >= 3)
  {
    retcode = SQLDisconnect (sql.hdbc);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
    {
      trace_dbc_error (ref sql, "SQLDisconnect()");
      return E_SQLDisconnect_FAILED;
    }
    sql.state = 2;
  }

  if (sql.state >= 2)
  {
    retcode = SQLFreeHandle (SQL_HANDLE_DBC, sql.hdbc);
    if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
    {
      trace_env_error ("SQLFreeHandle(SQL_HANDLE_DBC)");
      return E_SQLFreeHandle_FAILED;
    }
    sql.state = 1;
  }

  return 0;
}

/**********************************************************************/

public int sql_connect (out SQL sql, string source, string user, string password, int login_timeout_seconds = 15, bool tracing = true)
{
  int rc;

  if (tracing)
    trace ("info: sql_connect (database=%s, user=%s)\n", source, user);

  clear sql;
  sql.trace = tracing;

  // reserve environment handle
  rc = alloc_envir_handle ();
  if (rc < 0)
  {
    clear sql;
    return rc;
  }

  // connect to database
  rc = intern_connect (ref sql, source, user, password, login_timeout_seconds);
  if (rc < 0)
  {
    intern_disconnect (ref sql);
    free_envir_handle ();
    clear sql;
    return rc;
  }

  return 0;
}

/**********************************************************************/

// rollback transaction and disconnect

public int sql_disconnect (ref SQL sql)
{
  int rc;

  if (sql.trace)
    trace ("info: sql_disconnect()\n");

  rc = intern_disconnect (ref sql);
  if (rc < 0)
    return rc;

  return free_envir_handle ();
}

/***********************************************************************/

void close_statement (ref SQL sql)
{
  RETCODE retcode;

  free sql.bind;
  sql.bind = null;

  retcode = SQLFreeStmt (sql.hstmt, SQL_CLOSE);
  retcode = SQLFreeStmt (sql.hstmt, SQL_UNBIND);
  retcode = SQLFreeStmt (sql.hstmt, SQL_RESET_PARAMS);

  _unused retcode;
}

/***********************************************************************/

public int sql_execute (ref SQL sql, string sql_command, uint timeout_secs = 0)
{
  int        len, i, chunk, linelen;
  RETCODE    retcode;
  SQLPOINTER ptr;
  INT_PTR    ltimeout_secs = (int)timeout_secs;
  
  close_statement (ref sql);

  if (sql.trace)
  {
    linelen = 66;
    len = strlen (sql_command);
    i = 0;
    while (i < len)
    {
      chunk = min(linelen, len-i);
      if (i == 0)
        trace ("info: SQL> ");
      else
        trace ("info:      ");
      trace ("\"%.*s\"\n", min(linelen, len-i), sql_command[i : chunk]);
      i += linelen;
    }
  }

  ptr'byte = ltimeout_secs'byte;
  retcode = SQLSetStmtAttr (sql.hstmt, SQL_ATTR_QUERY_TIMEOUT, ptr, SQL_IS_UINTEGER);
  if (retcode != SQL_SUCCESS)
  {
    trace_stmt_error (ref sql, "SQLSetStmtAttr()");
    return E_SQLExecDirect_FAILED;
  }

  retcode = SQLExecDirect (sql.hstmt, &sql_command, strlen(sql_command));
  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
  {
    trace_stmt_error (ref sql, "SQLExecDirect()");
    close_statement (ref sql);
    return E_SQLExecDirect_FAILED;
  }

  return 0;
}

/***********************************************************************/

public int sql_bind (ref SQL sql, BIND[] bind)
{
  int i;

  if (sql.bind != null)
    return E_SQL_BIND_ALREADY_DONE;

  sql.bind = new BIND[] ' (bind);
  sql.ptr = null;

  sql.row_size = 0;
  for (i=0; i<bind'length; i++)
    sql.row_size += bind[i].size;

  return 0;
}

/***********************************************************************/

int bind_to_record (ref SQL sql, ref byte[] rec)
{
  if (sql.bind == null)
  {
    trace ("error: sql_fetch() : no previous call to sql_bind() was done\n");
    return E_MISSING_SQL_BIND;
  }

  if (sql.row_size != rec'size)
  {
    trace ("error: sql_fetch() : record size does not match with bind (%u != %u)\n", sql.row_size, rec'size);
    return E_SQL_BIND_BAD_ROW_SIZE;
  }

  if (sql.ptr == &rec)   // old bind still valid
    return 0;

  {
    ref BIND[] bind = sql.bind^;
    RETCODE    retcode;
    int        i, ofs;

    ofs = 0;

    for (i=0; i<bind'length; i++)
    {
      retcode = SQLBindCol (sql.hstmt,
                            (SQLUSMALLINT)(i+1),
                            (SWORD)bind[i].typ,
                            (SQLPOINTER)(&rec[0] + ofs),
                            (SQLLEN)bind[i].size,
                            (SQLLEN *)&g_indicator);

      if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
      {
        char msg[64];
        sprintf (out msg, "SQLBindCol(%d)", i);
        trace_stmt_error (ref sql, msg);
        return E_SQLBindCol_FAILED;
      }

      ofs += (int)bind[i].size;
    }
  }

  sql.ptr = (byte*)&rec;
  return 0;
}

/***********************************************************************/

public int sql_fetch (ref SQL sql, out byte[] row)
{
  int       ret, rc;
  RETCODE   retcode;

  clear row;

  rc = bind_to_record (ref sql, ref row);
  if (rc  < 0)
    return rc;

  retcode = SQLFetch (sql.hstmt);
  if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
    return 0;

  if (retcode == SQL_NO_DATA_FOUND)
  {
    ret = SQL_NO_MORE_DATA;
  }
  else
  {
    trace_stmt_error (ref sql, "SQLFetch()");
    ret = E_SQLFetch_FAILED;
  }

  close_statement (ref sql);

  return ret;
}

/***********************************************************************/

public int sql_count (ref SQL sql)
{
  RETCODE retcode;
  SDWORD  nb_rows;

  retcode = SQLRowCount (sql.hstmt, &nb_rows);
  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
  {
    trace_stmt_error (ref sql, "SQLRowCount()");
    return E_SQLRowCount_FAILED;
  }

  return nb_rows;
}

/***********************************************************************/

public int sql_commit (ref SQL sql)
{
  RETCODE retcode;

  if (sql.trace)
    trace ("info: sql_commit()\n");

  retcode = SQLEndTran (SQL_HANDLE_DBC, sql.hdbc, SQL_COMMIT);
  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
  {
    trace_stmt_error (ref sql, "SQLEndTran(SQL_COMMIT)");
    return E_SQL_COMMIT_FAILED;
  }

  return 0;
}

/***********************************************************************/

public int sql_rollback (ref SQL sql)
{
  RETCODE retcode;

  if (sql.trace)
    trace ("info: sql_rollback()\n");

  retcode = SQLEndTran (SQL_HANDLE_DBC, sql.hdbc, SQL_ROLLBACK);
  if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO)
  {
    trace_stmt_error (ref sql, "SQLEndTran(SQL_ROLLBACK)");
    return E_SQL_ROLLBACK_FAILED;
  }

  return 0;
}

/***********************************************************************/

public void sql_set_tracing (ref SQL sql, bool tracing)
{
  sql.trace = tracing;
}

/***********************************************************************/

// retrieve current trace flags

public bool sql_get_tracing (ref SQL sql)
{
  return sql.trace;
}

/***********************************************************************/
#end unsafe
