
// bug.c : get crash location

from std use console, files, strings, thread;

#begin unsafe
int main (string[] arg)
{
  uint    ip=0, size, current_unit, unit, line=0, len;
  byte[]^ p;
  uint*   l;
  bool    found;
  int     fd;

  if (arg'length != 3 || sscanf (arg[2], "%x", out ip) != 0)
  {
    printf ("usage: bug <file.dbg> <IP>\n");
    exit (-1);
  }

  fd = open (arg[1]);
  if (fd < 0)
  {
    printf ("error: cannot open file %s\n", arg[1]);
    exit (-1);
  }

  size = (uint)lseek (fd, 0L, SEEK_END);
  lseek (fd, 0L, SEEK_SET);

  p = new byte [size];

  if (read (fd, out p^) != (int)size)
  {
    printf ("error: cannot read %s\n", arg[1]);
    exit (-1);
  }

  close (fd);

  // header : 2 uint4 : 0xF2761287, 0xD5016423

  l = (uint *)&p^;
  if (size < 12 || l[0] != 0xF2761287 || l[1] != 0xD5016423)
  {
    printf ("error: illegal file %s\n", arg[1]);
    exit (-1);
  }

  l += 2;

  found = false;
  unit = 0;
  current_unit = 0;

  // record: <a4>  <b4>
  //   (0,  0x80000000) -> end of couple list
  //   (0,  unit_nr)    -> new unit nr
  //   (ip, line)       -> couples ordered by ip

  while (l[1] != 0x80000000 && (byte*)(l+2) <= &p^ + size)
  {
    if (l[0] == 0)
      current_unit = l[1];
    else if (ip >= l[0] && ip < l[2] && l[1] != 0)
    {
      found = true;
      unit = current_unit;
      line = l[1];
    }

    l += 2;
  }

  printf ("IP Address  : %06x\n", ip);

  if (!found)
  {
    printf ("IP not found\n");
    exit (-1);
  }

  l += 2;      // skip (0, 0x80000000)
  while (*l != 0)
  {
    // <unit_nr4>  <len4_library_name ; library_name>   <len4_source_filename ; source_filename>
    // ends with 2 longs : (0, 0x80000000)

    if (*l == unit)
    {
      l++;

      len = *l;
      l++;
      if (len != 0)
        printf ("library : %s\n", ((char*)l)[0:len]);
      l = (uint*)(((char*)l) + len);

      len = *l;
      l++;
      printf ("source file : %s\n", ((char*)l)[0:len]);
      l = (uint*)(((char*)l) + len);
    }
    else
    {
      l++;

      len = *l;
      l++;
      l = (uint*)(((char*)l) + len);

      len = *l;
      l++;
      l = (uint*)(((char*)l) + len);
    }
  }

  printf ("source line : %u\n", line);

  return 0;
}
#end unsafe
