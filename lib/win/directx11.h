
// win/directx11.h : Direct X version 11

use windows;

//---------------------------------------------------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------------------------------------------------

const UINT D3D11_SDK_VERSION	= 7;


const UINT DXGI_ERROR_DEVICE_RESET   = 0x887A0007;
const UINT DXGI_ERROR_DEVICE_REMOVED = 0x887A0005;

const REFIID IID_ID3D11Texture2D = {0x6f15aaf2,0xd208,0x4e89,0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c};
const REFIID IID_IDXGIFactory    = {0x7b7166ec,0x21c7,0x44ae,0xb2,0x1a,0xc9,0xae,0x32,0x1a,0xe3,0x69};

enum D3D11_BLEND {D3D11_BLEND_0, D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_SRC_COLOR,
  D3D11_BLEND_INV_SRC_COLOR, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
  D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_DEST_COLOR,
  D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_SRC_ALPHA_SAT, D3D11_BLEND_12, D3D11_BLEND_13,
  D3D11_BLEND_BLEND_FACTOR, D3D11_BLEND_INV_BLEND_FACTOR, D3D11_BLEND_SRC1_COLOR,
  D3D11_BLEND_INV_SRC1_COLOR, D3D11_BLEND_SRC1_ALPHA, D3D11_BLEND_INV_SRC1_ALPHA};

enum D3D11_BLEND_OP { D3D11_BLEND_OP_0, D3D11_BLEND_OP_ADD, D3D11_BLEND_OP_SUBTRACT,
  D3D11_BLEND_OP_REV_SUBTRACT, D3D11_BLEND_OP_MIN, D3D11_BLEND_OP_MAX};

typedef byte D3D11_COLOR_WRITE_ENABLE;
const byte D3D11_COLOR_WRITE_ENABLE_RED   = 1;
const byte D3D11_COLOR_WRITE_ENABLE_GREEN	= 2;
const byte D3D11_COLOR_WRITE_ENABLE_BLUE	= 4;
const byte D3D11_COLOR_WRITE_ENABLE_ALPHA	= 8;
const byte D3D11_COLOR_WRITE_ENABLE_ALL	  = 15;

struct D3D11_RENDER_TARGET_BLEND_DESC
{
  BOOL BlendEnable;
  D3D11_BLEND SrcBlend;
  D3D11_BLEND DestBlend;
  D3D11_BLEND_OP BlendOp;
  D3D11_BLEND SrcBlendAlpha;
  D3D11_BLEND DestBlendAlpha;
  D3D11_BLEND_OP BlendOpAlpha;
  UINT8 RenderTargetWriteMask;
}

struct D3D11_BLEND_DESC
{
  BOOL AlphaToCoverageEnable;
  BOOL IndependentBlendEnable;
  D3D11_RENDER_TARGET_BLEND_DESC RenderTarget[8];
}

struct LUID
{
  DWORD LowPart;
  LONG HighPart;
}

struct DXGI_ADAPTER_DESC
{
  WCHAR Description[128];
  UINT VendorId;
  UINT DeviceId;
  UINT SubSysId;
  UINT Revision;
  SIZE_T DedicatedVideoMemory;
  SIZE_T DedicatedSystemMemory;
  SIZE_T SharedSystemMemory;
  LUID AdapterLuid;
}


typedef IDXGIAdapter;
typedef IDXGIOutput;

typedef [callback] HRESULT FEnumOutputs (IDXGIAdapter * This, UINT Output, IDXGIOutput **ppOutput);
typedef [callback] HRESULT FGetDesc (IDXGIAdapter * This, DXGI_ADAPTER_DESC *pDesc);

struct IDXGIAdapterVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FDummy f3; // HRESULT ( STDMETHODCALLTYPE *SetPrivateData )(IDXGIAdapter * This,  REFGUID Name, UINT DataSize, void *pData);
  FDummy f4; // HRESULT ( STDMETHODCALLTYPE *SetPrivateDataInterface )(IDXGIAdapter * This, REFGUID Name, IUnknown *pUnknown);
  FDummy f5; // HRESULT ( STDMETHODCALLTYPE *GetPrivateData )(IDXGIAdapter * This, REFGUID Name, UINT *pDataSize, void *pData);
  FDummy f6; // HRESULT ( STDMETHODCALLTYPE *GetParent )(IDXGIAdapter * This, REFIID riid, void **ppParent);
  FEnumOutputs EnumOutputs; // HRESULT ( STDMETHODCALLTYPE *EnumOutputs )(IDXGIAdapter * This, UINT Output, IDXGIOutput **ppOutput);
  FGetDesc GetDesc; // HRESULT ( STDMETHODCALLTYPE *GetDesc )(IDXGIAdapter * This, DXGI_ADAPTER_DESC *pDesc);
  FDummy f9; // HRESULT ( STDMETHODCALLTYPE *CheckInterfaceSupport )(IDXGIAdapter * This, REFGUID InterfaceName, LARGE_INTEGER *pUMDVersion);
}

struct IDXGIAdapter
{
  IDXGIAdapterVtbl *lpVtbl;
}

typedef uint D3D_DRIVER_TYPE;
const uint D3D_DRIVER_TYPE_UNKNOWN = 0;
const uint D3D_DRIVER_TYPE_HARDWARE = 1;
const uint D3D_DRIVER_TYPE_REFERENCE = 2;
const uint D3D_DRIVER_TYPE_NULL = 3;
const uint D3D_DRIVER_TYPE_SOFTWARE = 4;
const uint D3D_DRIVER_TYPE_WARP = 5;

typedef uint D3D_FEATURE_LEVEL;
const uint D3D_FEATURE_LEVEL_9_1 = 0x9100;
const uint D3D_FEATURE_LEVEL_9_2 = 0x9200;
const uint D3D_FEATURE_LEVEL_9_3 = 0x9300;
const uint D3D_FEATURE_LEVEL_10_0  = 0xa000;
const uint D3D_FEATURE_LEVEL_10_1  = 0xa100;
const uint D3D_FEATURE_LEVEL_11_0  = 0xb000;
const uint D3D_FEATURE_LEVEL_11_1  = 0xb100;


struct DXGI_RATIONAL
{
  UINT Numerator;
  UINT Denominator;
}

const uint D3D11_CREATE_DEVICE_DEBUG = 2;

typedef uint DXGI_FORMAT;

const uint DXGI_FORMAT_R32G32B32A32_FLOAT = 2;
const uint DXGI_FORMAT_R32G32B32_FLOAT = 6;
const uint DXGI_FORMAT_R32G32_TYPELESS = 0x0000000F;
const uint DXGI_FORMAT_R32G32_FLOAT = 0x00000010;
const uint DXGI_FORMAT_D32_FLOAT_S8X24_UINT = 20;
const uint DXGI_FORMAT_R8G8B8A8_UNORM = 28;
const uint DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29;
const uint DXGI_FORMAT_R8G8B8A8_UINT = 30;
const uint DXGI_FORMAT_R16G16_TYPELESS = 0x00000021;
const uint DXGI_FORMAT_R16G16_FLOAT = 0x00000022;
const uint DXGI_FORMAT_R32_TYPELESS = 0x00000027;
const uint DXGI_FORMAT_D32_FLOAT = 40;
const uint DXGI_FORMAT_R32_FLOAT = 41;
const uint DXGI_FORMAT_R32_UINT = 42;
const uint DXGI_FORMAT_R32_SINT = 0x0000002B;
const uint DXGI_FORMAT_R24G8_TYPELESS = 0x0000002C;
const uint DXGI_FORMAT_D24_UNORM_S8_UINT = 45;
const uint DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 0x0000002E;
const uint DXGI_FORMAT_X24_TYPELESS_G8_UINT = 0x0000002F;
const uint DXGI_FORMAT_R16_UINT = 57;
const uint DXGI_FORMAT_R32G8X24_TYPELESS = 19;
const uint DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21;

typedef uint DXGI_MODE_SCANLINE_ORDER;
const uint DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED = 0;

typedef uint DXGI_MODE_SCALING;
const uint DXGI_MODE_SCALING_UNSPECIFIED = 0;

const uint D3D11_USAGE_DEFAULT = 0;
const uint D3D11_USAGE_IMMUTABLE = 1;
const uint D3D11_USAGE_DYNAMIC = 2;
const uint D3D11_USAGE_STAGING = 3;

const uint D3D11_BIND_VERTEX_BUFFER     = 0x1;
const uint D3D11_BIND_INDEX_BUFFER      = 0x2;
const uint D3D11_BIND_CONSTANT_BUFFER   = 0x4;
const uint D3D11_BIND_SHADER_RESOURCE   = 0x8;
const uint D3D11_BIND_STREAM_OUTPUT     = 0x10;
const uint D3D11_BIND_RENDER_TARGET     = 0x20;
const uint D3D11_BIND_DEPTH_STENCIL     = 0x40;
const uint D3D11_BIND_UNORDERED_ACCESS  = 0x80;
const uint D3D11_BIND_DECODER           = 0x200;
const uint D3D11_BIND_VIDEO_ENCODER     = 0x400;

const uint D3D11_DEPTH_WRITE_MASK_ZERO = 0;
const uint D3D11_DEPTH_WRITE_MASK_ALL  = 1;

const uint D3D11_COMPARISON_NEVER          = 1;
const uint D3D11_COMPARISON_LESS           = 2;
const uint D3D11_COMPARISON_EQUAL          = 3;
const uint D3D11_COMPARISON_LESS_EQUAL     = 4;
const uint D3D11_COMPARISON_GREATER        = 5;
const uint D3D11_COMPARISON_NOT_EQUAL      = 6;
const uint D3D11_COMPARISON_GREATER_EQUAL  = 7;
const uint D3D11_COMPARISON_ALWAYS         = 8;

const uint D3D11_STENCIL_OP_KEEP	= 1;
const uint D3D11_STENCIL_OP_ZERO  = 2;
const uint D3D11_STENCIL_OP_REPLACE = 3;
const uint D3D11_STENCIL_OP_INCR_SAT = 4;
const uint D3D11_STENCIL_OP_DECR_SAT = 5;
const uint D3D11_STENCIL_OP_INVERT = 6;
const uint D3D11_STENCIL_OP_INCR	= 7;
const uint D3D11_STENCIL_OP_DECR = 8;

const uint D3D11_DSV_DIMENSION_UNKNOWN = 0;
const uint D3D11_DSV_DIMENSION_TEXTURE1D = 1;
const uint D3D11_DSV_DIMENSION_TEXTURE1DARRAY = 2;
const uint D3D11_DSV_DIMENSION_TEXTURE2D = 3;
const uint D3D11_DSV_DIMENSION_TEXTURE2DARRAY = 4;
const uint D3D11_DSV_DIMENSION_TEXTURE2DMS = 5;
const uint D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY = 6;

const uint D3D11_CULL_NONE  = 1;
const uint D3D11_CULL_FRONT = 2;
const uint D3D11_CULL_BACK	= 3;

const uint D3D11_FILL_WIREFRAME = 2;
const uint D3D11_FILL_SOLID	= 3;

const uint D3D11_CLEAR_DEPTH = 1;
const uint D3D11_CLEAR_STENCIL = 2;

const uint D3D11_CPU_ACCESS_WRITE	= 0x10000;
const uint D3D11_CPU_ACCESS_READ	= 0x20000;

const uint D3D11_APPEND_ALIGNED_ELEMENT = ( 0xffffffff );

typedef uint D3D11_PRIMITIVE_TOPOLOGY;
const uint D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST	= 4;
const uint D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP	= 5;

const uint D3D11_FILTER_MIN_MAG_MIP_POINT  = 0;
const uint D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR                       = 0x00000001;
const uint D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT                 = 0x00000004;
const uint D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR                       = 0x00000005;
const uint D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT                       = 0x00000010;
const uint D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR                = 0x00000011;
const uint D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT                       = 0x00000014;
const uint D3D11_FILTER_MIN_MAG_MIP_LINEAR                             = 0x00000015;
const uint D3D11_FILTER_ANISOTROPIC                                    = 0x00000055;
const uint D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT                   = 0x00000080;
const uint D3D11_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR            = 0x00000081;
const uint D3D11_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT      = 0x00000084;
const uint D3D11_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR            = 0x00000085;
const uint D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT            = 0x00000090;
const uint D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR     = 0x00000091;
const uint D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT            = 0x00000094;
const uint D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR                  = 0x00000095;
const uint D3D11_FILTER_COMPARISON_ANISOTROPIC                         = 0x000000d5;
const uint D3D11_FILTER_MINIMUM_MIN_MAG_MIP_POINT                      = 0x00000100;
const uint D3D11_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR               = 0x00000101;
const uint D3D11_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT         = 0x00000104;
const uint D3D11_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR               = 0x00000105;
const uint D3D11_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT               = 0x00000110;
const uint D3D11_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR        = 0x00000111;
const uint D3D11_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT               = 0x00000114;
const uint D3D11_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR                     = 0x00000115;
const uint D3D11_FILTER_MINIMUM_ANISOTROPIC                            = 0x00000155;
const uint D3D11_FILTER_MAXIMUM_MIN_MAG_MIP_POINT                      = 0x00000180;
const uint D3D11_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR               = 0x00000181;
const uint D3D11_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT         = 0x00000184;
const uint D3D11_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR               = 0x00000185;
const uint D3D11_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT               = 0x00000190;
const uint D3D11_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR        = 0x00000191;
const uint D3D11_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT               = 0x00000194;
const uint D3D11_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR                     = 0x00000195;
const uint D3D11_FILTER_MAXIMUM_ANISOTROPIC                            = 0x000001d5;


const uint D3D11_TEXADDRESS_WRAP         = 1;
const uint D3D11_TEXTURE_ADDRESS_WRAP	   = 1;
const uint D3D11_TEXADDRESS_MIRROR       = 2;
const uint D3D11_TEXADDRESS_CLAMP        = 3;
const uint D3D11_TEXADDRESS_BORDER       = 4;
const uint D3D11_TEXTURE_ADDRESS_BORDER  = 4;
const uint D3D11_TEXADDRESS_MIRRORONCE   = 5;


const float D3D11_FLOAT32_MAX = ( 3.402823466e+38f );

const uint D3D11_RESOURCE_MISC_GENERATE_MIPS = 0x1;

struct DXGI_MODE_DESC
{
  UINT                     Width;
  UINT                     Height;
  DXGI_RATIONAL            RefreshRate;
  DXGI_FORMAT              Format;
  DXGI_MODE_SCANLINE_ORDER ScanlineOrdering;
  DXGI_MODE_SCALING        Scaling;
}

struct DXGI_SAMPLE_DESC
{
  UINT Count;
  UINT Quality;
}

typedef uint DXGI_USAGE;
const uint DXGI_USAGE_RENDER_TARGET_OUTPUT = 1 << (1 + 4);

typedef uint DXGI_SWAP_EFFECT;
const uint DXGI_SWAP_EFFECT_DISCARD = 0;
const uint DXGI_SWAP_EFFECT_SEQUENTIAL = 1;
const uint DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL = 3;
const uint DXGI_SWAP_EFFECT_FLIP_DISCARD = 4;

struct DXGI_SWAP_CHAIN_DESC
{
  DXGI_MODE_DESC   BufferDesc;
  DXGI_SAMPLE_DESC SampleDesc;
  DXGI_USAGE       BufferUsage;
  UINT             BufferCount;
  HWND             OutputWindow;
  BOOL             Windowed;
  DXGI_SWAP_EFFECT SwapEffect;
  UINT             Flags;
}

typedef IDXGISwapChain;

typedef [callback] HRESULT FPresent            (IDXGISwapChain* This, UINT SyncInterval, UINT Flags);
typedef [callback] HRESULT FGetBuffer          (IDXGISwapChain* This, UINT Buffer, REFIID riid, byte**ppSurface);
typedef [callback] HRESULT FSetFullscreenState (IDXGISwapChain* This, BOOL Fullscreen, IDXGIOutput *pTarget);
typedef [callback] HRESULT FResizeBuffers (IDXGISwapChain * This, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

struct IDXGISwapChainVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FDummy f3; // HRESULT SetPrivateData )(IDXGISwapChain * This, REFGUID Name, UINT DataSize, void *pData);
  FDummy f4; // HRESULT SetPrivateDataInterface )(IDXGISwapChain * This, REFGUID Name, IUnknown *pUnknown);
  FDummy f5; // HRESULT GetPrivateData )(IDXGISwapChain * This, REFGUID Name, UINT *pDataSize, void *pData);
  FDummy f6; // HRESULT GetParent )(IDXGISwapChain * This, REFIID riid, void **ppParent);
  FDummy f7; // HRESULT GetDevice )(IDXGISwapChain * This, REFIID riid, void **ppDevice);
  FPresent            Present;            // HRESULT Present )(IDXGISwapChain * This, UINT SyncInterval, UINT Flags);
  FGetBuffer          GetBuffer;          // HRESULT GetBuffer )(IDXGISwapChain * This, UINT Buffer, REFIID riid, void **ppSurface);
  FSetFullscreenState SetFullscreenState; // HRESULT SetFullscreenState )(IDXGISwapChain * This, BOOL Fullscreen, IDXGIOutput *pTarget);
  FDummy f11; // HRESULT GetFullscreenState )(IDXGISwapChain * This, BOOL *pFullscreen, IDXGIOutput **ppTarget);
  FDummy f12; // HRESULT GetDesc )(IDXGISwapChain * This, DXGI_SWAP_CHAIN_DESC *pDesc);
  FResizeBuffers ResizeBuffers; // HRESULT ResizeBuffers (IDXGISwapChain * This, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
  FDummy f14; // HRESULT ResizeTarget )(IDXGISwapChain * This, DXGI_MODE_DESC *pNewTargetParameters);
  FDummy f15; // HRESULT GetContainingOutput )(IDXGISwapChain * This, IDXGIOutput **ppOutput);
  FDummy f16; // HRESULT GetFrameStatistics )(IDXGISwapChain * This, DXGI_FRAME_STATISTICS *pStats);
  FDummy f17; // HRESULT GetLastPresentCount )(IDXGISwapChain * This, UINT *pLastPresentCount);
}

struct IDXGISwapChain
{
  IDXGISwapChainVtbl *lpVtbl;
}

typedef uint D3D11_USAGE;

struct D3D11_SUBRESOURCE_DATA
{
  byte*  pSysMem;
  UINT   SysMemPitch;
  UINT   SysMemSlicePitch;
}

struct D3D11_TEXTURE2D_DESC
{
  UINT             Width;
  UINT             Height;
  UINT             MipLevels;
  UINT             ArraySize;
  DXGI_FORMAT      Format;
  DXGI_SAMPLE_DESC SampleDesc;
  D3D11_USAGE      Usage;
  UINT             BindFlags;
  UINT             CPUAccessFlags;
  UINT             MiscFlags;
}

typedef uint D3D11_FILL_MODE;
typedef uint D3D11_CULL_MODE;

struct D3D11_RASTERIZER_DESC
{
  D3D11_FILL_MODE FillMode;
  D3D11_CULL_MODE CullMode;
  BOOL            FrontCounterClockwise;
  INT             DepthBias;
  FLOAT           DepthBiasClamp;
  FLOAT           SlopeScaledDepthBias;
  BOOL            DepthClipEnable;
  BOOL            ScissorEnable;
  BOOL            MultisampleEnable;
  BOOL            AntialiasedLineEnable;
}

typedef ID3D11RasterizerState;

typedef [callback] void FRasterizerGetDesc (ID3D11RasterizerState * This, D3D11_RASTERIZER_DESC *pDesc);

struct ID3D11RasterizerStateVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FDummy f3; // void GetDevice (ID3D11RasterizerState * This, ID3D11Device **ppDevice);
  FDummy f4; // HRESULT GetPrivateData (ID3D11RasterizerState * This, REFGUID guid, UINT *pDataSize, byte *pData);
  FDummy f5; // HRESULT SetPrivateData (ID3D11RasterizerState * This, REFGUID guid, UINT DataSize, byte *pData);
  FDummy f6; // HRESULT SetPrivateDataInterface (ID3D11RasterizerState * This, REFGUID guid, IUnknown *pData);
  FRasterizerGetDesc GetDesc; // void GetDesc (ID3D11RasterizerState * This, D3D11_RASTERIZER_DESC *pDesc);
}

struct ID3D11RasterizerState
{
  ID3D11RasterizerStateVtbl* lpVtbl;
}

typedef IUnknown ID3D11Resource;   // temporary
typedef IUnknown ID3D11BlendState; // temporary

typedef ID3D11Texture2D;
typedef [callback] void FFGetDesc (ID3D11Texture2D * This, D3D11_TEXTURE2D_DESC *pDesc);
struct ID3D11Texture2DVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FDummy f3; //  void GetDevice (ID3D11Texture2D * This, ID3D11Device **ppDevice);
  FDummy f4; //  HRESULT GetPrivateData (ID3D11Texture2D * This, REFGUID guid, UINT *pDataSize, byte *pData);
  FDummy f5; //  HRESULT SetPrivateData (ID3D11Texture2D * This, REFGUID guid, UINT DataSize, byte *pData);
  FDummy f6; //  HRESULT SetPrivateDataInterface (ID3D11Texture2D * This, REFGUID guid, IUnknown *pData);
  FDummy f7; //  void GetType (ID3D11Texture2D * This, D3D11_RESOURCE_DIMENSION *pResourceDimension);
  FDummy f8; //  void SetEvictionPriority (ID3D11Texture2D * This, UINT EvictionPriority);
  FDummy f9; //  UINT GetEvictionPriority (ID3D11Texture2D * This);
  FFGetDesc GetDesc; // void GetDesc (ID3D11Texture2D * This, D3D11_TEXTURE2D_DESC *pDesc);
}

struct ID3D11Texture2D
{
  ID3D11Texture2DVtbl *lpVtbl;
}


typedef IUnknown ID3D11RenderTargetView;  // temporary
typedef IUnknown ID3D11DepthStencilView;  // temporary
typedef IUnknown ID3D11DepthStencilState;  // temporary

packed struct D3D11_TEX1D_DSV
{
  UINT MipSlice;
}

packed struct D3D11_TEX1D_ARRAY_DSV
{
  UINT MipSlice;
  UINT FirstArraySlice;
  UINT ArraySize;
}

packed struct D3D11_TEX2D_DSV
{
  UINT MipSlice;
}

packed struct D3D11_TEX2D_ARRAY_DSV
{
  UINT MipSlice;
  UINT FirstArraySlice;
  UINT ArraySize;
}

packed struct D3D11_TEX2DMS_DSV
{
  UINT UnusedField_NothingToDefine;
}

packed struct D3D11_TEX2DMS_ARRAY_DSV
{
  UINT FirstArraySlice;
  UINT ArraySize;
}

union D3D11_TEXUNION
{
  D3D11_TEX1D_DSV         Texture1D;
  D3D11_TEX1D_ARRAY_DSV   Texture1DArray;
  D3D11_TEX2D_DSV         Texture2D;
  D3D11_TEX2D_ARRAY_DSV   Texture2DArray;
  D3D11_TEX2DMS_DSV       Texture2DMS;
  D3D11_TEX2DMS_ARRAY_DSV Texture2DMSArray;
}

typedef uint D3D11_DSV_DIMENSION;

struct D3D11_DEPTH_STENCIL_VIEW_DESC
{
  DXGI_FORMAT         Format;
  D3D11_DSV_DIMENSION ViewDimension;
  UINT                Flags;
  D3D11_TEXUNION      d3d11_texunion;
}

typedef uint D3D11_STENCIL_OP;
typedef uint D3D11_COMPARISON_FUNC;

struct D3D11_DEPTH_STENCILOP_DESC {
  D3D11_STENCIL_OP      StencilFailOp;
  D3D11_STENCIL_OP      StencilDepthFailOp;
  D3D11_STENCIL_OP      StencilPassOp;
  D3D11_COMPARISON_FUNC StencilFunc;
}

typedef uint D3D11_DEPTH_WRITE_MASK;

struct D3D11_DEPTH_STENCIL_DESC
{
  BOOL                       DepthEnable;
  D3D11_DEPTH_WRITE_MASK     DepthWriteMask;
  D3D11_COMPARISON_FUNC      DepthFunc;
  BOOL                       StencilEnable;
  UINT8                      StencilReadMask;
  UINT8                      StencilWriteMask;
  D3D11_DEPTH_STENCILOP_DESC FrontFace;
  D3D11_DEPTH_STENCILOP_DESC BackFace;
}

struct D3D11_VIEWPORT
{
  FLOAT TopLeftX;
  FLOAT TopLeftY;
  FLOAT Width;
  FLOAT Height;
  FLOAT MinDepth;
  FLOAT MaxDepth;
}

struct D3D11_BUFFER_DESC
{
  UINT ByteWidth;
  D3D11_USAGE Usage;
  UINT BindFlags;
  UINT CPUAccessFlags;
  UINT MiscFlags;
  UINT StructureByteStride;
}

typedef ID3D11Buffer;

struct ID3D11BufferVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FDummy f3; // void GetDevice (ID3D11Buffer * This, ID3D11Device **ppDevice);
  FDummy f4; // HRESULT GetPrivateData (ID3D11Buffer *This, REFGUID guid, UINT *pDataSize, byte *pData);
  FDummy f5; // HRESULT SetPrivateData (ID3D11Buffer * This, REFGUID guid, UINT DataSize, byte *pData);
  FDummy f6; // HRESULT SetPrivateDataInterface (ID3D11Buffer * This, REFGUID guid, IUnknown *pData);
  FDummy f7; // void GetType (ID3D11Buffer * This, D3D11_RESOURCE_DIMENSION *pResourceDimension);
  FDummy f8; // void SetEvictionPriority (ID3D11Buffer * This, UINT EvictionPriority);
  FDummy f9; // UINT GetEvictionPriority (ID3D11Buffer * This);
  FDummy f10; // void GetDesc (ID3D11Buffer* This, D3D11_BUFFER_DESC *pDesc);
}

struct ID3D11Buffer
{
  ID3D11BufferVtbl *lpVtbl;
}

typedef ID3D11InputLayout;

struct ID3D11InputLayoutVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FDummy f3; // void GetDevice (ID3D11InputLayout * This, ID3D11Device **ppDevice);
  FDummy f4; // HRESULT GetPrivateData (ID3D11InputLayout * This, REFGUID guid, UINT *pDataSize, byte *pData);
  FDummy f5; // HRESULT SetPrivateData (ID3D11InputLayout * This, REFGUID guid, UINT DataSize, byte *pData);
  FDummy f6; // HRESULT SetPrivateDataInterface (ID3D11InputLayout * This, REFGUID guid, IUnknown *pData);
}

struct ID3D11InputLayout
{
  ID3D11InputLayoutVtbl *lpVtbl;
}


enum D3D11_INPUT_CLASSIFICATION {D3D11_INPUT_PER_VERTEX_DATA, D3D11_INPUT_PER_INSTANCE_DATA};

struct D3D11_INPUT_ELEMENT_DESC
{
  LPCSTR SemanticName;
  UINT SemanticIndex;
  DXGI_FORMAT Format;
  UINT InputSlot;
  UINT AlignedByteOffset;
  D3D11_INPUT_CLASSIFICATION InputSlotClass;
  UINT InstanceDataStepRate;
}

struct D3D11_SO_DECLARATION_ENTRY
{
  UINT Stream;
  LPCSTR SemanticName;
  UINT SemanticIndex;
  BYTE StartComponent;
  BYTE ComponentCount;
  BYTE OutputSlot;
}

typedef uint D3D11_FILTER;
typedef uint D3D11_TEXTURE_ADDRESS_MODE;

struct D3D11_SAMPLER_DESC
{
  D3D11_FILTER Filter;
  D3D11_TEXTURE_ADDRESS_MODE AddressU;
  D3D11_TEXTURE_ADDRESS_MODE AddressV;
  D3D11_TEXTURE_ADDRESS_MODE AddressW;
  FLOAT MipLODBias;
  UINT MaxAnisotropy;
  D3D11_COMPARISON_FUNC ComparisonFunc;
  FLOAT BorderColor[ 4 ];
  FLOAT MinLOD;
  FLOAT MaxLOD;
}

enum D3D11_MAP {BAD_MAP, D3D11_MAP_READ, D3D11_MAP_WRITE, D3D11_MAP_READ_WRITE,
                D3D11_MAP_WRITE_DISCARD, D3D11_MAP_WRITE_NO_OVERWRITE};

packed struct D3D11_TEX2D_SRV
{
  UINT MostDetailedMip;
  UINT MipLevels;
}

packed struct D3D11_TEX2D_ARRAY_SRV
{
  UINT MostDetailedMip;
  UINT MipLevels;
  UINT FirstArraySlice;
  UINT ArraySize;
}

packed struct D3D11_TEX2DMS_SRV
{
  UINT UnusedField_NothingToDefine;
}

packed struct D3D11_TEX2DMS_ARRAY_SRV
{
  UINT FirstArraySlice;
  UINT ArraySize;
}

union D3D11_SHADER_RESOURCE_VIEW_DESC_array
{
//$  D3D11_BUFFER_SRV Buffer;
//$  D3D11_TEX1D_SRV Texture1D;
//$  D3D11_TEX1D_ARRAY_SRV Texture1DArray;
  D3D11_TEX2D_SRV         Texture2D;
  D3D11_TEX2D_ARRAY_SRV   Texture2DArray;
  D3D11_TEX2DMS_SRV       Texture2DMS;
  D3D11_TEX2DMS_ARRAY_SRV Texture2DMSArray;
//$  D3D11_TEX3D_SRV Texture3D;
//$  D3D11_TEXCUBE_SRV TextureCube;
//$  D3D11_TEXCUBE_ARRAY_SRV TextureCubeArray;
//$  D3D11_BUFFEREX_SRV BufferEx;
}

typedef uint D3D11_SRV_DIMENSION;

struct D3D11_SHADER_RESOURCE_VIEW_DESC
{
  DXGI_FORMAT Format;
  D3D11_SRV_DIMENSION ViewDimension;
  D3D11_SHADER_RESOURCE_VIEW_DESC_array array;
}

typedef uint D3D11_RTV_DIMENSION;

const D3D11_RTV_DIMENSION  D3D11_RTV_DIMENSION_UNKNOWN = 0;
const D3D11_RTV_DIMENSION  D3D11_RTV_DIMENSION_BUFFER = 1;
const D3D11_RTV_DIMENSION  D3D11_RTV_DIMENSION_TEXTURE1D = 2;
const D3D11_RTV_DIMENSION  D3D11_RTV_DIMENSION_TEXTURE1DARRAY = 3;
const D3D11_RTV_DIMENSION  D3D11_RTV_DIMENSION_TEXTURE2D = 4;
const D3D11_RTV_DIMENSION  D3D11_RTV_DIMENSION_TEXTURE2DARRAY = 5;
const D3D11_RTV_DIMENSION  D3D11_RTV_DIMENSION_TEXTURE2DMS = 6;
const D3D11_RTV_DIMENSION  D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY = 7;
const D3D11_RTV_DIMENSION  D3D11_RTV_DIMENSION_TEXTURE3D = 8;


packed struct D3D11_TEX2D_RTV
{
  UINT MipSlice;
}

packed struct D3D11_TEX2D_ARRAY_RTV
{
  UINT MipSlice;
  UINT FirstArraySlice;
  UINT ArraySize;
}

packed struct D3D11_TEX2DMS_RTV
{
  UINT UnusedField_NothingToDefine;
}

packed struct D3D11_TEX2DMS_ARRAY_RTV
{
  UINT FirstArraySlice;
  UINT ArraySize;
}

packed struct D3D11_TEX3D_RTV
{
  UINT MipSlice;
  UINT FirstWSlice;
  UINT WSize;
}

union D3D11_RENDER_TARGET_VIEW_DESC_union
{
//    D3D11_BUFFER_RTV        Buffer;
//    D3D11_TEX1D_RTV         Texture1D;
//    D3D11_TEX1D_ARRAY_RTV   Texture1DArray;
    D3D11_TEX2D_RTV         Texture2D;
    D3D11_TEX2D_ARRAY_RTV   Texture2DArray;
    D3D11_TEX2DMS_RTV       Texture2DMS;
    D3D11_TEX2DMS_ARRAY_RTV Texture2DMSArray;
    D3D11_TEX3D_RTV         Texture3D;
}

struct D3D11_RENDER_TARGET_VIEW_DESC
{
  DXGI_FORMAT         Format;
  D3D11_RTV_DIMENSION ViewDimension;
  D3D11_RENDER_TARGET_VIEW_DESC_union u;
}


typedef IUnknown ID3D11ClassLinkage;

typedef IUnknown ID3D11VertexShader;
typedef IUnknown ID3D11GeometryShader;
typedef IUnknown ID3D11PixelShader;
typedef IUnknown ID3D11HullShader;
typedef IUnknown ID3D11DomainShader;
typedef IUnknown ID3D11SamplerState;

typedef ID3D11ShaderResourceView;
typedef [callback] void FGetResource (ID3D11ShaderResourceView * This, ID3D11Resource **ppResource);

struct ID3D11ShaderResourceViewVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FDummy f3;  // void GetDevice (ID3D11ShaderResourceView * This, ID3D11Device **ppDevice);
  FDummy f4;  // HRESULT GetPrivateData (ID3D11ShaderResourceView * This, REFGUID guid, UINT *pDataSize, byte *pData);
  FDummy f5;  // HRESULT SetPrivateData (ID3D11ShaderResourceView * This, REFGUID guid, UINT DataSize, byte* pData);
  FDummy f6;  // HRESULT SetPrivateDataInterface (ID3D11ShaderResourceView * This, REFGUID guid, IUnknown *pData);
  FGetResource GetResource;  // void GetResource (ID3D11ShaderResourceView * This, ID3D11Resource **ppResource);
  FDummy f8;  // void GetDesc (ID3D11ShaderResourceView * This, D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc);
}

struct ID3D11ShaderResourceView
{
  ID3D11ShaderResourceViewVtbl *lpVtbl;
}


typedef IUnknown ID3D11ClassInstance;

struct D3D11_MAPPED_SUBRESOURCE
{
  byte* pData;
  UINT  RowPitch;
  UINT  DepthPitch;
}

typedef ID3D11Device;
typedef [callback] HRESULT FCreateTexture2D(ID3D11Device * This, D3D11_TEXTURE2D_DESC *pDesc, D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D **ppTexture2D);
typedef [callback] HRESULT FCreateRenderTargetView(ID3D11Device * This, ID3D11Resource *pResource, D3D11_RENDER_TARGET_VIEW_DESC *pDesc, ID3D11RenderTargetView **ppRTView);
typedef [callback] HRESULT FCreateDepthStencilView(ID3D11Device * This, ID3D11Resource *pResource, D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D11DepthStencilView **ppDepthStencilView);
typedef [callback] HRESULT FCreateDepthStencilState(ID3D11Device * This, D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D11DepthStencilState **ppDepthStencilState);
typedef [callback] HRESULT FCreateRasterizerState(ID3D11Device * This, D3D11_RASTERIZER_DESC *pRasterizerDesc, ID3D11RasterizerState **ppRasterizerState);
typedef [callback] HRESULT FCreateBuffer(ID3D11Device * This, D3D11_BUFFER_DESC *pDesc, D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Buffer **ppBuffer);
typedef [callback] HRESULT FCreateInputLayout(ID3D11Device * This, D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements, byte *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout **ppInputLayout);
typedef [callback] HRESULT FCreateVertexShader(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11VertexShader **ppVertexShader);
typedef [callback] HRESULT FCreateGeometryShader(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader);
typedef [callback] HRESULT FCreateGeometryShaderWithStreamOutput(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, D3D11_SO_DECLARATION_ENTRY *pSODeclaration,   UINT NumEntries,    UINT *pBufferStrides,   UINT NumStrides,   UINT RasterizedStream,   ID3D11ClassLinkage *pClassLinkage,   ID3D11GeometryShader **ppGeometryShader);
typedef [callback] HRESULT FCreatePixelShader(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11PixelShader **ppPixelShader);
typedef [callback] HRESULT FCreateHullShader(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader);
typedef [callback] HRESULT FCreateDomainShader(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11DomainShader **ppDomainShader);
typedef [callback] HRESULT FCreateSamplerState(ID3D11Device * This, D3D11_SAMPLER_DESC *pSamplerDesc, ID3D11SamplerState **ppSamplerState);
typedef [callback] HRESULT FCreateShaderResourceView(ID3D11Device * This, ID3D11Resource *pResource, D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D11ShaderResourceView **ppSRView);
typedef [callback] HRESULT FCreateBlendState(ID3D11Device * This, D3D11_BLEND_DESC *pBlendStateDesc, ID3D11BlendState **ppBlendState);
typedef [callback] HRESULT FCheckMultisampleQualityLevels(ID3D11Device * This, DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels);
typedef [callback] HRESULT FGetDeviceRemovedReason(ID3D11Device * This);

struct ID3D11DeviceVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FCreateBuffer CreateBuffer; //HRESULT CreateBuffer(ID3D11Device * This, D3D11_BUFFER_DESC *pDesc, D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Buffer **ppBuffer);
  FDummy f4; //HRESULT CreateTexture1D(ID3D11Device * This, D3D11_TEXTURE1D_DESC *pDesc, D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture1D **ppTexture1D);
  FCreateTexture2D CreateTexture2D; //HRESULT CreateTexture2D(ID3D11Device * This, D3D11_TEXTURE2D_DESC *pDesc, D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D **ppTexture2D);
  FDummy f6; //HRESULT CreateTexture3D(ID3D11Device * This, D3D11_TEXTURE3D_DESC *pDesc, D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D **ppTexture3D);
  FCreateShaderResourceView CreateShaderResourceView; //HRESULT CreateShaderResourceView(ID3D11Device * This, ID3D11Resource *pResource, D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D11ShaderResourceView **ppSRView);
  FDummy f8; //HRESULT CreateUnorderedAccessView(ID3D11Device * This, ID3D11Resource *pResource, D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc, ID3D11UnorderedAccessView **ppUAView);
  FCreateRenderTargetView CreateRenderTargetView; //HRESULT CreateRenderTargetView(ID3D11Device * This, ID3D11Resource *pResource, D3D11_RENDER_TARGET_VIEW_DESC *pDesc, ID3D11RenderTargetView **ppRTView);
  FCreateDepthStencilView CreateDepthStencilView; //HRESULT CreateDepthStencilView(ID3D11Device * This, ID3D11Resource *pResource, D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D11DepthStencilView **ppDepthStencilView);
  FCreateInputLayout CreateInputLayout; //HRESULT CreateInputLayout(ID3D11Device * This, D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements, byte *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout **ppInputLayout);
  FCreateVertexShader CreateVertexShader; //HRESULT CreateVertexShader(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11VertexShader **ppVertexShader);
  FCreateGeometryShader CreateGeometryShader; //HRESULT CreateGeometryShader(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader);
  FCreateGeometryShaderWithStreamOutput CreateGeometryShaderWithStreamOutput; //HRESULT CreateGeometryShaderWithStreamOutput(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, D3D11_SO_DECLARATION_ENTRY *pSODeclaration,   UINT NumEntries,    UINT *pBufferStrides,   UINT NumStrides,   UINT RasterizedStream,   ID3D11ClassLinkage *pClassLinkage,   ID3D11GeometryShader **ppGeometryShader);
  FCreatePixelShader CreatePixelShader; //HRESULT CreatePixelShader(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11PixelShader **ppPixelShader);
  FCreateHullShader CreateHullShader; //HRESULT CreateHullShader(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader);
  FCreateDomainShader CreateDomainShader; //HRESULT CreateDomainShader(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11DomainShader **ppDomainShader);

  FDummy f18; //HRESULT CreateComputeShader(ID3D11Device * This, byte *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage,  ID3D11ComputeShader **ppComputeShader);
  FDummy f19; //HRESULT CreateClassLinkage(ID3D11Device * This, ID3D11ClassLinkage **ppLinkage);
  FCreateBlendState CreateBlendState; //HRESULT CreateBlendState(ID3D11Device * This, D3D11_BLEND_DESC *pBlendStateDesc, ID3D11BlendState **ppBlendState);
  FCreateDepthStencilState CreateDepthStencilState; //HRESULT CreateDepthStencilState(ID3D11Device * This, D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D11DepthStencilState **ppDepthStencilState);
  FCreateRasterizerState   CreateRasterizerState; //HRESULT CreateRasterizerState(ID3D11Device * This, D3D11_RASTERIZER_DESC *pRasterizerDesc, ID3D11RasterizerState **ppRasterizerState);
  FCreateSamplerState CreateSamplerState; //HRESULT CreateSamplerState(ID3D11Device * This, D3D11_SAMPLER_DESC *pSamplerDesc, ID3D11SamplerState **ppSamplerState);
  FDummy f24; //HRESULT CreateQuery(ID3D11Device * This, D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery);
  FDummy f25; //HRESULT CreatePredicate(ID3D11Device * This, D3D11_QUERY_DESC *pPredicateDesc, ID3D11Predicate **ppPredicate);
  FDummy f26; //HRESULT CreateCounter(ID3D11Device * This, D3D11_COUNTER_DESC *pCounterDesc, ID3D11Counter **ppCounter);
  FDummy f27; //HRESULT CreateDeferredContext(ID3D11Device * This, UINT ContextFlags, ID3D11DeviceContext **ppDeferredContext);
  FDummy f28; //HRESULT OpenSharedResource(ID3D11Device * This, HANDLE hResource, REFIID ReturnedInterface, byte **ppResource);
  FDummy f29; //HRESULT CheckFormatSupport(ID3D11Device * This, DXGI_FORMAT Format, UINT *pFormatSupport);
  FCheckMultisampleQualityLevels CheckMultisampleQualityLevels; //HRESULT CheckMultisampleQualityLevels(ID3D11Device * This, DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels);
  FDummy f31; //void CheckCounterInfo(ID3D11Device * This, D3D11_COUNTER_INFO *pCounterInfo);
  FDummy f32; //HRESULT CheckCounter(ID3D11Device * This, D3D11_COUNTER_DESC *pDesc, D3D11_COUNTER_TYPE *pType, UINT *pActiveCounters, LPSTR szName,  UINT *pNameLength,   LPSTR szUnits,   UINT *pUnitsLength,   LPSTR szDescription,   UINT *pDescriptionLength);
  FDummy f33; //HRESULT CheckFeatureSupport(ID3D11Device * This, D3D11_FEATURE Feature, byte *pFeatureSupportData, UINT FeatureSupportDataSize);
  FDummy f34; //HRESULT GetPrivateData(ID3D11Device * This, REFGUID guid, UINT *pDataSize, byte *pData);
  FDummy f35; //HRESULT SetPrivateData(ID3D11Device * This, REFGUID guid, UINT DataSize, byte *pData);
  FDummy f36; //HRESULT SetPrivateDataInterface(ID3D11Device * This, REFGUID guid, IUnknown *pData);
  FDummy f37; //D3D_FEATURE_LEVEL GetFeatureLevel(ID3D11Device * This);
  FDummy f38; //UINT GetCreationFlags(ID3D11Device * This);
  FGetDeviceRemovedReason GetDeviceRemovedReason; //HRESULT GetDeviceRemovedReason(ID3D11Device * This);
  FDummy f40; //void GetImmediateContext(ID3D11Device * This, ID3D11DeviceContext **ppImmediateContext);
  FDummy f41; //HRESULT SetExceptionMode(ID3D11Device * This, UINT RaiseFlags);
  FDummy f42; //UINT GetExceptionMode(ID3D11Device * This);
}

struct ID3D11Device
{
  ID3D11DeviceVtbl *lpVtbl;
}


const uint D3D11_SRV_DIMENSION_TEXTURE2D = 4;
const uint D3D11_SRV_DIMENSION_TEXTURE2DARRAY = 5;
const uint D3D11_SRV_DIMENSION_TEXTURE2DMS        = 6;
const uint D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY   = 7;
const uint D3D11_SRV_DIMENSION_TEXTURE3D          = 8;
const uint D3D11_SRV_DIMENSION_TEXTURECUBE        = 9;
const uint D3D11_SRV_DIMENSION_TEXTURECUBEARRAY   = 10;
const uint D3D11_SRV_DIMENSION_BUFFEREX           = 11;

struct D3D11_BOX
{
  UINT left;
  UINT top;
  UINT front;
  UINT right;
  UINT bottom;
  UINT back;
}

typedef ID3D11DeviceContext;

typedef [callback] void FOMSetDepthStencilState (ID3D11DeviceContext * This, ID3D11DepthStencilState *pDepthStencilState,  UINT StencilRef);
typedef [callback] void FOMSetRenderTargets (ID3D11DeviceContext * This, UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews,  ID3D11DepthStencilView *pDepthStencilView);
typedef [callback] void FRSSetState (ID3D11DeviceContext * This, ID3D11RasterizerState *pRasterizerState);
typedef [callback] void FRSSetViewports (ID3D11DeviceContext * This, UINT NumViewports, D3D11_VIEWPORT *pViewports);
typedef [callback] void FClearRenderTargetView (ID3D11DeviceContext * This, ID3D11RenderTargetView *pRenderTargetView, FLOAT ColorRGBA[ 4 ]);
typedef [callback] void FClearDepthStencilView (ID3D11DeviceContext * This, ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil);
typedef [callback] void FIASetVertexBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers,  UINT *pStrides,  UINT *pOffsets);
typedef [callback] void FIASetIndexBuffer (ID3D11DeviceContext * This, ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset);
typedef [callback] void FIASetPrimitiveTopology (ID3D11DeviceContext * This, D3D11_PRIMITIVE_TOPOLOGY Topology);
typedef [callback] void FPSSetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
typedef [callback] void FIASetInputLayout (ID3D11DeviceContext * This, ID3D11InputLayout *pInputLayout);
typedef [callback] void FVSSetShader (ID3D11DeviceContext * This, ID3D11VertexShader *pVertexShader, ID3D11ClassInstance** ppClassInstances,  UINT NumClassInstances);
typedef [callback] void FPSSetShader (ID3D11DeviceContext * This, ID3D11PixelShader *pPixelShader, ID3D11ClassInstance** ppClassInstances,    UINT NumClassInstances);
typedef [callback] void FPSSetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
typedef [callback] HRESULT FMap (ID3D11DeviceContext * This, ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType,  UINT MapFlags,   D3D11_MAPPED_SUBRESOURCE *pMappedResource);
typedef [callback] void FUnmap (ID3D11DeviceContext * This, ID3D11Resource *pResource, UINT Subresource);
typedef [callback] void FVSSetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
typedef [callback] void FDrawIndexed (ID3D11DeviceContext * This, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
typedef [callback] void FDraw (ID3D11DeviceContext * This, UINT VertexCount, UINT StartVertexLocation);
typedef [callback] void FUpdateSubresource (ID3D11DeviceContext * This, ID3D11Resource *pDstResource, UINT DstSubresource, D3D11_BOX *pDstBox, byte* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
typedef [callback] void FGenerateMips (ID3D11DeviceContext * This, ID3D11ShaderResourceView *pShaderResourceView);
typedef [callback] void FPSSetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
typedef [callback] void FOMSetBlendState (ID3D11DeviceContext * This, ID3D11BlendState *pBlendState,  FLOAT BlendFactor[ 4 ], UINT SampleMask);
typedef [callback] void FClearState (ID3D11DeviceContext * This);
typedef [callback] void FCopySubresourceRegion (ID3D11DeviceContext * This, ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *pSrcResource, UINT SrcSubresource, D3D11_BOX *pSrcBox);
typedef [callback] void FCopyResource (ID3D11DeviceContext * This,ID3D11Resource *pDstResource, ID3D11Resource *pSrcResource);
typedef [callback] void FGSSetShader (ID3D11DeviceContext * This, ID3D11GeometryShader *pShader, ID3D11ClassInstance** ppClassInstances,  UINT NumClassInstances);
typedef [callback] void FGSSetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
typedef [callback] void FDrawIndexedInstanced (ID3D11DeviceContext * This, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,  INT BaseVertexLocation,   UINT StartInstanceLocation);
typedef [callback] void FDrawInstanced (ID3D11DeviceContext * This, UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation,  UINT StartInstanceLocation);
typedef [callback] void FResolveSubresource (ID3D11DeviceContext * This, ID3D11Resource *pDstResource, UINT DstSubresource, ID3D11Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format);
typedef [callback] void FVSSetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
typedef [callback] void FVSSetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);

struct ID3D11DeviceContextVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FDummy f3; //void GetDevice(ID3D11DeviceContext * This,ID3D11Device **ppDevice);
  FDummy f4; //HRESULT GetPrivateData (ID3D11DeviceContext * This,REFGUID guid, UINT *pDataSize, byte* pData);
  FDummy f5; //HRESULT SetPrivateData (ID3D11DeviceContext * This, REFGUID guid, UINT DataSize, byte* pData);
  FDummy f6; //HRESULT SetPrivateDataInterface (ID3D11DeviceContext * This, REFGUID guid, IUnknown *pData);
  FVSSetConstantBuffers VSSetConstantBuffers; //void VSSetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
  FPSSetShaderResources PSSetShaderResources; //void PSSetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
  FPSSetShader PSSetShader; //void PSSetShader (ID3D11DeviceContext * This, ID3D11PixelShader *pPixelShader, ID3D11ClassInstance** ppClassInstances,    UINT NumClassInstances);
  FPSSetSamplers PSSetSamplers; //void PSSetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
  FVSSetShader VSSetShader; //void VSSetShader (ID3D11DeviceContext * This, ID3D11VertexShader *pVertexShader, ID3D11ClassInstance** ppClassInstances,  UINT NumClassInstances);
  FDrawIndexed DrawIndexed; //void DrawIndexed (ID3D11DeviceContext * This, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
  FDraw Draw; //void Draw (ID3D11DeviceContext * This,UINT VertexCount, UINT StartVertexLocation);
  FMap Map; //HRESULT Map (ID3D11DeviceContext * This, ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType,  UINT MapFlags,   D3D11_MAPPED_SUBRESOURCE *pMappedResource);
  FUnmap Unmap; //void Unmap (ID3D11DeviceContext * This, ID3D11Resource *pResource, UINT Subresource);
  FPSSetConstantBuffers PSSetConstantBuffers; //void PSSetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
  FIASetInputLayout IASetInputLayout; //void IASetInputLayout (ID3D11DeviceContext * This, ID3D11InputLayout *pInputLayout);
  FIASetVertexBuffers IASetVertexBuffers; //void IASetVertexBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers,  UINT *pStrides,  UINT *pOffsets);
  FIASetIndexBuffer IASetIndexBuffer; //void IASetIndexBuffer (ID3D11DeviceContext * This, ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset);
  FDrawIndexedInstanced DrawIndexedInstanced; //void DrawIndexedInstanced (ID3D11DeviceContext * This, UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,  INT BaseVertexLocation,   UINT StartInstanceLocation);
  FDrawInstanced DrawInstanced; //void DrawInstanced (ID3D11DeviceContext * This, UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation,  UINT StartInstanceLocation);
  FGSSetConstantBuffers GSSetConstantBuffers; //void GSSetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
  FGSSetShader GSSetShader; //void GSSetShader (ID3D11DeviceContext * This, ID3D11GeometryShader *pShader, ID3D11ClassInstance** ppClassInstances,  UINT NumClassInstances);
  FIASetPrimitiveTopology IASetPrimitiveTopology; //void IASetPrimitiveTopology (ID3D11DeviceContext * This, D3D11_PRIMITIVE_TOPOLOGY Topology);
  FVSSetShaderResources VSSetShaderResources; //void VSSetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
  FVSSetSamplers VSSetSamplers; //void VSSetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
  FDummy f27; //void Begin (ID3D11DeviceContext * This, ID3D11Asynchronous *pAsync);
  FDummy f28; //void End (ID3D11DeviceContext * This, ID3D11Asynchronous *pAsync);
  FDummy f29; //HRESULT GetData (ID3D11DeviceContext * This, ID3D11Asynchronous *pAsync, byte* pData, UINT DataSize, UINT GetDataFlags);
  FDummy f30; //void SetPredication (ID3D11DeviceContext * This, ID3D11Predicate *pPredicate,  BOOL PredicateValue);
  FDummy f31; //void GSSetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
  FDummy f32; //void GSSetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
  FOMSetRenderTargets OMSetRenderTargets; //void OMSetRenderTargets (ID3D11DeviceContext * This, UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews,  ID3D11DepthStencilView *pDepthStencilView);
  FDummy f34; //void OMSetRenderTargetsAndUnorderedAccessViews (ID3D11DeviceContext * This, UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews,  ID3D11DepthStencilView *pDepthStencilView,      UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews, UINT *pUAVInitialCounts);
  FOMSetBlendState OMSetBlendState; //void OMSetBlendState (ID3D11DeviceContext * This, ID3D11BlendState *pBlendState,  FLOAT BlendFactor[ 4 ], UINT SampleMask);
  FOMSetDepthStencilState OMSetDepthStencilState; //void OMSetDepthStencilState ( ID3D11DeviceContext * This, ID3D11DepthStencilState *pDepthStencilState,  UINT StencilRef);
  FDummy f37; //void SOSetTargets (ID3D11DeviceContext * This, UINT NumBuffers, ID3D11Buffer** ppSOTargets, UINT *pOffsets);
  FDummy f38; //void DrawAuto (ID3D11DeviceContext * This);
  FDummy f39; //void DrawIndexedInstancedIndirect (ID3D11DeviceContext * This, ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs);
  FDummy f40; //void DrawInstancedIndirect (ID3D11DeviceContext * This, ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs);
  FDummy f41; //void Dispatch (ID3D11DeviceContext * This, UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ);
  FDummy f42; //void DispatchIndirect (ID3D11DeviceContext * This, ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs);
  FRSSetState RSSetState; //void RSSetState (ID3D11DeviceContext * This, ID3D11RasterizerState *pRasterizerState);
  FRSSetViewports RSSetViewports; //void RSSetViewports (ID3D11DeviceContext * This, UINT NumViewports, D3D11_VIEWPORT *pViewports);
  FDummy f45; //void RSSetScissorRects (ID3D11DeviceContext * This, UINT NumRects, D3D11_RECT *pRects);
  FCopySubresourceRegion CopySubresourceRegion; //void CopySubresourceRegion (ID3D11DeviceContext * This, ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *pSrcResource, UINT SrcSubresource, D3D11_BOX *pSrcBox);
  FCopyResource CopyResource; //void CopyResource (ID3D11DeviceContext * This,ID3D11Resource *pDstResource, ID3D11Resource *pSrcResource);
  FUpdateSubresource UpdateSubresource; //void UpdateSubresource (ID3D11DeviceContext * This, ID3D11Resource *pDstResource, UINT DstSubresource, D3D11_BOX *pDstBox, byte* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
  FDummy f49; //void CopyStructureCount (ID3D11DeviceContext * This, ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView);
  FClearRenderTargetView ClearRenderTargetView; //void ClearRenderTargetView (ID3D11DeviceContext * This, ID3D11RenderTargetView *pRenderTargetView, FLOAT ColorRGBA[ 4 ]);
  FDummy f51; //void ClearUnorderedAccessViewUint (ID3D11DeviceContext * This, ID3D11UnorderedAccessView *pUnorderedAccessView, UINT Values[ 4 ]);
  FDummy f52; //void ClearUnorderedAccessViewFloat (ID3D11DeviceContext * This, ID3D11UnorderedAccessView *pUnorderedAccessView, FLOAT Values[ 4 ]);
  FClearDepthStencilView ClearDepthStencilView; //void ClearDepthStencilView (ID3D11DeviceContext * This, ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil);
  FGenerateMips GenerateMips; //void GenerateMips (ID3D11DeviceContext * This, ID3D11ShaderResourceView *pShaderResourceView);
  FDummy f55; //void SetResourceMinLOD (ID3D11DeviceContext * This, ID3D11Resource *pResource, FLOAT MinLOD);
  FDummy f56; //FLOAT GetResourceMinLOD (ID3D11DeviceContext * This, ID3D11Resource *pResource);
  FResolveSubresource ResolveSubresource; // ResolveSubresource (ID3D11DeviceContext * This, ID3D11Resource *pDstResource, UINT DstSubresource, ID3D11Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format);
  FDummy f58; //void ExecuteCommandList (ID3D11DeviceContext * This, ID3D11CommandList *pCommandList, BOOL RestoreContextState);
  FDummy f59; //void HSSetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
  FDummy f60; //void HSSetShader (ID3D11DeviceContext * This, ID3D11HullShader *pHullShader, ID3D11ClassInstance** ppClassInstances, UINT NumClassInstances);
  FDummy f61; //void HSSetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
  FDummy f62; //void HSSetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
  FDummy f63; //void DSSetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
  FDummy f64; //void DSSetShader (ID3D11DeviceContext * This, ID3D11DomainShader *pDomainShader, ID3D11ClassInstance** ppClassInstances, UINT NumClassInstances);
  FDummy f65; //void DSSetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
  FDummy f66; //void DSSetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
  FDummy f67; //void CSSetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews);
  FDummy f68; //void CSSetUnorderedAccessViews (ID3D11DeviceContext * This, UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews, UINT *pUAVInitialCounts);
  FDummy f69; //void CSSetShader (ID3D11DeviceContext * This, ID3D11ComputeShader *pComputeShader, ID3D11ClassInstance** ppClassInstances, UINT NumClassInstances);
  FDummy f70; //void CSSetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers);
  FDummy f71; //void CSSetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers);
  FDummy f72; //void VSGetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers);
  FDummy f73; //void PSGetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews);
  FDummy f74; //void PSGetShader (ID3D11DeviceContext * This, ID3D11PixelShader **ppPixelShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances);
  FDummy f75; //void PSGetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers);
  FDummy f76; //void VSGetShader (ID3D11DeviceContext * This, ID3D11VertexShader **ppVertexShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances);
  FDummy f77; //void PSGetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers);
  FDummy f78; //void IAGetInputLayout (ID3D11DeviceContext * This, ID3D11InputLayout **ppInputLayout);
  FDummy f79; //void IAGetVertexBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppVertexBuffers, UINT *pStrides, UINT *pOffsets);
  FDummy f80; //void IAGetIndexBuffer (ID3D11DeviceContext * This, ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset);
  FDummy f81; //void GSGetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers);
  FDummy f82; //void GSGetShader (ID3D11DeviceContext * This, ID3D11GeometryShader **ppGeometryShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances);
  FDummy f83; //void IAGetPrimitiveTopology (ID3D11DeviceContext * This, D3D11_PRIMITIVE_TOPOLOGY *pTopology);
  FDummy f84; //void VSGetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews,ID3D11ShaderResourceView **ppShaderResourceViews);
  FDummy f85; //void VSGetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers);
  FDummy f86; //void GetPredication (ID3D11DeviceContext * This, ID3D11Predicate **ppPredicate, BOOL *pPredicateValue);
  FDummy f87; //void GSGetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews);
  FDummy f88; //void GSGetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers);
  FDummy f89; //void OMGetRenderTargets (ID3D11DeviceContext * This, UINT NumViews, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView);
  FDummy f90; //void OMGetRenderTargetsAndUnorderedAccessViews (ID3D11DeviceContext * This, UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews);
  FDummy f91; //void OMGetBlendState (ID3D11DeviceContext * This, ID3D11BlendState **ppBlendState, FLOAT BlendFactor[ 4 ], UINT *pSampleMask);
  FDummy f92; //void OMGetDepthStencilState (ID3D11DeviceContext * This, ID3D11DepthStencilState **ppDepthStencilState, UINT *pStencilRef);
  FDummy f93; //void SOGetTargets (ID3D11DeviceContext * This, UINT NumBuffers, ID3D11Buffer **ppSOTargets);
  FDummy f94; //void RSGetState (ID3D11DeviceContext * This, ID3D11RasterizerState **ppRasterizerState);
  FDummy f95; //void RSGetViewports (ID3D11DeviceContext * This, UINT *pNumViewports, D3D11_VIEWPORT *pViewports);
  FDummy f96; //void RSGetScissorRects (ID3D11DeviceContext * This, UINT *pNumRects, D3D11_RECT *pRects);
  FDummy f97; //void HSGetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews);
  FDummy f98; //void HSGetShader (ID3D11DeviceContext * This, ID3D11HullShader **ppHullShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances);
  FDummy f99; //void HSGetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers);
  FDummy f100; //void HSGetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers);
  FDummy f101; //void DSGetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews);
  FDummy f102; //void DSGetShader (ID3D11DeviceContext * This, ID3D11DomainShader **ppDomainShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances);
  FDummy f103; //void DSGetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers);
  FDummy f104; //void DSGetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers);
  FDummy f105; //void CSGetShaderResources (ID3D11DeviceContext * This, UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews);
  FDummy f106; //void CSGetUnorderedAccessViews (ID3D11DeviceContext * This, UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews);
  FDummy f107; //void CSGetShader ( ID3D11DeviceContext * This, ID3D11ComputeShader **ppComputeShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances);
  FDummy f108; //void CSGetSamplers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers);
  FDummy f109; //void CSGetConstantBuffers (ID3D11DeviceContext * This, UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers);
  FClearState ClearState; //void ClearState (ID3D11DeviceContext * This);
  FDummy f111; //void Flush (ID3D11DeviceContext * This);
  FDummy f112; //D3D11_DEVICE_CONTEXT_TYPE GetType (ID3D11DeviceContext * This);
  FDummy f113; //UINT GetContextFlags (ID3D11DeviceContext * This);
  FDummy f114; //HRESULT FinishCommandList (ID3D11DeviceContext * This, BOOL RestoreDeferredContextState, ID3D11CommandList **ppCommandList);
}

struct ID3D11DeviceContext
{
  ID3D11DeviceContextVtbl *lpVtbl;
}

typedef [callback]
HRESULT FD3D11CreateDeviceAndSwapChain
   (IDXGIAdapter*         pAdapter,
    D3D_DRIVER_TYPE       DriverType,
    HMODULE               Software,
    UINT                  Flags,
    D3D_FEATURE_LEVEL*    pFeatureLevels,
    UINT                  FeatureLevels,
    UINT                  SDKVersion,
    DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
    IDXGISwapChain**      ppSwapChain,
    ID3D11Device**        ppDevice,
    D3D_FEATURE_LEVEL*    pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext);

typedef [callback]
HRESULT D3D11CreateDevice
  (IDXGIAdapter*   pAdapter,
   D3D_DRIVER_TYPE DriverType,
   HMODULE         Software,
   UINT            Flags,
   D3D_FEATURE_LEVEL *pFeatureLevels,
   UINT            FeatureLevels,
   UINT            SDKVersion,
   ID3D11Device        **ppDevice,
   D3D_FEATURE_LEVEL   *pFeatureLevel,
   ID3D11DeviceContext **ppImmediateContext);

typedef ID3D10Blob;

typedef [callback] LPVOID FGetBufferPointer (ID3D10Blob * This);
typedef [callback] SIZE_T FGetBufferSize (ID3D10Blob * This);

struct ID3D10BlobVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FGetBufferPointer GetBufferPointer;
  FGetBufferSize GetBufferSize;
}

struct ID3D10Blob
{
  ID3D10BlobVtbl *lpVtbl;
}

typedef ID3D10Blob ID3DBlob;

typedef byte D3D_SHADER_MACRO;
typedef byte ID3DInclude;

const uint D3DCOMPILE_ENABLE_STRICTNESS = (1 << 11);
const uint D3DCOMPILE_WARNINGS_ARE_ERRORS = (1 << 18);

[extern "d3dcompiler_47.dll"]    // 43 is the earliest available without DirectX package, should be 47.
HRESULT D3DCompile (char*             pSrcData,
                    SIZE_T            SrcDataSize,
                    LPCSTR            pSourceName,  // null
                    D3D_SHADER_MACRO* pDefines,
                    ID3DInclude*      pInclude,
                    LPCSTR            pEntrypoint,
                    LPCSTR            pTarget,
                    UINT              Flags1,
                    UINT              Flags2,
                    ID3DBlob**        ppCode,
                    ID3DBlob**        ppErrorMsgs);

const uint DXGI_ENUM_MODES_INTERLACED = 1;

typedef [callback] HRESULT FGetDisplayModeList (IDXGIOutput * This, DXGI_FORMAT EnumFormat, UINT Flags, UINT *pNumModes, DXGI_MODE_DESC *pDesc);

struct IDXGIOutputVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FDummy f3; //  HRESULT SetPrivateData ( IDXGIOutput * This, REFGUID Name, UINT DataSize, byte *pData);
  FDummy f4; //  HRESULT SetPrivateDataInterface (IDXGIOutput * This, REFGUID Name, IUnknown *pUnknown);
  FDummy f5; //  HRESULT GetPrivateData (IDXGIOutput * This, REFGUID Name, UINT* pDataSize, byte* pData);
  FDummy f6; //  HRESULT GetParent (IDXGIOutput * This, REFIID riid, byte **ppParent);
  FDummy f7; //  HRESULT GetDesc (IDXGIOutput * This, DXGI_OUTPUT_DESC *pDesc);
  FGetDisplayModeList GetDisplayModeList; //  HRESULT GetDisplayModeList (IDXGIOutput * This, DXGI_FORMAT EnumFormat, UINT Flags, UINT *pNumModes, DXGI_MODE_DESC *pDesc);
  FDummy f9; //  HRESULT FindClosestMatchingMode (IDXGIOutput * This, DXGI_MODE_DESC *pModeToMatch, DXGI_MODE_DESC *pClosestMatch, IUnknown *pConcernedDevice);
  FDummy f10; //  HRESULT WaitForVBlank (IDXGIOutput * This);
  FDummy f11; //  HRESULT TakeOwnership (IDXGIOutput * This, IUnknown *pDevice, BOOL Exclusive);
  FDummy f12; //  void ReleaseOwnership (IDXGIOutput * This);
  FDummy f13; //  HRESULT GetGammaControlCapabilities (IDXGIOutput * This, DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps);
  FDummy f14; //  HRESULT SetGammaControl (IDXGIOutput * This, DXGI_GAMMA_CONTROL *pArray);
  FDummy f15; //  HRESULT GetGammaControl (IDXGIOutput * This, DXGI_GAMMA_CONTROL *pArray);
  FDummy f16; //  HRESULT SetDisplaySurface (IDXGIOutput * This, IDXGISurface *pScanoutSurface);
  FDummy f17; //  HRESULT GetDisplaySurfaceData (IDXGIOutput * This, IDXGISurface *pDestination);
  FDummy f18; //  HRESULT GetFrameStatistics (IDXGIOutput * This, DXGI_FRAME_STATISTICS *pStats);
}

struct IDXGIOutput
{
  IDXGIOutputVtbl* lpVtbl;
}

typedef IDXGIFactory;

typedef [callback] HRESULT FEnumAdapters (IDXGIFactory * This, UINT Adapter, IDXGIAdapter **ppAdapter);

struct IDXGIFactoryVtbl
{
  FQueryInterface        QueryInterface;
  FAddRef                AddRef;
  FRelease               Release;
  FDummy f3; //  HRESULT SetPrivateData (IDXGIFactory * This, REFGUID Name, UINT DataSize, byte *pData);
  FDummy f4; //  HRESULT SetPrivateDataInterface (IDXGIFactory * This, REFGUID Name, IUnknown *pUnknown);
  FDummy f5; //  HRESULT GetPrivateData (IDXGIFactory * This, REFGUID Name, UINT *pDataSize, byte *pData);
  FDummy f6; //  HRESULT GetParent (IDXGIFactory * This, REFIID riid, byte **ppParent);
  FEnumAdapters EnumAdapters; //  HRESULT EnumAdapters (IDXGIFactory * This, UINT Adapter, IDXGIAdapter **ppAdapter);
  FDummy f8; //  HRESULT MakeWindowAssociation (IDXGIFactory * This, HWND WindowHandle, UINT Flags);
  FDummy f9; //  HRESULT GetWindowAssociation (IDXGIFactory * This, HWND *pWindowHandle);
  FDummy f10; //  HRESULT CreateSwapChain (IDXGIFactory * This, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain);
  FDummy f11; //  HRESULT CreateSoftwareAdapter (IDXGIFactory * This, HMODULE Module, IDXGIAdapter **ppAdapter);
}

struct IDXGIFactory
{
  IDXGIFactoryVtbl* lpVtbl;
}

typedef [callback]
HRESULT FCreateDXGIFactory (REFIID riid, IDXGIFactory **ppFactory);

//---------------------------------------------------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------------------------------------------------
