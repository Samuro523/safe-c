
// makelib.c : create packed library file

from std use console, files, strings, thread, zip;
use ../makelib0;

/**************************************************************************/

int  fout, fin;   // file handle
long g_out_offset;

/**************************************************************************/

struct USER_INFO
{
  int  fin;
  int  fout;
  long count_in;
  long count_out;
}

/**************************************************************************/

struct NODE
{
  NODE_INFO i;
  string^   unit_name;
  NODE^     next;
}

NODE^ g_first, g_last;

/**************************************************************************/

string^ new_string (string name)
{
  int len = strlen(name);
  return new string ' (name[0:len]);
}

/**************************************************************************/

void insert_node (NODE_INFO n, string unit_name)
{
  NODE^ p = new NODE ' {i         => n, 
                        unit_name => new_string (unit_name),
                        next      => null};

  if (g_first == null)
    g_first = p;
  else
    g_last^.next = p;
  g_last = p;
}

/**************************************************************************/

int write_header (uint4 offset_dir)
{
  HEADER h;
  clear h;
  h.magic = MAGIC;
  h.version = LIB_VERSION;
  h.offset_dir = offset_dir;
  if (write (fout, h) != (int)h'size)
    return -1;
  return 0;
}

/**************************************************************************/

int writeout_directory_table ()
{
  NODE^ p = g_first;

  while (p != null)
  {
    if (write (fout, p^.i) != (int)NODE_INFO'size)
      return -1;
    if (write (fout, p^.unit_name^) != p^.i.unit_name_length)
      return -1;
    p = p^.next;
  }

  return 0;  
}

/**************************************************************************/

int my_read (ref USER_INFO user_info, out byte[] buffer)
{
  int rc;
  rc = read (user_info.fin, out buffer);
  user_info.count_in += rc;
  return rc;
}

/***********************************************************************/

int my_write (ref USER_INFO user_info, byte[] buffer)
{
  int rc;
  rc = write (user_info.fout, buffer);
  user_info.count_out += rc;
  return rc;
}

/***********************************************************************/

package P = new PACK (USER_INFO => USER_INFO);

/***********************************************************************/

void import (string filename, string unit_name)
{
  int       rc;
  USER_INFO info;
  NODE_INFO n;

  printf ("%-30.30s   ->  %-30.30s\n", filename, unit_name);

  fin = open (filename);
  if (fin < 0)
  {
    printf ("error: cannot open %s\n", filename);
    exit (-1);
  }

  clear info;
  info.fin = fin;
  info.fout = fout;
  info.count_in = 0;
  info.count_out = 0;

  rc = pack (ref info, my_read, my_write);
  if (rc < 0)
  {
    printf ("error: cannot treat %s (error %d)\n", filename, rc);
    exit (-1);
  }

  close (fin);

//  printf ("(%d -> %d bytes)\n", info.count_in, info.count_out);

  clear n;
  n.unit_name_length = strlen(unit_name);
  n.offset_in_file   = (uint4)g_out_offset;      // does not support files larger than 4 GB
  n.packed_size      = (uint4)info.count_out;
  n.compression      = 0;
  n.unpacked_size    = (uint4)info.count_in;

  g_out_offset += info.count_out;

  insert_node (n, unit_name);
}

/**************************************************************************/

void scan_directory_and_write_all_files_in_compressed_form (string pathname, string dir)
{
  FILE_INFO info;
  char      path[260];
  int       rc;

  sprintf (out path, "%s*.*", pathname);
  rc = open_directory (out info, path);
  while (rc == 0)
  {
    if (info.type == TYPE_REGULAR_FILE)
    {
      char local_name[260], lib_name[260];
      sprintf (out local_name, "%s%s", pathname, info.name);
      sprintf (out lib_name,   "%s%s", dir, info.name);
      import (local_name, lib_name);
    }
    else if (info.type == TYPE_DIRECTORY &&
             strcmp (info.name, ".") != 0 && strcmp (info.name, "..") != 0)
    {
      char local_dir[260], lib_dir[260];
      sprintf (out local_dir, "%s%s/", pathname, info.name);
      sprintf (out lib_dir,   "%s%s/", dir, info.name);
      scan_directory_and_write_all_files_in_compressed_form (local_dir, lib_dir);
    }

    rc = read_directory (ref info);
  }

  close_directory (ref info);
}

/**************************************************************************/

int main (string[] arg)
{
  char libfilename[260], pathname[260];

  if (arg'length != 3)
  {
    printf ("makelib  <newfile>.lib  <path/>\n");
    return -1;
  }

  sprintf (out libfilename, "%.200s", arg[1]);
  strcat (ref libfilename, ".lib");

  sprintf (out pathname, "%.200s", arg[2]);
  if (pathname[strlen(pathname)-1] != '\\' && pathname[strlen(pathname)-1] != '/')
  {
    printf ("error: pathname must end with \\ or /\n");
    return -1;
  }

  fout = create (libfilename);
  if (fout < 0)
  {
    printf ("error: cannot create %s\n", libfilename);
    return -1;
  }

  if (write_header (0) < 0)
  {
    printf ("error: cannot write to %s\n", libfilename);
    return -1;
  }

  g_out_offset = HEADER'size;
  
  // write all library units to output file in compressed form, fill g_first, g_last
  scan_directory_and_write_all_files_in_compressed_form (pathname, "/");

  if (lseek (fout, 0, SEEK_SET) < 0)
  {
    printf ("error: cannot seek in %s\n", libfilename);
    return -1;
  }

  if (write_header ((uint4)g_out_offset) < 0)
  {
    printf ("error: cannot write to %s\n", libfilename);
    return -1;
  }

  if (lseek (fout, g_out_offset, SEEK_SET) < 0)
  {
    printf ("error: cannot seek in %s\n", libfilename);
    return -1;
  }

  if (writeout_directory_table() < 0)
  {
    printf ("error: cannot write to %s\n", libfilename);
    return -1;
  }

  if (close (fout) < 0)
  {
    printf ("error: cannot close %s\n", libfilename);
    return -1;
  }

  printf ("ok (%s created)\n", libfilename);
  return 0;
}

/**************************************************************************/
