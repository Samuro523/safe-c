
// calendar.c

#if WINDOWS
  use win/windows;
#endif

#if ANDROID
  use android/bionic;
#endif

// -------------------------------------------------------------------
#begin unsafe
// -------------------------------------------------------------------

#if WINDOWS

public void get_datetime (out DATE_TIME datetime)
{
  SYSTEMTIME info;

  GetLocalTime (out info);

  datetime = {year  => (int2)info.wYear,
              month => (int1)info.wMonth,
              day   => (int1)info.wDay,
              hour  => (int1)info.wHour,
              min   => (int1)info.wMinute,
              sec   => (int1)info.wSecond,
              msec  => (int2)info.wMilliseconds};
}

// -------------------------------------------------------------------

public void get_gmt_datetime (out DATE_TIME datetime)
{
  SYSTEMTIME info;

  GetSystemTime (out info);

  datetime = {year  => (int2)info.wYear,
              month => (int1)info.wMonth,
              day   => (int1)info.wDay,
              hour  => (int1)info.wHour,
              min   => (int1)info.wMinute,
              sec   => (int1)info.wSecond,
              msec  => (int2)info.wMilliseconds};
}

// -------------------------------------------------------------------

// returns the time offset of the local computer compared to GMT time.
// the value is expressed in minutes and can be in range -12*60 .. +12*60.
// the value can fluctuate +/-60 min. during summer/winter time in Europe.
// some examples: London = 0, Brussels = +60 or +120, Mexico = -6*60.

public int get_time_zone ()
{
  TIME_ZONE_INFORMATION info;
  DWORD rc;

  rc = GetTimeZoneInformation (out info);
  if (rc == 1)
    info.Bias += info.StandardBias;
  if (rc == 2)
    info.Bias += info.DaylightBias;
  return -info.Bias;
}

// -------------------------------------------------------------------
#endif // WINDOWS
// -------------------------------------------------------------------

// -------------------------------------------------------------------
#if ANDROID
// -------------------------------------------------------------------

public void get_datetime (out DATE_TIME datetime)
{
  TIME time;
  tm   tm;

  clock_gettime (CLOCK_REALTIME, out time);
  localtime_r (&time.sec, &tm);

  if (tm.tm_sec == 60)  // avoid leapsecond crash
  {
    tm.tm_sec = 59;
    time.nsec = 999_999;
  }

  datetime = {year  => (int2)(tm.tm_year + 1900),
              month => (int1)(tm.tm_mon + 1),
              day   => (int1)tm.tm_mday,
              hour  => (int1)tm.tm_hour,
              min   => (int1)tm.tm_min,
              sec   => (int1)tm.tm_sec,
              msec  => (int2)((time.nsec * 2251799814L) >> 51)};   // divides by 1_000_000
}

// -------------------------------------------------------------------

public void get_gmt_datetime (out DATE_TIME datetime)
{
  TIME time;
  tm   tm;

  clock_gettime (CLOCK_REALTIME, out time);
  gmtime_r (&time.sec, &tm);

  if (tm.tm_sec == 60)  // avoid leapsecond crash
  {
    tm.tm_sec = 59;
    time.nsec = 999_999;
  }

  datetime = {year  => (int2)(tm.tm_year + 1900),
              month => (int1)(tm.tm_mon + 1),
              day   => (int1)tm.tm_mday,
              hour  => (int1)tm.tm_hour,
              min   => (int1)tm.tm_min,
              sec   => (int1)tm.tm_sec,
              msec  => (int2)((time.nsec * 2251799814L) >> 51)};   // divides by 1_000_000
}

// -------------------------------------------------------------------

// returns the time offset of the local computer compared to GMT time.
// the value is expressed in minutes and can be in range -12*60 .. +12*60.
// the value can fluctuate +/-60 min. during summer/winter time in Europe.
// some examples: London = 0, Brussels = +60 or +120, Mexico = -6*60.

public int get_time_zone ()
{
  TIME time;
  tm   tm;

  clock_gettime (CLOCK_REALTIME, out time);
  localtime_r (&time.sec, &tm);

  return (int)(tm.tm_gmtoff / 60);
}

// -------------------------------------------------------------------

#endif // WINDOWS

// -------------------------------------------------------------------
#end unsafe
// -------------------------------------------------------------------

bool is_leap_year (int year)
{
  bool is_leap = false;

  if ((year & 3) == 0)   // multiple of 4
  {
    if ((year % 100) == 0)
    {
      if ((year % 400) == 0)
        is_leap = true;
    }
    else
    {
      is_leap = true;
    }
  }

  return is_leap;
}

// -------------------------------------------------------------------

// assertion: year is in range 1901 .. 9999

public int max_days_in_month (int month, int year)
{
  int n, result;

  assert (month >= 1 && month <= 12 && year >= 1901 && year <= 9999);

  if (month == 2)
  {
    if (is_leap_year (year))
      result = 29;
    else
      result = 28;
  }
  else
  {
    n = month;

    if (n > 7)       // add 1 for month >= August
      n++;

    if ((n & 1) == 0)
      result = 30;     // even month
    else
      result = 31;     // odd month
  }

  return result;
}

// -------------------------------------------------------------------

// only years in range 1901 .. 9999 are considered as valid.

public bool is_valid_date (int day, int month, int year)
{
  return (year  >= 1901 && year  <= 9999)
      && (month >= 1    && month <= 12)
      && (day   >= 1    && day   <= max_days_in_month (month, year));
}

// -------------------------------------------------------------------

// returns number of days since 1/1/1901.
// (used to compute nb days between two dates)
// assertion: date is valid.

public int nb_days_since_1901 (int day, int month, int year)
{
  int yy, nb_days;

  assert (is_valid_date (day, month, year));

  yy = year - 1;
  nb_days = (yy * 365) + (yy >> 2) - (yy / 100) + (yy / 400) - 693960
          + (month-1) * 30 + (month >> 1);

  if (month >= 3)      // February included
  {
    if (is_leap_year (year))
      nb_days--;           // leap year : 29 instead of 30
    else
      nb_days -= 2;        // normal year : 28 instead of 30
  }

  if (month == 9 || month == 11)    // July/August = twice 31 days
    nb_days++;

  return nb_days + (day-1);
}

// -------------------------------------------------------------------

public void nb_days_since_1901_to_date (int nb_days, out int day, out int month, out int year)
{
  int rest, i;

  year = 1;
  rest = nb_days + 693960;

  while (rest >= 146097*5)
  {
    year += 2000;
    rest -= 146097*5;
  }

  while (rest >= 146097)
  {
    year += 400;
    rest -= 146097;
  }

  for (i=1; i<=3 && rest >= 36524; i++)
  {
    year += 100;
    rest -= 36524;
  }

  for (i=1; i<=4 && rest >= 1461*6; i++)
  {
    year += 24;
    rest -= 1461*6;
  }

  for (i=1; i<=2 && rest >= 1461*3; i++)
  {
    year += 12;
    rest -= 1461*3;
  }

  for (i=1; i<=3 && rest >= 1461; i++)
  {
    year += 4;
    rest -= 1461;
  }

  for (i=1; i<=3 && rest >= 365; i++)
  {
    year++;
    rest -= 365;
  }

  for (month=1; month<=12; month++)
  {
    int count = max_days_in_month (month, year);
    if (rest < count)
      break;
    rest -= count;
  }

  day = 1 + rest;
}

// -------------------------------------------------------------------

// returns: 1=Monday, ..., 7=Sunday
// assertion: date is valid.

public int day_of_week (int day, int month, int year)
{
  int nb_days;

  nb_days = nb_days_since_1901 (day, month, year);

  return 1 + ((nb_days+1) % 7);
}

// -------------------------------------------------------------------

// add 'days' to date; 'days' can be negative.
// assertion: date is valid.

public void add_days (ref DATE_TIME date, int nb_days)
{
  int count, days;

  days = nb_days;

  while (days > 0)
  {
    count = max_days_in_month (date.month, date.year) + 1 - (int)date.day;
    if (count > days)
    {
      date.day += (int1)days;
      return;
    }

    days -= count;
    date.day = 1;

    if (date.month < 12)
      date.month++;
    else
    {
      date.month = 1;
      date.year++;

      while (days >= 365 + (int)is_leap_year (date.year))
      {
        days -= (365 + (int)is_leap_year (date.year));
        date.year++;
      }
    }
  }

  while (days < 0L)
  {
    if (-(int)date.day < days)
    {
      date.day -= (int1)(-days);
      return;
    }

    days += date.day;

    if (date.month > 1)
      date.month--;
    else
    {
      date.month = 12;
      date.year--;

      while (days <= -(365 + (int)is_leap_year (date.year)))
      {
        days += (365 + (int)is_leap_year (date.year));
        date.year--;
      }
    }

    date.day = (int1)max_days_in_month (date.month, date.year);
  }
}

// -------------------------------------------------------------------

// add 'secs' to date; 'secs' can be negative.
// assertion: date is valid.

public void add_seconds (ref DATE_TIME date, long nb_secs)
{
  long secs = nb_secs;

  if (secs >= 0)
  {
    add_days (ref date, (int)(secs / 86400));
    secs %= 86400;
  }
  else
  {
    add_days (ref date, (int)-((-secs) / 86400));
    secs = -((-secs) % 86400);
  }

  secs += (date.hour * 3600 + date.min * 60 + date.sec);

  while (secs < 0)
  {
    add_days (ref date, -1);
    secs += 86400;
  }

  while (secs >= 86400)
  {
    add_days (ref date, +1);
    secs -= 86400;
  }

  date.sec = (int1)(secs % 60);
  secs /= 60;

  date.min = (int1)(secs % 60);
  secs /= 60;

  date.hour = (int1)secs;
}

// -------------------------------------------------------------------

public
void clock_to_datetime (    long      clock_time,
                            int       timezone,
                        out DATE_TIME datetime)
{
  long adapted_clock, limit, nb_msecs;
  int  nb_days, msecs, hours, min, sec, day, month, year;

  adapted_clock = clock_time + timezone * 600_000_000L;
  
  limit = 94_670_208_000_000_000;     // 1/1/1901
  if (adapted_clock < limit)
    adapted_clock = limit;

  limit = 2_650_467_743_999_990_000;   // 31/12/9999 23:59:59.999
  if (adapted_clock > limit)
    adapted_clock = limit;
  
  nb_msecs = adapted_clock / 10_000 - (300 * 365250 - 3000) * 86400;
  nb_days = (int)(nb_msecs / (86_400L * 1000L));
  msecs = (int)(nb_msecs - nb_days * (86_400L * 1000L));

  hours = msecs / (60 * 60 * 1000);
  msecs -= hours * (60 * 60 * 1000);
  min = msecs / (60 * 1000);
  msecs -= min * (60 * 1000);
  sec = msecs / 1000;
  msecs -= sec * 1000;

  nb_days_since_1901_to_date (nb_days, out day, out month, out year);

  clear datetime;
  datetime.year  = (int2)year;
  datetime.month = (int1)month;
  datetime.day   = (int1)day;
  datetime.hour  = (int1)hours;
  datetime.min   = (int1)min;
  datetime.sec   = (int1)sec;
  datetime.msec  = (int2)msecs;
}

// -------------------------------------------------------------------

public
void datetime_to_clock (    DATE_TIME datetime,
                            int       timezone,
                        out long      clock_time)
{
  int nb_days = nb_days_since_1901 (datetime.day, datetime.month, datetime.year);
  int nb_msecs = datetime.hour * 3_600_000 + (datetime.min - timezone) * 60_000 + datetime.sec * 1000 + datetime.msec;
  clock_time = (nb_days * 86_400_000L + nb_msecs + (300 * 365250 - 3000) * 86400) * 10_000L;
}

// -------------------------------------------------------------------
