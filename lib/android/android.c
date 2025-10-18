
// android.c

use bionic, ../strings;

//---------------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------------

public
void init_android (android_app *app)
{
  g_app = app;

  // set asset manager handle, so we can retrieve assets (for crash report)
  android.g_assetManager = app->activity->assetManager;

  // set internal directory as current directory, so we can create local files
  chdir (app->activity->internalDataPath);
}

//--------------------------------------------------------------------------------

public
byte[]^ android_load_asset (string filename)
{
  string^ filenamez;
  long    aset;
  byte*   p;
  int     size;
  byte[]^ rv;

  if (g_assetManager == 0)
    return null;

  // make sure it's null-terminated
  filenamez = new string (filename'length + 1);
  filenamez^[0 : filename'length] = filename;

  aset = AAssetManager_open (g_assetManager, &filenamez^, AASSET_MODE_BUFFER);

  free filenamez;

  if (aset == 0)
    return null;

  p = AAsset_getBuffer(aset);

  if (p == null)
  {
    AAsset_close(aset);
    return null;
  }

  size = (int)AAsset_getLength64(aset);

  rv = new byte[]' (p[0:size]);

  AAsset_close(aset);

  return rv;
}

//---------------------------------------------------------------------

public
byte* android_load_asset2 (string filename, out int size)
{
  char*   filenamez;
  int     len;
  long    aset;
  byte*   p;
  byte*   rv;

  size = 0;

  if (g_assetManager == 0)
    return null;

  // make sure it's null-terminated
  len = strlen(filename);
  filenamez = (char*)malloc ((uint)len + 1);
  filenamez[0 : len] = filename[0 : len];
  filenamez[len] = nul;

  aset = AAssetManager_open (g_assetManager, filenamez, AASSET_MODE_BUFFER);
  
  freem ((byte*)filenamez);
  
  if (aset == 0)
    return null;

  p = AAsset_getBuffer(aset);
  if (p == null)
  {
    AAsset_close(aset);
    return null;
  }

  size = (int)AAsset_getLength64(aset);

  rv = malloc ((uint)size);
  rv[0:size] = p[0:size];

  AAsset_close(aset);

  return rv;
}

//---------------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------------
