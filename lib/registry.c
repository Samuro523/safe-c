
// registry.c

use strings, win/windows;

/************************************************************************/
#begin unsafe
/************************************************************************/

public int store_key (HIVE hive, string key, string name, byte[] value, REG_TYPE type)
{
  char keyZ[260], nameZ[260];
  HKEY rkey;
  LSTATUS  rc;

  sprintf (out keyZ, key);
  keyZ[keyZ'length-1] = nul;

  sprintf (out nameZ, name);
  nameZ[nameZ'length-1] = nul;

  rc = RegCreateKeyExA ((int)(0x80000000 + (uint)hive),
                        &keyZ,
                        0,
                        null,
                        0,       // REG_OPTION_NON_VOLATILE
                        0x0002,  // KEY_SET_VALUE
                        null,
                        &rkey,
                        null);

  if (rc != 0)
    return -1;

  rc = RegSetValueExA (rkey, &nameZ, 0, (uint)type, &value, value'size);

  RegCloseKey (rkey);

  if (rc != 0)
    return -1;

  return 0;
}

/************************************************************************/

public int load_key (HIVE hive, string key, string name, out byte[] value, out int value_length,
                     out REG_TYPE value_type)
{
  char    keyZ[260], nameZ[260];
  HKEY    rKey;
  LSTATUS rc;
  uint    len;

  value_length = 0;
  clear value, value_type;

  sprintf (out keyZ, key);
  keyZ[keyZ'length-1] = nul;

  sprintf (out nameZ, name);
  nameZ[nameZ'length-1] = nul;

  if (RegOpenKeyExA ((int)(0x80000000 + (uint)hive),
                     &keyZ,
                     0,
                     0x0001,  // KEY_QUERY_VALUE
                     &rKey) != 0)
    return -1;

  len = value'size;
  rc = RegQueryValueExA (rKey,
                         &nameZ,
                         null,
                         (uint*)&value_type,
                         &value,
                         &len);
  RegCloseKey (rKey);

  if (rc != 0)
    return -1;

  value_length = (int)len;

  return 0;
}

/************************************************************************/

public int enumerate_key (HIVE hive, string key, int index, out string value)
{
  char keyZ[260];
  HKEY rKey;
  int  rc;
  uint len;

  clear value;

  sprintf (out keyZ, key);
  keyZ[keyZ'length-1] = nul;

  if (RegOpenKeyExA ((int)(0x80000000 + (uint)hive),
                     &keyZ,
                     0,
                     0x0008,  // KEY_ENUMERATE_SUB_KEYS
                     &rKey) != 0)
    return -1;

  len = value'size;
  rc = RegEnumKeyExA (rKey, (DWORD)index, (LPSTR)&value, &len, null, null, null, null);

  RegCloseKey (rKey);

  if (rc != 0)
    return -1;

  return 0;
}

/************************************************************************/
#end unsafe
/************************************************************************/
