
// calendar.h

// -------------------------------------------------------------------

struct DATE_TIME
{
  int2 year;     // 1901 to 9999
  int1 month;    //    1 to   12
  int1 day;      //    1 to   31
  int1 hour;     //    0 to   23
  int1 min;      //    0 to   59
  int1 sec;      //    0 to   59
  int2 msec;     //    0 to  999
}

void get_datetime (out DATE_TIME datetime);

// -------------------------------------------------------------------

// assertion: year is in range 1901 .. 9999
int max_days_in_month (int month, int year);

// -------------------------------------------------------------------

// only years in range 1901 .. 9999 are considered as valid.
bool is_valid_date (int day, int month, int year);

// -------------------------------------------------------------------

// returns: 1=Monday, ..., 7=Sunday
// assertion: date is valid.
int day_of_week (int day, int month, int year);

// -------------------------------------------------------------------

// returns number of days since 1/1/1901
// (used to compute nb days between two dates)
// assertion: date is valid.
int nb_days_since_1901 (int day, int month, int year);

// -------------------------------------------------------------------

// nb_days = number of days since 1/1/1901.
void nb_days_since_1901_to_date (int nb_days, out int day, out int month, out int year);

// -------------------------------------------------------------------
// see also clock() in thread.h
// -------------------------------------------------------------------

void clock_to_datetime (    long      clock_time,
                            int       timezone,    // in minutes, use get_time_zone()
                        out DATE_TIME datetime);

// -------------------------------------------------------------------

void datetime_to_clock (    DATE_TIME datetime,
                            int       timezone,    // in minutes, use get_time_zone()
                        out long      clock_time);

// -------------------------------------------------------------------

// add 'days' to date; 'days' can be negative.
// assertion: date is valid.
void add_days (ref DATE_TIME date, int nb_days);

// -------------------------------------------------------------------

// add 'secs' to date; 'secs' can be negative.
// assertion: date is valid.
void add_seconds (ref DATE_TIME date, long nb_secs);

// -------------------------------------------------------------------

// UTC (gmt) system time
void get_gmt_datetime (out DATE_TIME datetime);

// -------------------------------------------------------------------

// returns the time offset of the local computer compared to GMT time.
// the value is expressed in minutes and can be in range -12*60 .. +12*60.
// the value can fluctuate +/-60 min. during summer/winter time in Europe.
// some examples: London = 0, Brussels = +60 or +120, Mexico = -6*60.
int get_time_zone ();

// -------------------------------------------------------------------
