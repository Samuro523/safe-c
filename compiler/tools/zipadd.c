
// zipadd.c

from std use console, files, strings, zip;

//---------------------------------------------------------------------------------------

void convert (string source, out wstring target)
{
  int i, len;

  clear target;
  len = strlen (source);

  for (i=0; i<len; i++)
    target[i] = (wchar)(uint)source[i];
}

//---------------------------------------------------------------------------------------

int main (string[] arg)
{
  if (arg'length != 5)
  {
    printf ("add file in zip archive\n");
    printf ("format: %s <filename_to_add>  <path_to_add>  <source_zip>  <target_zip>\n", arg[0]);
//                   0        1                 2                3              4
    return -1;
  }

  {
    wchar unzip_container_filename[260];
    wchar zip_container_filename[260];

    convert (arg[3], out unzip_container_filename);
    convert (arg[4], out zip_container_filename);

    {
      UNZIP_CONTAINER unzip;
      ZIP_CONTAINER   zip;

      int nb_files, i, rc;

      rc = unzip_open_container (out unzip, unzip_container_filename);
      if (rc < 0)
      {
        printf ("error: cannot open %S\n", unzip_container_filename);
        return -1;
      }

      rc = wzip_create_container (out zip, zip_container_filename);
      if (rc < 0)
      {
        printf ("error: cannot create %S\n", zip_container_filename);
        return -1;
      }

      nb_files = unzip_number_of_files (unzip);

      for (i=0; i<nb_files; i++)
      {
        wstring(260) filename;
        int          method;
        
        rc = unzip_get_filename (ref unzip, i, out filename);
        if (rc < 0)
        {
          printf ("error: unzip_get_filename(%S, %d) returned %d\n", unzip_container_filename, i, rc);
          return -1;
        }

        // supports store, deflate & deflate64
        rc = unzip_extract_file (ref unzip, i, filename => L"temp-data.tmp332");
        if (rc < 0)
        {
          printf ("error: unzip_extract_file(%S, %d) returned %d\n", unzip_container_filename, i, rc);
          return -1;
        }
      
        rc = unzip_get_compression_method (ref unzip, i, out method);
        if (rc < 0)
        {
          printf ("error: unzip_get_compression_method(%S, %d) returned %d\n", unzip_container_filename, i, rc);
          return -1;
        }
      
        // name: filename to store in zip directory (must be relative, not starting with /, and contain no drive)
        // store or deflate method
        rc = wzip_add_file (ref zip, file => L"temp-data.tmp332", name => filename, always_store => method == 0);
        if (rc < 0)
        {
          printf ("error: wzip_add_file(%S, %d) returned %d\n", zip_container_filename, i, rc);
          return -1;
        }
      }

      wdelete_file (L"temp-data.tmp332");

      {
        wchar file_to_add[260];
        wchar filename_to_add[260];

        convert (arg[2], out file_to_add);
        convert (arg[1], out filename_to_add);

        if (filename_to_add[0] == L'/' || filename_to_add[0] == L'\\' || filename_to_add[1] == L':')
        if (rc < 0)
        {
          printf ("error: '%S' : must be relative, not starting with /, and contain no drive\n", filename_to_add);
          return -1;
        }

        // name: filename to store in zip directory (must be relative, not starting with /, and contain no drive)
        // store or deflate method
        rc = wzip_add_file (ref zip, file => file_to_add, name => filename_to_add, always_store => true);
        if (rc < 0)
        {
          printf ("error: wzip_add_file(%S) returned %d\n", filename_to_add, rc);
          return -1;
        }
      }

      rc = unzip_close_container (ref unzip);
      if (rc < 0)
      {
        printf ("error: unzip_close_container(%S) returned %d\n", unzip_container_filename, rc);
        return -1;
      }

      rc = zip_close_container (ref zip);
      if (rc < 0)
      {
        printf ("error: zip_close_container(%S) returned %d\n", zip_container_filename, rc);
        return -1;
      }
    }
  }

  return 0;
}

