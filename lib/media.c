
// media.c

#if WINDOWS

use arithm, directxdef, draw3d, image, thread, memory, strings, win/windows, win/mf, win/directx11;

/*
This layer contains 2 implementations for the playing of multimedia streams or files :
a) windows 7 media session (builds a pipeline to play media on gpu, fetch texture on cpu, create directx texture on gpu)
b) windows 8 media engine (same but entirely on gpu)
*/

//============================================================================================
#begin unsafe
//============================================================================================

SHARED_OBJECT  g_so;
int            g_mf_status;   // MF initialization : 0=not done, 1=success, -1=failure
MF_CALLS*      mf;
MF_PLAT_CALLS* mf_plat;

enum MediaState
{
  InitializationPending,
  InitializationFailed,
  Ready,          // No session, no media engine.
  OpenPending,    // Session is opening a file.
  Started,        // Session is playing a file.
  Paused,         // Session is paused.
  Stopped,        // Session is stopped (ready to play).
  Closing,        // Application has closed the session, but is waiting for MESessionClosed.
  Finished,
};

struct Media
{
  // !  MUST BE FIRST FIELD OF STRUCTURE !
  LPVOID                     *lpVtbl;    // type IMFAsyncCallbackVtbl* or IMFMediaEngineNotifyVtbl*

  UINT                       threadId;
  MediaState                 m_state;
  bool                       use_media_engine_api;

  // used for Media Session
  HWND                       m_hwndVideo;        // Video window (always null).
  HANDLE                     m_hCloseEvent;      // Event to wait on while closing.
  IMFMediaSession            *m_pSession;
  IMFMediaSource             *m_pSource;
  IMFVideoDisplayControl     *m_pVideoDisplay;
  IMFSimpleAudioVolume       *m_pSimpleAudioVolume;
  IMFPresentationClock       *m_PresentationClock;
  IMAGE_INFO                 image, corrected_image;

  // used for Media Engine
  IMFMediaEngine             *m_media_engine;  // media thread controls this, can set it to null or recreate
  IUnknown                   *pD11Texture;

  // common fields
  bool                       has_video;
  bool                       has_audio;
  uint                       previous_image_width;
  uint                       previous_image_height;
  RECT                       rect;
  BOOL                       mute;
  float                      volume;
  long                       duration;
}

//============================================================================================

// functions for COM object IMFAsyncCallback that we implement here (win 7)

[callback]
HRESULT MyQueryInterface (LPVOID This,  REFIID riid,  out LPVOID ppvObject)
{
  if (memcmp (riid, IID_IMFAsyncCallback) == 0 || memcmp (riid, IID_IUnknown) == 0)
  {
    ppvObject = This;
    return NOERROR;
  }
  ppvObject = null;
  return E_NOINTERFACE;
}

//---------------------------------------------------------------------------------------------------------

// used for both win7 and 8

[callback]
ULONG MyAddRef (LPVOID this)
{
  _unused this;
  return 2;
}

//---------------------------------------------------------------------------------------------------------

// used for both win7 and 8

[callback]
ULONG MyRelease (LPVOID this)
{
  _unused this;
  return 1;
}

//---------------------------------------------------------------------------------------------------------

// win 7

[callback]
HRESULT MyGetParameters (LPVOID * This, DWORD *pdwFlags, DWORD *pdwQueue)
{
  _unused This;
  _unused pdwFlags;
  _unused pdwQueue;

  return E_NOTIMPL;  // Implementation of this method is optional.
}

//---------------------------------------------------------------------------------------------------------

// function for COM object MediaEngineNotify that we implement here (win 8)

[callback]
HRESULT MyQueryInterface2 (LPVOID This,  REFIID riid,  out LPVOID ppvObject)
{
  if (memcmp (riid, IID_IMFMediaEngineNotify) == 0 || memcmp (riid, IID_IUnknown) == 0)
  {
    ppvObject = This;
    return NOERROR;
  }
  ppvObject = null;
  return E_NOINTERFACE;
}

//---------------------------------------------------------------------------------------------------------

void compute_media_duration (ref Media media)
{
  media.duration = 0L;

  if (media.use_media_engine_api)   // Windows 8
  {
    double secs = media.m_media_engine->lpVtbl->GetDuration ((LPVOID*)media.m_media_engine);
    if (secs >= 0.0 && secs <= 1_000_000_000.0)
      media.duration = (long)(secs * 10_000_000.0);
  }
  else   // Windows 7
  {
    IMFPresentationDescriptor *pPD = null;
    HRESULT                   hr;

    if (media.m_pSource != null)
    {
      hr = media.m_pSource->lpVtbl->CreatePresentationDescriptor ((LPVOID*)media.m_pSource, &pPD);
      if (SUCCEEDED(hr))
      {
        hr = pPD->lpVtbl->GetUINT64 ((LPVOID*)pPD, &MF_PD_DURATION, &media.duration);
        pPD->lpVtbl->Release((LPVOID)pPD);
      }
    }
  }
}

//============================================================================================

void set_media_position (ref Media media, long seek)  // pos in 1L / 10_000_000L seconds
{
  if (media.use_media_engine_api)   // Windows 8
  {
    if (media.m_media_engine != null)
      media.m_media_engine->lpVtbl->SetCurrentTime ((LPVOID*)media.m_media_engine, (double)seek * (1.0 / 10_000_000.0));
  }
  else   // Windows 7
  {
    PROPVARIANT varStart;
    HRESULT     hr;

    clear varStart;

    varStart.vt = 20; // VT_I8;
    varStart.Val = seek'byte;

    hr = media.m_pSession->lpVtbl->Start ((LPVOID*)media.m_pSession, &GUID_NULL, &varStart);
    if (SUCCEEDED(hr))
    {
      // Note: Start is an asynchronous operation. However, we
      // can treat our state as being already started. If Start
      // fails later, we'll get an MESessionStarted event with
      // an error code, and we will update our state then.
      media.m_state = Started;
    }

    PropVariantClear(&varStart);
  }
}

//---------------------------------------------------------------------------------------------------------

void get_media_position (Media media, out long time)
{
  time = 0L;

  if (media.use_media_engine_api)   // Windows 8
  {
    double secs = media.m_media_engine->lpVtbl->GetCurrentTime ((LPVOID*)media.m_media_engine);
    if (secs >= 0.0 && secs <= 1_000_000_000.0)
      time = (long)(secs * 10_000_000.0);
  }
  else   // Windows 7
  {
    if (media.m_PresentationClock != null)
    {
      HRESULT hr;
      long    time0 = 0;
      hr = media.m_PresentationClock->lpVtbl->GetTime ((LPVOID*)media.m_PresentationClock, &time0);
      if (SUCCEEDED(hr))
        time = time0;
    }
  }
}

//============================================================================================

// returns 0.0 if not playing

public float media_total_duration (Media media)
{
  return (float)media.duration * (1.0 / 10_000_000.0);
}

//============================================================================================

void set_volume_and_mute (Media media)
{
  if (media.use_media_engine_api)   // Media Engine (>= Windows 8)
  {
    if (media.m_media_engine != null)
    {
      media.m_media_engine->lpVtbl->SetMuted ((LPVOID*)media.m_media_engine, (BOOL)media.mute);
      media.m_media_engine->lpVtbl->SetVolume ((LPVOID*)media.m_media_engine, (double)media.volume);
    }
  }
  else   // windows 7
  {
    if (media.m_pSimpleAudioVolume != null)
    {
      media.m_pSimpleAudioVolume->lpVtbl->SetMasterVolume ((LPVOID *)media.m_pSimpleAudioVolume, media.volume);
      media.m_pSimpleAudioVolume->lpVtbl->SetMute         ((LPVOID *)media.m_pSimpleAudioVolume, media.mute);
    }
  }
}

//============================================================================================

//  Start playback from the current position (win 7)

void StartPlayback (ref Media media)
{
  PROPVARIANT varStart;
  HRESULT     hr;

  clear varStart;

  hr = media.m_pSession->lpVtbl->Start ((LPVOID*)media.m_pSession, &GUID_NULL, &varStart);
  if (SUCCEEDED(hr))
  {
    // Note: Start is an asynchronous operation. However, we
    // can treat our state as being already started. If Start
    // fails later, we'll get an MESessionStarted event with
    // an error code, and we will update our state then.
    media.m_state = Started;
  }

  PropVariantClear(&varStart);
}

//---------------------------------------------------------------------------------------------------------

void OnTopologyStatus (ref Media media, IMFMediaEvent *pEvent)  // (win 7)
{
  uint    status;
  HRESULT hr;

  hr = pEvent->lpVtbl->GetUINT32 ((LPVOID *)pEvent, &MF_EVENT_TOPOLOGY_STATUS, &status);
  if (SUCCEEDED(hr) && status == MF_TOPOSTATUS_READY)
  {
    SafeRelease((LPVOID**)&media.m_pVideoDisplay);
    SafeRelease((LPVOID**)&media.m_pSimpleAudioVolume);
    SafeRelease((LPVOID**)&media.m_PresentationClock);

    // Get the IMFVideoDisplayControl interface from EVR.
    // fails if the media file does not have a video stream.
    hr = mf->MFGetService ((IUnknown*)media.m_pSession, &MR_VIDEO_RENDER_SERVICE, &IID_IMFVideoDisplayControl, (LPVOID*)&media.m_pVideoDisplay);
    media.has_video = SUCCEEDED(hr);

    // Get the IMFSimpleAudioVolume interface from EVR.
    // fails if the media file does not have an audio stream.
    hr = mf->MFGetService ((IUnknown*)media.m_pSession, &MR_POLICY_VOLUME_SERVICE, &IID_IMFSimpleAudioVolume, (LPVOID*)&media.m_pSimpleAudioVolume);
    media.has_audio = SUCCEEDED(hr);
    if (media.has_audio)
      set_volume_and_mute (media);

    hr = media.m_pSession->lpVtbl->GetClock ((LPVOID *)media.m_pSession, (IMFClock **)&media.m_PresentationClock);

    compute_media_duration (ref media);

    StartPlayback (ref media);
  }
}

//---------------------------------------------------------------------------------------------------------

HRESULT GetEventObject (IMFMediaEvent *pEvent, LPVOID *ppObject)  // (win 7)
{
  PROPVARIANT var;
  HRESULT     hr;

  clear *ppObject;

  hr = pEvent->lpVtbl->GetValue((LPVOID*)pEvent, &var);
  if (SUCCEEDED(hr))
  {
    if (var.vt == 13)   // VT_UNKNOWN
    {
      IUnknown *i;
      i'byte = var.Val[0:i'size];
      hr = i->lpVtbl->QueryInterface ((LPVOID)i, IID_IUnknown, out *ppObject);
    }
    else
    {
      hr = 0xC00D36BD - 0x100000000;  //  MF_E_INVALIDTYPE;
    }

    PropVariantClear(&var);
  }

  return hr;
}

//---------------------------------------------------------------------------------------------------------

// Add a source node to a topology  (win 7)

HRESULT AddSourceNode
   (IMFTopology               *pTopology,   // Topology.
    IMFMediaSource            *pSource,     // Media source.
    IMFPresentationDescriptor *pPD,         // Presentation descriptor.
    IMFStreamDescriptor       *pSD,         // Stream descriptor.
    IMFTopologyNode           **ppNode)     // Receives the node pointer.
{
  IMFTopologyNode *pNode = null;
  HRESULT         hr;

  // Create the node.
  hr = mf->MFCreateTopologyNode (MF_TOPOLOGY_SOURCESTREAM_NODE, &pNode);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pNode);
    return hr;
  }

  // Set the attributes.
  hr = pNode->lpVtbl->SetUnknown ((LPVOID*)pNode, &MF_TOPONODE_SOURCE, (IUnknown *)pSource);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pNode);
    return hr;
  }

  hr = pNode->lpVtbl->SetUnknown ((LPVOID*)pNode, &MF_TOPONODE_PRESENTATION_DESCRIPTOR, (IUnknown *)pPD);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pNode);
    return hr;
  }

  hr = pNode->lpVtbl->SetUnknown ((LPVOID*)pNode, &MF_TOPONODE_STREAM_DESCRIPTOR, (IUnknown *)pSD);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pNode);
    return hr;
  }

  // Add the node to the topology.
  hr = pTopology->lpVtbl->AddNode ((LPVOID*)pTopology, pNode);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pNode);
    return hr;
  }

  // Return the pointer to the caller.
  *ppNode = pNode;
  return 0;
}

//---------------------------------------------------------------------------------------------------------

// Add an output node to a topology (win 7)

HRESULT AddOutputNode
   (IMFTopology     *pTopology,    // Topology.
    IMFActivate     *pActivate,    // Media sink activation object.
    DWORD           dwId,          // Identifier of the stream sink.
    IMFTopologyNode **ppNode)      // Receives the node pointer.
{
  IMFTopologyNode *pNode = null;

  // Create the node.
  HRESULT hr = mf->MFCreateTopologyNode (MF_TOPOLOGY_OUTPUT_NODE, &pNode);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pNode);
    return hr;
  }

  // Set the object pointer.
  hr = pNode->lpVtbl->SetObject ((LPVOID*)pNode, (IUnknown *)pActivate);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pNode);
    return hr;
  }

  // Set the stream sink ID attribute.
  hr = pNode->lpVtbl->SetUINT32 ((LPVOID*)pNode, &MF_TOPONODE_STREAMID, dwId);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pNode);
    return hr;
  }

  hr = pNode->lpVtbl->SetUINT32 ((LPVOID*)pNode, &MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, 0);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pNode);
    return hr;
  }

  // Add the node to the topology.
  hr = pTopology->lpVtbl->AddNode ((LPVOID*)pTopology, pNode);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pNode);
    return hr;
  }

  // Return the pointer to the caller.
  *ppNode = pNode;
  return 0;
}

//---------------------------------------------------------------------------------------------------------

//  Create an activation object for a renderer, based on the stream media type (win 7)

HRESULT CreateMediaSinkActivate(
    IMFStreamDescriptor *pSourceSD,     // Pointer to the stream descriptor.
    HWND                hVideoWindow,   // Handle to the video clipping window.
    IMFActivate         **ppActivate)
{
  IMFMediaTypeHandler *pHandler = null;
  IMFActivate         *pActivate = null;
  HRESULT             hr;
  GUID                guidMajorType;

  // Get the media type handler for the stream.
  hr = pSourceSD->lpVtbl->GetMediaTypeHandler((LPVOID*)pSourceSD, &pHandler);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pHandler);
    SafeRelease((LPVOID**)&pActivate);
    return hr;
  }

  // Get the major media type.
  hr = pHandler->lpVtbl->GetMajorType((LPVOID*)pHandler, &guidMajorType);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pHandler);
    SafeRelease((LPVOID**)&pActivate);
    return hr;
  }

  // Create an IMFActivate object for the renderer, based on the media type.
  if (memcmp (MFMediaType_Audio, guidMajorType) == 0)
  {
    // Create the audio renderer.
    hr = mf->MFCreateAudioRendererActivate (&pActivate);
  }
  else if (memcmp (MFMediaType_Video, guidMajorType) == 0)
  {
    // Create the video renderer.
    hr = mf->MFCreateVideoRendererActivate (hVideoWindow, &pActivate);
  }
  else
  {
    // Unknown stream type.
    hr = E_FAIL;
    // Optionally, you could deselect this stream instead of failing.
  }
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pHandler);
    SafeRelease((LPVOID**)&pActivate);
    return hr;
  }

  SafeRelease((LPVOID**)&pHandler);

  // Return IMFActivate pointer to caller.
  *ppActivate = pActivate;
  return 0;
}

//---------------------------------------------------------------------------------------------------------

//  Add a topology branch for one stream (win 7)
//
//  For each stream, this function does the following:
//
//    1. Creates a source node associated with the stream.
//    2. Creates an output node for the renderer.
//    3. Connects the two nodes.
//
//  The media session will add any decoders that are needed.

HRESULT AddBranchToPartialTopology
   (IMFTopology               *pTopology,   // Topology.
    IMFMediaSource            *pSource,     // Media source.
    IMFPresentationDescriptor *pPD,         // Presentation descriptor.
    DWORD                     iStream,      // Stream index.
    HWND                      hVideoWnd)    // Window for video playback.
{
  IMFStreamDescriptor *pSD           = null;
  IMFActivate         *pSinkActivate = null;
  IMFTopologyNode     *pSourceNode   = null;
  IMFTopologyNode     *pOutputNode   = null;
  BOOL                fSelected      = FALSE;
  HRESULT             hr;

  hr = pPD->lpVtbl->GetStreamDescriptorByIndex ((LPVOID*)pPD, iStream, &fSelected, &pSD);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pSD);
    SafeRelease((LPVOID**)&pSinkActivate);
    SafeRelease((LPVOID**)&pSourceNode);
    SafeRelease((LPVOID**)&pOutputNode);
    return hr;
  }

  if (fSelected != 0)
  {
    // Create the media sink activation object.
    hr = CreateMediaSinkActivate (pSD, hVideoWnd, &pSinkActivate);
    if (FAILED(hr))
    {
      SafeRelease((LPVOID**)&pSD);
      SafeRelease((LPVOID**)&pSinkActivate);
      SafeRelease((LPVOID**)&pSourceNode);
      SafeRelease((LPVOID**)&pOutputNode);
      return hr;
    }

    // Add a source node for this stream.
    hr = AddSourceNode (pTopology, pSource, pPD, pSD, &pSourceNode);
    if (FAILED(hr))
    {
      SafeRelease((LPVOID**)&pSD);
      SafeRelease((LPVOID**)&pSinkActivate);
      SafeRelease((LPVOID**)&pSourceNode);
      SafeRelease((LPVOID**)&pOutputNode);
      return hr;
    }

    // Create the output node for the renderer.
    hr = AddOutputNode (pTopology, pSinkActivate, 0, &pOutputNode);
    if (FAILED(hr))
    {
      SafeRelease((LPVOID**)&pSD);
      SafeRelease((LPVOID**)&pSinkActivate);
      SafeRelease((LPVOID**)&pSourceNode);
      SafeRelease((LPVOID**)&pOutputNode);
      return hr;
    }

    // Connect the source node to the output node.
    hr = pSourceNode->lpVtbl->ConnectOutput ((LPVOID*)pSourceNode, 0, pOutputNode, 0);
    if (FAILED(hr))
    {
      SafeRelease((LPVOID**)&pSD);
      SafeRelease((LPVOID**)&pSinkActivate);
      SafeRelease((LPVOID**)&pSourceNode);
      SafeRelease((LPVOID**)&pOutputNode);
      return hr;
    }
  }
  // else: If not selected, don't add the branch.

  SafeRelease((LPVOID**)&pSD);
  SafeRelease((LPVOID**)&pSinkActivate);
  SafeRelease((LPVOID**)&pSourceNode);
  SafeRelease((LPVOID**)&pOutputNode);
  return 0;
}

//---------------------------------------------------------------------------------------------------------

//  Create a playback topology from a media source (win 7)

HRESULT CreatePlaybackTopology
   (IMFMediaSource            *pSource,          // Media source.
    IMFPresentationDescriptor *pPD,              // Presentation descriptor.
    HWND                      hVideoWnd,         // Video window.
    IMFTopology               **ppTopology)      // Receives a pointer to the topology.
{
  IMFTopology *pTopology = null;
  DWORD       cSourceStreams = 0;
  HRESULT     hr;
  DWORD       i;

  // Create a new topology.
  hr = mf->MFCreateTopology (&pTopology);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pTopology);
    return hr;
  }

  // Get the number of streams in the media source.
  hr = pPD->lpVtbl->GetStreamDescriptorCount ((LPVOID*)pPD, &cSourceStreams);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pTopology);
    return hr;
  }

  // For each stream, create the topology nodes and add them to the topology.
  for (i=0; i<cSourceStreams; i++)
  {
    hr = AddBranchToPartialTopology (pTopology, pSource, pPD, i, hVideoWnd);
/*
    if (FAILED(hr))
    {
      SafeRelease ((LPVOID**)&pTopology);
      return hr;
    }
*/
  }

  // Return the IMFTopology pointer to the caller.
  *ppTopology = pTopology;
  return 0;
}

//---------------------------------------------------------------------------------------------------------

//  Handler for MENewPresentation event (win 7)
//
//  This event is sent if the media source has a new presentation, which
//  requires a new topology.

void OnNewPresentation (ref Media media, IMFMediaEvent *pEvent)
{
  IMFPresentationDescriptor *pPD       = null;
  IMFTopology               *pTopology = null;
  HRESULT                   hr;

  // Get the presentation descriptor from the event.
  hr = GetEventObject (pEvent, (LPVOID*)&pPD);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pPD);
    return;
  }

  // Create a partial topology.
  hr = CreatePlaybackTopology (media.m_pSource, pPD, media.m_hwndVideo, &pTopology);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pTopology);
    SafeRelease((LPVOID**)&pPD);
    return;
  }

  // Set the topology on the media session.
  hr = media.m_pSession->lpVtbl->SetTopology ((LPVOID*)media.m_pSession, 0, pTopology);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pTopology);
    SafeRelease((LPVOID**)&pPD);
    return;
  }

  SafeRelease((LPVOID**)&pTopology);
  SafeRelease((LPVOID**)&pPD);

  media.m_state = OpenPending;
}

//---------------------------------------------------------------------------------------------------------

[callback]
HRESULT MyInvoke (LPVOID * This, IMFAsyncResult *pAsyncResult)  // (win 7)
{
  ref Media      media = *(Media*)This;
  MediaEventType meType = 0; // MEUnknown
  IMFMediaEvent  *pEvent = null;
  HRESULT        hr, hrStatus;

  // Get the event from the event queue.
  hr = media.m_pSession->lpVtbl->EndGetEvent ((LPVOID*)media.m_pSession, pAsyncResult, &pEvent);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pEvent);
    return 0;
  }

  // Get the event type.
  hr = pEvent->lpVtbl->GetType((LPVOID*)pEvent, &meType);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pEvent);
    return 0;
  }

  if (meType == MESessionClosed)    // The session was closed.
  {
    SafeRelease((LPVOID**)&pEvent);

    // The application is waiting on the m_hCloseEvent event handle.
    SetEvent (media.m_hCloseEvent);
    return 0;
  }

  // Get the event status.
  hr = pEvent->lpVtbl->GetStatus((LPVOID*)pEvent, &hrStatus);

  // Check if the async operation succeeded.
  if (SUCCEEDED(hr) && SUCCEEDED(hrStatus) && media.m_state != Closing)
  {
    switch (meType)
    {
      case 111: // MESessionTopologyStatus
        OnTopologyStatus (ref media, pEvent);
        break;

      case 211: // MEEndOfPresentation
        media.m_state = Stopped;
        break;

      case 113: // MENewPresentation:
        OnNewPresentation (ref media, pEvent);
        break;

      default:
        break;
    }
  }

  SafeRelease((LPVOID**)&pEvent);

  // pull the next event from the queue.
  hr = media.m_pSession->lpVtbl->BeginGetEvent((LPVOID *)media.m_pSession, (IMFAsyncCallback*)&media, null);
  if (FAILED(hr))
    fatal_abort ("BeginGetEvent()", hr);

  return 0;
}

//---------------------------------------------------------------------------------------------------------

[callback]
HRESULT MyEventNotify (LPVOID * This, DWORD event, DWORD_PTR param1, DWORD param2)  // (win 8)
{
  ref Media media = *(Media*)This;

#if 0
  int iparam1 = *(int*)&param1;
  printf ("event %u %d %u\n", event, iparam1, param2);
#else
  _unused param1;
  _unused param2;
#endif

  // do not use media.m_media_engine in this function, it might be null already !

  if (event == MF_MEDIA_ENGINE_EVENT_CANPLAY)
    media.m_state = Started;

/*
event 1001 MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS
event 22   MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE
event 8    MF_MEDIA_ENGINE_EVENT_PLAY
event 12   MF_MEDIA_ENGINE_EVENT_WAITING

event 5 4  MF_MEDIA_ENGINE_EVENT_ERROR  (illegal url)

event 1    MF_MEDIA_ENGINE_EVENT_LOADSTART
event 21   MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE
event 10   MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA
event 11   MF_MEDIA_ENGINE_EVENT_LOADEDDATA
event 14   MF_MEDIA_ENGINE_EVENT_CANPLAY
event 15   MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH
event 18   MF_MEDIA_ENGINE_EVENT_TIMEUPDATE
event 1000 MF_MEDIA_ENGINE_EVENT_FORMATCHANGE     (only video)
event 1009 MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY  (only video)

event 18   MF_MEDIA_ENGINE_EVENT_TIMEUPDATE
event 18
event 13   MF_MEDIA_ENGINE_EVENT_PLAYING
event 18
event 18
..
event 19   MF_MEDIA_ENGINE_EVENT_ENDED
*/

  if (event == MF_MEDIA_ENGINE_EVENT_ABORT ||
      event == MF_MEDIA_ENGINE_EVENT_ERROR ||
      event == MF_MEDIA_ENGINE_EVENT_RESOURCELOST ||
      event == MF_MEDIA_ENGINE_EVENT_ENDED)
  {
    media.m_state = Stopped;
  }

  return S_OK;
}

//---------------------------------------------------------------------------------------------------------

//  Create a media source from a URL  (win 7)

HRESULT CreateMediaSource (PCWSTR sURL, IMFMediaSource **ppMediaSource)
{
  MF_OBJECT_TYPE     ObjectType      = MF_OBJECT_INVALID;
  IMFSourceResolver* pSourceResolver = null;
  IUnknown*          pSource         = null;
  HRESULT            hr;

  // Create the source resolver.
  hr = mf->MFCreateSourceResolver (&pSourceResolver);
  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pSourceResolver);
    SafeRelease((LPVOID**)&pSource);
    return hr;
  }

  // Use the source resolver to create the media source.

  // Note: For simplicity this sample uses the synchronous method to create
  // the media source. However, creating a media source can take a noticeable
  // amount of time, especially for a network source. For a more responsive
  // UI, use the asynchronous BeginCreateObjectFromURL method.

  hr = pSourceResolver->lpVtbl->CreateObjectFromURL
             ((LPVOID *)pSourceResolver,
               sURL,                       // URL of the source.
               MF_RESOLUTION_MEDIASOURCE,  // Create a source object. (or MF_RESOLUTION_BYTESTREAM)
               null,                       // Optional property store.
               &ObjectType,        // Receives the created object type.
               &pSource);          // Receives a pointer to the media source.

  if (FAILED(hr))
  {
    SafeRelease((LPVOID**)&pSourceResolver);
    SafeRelease((LPVOID**)&pSource);
    return hr;
  }

  // Get the IMFMediaSource interface from the media source.
  hr = pSource->lpVtbl->QueryInterface ((LPVOID)pSource, IID_IMFMediaSource, out *(LPVOID *)ppMediaSource);

  SafeRelease((LPVOID**)&pSourceResolver);
  SafeRelease((LPVOID**)&pSource);
  return hr;
}

//---------------------------------------------------------------------------------------------------------

// Open a URL for playback.
// from state Ready to states Ready (if error), OpenPending, Started or Stopped.

int open_media (ref Media media, wstring urlZ)
{
  assert media.m_state == Ready;

  if (media.use_media_engine_api)   // Windows 8
  {
    HRESULT                    hr;
    IMFMediaEngineClassFactory *mediaEngineClassFactory = null;
    IMFAttributes              *attributeStore = null;

    if (mf_plat->MFCreateDXGIDeviceManager == null)  // windows 8 not supported
      return -1;

    hr = CoCreateInstance (CLSID_MFMediaEngineClassFactory, null, CLSCTX_INPROC_SERVER, IID_IMFMediaEngineClassFactory, out *(LPVOID*)&mediaEngineClassFactory);
    if (hr != 0)
      return -1;

    hr = mf_plat->MFCreateAttributes (&attributeStore, 4);
    if (FAILED(hr))
    {
      SafeRelease((LPVOID**)&mediaEngineClassFactory);
      return -1;
    }

    hr = attributeStore->lpVtbl->SetUnknown ((LPVOID*)attributeStore, &MF_MEDIA_ENGINE_CALLBACK, (IUnknown *)&media);
    if (FAILED(hr))
    {
      SafeRelease((LPVOID**)&attributeStore);
      SafeRelease((LPVOID**)&mediaEngineClassFactory);
      return -1;
    }

    hr = attributeStore->lpVtbl->SetUINT32 ((LPVOID*)attributeStore, &MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (FAILED(hr))
    {
      SafeRelease((LPVOID**)&attributeStore);
      SafeRelease((LPVOID**)&mediaEngineClassFactory);
      return -1;
    }


    {
      IMFDXGIDeviceManager *ppDeviceManager = null;
      UINT                 resetToken;

      hr = mf_plat->MFCreateDXGIDeviceManager (&resetToken, &ppDeviceManager);
      if (FAILED(hr))
      {
        SafeRelease((LPVOID**)&attributeStore);
        SafeRelease((LPVOID**)&mediaEngineClassFactory);
        return -1;
      }

      hr = ppDeviceManager->lpVtbl->ResetDevice ((LPVOID*)ppDeviceManager,
                                                 (IUnknown *)DX.m_device,
                                                 resetToken);
      if (FAILED(hr))
      {
        SafeRelease((LPVOID**)&ppDeviceManager);
        SafeRelease((LPVOID**)&attributeStore);
        SafeRelease((LPVOID**)&mediaEngineClassFactory);
        return -1;
      }

      hr = attributeStore->lpVtbl->SetUnknown ((LPVOID*)attributeStore, &MF_MEDIA_ENGINE_DXGI_MANAGER, (IUnknown *)ppDeviceManager);
      if (FAILED(hr))
      {
        SafeRelease((LPVOID**)&ppDeviceManager);
        SafeRelease((LPVOID**)&attributeStore);
        SafeRelease((LPVOID**)&mediaEngineClassFactory);
        return -1;
      }

      // Create the engine
      hr = mediaEngineClassFactory->lpVtbl->CreateInstance ((LPVOID*)mediaEngineClassFactory, 0, attributeStore, &media.m_media_engine);
      if (FAILED(hr))
      {
        SafeRelease((LPVOID**)&ppDeviceManager);
        SafeRelease((LPVOID**)&attributeStore);
        SafeRelease((LPVOID**)&mediaEngineClassFactory);
        return -1;
      }

      SafeRelease((LPVOID**)&ppDeviceManager);
    }


    SafeRelease((LPVOID**)&attributeStore);
    SafeRelease((LPVOID**)&mediaEngineClassFactory);

    media.m_state = OpenPending;

    // Setup the engine
    hr = media.m_media_engine->lpVtbl->SetSource ((LPVOID*)media.m_media_engine, &urlZ);
    if (FAILED(hr))
    {
      // shutdown
      hr = media.m_media_engine->lpVtbl->Shutdown ((LPVOID*)media.m_media_engine);
      SafeRelease((LPVOID**)&media.m_media_engine);
      return -1;
    }

    hr = media.m_media_engine->lpVtbl->SetPreload ((LPVOID*)media.m_media_engine, MF_MEDIA_ENGINE_PRELOAD_AUTOMATIC);

    set_volume_and_mute (media);

    sleep 1;   // let preload happen ...

    hr = media.m_media_engine->lpVtbl->Play ((LPVOID*)media.m_media_engine);
    if (FAILED(hr))
    {
      // shutdown
      hr = media.m_media_engine->lpVtbl->Shutdown ((LPVOID*)media.m_media_engine);
      SafeRelease((LPVOID**)&media.m_media_engine);
      return -1;
    }
  }
  else   // Windows 7
  {
    IMFTopology              * pTopology = null;
    IMFPresentationDescriptor* pSourcePD = null;
    HRESULT                    hr;


    // 1. Create a new media session.

    // Create the media session.
    hr = mf->MFCreateMediaSession (null, &media.m_pSession);
    if (FAILED(hr))
    {
      return -1;
    }

    // Start pulling events from the media session
    hr = media.m_pSession->lpVtbl->BeginGetEvent((LPVOID *)media.m_pSession, (IMFAsyncCallback*)&media, null);
    if (FAILED(hr))
    {
      return -1;
    }

    // 2. Create the media source.
    // 3. Create the topology.
    // 4. Queue the topology [asynchronous]
    // 5. Start playback [asynchronous - does not happen in this method.]

    // Create the media source.
    hr = CreateMediaSource (&urlZ, &media.m_pSource);
    if (FAILED(hr))
    {
      return -1;
    }

    // Create the presentation descriptor for the media source.
    hr = media.m_pSource->lpVtbl->CreatePresentationDescriptor ((LPVOID *)media.m_pSource, &pSourcePD);
    if (FAILED(hr))
    {
      SafeRelease((LPVOID**)&pSourcePD);
      SafeRelease((LPVOID**)&pTopology);
      return -1;
    }

    // Create a partial topology.
    hr = CreatePlaybackTopology (media.m_pSource, pSourcePD, media.m_hwndVideo, &pTopology);
    if (FAILED(hr))
    {
      SafeRelease((LPVOID**)&pSourcePD);
      SafeRelease((LPVOID**)&pTopology);
      return -1;
    }

    // Set the topology on the media session.
    hr = media.m_pSession->lpVtbl->SetTopology ((LPVOID *)media.m_pSession, 0, pTopology);
    if (FAILED(hr))
    {
      SafeRelease((LPVOID**)&pSourcePD);
      SafeRelease((LPVOID**)&pTopology);
      return -1;
    }

    // If SetTopology succeeds, the media session will queue an
    // MESessionTopologySet event.

    SafeRelease((LPVOID**)&pSourcePD);
    SafeRelease((LPVOID**)&pTopology);

    media.m_state = OpenPending;
  }

  // wait til video is started before processing further messages
  {
    int i;
    for (i=0; ; i++)
    {
      if (media.m_state != OpenPending)
        break;
      
      if (i == 8*60)  // 8 seconds
      {
        media.m_state = Started;
        break;    
      }
      
      sleep 0.033;
    }
  }

  if (media.use_media_engine_api)   // Windows 8
  {
    compute_media_duration (ref media);
    media.has_video = media.m_media_engine->lpVtbl->HasVideo ((LPVOID*)media.m_media_engine) != 0;
    media.has_audio = media.m_media_engine->lpVtbl->HasAudio ((LPVOID*)media.m_media_engine) != 0;
  }

  return 0;
}

//============================================================================================

void close_media (ref Media media)
{
  if (media.use_media_engine_api)   // Media Engine (>= Windows 8)
  {
    if (media.m_media_engine != null)
    {
      HRESULT hr = media.m_media_engine->lpVtbl->Shutdown ((LPVOID*)media.m_media_engine);
      assert SUCCEEDED(hr);

      SafeRelease((LPVOID**)&media.m_media_engine);
    }
  }
  else   // windows 7
  {
    //  The IMFMediaSession::Close method is asynchronous, but the
    //  CloseSession method waits on the MESessionClosed event.
    //
    //  MESessionClosed is guaranteed to be the last event that the media session fires.

    HRESULT hr = S_OK;

    SafeRelease((LPVOID**)&media.m_pVideoDisplay);
    SafeRelease((LPVOID**)&media.m_pSimpleAudioVolume);
    SafeRelease((LPVOID**)&media.m_PresentationClock);

    // First close the media session.
    if (media.m_pSession != null)
    {
      DWORD dwWaitResult = 0;

      media.m_state = Closing;

      hr = media.m_pSession->lpVtbl->Close ((LPVOID *)media.m_pSession);
      if (SUCCEEDED(hr))
      {
        // Wait 30 secs for the close operation to complete
        dwWaitResult = WaitForSingleObject (media.m_hCloseEvent, 30000);
        if (dwWaitResult == WAIT_TIMEOUT)
          fatal_abort ("Close/WaitForSingleObject()", 0);
        
        // Now there will be no more events from this session.
      }
    }

    // Complete shutdown operations.
    if (SUCCEEDED(hr))
    {
      // Shut down the media source. (Synchronous operation, no events.)
      if (media.m_pSource != null)
      {
        hr = media.m_pSource->lpVtbl->Shutdown((LPVOID *)media.m_pSource);
        assert SUCCEEDED(hr);
      }

      // Shut down the media session. (Synchronous operation, no events.)
      if (media.m_pSession != null)
      {
        hr = media.m_pSession->lpVtbl->Shutdown((LPVOID *)media.m_pSession);
        assert SUCCEEDED(hr);
      }
    }

    SafeRelease((LPVOID**)&media.m_pSource);
    SafeRelease((LPVOID**)&media.m_pSession);

    free_image (ref media.image);
    free_image (ref media.corrected_image);
  }

  media.has_video = false;
  media.has_audio = false;
  media.duration = 0L;
  media.m_state = Ready;
}

//============================================================================================

// in case g_mf_status is 0, initialize media foundation and set it to +1 or -1

void init_media_fondation()
{
  if (g_mf_status != 0)  // already done
    return;

  enter_shared_object (ref g_so);

  if (g_mf_status == 0)   // media foundation not initialized yet
  {
    mf      = init_mf_calls ();
    mf_plat = init_mf_plat_calls ();

    if (mf != null && mf_plat != null)
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

//============================================================================================

// Media Engine (>= Windows 8)

void init_media_engine (ref Media media)
{
  IMFMediaEngineNotifyVtbl *lpVtbl = (IMFMediaEngineNotifyVtbl*) malloc (IMFMediaEngineNotifyVtbl'size);

  clear *lpVtbl;
  lpVtbl->QueryInterface = MyQueryInterface2;
  lpVtbl->AddRef         = MyAddRef;
  lpVtbl->Release        = MyRelease;
  lpVtbl->EventNotify    = MyEventNotify;
  media.lpVtbl = (LPVOID*)lpVtbl;
}

//============================================================================================

// Media Session (Windows 7)

void init_media_session (ref Media media)
{
  {
    HANDLE ev = CreateEventA (null, FALSE, FALSE, null);
    assert ev != 0;
    media.m_hCloseEvent = ev;
  }

  {
    IMFAsyncCallbackVtbl *lpVtbl = (IMFAsyncCallbackVtbl*) malloc (IMFAsyncCallbackVtbl'size);
    clear *lpVtbl;
    lpVtbl->QueryInterface = MyQueryInterface;
    lpVtbl->AddRef         = MyAddRef;
    lpVtbl->Release        = MyRelease;
    lpVtbl->GetParameters  = MyGetParameters;
    lpVtbl->Invoke         = MyInvoke;
    media.lpVtbl = (LPVOID*)lpVtbl;
  }
}

//============================================================================================

// media thread runs on multithread appartment

void MediaThread (Media *p)
{
  ref Media media = *(Media*)p;
  HRESULT   hr;
  bool      quit;

  media.threadId = get_current_thread_id ();

  hr = CoInitializeEx (null, COINIT_MULTITHREADED);
  if (hr != 0)
  {
    media.m_state = InitializationFailed;
    return;
  }

  // in case g_mf_status is 0, initialize media foundation and set it to +1 or -1
  init_media_fondation();

  if (g_mf_status < 0)
  {
    CoUninitialize ();
    media.m_state = InitializationFailed;
    return;
  }


  // either create a Media Engine (>= Windows 8), or a Media Session (Windows 7)

  if (media.use_media_engine_api)   // Media Engine (>= Windows 8)
  {
    if (mf_plat->MFCreateDXGIDeviceManager == null)  // not windows 8
    {
      CoUninitialize ();
      media.m_state = InitializationFailed;
      return;
    }

    init_media_engine (ref media);
  }
  else   // Media Session (Windows 7)
  {
    init_media_session (ref media);
  }

  // make sure message queue exists
  {
    MSG msg;
    PeekMessageA (&msg, 0, WM_USER, WM_USER, PM_NOREMOVE);
  }

  media.volume = 1.0;
  media.m_state = Ready;   // ready to accept URL's

  quit = false;
  while (!quit)
  {
    MSG msg;

    GetMessageA (&msg, 0, 0, 0);

    switch (msg.message)
    {
      case 100:
        {
          wchar* s;
          s'byte = msg.lParam'byte;

          if (media.m_state != Ready)
            close_media (ref media);

          // from state Ready to states Ready (if error), OpenPending or Started.
          if (msg.wParam > 1)  // not empty string
            open_media (ref media, s[0 : (int)msg.wParam]);

          freem ((byte*)s);
        }
        break;

      case 200:
       close_media (ref media);
       break;

      case 550:
        media.volume'byte = msg.wParam'byte[0:4];
        media.mute'byte = msg.lParam'byte[0:4];
        set_volume_and_mute (media);
        break;

      case 600:  // 0=stop 1=pause 2=start
        if (media.use_media_engine_api)   // Media Engine (>= Windows 8)
        {
          if (media.m_media_engine != null)
          {
            if (msg.wParam == 0 && (media.m_state == Started || media.m_state == Paused))
            {
              if (media.m_state == Started)
                hr = media.m_media_engine->lpVtbl->Pause ((LPVOID*)media.m_media_engine);
              hr = media.m_media_engine->lpVtbl->SetCurrentTime ((LPVOID*)media.m_media_engine, seekTime => 0.0);
              if (SUCCEEDED(hr))
                media.m_state = Stopped;
            }

            if (msg.wParam == 1 && media.m_state == Started)
            {
              hr = media.m_media_engine->lpVtbl->Pause ((LPVOID*)media.m_media_engine);
              if (SUCCEEDED(hr))
                media.m_state = Paused;
            }

            if (msg.wParam == 2 && (media.m_state == Paused || media.m_state == Stopped))
            {
              hr = media.m_media_engine->lpVtbl->Play ((LPVOID*)media.m_media_engine);
              if (SUCCEEDED(hr))
                media.m_state = Started;
            }
          }
        }
        else   // windows 7
        {
          if (media.m_pSession != null && media.m_pSource != null)
          {
            if (msg.wParam == 0 && (media.m_state == Started || media.m_state == Paused))
            {
              hr = media.m_pSession->lpVtbl->Stop ((LPVOID *)media.m_pSession);
              if (SUCCEEDED(hr))
                media.m_state = Stopped;
            }

            if (msg.wParam == 1 && media.m_state == Started)
            {
              hr = media.m_pSession->lpVtbl->Pause ((LPVOID *)media.m_pSession);
              if (SUCCEEDED(hr))
                media.m_state = Paused;
            }

            if (msg.wParam == 2 && (media.m_state == Paused || media.m_state == Stopped))
            {
              StartPlayback (ref media);
            }
          }
        }
        break;

      case 700:
        if ((media.use_media_engine_api && media.m_media_engine != null) ||                        // Windows 8
            (!media.use_media_engine_api && media.m_pSession != null && media.m_pSource != null))  // windows 7
        {
          uint  mode;
          float seconds;
          long  pos;

          if (media.duration == 0L)
            break;

          mode'byte = msg.wParam'byte[0:4];
          seconds'byte = msg.lParam'byte[0:4];

          pos = (long)(seconds * 10_000_000.0);

          switch (mode)
          {
            case 0:  // absolute
              break;

            case 1:  // relative
              {
                long current_time;
                get_media_position (media, out current_time);
                pos += current_time;
              }
              break;

            case 2:  // from end
              pos += media.duration;
              break;

            default:
              pos = 0;
              break;
          }

          if (pos > media.duration)
            pos = media.duration;

          if (pos < 0)
            pos = 0;

          set_media_position (ref media, seek => pos);
        }
        break;

      case 900:
        quit = true;
        break;

      default:
        break;
    }
  }

  close_media (ref media);
  if (media.m_hCloseEvent != 0)
    CloseHandle (media.m_hCloseEvent);
  CoUninitialize ();
  freem ((byte*)media.lpVtbl);
  media.m_state = Finished;
}

//============================================================================================

public void media_play (Media media, wstring s)
{
  int    len = wstrlen(s);
  int    size = 2 * (len + 1);
  wchar* z = (wchar*) malloc ((uint)size);
  LPARAM ptr;

  z[0 : len] = s[0 : len];
  z[len] = Lnul;

  ptr'byte = z'byte;

  *&media.duration = 0L;

  assert PostThreadMessageA (media.threadId, 100, (uint)len, ptr) != 0;
}

//============================================================================================

public void media_sound (Media media, float volume, bool mute)
{
  uint a;
  int  b;
  a'byte = volume'byte;
  b = (int)mute;
  assert PostThreadMessageA (media.threadId, 550, a, b) != 0;
}

//============================================================================================

public void media_action (Media media, int action)  // action:  0=stop 1=pause 2=start
{
  assert PostThreadMessageA (media.threadId, 600, (uint)action, 0) != 0;
}

//============================================================================================

public void media_seek (Media media, int mode, float seconds)
{
  int f;
  f'byte = seconds'byte;
  assert PostThreadMessageA (media.threadId, 700, (uint)mode, f) != 0;
}

//============================================================================================

public bool media_has_video_channel (Media media)
{
  return media.has_video;
}

//============================================================================================

public bool media_has_audio_channel (Media media)
{
  return media.has_audio;
}

//============================================================================================

public bool media_is_playing (Media media)
{
  return media.m_state == Started;
}

//============================================================================================

public bool media_is_paused (Media media)
{
  return media.m_state == Paused;
}

//============================================================================================

void stretch_to_4_3_screen (IMAGE_INFO source, ref IMAGE_INFO result)
{
  uint new_width, new_height, border;

  if (source.width * 3 > source.height * 4)  // wide picture : add top and bottom borders
  {
    new_width  = source.width;
    new_height = source.width * 3 / 4;
    border = (new_height - source.height) >> 1;
  }
  else   // add left and right borders
  {
    new_width  = source.height * 4 / 3;
    new_height = source.height;
    border = (new_width - source.width) >> 1;
  }

  if (result.pixel == null || result.width != new_width || result.height != new_height)
  {
    free_image (ref result);
  }

  if (result.pixel == null)
  {
    result.width = new_width;
    result.height = new_height;
    result.pixel = new byte[4 * new_width * new_height];
  }


  if (source.width * 3 > source.height * 4)  // wide picture : add top and bottom borders
  {
    result.pixel^[4 * new_width * border : source.pixel^'size] = source.pixel^;
  }
  else   // add left and right borders
  {
    uint y, src, dst;
    src = 0;
    dst = 4 * border;
    for (y=0; y<new_height; y++)
    {
      result.pixel^[dst : 4 * source.width] = source.pixel^[src : 4 * source.width];
      src += 4 * source.width;
      dst += 4 * new_width;
    }
  }
}

//===========================================================================================================

void copy_bitmap (byte[] dib, uint width, uint height, int bytes_per_pixel, uint stride, bool bottom_up, ref IMAGE_INFO result)
{
  uint x, y, source_line, source_offset;
  int  result_line, result_stride, result_offset;
  ref byte[] res = result.pixel^;

  if (bottom_up)
  {
    result_line = (int)(4 * width * (height - 1));  // last line
    result_stride = -(int)( 4 * width);
  }
  else
  {
    result_line = 0;   // first line
    result_stride = (int)(4 * width);
  }

  if (bytes_per_pixel == 2)
  {
    source_line = 0;
    for (y=0; y<height; y++)
    {
      source_offset = source_line;
      result_offset = result_line;

      for (x=0; x<width; x++)
      {
        // The value for blue is in the least significant 5 bits,
        // followed by 5 bits each for green and red.
        // The most significant bit is not used.
        uint2 val;
        uint  value;
        val'byte = dib[source_offset:2];
        value = ((val & 0b111110000000000) >> 7)  // red
              + ((val & 0b000001111100000) << 6)  // green
              + ((val & 0b000000000011111) << 19) // blue
              + 0xFF000000;
        res[result_offset:4] = value'byte;

        source_offset += (uint)bytes_per_pixel;
        result_offset += 4;
      }

      source_line += stride;
      result_line += result_stride;
    }
  }
  else if (bytes_per_pixel == 3)
  {
//    uint val = 0;
//    uint value;

    source_line = 0;
    for (y=0; y<height; y++)
    {
      source_offset = source_line;
      result_offset = result_line;

      for (x=0; x<width; x++)
      {
        // Each 3-byte triplet in the bitmap array represents the relative intensities
        // of blue, green, and red, respectively, for a pixel.
/*
        val'byte[0:3] = dib[source_offset:3];
        value = ((val & 0xFF0000) >> 16)  // red
              + ((val & 0x00FF00)      )  // green
              + ((val & 0x0000FF) << 16)  // blue
              + 0xFF000000;
        res[result_offset:4] = value'byte;
*/

        ((byte*)&res)[result_offset]   = ((byte*)&dib)[source_offset+2];
        ((byte*)&res)[result_offset+1] = ((byte*)&dib)[source_offset+1];
        ((byte*)&res)[result_offset+2] = ((byte*)&dib)[source_offset+0];
        ((byte*)&res)[result_offset+3] = 0xFF;

        source_offset += 3;
        result_offset += 4;
      }

      source_line += stride;
      result_line += result_stride;
    }
  }
  else if (bytes_per_pixel == 4)
  {
    source_line = 0;
    for (y=0; y<height; y++)
    {
      source_offset = source_line;
      result_offset = result_line;

      for (x=0; x<width; x++)
      {
        // Each DWORD in the bitmap array represents the relative intensities of blue, green, and red,
        // respectively, for a pixel. The high byte in each DWORD is not used.
/*
        uint val, value;
        val'byte = dib[source_offset:4];
        value = ((val & 0xFF0000) >> 16)  // red
              + ((val & 0x00FF00)      )  // green
              + ((val & 0x0000FF) << 16)  // blue
              + 0xFF000000;
        res[result_offset:4] = value'byte;
*/
        ((byte*)&res)[result_offset]   = ((byte*)&dib)[source_offset+2];
        ((byte*)&res)[result_offset+1] = ((byte*)&dib)[source_offset+1];
        ((byte*)&res)[result_offset+2] = ((byte*)&dib)[source_offset+0];
        ((byte*)&res)[result_offset+3] = 0xFF;

        source_offset += (uint)bytes_per_pixel;
        result_offset += 4;
      }

      source_line += stride;
      result_line += result_stride;
    }
  }
}

//============================================================================================

// in case hvideo is not zero, this function returns an image having the hvideo window size, otherwise original media size.

void media_grab_picture (    Media      media,
                         ref IMAGE_INFO image,    // can be preallocated or not
                         out int        seconds)  // current position
{
  if (media.m_pVideoDisplay != null)
  {
    HRESULT          hr;
    BITMAPINFOHEADER Bih;
    BYTE             *pDib;
    DWORD            pcbDib;
    long             long_time;
    uint             width, height;

    clear Bih;
    Bih.biSize = Bih'size;

    hr = media.m_pVideoDisplay->lpVtbl->GetCurrentImage ((LPVOID *)media.m_pVideoDisplay, &Bih, &pDib, &pcbDib, (LONGLONG*)&long_time);
    if (SUCCEEDED(hr))
    {
      seconds = (int)((float)long_time * (1.0 / 10_000_000.0));

      width  = (uint)Bih.biWidth;
      height = (uint) abs (Bih.biHeight);

      if (image.width != width || image.height != height || image.pixel == null)
      {
        free_image (ref image);
        image.width = width;
        image.height = height;
        if (width > 0 && height > 0)
          image.pixel = new byte[4 * image.width * image.height];
      }

      if (image.pixel != null)
      {
        uint stride = (((((uint)Bih.biWidth * Bih.biBitCount) + 31) & (uint'max - 31)) >> 3);
        copy_bitmap (pDib[0 : pcbDib], width, height, (int)Bih.biBitCount >> 3, stride, Bih.biHeight > 0, ref image);
      }

      CoTaskMemFree (pDib);
    }
    else  // we are seeking : don't change the image
    {
      seconds = 0;
    }
  }
  else
  {
    free_image (ref image);
    seconds = 0;
  }
}

//============================================================================================

TEXTURE_ID create_video_texture (uint width, uint height, out IUnknown* pD11Texture)
{
  HRESULT                         hResult;
  D3D11_TEXTURE2D_DESC            textureDesc;
  ID3D11Texture2D*                m_texture;
  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  ID3D11ShaderResourceView*       m_textureView;

  // Setup the description of the texture.
  clear textureDesc;
  textureDesc.Height = height;
  textureDesc.Width = width;
  textureDesc.MipLevels = 0;
  textureDesc.ArraySize = 1;
  textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  textureDesc.SampleDesc.Count = 1;
//textureDesc.SampleDesc.Quality = 0;

  textureDesc.Usage = D3D11_USAGE_DEFAULT;      // inaccessible to the CPU (the only one that works with mipmapping)
  textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

  hResult = DX.m_device->lpVtbl->CreateTexture2D (DX.m_device, &textureDesc, null, &m_texture);
  if (hResult < 0)
    fatal_abort ("CreateTexture2D()", hResult);

  clear srvDesc;
  srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  // srvDesc.array.Texture2D.MostDetailedMip = 0;
  srvDesc.array.Texture2D.MipLevels = UINT'max;  // -1

  hResult = DX.m_device->lpVtbl->CreateShaderResourceView (DX.m_device, *(ID3D11Resource**)&m_texture, &srvDesc, &m_textureView);
  if (hResult < 0)
    fatal_abort ("CreateShaderResourceView()", hResult);

  pD11Texture'byte = m_texture'byte;

  m_texture->lpVtbl->Release((LPVOID)m_texture);

  return *((TEXTURE_ID *)&m_textureView);
}

//---------------------------------------------------------------------

public void media_update_texture (Media media, ref TEXTURE_ID texture_id)
{
  ref Media m = *(Media*)&media;

  if (media.has_video && (media.m_state == Started || media.m_state == Paused))
  {
    if (m.use_media_engine_api)   // Media Engine (>= Windows 8)
    {
      if (m.m_media_engine != null && m.m_media_engine->lpVtbl->HasVideo ((LPVOID*)m.m_media_engine) != 0)
      {
        long    tick;
        DWORD   cx=0, cy=0;
        HRESULT hr;

        if (m.m_media_engine->lpVtbl->OnVideoStreamTick ((LPVOID*)m.m_media_engine, &tick) == 0 &&
            m.m_media_engine->lpVtbl->GetNativeVideoSize ((LPVOID*)m.m_media_engine, &cx, &cy) == 0)
        {
          // texture exists but has wrong size
          if (texture_id != 0 && (cx != m.previous_image_width || cy != m.previous_image_height))
          {
            free_texture (texture_id);
            texture_id = 0;
          }

          if (texture_id == 0)   // texture does not exist
          {
            m.previous_image_width = cx;
            m.previous_image_height = cy;

            {
              DWORD new_width, new_height, top_border, left_border;

              if (cx * 3 > cy * 4)  // wide picture : add top and bottom borders
              {
                new_width  = cx;
                new_height = cx * 3 / 4;
                top_border = (new_height - cy) >> 1;
                left_border = 0;
              }
              else   // add left and right borders
              {
                new_width  = cy * 4 / 3;
                new_height = cy;
                top_border = 0;
                left_border = (new_width - cx) >> 1;
              }

              texture_id = create_video_texture (new_width, new_height, out m.pD11Texture);
              m.rect = {left => (int)left_border, top => (int)top_border, right => (int)(left_border+cx), bottom => (int)(top_border+cy)};
            }
          }

          {
            const MFARGB border = {0, 0, 0, 0};
            hr = m.m_media_engine->lpVtbl->TransferVideoFrame ((LPVOID*)m.m_media_engine,
                                                               pDstSurf   => m.pD11Texture,
                                                               pSrc       => null,
                                                               pDst       => &m.rect,
                                                               pBorderClr => &border);
            if (hr == 0)
            {
              // generate mipmapping for texture
              ID3D11ShaderResourceView *v;
              v'byte = texture_id'byte;
              DX.m_deviceContext->lpVtbl->GenerateMips (DX.m_deviceContext, v);
            }
          }
        }

        return;
      }
    }
    else
    {
      int time;

      media_grab_picture (m, ref m.image, out time);

      _unused time;

      if (m.image.pixel != null)   // we have a picture to display
      {
        if (m.image.width != m.previous_image_width || m.image.height != m.previous_image_height)
        {
          m.previous_image_width = m.image.width;
          m.previous_image_height = m.image.height;

          free_image (ref m.corrected_image);

          if (texture_id != 0)
          {
            free_texture (texture_id);
            texture_id = 0;
          }
        }

        stretch_to_4_3_screen (m.image, ref m.corrected_image);

        if (texture_id == 0)   // texture does not exist
        {
          texture_id = create_texture (m.corrected_image, allow_update => true, use_mipmapping => true);
        }
        else   // texture exists with correct size
        {
          update_texture (texture_id, m.corrected_image);
        }

        return;
      }
    }
  }

  if (texture_id != 0)   // there is still an active texture
  {
    free_texture (texture_id);
    texture_id = 0;
  }
}

//============================================================================================

// needs prior initialization of socket and directx libraries

public int start_media_player (out Media media, bool use_media_engine = true)
{
  clear media;

  media.use_media_engine_api = use_media_engine;

  if (run MediaThread (&media) < 0)
    return -1;

  // wait til player is initialized
  while (media.m_state == InitializationPending)
    sleep 0.033;

  if (media.m_state == InitializationFailed)
    return -1;

  return 0;
}

//============================================================================================

public void stop_media_player (ref Media media)
{
  if (media.m_state == InitializationFailed ||
      media.m_state == InitializationPending ||
      media.m_state == Finished)
    return;

  assert PostThreadMessageA (media.threadId, 900, 0, 0) != 0;

  while (media.m_state != Finished)
    sleep 0.033;
}

//============================================================================================
#end unsafe
//============================================================================================

#endif


#if ANDROID

use draw3d;

//--------------------------------------------------------------------------
struct Media
{
}
//--------------------------------------------------------------------------

// needs prior initialization of socket and DirectX libraries
// 'use_media_engine' : false = use windows 7 media session, true = use window 8 media engine.

public int start_media_player (out Media media, bool use_media_engine = true)
{
  clear media;
  _unused use_media_engine;
  return 0;
}

//--------------------------------------------------------------------------

public void stop_media_player (ref Media media)
{
  _unused media;
}

//--------------------------------------------------------------------------

public void media_play (Media media, wstring s)
{
  _unused media, s;
}

//--------------------------------------------------------------------------

public void media_sound (Media media, float volume, bool mute)
{
  _unused media, volume, mute;
}

//--------------------------------------------------------------------------

public void media_action (Media media, int action)     // action:  0=stop 1=pause 2=start
{
  _unused media, action;
}

//--------------------------------------------------------------------------

public void media_seek (Media media, int mode, float seconds)  // mode 0=absolute, 1=relative, 2=from end
{
  _unused media, mode, seconds;
}

//--------------------------------------------------------------------------

public bool media_is_playing        (Media media)
{
  _unused media;
  return false;
}

//--------------------------------------------------------------------------

public bool media_is_paused         (Media media)
{
  _unused media;
  return false;
}

//--------------------------------------------------------------------------

public float media_total_duration   (Media media)  // returns media duration in seconds, or 0.0 if not yet playing
{
  _unused media;
  return 0.0;
}

//--------------------------------------------------------------------------

public bool media_has_video_channel (Media media)
{
  _unused media;
  return false;
}

//--------------------------------------------------------------------------

public bool media_has_audio_channel (Media media)
{
  _unused media;
  return false;
}

//--------------------------------------------------------------------------

// updates a DirectX 11 texture that can be immediately used in-world.
// texture_id must be initialized to 0
// the function will automatically allocate and free texture_id as needed.

public void media_update_texture (Media media, ref TEXTURE_ID texture_id)
{
  _unused media, texture_id;
}

//--------------------------------------------------------------------------


#endif
