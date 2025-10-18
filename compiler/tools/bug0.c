
// bug0.c : print all ip locations

from std use console, files, thread;

#begin unsafe


void list_ips (uint *start_ips, byte[]^ p, uint size, uint unit)
{
  uint* l = start_ips;
  uint current_unit = 0;
  while (l[1] != 0x80000000 && (byte*)(l+2) <= &p^ + size)
  {
    if (l[0] == 0)
      current_unit = l[1];
    else if (current_unit == unit && l[1] != 0)
    {
      printf ("  line %u  ip %x\n", l[1], l[0]);
    }

    l += 2;
  }
}  


int main (string[] arg)
{
  uint    size, unit, len;
  byte[]^ p;
  uint*   l;
  int     fd;
  uint*   start_ips;

  if (arg'length != 2)
  {
    printf ("usage: bug0 <file.dbg>\n");
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

  unit = 0;

  // record: <a4>  <b4>
  //   (0,  0x80000000) -> end of couple list
  //   (0,  unit_nr)    -> new unit nr
  //   (ip, line)       -> couples ordered by ip

  start_ips = l;
  
  
  while (l[1] != 0x80000000 && (byte*)(l+2) <= &p^ + size)
    l += 2;

  l += 2;      // skip (0, 0x80000000)
  while (*l != 0)
  {
    // <unit_nr4>  <len4_library_name ; library_name>   <len4_source_filename ; source_filename>
    // ends with 2 longs : (0, 0x80000000)

    unit = *l;
    
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

    list_ips (start_ips, p, size, unit);
  }

  return 0;
}
#end unsafe
