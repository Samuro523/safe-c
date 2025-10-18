
// bug2.c : add filenames and lines to crash report

from std use console, files, strings, thread;

#begin unsafe

void translate_ip (    string dbg_filename, 
                       uint   ip, 
                   out string info)
{
  uint    size, current_unit, unit, line=0, len;
  uint*   l;
  byte[]^ p;
  bool    found;
  int     fd;

  strcpy (out info, "");

  fd = open (dbg_filename);
  if (fd < 0)
  {
    printf ("error: cannot open file %s\n", dbg_filename);
    exit (-1);
  }

  size = (uint)lseek (fd, 0L, SEEK_END);
  lseek (fd, 0L, SEEK_SET);

  p = new byte[size];

  if (read (fd, out p^) != (int)size)
  {
    printf ("error: cannot read %s\n", dbg_filename);
    exit (-1);
  }

  close (fd);

  l = (uint*)&p^;
  if (size < 12 || l[0] != 0xF2761287 || l[1] != 0xD5016423)
  {
    printf ("error: illegal file %s\n", dbg_filename);
    exit (-1);
  }

  l += 2;

  found = false;
  unit = 0;
  current_unit = 0;

  while (l[1] != 0x80000000 && (byte*)&l[2] <= &p^ + size)
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

  if (!found)
  {
    return;
  }

  l += 2;
  while (*l != 0)
  {
    if (*l == unit)
    {
      l++;

      len = *l;
      l++;
//      if (len)   printf ("library : %s\n", ((char*)l)[0:len]);
      l = (uint*)(((char*)l) + len);

      len = *l;
      l++;

      sprintf (out info, "  file: %s  line: %u", ((char*)l)[0:len], line);
      
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

  free p;
}


int main (string[] arg)
{
  FILE fp1, fp2;
  char line[4096], info[512];
  
  if (arg'length != 4)
  {
    printf ("usage: bug2 <file.dbg> <crash-report.txt>  <modified-crash-report>\n");
    exit (-1);
  }

  if (fopen (out fp1, arg[2]) < 0)
    return -1;
    
  if (fcreate (out fp2, arg[3], ANSI) < 0)
    return -1;
    
  while (fgets (ref fp1, out line) == 0)
  {
    uint  ip;
    int   len = strlen(line);
    int   count;
    
    clear info;

    // cut trailing \n
    if (len > 0 && line[len-1] == '\n')
      line[--len] = nul;

    count = 0;
    while (len > 0 && isxdigit(line[len-1]))
    {
      len--;
      count++;
    }
    
    if (count >= 6 && count <= 8)
    {
      if (sscanf (line[len:count], "%x", out ip) == 0)
        translate_ip (arg[1], ip, out info);
    }

    fprintf (ref fp2, "%s%s\n", line, info);
  }

  fclose (ref fp1);  
  fclose (ref fp2);  
  return 0;
}

#end unsafe
