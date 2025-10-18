
// system.c

#if WINDOWS
  use win/windows;

#elif ANDROID
  from std use android/android, strings;
  use logging;
  
#else
  badd
  
#endif


/***********************************************************************/
#begin unsafe
/***********************************************************************/

public
void message_box (string title, string message)
{
#if WINDOWS
  string^ ptitle = new string (title'length+1);
  string^ pmessage = new string (message'length+1);

  ptitle^  [0:title  'length] = title;
  pmessage^[0:message'length] = message;

  MessageBoxA (windows.main_hWnd, &pmessage^, &ptitle^, MB_OK);

  free ptitle, pmessage;
  
#elif ANDROID
  log ("%s : %s", title, message);  // $ android to do

#else
  bad
  
#endif  
}

/***********************************************************************/

public void get_computer_name (out string(MAX_COMPUTERNAME_LENGTH) computer_name)
{
#if WINDOWS
  uint siz = computer_name'size;
  if (GetComputerNameA (&computer_name, &siz) == 0)
    computer_name[0] = nul;

#elif ANDROID
  clear computer_name;  // $ android to do

#else
  bad
  
#endif  
}

/***********************************************************************/

public void get_executable_filename (out string(MAX_EXECUTABLE_FILENAME_LENGTH) executable_filename)
{
#if WINDOWS
  GetModuleFileNameA (0, &executable_filename, (DWORD)executable_filename'length);

#elif ANDROID
  clear executable_filename;  // $ android to do

#else
  bad
  
#endif  
}

/***********************************************************************/

public void wget_executable_filename (out wstring(MAX_EXECUTABLE_FILENAME_LENGTH) executable_filename)
{
#if WINDOWS
  GetModuleFileNameW (0, &executable_filename, (DWORD)executable_filename'length);

#elif ANDROID
  clear executable_filename;  // $ android to do

#else
  bad
  
#endif  
}

/***********************************************************************/

public void get_login_userid (out string(MAX_USERID_LENGTH) userid)
{
#if WINDOWS
  uint siz = userid'size;
  if (GetUserNameA (&userid, &siz) == 0)
    userid[0] = '\0';

#elif ANDROID
  clear userid;  // $ android to do

#else
  bad
  
#endif  
}

/***********************************************************************/

// physical RAM on the PC

public long get_total_physical_RAM ()
{
#if WINDOWS
  MEMORYSTATUSEX m;
  clear m;
  m.dwLength = m'size;
  GlobalMemoryStatusEx (&m);
  return m.ullTotalPhys;

#elif ANDROID
  return 1024*1024;  // $ android to do

#else
  bad
  
#endif  
}

/***********************************************************************/

// returns free memory (in bytes)

public long get_free_memory ()
{
#if WINDOWS
  MEMORYSTATUSEX m;
  
  clear m;
  m.dwLength = m'size;
  GlobalMemoryStatusEx (&m);
  
#if MEM32  
  return m.ullAvailVirtual;
#else  
  return m.ullAvailPageFile;
#endif

#elif ANDROID
  return 1024*1024;  // $ android to do

#else
  bad
  
#endif  
}

//--------------------------------------------------------------------------

// A number between 0 and 100 that specifies the approximate percentage of physical memory 
// that is in use (0 indicates no memory use and 100 indicates full memory use).

public int get_virtual_memory_load ()
{
#if WINDOWS
  MEMORYSTATUSEX m;

  clear m;
  m.dwLength = m'size;
  GlobalMemoryStatusEx (&m);

  return (int)m.dwMemoryLoad;

#elif ANDROID
  return 50;  // $ android to do

#else
  bad
  
#endif  
}

/***********************************************************************/

public int get_nb_of_cores ()
{
#if WINDOWS
  SYSTEM_INFO v;
  GetSystemInfo (&v);
  return (int)v.dwNumberOfProcessors;

#elif ANDROID
  return 1;  // $ android to do

#else
  bad
  
#endif  
}

/***********************************************************************/

#if ANDROID

void android_get_langage (out char[2] lang)   // "fr"
{
  JavaVM* javaVM = (JavaVM*)g_app->activity->vm;
  JNIEnv* env;
  jclass localeClass;
  jmethodID getDefault;
  jobject defaultLocale;
  jmethodID getLanguage;
  jstring langStr;
  char* language;

  const string LOCALE   = "java/util/Locale\0";
  const string GET_DEFAULT = "getDefault\0";
  const string LOCALE2  = "()Ljava/util/Locale;\0";
  const string GETLANG  = "getLanguage\0";
  const string GETLANG2 = "()Ljava/lang/String;\0";

  clear lang;

  (*javaVM)->AttachCurrentThread(javaVM, &env, null);
  if (env == null)
  {
//    log ("env null");
    return;
  }

  localeClass = (*env)->FindClass(env, &LOCALE);
  if (localeClass == null)
  {
//    log ("localeClass null");
    return;
  }

  getDefault = (*env)->GetStaticMethodID(env, localeClass, &GET_DEFAULT, &LOCALE2);
  if (getDefault == null)
  {
//    log ("getDefault null");
    return;
  }

  defaultLocale = (*env)->CallStaticObjectMethod(env, localeClass, getDefault);
  if (defaultLocale == null)
  {
//    log ("defaultLocale null");
    return;
  }

  getLanguage = (*env)->GetMethodID(env, localeClass, &GETLANG, &GETLANG2);
  if (getLanguage == null)
  {
//    log ("getLanguage null");
    return;
  }

  langStr = (jstring)(*env)->CallObjectMethod(env, defaultLocale, getLanguage);
  if (langStr == null)
  {
//    log ("langStr null");
    return;
  }

  language = (*env)->GetStringUTFChars(env, langStr, null);
  if (language == null)
  {
//    log ("language null");
    return;
  }
  
  lang = language[0:2];
  
  (*env)->ReleaseStringUTFChars(env, langStr, language);

  (*javaVM)->DetachCurrentThread(javaVM);
}

#endif

//---------------------------------------------------------------------

public void get_language (out uint lang, out uint sublang)
{
#if WINDOWS
  uint l = GetUserDefaultLangID();

  lang    = ((ushort)l) & 0x3FF;
  sublang = ((ushort)l) >> 10;

#elif ANDROID
  {
    char[2] clang;
    android_get_langage (out clang);   // "fr"
    if (memcmp (clang, "fr") == 0)
      lang = LANG_FRENCH;
    else if (memcmp (clang, "de") == 0)
      lang = LANG_GERMAN;
    else
      lang = LANG_ENGLISH;
    sublang = 0;
  }

#else
  bad
  
#endif  
}

/***********************************************************************/

// retrieve operating system directory ("c:\windows")

public void get_operating_system_directory (out char dir[260])
{
#if WINDOWS
  clear dir;
  GetWindowsDirectoryA (&dir, dir'size-1);

#elif ANDROID
  clear dir;

#else
  bad
  
#endif  
}

/***********************************************************************/

public void get_country (out wchar[3] country)
{
#if WINDOWS
  const string KERNEL32DLL = "kernel32.dll\0";
  const string GETUSERDEFAULTGEONAME = "GetUserDefaultGeoName\0";
  HMODULE h;
  PVOID   func;
  GetUserDefaultGeoName pGetUserDefaultGeoName;
  wchar name[4];

  clear country;

  h = GetModuleHandleA (&KERNEL32DLL);
  if (h == 0)
    return;
    
  func = GetProcAddress (h, &GETUSERDEFAULTGEONAME);
  *(LPVOID*)&pGetUserDefaultGeoName = func;

  if (pGetUserDefaultGeoName != null)
  {
    if (pGetUserDefaultGeoName (&name, name'length) != 0)
      country = name[0:3];
  }

#elif ANDROID
  clear country;

#else
  bad
  
#endif  
}

/***********************************************************************/

public uint idle_msec ()
{
#if WINDOWS
  LASTINPUTINFO info;
  
  clear info;
  info.cbSize = info'size;
  
  if (GetLastInputInfo (&info) == 1)
  {
    return GetTickCount() - info.dwTime;
  }
  else
  {
    return 0;
  }

#elif ANDROID
  return 0;

#else
  bad
  
#endif  
}

/***********************************************************************/
#end unsafe
/***********************************************************************/
