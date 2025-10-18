
// writelog.c

use dbstruct, config, ../db, dbport;

#begin unsafe
/**********************************************************************/

/* note: logging data must be written after a successful command execution */
/* if this function returns an error, the transaction must be cancelled.   */

public int write_logging_data (ref DB_INFO p, byte[] buffer)
{
  int rc;

  if (p.log == -1)      /* no logging file : nothing to do */
    return 0;

  if (p.log_header.last.offset > (uint)int'max - buffer'size)   /* logging file is too large */
    return E_LOGGING_OVERFLOW;

  rc = __db_seek_log (p.log, p.log_header.last.offset);
  if (rc < 0)
    return rc;

  rc = __db_write_log (p.log, buffer);
  if (rc < 0)
    return rc;

  p.log_header.last.offset += buffer'size;

  return 0;
}

/**********************************************************************/

public int write_logging_byte (ref DB_INFO p, BYTE b)
{
  return write_logging_data (ref p, b);
}

/**********************************************************************/

public int write_logging_word (ref DB_INFO p, WORD w)
{
  BYTE b[2];
  b = {(BYTE)(w & 255), (BYTE)(w >> 8)};
  return write_logging_data (ref p, b);
}

/**********************************************************************/
#end unsafe
