
// win/direct_show.h : direct show definitions

use windows;

#begin unsafe

//---------------------------------------------------------------------------------------------------------

typedef DWORD LCID;
typedef LONG DISPID;
typedef LONG SCODE;
typedef uint OAHWND;

const INT OATRUE = -1;
const INT OAFALSE = 0;

enum FILTER_STATE {State_Stopped, State_Paused, State_Running};

struct FILETIME
{
  uint dwLowDateTime;
  uint dwHighDateTime;
}

typedef int8 REFERENCE_TIME;

struct RECT
{
  int left;
  int top;
  int right;
  int bottom;
}

struct PALETTEENTRY
{
  BYTE        peRed;
  BYTE        peGreen;
  BYTE        peBlue;
  BYTE        peFlags;
}

struct RGNDATAHEADER
{
  DWORD   dwSize;
  DWORD   iType;
  DWORD   nCount;
  DWORD   nRgnSize;
  RECT    rcBound;
}

struct RGNDATA
{
  RGNDATAHEADER   rdh;
  char            Buffer[1];
}

struct CAUUID
{
  ULONG cElems;
  GUID* pElems;
}

const REFCLSID CLSID_SystemDeviceEnum         = {0x62BE5D10,0x60EB,0x11d0,0xBD,0x3B,0x00,0xA0,0xC9,0x11,0xCE,0x86};
const REFCLSID CLSID_VideoInputDeviceCategory = {0x860BB310,0x5D01,0x11d0,0xBD,0x3B,0x00,0xA0,0xC9,0x11,0xCE,0x86};
const REFCLSID CLSID_SampleGrabber            = {0xC1F400A0,0x3F08,0x11d3,0x9F,0x0B,0x00,0x60,0x08,0x03,0x9E,0x37};
const REFCLSID CLSID_FilterGraph              = {0xe436ebb3,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70};
const REFCLSID CLSID_CaptureGraphBuilder2     = {0xBF87B6E1,0x8C27,0x11d0,0xB3,0xF0,0x00,0xAA,0x00,0x37,0x61,0xC5};
const REFCLSID CLSID_NullRenderer             = {0xC1F400A4,0x3F08,0x11d3,0x9F,0x0B,0x00,0x60,0x08,0x03,0x9E,0x37};

const REFIID IID_CreateDevEnum = {0x29840822, 0x5B84, 0x11D0, 0xBD, 0x3B, 0x00, 0xA0, 0xC9, 0x11, 0xCE, 0x86};
const REFIID IID_IPropertyBag  = {0x55272A00, 0x42CB, 0x11CE, 0x81, 0x35, 0x00, 0xAA, 0x00, 0x4B, 0xB8, 0x51};
const REFIID IID_IBaseFilter   = {0x56a86895, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70};
const REFIID IID_IFilterGraph  = {0x56a8689f, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70};
const REFIID IID_IGraphBuilder = {0x56a868a9, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70};
const REFIID IID_IVideoWindow  = {0x56a868b4, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70};
const REFIID IID_IPin          = {0x56a86891, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70};
const REFIID IID_IAMStreamConfig ={0xC6E13340, 0x30AC, 0x11d0, 0xA1, 0x8C, 0x00, 0xA0, 0xC9, 0x11, 0x89, 0x56};
const REFIID IID_ISampleGrabber  ={0x6B652FFF, 0x11FE, 0x4fce, 0x92, 0xAD, 0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F};
const REFIID IID_ISampleGrabberCB={0x0579154A, 0x2B53, 0x4994, 0xB0, 0xD0, 0xE7, 0x73, 0x14, 0x8E, 0xFF, 0x85};
const REFIID IID_IEnumPins       ={0x56a86892, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70};
const REFIID IID_IOverlay        ={0x56a868a1, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70};
const REFIID IID_IOverlayNotify  ={0x56a868a0, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70};
const REFIID IID_ICaptureGraphBuilder2 ={0x93E5A4E0, 0x2D50, 0x11d2, 0xAB, 0xFA, 0x00, 0xA0, 0xC9, 0xC6, 0xE3, 0x8D};
const REFIID IID_IMediaControl   = {0x56a868b1, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70};
const REFIID IID_ISpecifyPropertyPages =
                                   {0xB196B28B, 0xBAB4, 0x101A, 0xB6, 0x9C, 0x00, 0xAA, 0x00, 0x34, 0x1D, 0x07};
const GUID MEDIATYPE_Interleaved = {0x73766169, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
const GUID MEDIATYPE_Video       = {0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
const GUID MEDIASUBTYPE_RGB24    = {0xe436eb7d, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70};

const GUID PIN_CATEGORY_CAPTURE= {0xfb6c4281, 0x0353, 0x11d1, 0x90, 0x5f, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba};
const GUID PIN_CATEGORY_PREVIEW= {0xfb6c4282, 0x0353, 0x11d1, 0x90, 0x5f, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba};
const GUID PIN_CATEGORY_VIDEOPORT={0xfb6c4285, 0x0353, 0x11d1, 0x90, 0x5f, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba};

const GUID FORMAT_VideoInfo   = {0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a};

//---------------------------------------------------------------------------------------------------------

// not used com definitions
typedef bool IStream;
typedef bool IBindCtx;
typedef bool IRecordInfo;
typedef bool IErrorLog;
typedef bool IReferenceClock;
typedef bool IEnumMediaTypes;
typedef bool IEnumFilters;
typedef bool IFileSinkFilter;
typedef bool IAMCopyCaptureFileProgress;
typedef bool ITypeInfo;
typedef bool DISPPARAMS;
typedef bool IOverlayNotify;

//---------------------------------------------------------------------------------------------------------

// forward types
typedef IMoniker;
typedef IEnumMoniker;
typedef ICreateDevEnum;
typedef IPropertyBag;
typedef IPin;
typedef IBaseFilter;
typedef IFilterGraph;
typedef IEnumPins;

//---------------------------------------------------------------------------------------------------------

[extern "OLEAut32.DLL"]
HRESULT OleCreatePropertyFrame(HWND hwndOwner, UINT x, UINT y, LPCOLESTR lpszCaption,
                 ULONG cObjects, LPUNKNOWN *ppUnk, ULONG cPages, LPCLSID* pPageClsID,
                 LCID lcid, DWORD dwReserved, LPVOID pvReserved);

typedef short  VARIANT_BOOL;

packed struct CYs
{
  uint Lo;
  int  Hi;
}

union CY
{
  CYs  cys;
  long int64;
}

packed struct TAGv2
{
  PVOID pvRecord;
  IRecordInfo  *pRecInfo;
}

typedef wchar OLECHAR;
typedef OLECHAR *BSTR;

typedef double DATE;

struct SAFEARRAYBOUND
{
 ULONG cElements;
 LONG  lLbound;
}

struct SAFEARRAY
{
  USHORT cDims;
  USHORT fFeatures;
  ULONG cbElements;
  ULONG cLocks;
  PVOID pvData;
  SAFEARRAYBOUND rgsabound[1];
}

packed struct DEC11
{
  BYTE scale;
  BYTE sign;
}

union DEC1
{
  DEC11  dec11;
  USHORT signscale;
}

packed struct DEC22
{
  ULONG Lo32;
  ULONG Mid32;
}

union DEC2
{
  DEC22     dec22;
  ULONGLONG Lo64;
}

packed struct DECIMAL
{
  USHORT wReserved;
  DEC1   dec1;
  ULONG  Hi32;
  DEC2   dec2;
}


typedef VARIANT;  // forward declaration

union TAGv
{
  LONG lVal;
  BYTE bVal;
  SHORT iVal;
  FLOAT fltVal;
  DOUBLE dblVal;
  VARIANT_BOOL boolVal;

  SCODE scode;
  CY    cyVal;
  DATE  date;
  BSTR  bstrVal;

#if 0
  IUnknown  *punkVal;
  IDispatch  *pdispVal;
#endif
  SAFEARRAY  *parray;
  BYTE       *pbVal;
  SHORT      *piVal;
  LONG       *plVal;
  FLOAT      *pfltVal;
  DOUBLE     *pdblVal;
  VARIANT_BOOL  *pboolVal;

  SCODE  *pscode;
  CY  *pcyVal;
  DATE  *pdate;
  BSTR  *pbstrVal;
#if 0
  IUnknown  * *ppunkVal;
  IDispatch  * *ppdispVal;
#endif
  SAFEARRAY  * *pparray;
  VARIANT  *pvarVal;
  PVOID byref;
  CHAR cVal;
  USHORT uiVal;
  ULONG ulVal;
  INT intVal;
  UINT uintVal;
  DECIMAL  *pdecVal;
  CHAR  *pcVal;
  USHORT  *puiVal;
  ULONG  *pulVal;
  INT  *pintVal;
  UINT  *puintVal;
  TAGv2 tagv2;
}

typedef ushort VARTYPE;

const VARTYPE VT_BSTR = 8;

struct VARIANT
{
  VARTYPE vt;
  WORD    wReserved1;
  WORD    wReserved2;
  WORD    wReserved3;
  TAGv    tagv;
}

struct EXCEPINFO
{
  WORD  wCode;
  WORD  wReserved;
  BSTR  bstrSource;
  BSTR  bstrDescription;
  BSTR  bstrHelpFile;
  DWORD dwHelpContext;
  PVOID pvReserved;
  byte *ptr; // HRESULT (*pfnDeferredFillIn)(struct tagEXCEPINFO *);
  SCODE scode;
}

struct AM_MEDIA_TYPE
{
 GUID majortype;
 GUID subtype;
 BOOL bFixedSizeSamples;
 BOOL bTemporalCompression;
 ULONG lSampleSize;
 GUID formattype;
 IUnknown *pUnk;
 ULONG cbFormat;
 BYTE *pbFormat;
}

enum PIN_DIRECTION {PINDIR_INPUT, PINDIR_OUTPUT};

struct PIN_INFO
{
  IBaseFilter *pFilter;
  PIN_DIRECTION dir;
  wchar achName[128];
}

struct FILTER_INFO
{
  WCHAR achName[128];
  IFilterGraph *pGraph;
}

struct VIDEOINFOHEADER
{
  RECT             rcSource;
  RECT             rcTarget;
  DWORD            dwBitRate;
  DWORD            dwBitErrorRate;
  REFERENCE_TIME   AvgTimePerFrame;
  BITMAPINFOHEADER bmiHeader;
}

struct SIZE
{
  LONG cx;
  LONG cy;
}

struct VIDEO_STREAM_CONFIG_CAPS
{
  GUID guid;
  ULONG VideoStandard;
  SIZE InputSize;
  SIZE MinCroppingSize;
  SIZE MaxCroppingSize;
  int CropGranularityX;
  int CropGranularityY;
  int CropAlignX;
  int CropAlignY;
  SIZE MinOutputSize;
  SIZE MaxOutputSize;
  int OutputGranularityX;
  int OutputGranularityY;
  int StretchTapsX;
  int StretchTapsY;
  int ShrinkTapsX;
  int ShrinkTapsY;
  LONGLONG MinFrameInterval;
  LONGLONG MaxFrameInterval;
  LONG MinBitsPerSecond;
  LONG MaxBitsPerSecond;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FRead  (IPropertyBag* This, LPCOLESTR pszPropName, ref VARIANT pVar, IErrorLog *pErrorLog);
typedef [callback] HRESULT FWrite (IPropertyBag* This, LPCOLESTR pszPropName, VARIANT pVar);

struct IPropertyBagVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FRead                  Read;
  FWrite                 Write;
}

struct IPropertyBag
{
  IPropertyBagVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FGetClassID    (LPVOID This, out REFCLSID pClassID);
typedef [callback] HRESULT FIsDirty       (LPVOID This);
typedef [callback] HRESULT FLoad          (LPVOID This, IStream pStm);
typedef [callback] HRESULT FSave          (LPVOID This, IStream pStm, BOOL fClearDirty);
typedef [callback] HRESULT FGetSizeMax    (LPVOID This, out ULARGE_INTEGER pcbSize);
typedef [callback] HRESULT FBindToObject  (LPVOID This, IBindCtx* pbc, IMoniker* pmkToLeft, REFIID riidResult,
                                             out LPVOID ppvResult);
typedef [callback] HRESULT FBindToStorage (LPVOID This, IBindCtx pbc, IMoniker* pmkToLeft, REFIID riid,
                                             out LPVOID ppvObj);
typedef [callback] HRESULT FReduce        (LPVOID This, IBindCtx pbc, DWORD dwReduceHowFar,
                                             ref IMoniker* ppmkToLeft,
                                             out IMoniker* ppmkReduced);
typedef [callback] HRESULT FComposeWith   (LPVOID This, IMoniker pmkRight, BOOL fOnlyIfNotGeneric,
                                             out IMoniker* ppmkComposite);
typedef [callback] HRESULT FEnum          (LPVOID This, BOOL fForward, out IEnumMoniker* ppenumMoniker);
typedef [callback] HRESULT FIsEqual       (LPVOID This, IMoniker pmkOtherMoniker);
typedef [callback] HRESULT FHash          (LPVOID This, out DWORD pdwHash);
typedef [callback] HRESULT FIsRunning     (LPVOID This, IBindCtx pbc, IMoniker pmkToLeft, IMoniker pmkNewlyRunning);
typedef [callback] HRESULT FGetTimeOfLastChange (LPVOID This, IBindCtx pbc, IMoniker pmkToLeft,
                                             out FILETIME pFileTime);
typedef [callback] HRESULT FInverse       (IMoniker This, out IMoniker* ppmk);
typedef [callback] HRESULT FCommonPrefixWith (IMoniker This, IMoniker pmkOther, out IMoniker* ppmkPrefix);
typedef [callback] HRESULT FRelativePathTo (IMoniker This, IMoniker pmkOther, out IMoniker* ppmkRelPath);
typedef [callback] HRESULT FGetDisplayName (IMoniker This, IBindCtx pbc, IMoniker pmkToLeft,
                                               out LPOLESTR ppszDisplayName);
typedef [callback] HRESULT FParseDisplayName (IMoniker This, IBindCtx pbc, IMoniker pmkToLeft, LPOLESTR pszDisplayName,
                                  out ULONG pchEaten, out IMoniker* ppmkOut);
typedef [callback] HRESULT FIsSystemMoniker  (IMoniker This, out DWORD pdwMksys);


struct IMonikerVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FGetClassID            GetClassID;
  FIsDirty               IsDirty;
  FLoad                  Load;
  FSave                  Save;
  FGetSizeMax            GetSizeMax;
  FBindToObject          BindToObject;
  FBindToStorage         BindToStorage;
  FReduce                Reduce;
  FComposeWith           ComposeWith;
  FEnum                  Enum;
  FIsEqual               IsEqual;
  FHash                  Hash;
  FIsRunning             IsRunning;
  FGetTimeOfLastChange   GetTimeOfLastChange;
  FInverse               Inverse;
  FCommonPrefixWith      CommonPrefixWith;
  FRelativePathTo        RelativePathTo;
  FGetDisplayName        GetDisplayName;
  FParseDisplayName      ParseDisplayName;
  FIsSystemMoniker       IsSystemMoniker;
}

struct IMoniker
{
  IMonikerVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FNext  (IEnumMoniker* This, ULONG celt, out IMoniker *rgelt, ULONG* pceltFetched);
typedef [callback] HRESULT FSkip  (IEnumMoniker* This, ULONG celt);
typedef [callback] HRESULT FReset (IEnumMoniker* This);
typedef [callback] HRESULT FClone (IEnumMoniker* This, out IEnumMoniker* ppenum);

struct IEnumMonikerVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FNext                  Next;
  FSkip                  Skip;
  FReset                 Reset;
  FClone                 Clone;
}

struct IEnumMoniker
{
  IEnumMonikerVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FCreateClassEnumerator (ICreateDevEnum* This,
                                                   REFCLSID        clsidDeviceClass,
                                                   IEnumMoniker**  ppEnumMoniker,
                                                   DWORD           dwFlags);

struct ICreateDevEnumVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FCreateClassEnumerator CreateClassEnumerator;
}

struct ICreateDevEnum
{
  ICreateDevEnumVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FConnect           (LPVOID This, IPin *pReceivePin, AM_MEDIA_TYPE *pmt);
typedef [callback] HRESULT FReceiveConnection (LPVOID This, IPin *pConnector, AM_MEDIA_TYPE *pmt);
typedef [callback] HRESULT FDisconnect (LPVOID This);
typedef [callback] HRESULT FConnectedTo (LPVOID This, IPin **pPin);
typedef [callback] HRESULT FConnectionMediaType (LPVOID This, AM_MEDIA_TYPE *pmt);
typedef [callback] HRESULT FQueryPinInfo (LPVOID This, PIN_INFO *pInfo);
typedef [callback] HRESULT FQueryDirection (LPVOID This, PIN_DIRECTION *pPinDir);
typedef [callback] HRESULT FQueryId (LPVOID This, LPWSTR *Id);
typedef [callback] HRESULT FQueryAccept (LPVOID This, AM_MEDIA_TYPE *pmt);
typedef [callback] HRESULT FEnumMediaTypes (LPVOID This, IEnumMediaTypes **ppEnum);
typedef [callback] HRESULT FQueryInternalConnections(LPVOID This, IPin **apPin, ULONG *nPin);
typedef [callback] HRESULT FEndOfStream (LPVOID This);
typedef [callback] HRESULT FBeginFlush  (LPVOID This);
typedef [callback] HRESULT FEndFlush    (LPVOID This);
typedef [callback] HRESULT FNewSegment  (LPVOID This, REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

struct IPinVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FConnect               Connect;
  FReceiveConnection     ReceiveConnection;
  FDisconnect            Disconnect;
  FConnectedTo           ConnectedTo;
  FConnectionMediaType   ConnectionMediaType;
  FQueryPinInfo          QueryPinInfo;
  FQueryDirection        QueryDirection;
  FQueryId               QueryId;
  FQueryAccept           QueryAccept;
  FEnumMediaTypes        EnumMediaTypes;
  FQueryInternalConnections  QueryInternalConnections;
  FEndOfStream           EndOfStream;
  FBeginFlush            BeginFlush;
  FEndFlush              EndFlush;
  FNewSegment            NewSegment;
}

struct IPin
{
  IPinVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FGetPointer (LPVOID This, BYTE **ppBuffer);
typedef [callback] int FGetSize(LPVOID This);
typedef [callback] HRESULT FGetTime(LPVOID This, REFERENCE_TIME *pTimeStart, REFERENCE_TIME *pTimeEnd);
typedef [callback] HRESULT FSetTime(LPVOID This, REFERENCE_TIME *pTimeStart, REFERENCE_TIME *pTimeEnd);
typedef [callback] HRESULT FIsSyncPoint(LPVOID This);
typedef [callback] HRESULT FSetSyncPoint(LPVOID This, BOOL bIsSyncPoint);
typedef [callback] HRESULT FIsPreroll(LPVOID This);
typedef [callback] HRESULT FSetPreroll(LPVOID This, BOOL bIsPreroll);
typedef [callback] int FGetActualDataLength(LPVOID This);
typedef [callback] HRESULT FSetActualDataLength(LPVOID This, int __MIDL_0010);
typedef [callback] HRESULT FGetMediaType(LPVOID This, AM_MEDIA_TYPE **ppMediaType);
typedef [callback] HRESULT FSetMediaType(LPVOID This, AM_MEDIA_TYPE pMediaType);
typedef [callback] HRESULT FIsDiscontinuity(LPVOID This);
typedef [callback] HRESULT FSetDiscontinuity(LPVOID This, BOOL bDiscontinuity);
typedef [callback] HRESULT FGetMediaTime(LPVOID This, LONGLONG *pTimeStart, LONGLONG *pTimeEnd);
typedef [callback] HRESULT FSetMediaTime(LPVOID This, LONGLONG *pTimeStart, LONGLONG *pTimeEnd);

struct IMediaSampleVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;

  FGetPointer    GetPointer;
  FGetSize       GetSize;
  FGetTime       GetTime;
  FSetTime       SetTime;
  FIsSyncPoint   IsSyncPoint;
  FSetSyncPoint  SetSyncPoint;
  FIsPreroll     IsPreroll;
  FSetPreroll    SetPreroll;
  FGetActualDataLength  GetActualDataLength;
  FSetActualDataLength  SetActualDataLength;
  FGetMediaType  GetMediaType;
  FSetMediaType  SetMediaType;
  FIsDiscontinuity   IsDiscontinuity;
  FSetDiscontinuity  SetDiscontinuity;
  FGetMediaTime  GetMediaTime;
  FSetMediaTime  SetMediaTime;
}

struct IMediaSample
{
  IMediaSampleVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FAddFilter(LPVOID This, IBaseFilter *pFilter, LPCWSTR pName);
typedef [callback] HRESULT FRemoveFilter(LPVOID This, IBaseFilter *pFilter);
typedef [callback] HRESULT FEnumFilters(LPVOID This, IEnumFilters **ppEnum);
typedef [callback] HRESULT FFindFilterByName(LPVOID This, LPCWSTR pName, IBaseFilter **ppFilter);
typedef [callback] HRESULT FConnectDirect(LPVOID This, IPin *ppinOut, IPin *ppinIn, AM_MEDIA_TYPE *pmt);
typedef [callback] HRESULT FReconnect(LPVOID This, IPin *ppin);
typedef [callback] HRESULT FDisconnectPin(LPVOID This, IPin *ppin);
typedef [callback] HRESULT FSetDefaultSyncSource(LPVOID This);

struct IFilterGraphVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;

  FAddFilter          AddFilter;
  FRemoveFilter       RemoveFilter;
  FEnumFilters        EnumFilters;
  FFindFilterByName   FindFilterByName;
  FConnectDirect      ConnectDirect;
  FReconnect          Reconnect;
  FDisconnectPin      Disconnect;
  FSetDefaultSyncSource  SetDefaultSyncSource;
}

struct IFilterGraph
{
  IFilterGraphVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FConnectPP(LPVOID This, IPin *ppinOut, IPin *ppinIn);
typedef [callback] HRESULT FRender(LPVOID This, IPin *ppinOut);
typedef [callback] HRESULT FRenderFile(LPVOID This, LPCWSTR lpcwstrFile, LPCWSTR lpcwstrPlayList);
typedef [callback] HRESULT FAddSourceFilter(LPVOID This, LPCWSTR lpcwstrFileName, LPCWSTR lpcwstrFilterName, IBaseFilter **ppFilter);
typedef [callback] HRESULT FSetLogFile (LPVOID This, DWORD_PTR hFile);
typedef [callback] HRESULT FAbort(LPVOID This);
typedef [callback] HRESULT FShouldOperationContinue(LPVOID This);

struct IGraphBuilderVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;

  FAddFilter          AddFilter;
  FRemoveFilter       RemoveFilter;
  FEnumFilters        EnumFilters;
  FFindFilterByName   FindFilterByName;
  FConnectDirect      ConnectDirect;
  FReconnect          Reconnect;
  FDisconnectPin      Disconnect;
  FSetDefaultSyncSource  SetDefaultSyncSource;

  FConnectPP  Connect;
  FRender  Render;
  FRenderFile  RenderFile;
  FAddSourceFilter  AddSourceFilter;
  FSetLogFile  SetLogFile;
  FAbort  Abort;
  FShouldOperationContinue  ShouldOperationContinue;
}

struct IGraphBuilder
{
  IGraphBuilderVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FSetOneShot (LPVOID This, BOOL OneShot);
typedef [callback] HRESULT FGetConnectedMediaType (LPVOID This, out AM_MEDIA_TYPE pType);
typedef [callback] HRESULT FSetBufferSamples(LPVOID This, BOOL BufferThem);
typedef [callback] HRESULT FGetCurrentBuffer(LPVOID This, int *pBufferSize, int *pBuffer);
typedef [callback] HRESULT FGetCurrentSample(LPVOID This, IMediaSample **ppSample);
typedef [callback] HRESULT FSetCallback(LPVOID This, LPVOID pCallback, int WhichMethodToCallback);

struct ISampleGrabberVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;

  FSetOneShot  SetOneShot;
  FSetMediaType  SetMediaType;
  FGetConnectedMediaType  GetConnectedMediaType;
  FSetBufferSamples  SetBufferSamples;
  FGetCurrentBuffer  GetCurrentBuffer;
  FGetCurrentSample  GetCurrentSample;
  FSetCallback  SetCallback;
}

struct ISampleGrabber
{
  ISampleGrabberVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FSampleCB (PVOID This, double SampleTime, IMediaSample *pSample);
typedef [callback] HRESULT FBufferCB (PVOID This, double SampleTime, BYTE *pBuffer, int BufferLen);

struct ISampleGrabberCBVtbl
{
  FQueryInterface  QueryInterface;
  FAddRef          AddRef;
  FRelease         Release;
  FSampleCB        SampleCB;
  FBufferCB        BufferCB;
}

struct ISampleGrabberCB
{
  ISampleGrabberCBVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

struct IPersistVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FGetClassID            GetClassID;
}

struct IPersist
{
  IPersistVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FStop  (LPVOID This);
typedef [callback] HRESULT FPause (LPVOID This);
typedef [callback] HRESULT FRun   (LPVOID This, REFERENCE_TIME tStart);
typedef [callback] HRESULT FGetState (LPVOID This, DWORD dwMilliSecsTimeout, out FILTER_STATE State);
typedef [callback] HRESULT FSetSyncSource (LPVOID This, IReferenceClock pClock);
typedef [callback] HRESULT FGetSyncSource (LPVOID This, out IReferenceClock *pClock);

struct IMediaFilterVtbl
{
  FQueryInterface QueryInterface;
  FAddRef         AddRef;
  FRelease        Release;

  FGetClassID     GetClassID;
  FStop           Stop;
  FPause          Pause;
  FRun            Run;
  FGetState       GetState;
  FSetSyncSource  SetSyncSource;
  FGetSyncSource  GetSyncSource;
}

struct IMediaFilter
{
  IMediaFilterVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FEnumPins        (LPVOID This, IEnumPins **ppEnum);
typedef [callback] HRESULT FFindPin         (LPVOID This, LPCWSTR Id, IPin **ppPin);
typedef [callback] HRESULT FQueryFilterInfo (LPVOID This, FILTER_INFO *pInfo);
typedef [callback] HRESULT FJoinFilterGraph (LPVOID This, IFilterGraph *pGraph, LPCWSTR pName);
typedef [callback] HRESULT FQueryVendorInfo (LPVOID This, LPWSTR *pVendorInfo);

struct IBaseFilterVtbl
{
  FQueryInterface QueryInterface;
  FAddRef         AddRef;
  FRelease        Release;

  FGetClassID     GetClassID;
  FStop           Stop;
  FPause          Pause;
  FRun            Run;
  FGetState       GetState;
  FSetSyncSource  SetSyncSource;
  FGetSyncSource  GetSyncSource;
  FEnumPins       EnumPins;
  FFindPin        FindPin;
  FQueryFilterInfo  QueryFilterInfo;
  FJoinFilterGraph  JoinFilterGraph;
  FQueryVendorInfo  QueryVendorInfo;
}

struct IBaseFilter
{
  IBaseFilterVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FSetFiltergraph (LPVOID This, IGraphBuilder *pfg);
typedef [callback] HRESULT FGetFiltergraph (LPVOID This, IGraphBuilder **ppfg);
typedef [callback] HRESULT FSetOutputFileName (LPVOID This, GUID *pType, LPCOLESTR lpstrFile, IBaseFilter **ppf, IFileSinkFilter **ppSink);
typedef [callback] HRESULT FFindInterface(LPVOID This, GUID *pCategory, GUID *pType, IBaseFilter *pf, IID riid, byte** ppint);
typedef [callback] HRESULT FRenderStream(LPVOID This, GUID pCategory, GUID pType, IUnknown *pSource, IBaseFilter *pfCompressor, IBaseFilter *pfRenderer);
typedef [callback] HRESULT FControlStream(LPVOID This, GUID *pCategory, GUID *pType, IBaseFilter *pFilter, REFERENCE_TIME *pstart, REFERENCE_TIME *pstop, WORD wStartCookie, WORD wStopCookie);
typedef [callback] HRESULT FAllocCapFile(LPVOID This, LPCOLESTR lpstr, DWORDLONG dwlSize);
typedef [callback] HRESULT FCopyCaptureFile(LPVOID This, LPOLESTR lpwstrOld, LPOLESTR lpwstrNew, int fAllowEscAbort, IAMCopyCaptureFileProgress *pCallback);
typedef [callback] HRESULT FFindPinSEC(LPVOID This, IUnknown *pSource, PIN_DIRECTION pindir, GUID pCategory, GUID *pType, BOOL fUnconnected, int num, out IPin* ppPin);

struct ICaptureGraphBuilder2Vtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;

  FSetFiltergraph  SetFiltergraph;
  FGetFiltergraph  GetFiltergraph;
  FSetOutputFileName  SetOutputFileName;
  FFindInterface  FindInterface;
  FRenderStream  RenderStream;
  FControlStream  ControlStream;
  FAllocCapFile  AllocCapFile;
  FCopyCaptureFile  CopyCaptureFile;
  FFindPinSEC  FindPin;
}

struct ICaptureGraphBuilder2
{
  ICaptureGraphBuilder2Vtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FSetFormat(LPVOID This, AM_MEDIA_TYPE *pmt);
typedef [callback] HRESULT FGetFormat(LPVOID This, AM_MEDIA_TYPE **ppmt);
typedef [callback] HRESULT FGetNumberOfCapabilities(LPVOID This, int *piCount, int *piSize);
typedef [callback] HRESULT FGetStreamCaps(LPVOID This, int iIndex, AM_MEDIA_TYPE **ppmt, BYTE *pSCC);

struct IAMStreamConfigVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;

  FSetFormat  SetFormat;
  FGetFormat  GetFormat;
  FGetNumberOfCapabilities  GetNumberOfCapabilities;
  FGetStreamCaps  GetStreamCaps;
}

struct IAMStreamConfig
{
  IAMStreamConfigVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FGetTypeInfoCount(LPVOID This, UINT  *pctinfo);
typedef [callback] HRESULT FGetTypeInfo(LPVOID This, UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo);
typedef [callback] HRESULT FGetIDsOfNames(LPVOID This, IID riid, LPOLESTR  *rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId);
typedef [callback] HRESULT FInvoke(LPVOID This, DISPID dispIdMember, IID riid, LCID lcid, WORD wFlags, DISPPARAMS  *pDispParams, VARIANT  *pVarResult, EXCEPINFO  *pExcepInfo, UINT  *puArgErr);

struct IDispatchVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;

  FGetTypeInfoCount GetTypeInfoCount;
  FGetTypeInfo      GetTypeInfo;
  FGetIDsOfNames    GetIDsOfNames;
  FInvoke           Invoke;
}

struct IDispatch
{
  IDispatchVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT Fput_Caption(LPVOID This, BSTR strCaption);
typedef [callback] HRESULT Fget_Caption(LPVOID This, BSTR *strCaption);
typedef [callback] HRESULT Fput_WindowStyle(LPVOID This, uint WindowStyle);
typedef [callback] HRESULT Fget_WindowStyle(LPVOID This, int *WindowStyle);
typedef [callback] HRESULT Fput_WindowStyleEx(LPVOID This, int WindowStyleEx);
typedef [callback] HRESULT Fget_WindowStyleEx(LPVOID This, int *WindowStyleEx);
typedef [callback] HRESULT Fput_AutoShow(LPVOID This, int AutoShow);
typedef [callback] HRESULT Fget_AutoShow(LPVOID This, int *AutoShow);
typedef [callback] HRESULT Fput_WindowState(LPVOID This, int WindowState);
typedef [callback] HRESULT Fget_WindowState(LPVOID This, int *WindowState);
typedef [callback] HRESULT Fput_BackgroundPalette(LPVOID This, int BackgroundPalette);
typedef [callback] HRESULT Fget_BackgroundPalette(LPVOID This, int *pBackgroundPalette);
typedef [callback] HRESULT Fput_Visible(LPVOID This, int Visible);
typedef [callback] HRESULT Fget_Visible(LPVOID This, int *pVisible);
typedef [callback] HRESULT Fput_Left(LPVOID This, int Left);
typedef [callback] HRESULT Fget_Left(LPVOID This, int *pLeft);
typedef [callback] HRESULT Fput_Width(LPVOID This, int Width);
typedef [callback] HRESULT Fget_Width(LPVOID This, int *pWidth);
typedef [callback] HRESULT Fput_Top(LPVOID This, int Top);
typedef [callback] HRESULT Fget_Top(LPVOID This, int *pTop);
typedef [callback] HRESULT Fput_Height(LPVOID This, int Height);
typedef [callback] HRESULT Fget_Height(LPVOID This, int *pHeight);
typedef [callback] HRESULT Fput_Owner(LPVOID This, OAHWND Owner);
typedef [callback] HRESULT Fget_Owner(LPVOID This, OAHWND *Owner);
typedef [callback] HRESULT Fput_MessageDrain(LPVOID This, OAHWND Drain);
typedef [callback] HRESULT Fget_MessageDrain(LPVOID This, OAHWND *Drain);
typedef [callback] HRESULT Fget_BorderColor(LPVOID This, int *Color);
typedef [callback] HRESULT Fput_BorderColor(LPVOID This, int Color);
typedef [callback] HRESULT Fget_FullScreenMode(LPVOID This, int *FullScreenMode);
typedef [callback] HRESULT Fput_FullScreenMode(LPVOID This, int FullScreenMode);
typedef [callback] HRESULT FSetWindowForeground(LPVOID This, int Focus);
typedef [callback] HRESULT FNotifyOwnerMessage(LPVOID This, OAHWND hwnd, int uMsg, INT_PTR wParam, INT_PTR lParam);
typedef [callback] HRESULT FSetWindowPosition(LPVOID This, int Left, int Top, int Width, int Height);
typedef [callback] HRESULT FGetWindowPosition(LPVOID This, int *pLeft, int *pTop, int *pWidth, int *pHeight);
typedef [callback] HRESULT FGetMinIdealImageSize(LPVOID This, int *pWidth, int *pHeight);
typedef [callback] HRESULT FGetMaxIdealImageSize(LPVOID This, int *pWidth, int *pHeight);
typedef [callback] HRESULT FGetRestorePosition(LPVOID This, int *pLeft, int *pTop, int *pWidth, int *pHeight);
typedef [callback] HRESULT FHideCursor(LPVOID This, int HideCursor);
typedef [callback] HRESULT FIsCursorHidden(LPVOID This, int *CursorHidden);

struct IVideoWindowVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;

  FGetTypeInfoCount GetTypeInfoCount;
  FGetTypeInfo      GetTypeInfo;
  FGetIDsOfNames    GetIDsOfNames;
  FInvoke           Invoke;

  Fput_Caption  put_Caption;
  Fget_Caption  get_Caption;
  Fput_WindowStyle  put_WindowStyle;
  Fget_WindowStyle  get_WindowStyle;
  Fput_WindowStyleEx  put_WindowStyleEx;
  Fget_WindowStyleEx  get_WindowStyleEx;
  Fput_AutoShow  put_AutoShow;
  Fget_AutoShow  get_AutoShow;
  Fput_WindowState  put_WindowState;
  Fget_WindowState  get_WindowState;
  Fput_BackgroundPalette  put_BackgroundPalette;
  Fget_BackgroundPalette  get_BackgroundPalette;
  Fput_Visible  put_Visible;
  Fget_Visible  get_Visible;
  Fput_Left  put_Left;
  Fget_Left  get_Left;
  Fput_Width  put_Width;
  Fget_Width  get_Width;
  Fput_Top  put_Top;
  Fget_Top  get_Top;
  Fput_Height  put_Height;
  Fget_Height  get_Height;
  Fput_Owner  put_Owner;
  Fget_Owner  get_Owner;
  Fput_MessageDrain  put_MessageDrain;
  Fget_MessageDrain  get_MessageDrain;
  Fget_BorderColor  get_BorderColor;
  Fput_BorderColor  put_BorderColor;
  Fget_FullScreenMode  get_FullScreenMode;
  Fput_FullScreenMode  put_FullScreenMode;
  FSetWindowForeground  SetWindowForeground;
  FNotifyOwnerMessage  NotifyOwnerMessage;
  FSetWindowPosition  SetWindowPosition;
  FGetWindowPosition  GetWindowPosition;
  FGetMinIdealImageSize  GetMinIdealImageSize;
  FGetMaxIdealImageSize  GetMaxIdealImageSize;
  FGetRestorePosition  GetRestorePosition;
  FHideCursor  HideCursor;
  FIsCursorHidden  IsCursorHidden;
}

struct IVideoWindow
{
  IVideoWindowVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FRunSEC   (LPVOID This);
typedef [callback] HRESULT FPauseSEC (LPVOID This);
typedef [callback] HRESULT FStopSEC  (LPVOID This);
typedef [callback] HRESULT FGetStateSEC(LPVOID This, LONG msTimeout, FILTER_STATE *pfs);
typedef [callback] HRESULT FRenderFileSEC(LPVOID This, BSTR strFilename);
typedef [callback] HRESULT FAddSourceFilterSEC(LPVOID This, BSTR strFilename, IDispatch **ppUnk);
typedef [callback] HRESULT Fget_FilterCollection(LPVOID This, IDispatch **ppUnk);
typedef [callback] HRESULT Fget_RegFilterCollection(LPVOID This, IDispatch **ppUnk);
typedef [callback] HRESULT FStopWhenReady(LPVOID This);

struct IMediaControlVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;

  FGetTypeInfoCount  GetTypeInfoCount;
  FGetTypeInfo       GetTypeInfo;
  FGetIDsOfNames     GetIDsOfNames;
  FInvoke            Invoke;

  FRunSEC         Run;
  FPauseSEC       Pause;
  FStopSEC        Stop;
  FGetStateSEC    GetState;
  FRenderFileSEC  RenderFile;
  FAddSourceFilterSEC       AddSourceFilter;
  Fget_FilterCollection     get_FilterCollection;
  Fget_RegFilterCollection  get_RegFilterCollection;
  FStopWhenReady            StopWhenReady;
}

struct IMediaControl
{
  IMediaControlVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT AFNext  (LPVOID This, ULONG cPins, IPin **ppPins, ULONG *pcFetched);
typedef [callback] HRESULT AFSkip  (LPVOID This, ULONG cPins);
typedef [callback] HRESULT AFReset (LPVOID This);
typedef [callback] HRESULT AFClone (LPVOID This, IEnumPins **ppEnum);

struct IEnumPinsVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;

  AFNext  Next;
  AFSkip  Skip;
  AFReset  Reset;
  AFClone  Clone;
}

struct IEnumPins
{
  IEnumPinsVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FGetPalette(LPVOID This, DWORD *pdwColors, PALETTEENTRY **ppPalette);
typedef [callback] HRESULT FSetPalette(LPVOID This, DWORD dwColors, PALETTEENTRY *pPalette);
typedef [callback] HRESULT FGetDefaultColorKey(LPVOID This, COLORKEY *pColorKey);
typedef [callback] HRESULT FGetColorKey(LPVOID This, COLORKEY *pColorKey);
typedef [callback] HRESULT FSetColorKey(LPVOID This, COLORKEY *pColorKey);
typedef [callback] HRESULT FGetWindowHandle(LPVOID This, HWND *pHwnd);
typedef [callback] HRESULT FGetClipList(LPVOID This, RECT *pSourceRect, RECT *pDestinationRect, RGNDATA **ppRgnData);
typedef [callback] HRESULT FGetVideoPosition(LPVOID This, RECT *pSourceRect, RECT *pDestinationRect);
typedef [callback] HRESULT FAdvise(LPVOID This, IOverlayNotify *pOverlayNotify, DWORD dwInterests);
typedef [callback] HRESULT FUnadvise(LPVOID This);

struct IOverlayVtbl
{
 FQueryInterface   QueryInterface;
 FAddRef           AddRef;
 FRelease          Release;

 FGetPalette GetPalette;
 FSetPalette SetPalette;
 FGetDefaultColorKey GetDefaultColorKey;
 FGetColorKey GetColorKey;
 FSetColorKey SetColorKey;
 FGetWindowHandle GetWindowHandle;
 FGetClipList GetClipList;
 FGetVideoPosition GetVideoPosition;
 FAdvise Advise;
 FUnadvise Unadvise;
}

struct IOverlay
{
  IOverlayVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

typedef [callback] HRESULT FGetPages (LPVOID This, CAUUID *pPages);

struct ISpecifyPropertyPagesVtbl
{
  FQueryInterface  QueryInterface;
  FAddRef          AddRef;
  FRelease         Release;

  FGetPages GetPages;
}

struct ISpecifyPropertyPages
{
  ISpecifyPropertyPagesVtbl* lpVtbl;
}

//---------------------------------------------------------------------------------------------------------

#end unsafe
