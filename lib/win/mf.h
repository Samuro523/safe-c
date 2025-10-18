
// mf.h : media foundation

use windows;

//============================================================================================
#begin unsafe
//============================================================================================

//const uint MF_SDK_VERSION = 0x0001;  // vista
const uint MF_SDK_VERSION = 0x0002;  // windows 7

const uint MF_API_VERSION = 0x0070;
const uint MF_VERSION = (MF_SDK_VERSION << 16 | MF_API_VERSION);

const uint MFSTARTUP_NOSOCKET = 0x1;    // don't initialize socket library

typedef DWORD MediaEventType;
const DWORD MESessionClosed = 106;
const uint MF_TOPOSTATUS_READY = 100;
enum MF_OBJECT_TYPE {MF_OBJECT_MEDIASOURCE, MF_OBJECT_BYTESTREAM, MF_OBJECT_INVALID};
enum MF_TOPOLOGY_TYPE {MF_TOPOLOGY_OUTPUT_NODE, MF_TOPOLOGY_SOURCESTREAM_NODE, MF_TOPOLOGY_TRANSFORM_NODE, MF_TOPOLOGY_TEE_NODE};

const uint MF_RESOLUTION_MEDIASOURCE = 0x1;
const uint MF_RESOLUTION_BYTESTREAM  = 0x2;

const GUID GUID_NULL                  = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const GUID MF_EVENT_TOPOLOGY_STATUS   = {0x30c5018d, 0x9a53, 0x454b, 0xad, 0x9e, 0x6d, 0x5f, 0x8f, 0xa7, 0xc4, 0x3b};
const GUID MR_VIDEO_RENDER_SERVICE    = {0x1092a86c, 0xab1a, 0x459a, 0xa3, 0x36, 0x83, 0x1f, 0xbc, 0x4d, 0x11, 0xff};
const GUID IID_IMFAsyncCallback       = {0xa27003cf, 0x2354, 0x4f2a, 0x8d, 0x6a, 0xab, 0x7c, 0xff, 0x15, 0x43, 0x7e};
const GUID IID_IMFVideoDisplayControl = {0xa490b1e4, 0xab84, 0x4d31, 0xa1, 0xb2, 0x18, 0x1e, 0x03, 0xb1, 0x07, 0x7a};
const GUID IID_IMFSimpleAudioVolume   = {0x089EDF13, 0xCF71, 0x4338, 0x8D, 0x13, 0x9E, 0x56, 0x9D, 0xBD, 0xC3, 0x19};
const GUID IID_IMFClock               = {0x2eb1e945, 0x18b8, 0x4139, 0x9b, 0x1a, 0xd5, 0xd5, 0x84, 0x81, 0x85, 0x30};
const GUID IID_IMFPresentationClock   = {0x868CE85C, 0x8EA9, 0x4f55, 0xAB, 0x82, 0xB0, 0x09, 0xA9, 0x10, 0xA8, 0x05};
const GUID IID_IMF2DBuffer2           = {0x33ae5ea6, 0x4316, 0x436f, 0x8d, 0xdd, 0xd7, 0x3d, 0x22, 0xf8, 0x29, 0xec};
const GUID IID_IMF2DBuffer            = {0x7DC9D5F9, 0x9ED9, 0x44ec, 0x9B, 0xBF, 0x06, 0x00, 0xBB, 0x58, 0x9F, 0xBB};
const GUID IID_IMFMediaSource         = {0x279a808d, 0xaec7, 0x40c8, 0x9c, 0x6b, 0xa6, 0xb4, 0x92, 0xc7, 0x8a, 0x66};
const GUID IID_IMFSourceReaderCallback= {0xdeec8d99, 0xfa1d, 0x4d82, 0x84, 0xc2, 0x2c, 0x89, 0x69, 0x94, 0x48, 0x67};

const GUID MFMediaType_Audio = {0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
const GUID MFMediaType_Video = {0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
const GUID MFMediaType_Image = {0x72178C23, 0xE45B, 0x11D5, 0xBC, 0x2A, 0x00, 0xB0, 0xD0, 0xF3, 0xF4, 0xAB};
const GUID MFMediaType_HTML = {0x72178C24, 0xE45B, 0x11D5, 0xBC, 0x2A, 0x00, 0xB0, 0xD0, 0xF3, 0xF4, 0xAB};
const GUID MFMediaType_Binary = {0x72178C25, 0xE45B, 0x11D5, 0xBC, 0x2A, 0x00, 0xB0, 0xD0, 0xF3, 0xF4, 0xAB};
const GUID MFMediaType_Stream = {0xe436eb83, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70};
const GUID MFMediaType_Subtitle = {0xa6d13581, 0xed50, 0x4e65, 0xae, 0x08, 0x26, 0x06, 0x55, 0x76, 0xaa, 0xcc};
const GUID MR_POLICY_VOLUME_SERVICE = {0x1abaa2ac, 0x9d3b, 0x47c6, 0xab, 0x48, 0xc5, 0x95, 0x6, 0xde, 0x78, 0x4d};

const GUID MF_TOPONODE_SOURCE                  = {0x835c58ec, 0xe075, 0x4bc7, 0xbc, 0xba, 0x4d, 0xe0, 0x00, 0xdf, 0x9a, 0xe6};
const GUID MF_TOPONODE_PRESENTATION_DESCRIPTOR = {0x835c58ed, 0xe075, 0x4bc7, 0xbc, 0xba, 0x4d, 0xe0, 0x00, 0xdf, 0x9a, 0xe6};
const GUID MF_TOPONODE_STREAM_DESCRIPTOR       = {0x835c58ee, 0xe075, 0x4bc7, 0xbc, 0xba, 0x4d, 0xe0, 0x00, 0xdf, 0x9a, 0xe6};
const GUID MF_TOPONODE_STREAMID                = {0x14932f9b, 0x9087, 0x4bb4, 0x84, 0x12, 0x51, 0x67, 0x14, 0x5c, 0xbe, 0x04};
const GUID MF_TOPONODE_NOSHUTDOWN_ON_REMOVE    = {0x14932f9c, 0x9087, 0x4bb4, 0x84, 0x12, 0x51, 0x67, 0x14, 0x5c, 0xbe, 0x04};
const GUID MF_PD_DURATION                      = {0x6c990d33, 0xbb8e, 0x477a, 0x85, 0x98, 0x0d, 0x5d, 0x96, 0xfc, 0xd8, 0x8a};

const GUID MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE             = {0xc60ac5fe, 0x252a, 0x478f, 0xa0, 0xef, 0xbc, 0x8f, 0xa5, 0xf7, 0xca, 0xd3};
const GUID MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID = {0x8ac3587a, 0x4ae7, 0x42d8, 0x99, 0xe0, 0x0a, 0x60, 0x13, 0xee, 0xf9, 0x0f};
const GUID MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME           = {0x60D0E559, 0x52F8, 0x4FA2, 0xBB, 0xCE, 0xAC, 0xDB, 0x34, 0xA8, 0xEC, 0x01};

const GUID MF_SOURCE_READER_ASYNC_CALLBACK                = {0x1e3dbeac, 0xbb43, 0x4c35, 0xb5, 0x07, 0xcd, 0x64, 0x44, 0x64, 0xc9, 0x65};

const GUID MF_MT_MAJOR_TYPE = {0x48eba18e, 0xf8c9, 0x4687, 0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xf9, 0x6a, 0x8f};
const GUID MF_MT_SUBTYPE    = {0xf7e34c9a, 0x42e8, 0x4714, 0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5};
const GUID MF_MT_FRAME_SIZE = {0x1652c33d, 0xd6b2, 0x4012, 0xb8, 0x34, 0x72, 0x03, 0x08, 0x49, 0xa3, 0x7d};

const GUID MFVideoFormat_RGB32 = { 22, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};

const GUID MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING = {0xfb394f3d, 0xccf1, 0x42ee, 0xbb, 0xb3, 0xf9, 0xb8, 0x45, 0xd5, 0x68, 0x1d};
const GUID MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING = {0xf81da2c, 0xb537, 0x4672, 0xa8, 0xb2, 0xa6, 0x81, 0xb1, 0x73, 0x7, 0xa3};
const GUID MF_MT_DEFAULT_STRIDE     = {0x644b4e48, 0x1e02, 0x4516, 0xb0, 0xeb, 0xc0, 0x1c, 0xa9, 0xd4, 0x9a, 0xc6};
const GUID MF_MT_PAD_CONTROL_FLAGS  = {0x4d0e73e5, 0x80ea, 0x4354, 0xa9, 0xd0, 0x11, 0x76, 0xce, 0xb0, 0x28, 0xea};
const GUID MF_MT_PIXEL_ASPECT_RATIO = {0xc6376a1e, 0x8d0a, 0x4027, 0xbe, 0x45, 0x6d, 0x9a, 0x0a, 0xd3, 0x9b, 0xb6};

//============================================================================================

typedef [callback] HRESULT FSetUnknown (LPVOID * This, REFGUID guidKey, IUnknown *pUnknown);
typedef [callback] HRESULT FSetUINT32 (LPVOID * This, REFGUID guidKey, UINT32 unValue);
typedef [callback] HRESULT FSetGUID (LPVOID * This, REFGUID guidKey, REFGUID guidValue);

struct IMFAttributesVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  LPVOID GetItem; //  HRESULT GetItem (IMFAttributes * This, REFGUID guidKey, PROPVARIANT *pValue);
  LPVOID GetItemType; //  HRESULT GetItemType (IMFAttributes * This, REFGUID guidKey, MF_ATTRIBUTE_TYPE *pType);
  LPVOID CompareItem; //  HRESULT CompareItem (IMFAttributes * This, REFGUID guidKey, REFPROPVARIANT Value, BOOL *pbResult);
  LPVOID Compare; //  HRESULT Compare (IMFAttributes * This, IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL *pbResult);
  LPVOID GetUINT32; //  HRESULT GetUINT32 (IMFAttributes * This, REFGUID guidKey, UINT32 *punValue);
  LPVOID GetUINT64; //  HRESULT GetUINT64 (IMFAttributes * This, REFGUID guidKey, UINT64 *punValue);
  LPVOID GetDouble; //  HRESULT GetDouble (IMFAttributes * This, REFGUID guidKey, double *pfValue);
  LPVOID GetGUID; //  HRESULT GetGUID (IMFAttributes * This, REFGUID guidKey, GUID *pguidValue);
  LPVOID GetStringLength; //  HRESULT GetStringLength (IMFAttributes * This, REFGUID guidKey, UINT32 *pcchLength);
  LPVOID GetString; //  HRESULT GetString (IMFAttributes * This, REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32 *pcchLength);
  LPVOID GetAllocatedString; //  HRESULT GetAllocatedString (IMFAttributes * This, REFGUID guidKey, LPWSTR *ppwszValue, UINT32 *pcchLength);
  LPVOID GetBlobSize; //  HRESULT GetBlobSize (IMFAttributes * This, REFGUID guidKey, UINT32 *pcbBlobSize);
  LPVOID GetBlob; //  HRESULT GetBlob (IMFAttributes * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize, UINT32 *pcbBlobSize);
  LPVOID GetAllocatedBlob; //  HRESULT GetAllocatedBlob (IMFAttributes * This, REFGUID guidKey, UINT8 **ppBuf, UINT32 *pcbSize);
  LPVOID GetUnknown; //  HRESULT GetUnknown (IMFAttributes * This, REFGUID guidKey, REFIID riid, LPVOID *ppv);
  LPVOID SetItem; //  HRESULT SetItem (IMFAttributes * This, REFGUID guidKey, REFPROPVARIANT Value);
  LPVOID DeleteItem; //  HRESULT DeleteItem (IMFAttributes * This, REFGUID guidKey);
  LPVOID DeleteAllItems; //  HRESULT DeleteAllItems (IMFAttributes * This);
  FSetUINT32 SetUINT32; //  HRESULT SetUINT32 (IMFAttributes * This, REFGUID guidKey, UINT32 unValue);
  LPVOID SetUINT64; //  HRESULT SetUINT64 (IMFAttributes * This, REFGUID guidKey, UINT64 unValue);
  LPVOID SetDouble; //  HRESULT SetDouble (IMFAttributes * This, REFGUID guidKey, double fValue);
  FSetGUID SetGUID; //  HRESULT SetGUID (IMFAttributes * This, REFGUID guidKey, REFGUID guidValue);
  LPVOID SetString; //  HRESULT SetString (IMFAttributes * This, REFGUID guidKey, LPCWSTR wszValue);
  LPVOID SetBlob; //  HRESULT SetBlob (IMFAttributes * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize);
  FSetUnknown SetUnknown; //  HRESULT SetUnknown (IMFAttributes * This, REFGUID guidKey, IUnknown *pUnknown);
  LPVOID LockStore; //  HRESULT LockStore (IMFAttributes * This);
  LPVOID UnlockStore; //  HRESULT UnlockStore (IMFAttributes * This);
  LPVOID GetCount; //  HRESULT GetCount (IMFAttributes * This, UINT32 *pcItems);
  LPVOID GetItemByIndex; //  HRESULT GetItemByIndex (IMFAttributes * This, UINT32 unIndex, GUID *pguidKey,  PROPVARIANT *pValue);
  LPVOID CopyAllItems; //  HRESULT CopyAllItems (IMFAttributes * This, IMFAttributes *pDest);
}

struct IMFAttributes
{
  IMFAttributesVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FGetItem (LPVOID * This, REFGUID guidKey, PROPVARIANT *pValue);
typedef [callback] HRESULT FGetType  (LPVOID * This, MediaEventType *pmet);
typedef [callback] HRESULT FGetUINT32 (LPVOID * This, REFGUID guidKey, UINT32 *punValue);
typedef [callback] HRESULT FGetValue (LPVOID * This, PROPVARIANT *pvValue);
typedef [callback] HRESULT FGetStatus (LPVOID * This, HRESULT *phrStatus);

struct IMFMediaEventVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FGetItem GetItem;
  LPVOID  GetItemType; // HRESULT GetItemType (LPVOID * This, REFGUID guidKey, MF_ATTRIBUTE_TYPE *pType);
  LPVOID  CompareItem; // HRESULT CompareItem (LPVOID * This, REFGUID guidKey, REFPROPVARIANT Value, BOOL *pbResult);
  LPVOID  Compare; // HRESULT Compare (LPVOID * This, IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL *pbResult);
  FGetUINT32 GetUINT32;
  LPVOID  GetUINT64; // HRESULT GetUINT64 (LPVOID * This, REFGUID guidKey, UINT64 *punValue);
  LPVOID  GetDouble; // HRESULT GetDouble (LPVOID * This, REFGUID guidKey, double *pfValue);
  LPVOID  GetGUID; //HRESULT GetGUID (LPVOID * This, REFGUID guidKey, GUID *pguidValue);
  LPVOID  GetStringLength; //HRESULT GetStringLength (LPVOID * This, REFGUID guidKey, UINT32 *pcchLength);
  LPVOID  GetString; //HRESULT GetString (LPVOID * This, REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32 *pcchLength);
  LPVOID  GetAllocatedString; //HRESULT GetAllocatedString (LPVOID * This, REFGUID guidKey, LPWSTR *ppwszValue, UINT32 *pcchLength);
  LPVOID  GetBlobSize; //HRESULT GetBlobSize (LPVOID * This, REFGUID guidKey, UINT32 *pcbBlobSize);
  LPVOID  GetBlob; //HRESULT GetBlob (LPVOID * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize, UINT32 *pcbBlobSize);
  LPVOID  GetAllocatedBlob; //HRESULT GetAllocatedBlob (LPVOID * This, REFGUID guidKey, UINT8 **ppBuf, UINT32 *pcbSize);
  LPVOID  GetUnknown; //HRESULT GetUnknown (LPVOID * This, REFGUID guidKey, REFIID riid, LPVOID *ppv);
  LPVOID  SetItem; //HRESULT SetItem (LPVOID * This, REFGUID guidKey, REFPROPVARIANT Value);
  LPVOID  DeleteItem; //HRESULT DeleteItem (LPVOID * This, REFGUID guidKey);
  LPVOID  DeleteAllItems; //HRESULT DeleteAllItems (LPVOID * This);
  LPVOID  SetUINT32; //HRESULT SetUINT32 (LPVOID * This, REFGUID guidKey, UINT32 unValue);
  LPVOID  SetUINT64; //HRESULT SetUINT64 (LPVOID * This, REFGUID guidKey, UINT64 unValue);
  LPVOID  SetDouble; //HRESULT SetDouble (LPVOID * This, REFGUID guidKey, double fValue);
  LPVOID  SetGUID; //HRESULT SetGUID (LPVOID * This, REFGUID guidKey, REFGUID guidValue);
  LPVOID  SetString; //HRESULT SetString (LPVOID * This, REFGUID guidKey, LPCWSTR wszValue);
  LPVOID  SetBlob; //HRESULT SetBlob (LPVOID * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize);
  LPVOID  SetUnknown; //HRESULT SetUnknown (LPVOID * This, REFGUID guidKey, IUnknown *pUnknown);
  LPVOID  LockStore; //HRESULT LockStore (LPVOID * This);
  LPVOID  UnlockStore; //HRESULT UnlockStore (LPVOID * This);
  LPVOID  GetCount; //HRESULT GetCount (LPVOID * This, UINT32 *pcItems);
  LPVOID  GetItemByIndex; //HRESULT GetItemByIndex (LPVOID * This, UINT32 unIndex, GUID *pguidKey, PROPVARIANT *pValue);
  LPVOID  CopyAllItems; //HRESULT CopyAllItems (LPVOID * This, IMFAttributes *pDest);
  FGetType GetType;
  LPVOID  GetExtendedType; //HRESULT GetExtendedType (LPVOID * This, GUID *pguidExtendedType);
  FGetStatus  GetStatus;
  FGetValue  GetValue;
}

struct IMFMediaEvent
{
  IMFMediaEventVtbl *lpVtbl;
}

//============================================================================================

struct IMFAsyncResultVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
/*
  LPVOID LPVOID; //  HRESULT ( STDMETHODCALLTYPE *GetState )(IMFAsyncResult * This, IUnknown **ppunkState);
  LPVOID LPVOID; //  HRESULT ( STDMETHODCALLTYPE *GetStatus )(IMFAsyncResult * This);
  LPVOID LPVOID; //  HRESULT ( STDMETHODCALLTYPE *SetStatus )(IMFAsyncResult * This, HRESULT hrStatus);
  LPVOID LPVOID; //  HRESULT ( STDMETHODCALLTYPE *GetObject )(IMFAsyncResult * This, IUnknown **ppObject);
  LPVOID LPVOID; //  IUnknown *( STDMETHODCALLTYPE *GetStateNoAddRef )(IMFAsyncResult * This);
*/
}

struct IMFAsyncResult
{
  IMFAsyncResultVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FGetParameters (LPVOID * This, DWORD *pdwFlags, DWORD *pdwQueue);
typedef [callback] HRESULT FInvoke        (LPVOID * This, IMFAsyncResult *pAsyncResult);

struct IMFAsyncCallbackVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;
  FGetParameters    GetParameters;
  FInvoke           Invoke;
}

struct IMFAsyncCallback
{
  IMFAsyncCallbackVtbl *lpVtbl;
}

//============================================================================================

typedef IMFTopologyNode;

typedef [callback] HRESULT FSetUnknownTopology (LPVOID * This, REFGUID guidKey, IUnknown *pUnknown);
typedef [callback] HRESULT FSetObject  (LPVOID * This, IUnknown *pObject);
typedef [callback] HRESULT FSetUINT32Topology  (LPVOID * This, REFGUID guidKey, UINT32 unValue);
typedef [callback] HRESULT FConnectOutput (LPVOID * This, DWORD dwOutputIndex, IMFTopologyNode *pDownstreamNode, DWORD dwInputIndexOnDownstreamNode);

struct IMFTopologyNodeVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  LPVOID GetItem;  //   HRESULT GetItem (IMFTopologyNode * This, REFGUID guidKey, PROPVARIANT *pValue);
  LPVOID GetItemType;  //   HRESULT GetItemType (IMFTopologyNode * This, REFGUID guidKey, MF_ATTRIBUTE_TYPE *pType);
  LPVOID CompareItem;  //   HRESULT CompareItem (IMFTopologyNode * This, REFGUID guidKey, REFPROPVARIANT Value, BOOL *pbResult);
  LPVOID Compare;  //   HRESULT Compare (IMFTopologyNode * This, IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL *pbResult);
  LPVOID GetUINT32;  //   HRESULT GetUINT32 (IMFTopologyNode * This, REFGUID guidKey, UINT32 *punValue);
  LPVOID GetUINT64;  //   HRESULT GetUINT64 (IMFTopologyNode * This, REFGUID guidKey, UINT64 *punValue);
  LPVOID GetDouble;  //   HRESULT GetDouble (IMFTopologyNode * This, REFGUID guidKey, double *pfValue);
  LPVOID GetGUID;  //   HRESULT GetGUID (IMFTopologyNode * This, REFGUID guidKey, GUID *pguidValue);
  LPVOID GetStringLength;  //   HRESULT GetStringLength (IMFTopologyNode * This, REFGUID guidKey, UINT32 *pcchLength);
  LPVOID GetString;  //   HRESULT GetString (IMFTopologyNode * This, REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32 *pcchLength);
  LPVOID GetAllocatedString;  //   HRESULT GetAllocatedString (IMFTopologyNode * This, REFGUID guidKey, LPWSTR *ppwszValue, UINT32 *pcchLength);
  LPVOID GetBlobSize;  //   HRESULT GetBlobSize (IMFTopologyNode * This, REFGUID guidKey, UINT32 *pcbBlobSize);
  LPVOID GetBlob;  //   HRESULT GetBlob (IMFTopologyNode * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize, UINT32 *pcbBlobSize);
  LPVOID GetAllocatedBlob;  //   HRESULT GetAllocatedBlob (IMFTopologyNode * This, REFGUID guidKey, UINT8 **ppBuf, UINT32 *pcbSize);
  LPVOID GetUnknown;  //   HRESULT GetUnknown (IMFTopologyNode * This, REFGUID guidKey, REFIID riid, LPVOID *ppv);
  LPVOID SetItem;  //   HRESULT SetItem (IMFTopologyNode * This, REFGUID guidKey, REFPROPVARIANT Value);
  LPVOID DeleteItem;  //   HRESULT DeleteItem (IMFTopologyNode * This, REFGUID guidKey);
  LPVOID DeleteAllItems;  //   HRESULT DeleteAllItems (IMFTopologyNode * This);
  FSetUINT32Topology SetUINT32;
  LPVOID SetUINT64;  //   HRESULT SetUINT64 (IMFTopologyNode * This, REFGUID guidKey, UINT64 unValue);
  LPVOID SetDouble;  //   HRESULT SetDouble (IMFTopologyNode * This, REFGUID guidKey, double fValue);
  LPVOID SetGUID;  //   HRESULT SetGUID (IMFTopologyNode * This, REFGUID guidKey, REFGUID guidValue);
  LPVOID SetString;  //   HRESULT SetString (IMFTopologyNode * This, REFGUID guidKey, LPCWSTR wszValue);
  LPVOID SetBlob;  //   HRESULT SetBlob (IMFTopologyNode * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize);
  FSetUnknownTopology SetUnknown;
  LPVOID LockStore;  //   HRESULT LockStore (IMFTopologyNode * This);
  LPVOID UnlockStore;  //   HRESULT UnlockStore (IMFTopologyNode * This);
  LPVOID GetCount;  //   HRESULT GetCount (IMFTopologyNode * This, UINT32 *pcItems);
  LPVOID GetItemByIndex;  //   HRESULT GetItemByIndex (IMFTopologyNode * This, UINT32 unIndex, GUID *pguidKey, PROPVARIANT *pValue);
  LPVOID CopyAllItems;  //   HRESULT CopyAllItems (IMFTopologyNode * This, IMFAttributes *pDest);
  FSetObject SetObject;
  LPVOID GetObject;  //   HRESULT GetObject (IMFTopologyNode * This, IUnknown **ppObject);
  LPVOID GetNodeType;  //   HRESULT GetNodeType (IMFTopologyNode * This, MF_TOPOLOGY_TYPE *pType);
  LPVOID GetTopoNodeID;  //   HRESULT GetTopoNodeID (IMFTopologyNode * This, TOPOID *pID);
  LPVOID SetTopoNodeID;  //   HRESULT SetTopoNodeID (IMFTopologyNode * This, TOPOID ullTopoID);
  LPVOID GetInputCount;  //   HRESULT GetInputCount (IMFTopologyNode * This, DWORD *pcInputs);
  LPVOID GetOutputCount;  //   HRESULT GetOutputCount (IMFTopologyNode * This, DWORD *pcOutputs);
  FConnectOutput ConnectOutput;
  LPVOID DisconnectOutput;  //   HRESULT DisconnectOutput (IMFTopologyNode * This, DWORD dwOutputIndex);
  LPVOID GetInput;  //   HRESULT GetInput (IMFTopologyNode * This, DWORD dwInputIndex, IMFTopologyNode **ppUpstreamNode, DWORD *pdwOutputIndexOnUpstreamNode);
  LPVOID GetOutput;  //   HRESULT GetOutput (IMFTopologyNode * This, DWORD dwOutputIndex, IMFTopologyNode **ppDownstreamNode, DWORD *pdwInputIndexOnDownstreamNode);
  LPVOID SetOutputPrefType;  //   HRESULT SetOutputPrefType (IMFTopologyNode * This, DWORD dwOutputIndex, IMFMediaType *pType);
  LPVOID GetOutputPrefType;  //   HRESULT GetOutputPrefType (IMFTopologyNode * This, DWORD dwOutputIndex, IMFMediaType **ppType);
  LPVOID SetInputPrefType;  //   HRESULT SetInputPrefType (IMFTopologyNode * This, DWORD dwInputIndex, IMFMediaType *pType);
  LPVOID GetInputPrefType;  //   HRESULT GetInputPrefType (IMFTopologyNode * This, DWORD dwInputIndex, IMFMediaType **ppType);
  LPVOID CloneFrom;  //   HRESULT CloneFrom (IMFTopologyNode * This, IMFTopologyNode *pNode);
}

struct IMFTopologyNode
{
  IMFTopologyNodeVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FAddNode (LPVOID * This, IMFTopologyNode *pNode);

struct IMFTopologyVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  LPVOID GetItem; //   HRESULT GetItem (IMFTopology * This, REFGUID guidKey, PROPVARIANT *pValue);
  LPVOID GetItemType; //   HRESULT GetItemType (IMFTopology * This, REFGUID guidKey, MF_ATTRIBUTE_TYPE *pType);
  LPVOID CompareItem; //   HRESULT CompareItem (IMFTopology * This, REFGUID guidKey, REFPROPVARIANT Value, BOOL *pbResult);
  LPVOID Compare; //   HRESULT Compare (IMFTopology * This, IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL *pbResult);
  LPVOID GetUINT32; //   HRESULT GetUINT32 (IMFTopology * This, REFGUID guidKey, UINT32 *punValue);
  LPVOID GetUINT64; //   HRESULT GetUINT64 (IMFTopology * This, REFGUID guidKey, UINT64 *punValue);
  LPVOID GetDouble; //   HRESULT GetDouble (IMFTopology * This, REFGUID guidKey, double *pfValue);
  LPVOID GetGUID; //   HRESULT GetGUID (IMFTopology * This, REFGUID guidKey, GUID *pguidValue);
  LPVOID GetStringLength; //   HRESULT GetStringLength (IMFTopology * This, REFGUID guidKey, UINT32 *pcchLength);
  LPVOID GetString; //   HRESULT GetString (IMFTopology * This, REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32 *pcchLength);
  LPVOID GetAllocatedString; //   HRESULT GetAllocatedString (IMFTopology * This, REFGUID guidKey, LPWSTR *ppwszValue, UINT32 *pcchLength);
  LPVOID GetBlobSize; //   HRESULT GetBlobSize (IMFTopology * This, REFGUID guidKey, UINT32 *pcbBlobSize);
  LPVOID GetBlob; //   HRESULT GetBlob (IMFTopology * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize, UINT32 *pcbBlobSize);
  LPVOID GetAllocatedBlob; //   HRESULT GetAllocatedBlob (IMFTopology * This, REFGUID guidKey, UINT8 **ppBuf, UINT32 *pcbSize);
  LPVOID GetUnknown; //   HRESULT GetUnknown (IMFTopology * This, REFGUID guidKey, REFIID riid, LPVOID *ppv);
  LPVOID SetItem; //   HRESULT SetItem (IMFTopology * This, REFGUID guidKey, REFPROPVARIANT Value);
  LPVOID DeleteItem; //   HRESULT DeleteItem (IMFTopology * This, REFGUID guidKey);
  LPVOID DeleteAllItems; //   HRESULT DeleteAllItems (IMFTopology * This);
  LPVOID SetUINT32; //   HRESULT SetUINT32 (IMFTopology * This, REFGUID guidKey, UINT32 unValue);
  LPVOID SetUINT64; //   HRESULT SetUINT64 (IMFTopology * This, REFGUID guidKey, UINT64 unValue);
  LPVOID SetDouble; //   HRESULT SetDouble (IMFTopology * This, REFGUID guidKey, double fValue);
  LPVOID SetGUID; //   HRESULT SetGUID (IMFTopology * This, REFGUID guidKey, REFGUID guidValue);
  LPVOID SetString; //   HRESULT SetString (IMFTopology * This, REFGUID guidKey, LPCWSTR wszValue);
  LPVOID SetBlob; //   HRESULT SetBlob (IMFTopology * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize);
  LPVOID SetUnknown; //   HRESULT SetUnknown (IMFTopology * This, REFGUID guidKey, IUnknown *pUnknown);
  LPVOID LockStore; //   HRESULT LockStore (IMFTopology * This);
  LPVOID UnlockStore; //   HRESULT UnlockStore (IMFTopology * This);
  LPVOID GetCount; //   HRESULT GetCount (IMFTopology * This, UINT32 *pcItems);
  LPVOID GetItemByIndex; //   HRESULT GetItemByIndex (IMFTopology * This, UINT32 unIndex, GUID *pguidKey, PROPVARIANT *pValue);
  LPVOID CopyAllItems; //   HRESULT CopyAllItems (IMFTopology * This, IMFAttributes *pDest);
  LPVOID GetTopologyID; //   HRESULT GetTopologyID (IMFTopology * This, TOPOID *pID);
  FAddNode AddNode;
  LPVOID RemoveNode; //   HRESULT RemoveNode (IMFTopology * This, IMFTopologyNode *pNode);
  LPVOID GetNodeCount; //   HRESULT GetNodeCount (IMFTopology * This, WORD *pwNodes);
  LPVOID GetNode; //   HRESULT GetNode (IMFTopology * This, WORD wIndex, IMFTopologyNode **ppNode);
  LPVOID Clear; //   HRESULT Clear (IMFTopology * This);
  LPVOID CloneFrom; //   HRESULT CloneFrom (IMFTopology * This, IMFTopology *pTopology);
  LPVOID GetNodeByID; //   HRESULT GetNodeByID (IMFTopology * This, TOPOID qwTopoNodeID, IMFTopologyNode **ppNode);
  LPVOID GetSourceNodeCollection; //   HRESULT GetSourceNodeCollection (IMFTopology * This, IMFCollection **ppCollection);
  LPVOID GetOutputNodeCollection; //   HRESULT GetOutputNodeCollection (IMFTopology * This, IMFCollection **ppCollection);
}

struct IMFTopology
{
  IMFTopologyVtbl *lpVtbl;
}

//============================================================================================

struct IMFClockVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
/*
  HRESULT ( STDMETHODCALLTYPE *GetClockCharacteristics )(IMFClock * This, DWORD *pdwCharacteristics);
  HRESULT ( STDMETHODCALLTYPE *GetCorrelatedTime )(IMFClock * This, DWORD dwReserved, LONGLONG *pllClockTime, MFTIME *phnsSystemTime);
  HRESULT ( STDMETHODCALLTYPE *GetContinuityKey )(IMFClock * This, DWORD *pdwContinuityKey);
  HRESULT ( STDMETHODCALLTYPE *GetState )(IMFClock * This, DWORD dwReserved, MFCLOCK_STATE *peClockState);
  HRESULT ( STDMETHODCALLTYPE *GetProperties )(IMFClock * This, MFCLOCK_PROPERTIES *pClockProperties);
*/
}

struct IMFClock
{
  IMFClockVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FGetTime (LPVOID * This, MFTIME *phnsClockTime);

struct IMFPresentationClockVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  LPVOID GetClockCharacteristics; // HRESULT ( STDMETHODCALLTYPE *GetClockCharacteristics )(IMFPresentationClock * This, DWORD *pdwCharacteristics);
  LPVOID GetCorrelatedTime ; //  HRESULT ( STDMETHODCALLTYPE *GetCorrelatedTime )(IMFPresentationClock * This, DWORD dwReserved, LONGLONG *pllClockTime, MFTIME *phnsSystemTime);
  LPVOID GetContinuityKey ; //  HRESULT ( STDMETHODCALLTYPE *GetContinuityKey )(IMFPresentationClock * This, DWORD *pdwContinuityKey);
  LPVOID GetState ; //  HRESULT ( STDMETHODCALLTYPE *GetState )(IMFPresentationClock * This, DWORD dwReserved, MFCLOCK_STATE *peClockState);
  LPVOID GetProperties ; //  HRESULT ( STDMETHODCALLTYPE *GetProperties )(IMFPresentationClock * This, MFCLOCK_PROPERTIES *pClockProperties);
  LPVOID SetTimeSource ; //  HRESULT ( STDMETHODCALLTYPE *SetTimeSource )(IMFPresentationClock * This, IMFPresentationTimeSource *pTimeSource);
  LPVOID GetTimeSource ; //  HRESULT ( STDMETHODCALLTYPE *GetTimeSource )(IMFPresentationClock * This, IMFPresentationTimeSource **ppTimeSource);
  FGetTime GetTime ; //  HRESULT ( STDMETHODCALLTYPE *GetTime )(IMFPresentationClock * This, MFTIME *phnsClockTime);
  LPVOID AddClockStateSink ; //  HRESULT ( STDMETHODCALLTYPE *AddClockStateSink )(IMFPresentationClock * This, IMFClockStateSink *pStateSink);
  LPVOID RemoveClockStateSink ; //  HRESULT ( STDMETHODCALLTYPE *RemoveClockStateSink )(IMFPresentationClock * This, IMFClockStateSink *pStateSink);
  LPVOID Start ; //  HRESULT ( STDMETHODCALLTYPE *Start )(IMFPresentationClock * This, LONGLONG llClockStartOffset);
  LPVOID Stop ; //  HRESULT ( STDMETHODCALLTYPE *Stop )(IMFPresentationClock * This);
  LPVOID Pause ; //  HRESULT ( STDMETHODCALLTYPE *Pause )(IMFPresentationClock * This);
}

struct IMFPresentationClock
{
  IMFPresentationClockVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FGetEvent        (LPVOID * This, DWORD dwFlags, IMFMediaEvent **ppEvent);
typedef [callback] HRESULT FBeginGetEvent   (LPVOID * This, IMFAsyncCallback *pCallback, IUnknown *punkState);
typedef [callback] HRESULT FEndGetEvent     (LPVOID * This, IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
typedef [callback] HRESULT FQueueEvent      (LPVOID * This, MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, PROPVARIANT *pvValue);
typedef [callback] HRESULT FSetTopology     (LPVOID * This, DWORD dwSetTopologyFlags, IMFTopology *pTopology);
typedef [callback] HRESULT FClearTopologies (LPVOID * This);
typedef [callback] HRESULT FStart           (LPVOID * This, GUID *pguidTimeFormat, PROPVARIANT *pvarStartPosition);
typedef [callback] HRESULT FPause           (LPVOID * This);
typedef [callback] HRESULT FStop            (LPVOID * This);
typedef [callback] HRESULT FClose           (LPVOID * This);
typedef [callback] HRESULT FShutdown        (LPVOID * This);
typedef [callback] HRESULT FGetClock        (LPVOID * This, IMFClock **ppClock);

struct IMFMediaSessionVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FGetEvent      GetEvent;
  FBeginGetEvent BeginGetEvent;
  FEndGetEvent   EndGetEvent;
  FQueueEvent    QueueEvent;
  FSetTopology   SetTopology;
  FClearTopologies ClearTopologies;
  FStart         Start;
  FPause         Pause;
  FStop          Stop;
  FClose         Close;
  FShutdown      Shutdown;
  FGetClock      GetClock;
//  LPVOID LPVOID; //  HRESULT GetSessionCapabilities (IMFMediaSession * This, DWORD *pdwCaps);
//  LPVOID LPVOID; //  HRESULT GetFullTopology (IMFMediaSession * This, DWORD dwGetFullTopologyFlags, TOPOID TopoId, IMFTopology **ppFullTopology);
}

struct IMFMediaSession
{
  IMFMediaSessionVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FGetMajorType (LPVOID * This, GUID *pguidMajorType);

struct IMFMediaTypeHandlerVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  LPVOID IsMediaTypeSupported; //  HRESULT IsMediaTypeSupported (IMFMediaTypeHandler * This, IMFMediaType *pMediaType, IMFMediaType **ppMediaType);
  LPVOID GetMediaTypeCount;    //  HRESULT GetMediaTypeCount (IMFMediaTypeHandler * This, DWORD *pdwTypeCount);
  LPVOID GetMediaTypeByIndex;  //  HRESULT GetMediaTypeByIndex (IMFMediaTypeHandler * This, DWORD dwIndex, IMFMediaType **ppType);
  LPVOID SetCurrentMediaType;  //  HRESULT SetCurrentMediaType (IMFMediaTypeHandler * This, IMFMediaType *pMediaType);
  LPVOID GetCurrentMediaType;  //  HRESULT GetCurrentMediaType (IMFMediaTypeHandler * This, IMFMediaType **ppMediaType);
  FGetMajorType GetMajorType;
}

struct IMFMediaTypeHandler
{
  IMFMediaTypeHandlerVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FGetMediaTypeHandler (LPVOID * This, IMFMediaTypeHandler **ppMediaTypeHandler);

struct IMFStreamDescriptorVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  LPVOID GetItem; //  HRESULT GetItem (IMFStreamDescriptor * This, REFGUID guidKey, PROPVARIANT *pValue);
  LPVOID GetItemType; //  HRESULT GetItemType (IMFStreamDescriptor * This, REFGUID guidKey, MF_ATTRIBUTE_TYPE *pType);
  LPVOID CompareItem; //  HRESULT CompareItem (IMFStreamDescriptor * This, REFGUID guidKey, REFPROPVARIANT Value, BOOL *pbResult);
  LPVOID Compare; //  HRESULT Compare (IMFStreamDescriptor * This, IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL *pbResult);
  LPVOID GetUINT32; //  HRESULT GetUINT32 (IMFStreamDescriptor * This, REFGUID guidKey, UINT32 *punValue);
  LPVOID GetUINT64; //  HRESULT GetUINT64 (IMFStreamDescriptor * This, REFGUID guidKey, UINT64 *punValue);
  LPVOID GetDouble; //  HRESULT GetDouble (IMFStreamDescriptor * This, REFGUID guidKey, double *pfValue);
  LPVOID GetGUID; //  HRESULT GetGUID (IMFStreamDescriptor * This, REFGUID guidKey, GUID *pguidValue);
  LPVOID GetStringLength; //  HRESULT GetStringLength (IMFStreamDescriptor * This, REFGUID guidKey, UINT32 *pcchLength);
  LPVOID GetString; //  HRESULT GetString (IMFStreamDescriptor * This, REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32 *pcchLength);
  LPVOID GetAllocatedString; //  HRESULT GetAllocatedString (IMFStreamDescriptor * This, REFGUID guidKey, LPWSTR *ppwszValue, UINT32 *pcchLength);
  LPVOID GetBlobSize; //  HRESULT GetBlobSize (IMFStreamDescriptor * This, REFGUID guidKey, UINT32 *pcbBlobSize);
  LPVOID GetBlob; //  HRESULT GetBlob (IMFStreamDescriptor * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize, UINT32 *pcbBlobSize);
  LPVOID GetAllocatedBlob; //  HRESULT GetAllocatedBlob (IMFStreamDescriptor * This, REFGUID guidKey, UINT8 **ppBuf, UINT32 *pcbSize);
  LPVOID GetUnknown; //  HRESULT GetUnknown (IMFStreamDescriptor * This, REFGUID guidKey, REFIID riid, LPVOID *ppv);
  LPVOID SetItem; //  HRESULT SetItem (IMFStreamDescriptor * This, REFGUID guidKey, REFPROPVARIANT Value);
  LPVOID DeleteItem; //  HRESULT DeleteItem (IMFStreamDescriptor * This, REFGUID guidKey);
  LPVOID DeleteAllItems; //  HRESULT DeleteAllItems (IMFStreamDescriptor * This);
  LPVOID SetUINT32; //  HRESULT SetUINT32 (IMFStreamDescriptor * This, REFGUID guidKey, UINT32 unValue);
  LPVOID SetUINT64; //  HRESULT SetUINT64 (IMFStreamDescriptor * This, REFGUID guidKey, UINT64 unValue);
  LPVOID SetDouble; //  HRESULT SetDouble (IMFStreamDescriptor * This, REFGUID guidKey, double fValue);
  LPVOID SetGUID; //  HRESULT SetGUID (IMFStreamDescriptor * This, REFGUID guidKey, REFGUID guidValue);
  LPVOID SetString; //  HRESULT SetString (IMFStreamDescriptor * This, REFGUID guidKey, LPCWSTR wszValue);
  LPVOID SetBlob; //  HRESULT SetBlob (IMFStreamDescriptor * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize);
  LPVOID SetUnknown; //  HRESULT SetUnknown (IMFStreamDescriptor * This, REFGUID guidKey, IUnknown *pUnknown);
  LPVOID LockStore; //  HRESULT LockStore (IMFStreamDescriptor * This);
  LPVOID UnlockStore; //  HRESULT UnlockStore (IMFStreamDescriptor * This);
  LPVOID GetCount; //  HRESULT GetCount (IMFStreamDescriptor * This, UINT32 *pcItems);
  LPVOID GetItemByIndex; //  HRESULT GetItemByIndex (IMFStreamDescriptor * This, UINT32 unIndex, GUID *pguidKey, PROPVARIANT *pValue);
  LPVOID CopyAllItems; //  HRESULT CopyAllItems (IMFStreamDescriptor * This, IMFAttributes *pDest);
  LPVOID GetStreamIdentifier; //  HRESULT GetStreamIdentifier (IMFStreamDescriptor * This, DWORD *pdwStreamIdentifier);
  FGetMediaTypeHandler GetMediaTypeHandler; //  HRESULT GetMediaTypeHandler (IMFStreamDescriptor * This, IMFMediaTypeHandler **ppMediaTypeHandler);
}

struct IMFStreamDescriptor
{
  IMFStreamDescriptorVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FGetStreamDescriptorCount (LPVOID * This, DWORD *pdwDescriptorCount);
typedef [callback] HRESULT FGetStreamDescriptorByIndex (LPVOID * This, DWORD dwIndex, BOOL *pfSelected, IMFStreamDescriptor **ppDescriptor);
typedef [callback] HRESULT FGetUINT64 (LPVOID * This, REFGUID guidKey, UINT64 *punValue);

struct IMFPresentationDescriptorVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  LPVOID GetItem; // HRESULT GetItem (IMFPresentationDescriptor * This, REFGUID guidKey, PROPVARIANT *pValue);
  LPVOID GetItemType; //  HRESULT GetItemType (IMFPresentationDescriptor * This, REFGUID guidKey, MF_ATTRIBUTE_TYPE *pType);
  LPVOID CompareItem; //  HRESULT CompareItem (IMFPresentationDescriptor * This, REFGUID guidKey, REFPROPVARIANT Value, BOOL *pbResult);
  LPVOID Compare; //  HRESULT Compare (IMFPresentationDescriptor * This, IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL *pbResult);
  LPVOID GetUINT32; //  HRESULT GetUINT32 (IMFPresentationDescriptor * This, REFGUID guidKey, UINT32 *punValue);
  FGetUINT64 GetUINT64; //  HRESULT GetUINT64 (IMFPresentationDescriptor * This, REFGUID guidKey, UINT64 *punValue);
  LPVOID GetDouble; //  HRESULT GetDouble (IMFPresentationDescriptor * This, REFGUID guidKey, double *pfValue);
  LPVOID GetGUID; //  HRESULT GetGUID (IMFPresentationDescriptor * This, REFGUID guidKey, GUID *pguidValue);
  LPVOID GetStringLength; //  HRESULT GetStringLength (IMFPresentationDescriptor * This, REFGUID guidKey, UINT32 *pcchLength);
  LPVOID GetString; //  HRESULT GetString (IMFPresentationDescriptor * This, REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32 *pcchLength);
  LPVOID GetAllocatedString; //  HRESULT GetAllocatedString (IMFPresentationDescriptor * This, REFGUID guidKey, LPWSTR *ppwszValue, UINT32 *pcchLength);
  LPVOID GetBlobSize; //  HRESULT GetBlobSize (IMFPresentationDescriptor * This, REFGUID guidKey, UINT32 *pcbBlobSize);
  LPVOID GetBlob; //  HRESULT GetBlob (IMFPresentationDescriptor * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize, UINT32 *pcbBlobSize);
  LPVOID GetAllocatedBlob; //  HRESULT GetAllocatedBlob (IMFPresentationDescriptor * This, REFGUID guidKey, UINT8 **ppBuf, UINT32 *pcbSize);
  LPVOID GetUnknown; //  HRESULT GetUnknown (IMFPresentationDescriptor * This, REFGUID guidKey, REFIID riid, LPVOID *ppv);
  LPVOID SetItem; //  HRESULT SetItem (IMFPresentationDescriptor * This, REFGUID guidKey, REFPROPVARIANT Value);
  LPVOID DeleteItem; //  HRESULT DeleteItem (IMFPresentationDescriptor * This, REFGUID guidKey);
  LPVOID DeleteAllItems; //  HRESULT DeleteAllItems (IMFPresentationDescriptor * This);
  LPVOID SetUINT32; //  HRESULT SetUINT32 (IMFPresentationDescriptor * This, REFGUID guidKey, UINT32 unValue);
  LPVOID SetUINT64; //  HRESULT SetUINT64 (IMFPresentationDescriptor * This, REFGUID guidKey, UINT64 unValue);
  LPVOID SetDouble; //  HRESULT SetDouble (IMFPresentationDescriptor * This, REFGUID guidKey, double fValue);
  LPVOID SetGUID; //  HRESULT SetGUID (IMFPresentationDescriptor * This, REFGUID guidKey, REFGUID guidValue);
  LPVOID SetString; //  HRESULT SetString (IMFPresentationDescriptor * This, REFGUID guidKey, LPCWSTR wszValue);
  LPVOID SetBlob; //  HRESULT SetBlob (IMFPresentationDescriptor * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize);
  LPVOID SetUnknown; //  HRESULT SetUnknown (IMFPresentationDescriptor * This, REFGUID guidKey, IUnknown *pUnknown);
  LPVOID LockStore; //  HRESULT LockStore (IMFPresentationDescriptor * This);
  LPVOID UnlockStore; //  HRESULT UnlockStore (IMFPresentationDescriptor * This);
  LPVOID GetCount; //  HRESULT GetCount (IMFPresentationDescriptor * This, UINT32 *pcItems);
  LPVOID GetItemByIndex; //  HRESULT GetItemByIndex (IMFPresentationDescriptor * This, UINT32 unIndex, GUID *pguidKey, PROPVARIANT *pValue);
  LPVOID CopyAllItems; //  HRESULT CopyAllItems (IMFPresentationDescriptor * This, IMFAttributes *pDest);
  FGetStreamDescriptorCount GetStreamDescriptorCount; //  HRESULT GetStreamDescriptorCount (IMFPresentationDescriptor * This, DWORD *pdwDescriptorCount);
  FGetStreamDescriptorByIndex GetStreamDescriptorByIndex; //  HRESULT GetStreamDescriptorByIndex (IMFPresentationDescriptor * This, DWORD dwIndex, BOOL *pfSelected, IMFStreamDescriptor **ppDescriptor);
  LPVOID SelectStream; //  HRESULT SelectStream (IMFPresentationDescriptor * This, DWORD dwDescriptorIndex);
  LPVOID DeselectStream; //  HRESULT DeselectStream (IMFPresentationDescriptor * This, DWORD dwDescriptorIndex);
  LPVOID Clone; //  HRESULT Clone (IMFPresentationDescriptor * This, IMFPresentationDescriptor **ppPresentationDescriptor);
}

struct IMFPresentationDescriptor
{
  IMFPresentationDescriptorVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FCreatePresentationDescriptor (LPVOID * This, IMFPresentationDescriptor **ppPresentationDescriptor);
typedef [callback] HRESULT FShutdown2                    (LPVOID * This);

struct IMFMediaSourceVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  LPVOID GetEvent; //  HRESULT GetEvent (IMFMediaSource * This, DWORD dwFlags, IMFMediaEvent **ppEvent);
  LPVOID BeginGetEvent; //  HRESULT BeginGetEvent (IMFMediaSource * This, IMFAsyncCallback *pCallback, IUnknown *punkState);
  LPVOID EndGetEvent; //  HRESULT EndGetEvent (IMFMediaSource * This, IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
  LPVOID QueueEvent; //  HRESULT QueueEvent (IMFMediaSource * This, MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, PROPVARIANT *pvValue);
  LPVOID GetCharacteristics; //  HRESULT GetCharacteristics (IMFMediaSource * This, DWORD *pdwCharacteristics);
  FCreatePresentationDescriptor CreatePresentationDescriptor; //  HRESULT CreatePresentationDescriptor (IMFMediaSource * This, IMFPresentationDescriptor **ppPresentationDescriptor);
  LPVOID Start; //  HRESULT Start (IMFMediaSource * This, IMFPresentationDescriptor *pPresentationDescriptor, GUID *pguidTimeFormat, PROPVARIANT *pvarStartPosition);
  LPVOID Stop; //  HRESULT Stop (IMFMediaSource * This);
  LPVOID Pause; //  HRESULT Pause (IMFMediaSource * This);
  FShutdown2 Shutdown; //  HRESULT Shutdown (IMFMediaSource * This);
}

struct IMFMediaSource
{
  IMFMediaSourceVtbl *lpVtbl;
}

//============================================================================================

struct MFVideoNormalizedRect
{
  float left;
  float top;
  float right;
  float bottom;
}

typedef windows.RECT* LPRECT;

typedef [callback] HRESULT FSetVideoPosition (LPVOID * This, MFVideoNormalizedRect *pnrcSource, LPRECT prcDest);
typedef [callback] HRESULT FRepaintVideo (LPVOID * This);
typedef [callback] HRESULT FGetCurrentImage (LPVOID * This, BITMAPINFOHEADER *pBih, BYTE **pDib, DWORD *pcbDib, LONGLONG *pTimeStamp);

struct IMFVideoDisplayControlVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  LPVOID GetNativeVideoSize; //  HRESULT GetNativeVideoSize (IMFVideoDisplayControl * This, SIZE *pszVideo, SIZE *pszARVideo);
  LPVOID GetIdealVideoSize; //  HRESULT GetIdealVideoSize (IMFVideoDisplayControl * This, SIZE *pszMin, SIZE *pszMax);
  FSetVideoPosition SetVideoPosition; //  HRESULT SetVideoPosition (IMFVideoDisplayControl * This, MFVideoNormalizedRect *pnrcSource, LPRECT prcDest);
  LPVOID GetVideoPosition; //  HRESULT GetVideoPosition (IMFVideoDisplayControl * This, MFVideoNormalizedRect *pnrcSource, LPRECT prcDest);
  LPVOID SetAspectRatioMode; //  HRESULT SetAspectRatioMode (IMFVideoDisplayControl * This, DWORD dwAspectRatioMode);
  LPVOID GetAspectRatioMode; //  HRESULT GetAspectRatioMode (IMFVideoDisplayControl * This, DWORD *pdwAspectRatioMode);
  LPVOID SetVideoWindow; //  HRESULT SetVideoWindow (IMFVideoDisplayControl * This, HWND hwndVideo);
  LPVOID GetVideoWindow; //  HRESULT GetVideoWindow (IMFVideoDisplayControl * This, HWND *phwndVideo);
  FRepaintVideo RepaintVideo; //  HRESULT RepaintVideo (IMFVideoDisplayControl * This);
  FGetCurrentImage GetCurrentImage; //  HRESULT GetCurrentImage (IMFVideoDisplayControl * This, BITMAPINFOHEADER *pBih, BYTE **pDib, DWORD *pcbDib, LONGLONG *pTimeStamp);
  LPVOID SetBorderColor; //  HRESULT SetBorderColor (IMFVideoDisplayControl * This, COLORREF Clr);
  LPVOID GetBorderColor; //  HRESULT GetBorderColor (IMFVideoDisplayControl * This, COLORREF *pClr);
  LPVOID SetRenderingPrefs; //  HRESULT SetRenderingPrefs (IMFVideoDisplayControl * This, DWORD dwRenderFlags);
  LPVOID GetRenderingPrefs; //  HRESULT GetRenderingPrefs (IMFVideoDisplayControl * This, DWORD *pdwRenderFlags);
  LPVOID SetFullscreen; //  HRESULT SetFullscreen (IMFVideoDisplayControl * This, BOOL fFullscreen);
  LPVOID GetFullscreen; //  HRESULT GetFullscreen (IMFVideoDisplayControl * This, BOOL *pfFullscreen);
}

struct IMFVideoDisplayControl
{
  IMFVideoDisplayControlVtbl *lpVtbl;
}

//============================================================================================

typedef byte* IPropertyStore;
typedef [callback] HRESULT FCreateObjectFromURL (LPVOID * This, LPCWSTR pwszURL, DWORD dwFlags, IPropertyStore *pProps, MF_OBJECT_TYPE *pObjectType, IUnknown **ppObject);

struct IMFSourceResolverVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FCreateObjectFromURL CreateObjectFromURL;  //   HRESULT CreateObjectFromURL (IMFSourceResolver * This, LPCWSTR pwszURL, DWORD dwFlags, IPropertyStore *pProps, MF_OBJECT_TYPE *pObjectType, IUnknown **ppObject);
  LPVOID CreateObjectFromByteStream;  //   HRESULT CreateObjectFromByteStream (IMFSourceResolver * This, IMFByteStream *pByteStream, LPCWSTR pwszURL, DWORD dwFlags, IPropertyStore *pProps, MF_OBJECT_TYPE *pObjectType, IUnknown **ppObject);
  LPVOID BeginCreateObjectFromURL;    //   HRESULT BeginCreateObjectFromURL (IMFSourceResolver * This, LPCWSTR pwszURL, DWORD dwFlags, IPropertyStore *pProps, IUnknown **ppIUnknownCancelCookie,  IMFAsyncCallback *pCallback, IUnknown *punkState);
  LPVOID EndCreateObjectFromURL;      //   HRESULT EndCreateObjectFromURL (IMFSourceResolver * This, IMFAsyncResult *pResult, MF_OBJECT_TYPE *pObjectType, IUnknown **ppObject);
  LPVOID BeginCreateObjectFromByteStream; //   HRESULT BeginCreateObjectFromByteStream (IMFSourceResolver * This, IMFByteStream *pByteStream, LPCWSTR pwszURL, DWORD dwFlags, IPropertyStore *pProps, IUnknown **ppIUnknownCancelCookie, IMFAsyncCallback *pCallback, IUnknown *punkState);
  LPVOID EndCreateObjectFromByteStream;   //   HRESULT EndCreateObjectFromByteStream (IMFSourceResolver * This, IMFAsyncResult *pResult, MF_OBJECT_TYPE *pObjectType, IUnknown **ppObject);
  LPVOID CancelObjectCreation;        //   HRESULT CancelObjectCreation (IMFSourceResolver * This, IUnknown *pIUnknownCancelCookie);
}

struct IMFSourceResolver
{
  IMFSourceResolverVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FActivateObject (LPVOID * This, REFIID riid, byte **ppv);
typedef [callback] HRESULT FGetAllocatedString (LPVOID * This, REFGUID guidKey, LPWSTR *ppwszValue, UINT32 *pcchLength);

struct IMFActivateVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  LPVOID GetItem; //  HRESULT GetItem (IMFActivate * This, REFGUID guidKey, PROPVARIANT *pValue);
  LPVOID GetItemType; //  HRESULT GetItemType (IMFActivate * This, REFGUID guidKey, MF_ATTRIBUTE_TYPE *pType);
  LPVOID CompareItem; //  HRESULT CompareItem (IMFActivate * This, REFGUID guidKey, REFPROPVARIANT Value, BOOL *pbResult);
  LPVOID Compare; //  HRESULT Compare (IMFActivate * This, IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL *pbResult);
  LPVOID GetUINT32; //  HRESULT GetUINT32 (IMFActivate * This, REFGUID guidKey, UINT32 *punValue);
  LPVOID GetUINT64; //  HRESULT GetUINT64 (IMFActivate * This, REFGUID guidKey, UINT64 *punValue);
  LPVOID GetDouble; //  HRESULT GetDouble (IMFActivate * This, REFGUID guidKey, double *pfValue);
  LPVOID GetGUID; //  HRESULT GetGUID (IMFActivate * This, REFGUID guidKey, GUID *pguidValue);
  LPVOID GetStringLength; //  HRESULT GetStringLength (IMFActivate * This, REFGUID guidKey, UINT32 *pcchLength);
  LPVOID GetString; //  HRESULT GetString (IMFActivate * This, REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32 *pcchLength);
  FGetAllocatedString GetAllocatedString; // HRESULT GetAllocatedString (IMFActivate * This, REFGUID guidKey, LPWSTR *ppwszValue, UINT32 *pcchLength);
  LPVOID GetBlobSize; //  HRESULT GetBlobSize (IMFActivate * This, REFGUID guidKey, UINT32 *pcbBlobSize);
  LPVOID GetBlob; //  HRESULT GetBlob (IMFActivate * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize, UINT32 *pcbBlobSize);
  LPVOID GetAllocatedBlob; //  HRESULT GetAllocatedBlob (IMFActivate * This, REFGUID guidKey, UINT8 **ppBuf, UINT32 *pcbSize);
  LPVOID GetUnknown; //  HRESULT GetUnknown (IMFActivate * This, REFGUID guidKey, REFIID riid, LPVOID *ppv);
  LPVOID SetItem; //  HRESULT SetItem (IMFActivate * This, REFGUID guidKey, REFPROPVARIANT Value);
  LPVOID DeleteItem; //  HRESULT DeleteItem (IMFActivate * This, REFGUID guidKey);
  LPVOID DeleteAllItems; //  HRESULT DeleteAllItems (IMFActivate * This);
  LPVOID SetUINT32; //  HRESULT SetUINT32 (IMFActivate * This, REFGUID guidKey, UINT32 unValue);
  LPVOID SetUINT64; //  HRESULT SetUINT64 (IMFActivate * This, REFGUID guidKey, UINT64 unValue);
  LPVOID SetDouble; //  HRESULT SetDouble (IMFActivate * This, REFGUID guidKey, double fValue);
  LPVOID SetGUID; //  HRESULT SetGUID (IMFActivate * This, REFGUID guidKey, REFGUID guidValue);
  LPVOID SetString; //  HRESULT SetString (IMFActivate * This, REFGUID guidKey, LPCWSTR wszValue);
  LPVOID SetBlob; //  HRESULT SetBlob (IMFActivate * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize);
  LPVOID SetUnknown; //  HRESULT SetUnknown (IMFActivate * This, REFGUID guidKey, IUnknown *pUnknown);
  LPVOID LockStore; //  HRESULT LockStore (IMFActivate * This);
  LPVOID UnlockStore; //  HRESULT UnlockStore (IMFActivate * This);
  LPVOID GetCount; //  HRESULT GetCount (IMFActivate * This, UINT32 *pcItems);
  LPVOID GetItemByIndex; //  HRESULT GetItemByIndex (IMFActivate * This, UINT32 unIndex, GUID *pguidKey, PROPVARIANT *pValue);
  LPVOID CopyAllItems; //  HRESULT CopyAllItems (IMFActivate * This, IMFAttributes *pDest);
  FActivateObject ActivateObject; //  HRESULT ActivateObject (IMFActivate * This, REFIID riid, void **ppv);
  LPVOID ShutdownObject; //  HRESULT ShutdownObject (IMFActivate * This);
  LPVOID DetachObject; //  HRESULT DetachObject (IMFActivate * This);
}

struct IMFActivate
{
  IMFActivateVtbl *lpVtbl;
}

//============================================================================================

typedef [callback]  HRESULT FSetMasterVolume (LPVOID * This, float fLevel);
typedef [callback]  HRESULT FGetMasterVolume (LPVOID * This, float *pfLevel);
typedef [callback]  HRESULT FSetMute         (LPVOID * This, BOOL bMute);
typedef [callback]  HRESULT FGetMute         (LPVOID * This, BOOL *pbMute);

struct IMFSimpleAudioVolumeVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;
  FSetMasterVolume  SetMasterVolume;
  FGetMasterVolume  GetMasterVolume;
  FSetMute          SetMute;
  FGetMute          GetMute;
}

struct IMFSimpleAudioVolume
{
  IMFSimpleAudioVolumeVtbl *lpVtbl;
}

//============================================================================================

typedef [callback]  HRESULT FCloseDeviceHandle (LPVOID * This, HANDLE hDevice);
typedef [callback]  HRESULT FGetVideoService   (LPVOID * This, HANDLE hDevice, REFIID riid, byte **ppService);
typedef [callback]  HRESULT FLockDevice        (LPVOID * This, HANDLE hDevice, REFIID riid, byte **ppUnkDevice, BOOL fBlock);
typedef [callback]  HRESULT FOpenDeviceHandle  (LPVOID * This, HANDLE *phDevice);
typedef [callback]  HRESULT FResetDevice       (LPVOID * This, IUnknown *pUnkDevice, UINT resetToken);
typedef [callback]  HRESULT FTestDevice        (LPVOID * This, HANDLE hDevice);
typedef [callback]  HRESULT FUnlockDevice      (LPVOID * This, HANDLE hDevice, BOOL fSaveState);

struct IMFDXGIDeviceManagerVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;
  FCloseDeviceHandle CloseDeviceHandle; // HRESULT CloseDeviceHandle (IMFDXGIDeviceManager * This, HANDLE hDevice);
  FGetVideoService GetVideoService; // HRESULT GetVideoService   (IMFDXGIDeviceManager * This, HANDLE hDevice, REFIID riid, void **ppService);
  FLockDevice LockDevice; // HRESULT LockDevice        (IMFDXGIDeviceManager * This, HANDLE hDevice, REFIID riid, void **ppUnkDevice, BOOL fBlock);
  FOpenDeviceHandle OpenDeviceHandle; // HRESULT OpenDeviceHandle  (IMFDXGIDeviceManager * This, HANDLE *phDevice);
  FResetDevice ResetDevice; // HRESULT ResetDevice       (IMFDXGIDeviceManager * This, IUnknown *pUnkDevice, UINT resetToken);
  FTestDevice TestDevice; // HRESULT TestDevice        (IMFDXGIDeviceManager * This, HANDLE hDevice);
  FUnlockDevice UnlockDevice; // HRESULT UnlockDevice      (IMFDXGIDeviceManager * This, HANDLE hDevice, BOOL fSaveState);
}

struct IMFDXGIDeviceManager
{
  IMFDXGIDeviceManagerVtbl *lpVtbl;
}

//============================================================================================

typedef [callback]  HRESULT FMTGetGUID (LPVOID * This, REFGUID guidKey, GUID *pguidValue);
typedef [callback]  HRESULT FMTSetGUID (LPVOID * This, REFGUID guidKey, REFGUID guidValue);
typedef [callback]  HRESULT FMTGetUINT32 (LPVOID * This, REFGUID guidKey, UINT32 *punValue);
typedef [callback]  HRESULT FMTSetUINT32 (LPVOID * This, REFGUID guidKey, UINT32 unValue);
typedef [callback]  HRESULT FMTGetUINT64 (LPVOID * This, REFGUID guidKey, UINT64 *punValue);
typedef [callback]  HRESULT FMTSetUINT64 (LPVOID * This, REFGUID guidKey, UINT64 unValue);

struct IMFMediaTypeVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;
  LPVOID  GetItem; //  HRESULT GetItem (IMFMediaType * This, REFGUID guidKey, PROPVARIANT *pValue);
  LPVOID  GetItemType ; //  HRESULT GetItemType (IMFMediaType * This, REFGUID guidKey, MF_ATTRIBUTE_TYPE *pType);
  LPVOID  CompareItem ; //  HRESULT CompareItem (IMFMediaType * This, REFGUID guidKey, REFPROPVARIANT Value, BOOL *pbResult);
  LPVOID  Compare ; //  HRESULT Compare (IMFMediaType * This, IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL *pbResult);
  FMTGetUINT32  GetUINT32 ; //  HRESULT GetUINT32 (IMFMediaType * This, REFGUID guidKey, UINT32 *punValue);
  FMTGetUINT64  GetUINT64 ; //  HRESULT GetUINT64 (IMFMediaType * This, REFGUID guidKey, UINT64 *punValue);
  LPVOID  GetDouble ; //  HRESULT GetDouble (IMFMediaType * This, REFGUID guidKey, double *pfValue);
  FMTGetGUID  GetGUID ; //  HRESULT GetGUID (IMFMediaType * This, REFGUID guidKey, GUID *pguidValue);
  LPVOID  GetStringLength ; //  HRESULT GetStringLength (IMFMediaType * This, REFGUID guidKey, UINT32 *pcchLength);
  LPVOID  GetString ; //  HRESULT GetString (IMFMediaType * This, REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32 *pcchLength);
  LPVOID  GetAllocatedString ; //  HRESULT GetAllocatedString (IMFMediaType * This, REFGUID guidKey, LPWSTR *ppwszValue, UINT32 *pcchLength);
  LPVOID  GetBlobSize ; //  HRESULT GetBlobSize (IMFMediaType * This, REFGUID guidKey, UINT32 *pcbBlobSize);
  LPVOID  GetBlob ; //  HRESULT GetBlob (IMFMediaType * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize, UINT32 *pcbBlobSize);
  LPVOID  GetAllocatedBlob ; //  HRESULT GetAllocatedBlob (IMFMediaType * This, REFGUID guidKey, UINT8 **ppBuf, UINT32 *pcbSize);
  LPVOID  GetUnknown ; //  HRESULT GetUnknown (IMFMediaType * This, REFGUID guidKey, REFIID riid, LPVOID *ppv);
  LPVOID  SetItem ; //  HRESULT SetItem (IMFMediaType * This, REFGUID guidKey, REFPROPVARIANT Value);
  LPVOID  DeleteItem ; //  HRESULT DeleteItem (IMFMediaType * This, REFGUID guidKey);
  LPVOID  DeleteAllItems ; //  HRESULT DeleteAllItems (IMFMediaType * This);
  FMTSetUINT32 SetUINT32 ; //  HRESULT SetUINT32 (IMFMediaType * This, REFGUID guidKey, UINT32 unValue);
  FMTSetUINT64  SetUINT64 ; //  HRESULT SetUINT64 (IMFMediaType * This, REFGUID guidKey, UINT64 unValue);
  LPVOID  SetDouble ; //  HRESULT SetDouble (IMFMediaType * This, REFGUID guidKey, double fValue);
  FMTSetGUID SetGUID  ; //  HRESULT SetGUID (IMFMediaType * This, REFGUID guidKey, REFGUID guidValue);
  LPVOID  SetString ; //  HRESULT SetString (IMFMediaType * This, REFGUID guidKey, LPCWSTR wszValue);
  LPVOID  SetBlob ; //  HRESULT SetBlob (IMFMediaType * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize);
  LPVOID  SetUnknown ; //  HRESULT SetUnknown (IMFMediaType * This, REFGUID guidKey, IUnknown *pUnknown);
  LPVOID  LockStore ; //  HRESULT LockStore (IMFMediaType * This);
  LPVOID  UnlockStore; //  HRESULT UnlockStore (IMFMediaType * This);
  LPVOID  GetCount ; //  HRESULT GetCount (IMFMediaType * This, UINT32 *pcItems);
  LPVOID  GetItemByIndex ; //  HRESULT GetItemByIndex (IMFMediaType * This, UINT32 unIndex, GUID *pguidKey, PROPVARIANT *pValue);
  LPVOID  CopyAllItems ; //  HRESULT CopyAllItems (IMFMediaType * This, IMFAttributes *pDest);
  LPVOID  GetMajorType ; //  HRESULT GetMajorType (IMFMediaType * This, GUID *pguidMajorType);
  LPVOID  IsCompressedFormat ; //  HRESULT IsCompressedFormat (IMFMediaType * This, BOOL *pfCompressed);
  LPVOID  IsEqual ; //  HRESULT IsEqual (IMFMediaType * This, IMFMediaType *pIMediaType, DWORD *pdwFlags);
  LPVOID  GetRepresentation ; //  HRESULT GetRepresentation (IMFMediaType * This, GUID guidRepresentation, LPVOID *ppvRepresentation);
  LPVOID  FreeRepresentation ; //  HRESULT FreeRepresentation (IMFMediaType * This, GUID guidRepresentation, LPVOID pvRepresentation);
}

struct IMFMediaType
{
  IMFMediaTypeVtbl *lpVtbl;
}

//============================================================================================

HRESULT MFGetAttributeSize (IMFMediaType* pAttributes, REFGUID guidKey, UINT32 *punWidth, UINT32 *punHeight);
HRESULT MFSetAttributeSize (IMFMediaType* pAttributes, REFGUID guidKey, UINT32 unWidth, UINT32 unHeight);

//============================================================================================

typedef [callback] HRESULT FMBLock (LPVOID * This, BYTE **ppbBuffer, DWORD *pcbMaxLength, DWORD *pcbCurrentLength);
typedef [callback] HRESULT FMBUnlock (LPVOID * This);

struct IMFMediaBufferVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;
  FMBLock  Lock; //HRESULT Lock (IMFMediaBuffer * This, BYTE **ppbBuffer, DWORD *pcbMaxLength, DWORD *pcbCurrentLength);
  FMBUnlock  Unlock; //HRESULT Unlock (IMFMediaBuffer * This);
  LPVOID  GetCurrentLength; //HRESULT GetCurrentLength (IMFMediaBuffer * This, DWORD *pcbCurrentLength);
  LPVOID  SetCurrentLength; //HRESULT SetCurrentLength (IMFMediaBuffer * This, DWORD cbCurrentLength);
  LPVOID  GetMaxLength; //HRESULT GetMaxLength (IMFMediaBuffer * This, DWORD *pcbMaxLength);
}

struct IMFMediaBuffer
{
  IMFMediaBufferVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FMB2Lock2D (LPVOID * This, BYTE **ppbScanline0, LONG *plPitch);
typedef [callback] HRESULT FMB2Unlock2D (LPVOID * This);

struct IMF2DBufferVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;
  FMB2Lock2D  Lock2D; //HRESULT Lock2D (IMF2DBuffer * This, BYTE **ppbScanline0, LONG *plPitch);
  FMB2Unlock2D  Unlock2D; //HRESULT Unlock2D (IMF2DBuffer * This);
  LPVOID  GetScanline0AndPitch; //HRESULT GetScanline0AndPitch (IMF2DBuffer * This, BYTE **pbScanline0, LONG *plPitch);
  LPVOID  IsContiguousFormat; //HRESULT IsContiguousFormat (IMF2DBuffer * This, BOOL *pfIsContiguous);
  LPVOID  GetContiguousLength; //HRESULT GetContiguousLength (IMF2DBuffer * This, DWORD *pcbLength);
  LPVOID  ContiguousCopyTo; //HRESULT ContiguousCopyTo (IMF2DBuffer * This, BYTE *pbDestBuffer, DWORD cbDestBuffer);
  LPVOID  ContiguousCopyFrom; //HRESULT ContiguousCopyFrom (IMF2DBuffer * This, BYTE *pbSrcBuffer, DWORD cbSrcBuffer);
}

struct IMF2DBuffer
{
  IMF2DBufferVtbl *lpVtbl;
}

//============================================================================================

typedef UINT32 MF2DBuffer_LockFlags;
const MF2DBuffer_LockFlags MF2DBuffer_LockFlags_Read	= 0x1;
const MF2DBuffer_LockFlags MF2DBuffer_LockFlags_Write	= 0x2;

typedef [callback] HRESULT FB2Lock2DSize (LPVOID * This, MF2DBuffer_LockFlags lockFlags, BYTE **ppbScanline0, LONG *plPitch, BYTE **ppbBufferStart, DWORD *pcbBufferLength);
typedef [callback] HRESULT FB2Unlock2D (LPVOID * This);

struct IMF2DBuffer2Vtbl        // available for Windows 8 only, so not always
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;
  LPVOID  Lock2D; //HRESULT Lock2D (IMF2DBuffer2 * This, BYTE **ppbScanline0, LONG *plPitch);
  FB2Unlock2D  Unlock2D; //HRESULT Unlock2D (IMF2DBuffer2 * This);
  LPVOID  GetScanline0AndPitch; //HRESULT GetScanline0AndPitch (IMF2DBuffer2 * This, BYTE **pbScanline0, LONG *plPitch);
  LPVOID  IsContiguousFormat; //HRESULT IsContiguousFormat (IMF2DBuffer2 * This, BOOL *pfIsContiguous);
  LPVOID  GetContiguousLength; //HRESULT GetContiguousLength (IMF2DBuffer2 * This, DWORD *pcbLength);
  LPVOID  ContiguousCopyTo; //HRESULT ContiguousCopyTo (IMF2DBuffer2 * This, BYTE *pbDestBuffer, DWORD cbDestBuffer);
  LPVOID  ContiguousCopyFrom; //HRESULT ContiguousCopyFrom (IMF2DBuffer2 * This, BYTE *pbSrcBuffer, DWORD cbSrcBuffer);
  FB2Lock2DSize  Lock2DSize; //HRESULT Lock2DSize (IMF2DBuffer2 * This, MF2DBuffer_LockFlags lockFlags, BYTE **ppbScanline0, LONG *plPitch, BYTE **ppbBufferStart, DWORD *pcbBufferLength);
  LPVOID  Copy2DTo; //HRESULT Copy2DTo (IMF2DBuffer2 * This, IMF2DBuffer2 *pDestBuffer);
}

struct IMF2DBuffer2
{
  IMF2DBuffer2Vtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FB3GetBufferCount (LPVOID * This, DWORD *pdwBufferCount);
typedef [callback] HRESULT FB3GetBufferByIndex (LPVOID * This, DWORD dwIndex, IMFMediaBuffer **ppBuffer);

struct IMFSampleVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;
  LPVOID  GetItem; //HRESULT GetItem (IMFSample * This, REFGUID guidKey, PROPVARIANT *pValue);
  LPVOID  GetItemType; //HRESULT GetItemType (IMFSample * This, REFGUID guidKey, MF_ATTRIBUTE_TYPE *pType);
  LPVOID  CompareItem; //HRESULT CompareItem (IMFSample * This, REFGUID guidKey, REFPROPVARIANT Value, BOOL *pbResult);
  LPVOID  Compare; //HRESULT Compare (IMFSample * This, IMFAttributes *pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL *pbResult);
  LPVOID  GetUINT32; //HRESULT GetUINT32 (IMFSample * This, REFGUID guidKey, UINT32 *punValue);
  LPVOID  GetUINT64; //HRESULT GetUINT64 (IMFSample * This, REFGUID guidKey, UINT64 *punValue);
  LPVOID  GetDouble; //HRESULT GetDouble (IMFSample * This, REFGUID guidKey, double *pfValue);
  LPVOID  GetGUID; //HRESULT GetGUID (IMFSample * This, REFGUID guidKey, GUID *pguidValue);
  LPVOID  GetStringLength; //HRESULT GetStringLength (IMFSample * This, REFGUID guidKey, UINT32 *pcchLength);
  LPVOID  GetString; //HRESULT GetString (IMFSample * This, REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32 *pcchLength);
  LPVOID  GetAllocatedString; //HRESULT GetAllocatedString (IMFSample * This, REFGUID guidKey, LPWSTR *ppwszValue, UINT32 *pcchLength);
  LPVOID  GetBlobSize; //HRESULT GetBlobSize (IMFSample * This, REFGUID guidKey, UINT32 *pcbBlobSize);
  LPVOID  GetBlob; //HRESULT GetBlob (IMFSample * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize, UINT32 *pcbBlobSize);
  LPVOID  GetAllocatedBlob; //HRESULT GetAllocatedBlob (IMFSample * This, REFGUID guidKey, UINT8 **ppBuf, UINT32 *pcbSize);
  LPVOID  GetUnknown; //HRESULT GetUnknown (IMFSample * This, REFGUID guidKey, REFIID riid, LPVOID *ppv);
  LPVOID  SetItem; //HRESULT SetItem (IMFSample * This, REFGUID guidKey, REFPROPVARIANT Value);
  LPVOID  DeleteItem; //HRESULT DeleteItem (IMFSample * This, REFGUID guidKey);
  LPVOID  DeleteAllItems; //HRESULT DeleteAllItems (IMFSample * This);
  LPVOID  SetUINT32; //HRESULT SetUINT32 (IMFSample * This, REFGUID guidKey, UINT32 unValue);
  LPVOID  SetUINT64; //HRESULT SetUINT64 (IMFSample * This, REFGUID guidKey, UINT64 unValue);
  LPVOID  SetDouble; //HRESULT SetDouble (IMFSample * This, REFGUID guidKey, double fValue);
  LPVOID  SetGUID; //HRESULT SetGUID (IMFSample * This, REFGUID guidKey, REFGUID guidValue);
  LPVOID  SetString; //HRESULT SetString (IMFSample * This, REFGUID guidKey, LPCWSTR wszValue);
  LPVOID  SetBlob; //HRESULT SetBlob (IMFSample * This, REFGUID guidKey, UINT8 *pBuf, UINT32 cbBufSize);
  LPVOID  SetUnknown; //HRESULT SetUnknown (IMFSample * This, REFGUID guidKey, IUnknown *pUnknown);
  LPVOID  LockStore; //HRESULT LockStore (IMFSample * This);
  LPVOID  UnlockStore; //HRESULT UnlockStore (IMFSample * This);
  LPVOID  GetCount; //HRESULT GetCount (IMFSample * This, UINT32 *pcItems);
  LPVOID  GetItemByIndex; //HRESULT GetItemByIndex (IMFSample * This, UINT32 unIndex, GUID *pguidKey, PROPVARIANT *pValue);
  LPVOID  CopyAllItems; //HRESULT CopyAllItems (IMFSample * This, IMFAttributes *pDest);
  LPVOID  GetSampleFlags; //HRESULT GetSampleFlags (IMFSample * This, DWORD *pdwSampleFlags);
  LPVOID  SetSampleFlags; //HRESULT SetSampleFlags (IMFSample * This, DWORD dwSampleFlags);
  LPVOID  GetSampleTime; //HRESULT GetSampleTime (IMFSample * This, LONGLONG *phnsSampleTime);
  LPVOID  SetSampleTime; //HRESULT SetSampleTime (IMFSample * This, LONGLONG hnsSampleTime);
  LPVOID  GetSampleDuration; //HRESULT GetSampleDuration (IMFSample * This, LONGLONG *phnsSampleDuration);
  LPVOID  SetSampleDuration; //HRESULT SetSampleDuration (IMFSample * This, LONGLONG hnsSampleDuration);
  FB3GetBufferCount  GetBufferCount; //HRESULT GetBufferCount (IMFSample * This, DWORD *pdwBufferCount);
  FB3GetBufferByIndex  GetBufferByIndex; //HRESULT GetBufferByIndex (IMFSample * This, DWORD dwIndex, IMFMediaBuffer **ppBuffer);
  LPVOID  ConvertToContiguousBuffer; //HRESULT ConvertToContiguousBuffer (IMFSample * This, IMFMediaBuffer **ppBuffer);
  LPVOID  AddBuffer; //HRESULT AddBuffer (IMFSample * This, IMFMediaBuffer *pBuffer);
  LPVOID  RemoveBufferByIndex; //HRESULT RemoveBufferByIndex (IMFSample * This, DWORD dwIndex);
  LPVOID  RemoveAllBuffers; //HRESULT RemoveAllBuffers (IMFSample * This);
  LPVOID  GetTotalLength; //HRESULT GetTotalLength (IMFSample * This, DWORD *pcbTotalLength);
  LPVOID  CopyToBuffer; //HRESULT CopyToBuffer (IMFSample * This, IMFMediaBuffer *pBuffer);
}

struct IMFSample
{
  IMFSampleVtbl *lpVtbl;
}

//============================================================================================

typedef [callback] HRESULT FOnReadSample (LPVOID * This, HRESULT hrStatus,
                                          DWORD dwStreamIndex, DWORD dwStreamFlags,
                                          LONGLONG llTimestamp, IMFSample *pSample);
typedef [callback] HRESULT FOnFlush (LPVOID * This, DWORD dwStreamIndex);
typedef [callback] HRESULT FOnEvent (LPVOID * This, DWORD dwStreamIndex, IMFMediaEvent *pEvent);

struct IMFSourceReaderCallbackVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FOnReadSample OnReadSample; // HRESULT OnReadSample (IMFSourceReaderCallback * This, HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *pSample);
  FOnFlush OnFlush; // HRESULT OnFlush (IMFSourceReaderCallback * This, DWORD dwStreamIndex);
  FOnEvent OnEvent; // HRESULT OnEvent (IMFSourceReaderCallback * This, DWORD dwStreamIndex, IMFMediaEvent *pEvent);
}

struct IMFSourceReaderCallback
{
  IMFSourceReaderCallbackVtbl *lpVtbl;
}

//============================================================================================


const UINT32 MF_SOURCE_READER_ALL_STREAMS = 0xFFFFFFFE;

const UINT32 MF_SOURCE_READERF_ERROR	= 0x1;
const UINT32 MF_SOURCE_READERF_ENDOFSTREAM	= 0x2;
const UINT32 MF_SOURCE_READERF_NEWSTREAM	= 0x4;
const UINT32 MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED	= 0x10;
const UINT32 MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED	= 0x20;
const UINT32 MF_SOURCE_READERF_STREAMTICK	= 0x100;
const UINT32 MF_SOURCE_READERF_ALLEFFECTSREMOVED	= 0x200;

typedef [callback]  HRESULT FSetStreamSelection (LPVOID * This, DWORD dwStreamIndex, BOOL fSelected);
typedef [callback]  HRESULT FGetNativeMediaType (LPVOID * This, DWORD dwStreamIndex, DWORD dwMediaTypeIndex, IMFMediaType **ppMediaType);
typedef [callback]  HRESULT FGetCurrentMediaType(LPVOID * This, DWORD dwStreamIndex, IMFMediaType **ppMediaType);
typedef [callback]  HRESULT FSetCurrentMediaType (LPVOID * This, DWORD dwStreamIndex, DWORD *pdwReserved, IMFMediaType *pMediaType);
typedef [callback]  HRESULT FReadSample (LPVOID * This, DWORD dwStreamIndex, DWORD dwControlFlags, DWORD *pdwActualStreamIndex,
                                         DWORD *pdwStreamFlags, LONGLONG *pllTimestamp, IMFSample **ppSample);

struct IMFSourceReaderVtbl
{
  FQueryInterface   QueryInterface;
  FAddRef           AddRef;
  FRelease          Release;
  LPVOID GetStreamSelection; // HRESULT GetStreamSelection (IMFSourceReader * This, DWORD dwStreamIndex, BOOL *pfSelected);
  FSetStreamSelection SetStreamSelection; // HRESULT SetStreamSelection (IMFSourceReader * This, DWORD dwStreamIndex, BOOL fSelected);
  FGetNativeMediaType GetNativeMediaType; // HRESULT GetNativeMediaType (IMFSourceReader * This, DWORD dwStreamIndex, DWORD dwMediaTypeIndex, IMFMediaType **ppMediaType);
  FGetCurrentMediaType GetCurrentMediaType; // HRESULT GetCurrentMediaType(IMFSourceReader * This, DWORD dwStreamIndex, IMFMediaType **ppMediaType);
  FSetCurrentMediaType SetCurrentMediaType; // HRESULT SetCurrentMediaType (IMFSourceReader * This, DWORD dwStreamIndex, DWORD *pdwReserved, IMFMediaType *pMediaType);
  LPVOID SetCurrentPosition; // HRESULT SetCurrentPosition (IMFSourceReader * This, REFGUID guidTimeFormat, REFPROPVARIANT varPosition);
  FReadSample ReadSample; // HRESULT ReadSample (IMFSourceReader * This, DWORD dwStreamIndex, DWORD dwControlFlags, DWORD *pdwActualStreamIndex, DWORD *pdwStreamFlags, LONGLONG *pllTimestamp, IMFSample **ppSample);
  LPVOID Flush; // HRESULT Flush (IMFSourceReader * This, DWORD dwStreamIndex);
  LPVOID GetServiceForStream; //  HRESULT GetServiceForStream (IMFSourceReader * This, DWORD dwStreamIndex, REFGUID guidService, REFIID riid, LPVOID *ppvObject);
  LPVOID GetPresentationAttribute; // HRESULT GetPresentationAttribute (IMFSourceReader * This, DWORD dwStreamIndex, REFGUID guidAttribute, PROPVARIANT *pvarAttribute);
}

struct IMFSourceReader
{
 IMFSourceReaderVtbl *lpVtbl;
}

//============================================================================================

bool SUCCEEDED (HRESULT hr);
bool FAILED (HRESULT hr);

//============================================================================================

typedef [callback]
HRESULT MFCREATEMEDIASESSION (IMFAttributes *pConfiguration, IMFMediaSession **ppMediaSession);

typedef [callback]
HRESULT MFGETSERVICE (IUnknown* punkObject, REFGUID guidService, REFGUID riid, LPVOID* ppvObject);

typedef [callback]
HRESULT MFCREATETOPOLOGY (IMFTopology **ppTopo);

typedef [callback]
HRESULT MFCREATETOPOLOGYNODE (MF_TOPOLOGY_TYPE NodeType, IMFTopologyNode ** ppNode );

typedef [callback]
HRESULT MFCREATEAUDIORENDERERACTIVATE (IMFActivate **ppActivate);

typedef [callback]
HRESULT MFCREATEVIDEORENDERERACTIVATE (HWND hwndVideo, IMFActivate **ppActivate);

typedef [callback]
HRESULT MFCREATESOURCERESOLVER (IMFSourceResolver **ppISourceResolver);

typedef [callback]
HRESULT MFENUMDEVICESOURCES (IMFAttributes *pAttributes, IMFActivate   ***pppSourceActivate, UINT32 *pcSourceActivate);

struct MF_CALLS  // [extern "Mf.dll"]
{
  MFCREATEMEDIASESSION           MFCreateMediaSession;
  MFGETSERVICE                   MFGetService;
  MFCREATETOPOLOGY               MFCreateTopology;
  MFCREATETOPOLOGYNODE           MFCreateTopologyNode;
  MFCREATEAUDIORENDERERACTIVATE  MFCreateAudioRendererActivate;
  MFCREATEVIDEORENDERERACTIVATE  MFCreateVideoRendererActivate;
  MFCREATESOURCERESOLVER         MFCreateSourceResolver;
  MFENUMDEVICESOURCES            MFEnumDeviceSources;
}

MF_CALLS* init_mf_calls ();

//============================================================================================

typedef [callback]
HRESULT MFSTARTUP (ULONG Version, DWORD dwFlags);

typedef [callback]
HRESULT MFCREATEATTRIBUTES (IMFAttributes **pattributes, UINT32 csize);

typedef [callback]
HRESULT MFSHUTDOWN ();

typedef [callback]
HRESULT MFCREATEDXGIDEVICEMANAGER (UINT *resetToken, IMFDXGIDeviceManager **ppDeviceManager);

typedef [callback]
HRESULT MFCREATEMEDIATYPE(IMFMediaType **ppMFType);

// typedef [callback] HRESULT MFGETSTRIDEFORBITMAPINFOHEADER(DWORD format, DWORD dwWidth, LONG *pStride);

struct MF_PLAT_CALLS   // [extern "Mfplat.dll"]
{
  MFSTARTUP          MFStartup;
  MFCREATEATTRIBUTES MFCreateAttributes;
  MFSHUTDOWN         MFShutdown;
  MFCREATEMEDIATYPE  MFCreateMediaType;
//  MFGETSTRIDEFORBITMAPINFOHEADER MFGetStrideForBitmapInfoHeader;
  MFCREATEDXGIDEVICEMANAGER  MFCreateDXGIDeviceManager;  // can be null (for windows 8 only)
}

MF_PLAT_CALLS* init_mf_plat_calls ();

//============================================================================================

typedef [callback]
HRESULT MFCREATESOURCEREADERFROMMEDIASOURCE (IMFMediaSource *pMediaSource, IMFAttributes *pAttributes, IMFSourceReader **ppSourceReader);

struct MF_READ_WRITE_CALLS   // [extern "Mfreadwrite.dll"]
{
  MFCREATESOURCEREADERFROMMEDIASOURCE  MFCreateSourceReaderFromMediaSource;  // can be null
}

MF_READ_WRITE_CALLS* init_mf_read_write_calls ();

//============================================================================================

//============================================================================================
// Media Engine (from Microsoft Media Foundation, for Windows 8 or above)
//============================================================================================

const IID IID_IMFMediaEngineNotify             = {0xfee7c112, 0xe776, 0x42b5, 0x9b, 0xbf, 0x00, 0x48, 0x52, 0x4e, 0x2b, 0xd5};
const IID IID_IMFMediaEngine                   = {0x98a1b0bb, 0x03eb, 0x4935, 0xae, 0x7c, 0x93, 0xc1, 0xfa, 0x0e, 0x1c, 0x93};
const IID IID_IMFMediaEngineClassFactory       = {0x4D645ACE, 0x26AA, 0x4688, 0x9B, 0xE1, 0xDF, 0x35, 0x16, 0x99, 0x0B, 0x93};
const IID IID_IMFMediaError                    = {0xfc0e10d2, 0xab2a, 0x4501, 0xa9, 0x51, 0x06, 0xbb, 0x10, 0x75, 0x18, 0x4c};
const REFCLSID CLSID_MFMediaEngineClassFactory = {0xb44392da, 0x499b, 0x446b, 0xa4, 0xcb, 0x00, 0x5f, 0xea, 0xd0, 0xe6, 0xd5};
const GUID MF_MEDIA_ENGINE_CALLBACK            = {0xc60381b8, 0x83a4, 0x41f8, 0xa3, 0xd0, 0xde, 0x05, 0x07, 0x68, 0x49, 0xa9};
const GUID MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT = {0x5066893c, 0x8cf9, 0x42bc, 0x8b, 0x8a, 0x47, 0x22, 0x12, 0xe5, 0x27, 0x26};
const GUID MF_MEDIA_ENGINE_DXGI_MANAGER        = {0x065702da, 0x1094, 0x486d, 0x86, 0x17, 0xee, 0x7c, 0xc4, 0xee, 0x46, 0x48};

//---------------------------------------------------------------------------------------

typedef uint MF_MEDIA_ENGINE_EVENT;

const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_LOADSTART	= 1;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_PROGRESS	= 2;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_SUSPEND	= 3;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_ABORT	= 4;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_ERROR	= 5;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_EMPTIED	= 6;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_STALLED	= 7;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_PLAY	= 8;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_PAUSE	= 9;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA	= 10;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_LOADEDDATA	= 11;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_WAITING	= 12;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_PLAYING	= 13;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_CANPLAY	= 14;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH	= 15;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_SEEKING	= 16;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_SEEKED	= 17;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_TIMEUPDATE	= 18;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_ENDED	= 19;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_RATECHANGE	= 20;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE	= 21;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE	= 22;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_FORMATCHANGE	= 1000;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS	= 1001;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_TIMELINE_MARKER	= 1002;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_BALANCECHANGE	= 1003;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_DOWNLOADCOMPLETE	= 1004;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED	= 1005;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED	= 1006;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED	= 1007;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE	= 1008;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY	= 1009;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_TRACKSCHANGE	= 1010;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_OPMINFO	= 1011;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_RESOURCELOST	= 1012;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED	= 1013;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_STREAMRENDERINGERROR	= 1014;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_SUPPORTEDRATES_CHANGED	= 1015;
const MF_MEDIA_ENGINE_EVENT  MF_MEDIA_ENGINE_EVENT_AUDIOENDPOINTCHANGE	= 1016;

//---------------------------------------------------------------------------------------

typedef wchar *BSTR;

enum MF_MEDIA_ENGINE_CANPLAY
 {MF_MEDIA_ENGINE_CANPLAY_NOT_SUPPORTED,
  MF_MEDIA_ENGINE_CANPLAY_MAYBE,
  MF_MEDIA_ENGINE_CANPLAY_PROBABLY};

enum MF_MEDIA_ENGINE_PRELOAD
 {MF_MEDIA_ENGINE_PRELOAD_MISSING,
  MF_MEDIA_ENGINE_PRELOAD_EMPTY,
  MF_MEDIA_ENGINE_PRELOAD_NONE,
  MF_MEDIA_ENGINE_PRELOAD_METADATA,
  MF_MEDIA_ENGINE_PRELOAD_AUTOMATIC};

//---------------------------------------------------------------------------------------

typedef [callback] USHORT FGetErrorCode (LPVOID * This);
typedef [callback] HRESULT FGetExtendedErrorCode (LPVOID * This);

struct IMFMediaErrorVtbl
{
  FQueryInterface     QueryInterface;
  FAddRef             AddRef;
  FRelease            Release;
  FGetErrorCode GetErrorCode;                 // USHORT GetErrorCode (IMFMediaError * This);
  FGetExtendedErrorCode GetExtendedErrorCode; // HRESULT GetExtendedErrorCode (IMFMediaError * This);
//  HRESULT SetErrorCode (IMFMediaError * This, MF_MEDIA_ENGINE_ERR error);
//  HRESULT SetExtendedErrorCode (IMFMediaError * This, HRESULT error);
}

struct IMFMediaError
{
  IMFMediaErrorVtbl *lpVtbl;
}

//---------------------------------------------------------------------------------------

typedef [callback] HRESULT FEventNotify (LPVOID * This, DWORD event, DWORD_PTR param1, DWORD param2);

struct IMFMediaEngineNotifyVtbl
{
  FQueryInterface     QueryInterface;
  FAddRef             AddRef;
  FRelease            Release;
  FEventNotify        EventNotify;
}

struct IMFMediaEngineNotify
{
  IMFMediaEngineNotifyVtbl *lpVtbl;
}

//---------------------------------------------------------------------------------------

typedef [callback]  DWORD   FGetLength   (LPVOID * This);
typedef [callback]  HRESULT FGetStart  (LPVOID * This, DWORD index, double *pStart);
typedef [callback]  HRESULT FGetEnd    (LPVOID * This, DWORD index, double *pEnd);
typedef [callback]  BOOL    FContainsTime (LPVOID * This, double time);
typedef [callback]  HRESULT FAddRange  (LPVOID * This, double startTime, double endTime);
typedef [callback]  HRESULT FClear     (LPVOID * This);

struct IMFMediaTimeRangeVtbl
{
  FQueryInterface     QueryInterface;
  FAddRef             AddRef;
  FRelease            Release;
  FGetLength  GetLength;
  FGetStart   GetStart;
  FGetEnd     GetEnd;
  FContainsTime ContainsTime;
  FAddRange   AddRange;
  FClear      Clear;
}

struct IMFMediaTimeRange
{
  IMFMediaTimeRangeVtbl *lpVtbl;
}

//---------------------------------------------------------------------------------------

typedef [callback] HRESULT FGetError (LPVOID * This, IMFMediaError **ppError);
typedef [callback] HRESULT FSetSource (LPVOID * This, BSTR pUrl);
typedef [callback] double  FGetCurrentTime (LPVOID * This);
typedef [callback] HRESULT FSetCurrentTime (LPVOID * This, double seekTime);
typedef [callback] double  FGetDuration (LPVOID * This);
typedef [callback] HRESULT FGetCurrentSource (LPVOID * This, BSTR *ppUrl);
typedef [callback] USHORT FGetNetworkState (LPVOID * This);
typedef [callback] MF_MEDIA_ENGINE_PRELOAD FGetPreload (LPVOID * This);
typedef [callback] HRESULT FSetPreload (LPVOID * This, MF_MEDIA_ENGINE_PRELOAD Preload);
typedef [callback] HRESULT FLoad (LPVOID * This);
typedef [callback] HRESULT FCanPlayType (LPVOID * This, BSTR type, MF_MEDIA_ENGINE_CANPLAY *pAnswer);
typedef [callback] USHORT  FGetReadyState (LPVOID * This);
typedef [callback] HRESULT FGetBuffered (LPVOID * This, IMFMediaTimeRange **ppBuffered);
typedef [callback] HRESULT FGetPlayed   (LPVOID * This, IMFMediaTimeRange **ppPlayed);
typedef [callback] HRESULT FGetSeekable (LPVOID * This, IMFMediaTimeRange **ppSeekable);
typedef [callback] double  FGetStartTime (LPVOID * This);
typedef [callback] BOOL FIsSeeking                 (LPVOID * This);
typedef [callback] double FGetDefaultPlaybackRate  (LPVOID * This);
typedef [callback] HRESULT FSetDefaultPlaybackRate (LPVOID * This, double Rate);
typedef [callback] double FGetPlaybackRate  (LPVOID * This);
typedef [callback] HRESULT FSetPlaybackRate (LPVOID * This, double Rate);
typedef [callback] BOOL FGetAutoPlay    (LPVOID * This);
typedef [callback] HRESULT FSetAutoPlay (LPVOID * This, BOOL AutoPlay);
typedef [callback] BOOL FGetLoop        (LPVOID * This);
typedef [callback] HRESULT FSetLoop     (LPVOID * This, BOOL Loop);
typedef [callback] BOOL    FIsPaused (LPVOID * This);
typedef [callback] BOOL    FIsEnded (LPVOID * This);
typedef [callback] HRESULT FPlay (LPVOID * This);
typedef [callback] HRESULT FPause2 (LPVOID * This);
typedef [callback] BOOL    FGetMuted (LPVOID * This);
typedef [callback] HRESULT FSetMuted (LPVOID * This, BOOL Muted);
typedef [callback] double  FGetVolume (LPVOID * This);
typedef [callback] HRESULT FSetVolume (LPVOID * This, double Volume);
typedef [callback] BOOL    FHasVideo (LPVOID * This);
typedef [callback] BOOL    FHasAudio (LPVOID * This);
typedef [callback] HRESULT FGetNativeVideoSize (LPVOID * This, DWORD *cx, DWORD *cy);
typedef [callback] HRESULT FGetVideoAspectRatio (LPVOID * This, DWORD *cx, DWORD *cy);
typedef [callback] HRESULT FShutdownME (LPVOID * This);

struct MFARGB
{
  BYTE rgbBlue;
  BYTE rgbGreen;
  BYTE rgbRed;
  BYTE rgbAlpha;
}

typedef [callback] HRESULT FTransferVideoFrame (LPVOID * This,IUnknown *pDstSurf, MFVideoNormalizedRect *pSrc, RECT *pDst, MFARGB *pBorderClr);
typedef [callback] HRESULT FOnVideoStreamTick (LPVOID * This, LONGLONG *pPts);

struct IMFMediaEngineVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FGetError  GetError; // HRESULT GetError (IMFMediaEngine * This, IMFMediaError **ppError);
  LPVOID  SetErrorCode; //   HRESULT SetErrorCode (IMFMediaEngine * This, MF_MEDIA_ENGINE_ERR error);
  LPVOID  SetSourceElements; //   HRESULT SetSourceElements (IMFMediaEngine * This, IMFMediaEngineSrcElements *pSrcElements);
  FSetSource SetSource; // HRESULT SetSource (IMFMediaEngine * This, BSTR pUrl);
  FGetCurrentSource  GetCurrentSource; //   HRESULT GetCurrentSource (IMFMediaEngine * This, BSTR *ppUrl);
  FGetNetworkState  GetNetworkState; //   USHORT GetNetworkState (IMFMediaEngine * This);
  FGetPreload  GetPreload; //   MF_MEDIA_ENGINE_PRELOAD GetPreload (IMFMediaEngine * This);
  FSetPreload  SetPreload; //   HRESULT SetPreload (IMFMediaEngine * This, MF_MEDIA_ENGINE_PRELOAD Preload);
  FGetBuffered  GetBuffered; //   HRESULT GetBuffered (IMFMediaEngine * This, IMFMediaTimeRange **ppBuffered);
  FLoad  Load; //   HRESULT Load (IMFMediaEngine * This);
  FCanPlayType  CanPlayType; //   HRESULT CanPlayType (IMFMediaEngine * This, BSTR type, MF_MEDIA_ENGINE_CANPLAY *pAnswer);
  FGetReadyState  GetReadyState; //   USHORT GetReadyState (IMFMediaEngine * This);
  FIsSeeking  IsSeeking; //   BOOL IsSeeking (IMFMediaEngine * This);
  FGetCurrentTime  GetCurrentTime; // double GetCurrentTime (IMFMediaEngine * This);
  FSetCurrentTime  SetCurrentTime; // HRESULT SetCurrentTime (IMFMediaEngine * This, double seekTime);
  FGetStartTime  GetStartTime; //   double GetStartTime (IMFMediaEngine * This);
  FGetDuration  GetDuration; //   double GetDuration (IMFMediaEngine * This);
  FIsPaused     IsPaused; //   BOOL IsPaused (IMFMediaEngine * This);
  FGetDefaultPlaybackRate  GetDefaultPlaybackRate; //   double GetDefaultPlaybackRate (IMFMediaEngine * This);
  FSetDefaultPlaybackRate  SetDefaultPlaybackRate; //   HRESULT SetDefaultPlaybackRate (IMFMediaEngine * This, double Rate);
  FGetPlaybackRate  GetPlaybackRate; //   double GetPlaybackRate (IMFMediaEngine * This);
  FSetPlaybackRate  SetPlaybackRate; //   HRESULT SetPlaybackRate (IMFMediaEngine * This, double Rate);
  FGetPlayed  GetPlayed; //   HRESULT GetPlayed (IMFMediaEngine * This, IMFMediaTimeRange **ppPlayed);
  FGetSeekable  GetSeekable; //   HRESULT GetSeekable (IMFMediaEngine * This, IMFMediaTimeRange **ppSeekable);
  FIsEnded  IsEnded; //   BOOL IsEnded (IMFMediaEngine * This);
  FGetAutoPlay  GetAutoPlay; //   BOOL GetAutoPlay (IMFMediaEngine * This);
  FSetAutoPlay  SetAutoPlay; //   HRESULT SetAutoPlay (IMFMediaEngine * This, BOOL AutoPlay);
  FGetLoop  GetLoop; //   BOOL GetLoop (IMFMediaEngine * This);
  FSetLoop  SetLoop; //   HRESULT SetLoop (IMFMediaEngine * This, BOOL Loop);
  FPlay  Play; //   HRESULT Play (IMFMediaEngine * This);
  FPause2  Pause; //   HRESULT Pause (IMFMediaEngine * This);
  FGetMuted  GetMuted; //   BOOL GetMuted (IMFMediaEngine * This);
  FSetMuted  SetMuted; //   HRESULT SetMuted (IMFMediaEngine * This, BOOL Muted);
  FGetVolume  GetVolume; //   double GetVolume (IMFMediaEngine * This);
  FSetVolume  SetVolume; //   HRESULT SetVolume (IMFMediaEngine * This, double Volume);
  FHasVideo  HasVideo; //   BOOL HasVideo (IMFMediaEngine * This);
  FHasAudio  HasAudio; //   BOOL HasAudio (IMFMediaEngine * This);
  FGetNativeVideoSize  GetNativeVideoSize; //   HRESULT GetNativeVideoSize (IMFMediaEngine * This, DWORD *cx, DWORD *cy);
  FGetVideoAspectRatio  GetVideoAspectRatio; //   HRESULT GetVideoAspectRatio (IMFMediaEngine * This, DWORD *cx, DWORD *cy);
  FShutdownME  Shutdown; //   HRESULT Shutdown (IMFMediaEngine * This);
  FTransferVideoFrame  TransferVideoFrame; //   HRESULT TransferVideoFrame (IMFMediaEngine * This,IUnknown *pDstSurf, MFVideoNormalizedRect *pSrc, RECT *pDst, MFARGB *pBorderClr);
  FOnVideoStreamTick  OnVideoStreamTick; //   HRESULT OnVideoStreamTick (IMFMediaEngine * This, LONGLONG *pPts);
}

struct IMFMediaEngine
{
  IMFMediaEngineVtbl *lpVtbl;
}

//---------------------------------------------------------------------------------------

typedef [callback] HRESULT FCreateInstance (LPVOID * This, DWORD dwFlags, IMFAttributes *pAttr, IMFMediaEngine **ppPlayer);

struct IMFMediaEngineClassFactoryVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FCreateInstance   CreateInstance;  //   HRESULT CreateInstance (IMFMediaEngineClassFactory * This, DWORD dwFlags, IMFAttributes *pAttr, IMFMediaEngine **ppPlayer);
  LPVOID  CreateTimeRange; //   HRESULT CreateTimeRange (IMFMediaEngineClassFactory * This, IMFMediaTimeRange **ppTimeRange);
  LPVOID  CreateError;     //   HRESULT CreateError (IMFMediaEngineClassFactory * This, IMFMediaError **ppError);
}

struct IMFMediaEngineClassFactory
{
  IMFMediaEngineClassFactoryVtbl *lpVtbl;
}

//---------------------------------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------------------------------
