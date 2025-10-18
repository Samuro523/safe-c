
// shortcut.c

use files, strings, registry, win/windows;

#define debug 0

#if debug
  use tracing;
#endif

//---------------------------------------------------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------------------------------------------------

public int create_shortcut (string shortcut,
                            string target_directory)
{
  char[MAX_FILENAME_LENGTH] current_dir, fullpath_target, working_dir;
  int            len, i, rc;
  int            hres;
  IShellLink     *psl;
  IPersistFile   *ppf;
  wchar          wsz[MAX_FILENAME_LENGTH];
  const string(1) null_string = {nul};
  
  get_current_directory (out current_dir);

  if (expand_pathname (current_dir, target_directory, out fullpath_target) < 0)
  {
#if debug
    trace ("info: create_shortcut() : expand_pathname() failed\n");
#endif
    return -1;
  }

  // convert to upper case, and slash to backslash
  len = (int)strlen(fullpath_target);
  for (i=0; i<len; i++)
  {
    ref char c = fullpath_target[i];
    c = toupper(c);
    if (c == '/')
      c = '\\';
  }

  strcpy (out working_dir, fullpath_target);
  len = strlen(working_dir);
  while (len > 0)
  {
    if (working_dir[len] == '\\')
      break;
    len--;
  }
  working_dir[len] = nul;
  if (strchr (working_dir, '\\') < 0)
    strcat (ref working_dir, "\\");

  rc = CoInitializeEx (null, COINIT_APARTMENTTHREADED);
  if (rc != S_OK && rc != S_FALSE)
  {
#if debug
    trace ("info: create_shortcut() : CoInitializeEx() failed\n");
#endif
    return -1;
  }

  // Get a pointer to the IShellLink interface.
  hres = CoCreateInstance (CLSID_ShellLink, null, CLSCTX_INPROC_SERVER,
                           IID_IShellLinkA, out *(byte**)&psl);
  if (hres != S_OK)
  {
#if debug
    trace ("info: create_shortcut() : CoCreateInstance() failed\n");
#endif
    CoUninitialize ();
    return -1;
  }

  // Query IShellLink for the IPersistFile interface for 
  // saving the shell link in persistent storage.
  hres = psl->lpVtbl->QueryInterface ((PVOID)psl, IID_IPersistFile, out *(byte**)&ppf);
  if (hres != S_OK)
  {
#if debug
    trace ("info: create_shortcut() : QueryInterface() failed\n");
#endif
    psl->lpVtbl->Release((PVOID)psl);       // Release pointer to IShellLink.
    CoUninitialize ();
    return -1;
  }

  // Set the path to the shell link target.
  hres = psl->lpVtbl->SetPath(psl, (LPCSTR)&fullpath_target);
  if (hres != S_OK)
  {
#if debug
    trace ("info: create_shortcut() : SetPath() failed\n");
#endif
    ppf->lpVtbl->Release((PVOID)ppf);       // Release pointer to IPersistFile.
    psl->lpVtbl->Release((PVOID)psl);       // Release pointer to IShellLink.
    CoUninitialize ();
    return -1;
  }

  // Set the description of the shell link.
  hres = psl->lpVtbl->SetDescription(psl, &null_string);
  if (hres != S_OK)
  {
#if debug
    trace ("info: create_shortcut() : SetDescription() failed\n");
#endif
    ppf->lpVtbl->Release((PVOID)ppf);       // Release pointer to IPersistFile.
    psl->lpVtbl->Release((PVOID)psl);       // Release pointer to IShellLink.
    CoUninitialize ();
    return -1;
  }

  hres = psl->lpVtbl->SetWorkingDirectory (psl, &working_dir);
  if (hres != S_OK)
  {
#if debug
    trace ("info: create_shortcut() : SetWorkingDirectory() failed\n");
#endif
    ppf->lpVtbl->Release((PVOID)ppf);       // Release pointer to IPersistFile.
    psl->lpVtbl->Release((PVOID)psl);       // Release pointer to IShellLink.
    CoUninitialize ();
    return -1;
  }

  wsprintf (out wsz, L"%s", shortcut);

  // Save the link via the IPersistFile::Save method.
  hres = ppf->lpVtbl->Save (ppf, (LPCOLESTR)&wsz, TRUE);
  if (hres != S_OK)
  {
#if debug
    trace ("info: create_shortcut() : Save() returned error %04x\n", hres);
#endif
    ppf->lpVtbl->Release((PVOID)ppf);       // Release pointer to IPersistFile.
    psl->lpVtbl->Release((PVOID)psl);       // Release pointer to IShellLink.
    CoUninitialize ();
    return -1;
  }

  ppf->lpVtbl->Release((PVOID)ppf);       // Release pointer to IPersistFile.
  psl->lpVtbl->Release((PVOID)psl);       // Release pointer to IShellLink.
  CoUninitialize ();

  return 0;
}

//---------------------------------------------------------------------------------------------------------

public
int get_system_directory (    string folder_name,
                          out char   directory[260])
{
  int rc, dummy;
  REG_TYPE tdummy;
  
  rc = load_key (hive => strnicmp (folder_name, "common ", 7) == 0 ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                 key  => "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
                 name => folder_name,
            out value => directory,
            out value_length => dummy,
            out value_type   => tdummy);
            
  if (rc < 0)
  {
    rc = load_key (hive => HKEY_LOCAL_MACHINE,
                   key  => "Software\\Microsoft\\Windows\\CurrentVersion",
                   name => folder_name,
              out value => directory,
              out value_length => dummy,
              out value_type   => tdummy);
  }

  _unused dummy;
  _unused tdummy;
  
  return rc;
}

//---------------------------------------------------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------------------------------------------------
