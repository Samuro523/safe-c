
// mf.c : media foundation

use ../memory, windows;

//============================================================================================

public bool SUCCEEDED (HRESULT hr)
{
  return hr >= 0;
}

//============================================================================================

public bool FAILED (HRESULT hr)
{
  return hr < 0;
}

//============================================================================================

#begin unsafe
public HRESULT MFGetAttributeSize (IMFMediaType* pAttributes, REFGUID guidKey, UINT32 *punWidth, UINT32 *punHeight)
{
  UINT64 val;
  HRESULT hres;

  hres = pAttributes->lpVtbl->GetUINT64((LPVOID*)pAttributes, guidKey, &val);
  if (SUCCEEDED(hres))
  {
    (*punHeight)'byte = val'byte[0:4];
    (*punWidth)'byte = val'byte[4:4];
  }

  return hres;
}
#end unsafe

//============================================================================================

#begin unsafe
public HRESULT MFSetAttributeSize (IMFMediaType* pAttributes, REFGUID guidKey, UINT32 unWidth, UINT32 unHeight)
{
  UINT64 val;

  clear val;
  val'byte[0:4] = unHeight'byte;
  val'byte[4:4] = unWidth'byte;

  return pAttributes->lpVtbl->SetUINT64((LPVOID*)pAttributes, guidKey, val);
}
#end unsafe

/****************************************************************************/
#begin unsafe
/****************************************************************************/

public MF_CALLS* init_mf_calls ()
{
  const string dll_name = "mf.dll\0";
  HMODULE      hinstLib;
  MF_CALLS*    p;

  hinstLib = LoadLibraryA (&dll_name);
  if (hinstLib == 0)
    return null;

  p = (MF_CALLS*) malloc (MF_CALLS'size);

  {
    const string func_name = "MFCreateMediaSession\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFCreateMediaSession = *(LPVOID*)&f;
  }

  {
    const string func_name = "MFGetService\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFGetService = *(LPVOID*)&f;
  }

  {
    const string func_name = "MFCreateTopology\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFCreateTopology = *(LPVOID*)&f;
  }

  {
    const string func_name = "MFCreateTopologyNode\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFCreateTopologyNode = *(LPVOID*)&f;
  }

  {
    const string func_name = "MFCreateAudioRendererActivate\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFCreateAudioRendererActivate = *(LPVOID*)&f;
  }

  {
    const string func_name = "MFCreateVideoRendererActivate\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFCreateVideoRendererActivate = *(LPVOID*)&f;
  }

  {
    const string func_name = "MFCreateSourceResolver\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFCreateSourceResolver = *(LPVOID*)&f;
  }

  {
    const string func_name = "MFEnumDeviceSources\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFEnumDeviceSources = *(LPVOID*)&f;
  }

  return p;
}

/****************************************************************************/

public MF_PLAT_CALLS* init_mf_plat_calls ()
{
  const string    dll_name = "mfplat.dll\0";
  HMODULE         hinstLib;
  MF_PLAT_CALLS*  p;

  hinstLib = LoadLibraryA (&dll_name);
  if (hinstLib == 0)
    return null;

  p = (MF_PLAT_CALLS *)malloc (MF_PLAT_CALLS'size);

  {
    const string func_name = "MFStartup\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFStartup = *(LPVOID*)&f;
  }

  {
    const string func_name = "MFCreateAttributes\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFCreateAttributes = *(LPVOID*)&f;
  }

  {
    const string func_name = "MFShutdown\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFShutdown = *(LPVOID*)&f;
  }

  {
    const string func_name = "MFCreateMediaType\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFCreateMediaType = *(LPVOID*)&f;
  }


/*
  {
    const string func_name = "MFGetStrideForBitmapInfoHeader\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFGetStrideForBitmapInfoHeader = *(LPVOID*)&f;
  }
*/

  {
    const string func_name = "MFCreateDXGIDeviceManager\0";   // windows 8
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    *(LPVOID*)&p->MFCreateDXGIDeviceManager = *(LPVOID*)&f;
  }

  return p;
}

/****************************************************************************/

public MF_READ_WRITE_CALLS* init_mf_read_write_calls ()
{
  const string    dll_name = "Mfreadwrite.dll\0";
  HMODULE         hinstLib;
  MF_READ_WRITE_CALLS*  p;

  hinstLib = LoadLibraryA (&dll_name);
  if (hinstLib == 0)
    return null;

  p = (MF_READ_WRITE_CALLS *)malloc (MF_READ_WRITE_CALLS'size);

  {
    const string func_name = "MFCreateSourceReaderFromMediaSource\0";
    LPVOID f = GetProcAddress (hinstLib, &func_name);
    if (f == null)
      return null;
    *(LPVOID*)&p->MFCreateSourceReaderFromMediaSource = *(LPVOID*)&f;
  }

  return p;
}

/****************************************************************************/
#end unsafe
/****************************************************************************/
