
// webcam.c - webcam image capture

#if WINDOWS

use arithm, image, strings, thread, win/windows, win/direct_show, win/mf;

//---------------------------------------------------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------------------------------------------------

#define  debug       0 
#define  debug_error 0 

#if debug || debug_error
  from std use tracing;
#endif

//---------------------------------------------------------------------------------------------------------

const int MAX_BUFFERS = 4;

//---------------------------------------------------------------------------------------------------------

enum STATE {EMPTY, WRITTEN, WRITTEN_AND_READ};

struct WEBCAM_CAPTURE
{
  // !! vector table must always be first field of struct !!!
  LPVOID  lpVtbl;          // ISampleGrabberCB or IMFSourceReaderCallback
  uint    threadId;
  int     thread_status;  // 0 = not running or waiting to initialize
                          // +1 = running, success
                          // -1 = not running, failure

  // dimensions requested by user
  int  width_out;
  int  height_out;

  WEBCAM_NAME source_name;

  bool  use_media_foundation;  // false = use directshow layer, true = use media foundation layer

  // for directshow layer

  IBaseFilter*    pCap;     // the capture filter
  IGraphBuilder*  pGraph;   // the graph builder
  ISampleGrabber* pGrabber; // the sample grabber

  uint    bgr_line_size;
  uint    bgr_size;
  uint    rgbs_line_size;

  // for media foundation layer

  IMFSourceReader *pSourceReader;
  int             stride;
  DWORD           dwStreamIndex;
  bool            rearm_callback;

  // for both layers

  uint capture_width;  // dimension of captured images - will be set when capture begins, can change if media changes
  uint capture_height;

  uint    buffer1_size;
  byte[]^ buffer1[MAX_BUFFERS];       // capture_width, capture_height
  uint    buffer2_size;
  byte[]^ buffer2[MAX_BUFFERS];       // width_out, height_out
  STATE   state[MAX_BUFFERS];
  int     nb_locks[MAX_BUFFERS];
  int     current_buffer_nr;
  int     current_image_nr;
}

//---------------------------------------------------------------------------------------------------------

SHARED_OBJECT g_thread_safe;  // makes calls from public functions thread-safe

//---------------------------------------------------------------------------------------------------------

// for media foundation
SHARED_OBJECT        g_so;
int                  g_mf_status;   // MF initialization : 0=not done, 1=success, -1=failure
MF_CALLS*            mf;
MF_PLAT_CALLS*       mf_plat;
MF_READ_WRITE_CALLS* mf_read_write_calls;

//---------------------------------------------------------------------------------------------------------

// in case g_mf_status is 0, initialize media foundation and set it to +1 or -1

void init_media_foundation()
{
  if (g_mf_status != 0)  // already done
    return;

  enter_shared_object (ref g_so);

  if (g_mf_status == 0)   // media foundation not initialized yet
  {
    mf                  = init_mf_calls ();
    mf_plat             = init_mf_plat_calls ();
    mf_read_write_calls = init_mf_read_write_calls ();

    if (mf != null && mf_plat != null && mf_read_write_calls != null)
    {
      // Start up Media Foundation platform.
      HRESULT hr = mf_plat->MFStartup (MF_VERSION,
                                       MFSTARTUP_NOSOCKET);  // don't initialize socket library
      if (SUCCEEDED(hr))
        g_mf_status = +1;
      else
        g_mf_status = -1;
    }
    else
      g_mf_status = -1;
  }

  leave_shared_object (ref g_so);
}

//---------------------------------------------------------------------------------------------------------

// for DirectShow
// returns nb of sources

int get_source_list_using_direct_show (out WEBCAM_NAME[] list)
{
  int             rc, count;
  LPVOID          p;
  ICreateDevEnum* pCreateDevEnum;
  IEnumMoniker*   pEm;
  IMoniker*       pM;

  clear list;

  clear p;
  CoCreateInstance (CLSID_SystemDeviceEnum, null, CLSCTX_ALL, IID_CreateDevEnum, out p);
  if (p == null)
    return 0;
  pCreateDevEnum = (ICreateDevEnum*) p;

  clear pEm;
  rc = pCreateDevEnum->lpVtbl->CreateClassEnumerator (pCreateDevEnum,
                                                      CLSID_VideoInputDeviceCategory,
                                                      &pEm,
                                                      dwFlags => 0);
  if (pEm == null)
  {
    SafeRelease((LPVOID**)&pCreateDevEnum);
    return 0;
  }

  pEm->lpVtbl->Reset (pEm);

  count = 0;

  while (count < list'length)
  {
    pM = null;
    if (pEm->lpVtbl->Next (pEm, 1, out pM, null) != S_OK)
      break;

    p = null;
    rc = pM->lpVtbl->BindToStorage ((LPVOID)pM, false, null, IID_IPropertyBag, out p);
    if (rc == S_OK)
    {
      IPropertyBag* pBag = (IPropertyBag*) p;
      VARIANT       var;
      const wstring FRIENDLY = L"FriendlyName\0";

      clear var;
      var.vt = VT_BSTR;
      rc = pBag->lpVtbl->Read (pBag, &FRIENDLY, ref var, null);
      if (rc == S_OK)
        wsprintf (out list[count++], L"DS:%S", var.tagv.bstrVal[0:list'length(2)-3]);

      SafeRelease((LPVOID**)&pBag);
    }

    SafeRelease((LPVOID**)&pM);
  }

  SafeRelease((LPVOID**)&pEm);
  SafeRelease((LPVOID**)&pCreateDevEnum);

  return count;
}

//---------------------------------------------------------------------------------------------------------

// for Media Foundation
// returns nb of sources

int get_source_list_using_media_foundation (out WEBCAM_NAME[] list)
{
  IMFAttributes *pAttributes = null;
  IMFActivate **ppDevices = null;
  HRESULT hr;
  UINT32 count = 0;
  int    list_count = 0;

  clear list;

  // Create an attribute store to specify the enumeration parameters.
  hr = mf_plat->MFCreateAttributes(&pAttributes, 1);
  if (SUCCEEDED(hr))
  {
    // Source type: video capture devices
    hr = pAttributes->lpVtbl->SetGUID((LPVOID*)pAttributes,
                                      &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                                      &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (SUCCEEDED(hr))
    {
      // Enumerate devices.
      hr = mf->MFEnumDeviceSources (pAttributes, &ppDevices, &count);
      if (SUCCEEDED(hr))
      {
        DWORD i;
        for (i = 0; i < count; i++)
        {
          UINT32 pcchLength;
          LPWSTR ppwszValue;

          hr = ppDevices[i]->lpVtbl->GetAllocatedString ((LPVOID*)ppDevices[i], &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &ppwszValue, &pcchLength);
          if (SUCCEEDED(hr))
          {
            if (list_count < list'length)
              wsprintf (out list[list_count++], L"%S", ppwszValue[0 : list'length(2)]);
            CoTaskMemFree ((LPVOID)ppwszValue);
          }

          SafeRelease((LPVOID**)&ppDevices[i]);
        }

        CoTaskMemFree((LPVOID)ppDevices);
      }
    }

    SafeRelease((LPVOID**)&pAttributes);
  }

  return list_count;
}

//---------------------------------------------------------------------------------------------------------

package GET_SOURCE_LIST

  struct SOURCE_LIST_PARAMS
  {
    int          count;
    int          max_count;
    WEBCAM_NAME* list;
    bool         done;
  }

end GET_SOURCE_LIST;

//---------------------------------------------------------------------------------------------------------

void get_source_list_thread_for_direct_show (SOURCE_LIST_PARAMS *par)
{
  int rc;

  rc = CoInitializeEx (null, COINIT_APARTMENTTHREADED);
  if (rc == S_OK || rc == S_FALSE)
  {
    par->count += get_source_list_using_direct_show (out par->list[par->count : par->max_count - par->count]);
    CoUninitialize();
  }

  par->done = true;
}

//---------------------------------------------------------------------------------------------------------

void get_source_list_thread_for_media_foundation (SOURCE_LIST_PARAMS *par)
{
  HRESULT hr;

  hr = CoInitializeEx (null, COINIT_MULTITHREADED);
  if (hr == 0)
  {
    init_media_foundation();  // in case g_mf_status is 0, initialize media foundation and set it to +1 or -1
    if (g_mf_status > 0)
      par->count += get_source_list_using_media_foundation (out par->list[par->count : par->max_count - par->count]);
    CoUninitialize ();
  }

  par->done = true;
}

//---------------------------------------------------------------------------------------------------------

public int get_webcam_list (out WEBCAM_NAME[] list)
{
  SOURCE_LIST_PARAMS par;
  int                rc;

  par = {count     => 0,
         max_count => list'length,
         list      => &list,
         done      => false};

  enter_shared_object (ref g_thread_safe);

  rc = run get_source_list_thread_for_media_foundation (&par);
  if (rc == 0)
  {
    while (!par.done)
      sleep 0.0020;
  }

  par.done = false;

  rc = run get_source_list_thread_for_direct_show (&par);
  if (rc == 0)
  {
    while (!par.done)
      sleep 0.0020;
  }

  leave_shared_object (ref g_thread_safe);

  return par.count;
}

//---------------------------------------------------------------------------------------------------------
// Direct Show functions
//---------------------------------------------------------------------------------------------------------

// convert BGR to RGBS

void convert_bgr_to_rgbs (    byte[] source_bgr,   // size info.bgr_size
                              uint   width,
                              uint   height,
                              uint   bgr_line_size,
                          ref byte[] target_rgbs)  // size info.buffer1_size
{
  byte*  psource, ptarget, psource2;
  uint   x, y;

  psource = &source_bgr + source_bgr'size;   // set to bottom line (note: source lines are stored bottom-up)
  ptarget = &target_rgbs;

  y = 0;
  for (;;)
  {
    psource -= bgr_line_size;  // set to source line corresponding to y target line
    psource2 = psource;

    x = 0;
    for (;;)
    {
      // convert BGR to RGBS
      ptarget[0] = psource2[2];
      ptarget[1] = psource2[1];
      ptarget[2] = psource2[0];
      ptarget[3] = 255;
      ptarget += 4;
      psource2 += 3;

      x++;
      if (x == width)
        break;
    }

    y++;
    if (y == height)
      break;
  }
}

//---------------------------------------------------------------------------------------------------------

[callback]
HRESULT MyQueryInterface_for_SampleGrabberCB (LPVOID This,  REFIID riid, out LPVOID ppvObject)
{
  if (memcmp (riid, IID_ISampleGrabberCB) == 0 || memcmp (riid, IID_IUnknown) == 0)
  {
    ppvObject = This;
    return NOERROR;
  }
  ppvObject = null;
  return E_NOINTERFACE;
}

//---------------------------------------------------------------------------------------------------------

[callback]
ULONG MyAddRef (LPVOID This)
{
  _unused This;
  return 2;
}

//---------------------------------------------------------------------------------------------------------

[callback]
ULONG MyRelease (LPVOID This)
{
  _unused This;
  return 1;
}

//---------------------------------------------------------------------------------------------------------

[callback]
HRESULT MySampleCB (PVOID This, double SampleTime, IMediaSample *pSample)
{
  ref WEBCAM_CAPTURE wc = *((WEBCAM_CAPTURE*)This);
  int i, buffer_nr, next_buffer_nr;

  _unused SampleTime;

#if debug
  trace ("GrabberCB: 0\n");
#endif

  for (i=0; i<60; i++)   // suspend for maximum 1 second
  {
    if (wc.state[wc.current_buffer_nr] != WRITTEN)
      break;
    // we have still an unread WRITTEN buffer, so wait a bit more, blocking the capture thread.
    sleep 1.0 / 60.0;
  }

  buffer_nr = wc.current_buffer_nr;
  next_buffer_nr = buffer_nr + 1;
  if (next_buffer_nr == MAX_BUFFERS)
    next_buffer_nr = 0;
  if (wc.nb_locks[next_buffer_nr] == 0)  // next slot is available : write it there
  {
    ref byte[]^ p = wc.buffer1[next_buffer_nr];
    ref byte[]^ q = wc.buffer2[next_buffer_nr];
    byte*       psource;
    uint        lBufferSize;

    pSample->lpVtbl->GetPointer ((LPVOID)pSample, &psource);
    if (psource == null)
    {
#if debug_error
  trace ("GrabberCB: null source buffer !\n");
#endif
      return 0;
    }

    lBufferSize = (uint)pSample->lpVtbl->GetActualDataLength ((LPVOID)pSample);

#if debug
  trace ("GrabberCB: 1 : size=%u\n", lBufferSize);
#endif

    if (lBufferSize != wc.bgr_size)
    {
#if debug_error
  trace ("GrabberCB: bad buffer size %u (should be %u) !\n", lBufferSize, wc.bgr_size);
#endif

      return 0;
    }

     // make sure if is allocated with correct size
    if (p == null || p^'size != wc.buffer1_size)
    {
      free p;
      p = new byte[wc.buffer1_size];
    }

     // make sure if is allocated with correct size
    if (q == null || q^'size != (uint)wc.buffer2_size)
    {
      free q;
      q = new byte[wc.buffer2_size];
    }

#if debug
  trace ("GrabberCB: 2 : capture %u x %u\n", wc.capture_width, wc.capture_height);
#endif

    convert_bgr_to_rgbs (    source_bgr    => psource[0:lBufferSize],   // size info.bgr_size
                             width         => wc.capture_width,
                             height        => wc.capture_height,
                             bgr_line_size => wc.bgr_line_size,
                         ref target_rgbs   => p^);                      // size info.buffer1_size

    if (wc.capture_width == (uint)wc.width_out && wc.capture_height == (uint)wc.height_out)
    {
      byte[]^ temp = q;
      q = p;
      p = temp;
    }
    else
    {
      IMAGE_INFO img1  = {pixel => p, width => wc.capture_width,   height => wc.capture_height};
      IMAGE_INFO img2  = {pixel => q, width => (uint)wc.width_out, height => (uint)wc.height_out};
      CLIP_INFO  clip1 = {offset_x => 0, offset_y => 0, size_x => img1.width, size_y => img1.height};
      CLIP_INFO  clip2 = {offset_x => 0, offset_y => 0, size_x => img2.width, size_y => img2.height};

      assert stretch_image (img1, clip1, img2, clip2, use_linear_colors => false, high_quality => false) == 0;
    }

    enter_shared_object (ref g_thread_safe);

    if (wc.state[buffer_nr] == WRITTEN_AND_READ)   // current buffer is or was in use
    {
      wc.current_buffer_nr = next_buffer_nr;      // switch to next buffer
    }
    else     // current buffer was not used, reuse it
    {
      // exchange wc.buffer2[buffer_nr] and wc.buffer2[next_buffer_nr]
      byte[]^ temp               = wc.buffer2[buffer_nr];
      wc.buffer2[buffer_nr]      = wc.buffer2[next_buffer_nr];
      wc.buffer2[next_buffer_nr] = temp;
    }

    wc.state[wc.current_buffer_nr] = WRITTEN;
    wc.current_image_nr = (wc.current_image_nr + 1) & 0x7FFFFFFF;   // make sure it's not -1 anymore

    leave_shared_object (ref g_thread_safe);
  }

#if debug
  trace ("GrabberCB: 3 : done (buffer=%d, image_nr=%d)\n", wc.current_buffer_nr, wc.current_image_nr);
#endif

  return 0;
}

//---------------------------------------------------------------------------------------------------------

[callback]
HRESULT MyBufferCB (PVOID This, double SampleTime, BYTE *pBuffer, int BufferLen)
{
  _unused This;
  _unused SampleTime;
  _unused pBuffer;
  _unused BufferLen;
  return 0;
}

//---------------------------------------------------------------------------------------------------------

package SampleGrabberCBVtbl
  ISampleGrabberCBVtbl  MySampleGrabberCBVtbl;    // vector table for callback COM object
end SampleGrabberCBVtbl;

//---------------------------------------------------------------------------------------------------------

void create_MySampleGrabberCB (ref WEBCAM_CAPTURE wc)
{
  // fill global virtual table (in case it's not already done)
  MySampleGrabberCBVtbl = {QueryInterface => MyQueryInterface_for_SampleGrabberCB,
                           AddRef         => MyAddRef,
                           Release        => MyRelease,
                           SampleCB       => MySampleCB,
                           BufferCB       => MyBufferCB};

  wc.lpVtbl = (LPVOID)&MySampleGrabberCBVtbl;
}

//---------------------------------------------------------------------------------------------------------

void get_capture_filter (WEBCAM_NAME webcam_name, out IBaseFilter* ppCap)
{
  int             rc;
  LPVOID          p;
  ICreateDevEnum* pCreateDevEnum;
  IEnumMoniker*   pEm;
  IMoniker*       pM;

  clear ppCap;

  clear p;
  CoCreateInstance (CLSID_SystemDeviceEnum, null, CLSCTX_ALL, IID_CreateDevEnum, out p);
  if (p == null)
    return;
  pCreateDevEnum = (ICreateDevEnum*) p;

  clear pEm;
  rc = pCreateDevEnum->lpVtbl->CreateClassEnumerator (pCreateDevEnum,
                                                      CLSID_VideoInputDeviceCategory,
                                                      &pEm,
                                                      dwFlags => 0);
  if (pEm == null)
  {
    SafeRelease((LPVOID**)&pCreateDevEnum);
    return;
  }

  pEm->lpVtbl->Reset (pEm);

  for (;;)
  {
    pM = null;
    if (pEm->lpVtbl->Next (pEm, 1, out pM, null) != S_OK)
      break;

    p = null;
    rc = pM->lpVtbl->BindToStorage ((LPVOID)pM, false, null, IID_IPropertyBag, out p);
    if (rc == S_OK)
    {
      IPropertyBag* pBag = (IPropertyBag*) p;
      VARIANT       var;
      const wstring FRIENDLY = L"FriendlyName\0";

      clear var;
      var.vt = VT_BSTR;
      rc = pBag->lpVtbl->Read (pBag, &FRIENDLY, ref var, null);
      if (rc == S_OK)
      {
        WEBCAM_NAME name;
        wsprintf (out name, L"DS:%S", var.tagv.bstrVal[0:name'length-3]);
        if (wstrcmp (name, webcam_name) == 0)
        {
          // activate object and bind to it

          pM->lpVtbl->BindToObject ((LPVOID)pM, null, null, IID_IBaseFilter, out p);
          ppCap = (IBaseFilter*)p;

          SafeRelease((LPVOID**)&pBag);
          SafeRelease((LPVOID**)&pM);
          SafeRelease((LPVOID**)&pEm);
          SafeRelease((LPVOID**)&pCreateDevEnum);

          return;
        }
      }

      SafeRelease((LPVOID**)&pBag);
    }

    SafeRelease((LPVOID**)&pM);
  }

  SafeRelease((LPVOID**)&pEm);
  SafeRelease((LPVOID**)&pCreateDevEnum);
}

//---------------------------------------------------------------------------------------------------------

// Release the format block for a media type.

void FreeMediaType (ref AM_MEDIA_TYPE mt)
{
  if (mt.cbFormat != 0)
  {
    CoTaskMemFree((PVOID)mt.pbFormat);
    mt.cbFormat = 0;
    mt.pbFormat = null;
  }

  SafeRelease((LPVOID**)&mt.pUnk);
}

//---------------------------------------------------------------------------------------------------------

void DeleteMediaType (AM_MEDIA_TYPE* pmt)
{
  if (pmt != null)
  {
    FreeMediaType (ref *pmt);
    CoTaskMemFree ((LPVOID)pmt);
  }
}

//---------------------------------------------------------------------------------------------------------

// called by windows thread in case a function fails during start,
// or by capture tread when the grabber is stopped.

void free_all_for_direct_show (ref WEBCAM_CAPTURE info)
{
  SafeRelease((LPVOID**)&info.pGrabber);
  SafeRelease((LPVOID**)&info.pCap);
  SafeRelease((LPVOID**)&info.pGraph);
}

//---------------------------------------------------------------------------------------------------------

HRESULT GetPin (IBaseFilter* pFilter, PIN_DIRECTION dirrequired, out IPin *ppPin)
{
  IEnumPins* pEnum;
  IPin*      pPin;
  HRESULT    hr;
  ULONG      ulFound;

  ppPin = null;

  if (pFilter == null)
    return E_POINTER;

  hr = pFilter->lpVtbl->EnumPins ((LPVOID)pFilter, &pEnum);
  if (hr < 0)
    return hr;

  hr = E_FAIL;

  while (pEnum->lpVtbl->Next ((LPVOID)pEnum, 1, &pPin, &ulFound) == S_OK)
  {
    PIN_DIRECTION pindir;

    pPin->lpVtbl->QueryDirection ((LPVOID)pPin, &pindir);
    if (pindir == dirrequired)
    {
      ppPin = pPin;  // Return the pin's interface
      hr = S_OK;     // Found requested pin, so clear error
      break;
    }

    SafeRelease((LPVOID**)&pPin);
  }

  SafeRelease((LPVOID**)&pEnum);

  return hr;
}

//---------------------------------------------------------------------------------------------------------

int start_capture_for_direct_show (ref WEBCAM_CAPTURE info)
{
  HRESULT                hr;
  LPVOID                 p;
  IBaseFilter*           pGrabBase;
  ICaptureGraphBuilder2* pCGB2;
  IBaseFilter*           pRenderer;
  IPin*                  pSourcePin;
  AM_MEDIA_TYPE          VideoType;

  create_MySampleGrabberCB (ref info);


  // get the capture filter

  get_capture_filter (info.source_name, out info.pCap);
  if (info.pCap == null)
  {
#if debug_error
  trace ("get_capture_filter() failed\n");
#endif

    return -1;
  }


  // create a graph

  clear p;
  CoCreateInstance (CLSID_FilterGraph, null, CLSCTX_ALL, IID_IGraphBuilder, out p);
  if (p == null)
  {
#if debug_error
  trace ("CoCreateInstance (CLSID_FilterGraph) failed\n");
#endif
    free_all_for_direct_show (ref info);
    return -1;
  }
  info.pGraph = (IGraphBuilder *) p;


  // add the capture filter to the graph

  hr = info.pGraph->lpVtbl->AddFilter ((LPVOID)info.pGraph, info.pCap, null);
  if (hr < 0)
  {
#if debug_error
   trace ("AddFilter(pCap) failed\n");
#endif
    free_all_for_direct_show (ref info);
    return -1;
  }


  // create a sample grabber

  clear p;
  CoCreateInstance (CLSID_SampleGrabber, null, CLSCTX_ALL, IID_ISampleGrabber, out p);
  if (p == null)
  {
#if debug_error
  trace ("CoCreateInstance (CLSID_SampleGrabber) failed\n");
#endif
    free_all_for_direct_show (ref info);
    return -1;
  }
  info.pGrabber = (ISampleGrabber*) p;

  info.pGrabber->lpVtbl->QueryInterface ((LPVOID)info.pGrabber, IID_IBaseFilter, out p);
  if (p == null)
  {
#if debug_error
  trace ("QueryInterface(pGrabber) failed\n");
#endif
    free_all_for_direct_show (ref info);
    return -1;
  }
  pGrabBase = (IBaseFilter *) p;


  // force it to connect to video, uncompressed 24 bit

  clear VideoType;
  VideoType.majortype = MEDIATYPE_Video;
  VideoType.subtype   = MEDIASUBTYPE_RGB24;
  VideoType.bFixedSizeSamples = 1;  // true
  VideoType.lSampleSize = 1;

  hr = info.pGrabber->lpVtbl->SetMediaType ((LPVOID)info.pGrabber, VideoType);
  if (hr < 0)
  {
#if debug_error
  trace ("SetMediaType(pGrabber) failed\n");
#endif
    SafeRelease((LPVOID**)&pGrabBase);
    free_all_for_direct_show (ref info);
    return -1;
  }


  // add the grabber to the graph

  hr = info.pGraph->lpVtbl->AddFilter ((LPVOID)info.pGraph, pGrabBase, null);
  if (hr < 0)
  {
#if debug_error
  trace ("AddFilter(pGrabBase) failed\n");
#endif
    SafeRelease((LPVOID**)&pGrabBase);
    free_all_for_direct_show (ref info);
    return -1;
  }


  // build the graph using CaptureGraphBuilder2

  clear p;
  CoCreateInstance (CLSID_CaptureGraphBuilder2, null, CLSCTX_INPROC, IID_ICaptureGraphBuilder2, out p);
  if (p == null)
  {
#if debug_error
  trace ("CoCreateInstance (CLSID_CaptureGraphBuilder2) failed\n");
#endif
    SafeRelease((LPVOID**)&pGrabBase);
    free_all_for_direct_show (ref info);
    return -1;
  }
  pCGB2 = (ICaptureGraphBuilder2*) p;

  hr = pCGB2->lpVtbl->SetFiltergraph ((LPVOID)pCGB2, info.pGraph);
  if (hr < 0)
  {
#if debug_error
  trace ("SetFiltergraph() failed\n");
#endif
    SafeRelease((LPVOID**)&pCGB2);
    SafeRelease((LPVOID**)&pGrabBase);
    free_all_for_direct_show (ref info);
    return -1;
  }


  // setup the PREVIEW GRAPH

  // put the preview renderer on CLSID_NullRenderer

  clear p;
  CoCreateInstance (CLSID_NullRenderer, null, CLSCTX_ALL, IID_IBaseFilter, out p);
  if (p == null)
  {
#if debug_error
  trace ("create(NullRenderer) failed\n");
#endif
    SafeRelease((LPVOID**)&pCGB2);
    SafeRelease((LPVOID**)&pGrabBase);
    free_all_for_direct_show (ref info);
    return -1;
  }
  pRenderer = (IBaseFilter*) p;

  if (info.pGraph->lpVtbl->AddFilter((LPVOID)info.pGraph, pRenderer, null) < 0)
  {
#if debug_error
  trace ("AddFilter(NullRenderer)(1) failed\n");
#endif
    SafeRelease((LPVOID**)&pRenderer);
    SafeRelease((LPVOID**)&pCGB2);
    SafeRelease((LPVOID**)&pGrabBase);
    free_all_for_direct_show (ref info);
    return -1;
  }

  hr = pCGB2->lpVtbl->RenderStream ((LPVOID)pCGB2, PIN_CATEGORY_PREVIEW, MEDIATYPE_Video,
                                    (IUnknown *)info.pCap, null, pRenderer);
  if (hr < 0)
  {
    hr = pCGB2->lpVtbl->RenderStream ((LPVOID)pCGB2, PIN_CATEGORY_PREVIEW, MEDIATYPE_Interleaved,
                                      (IUnknown *)info.pCap, null, pRenderer);
  }

  // pRenderer no longer needed
  SafeRelease((LPVOID**)&pRenderer);

  if (hr < 0)
  {
#if debug_error
  trace ("RenderStream(preview) failed\n");
#endif
    SafeRelease((LPVOID**)&pCGB2);
    SafeRelease((LPVOID**)&pGrabBase);
    free_all_for_direct_show (ref info);
    return -1;
  }


  // setup the CAPTURE GRAPH

  // create Null Renderer

  clear p;
  CoCreateInstance (CLSID_NullRenderer, null, CLSCTX_ALL, IID_IBaseFilter, out p);
  if (p == null)
  {
#if debug_error
  trace ("create(NullRenderer) failed\n");
#endif
    SafeRelease((LPVOID**)&pCGB2);
    SafeRelease((LPVOID**)&pGrabBase);
    free_all_for_direct_show (ref info);
    return -1;
  }
  pRenderer = (IBaseFilter*) p;

  if (info.pGraph->lpVtbl->AddFilter((LPVOID)info.pGraph, pRenderer, null) < 0)
  {
#if debug_error
  trace ("AddFilter(NullRenderer)(2) failed\n");
#endif
    SafeRelease((LPVOID**)&pRenderer);
    SafeRelease((LPVOID**)&pCGB2);
    SafeRelease((LPVOID**)&pGrabBase);
    free_all_for_direct_show (ref info);
    return -1;
  }

  hr = pCGB2->lpVtbl->RenderStream ((LPVOID)pCGB2, PIN_CATEGORY_CAPTURE, MEDIATYPE_Video,
                                    (IUnknown *)info.pCap, pGrabBase, pRenderer);
  if (hr < 0)
  {
    hr = pCGB2->lpVtbl->RenderStream ((LPVOID)pCGB2, PIN_CATEGORY_CAPTURE, MEDIATYPE_Interleaved,
                                      (IUnknown *)info.pCap, pGrabBase, pRenderer);
  }

  // pRenderer no longer needed
  SafeRelease((LPVOID**)&pRenderer);

  if (hr < 0)
  {
#if debug_error
  trace ("RenderStream(capture) failed\n");
#endif
    SafeRelease((LPVOID**)&pCGB2);
    SafeRelease((LPVOID**)&pGrabBase);
    free_all_for_direct_show (ref info);
    return -1;
  }

  // pGrabBase no longer needed
  SafeRelease((LPVOID**)&pGrabBase);



  hr = pCGB2->lpVtbl->FindPin ((LPVOID)pCGB2, (IUnknown *)info.pCap, PINDIR_OUTPUT, PIN_CATEGORY_CAPTURE,
                               null, FALSE, 0, out pSourcePin);
  if (hr == S_OK)
  {
    /* LOGITECH 9000  320 x 240 : 15   160 x 120 : 15   176 x 144 : 15   352 x 288 : 15  640 x 480 : 15
                      800 x 600 : 15   960 x 720 : 15   1600 x 1200 : 5  */
    /* CREATIVE ULTRA 320 x 240 : 30   160 x 120 : 30   176 x 144 : 30   352 x 288 : 30  640 x 480 : 30 */

    pSourcePin->lpVtbl->QueryInterface ((LPVOID)pSourcePin, IID_IAMStreamConfig, out p);
    if (p != null)
    {
      IAMStreamConfig*         pVSC = (IAMStreamConfig*) p;
      VIDEO_STREAM_CONFIG_CAPS scc;
      int                      i, piCount, piSize;
      AM_MEDIA_TYPE*           pmt = null;

      hr = pVSC->lpVtbl->GetNumberOfCapabilities ((LPVOID)pVSC, &piCount, &piSize);

      if (hr == S_OK)
      {
        int best = 2_000_000_000;
        int pass;

        for (pass=1; pass<=2; pass++)
        {
          for (i=0; i<piCount; i++)
          {
            pVSC->lpVtbl->GetStreamCaps ((LPVOID)pVSC, i, &pmt, (BYTE *)&scc);
            if (pmt != null)
            {
              if (memcmp (pmt->formattype, FORMAT_VideoInfo) == 0 && pmt->cbFormat >= VIDEOINFOHEADER'size)
              {
                VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER *)pmt->pbFormat;

#if debug
 if (pass == 1) trace ("webcam.c : resolution %d x %d  bits %d  compression 0x%x  frames %d\n",
     pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, (int)pVih->bmiHeader.biBitCount, pVih->bmiHeader.biCompression, pVih->AvgTimePerFrame);
#endif

                if (pVih->bmiHeader.biBitCount == 24 &&
                    pVih->bmiHeader.biCompression == BI_RGB &&
                    pVih->bmiHeader.biWidth >= info.width_out &&
                    pVih->bmiHeader.biHeight >= info.height_out)
                {
                  int offset = (pVih->bmiHeader.biWidth - info.width_out)
                             + (pVih->bmiHeader.biHeight - info.height_out);

                  if (pass == 1)
                  {
                    if (offset < best)
                      best = offset;
                  }
                  else if (offset == best)
                  {
                    pVih->AvgTimePerFrame = (10000000 / 30);    // 30 fps

                    hr = pVSC->lpVtbl->SetFormat ((LPVOID)pVSC, pmt);
#if debug
  trace ("format %d x %d set rc=0x%x\n", pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, hr);
#endif
                    // 0x80040200  invalid media type
                    // 0xc00d36b4

                    pVih->bmiHeader.biWidth = info.width_out;
                    pVih->bmiHeader.biHeight = info.height_out;
                    hr = pVSC->lpVtbl->SetFormat ((LPVOID)pVSC, pmt);
#if debug
  trace ("format %d x %d set rc=0x%x\n", pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, hr);
#endif

                    DeleteMediaType (pmt);

                    break;
                  }
                }
              }

              DeleteMediaType (pmt);
            }
          }
        } // pass
      }

      SafeRelease((LPVOID**)&pVSC);
    }
  }


  // pCGB2 no longer needed.
  SafeRelease((LPVOID**)&pCGB2);

  // pSourcePin no longer needed.
  SafeRelease((LPVOID**)&pSourcePin);


  // ask for the connection media type so we know how large the captured bitmaps are.

  {
    AM_MEDIA_TYPE mt;

    hr = info.pGrabber->lpVtbl->GetConnectedMediaType ((LPVOID)info.pGrabber, out mt);
    if (hr < 0)
    {
#if debug_error
  trace ("GetConnectedMediaType() failed\n");
#endif
      free_all_for_direct_show (ref info);
      return -1;
    }


    {
      ref BITMAPINFOHEADER bh = ((VIDEOINFOHEADER *)mt.pbFormat)->bmiHeader;

      info.capture_width = (uint)bh.biWidth;
      info.capture_height = (uint)bh.biHeight;

#if debug
  trace ("grabber width=%u height=%u\n", info.capture_width, info.capture_height);
#endif

      info.bgr_line_size  = ((info.capture_width * 24 + 31) & (uint'max - 31)) >> 3;
      info.rgbs_line_size = 4*info.capture_width;

      info.bgr_size  = info.bgr_line_size  * info.capture_height;
      info.buffer1_size = info.rgbs_line_size * info.capture_height;
    }

    FreeMediaType (ref mt);
  }


  // don't buffer the samples as they pass through
  info.pGrabber->lpVtbl->SetBufferSamples ((LPVOID)info.pGrabber, FALSE);

  // don't stop stream after grabbing one sample
  info.pGrabber->lpVtbl->SetOneShot ((LPVOID)info.pGrabber, FALSE);

  // set the callback, so we can grab the one sample;
  // 0 indicates SampleCB(), 1 indicates BufferCB().
  info.pGrabber->lpVtbl->SetCallback ((LPVOID)info.pGrabber, (LPVOID)&info, 0);



  // run the graph

  info.pGraph->lpVtbl->QueryInterface ((LPVOID)info.pGraph, IID_IMediaControl, out p);
  if (p == null)
  {
#if debug_error
  trace ("QueryInterface(IID_IMediaControl) failed\n");
#endif
    free_all_for_direct_show (ref info);
    return -1;
  }


  {
    IMediaControl* pControl = (IMediaControl*) p;

    hr = pControl->lpVtbl->Run ((LPVOID)pControl);
    if (hr < 0)
    {
#if debug_error
  trace ("Run() failed\n");
#endif
      SafeRelease((LPVOID**)&pControl);
      free_all_for_direct_show (ref info);
      return -1;
    }

    SafeRelease((LPVOID**)&pControl);
  }

  return 0;
}

//---------------------------------------------------------------------------------------------------------

void stop_capture_for_direct_show (ref WEBCAM_CAPTURE info)
{
  LPVOID         p;
  int            hr;
  IMediaControl* pControl;

  info.pGraph->lpVtbl->QueryInterface ((LPVOID)info.pGraph, IID_IMediaControl, out p);
  if (p == null)
  {
#if debug_error
  trace ("QueryInterface(IID_IMediaControl) failed\n");
#endif
    return;
  }
  pControl = (IMediaControl*) p;

  hr = pControl->lpVtbl->Stop ((LPVOID)pControl);
  if (hr < 0)
  {
#if debug_error
  trace ("Stop() failed\n");
#endif
    SafeRelease((LPVOID**)&pControl);
    return;
  }

  SafeRelease((LPVOID**)&pControl);
}

//---------------------------------------------------------------------------------------------------------

void display_dialog (ref WEBCAM_CAPTURE info, HWND hWnd, int x, int y)
{
  HRESULT                hr;
  LPVOID                 p;
  ISpecifyPropertyPages* pSpec;
  CAUUID                 cauuid;

  if (info.pCap == null)
    return;

  hr = info.pCap->lpVtbl->QueryInterface ((LPVOID)info.pCap, IID_ISpecifyPropertyPages, out p);
  if (hr != S_OK)
    return;
  pSpec = (ISpecifyPropertyPages *) p;


  hr = pSpec->lpVtbl->GetPages ((LPVOID)pSpec, &cauuid);

  if (hr == S_OK)
  {
    hr = OleCreatePropertyFrame (hWnd, (UINT)x, (UINT)y,
                                 lpszCaption => null,
                                 cObjects=>1, (LPUNKNOWN *)&info.pCap, cPages=>cauuid.cElems, (LPCLSID *)cauuid.pElems,
                                 0, 0, null);
  }

  CoTaskMemFree ((LPVOID)cauuid.pElems);
  SafeRelease((LPVOID**)&pSpec);
}

//---------------------------------------------------------------------------------------------------------
// Media Foundation functions
//---------------------------------------------------------------------------------------------------------

int create_video_device_source (WEBCAM_NAME webcam_name, IMFMediaSource** ppSource)
{
  IMFAttributes *pAttributes = null;
  IMFActivate **ppDevices = null;
  HRESULT hr;
  UINT32 count = 0;

  *ppSource = null;

  // Create an attribute store to specify the enumeration parameters.
  hr = mf_plat->MFCreateAttributes(&pAttributes, 1);
  if (SUCCEEDED(hr))
  {
    // Source type: video capture devices
    hr = pAttributes->lpVtbl->SetGUID((LPVOID*)pAttributes,
                                      &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                                      &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (SUCCEEDED(hr))
    {
      // Enumerate devices.
      hr = mf->MFEnumDeviceSources (pAttributes, &ppDevices, &count);
      if (SUCCEEDED(hr))
      {
        DWORD i;
        for (i = 0; i < count; i++)
        {
          UINT32 pcchLength;
          LPWSTR ppwszValue;

          hr = ppDevices[i]->lpVtbl->GetAllocatedString ((LPVOID*)ppDevices[i], &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &ppwszValue, &pcchLength);
          if (SUCCEEDED(hr))
          {
            WEBCAM_NAME str;

            wsprintf (out str, L"%S", ppwszValue[0 : str'length]);
            if (wstrcmp (str, webcam_name) == 0 && *ppSource == null)
            {
              // Create the media source object.
              hr = ppDevices[i]->lpVtbl->ActivateObject((LPVOID*)(ppDevices[i]), IID_IMFMediaSource, (byte**)ppSource);
              if (SUCCEEDED(hr))
              {
                (*ppSource)->lpVtbl->AddRef((LPVOID)(*ppSource));
              }
            }

            CoTaskMemFree ((LPVOID)ppwszValue);
          }

          SafeRelease((LPVOID**)&ppDevices[i]);
        }

        CoTaskMemFree((LPVOID)ppDevices);
      }
    }

    SafeRelease((LPVOID**)&pAttributes);
  }

  if (*ppSource == null)
  {
    hr = -1;
  }
  else if (FAILED (hr) && (*ppSource) != null)
  {
    SafeRelease((LPVOID**)ppSource);
  }

  return hr;
}

//---------------------------------------------------------------------------------------------------------

HRESULT ConfigureDecoder (ref WEBCAM_CAPTURE wc, DWORD dwStreamIndex, out bool more_streams)
{
  IMFMediaType *pNativeType = null;
  HRESULT      hr;

  wc.stride = 0;

  more_streams = false;

  // Find the native format of the stream.
  hr = wc.pSourceReader->lpVtbl->GetNativeMediaType ((LPVOID*)wc.pSourceReader, dwStreamIndex, 0, &pNativeType);
  if (SUCCEEDED(hr))
  {
    GUID majorType;

    more_streams = true;

    // Find the major type.
    hr = pNativeType->lpVtbl->GetGUID ((LPVOID*)pNativeType, &MF_MT_MAJOR_TYPE, &majorType);
    if (SUCCEEDED(hr))
    {
      if (memcmp (majorType, MFMediaType_Video) == 0)
      {
        IMFMediaType *pType = null;

        // Define the output type.
        hr = mf_plat->MFCreateMediaType (&pType);
        if (SUCCEEDED(hr))
        {
          hr = pType->lpVtbl->SetGUID ((LPVOID*)pType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
          if (SUCCEEDED(hr))
          {
            // Select uncompressed subtype
            hr = pType->lpVtbl->SetGUID ((LPVOID*)pType, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
            if (SUCCEEDED(hr))
            {
              // set dimensions
              (void) MFSetAttributeSize (pType, &MF_MT_FRAME_SIZE, unWidth => (uint)wc.width_out, unHeight => (uint)wc.height_out);

              // set aspect ratio
              if (false)
              {
                if (SUCCEEDED(MFGetAttributeSize (pNativeType, guidKey => &MF_MT_FRAME_SIZE, &wc.capture_width, &wc.capture_height)))
                  (void) MFSetAttributeSize (pType, &MF_MT_PIXEL_ASPECT_RATIO, unWidth => (uint)wc.height_out * wc.capture_width, unHeight => (uint)wc.width_out * wc.capture_height);
              }

              // Set the uncompressed format.
              (void) wc.pSourceReader->lpVtbl->SetCurrentMediaType ((LPVOID*)wc.pSourceReader, dwStreamIndex, null, pType);

              // select stream
              hr = wc.pSourceReader->lpVtbl->SetStreamSelection ((LPVOID*)wc.pSourceReader, dwStreamIndex, fSelected => TRUE);
              if (SUCCEEDED(hr))
              {
                IMFMediaType *pType2 = null;
                hr = wc.pSourceReader->lpVtbl->GetCurrentMediaType ((LPVOID*)wc.pSourceReader, dwStreamIndex, &pType2);
                if (SUCCEEDED(hr))
                {
                  hr = MFGetAttributeSize (pType2, guidKey => &MF_MT_FRAME_SIZE, &wc.capture_width, &wc.capture_height);
                  if (SUCCEEDED(hr))
                  {
                    wc.buffer1_size =  4 * wc.capture_width * wc.capture_height;
                    if (FAILED (pType->lpVtbl->GetUINT32 ((LPVOID *)pType, &MF_MT_DEFAULT_STRIDE, (UINT32*)&wc.stride)))
                      wc.stride = 4 * (int)wc.capture_width;
                  }

                  SafeRelease((LPVOID**)&pType2);
                }
              }
            }
          }

          SafeRelease((LPVOID**)&pType);
        }
      }
    }
  }

  SafeRelease((LPVOID**)&pNativeType);
  return hr;
}

//---------------------------------------------------------------------------------------------------------

// from XRGB to RGBX  (switch red and blue bytes)

void move_row (byte *src, byte *dst, uint nb_bytes)
{
  byte* s = src;
  byte* d = dst;
  byte* s9 = src + nb_bytes - 4;

  while (s <= s9)
  {
    d[0] = s[2];
    d[1] = s[1];
    d[2] = s[0];
    d[3] = 255;

    s += 4;
    d += 4;
  }
}

//---------------------------------------------------------------------------------------------------------

int extract_image (IMFSample *pSample, IMAGE_INFO image, int stride)
{
  HRESULT        hr;
  DWORD          dwBufferCount, buffer_nr;
  ref byte[]     img = image.pixel^;
  IMFMediaBuffer *pBuffer;

  hr = pSample->lpVtbl->GetBufferCount ((LPVOID*)pSample, &dwBufferCount);
  if (FAILED(hr))
  {
#if debug_error
  trace ("error: GetBufferCount() returned %d\n", hr);
#endif
    return hr;
  }

  if (dwBufferCount == 0)
  {
#if debug_error
  trace ("error: dwBufferCount == 0\n");
#endif
    return -1;
  }

  buffer_nr = dwBufferCount - 1;   // treat only last (latest) buffer

  hr = pSample->lpVtbl->GetBufferByIndex ((LPVOID*)pSample, buffer_nr, &pBuffer);
  if (FAILED(hr))
  {
#if debug_error
  trace ("error: GetBufferByIndex() returned %d\n", hr);
#endif
    return hr;
  }

  {
    IMF2DBuffer2 *imf2dbuffer2;
    hr = pBuffer->lpVtbl->QueryInterface ((LPVOID)pBuffer, IID_IMF2DBuffer2, out *(LPVOID*)&imf2dbuffer2);
    if (SUCCEEDED(hr))
    {
      BYTE  *pbScanline0;
      LONG  lPitch;
      BYTE  *pbBufferStart;
      DWORD cbBufferLength;

      hr = imf2dbuffer2->lpVtbl->Lock2DSize ((LPVOID*)imf2dbuffer2, MF2DBuffer_LockFlags_Read,
                                             &pbScanline0, &lPitch, &pbBufferStart, &cbBufferLength);
      if (SUCCEEDED(hr))
      {
        uint row = 4 * image.width;
        BYTE* pbScanline9 = pbScanline0 + lPitch * ((int)image.height - 1) + ((lPitch > 0) ? (int)row : -(int)row);

        if (abs(lPitch) <= (int)row + 1024 &&
            pbScanline0 >= pbBufferStart && pbScanline0 < pbBufferStart + cbBufferLength &&
            pbScanline9 >= pbBufferStart && pbScanline9 <= pbBufferStart + cbBufferLength)
        {
          int y;
          byte* src, dst;

          src = pbScanline0;
          dst = &img;

          for (y=(int)image.height-1; y>=0; y--)
          {
            move_row (src, dst, row);
            dst += row;
            src += lPitch;
          }
        }

        assert SUCCEEDED (imf2dbuffer2->lpVtbl->Unlock2D ((LPVOID*)imf2dbuffer2));
      }

      SafeRelease((LPVOID**)&imf2dbuffer2);
    }
    else
    {
      IMF2DBuffer *imf2dbuffer;
      hr = pBuffer->lpVtbl->QueryInterface ((LPVOID)pBuffer, IID_IMF2DBuffer, out *(LPVOID*)&imf2dbuffer);
      if (SUCCEEDED(hr))
      {
        BYTE  *pbScanline0;
        LONG  lPitch;

        hr = imf2dbuffer->lpVtbl->Lock2D ((LPVOID*)imf2dbuffer, &pbScanline0, &lPitch);
        if (SUCCEEDED(hr))
        {
          uint row = 4 * image.width;
          int y;
          byte* src, dst;

          src = pbScanline0;
          dst = &img;

          for (y=(int)image.height-1; y>=0; y--)
          {
            move_row (src, dst, row);
            dst += row;
            src += lPitch;
          }

          assert SUCCEEDED (imf2dbuffer->lpVtbl->Unlock2D ((LPVOID*)imf2dbuffer));
        }

        SafeRelease((LPVOID**)&imf2dbuffer);
      }
      else
      {
        BYTE  *bBuffer;
        DWORD cbMaxLength;
        DWORD cbCurrentLength;

        hr = pBuffer->lpVtbl->Lock ((LPVOID*)pBuffer, &bBuffer, &cbMaxLength, &cbCurrentLength);
        if (SUCCEEDED(hr))
        {
          uint row = 4 * image.width;
          LONG lPitch = stride;

          if (lPitch < (int)row || lPitch > (int)row + 1024 || (uint)lPitch * image.height > cbCurrentLength)
            lPitch = (int)row;   // lPitch seems out of range, fallback to default

          if ((uint)lPitch * image.height <= cbCurrentLength)
          {
            int y;
            byte* src, dst;

            src = bBuffer;
            dst = &img;

            for (y=(int)image.height-1; y>=0; y--)
            {
              move_row (src, dst, row);
              dst += row;
              src += lPitch;
            }
          }

          assert SUCCEEDED (pBuffer->lpVtbl->Unlock ((LPVOID*)pBuffer));
        }
      }
    }
  }

  SafeRelease((LPVOID**)&pBuffer);

  return hr;
}

//---------------------------------------------------------------------------------------------------------

// functions for COM object IMFSourceReaderCallback

[callback]
HRESULT MyQueryInterface_for_SourceReaderCallback (LPVOID This,  REFIID riid,  out LPVOID ppvObject)
{
  if (memcmp (riid, IID_IMFSourceReaderCallback) == 0 || memcmp (riid, IID_IUnknown) == 0)
  {
    ppvObject = This;
    return NOERROR;
  }
  ppvObject = null;
  return E_NOINTERFACE;
}

//---------------------------------------------------------------------------------------------------------

[callback]
HRESULT OnReadSample (LPVOID * This, HRESULT hrStatus, DWORD dwStreamIndex,
                      DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *pSample)
{
  ref WEBCAM_CAPTURE wc = *(WEBCAM_CAPTURE*)This;
  HRESULT hr;

  _unused llTimestamp;

  if (FAILED(hrStatus))
  {
#if debug_error
  trace ("error: OnReadSample() returned %d\n", hrStatus);
#endif
    return hrStatus;  // ignored
  }

  if ((dwStreamFlags & (MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED | MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)) != 0)
  {
    bool more_streams;

    hr = ConfigureDecoder (ref wc, dwStreamIndex, out more_streams);     // reconfigure the decoder.
    if (FAILED(hr))
    {
#if debug_error
  trace ("error: MEDIATYPECHANGED -> ConfigureDecoder() returned %d\n", hr);
#endif
      return hr;  // ignored
    }

    _unused more_streams;
  }

  if (pSample != null)
  {
    int buffer_nr, next_buffer_nr;

    buffer_nr = wc.current_buffer_nr;
    next_buffer_nr = buffer_nr + 1;
    if (next_buffer_nr == MAX_BUFFERS)
      next_buffer_nr = 0;
    if (wc.nb_locks[next_buffer_nr] == 0)  // next slot is available : write it there
    {
      ref byte[]^ p = wc.buffer1[next_buffer_nr];
      ref byte[]^ q = wc.buffer2[next_buffer_nr];

       // make sure if is allocated with correct size
      if (p == null || p^'size != wc.buffer1_size)
      {
        free p;
        p = new byte[wc.buffer1_size];
      }

      // make sure if is allocated with correct size
      if (q == null || q^'size != (uint)wc.buffer2_size)
      {
        free q;
        q = new byte[wc.buffer2_size];
      }

      {
        IMAGE_INFO image = {pixel => p, width => wc.capture_width, height => wc.capture_height};
        if (extract_image (pSample, image, wc.stride) < 0)
        {
#if debug_error
  trace ("error: extract_image() failed\n");
#endif
          return -1;
        }
      }

      if (wc.capture_width == (uint)wc.width_out && wc.capture_height == (uint)wc.height_out)
      {
        byte[]^ temp = q;
        q = p;
        p = temp;
      }
      else
      {
        IMAGE_INFO img1  = {pixel => p, width => wc.capture_width, height => wc.capture_height};
        IMAGE_INFO img2  = {pixel => q, width => (uint)wc.width_out, height => (uint)wc.height_out};
        CLIP_INFO  clip1 = {offset_x => 0, offset_y => 0, size_x => img1.width, size_y => img1.height};
        CLIP_INFO  clip2 = {offset_x => 0, offset_y => 0, size_x => img2.width, size_y => img2.height};

        assert stretch_image (img1, clip1, img2, clip2, use_linear_colors => false, high_quality => false) == 0;
      }


      enter_shared_object (ref g_thread_safe);

      if (wc.state[buffer_nr] == WRITTEN_AND_READ)   // current buffer is or was in use
      {
        wc.current_buffer_nr = next_buffer_nr;      // switch to next buffer
      }
      else     // current buffer was not used, reuse it
      {
        // exchange wc.buffer2[buffer_nr] and wc.buffer2[next_buffer_nr]
        byte[]^ temp               = wc.buffer2[buffer_nr];
        wc.buffer2[buffer_nr]      = wc.buffer2[next_buffer_nr];
        wc.buffer2[next_buffer_nr] = temp;
      }

      wc.state[wc.current_buffer_nr] = WRITTEN;
      wc.current_image_nr = (wc.current_image_nr + 1) & 0x7FFFFFFF;   // make sure it's not -1 anymore

      wc.rearm_callback = true;

      leave_shared_object (ref g_thread_safe);
    }
  }
  else
  {
    // we received no samples, rearm caller
    hr = wc.pSourceReader->lpVtbl->ReadSample((LPVOID*)wc.pSourceReader,
                                              dwStreamIndex, // Stream index.
                                              0,                // Flags.
                                              null, null, null, null);
#if debug || debug_error
    trace ("info: ReadSample(1) returned %d\n", hr);
#endif
  }

  return 0;  // ignored;
}

//---------------------------------------------------------------------------------------------------------

[callback]
HRESULT OnFlush (LPVOID * This, DWORD dwStreamIndex)
{
  _unused This;
  _unused dwStreamIndex;
  return 0;
}

[callback]
HRESULT OnEvent (LPVOID * This, DWORD dwStreamIndex, IMFMediaEvent *pEvent)
{
  _unused This;
  _unused dwStreamIndex;
  _unused pEvent;
  return 0;
}

//---------------------------------------------------------------------------------------------------------

package Vtbl2
  IMFSourceReaderCallbackVtbl  MySourceReaderCallbackVtbl;    // vector table for callback COM object
end Vtbl2;

void create_SourceReaderCallback (ref WEBCAM_CAPTURE wc)
{
  // fill global virtual table (in case it's not already done)
  MySourceReaderCallbackVtbl = {QueryInterface => MyQueryInterface_for_SourceReaderCallback,
                                AddRef         => MyAddRef,
                                Release        => MyRelease,
                                OnReadSample   => OnReadSample,
                                OnFlush        => OnFlush,
                                OnEvent        => OnEvent};

  wc.lpVtbl = (LPVOID)&MySourceReaderCallbackVtbl;
}

//---------------------------------------------------------------------------------------------------------

void free_all_for_media_foundation (ref WEBCAM_CAPTURE wc)
{
  SafeRelease((LPVOID**)&wc.pSourceReader);
}

//---------------------------------------------------------------------------------------------------------

int start_capture_for_media_foundation (ref WEBCAM_CAPTURE wc)
{
  HRESULT        hr;
  IMFMediaSource *pSource;
  IMFAttributes  *pAttributes;

  if (create_video_device_source (wc.source_name, &pSource) < 0)
  {
#if debug_error
  trace ("error: create_video_device_source() failed\n");
#endif
    return -1;
  }

  create_SourceReaderCallback (ref wc);

  hr = mf_plat->MFCreateAttributes (&pAttributes, 2);
  if (FAILED(hr))
  {
#if debug_error
  trace ("error: MFCreateAttributes() failed\n");
#endif
    SafeRelease((LPVOID**)&pSource);
    return -1;
  }

  hr = pAttributes->lpVtbl->SetUnknown((LPVOID*)pAttributes, &MF_SOURCE_READER_ASYNC_CALLBACK, (IUnknown *)&wc);
  if (FAILED(hr))
  {
#if debug_error
  trace ("error: SetUnknown() failed\n");
#endif
    SafeRelease((LPVOID**)&pAttributes);
    SafeRelease((LPVOID**)&pSource);
    return -1;
  }

  hr = pAttributes->lpVtbl->SetUINT32((LPVOID*)pAttributes, &MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, 1);
  if (FAILED(hr))
  {
#if debug_error
  trace ("error: SetUINT32() failed\n");
#endif
    SafeRelease((LPVOID**)&pAttributes);
    SafeRelease((LPVOID**)&pSource);
    return -1;
  }

  hr = mf_read_write_calls->MFCreateSourceReaderFromMediaSource (pSource, pAttributes, &wc.pSourceReader);
  if (FAILED(hr))
  {
#if debug_error
  trace ("error: MFCreateSourceReaderFromMediaSource() failed\n");
#endif
    SafeRelease((LPVOID**)&pAttributes);
    SafeRelease((LPVOID**)&pSource);
    return -1;
  }

  SafeRelease((LPVOID**)&pAttributes);
  SafeRelease((LPVOID**)&pSource);

  // deselect ALL streams
  hr = wc.pSourceReader->lpVtbl->SetStreamSelection ((LPVOID*)wc.pSourceReader, MF_SOURCE_READER_ALL_STREAMS, fSelected => FALSE);
  if (FAILED(hr))
  {
#if debug_error
  trace ("error: SetStreamSelection() failed\n");
#endif
    free_all_for_media_foundation (ref wc);
    return -1;
  }


  {
    uint dwStreamIndex;
    bool more_streams;

    for (dwStreamIndex=0;  ; dwStreamIndex++)
    {
      hr = ConfigureDecoder (ref wc, dwStreamIndex, out more_streams);
      if (SUCCEEDED(hr))
        break;

      if (!more_streams)
      {
#if debug_error
  trace ("error: ConfigureDecoder(1) failed\n");
#endif
        free_all_for_media_foundation (ref wc);
        return -1;
      }
    }

    wc.dwStreamIndex = dwStreamIndex;
  }

  // initiate asyn calls
  hr = wc.pSourceReader->lpVtbl->ReadSample((LPVOID*)wc.pSourceReader,
                                            wc.dwStreamIndex,             // Stream index.
                                            0,                            // Flags.
                                            null, null, null, null);
  if (FAILED(hr))
  {
#if debug_error
    trace ("error: ReadSample(2) failed\n");
#endif
    free_all_for_media_foundation (ref wc);
    return -1;
  }

  return 0;
}

//---------------------------------------------------------------------------------------------------------

void capture_thread (WEBCAM_CAPTURE* pinfo)
{
  ref WEBCAM_CAPTURE wc = *pinfo;
  bool ok1 = false;
  bool ok2 = false;

  wc.threadId = get_current_thread_id ();

  if (wc.use_media_foundation)
  {
    if (CoInitializeEx (null, COINIT_MULTITHREADED) == 0)
    {
      ok1 = true;
      init_media_foundation();  // in case g_mf_status is 0, initialize media foundation and set it to +1 or -1
      ok2 = (g_mf_status > 0);
    }
  }
  else
  {
    int rc = CoInitializeEx (null, COINIT_APARTMENTTHREADED);
    ok1 = (rc == S_OK || rc == S_FALSE);
    ok2 = ok1;
  }

  if (ok2)
  {
    int rc;

    if (wc.use_media_foundation)
      rc = start_capture_for_media_foundation (ref wc);
    else
      rc = start_capture_for_direct_show (ref wc);

    if (rc == 0)
    {
      bool stop = false;

      wc.thread_status = +1;

      while (!stop)    // message loop
      {
        MSG msg;
        GetMessageA (&msg, 0, 0, 0);
        if (msg.message == 40000)
        {
          if (msg.wParam == 50000)
            stop = true;
          if (msg.wParam == 50001)
          {
            // initiate loading of next frame
            HRESULT hr = wc.pSourceReader->lpVtbl->ReadSample((LPVOID*)wc.pSourceReader,
                                                              wc.dwStreamIndex, // Stream index.
                                                              0,                // Flags.
                                                              null, null, null, null);
#if debug || debug_error
            trace ("info: ReadSample(3) returned %d\n", hr);
#else
            _unused hr;
#endif
          }
        }
      }

      if (wc.use_media_foundation)
      {
        free_all_for_media_foundation (ref wc);
      }
      else
      {
        stop_capture_for_direct_show (ref wc);
        free_all_for_direct_show (ref wc);
      }
    }
  }

  if (ok1)
    CoUninitialize();

  wc.thread_status = -1;
}

//---------------------------------------------------------------------------------------------------------

public void stop_webcam (ref WEBCAM_CAPTURE info)
{
  while (info.thread_status > 0)       // running
  {
    PostThreadMessageA ((DWORD)info.threadId, 40000, 50000, 0);   // post a message to make the graph stop
    sleep 0.2;
  }
}

//---------------------------------------------------------------------------------------------------------

public int start_webcam (ref WEBCAM_CAPTURE info,
                             WEBCAM_NAME    source_name,
                             int            width  = 320,
                             int            height = 240)
{
  stop_webcam (ref info);

  {
    int ret;

    enter_shared_object (ref g_thread_safe);

    info.width_out    = width;
    info.height_out   = height;
    info.source_name  = source_name;
    info.buffer2_size = (uint)(4 * width * height);
    info.current_image_nr = -1;
    info.rearm_callback = false;

    info.use_media_foundation = memcmp (source_name[0:3], L"DS:") != 0;

    info.thread_status = 0;    // 0 = waiting to initialize, +1 = success, -1 = failure

    ret = run capture_thread (&info);
    if (ret == 0)
    {
      while (info.thread_status == 0)   // waiting for success or failure
        sleep 0.020;
      if (info.thread_status < 0)
        ret = -1;
    }

    leave_shared_object (ref g_thread_safe);

    return ret;
  }
}

//---------------------------------------------------------------------------------------------------------

// to be called if webcam is no longer used

public void deallocate_webcam (ref WEBCAM_CAPTURE info)
{
  int i;

  stop_webcam (ref info);

  for (i=0; i<MAX_BUFFERS; i++)
  {
    free info.buffer1[i];
    free info.buffer2[i];
  }

  clear info;
}

//---------------------------------------------------------------------------------------------------------

public void display_webcam_dialog (ref WEBCAM_CAPTURE info, long ParentWhwnd, int x, int y)
{
enter_shared_object (ref g_thread_safe);
  display_dialog (ref info, (HWND)ParentWhwnd, x, y);
leave_shared_object (ref g_thread_safe);
}

//---------------------------------------------------------------------------------------------------------

// returns the most recent webcam image.
// returns -1 if there is no image available (especially just after starting webcam),
//   or returns an image number >= 0 that can be the same as for the previous call if no new image is available.
// The function manages 'image', don't ever allocate or deallocate it.
// You can use 'image' after the call until calling unlock_webcam_image().
// several threads can call this function.

public
int lock_and_get_webcam_image (ref WEBCAM_CAPTURE info,
                               out IMAGE_INFO     image,
                               out int            lock_number)
{
  int image_nr;

  enter_shared_object (ref g_thread_safe);

  if (info.thread_status > 0 && info.current_image_nr >= 0)   // there is an available image
  {
    int i = info.current_buffer_nr;

    image = {pixel  => info.buffer2[i],
             width  => (uint)info.width_out,
             height => (uint)info.height_out};

    info.state[i] = WRITTEN_AND_READ;
    info.nb_locks[i]++;

    lock_number = i;

    image_nr = info.current_image_nr;

    if (info.rearm_callback)
    {
      info.rearm_callback = false;
      PostThreadMessageA ((DWORD)info.threadId, 40000, 50001, 0);   // post a message to load next image
    }
  }
  else
  {
    clear image;
    lock_number = -1;
    image_nr = -1;
  }

  leave_shared_object (ref g_thread_safe);

  return image_nr;
}

//---------------------------------------------------------------------------------------------------------

// releases intern buffer
// You must always call unlock_webcam_image() after each lock_and_get_lock_webcam_image() call !

public void unlock_webcam_image (ref WEBCAM_CAPTURE info, int lock_number)
{
enter_shared_object (ref g_thread_safe);

  if (lock_number >= 0 && lock_number < info.nb_locks'length)
  {
    ref int locks = info.nb_locks[lock_number];
    assert locks > 0;
    locks--;
  }

leave_shared_object (ref g_thread_safe);
}

//---------------------------------------------------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------------------------------------------------

#endif

//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------

#if ANDROID
//------------------------------------------------------------------------------------

use image;

//------------------------------------------------------------------------------------

struct WEBCAM_CAPTURE   // opaque type
{
}

//------------------------------------------------------------------------------------

// get list of all webcams;
// returns number of webcams available.

public int get_webcam_list (out WEBCAM_NAME[] list)
{
  clear list;
  return 0;
}

//------------------------------------------------------------------------------------

// returns 0 if OK, -1 if error.
public 
int start_webcam (ref WEBCAM_CAPTURE info,
                      WEBCAM_NAME    source_name,
                      int            width  = 320,
                      int            height = 240)
{
  _unused info, source_name, width, height;
  return -1;
}

//------------------------------------------------------------------------------------

public 
void stop_webcam (ref WEBCAM_CAPTURE info)
{
  _unused info;
}

//------------------------------------------------------------------------------------

// to be called if webcam is no longer used

public void deallocate_webcam (ref WEBCAM_CAPTURE info)
{
  _unused info;
}

//------------------------------------------------------------------------------------

// display dialog box with capture settings.
// (works only for old DirectShow cams)

public void display_webcam_dialog (ref WEBCAM_CAPTURE info, long ParentWhwnd, int x, int y)
{
  _unused info, ParentWhwnd, x, y;
}

//------------------------------------------------------------------------------------

// returns the most recent webcam image.
// returns -1 if there is no image available (especially just after starting webcam),
//   or returns an image number >= 0 that can be the same as for the previous call if no new image is available.
// The function manages 'image', don't ever allocate or deallocate it.
// You can use 'image' after the call until calling unlock_webcam_image().
// several threads can call this function.

public 
int lock_and_get_webcam_image (ref WEBCAM_CAPTURE info,
                               out IMAGE_INFO     image,
                               out int            lock_number)
{
  _unused info;
  clear image, lock_number;
  return -1;
}

//------------------------------------------------------------------------------------

// releases intern buffer
// You must always call unlock_webcam_image() after each lock_and_get_lock_webcam_image() call !

public void unlock_webcam_image (ref WEBCAM_CAPTURE info, int lock_number)
{
  _unused info, lock_number;
}

//------------------------------------------------------------------------------------

#endif  // ANDROID

