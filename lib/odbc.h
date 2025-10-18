
// odbc.h : interface to SQL database using odbc 3.0

/***********************************************************************/

struct SQL;

/***********************************************************************/

// returns 0 if OK, or a negative error code.

int sql_connect (out SQL    sql,
                     string source,
                     string user,
                     string password,
                     int    login_timeout_seconds = 15,
                     bool   tracing = true);

/***********************************************************************/

// rollback last transaction and disconnect.
// returns 0 if OK or a negative error code.
// note: if this function blocks under Oracle, try setting the option "Enable Thread Safety" in Control Pannel / Odbc.

int sql_disconnect (ref SQL sql);

/***********************************************************************/

// execute an sql command.
// timeout_secs == 0 means no timeout.
// note that an sql command has no permanent effect until a commit or a rollback is done.
// returns 0 if OK or a negative error code.

int sql_execute (ref SQL sql, string sql_command, uint timeout_secs = 0);

/***********************************************************************/

enum SQL_TYP {SQL_UNKNOWN_TYPE,
              SQL_CHAR,      // char
              SQL_NUMERIC,
              SQL_DECIMAL,
              SQL_INTEGER,   // int
              SQL_SMALLINT,  // int2
              SQL_FLOAT,
              SQL_REAL,
              SQL_DOUBLE};

struct BIND
{
  SQL_TYP typ;
  uint    size;
}

// bind the result columns of an sql select statement to program variables.
// fields of type SQL_CHAR must reserve 1 extra byte for the terminating null character.
/* Example:

  rc = sql_bind (ref sql,
                 {SQL_CHAR,  8},
                 {SQL_INT,   4});
*/

int sql_bind (ref SQL sql, BIND[] bind);

/***********************************************************************/

const int SQL_NO_MORE_DATA = 1;

// retrieve next row of result
// returns 0 if OK, a negative value in case of error, or SQL_NO_MORE_DATA

int sql_fetch (ref SQL sql, out byte[] row);

/***********************************************************************/

// returns nb rows affected by previous (insert/update/delete) command
// or a negative value in case of error.

int sql_count (ref SQL sql);

/***********************************************************************/

// commit or rollback transaction

int sql_commit (ref SQL sql);
int sql_rollback (ref SQL sql);

/***********************************************************************/

// enable or disable tracing (connection must be open)
void sql_set_tracing (ref SQL sql, bool tracing);

// retrieve current trace flags
bool sql_get_tracing (ref SQL sql);

/***********************************************************************/

// error messages
const int E_SQLAllocHandle_FAILED     = (-1);
const int E_SQLFreeHandle_FAILED      = (-2);
const int E_SQLSetEnvAttr_FAILED      = (-3);
const int E_SQLConnect_FAILED         = (-4);
const int E_SQLDisconnect_FAILED      = (-5);
const int E_CANNOT_SET_AUTOCOMMIT_OFF = (-6);
const int E_SQL_COMMIT_FAILED         = (-7);
const int E_SQL_ROLLBACK_FAILED       = (-8);
const int E_SQLExecDirect_FAILED      = (-9);
const int E_SQL_BIND_ALREADY_DONE     = (-10);
const int E_MISSING_SQL_BIND          = (-11);
const int E_SQL_BIND_BAD_ROW_SIZE     = (-12);
const int E_SQLBindCol_FAILED         = (-13);
const int E_SQLFetch_FAILED           = (-14);
const int E_SQLRowCount_FAILED        = (-15);

/***********************************************************************/

#if 0


// odbc-example.c

from std use odbc, tracing;

int main()
{
  int rc;
  SQL sql;

  packed struct REC
  {
    int  jobstate;
    char jobtype[256+1];
  }

  REC rec;

  open_trace ("p.tra");

  rc = sql_connect (out sql, "MyDatabase", "MyUser", "MyPassword");
  if (rc < 0)
  {
    trace ("error: sql_connect() returned %d\n", rc);
    return -1;
  }

  rc = sql_execute (ref sql,  "insert into Jobs values (1, 1, 'test')");
  if (rc < 0)
  {
    trace ("error: sql_execute(1) failed rc = %d\n", rc);
    return -1;
  }                           
  
  trace ("rows affected = %d\n", sql_count (ref sql));
  
  
  rc = sql_commit (ref sql);
  if (rc < 0)
  {
    trace ("error: sql_commit(1) failed rc = %d\n", rc);
    return -1;
  }                           
    
  rc = sql_execute (ref sql,  "select jobstate, jobtype from Jobs");
  if (rc < 0)
  {
    trace ("error: sql_execute(2) failed rc = %d\n", rc);
    return -1;
  }                           
  
  rc = sql_bind (ref sql, {{SQL_INTEGER, rec.jobstate'size},
                           {SQL_CHAR,    rec.jobtype 'size}});
  if (rc < 0)
  {
    trace ("error: sql_bind() failed rc = %d\n", rc);
    return -1;
  }                           

  for (;;)
  {
    rc = sql_fetch (ref sql, out rec);
    if (rc != 0)
      break;

    trace ("info: jobstate = %4d, recjobtype = %s\n", rec.jobstate, rec.jobtype);
  }

  if (rc < 0)
  {
    trace ("error: sql_fetch() failed rc = %d\n", rc);
    return -1;
  }                           

  rc = sql_disconnect (ref sql);
  if (rc < 0)
  {
    trace ("error: sql_disconnect() failed rc = %d\n", rc);
    return -1;
  }                           

  trace ("ok\n");
  return 0;
}

#endif
