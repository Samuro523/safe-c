
// registry.h : load/store registry key (windows-specific)

//--------------------------------------------------------------------------

enum HIVE {HKEY_CLASSES_ROOT, HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, HKEY_USERS,
           HKEY_PERFORMANCE_DATA, HKEY_CURRENT_CONFIG, HKEY_DYN_DATA};

//--------------------------------------------------------------------------

enum REG_TYPE {REG_NONE, 
               REG_SZ, 
               REG_EXPAND_SZ,   // can contain envir variables like "%PATH%"
               REG_BINARY, 
               REG_DWORD, 
               REG_DWORD_BIG_ENDIAN,
               REG_LINK, 
               REG_MULTI_SZ };  // example: "string1\0String2\0String3\0LastString\0\0" 

//--------------------------------------------------------------------------

// store value in registry key/name (overwrites if exists)
// returns 0 if OK, a negative value if error

int store_key (HIVE     hive,
               string   key,    // key path, ex: "a\\b\\c"
               string   name,   // parameter name, ex: "tcp-timeout"
               byte[]   value,
               REG_TYPE type);

//--------------------------------------------------------------------------

// load value from registry key/name
// returns 0 if OK, a negative value if error

int load_key (    HIVE     hive,
                  string   key,
                  string   name,
              out byte[]   value,
              out int      value_length,
              out REG_TYPE value_type);

//--------------------------------------------------------------------------

int enumerate_key (    HIVE   hive, 
                       string key,     // "" or key name
                       int    index,   // 0, 1, ..
                   out string value);
                   
//--------------------------------------------------------------------------
