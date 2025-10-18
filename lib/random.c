
/* random.c : random number generator */

#if WINDOWS
  use thread, win/windows;
#endif  

#if ANDROID
  use android/bionic;
#endif  

/****************************************************************************/

#if WINDOWS
  SHARED_OBJECT o;
#endif  

/****************************************************************************/

#if WINDOWS
public void get_random_number (out byte[] value)
{
#begin unsafe
enter_shared_object (ref o);
  assert SystemFunction036 (&value, value'size) == 1; // RtlGenRandom
leave_shared_object (ref o);
#end unsafe
}
#endif

#if ANDROID
public void get_random_number (out byte[] value)
{
#begin unsafe
  const string FILENAME = "/dev/urandom\0";
  int fd = open(&FILENAME, O_RDONLY, 0);
  assert read (fd, &value, value'length) == value'length;
  close (fd);
#end unsafe
}
#endif


/****************************************************************************/

public int rnd (int first, int last)
{
  uint n;

  assert first <= last;

  get_random_number (out n);

  return first + (int)(n % (uint)(last-first+1));
}

/****************************************************************************/
