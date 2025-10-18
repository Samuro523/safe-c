
// draw3d.c

#if WINDOWS

// directx.c  -  DirectX 11

use arithm, d3dutil, image, linear_algebra, math, strings, thread, tracing;
use win/windows, win/directx11, win/dxshaders, directxdef;

#begin unsafe

//-----------------------------------------------------------------------------------------------

uint          g_back_buffer_x_res;
uint          g_back_buffer_y_res;
int           g_new_width;
int           g_new_height;
bool          g_flip_mode;   // true = new Win10 flip method, false = old discard method
UINT          g_sampling_count;
const UINT    g_sampling_quality = 0;
int           g_best_antialiasing[5];
int           g_best_antialiasing_count;
float         g_background_color[4] = {0.0, 0.0, 0.0, 1.0};
const float   g_float_initial_picking[4] = {1.0, 1.0, 1.0, 1.0};
float         g_near_z = 0.0625;  // 6 cm
float         g_far_z  = 1024.0;  // 1024 m
volatile bool g_screen_was_resized;
XMMATRIX      g_default_view_matrix;          // used to compute picking ray
vector3       g_default_camera_eye;
vector3       g_world_position;
XMMATRIX      g_default_projection_matrix;    // 3D to 2D screen
XMMATRIX      g_default_ortho_matrix;         // 3D to 2D screen
XMMATRIX      g_default_shadow_matrix;
volatile bool g_shader_supports_indexing;
volatile bool g_projection_matrix_needs_update;
vector4       g_frustum[6];
bool          g_shader_constants_stable_dirty;
BLENDING      g_blending;
TEXTURE_ID    g_1_pixel_texture;
bool          g_wait_for_sync;
int           g_depth_stencil_buffer_mode;

int           g_next_frame_shadows_level;
int           g_shadows_level;   // 0 = off, 1 to 4 = number of shadow maps
bool          g_shadow_failure;
int           g_nb_of_instances;  // 1 for normal draw, 1 to 4 for shadows

//-----------------------------------------------------------------------------------------------

const float SHADOW_MAP_SIZE = 1024.0;  // 4 MB x number of maps

const float INITIAL_DEPTH_VALUE = 0.0;  // inversed for better precision

const byte INITIAL_STENCIL_VALUE = 0;
const byte COMPARE_STENCIL_VALUE = 0;

//-----------------------------------------------------------------------------------------------

packed struct ShaderConstantsStable  // max allowed size : 16K, must be a multiple of 16 !
{
  XMMATRIX   TransposedView;         // 64 bytes, camera position, rotation (changes if camera moves - once per frame)
  XMMATRIX   TransposedProjection;   // 64 bytes, 3D to 2D screen (changes if screen is resized or z view changed - rare)
  XMMATRIX   ShadowTransposedTransform;   // 64 bytes

  vector3    eyePosition;   // used for specular lightning (changes if camera moves - once per frame)
  float      fogInvRange;   // 1.0/(end - start) [0 if no fog]

  vector3    fogColor;
  float      fogStart;

  vector3    CameraOffset;
  float      FogInvExponent;

  float[1]   filler1;
  int        test_if_shader_supports_indexing;
  int        nb_of_shadow_maps;
  int        shader_supports_indexing;

  vector3    AmbiantLightColor;
  float      filler2;

  vector3    SunLightColor;
  float      filler3;

  vector3    SunLightDirection;
  int        NbLights;

  LIGHT      Light[MAX_LIGHTS];  // 48 x 128 = 6144 bytes
}

//-----------------------------------------------------------------------------------------------

packed struct ShaderConstantsDynamic  // max allowed size : 16K, must be a multiple of 16 !
{
  matrix44   TransposedWorld;     // object position, rotation (changes for each object) (64 bytes)

  float[4]   MaterialColor;       // (16 bytes)

  vector3    EmissiveColor;
  float      metallic;   // 0 .. 1

  OBJECT_UV_TRANSFORM uv_transform;  // 24 bytes
  float      tclock;        // current time in secs, used for perlin noise
  float      smooth;        // 0 .. 1  (avoid 1)

  uint       is_ambiantlight_on;
  uint       is_sunlight_on;
  uint       is_ambiant_downwards;  // 1 = ambiant 10% more intense when normal shines from above
  uint       disable_ambiant_sun_underground;

  uint       dummy1;
  uint       dummy2;
  uint       dummy3;
  float      parallax_factor;       // 0 = none, ex: -0.05

  float      perlin_amplitude;   // 1.0
  float      translucid;         // 0.0 to 1.0
  uint       has_bump_texture;
  uint       has_mro_texture;

  float[4]   objectid;            // unique object id set by user for picking (on 3 bytes only)
}

const ShaderConstantsDynamic DEFAULT_DYNAMIC_CONSTANT =
   {TransposedWorld   => M44_ONE,
    MaterialColor     => {1.0, 1.0, 1.0, 1.0},
    EmissiveColor     => {0.0, 0.0, 0.0},
    metallic          => 0.0,
    uv_transform      => {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    tclock            => 0.0,
    smooth            => 0.0,
    is_ambiantlight_on=> 0,
    is_sunlight_on    => 0,
    is_ambiant_downwards => 0,
    disable_ambiant_sun_underground => 0,
    dummy1            => 0,
    dummy2            => 0,
    dummy3            => 0,
    parallax_factor   => 0.0,
    perlin_amplitude  => 1.0,
    translucid        => 0.0,
    has_bump_texture  => 0,
    has_mro_texture   => 0,
    objectid          => {0.0, 0.0, 0.0, 0.0},
   };

//-----------------------------------------------------------------------------------------------

ShaderConstantsStable   g_shader_constants_stable;
ShaderConstantsDynamic  g_shader_constants_dynamic;

//-----------------------------------------------------------------------------------------------

struct VERTEX_SHADER
{
  ID3D11VertexShader* m_vertexShader;
  ID3D11InputLayout*  m_layout;
}

struct GEOMETRY_SHADER
{
  ID3D11GeometryShader*  m_geometryShader;
}

struct PIXEL_SHADER
{
  ID3D11PixelShader*  m_pixelShader;
}

//-----------------------------------------------------------------------------------------------

const int SHADER_PASS_RENDERING        = 0;   // this will be the final rendering pass
const int SHADER_PASS_BUILD_SHADOW_MAP = 1;
// more preparation passes could appear here
const int MAX_SHADERS_PASSES_SHIFTS = 1;
const int MAX_SHADER_PASSES = 1 << MAX_SHADERS_PASSES_SHIFTS;    // = 2
const int MAX_SHADER_PASS_MASK = MAX_SHADER_PASSES - 1;

int g_shader_pass = SHADER_PASS_RENDERING;  // one of above

//-----------------------------------------------------------------------------------------------

const int VERTEX_SHADER_NORMAL = 0;
const int VERTEX_SHADER_RIGGED = 1;
const int MAX_VERTEX_SHADERS = 2;
int g_current_vertex_shader;
int g_next_vertex_shader;
VERTEX_SHADER  g_vertex_shader[MAX_SHADER_PASSES * MAX_VERTEX_SHADERS];  // low index = SHADER_PASS_xxx, high index = VERTEX_xxx

//-----------------------------------------------------------------------------------------------

const int GEOMETRY_SHADER_DEFAULT = 0;
const int MAX_GEOMETRY_SHADERS = 1;
int g_current_geometry_shader;
int g_next_geometry_shader;
GEOMETRY_SHADER g_geometry_shader[MAX_SHADER_PASSES * MAX_GEOMETRY_SHADERS];

//-----------------------------------------------------------------------------------------------

const int PIXEL_SHADER_SOLID = 0;
const int MAX_PIXEL_SHADERS = 1;
int g_current_pixel_shader;
int g_next_pixel_shader;
PIXEL_SHADER  g_pixel_shader[MAX_SHADER_PASSES * MAX_PIXEL_SHADERS];

//-----------------------------------------------------------------------------------------------

XMMATRIX g_identityMatrix;

//-----------------------------------------------------------------------------------------------

enum STATE {STOPPED, RUNNING};
STATE g_state;
bool g_stop_rendering;

CALLBACK_FUNCTIONS_3D g_callback_func;

//---------------------------------------------------------------------
void build_view_frustum (    matrix44 m_view,
                             matrix44 m_projection,
                         out vector4  m_frustum[6]);  // 6 plane a, b, c, d
//---------------------------------------------------------------------

// nearest and farthest z-points to draw

public void set_near_far_Z (float near_z = 0.0625,  // 6 cm
                            float far_z = 1024.00)  // 1024 m
{
  g_near_z = near_z;
  g_far_z  = far_z;
  g_projection_matrix_needs_update = true;
}

//---------------------------------------------------------------------

void compute_screen_size ()
{
  RECT rect;

  GetClientRect (main_hWnd, &rect);
  if (rect.bottom == 0)  // avoid division by zero
    rect.bottom = 1;
  if (rect.right == 0)
    rect.right = 1;

  g_back_buffer_x_res = (uint)rect.right;
  g_back_buffer_y_res = (uint)rect.bottom;
}

//---------------------------------------------------------------------

int create_vertex_shader (    ID3D11Device*        device,
                          byte[]                   blob,
                          D3D11_INPUT_ELEMENT_DESC Layout[],
                      out VERTEX_SHADER            shader)
{
  HRESULT result;

  clear shader;

  // Create the vertex shader from the buffer.
  result = device->lpVtbl->CreateVertexShader (device, &blob, blob'size, null, &shader.m_vertexShader);
  if (result < 0)
  {
    log_error ("CreateVertexShader()", result);
    return -1;
  }

  // Create the vertex input layout.
  result = device->lpVtbl->CreateInputLayout (device, &Layout, (uint)Layout'length, &blob, blob'size, &shader.m_layout);
  if (result < 0)
  {
    log_error ("CreateInputLayout()", result);
    return -1;
  }

  return 0;
}

//---------------------------------------------------------------------

int create_geometry_shader (    ID3D11Device*   device,
                                byte[]          blob,
                            out GEOMETRY_SHADER shader)
{
  HRESULT result;

  clear shader;

  // Create the pixel shader from the buffer.
  result = device->lpVtbl->CreateGeometryShader (device, &blob, blob'size, null, &shader.m_geometryShader);
  if (result < 0)
  {
    log_error ("CreateGeometryShader()", result);
    return -1;
  }

  return 0;
}

//---------------------------------------------------------------------

int create_pixel_shader (    ID3D11Device*  device,
                             byte[]         blob,
                         out PIXEL_SHADER   shader)
{
  HRESULT result;

  clear shader;

  // Create the pixel shader from the buffer.
  result = device->lpVtbl->CreatePixelShader (device, &blob, blob'size, null, &shader.m_pixelShader);
  if (result < 0)
  {
    log_error ("CreatePixelShader()", result);
    return -1;
  }

  return 0;
}

//---------------------------------------------------------------------

void select_vs_constant_buffers (ID3D11Buffer* buffers[])
{
  DX.m_deviceContext->lpVtbl->VSSetConstantBuffers (DX.m_deviceContext, StartSlot => 0, NumBuffers => (UINT)buffers'length, (ID3D11Buffer**)&buffers);
}

//---------------------------------------------------------------------

void select_new_vertex_shader (BONE_BUFFER_ID bone_id = 0)
{
  if (g_next_vertex_shader != g_current_vertex_shader)
  {
    ref VERTEX_SHADER vs = g_vertex_shader[g_next_vertex_shader];

    // set vertex input layout & vertex shader.
    DX.m_deviceContext->lpVtbl->IASetInputLayout (DX.m_deviceContext, vs.m_layout);
    DX.m_deviceContext->lpVtbl->VSSetShader      (DX.m_deviceContext, vs.m_vertexShader, null, 0);

    if ((g_next_vertex_shader >> MAX_SHADERS_PASSES_SHIFTS) == VERTEX_SHADER_NORMAL)
    {
      select_vs_constant_buffers ({DX.m_constantBufferStable,
                                   DX.m_constantBufferDynamic});
    }
    else
    {
      select_vs_constant_buffers ({DX.m_constantBufferStable,
                                   DX.m_constantBufferDynamic,
                                   *(ID3D11Buffer**)&bone_id});
    }

    g_current_vertex_shader = g_next_vertex_shader;
  }
  else  // same shader as previous call
  {
    if ((g_next_vertex_shader >> MAX_SHADERS_PASSES_SHIFTS) == VERTEX_SHADER_RIGGED)
    {
      select_vs_constant_buffers ({DX.m_constantBufferStable,
                                   DX.m_constantBufferDynamic,
                                   *(ID3D11Buffer**)&bone_id});
    }
  }
}

//---------------------------------------------------------------------

void select_ps_constant_buffers (ID3D11Buffer* buffers[])
{
  DX.m_deviceContext->lpVtbl->PSSetConstantBuffers (DX.m_deviceContext, StartSlot => 0, NumBuffers => (UINT)buffers'length, (ID3D11Buffer**)&buffers);
}

//---------------------------------------------------------------------

void select_new_geometry_shader ()
{
  if (g_next_geometry_shader != g_current_geometry_shader)
  {
    // Set the pixel shader (can be null to disable it).
    DX.m_deviceContext->lpVtbl->GSSetShader (DX.m_deviceContext, g_geometry_shader[g_next_geometry_shader].m_geometryShader, null, 0);

    g_current_geometry_shader = g_next_geometry_shader;
  }
}

//---------------------------------------------------------------------

void select_new_pixel_shader ()
{
  if (g_next_pixel_shader != g_current_pixel_shader)
  {
    // Set the pixel shader (can be null to disable it).
    DX.m_deviceContext->lpVtbl->PSSetShader (DX.m_deviceContext, g_pixel_shader[g_next_pixel_shader].m_pixelShader, null, 0);

    // set the constant buffers in the pixel shader
    select_ps_constant_buffers ({DX.m_constantBufferStable, DX.m_constantBufferDynamic});

    // Set the sampler state in the pixel shader.
    DX.m_deviceContext->lpVtbl->PSSetSamplers (DX.m_deviceContext, 0, (UINT)DX.m_sampleState'length, &DX.m_sampleState);

    g_current_pixel_shader = g_next_pixel_shader;
  }
}

//---------------------------------------------------------------------

void close_vertex_shader (ref VERTEX_SHADER shader)
{
  if (shader.m_layout != null)
    shader.m_layout->lpVtbl->Release((LPVOID)shader.m_layout);

  // Release the vertex shader.
  if (shader.m_vertexShader != null)
    shader.m_vertexShader->lpVtbl->Release((LPVOID)shader.m_vertexShader);

  clear shader;
}

//---------------------------------------------------------------------

void close_geometry_shader (ref GEOMETRY_SHADER shader)
{
  // Release the pixel shader.
  if (shader.m_geometryShader != null)
    shader.m_geometryShader->lpVtbl->Release((LPVOID)shader.m_geometryShader);

  clear shader;
}

//---------------------------------------------------------------------

void close_pixel_shader (ref PIXEL_SHADER shader)
{
  // Release the pixel shader.
  if (shader.m_pixelShader != null)
    shader.m_pixelShader->lpVtbl->Release((LPVOID)shader.m_pixelShader);

  clear shader;
}

//---------------------------------------------------------------------

// Setup the constant buffers that are used in the shaders.

int create_constant_buffers (byte[] constant_data_stable, byte[] constant_data_dynamic)
{
  HRESULT           result;
  D3D11_BUFFER_DESC BufferDesc;

  clear BufferDesc;
  BufferDesc.Usage = D3D11_USAGE_DYNAMIC;    // cpu write, GPU read
  BufferDesc.ByteWidth = constant_data_stable'size;
  BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  result = DX.m_device->lpVtbl->CreateBuffer (DX.m_device, &BufferDesc, pInitialData => null, &DX.m_constantBufferStable);
  if (result < 0)
  {
    log_error ("CreateBuffer(1)", result);
    return -1;
  }

  BufferDesc.ByteWidth = constant_data_dynamic'size;

  result = DX.m_device->lpVtbl->CreateBuffer (DX.m_device, &BufferDesc, pInitialData => null, &DX.m_constantBufferDynamic);
  if (result < 0)
  {
    log_error ("CreateBuffer(2)", result);
    return -1;
  }

  return 0;
}

//---------------------------------------------------------------------

void write_constant_buffer_stable (byte[] constant_data)
{
  HRESULT                  result;
  D3D11_MAPPED_SUBRESOURCE mappedResource;

  // Lock the constant buffer so it can be written to.
  result = DX.m_deviceContext->lpVtbl->Map (DX.m_deviceContext, (ID3D11Resource*)DX.m_constantBufferStable, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
  if (result < 0)
    fatal_abort ("Map()", result);

  // copy the new values into the constant buffer.
  mappedResource.pData[0 : constant_data'size] = constant_data;

  // Unlock the constant buffer.
  DX.m_deviceContext->lpVtbl->Unmap (DX.m_deviceContext, (ID3D11Resource*)DX.m_constantBufferStable, 0);
}

//---------------------------------------------------------------------

void write_constant_buffer_dynamic (byte[] constant_data)
{
  HRESULT                  result;
  D3D11_MAPPED_SUBRESOURCE mappedResource;

  // Lock the constant buffer so it can be written to.
  result = DX.m_deviceContext->lpVtbl->Map (DX.m_deviceContext, (ID3D11Resource*)DX.m_constantBufferDynamic, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
  if (result < 0)
    fatal_abort ("Map()", result);

  // copy the new values into the constant buffer.
  mappedResource.pData[0 : constant_data'size] = constant_data;

  // Unlock the constant buffer.
  DX.m_deviceContext->lpVtbl->Unmap (DX.m_deviceContext, (ID3D11Resource*)DX.m_constantBufferDynamic, 0);
}

//---------------------------------------------------------------------

#if 0 // not reliable
void get_card_information (out UINT pnumerator, out UINT pdenominator, out SIZE_T pcard_dedicated_ram, out SIZE_T pcard_max_additional_cpu_ram)
{
  UINT           screenWidth, screenHeight;
  HRESULT        result;
  IDXGIFactory*  factory;
  IDXGIAdapter*  adapter;
  IDXGIOutput*   adapterOutput;
  uint           numModes, i;

  // default values
  pnumerator = 60;
  pdenominator = 1;
  pcard_dedicated_ram = 64;
  pcard_max_additional_cpu_ram = 0;

  // get resolution of desktop
  screenWidth  = (UINT)GetSystemMetrics(SM_CXSCREEN);
  screenHeight = (UINT)GetSystemMetrics(SM_CYSCREEN);

  trace ("primary display resolution : %u x %u\n", screenWidth, screenHeight);

  // Create a DirectX graphics interface factory.

  {
    const string       dll_name  = "DXGI.dll\0";
    const string       func_name = "CreateDXGIFactory\0";
    HMODULE            hinstLib;
    LPVOID             func;
    FCreateDXGIFactory CreateDXGIFactory;

    hinstLib  = LoadLibraryA (&dll_name);
    if (hinstLib != 0)
    {
      func = GetProcAddress (hinstLib, &func_name);
      *(LPVOID*)&CreateDXGIFactory = *(LPVOID*)&func;

      if (CreateDXGIFactory != null)
      {
        result = CreateDXGIFactory (IID_IDXGIFactory, &factory);
        if (result >= 0)
        {
          // Use the factory to create an adapter for the primary graphics interface (video card).
          result = factory->lpVtbl->EnumAdapters (factory, 0, &adapter);
          if (result >= 0)
          {
            // Enumerate the primary adapter output (monitor).
            result = adapter->lpVtbl->EnumOutputs (adapter, 0, &adapterOutput);
            if (result >= 0)
            {
              // Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM_SRGB display format for the adapter output (monitor).
              result = adapterOutput->lpVtbl->GetDisplayModeList (adapterOutput, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_ENUM_MODES_INTERLACED, &numModes, null);
              if (result >= 0)
              {
                // Create a list to hold all the possible display modes for this monitor/video card combination.
                DXGI_MODE_DESC[]^ displayModeList = new DXGI_MODE_DESC[numModes];

                // Now fill the display mode list structures.
                result = adapterOutput->lpVtbl->GetDisplayModeList(adapterOutput, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_ENUM_MODES_INTERLACED, &numModes, &displayModeList^);
                if (result >= 0)
                {
                  // Now go through all the display modes and find the one that matches the screen width and height.
                  // When a match is found store the numerator and denominator of the refresh rate for that monitor.
                  for (i=0; i<numModes; i++)
                  {
                    ref DXGI_MODE_DESC d = displayModeList^[i];
                    if (d.Width == screenWidth)
                    {
                      if (d.Height == screenHeight)   // this makes no sense, refresh rates are not ordered
                      {
//                        pnumerator   = d.RefreshRate.Numerator;
//                        pdenominator = d.RefreshRate.Denominator;
                      }
                    }
                  }

                  // Get the adapter (video card) description.
                  {
                    DXGI_ADAPTER_DESC adapterDesc;

                    result = adapter->lpVtbl->GetDesc (adapter, &adapterDesc);
                    if (result >= 0)
                    {
                      pcard_dedicated_ram = adapterDesc.DedicatedVideoMemory;
                      pcard_max_additional_cpu_ram = adapterDesc.SharedSystemMemory;
                    }
                  }
                }

                // Release the display mode list.
                free displayModeList;
              }

              // Release the adapter output.
              adapterOutput->lpVtbl->Release((LPVOID)adapterOutput);
            }

            // Release the adapter.
            adapter->lpVtbl->Release((LPVOID)adapter);
          }

          // Release the factory.
          factory->lpVtbl->Release((LPVOID)factory);
        }
      }
    }
  }
}
#endif

//---------------------------------------------------------------------

void intern_set_depth_stencil_state ()
{
  DX.m_deviceContext->lpVtbl->OMSetDepthStencilState
        (DX.m_deviceContext,
         DX.m_depthStencilStateDepth[g_depth_stencil_buffer_mode],
         StencilRef => COMPARE_STENCIL_VALUE);  // Reference value to perform against when doing a depth-stencil test.
}

//---------------------------------------------------------------------

public void set_depth_buffer (bool testing, bool writing = true)    // default: on/on for render_3D, off/on for render_2D.
{
  g_depth_stencil_buffer_mode = (g_depth_stencil_buffer_mode & (255-1-2)) + (int)testing + 2*(int)writing;
  intern_set_depth_stencil_state ();
}

//---------------------------------------------------------------------

public void set_stencil_buffer (bool testing, bool writing = false)    // default: off/off for render_3D, off/off for render_2D.
{
  g_depth_stencil_buffer_mode = (g_depth_stencil_buffer_mode & (255-4-8)) + 4*(int)testing + 8*(int)writing;
  intern_set_depth_stencil_state ();
}

//---------------------------------------------------------------------

public void set_blending (BLENDING blending)
{
  float blendFactor[4];

  if (g_blending == blending)
    return;

  clear blendFactor; /*  blendFactor[0] = 0.0f; blendFactor[1] = 0.0f; blendFactor[2] = 0.0f; blendFactor[3] = 0.0f; */

  DX.m_deviceContext->lpVtbl->OMSetBlendState (DX.m_deviceContext,
                                               DX.m_BlendingState[(int)blending],
                                               blendFactor,
                                               0xffffffff);
  g_blending = blending;
}

//---------------------------------------------------------------------

int create_all_shaders ()
{
  int p;

  for (p=0; p<MAX_SHADER_PASSES; p++)
  {
    {
      ////////////////////////////////////////////////////////////////////////////////
      // basic vertex shader
      ////////////////////////////////////////////////////////////////////////////////

      // Create the vertex input layout description.
      // This setup needs to match the structure VERTEX and the one in the Shader.

      const string POS = "POSITION\0";
      const string NOR = "NORMAL\0";
      const string TAG = "TANGENT\0";
      const string COL = "COLOR\0";
      const string TEX = "TEXCOORD\0";

      D3D11_INPUT_ELEMENT_DESC Layout[5];

      int rc;

      clear Layout;
      Layout[0].SemanticName = &POS;
      // Layout[0].SemanticIndex = 0;
      Layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
      // Layout[0].InputSlot = 0;
      // Layout[0].AlignedByteOffset = 0;
      Layout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      // Layout[0].InstanceDataStepRate = 0;

      Layout[1].SemanticName = &NOR;
      // Layout[1].SemanticIndex = 0;
      Layout[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
      // Layout[1].InputSlot = 0;
      Layout[1].AlignedByteOffset = 12;
      Layout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      // Layout[1].InstanceDataStepRate = 0;

      Layout[2].SemanticName = &TAG;
      // Layout[1].SemanticIndex = 0;
      Layout[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
      // Layout[2].InputSlot = 0;
      Layout[2].AlignedByteOffset = 24;
      Layout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      // Layout[2].InstanceDataStepRate = 0;

      Layout[3].SemanticName = &COL;
      // Layout[3].SemanticIndex = 0;
      Layout[3].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      // Layout[3].InputSlot = 0;
      Layout[3].AlignedByteOffset = 36;
      Layout[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      // Layout[2].InstanceDataStepRate = 0;

      Layout[4].SemanticName = &TEX;
      // Layout[4].SemanticIndex = 0;
      Layout[4].Format = DXGI_FORMAT_R32G32_FLOAT;
      // Layout[4].InputSlot = 0;
      Layout[4].AlignedByteOffset = 40;
      Layout[4].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      // Layout[4].InstanceDataStepRate = 0;

      rc = create_vertex_shader (DX.m_device, vertex_shader_basic[p], Layout, out g_vertex_shader[p + MAX_SHADER_PASSES * VERTEX_SHADER_NORMAL]);
      if (rc != 0)
      {
        log_error ("create_vertex_shader(1)", rc);
        return -1;
      }
    }


    {
      ////////////////////////////////////////////////////////////////////////////////
      // vertex shader for rigged mesh
      ////////////////////////////////////////////////////////////////////////////////

      // Create the vertex input layout description.
      // This setup needs to match the structure VERTEX and the one in the Shader.

      const string POS = "POSITION\0";
      const string NOR = "NORMAL\0";
      const string TAG = "TANGENT\0";
      const string TEX = "TEXCOORD\0";
      const string BONE = "BONE\0";
      const string WEIGHT = "WEIGHT\0";

      D3D11_INPUT_ELEMENT_DESC Layout[5+4];

      int  rc;
      uint i, idx, ofs;

      clear Layout;
      Layout[0].SemanticName = &POS;
      // Layout[0].SemanticIndex = 0;
      Layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
      // Layout[0].InputSlot = 0;
      // Layout[0].AlignedByteOffset = 0;
      Layout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      // Layout[0].InstanceDataStepRate = 0;

      Layout[1].SemanticName = &NOR;
      // Layout[1].SemanticIndex = 0;
      Layout[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
      // Layout[1].InputSlot = 0;
      Layout[1].AlignedByteOffset = 12;
      Layout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      // Layout[1].InstanceDataStepRate = 0;

      Layout[2].SemanticName = &TAG;
      // Layout[2].SemanticIndex = 0;
      Layout[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
      // Layout[2].InputSlot = 0;
      Layout[2].AlignedByteOffset = 24;
      Layout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      // Layout[2].InstanceDataStepRate = 0;

      Layout[3].SemanticName = &TEX;
      // Layout[3].SemanticIndex = 0;
      Layout[3].Format = DXGI_FORMAT_R32G32_FLOAT;
      // Layout[2].InputSlot = 0;
      Layout[3].AlignedByteOffset = 36;
      Layout[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      // Layout[3].InstanceDataStepRate = 0;

      idx = 4;
      ofs = 36 + 8;
      Layout[idx].SemanticName = &BONE;
      Layout[idx].SemanticIndex = 0;
      Layout[idx].Format = DXGI_FORMAT_R32_UINT;
      // Layout[idx].InputSlot = 0;
      Layout[idx].AlignedByteOffset = ofs;
      Layout[idx].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      // Layout[idx].InstanceDataStepRate = 0;
      idx++;
      ofs+=4;

      for (i=0; i<4; i++)
      {
        Layout[idx].SemanticName = &WEIGHT;
        Layout[idx].SemanticIndex = i;
        Layout[idx].Format = DXGI_FORMAT_R32_FLOAT;
        // Layout[idx].InputSlot = 0;
        Layout[idx].AlignedByteOffset = ofs;
        Layout[idx].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        // Layout[idx].InstanceDataStepRate = 0;
        idx++;
        ofs+=4;
      }

      rc = create_vertex_shader (DX.m_device, vertex_shader_rigged[p], Layout, out g_vertex_shader[p + MAX_SHADER_PASSES * VERTEX_SHADER_RIGGED]);
      if (rc != 0)
      {
        log_error ("create_vertex_shader(2)", rc);
        return -1;
      }
    }

    {
      ////////////////////////////////////////////////////////////////////////////////
      // geometry shaders
      ////////////////////////////////////////////////////////////////////////////////

      int rc;

      if (geometry_shader[p]'length > 0)
      {
        rc = create_geometry_shader (DX.m_device, geometry_shader[p], out g_geometry_shader[p + MAX_SHADER_PASSES * GEOMETRY_SHADER_DEFAULT]);
        if (rc != 0)
        {
          log_error ("create_geometry_shader()", rc);
          return -1;
        }
      }
    }

    {
      ////////////////////////////////////////////////////////////////////////////////
      // pixel shaders
      ////////////////////////////////////////////////////////////////////////////////

      int rc;

      if (pixel_shader_solid[p]'length > 0)
      {
        rc = create_pixel_shader (DX.m_device, pixel_shader_solid[p], out g_pixel_shader[p + MAX_SHADER_PASSES * PIXEL_SHADER_SOLID]);
        if (rc != 0)
        {
          log_error ("create_pixel_shader(1)", rc);
          return -1;
        }
      }
    }
  }

  return 0;
}

//---------------------------------------------------------------------

void free_shadow_resources ()
{
  if (DX.m_Shadow_depth_View != null)
  {
    DX.m_Shadow_depth_View->lpVtbl->Release((LPVOID)DX.m_Shadow_depth_View);
    DX.m_Shadow_depth_View = null;
  }

  if (DX.m_shadowTextureView != null)
  {
    DX.m_shadowTextureView->lpVtbl->Release((LPVOID)DX.m_shadowTextureView);
    DX.m_shadowTextureView = null;
  }

  if (DX.m_Shadow_depth_Buffer != null)
  {
    DX.m_Shadow_depth_Buffer->lpVtbl->Release((LPVOID)DX.m_Shadow_depth_Buffer);
    DX.m_Shadow_depth_Buffer = null;
  }

  DX.m_DefaultTextures[3] = *(ID3D11ShaderResourceView**)&g_1_pixel_texture;  // set shadow to 1 pixel
}

//---------------------------------------------------------------------

int allocate_shadow_ressources (int trial)
{
  HRESULT result;

  {
    D3D11_TEXTURE2D_DESC  shadowDepthBufferDesc;

    // Set up the description of the depth buffer.
    clear shadowDepthBufferDesc;
    shadowDepthBufferDesc.Width = (UINT)SHADOW_MAP_SIZE;
    shadowDepthBufferDesc.Height = (UINT)SHADOW_MAP_SIZE;
    shadowDepthBufferDesc.MipLevels = 1;
    shadowDepthBufferDesc.ArraySize = (UINT)g_shadows_level;

    // requires 'D' or TYPELESS format
    shadowDepthBufferDesc.Format = (trial == 1) ? DXGI_FORMAT_R32G8X24_TYPELESS : DXGI_FORMAT_R24G8_TYPELESS;

    shadowDepthBufferDesc.SampleDesc.Count = 1;
    shadowDepthBufferDesc.SampleDesc.Quality = 0;
    shadowDepthBufferDesc.Usage = D3D11_USAGE_DEFAULT;   // inaccessible to the CPU
    shadowDepthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    // shadowDepthBufferDesc.CPUAccessFlags = 0;
    // shadowDepthBufferDesc.MiscFlags = 0;

    // Create the texture for the depth buffer using the filled out description.
    result = DX.m_device->lpVtbl->CreateTexture2D (DX.m_device, &shadowDepthBufferDesc, null, &DX.m_Shadow_depth_Buffer);
    if (result < 0)
    {
      log_error ("CreateTexture2D(Shadow)", result);
      free_shadow_resources ();
      return -1;
    }
  }


  {
    D3D11_DEPTH_STENCIL_VIEW_DESC ShadowDepthStencilViewDesc;

    // Set up the depth stencil view description.
    clear ShadowDepthStencilViewDesc;

    // NOT ALLOWED : any format with TYPELESS in the name.
    ShadowDepthStencilViewDesc.Format = (trial == 1) ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_D24_UNORM_S8_UINT;

    ShadowDepthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;

    // ShadowDepthStencilViewDesc.d3d11_texunion.Texture2DArray.MipSlice = 0;
    // ShadowDepthStencilViewDesc.d3d11_texunion.Texture2DArray.FirstArraySlice = 0;
    ShadowDepthStencilViewDesc.d3d11_texunion.Texture2DArray.ArraySize = (UINT)g_shadows_level;

    // Create the depth stencil view.
    result = DX.m_device->lpVtbl->CreateDepthStencilView (DX.m_device, (ID3D11Resource*)DX.m_Shadow_depth_Buffer, &ShadowDepthStencilViewDesc, &DX.m_Shadow_depth_View);
    if (result < 0)
    {
      log_error ("CreateDepthStencilView(shadow)", result);
      free_shadow_resources ();
      return -1;
    }
  }

  {
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;

    clear srvDesc;
    srvDesc.Format = (trial == 1) ? DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS : DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;

    // srvDesc.array.Texture2DArray.MostDetailedMip = 0;
    srvDesc.array.Texture2DArray.MipLevels = 1;
    // srvDesc.array.Texture2DArray.FirstArraySlice = 0;
    srvDesc.array.Texture2DArray.ArraySize = (UINT)g_shadows_level;

    result = DX.m_device->lpVtbl->CreateShaderResourceView (DX.m_device, *(ID3D11Resource**)&DX.m_Shadow_depth_Buffer, &srvDesc, &DX.m_shadowTextureView);
    if (result < 0)
    {
      log_error ("CreateShaderResourceView(shadow)", result);
      free_shadow_resources ();
      return -1;
    }
  }

  DX.m_DefaultTextures[3] = DX.m_shadowTextureView;  // set shadow

  return 0;
}

//---------------------------------------------------------------------

int create_state ()
{
  HRESULT result;

  {
    ID3D11Resource*  backBufferPtr;

    // Get the pointer to the first back buffer.
    result = DX.m_swapChain->lpVtbl->GetBuffer (DX.m_swapChain, 0, IID_ID3D11Texture2D, (LPVOID*)&backBufferPtr);
    if (result < 0)
    {
      log_error ("GetBuffer()", result);
      return -1;
    }

    // Create the render target view with the back buffer pointer.

    if (g_flip_mode)   // new swap method : flip
    {
      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;

      clear rtvDesc;
      rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
      rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
//      rtvDesc.Texture2D = {MipSlice => 0};

      result = DX.m_device->lpVtbl->CreateRenderTargetView (DX.m_device, backBufferPtr, &rtvDesc, &DX.m_renderTargetView[0]);
    }
    else    // old swap method : copy
    {
      result = DX.m_device->lpVtbl->CreateRenderTargetView (DX.m_device, backBufferPtr, null, &DX.m_renderTargetView[0]);
    }

    if (result < 0)
    {
      log_error ("CreateRenderTargetView()", result);
      return -1;
    }

    // Release pointer to the back buffer as we no longer need it.
    backBufferPtr->lpVtbl->Release((LPVOID)backBufferPtr);
  }


  // MSAA render target

  if (g_flip_mode && g_sampling_count > 1)   // mode FLIP and MSAA
  {
    // we need to render to a temporary MSAA buffer

    D3D11_TEXTURE2D_DESC  msaaRenderTargetDesc;

    // Set up the description of the MSAA buffer.

    clear msaaRenderTargetDesc;
    msaaRenderTargetDesc.Width = g_back_buffer_x_res;
    msaaRenderTargetDesc.Height = g_back_buffer_y_res;
    msaaRenderTargetDesc.MipLevels = 1;
    msaaRenderTargetDesc.ArraySize = 1;
    msaaRenderTargetDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    msaaRenderTargetDesc.SampleDesc.Count = g_sampling_count;
    msaaRenderTargetDesc.SampleDesc.Quality = g_sampling_quality;
    msaaRenderTargetDesc.Usage = D3D11_USAGE_DEFAULT;   // inaccessible to the CPU
    msaaRenderTargetDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    // msaaRenderTargetDesc.CPUAccessFlags = 0;
    // msaaRenderTargetDesc.MiscFlags = 0;

    result = DX.m_device->lpVtbl->CreateTexture2D (DX.m_device, &msaaRenderTargetDesc, null, &DX.m_msaaRenderTarget);
    if (result < 0)
    {
      log_error ("CreateTexture2D(2)", result);
      return -1;
    }

    {
      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;

      clear rtvDesc;
      rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
      rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
//      rtvDesc.Texture2DMS = ?;

      result = DX.m_device->lpVtbl->CreateRenderTargetView (DX.m_device, (ID3D11Resource *)DX.m_msaaRenderTarget, &rtvDesc, &DX.m_msaaRenderTargetView);
      if (result < 0)
      {
        log_error ("CreateRenderTargetView()", result);
        return -1;
      }
    }
  }


  // picking buffer

  {
    D3D11_TEXTURE2D_DESC  pickingBufferDesc;

    // Set up the description of the picking buffer.
    clear pickingBufferDesc;
    pickingBufferDesc.Width = g_back_buffer_x_res;
    pickingBufferDesc.Height = g_back_buffer_y_res;
    pickingBufferDesc.MipLevels = 1;
    pickingBufferDesc.ArraySize = 1;
    pickingBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // a 32-bit format, NOT SRGB because it would change the object_id !
    pickingBufferDesc.SampleDesc.Count = g_sampling_count;
    pickingBufferDesc.SampleDesc.Quality = g_sampling_quality;
    pickingBufferDesc.Usage = D3D11_USAGE_DEFAULT;   // inaccessible to the CPU
    pickingBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    // pickingBufferDesc.CPUAccessFlags = 0;
    // pickingBufferDesc.MiscFlags = 0;

    // create the texture for the picking buffer
    result = DX.m_device->lpVtbl->CreateTexture2D (DX.m_device, &pickingBufferDesc, null, &DX.m_pickingBuffer);
    if (result < 0)
    {
      log_error ("CreateTexture2D(2)", result);
      return -1;
    }

    result = DX.m_device->lpVtbl->CreateRenderTargetView (DX.m_device, (ID3D11Resource *)DX.m_pickingBuffer, null, &DX.m_renderTargetView[1]);
    if (result < 0)
    {
      log_error ("CreateRenderTargetView()", result);
      return -1;
    }

    if (g_sampling_count > 1)
    {
      D3D11_TEXTURE2D_DESC  tempBufferDesc = pickingBufferDesc;
      tempBufferDesc.SampleDesc.Count = 1;  // must be 1 !
      tempBufferDesc.SampleDesc.Quality = 0;
      tempBufferDesc.Usage = D3D11_USAGE_DEFAULT;   // inaccessible to the CPU
      tempBufferDesc.BindFlags = 0;
      // tempBufferDesc.CPUAccessFlags = 0;
      // tempBufferDesc.MiscFlags = 0;

      // create the texture for the picking buffer
      result = DX.m_device->lpVtbl->CreateTexture2D (DX.m_device, &tempBufferDesc, null, (ID3D11Texture2D **)&DX.m_temp_multisampling);
      if (result < 0)
      {
        log_error ("CreateTexture2D(MS)", result);
        return -1;
      }
    }


    // picking result output, allocated on CPU

    {
      D3D11_TEXTURE2D_DESC  pickingOutputBufferDesc = pickingBufferDesc;

      // Set up the description of the picking buffer.
      pickingOutputBufferDesc.SampleDesc.Count = 1;  // must be 1 !
      pickingOutputBufferDesc.SampleDesc.Quality = 0;
      pickingOutputBufferDesc.Usage = D3D11_USAGE_STAGING;   // used only for copy gpu->cpu
      pickingOutputBufferDesc.BindFlags = 0;
      pickingOutputBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      // pickingOutputBufferDesc.MiscFlags = 0;

      // create the texture for the picking buffer output
      result = DX.m_device->lpVtbl->CreateTexture2D (DX.m_device, &pickingOutputBufferDesc, null, &DX.m_pickingOutputBuffer);
      if (result < 0)
      {
        log_error ("CreateTexture2D(3)", result);
        return -1;
      }
    }
  }


  // create depth/stencil buffer

  {
    int trial;
    for (trial = 1; trial <= 2; trial++)
    {
      {
        D3D11_TEXTURE2D_DESC  depthBufferDesc;

        // Set up the description of the depth buffer.
        clear depthBufferDesc;
        depthBufferDesc.Width = g_back_buffer_x_res;
        depthBufferDesc.Height = g_back_buffer_y_res;
        depthBufferDesc.MipLevels = 1;
        depthBufferDesc.ArraySize = 1;

        // requires 'D' or TYPELESS format
        depthBufferDesc.Format = (trial == 1) ? DXGI_FORMAT_R32G8X24_TYPELESS /* a 64-bit format */
                                              : DXGI_FORMAT_R24G8_TYPELESS;  /* a 32-bit format */

        depthBufferDesc.SampleDesc.Count = g_sampling_count;
        depthBufferDesc.SampleDesc.Quality = g_sampling_quality;
        depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;   // inaccessible to the CPU
        depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        // depthBufferDesc.CPUAccessFlags = 0;
        // depthBufferDesc.MiscFlags = 0;

        // Create the texture for the depth buffer using the filled out description.
        result = DX.m_device->lpVtbl->CreateTexture2D (DX.m_device, &depthBufferDesc, null, &DX.m_DepthStencil_Buffer);
        if (result < 0)
        {
          log_error ("CreateTexture2D(A)", result);

          if (trial == 1)
            continue;
          else
            return -1;
        }
      }


      {
        D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;

        // Set up the depth stencil view description.
        clear depthStencilViewDesc;

        // NOT ALLOWED : any format with TYPELESS in the name.
        depthStencilViewDesc.Format = (trial == 1) ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT /* 32-bit depth, 8-bit stencil, 24 bits unused */
                                                   : DXGI_FORMAT_D24_UNORM_S8_UINT;   /* 24 bits for depth and 8 bits for stencil */

        depthStencilViewDesc.ViewDimension = g_sampling_count > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
        // depthStencilViewDesc.d3d11_texunion.Texture2D.MipSlice = 0;

        // Create the depth stencil view.
        result = DX.m_device->lpVtbl->CreateDepthStencilView (DX.m_device, (ID3D11Resource*)DX.m_DepthStencil_Buffer, &depthStencilViewDesc, &DX.m_DepthStencil_View);
        if (result < 0)
        {
          log_error ("CreateDepthStencilView(A)", result);

          if (DX.m_DepthStencil_Buffer != null)
          {
            DX.m_DepthStencil_Buffer->lpVtbl->Release((LPVOID)DX.m_DepthStencil_Buffer);
            DX.m_DepthStencil_Buffer = null;
          }

          if (trial == 1)
            continue;
          else
            return -1;
        }
      }

      break;
    }
  }


  {
    int i;

    for (i=0; i<16; i+=4)       // create 16 depthstencilstates
    {
      bool stencil_test  = (i & 4) != 0;
      bool stencil_write = (i & 8) != 0;

      D3D11_DEPTH_STENCIL_DESC depthStencilDesc;

      // Set up the description of the stencil state.
      clear depthStencilDesc;

      // stencil buffer settings

      depthStencilDesc.StencilEnable    = i>=4 ? TRUE : FALSE;     // Enable stencil
      depthStencilDesc.StencilReadMask  = 0x000000FF;   // 8 bits for stencil
      depthStencilDesc.StencilWriteMask = 0x000000FF;   // 8 bits for stencil

      // Stencil operations if pixel is front-facing.

      if (stencil_test)
        depthStencilDesc.FrontFace.StencilFunc      = D3D11_COMPARISON_EQUAL;   // test if equal stencil reference value
      else
        depthStencilDesc.FrontFace.StencilFunc      = D3D11_COMPARISON_ALWAYS;  // stencil always passes

      // operation to perform when stencil testing and depth testing both pass.
      if (stencil_write)
        depthStencilDesc.FrontFace.StencilPassOp    = D3D11_STENCIL_OP_INCR_SAT;  // increment til 255
      else
        depthStencilDesc.FrontFace.StencilPassOp    = D3D11_STENCIL_OP_KEEP;  // Keep the existing stencil data

      // operation to perform when stencil test fails.
      depthStencilDesc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;  // Keep the existing stencil data

      // operation to perform when stencil test passes and depth test fails.
      depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;  // Keep the existing stencil data

      // Stencil operations if pixel is back-facing.
      depthStencilDesc.BackFace.StencilFunc        = D3D11_COMPARISON_NEVER;  // stencil never passes
      depthStencilDesc.BackFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
      depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
      depthStencilDesc.BackFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;


      // depth buffer settings

      // D3D11_COMPARISON_NEVER = writes nothing, D3D11_COMPARISON_ALWAYS = write all
      depthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER; // If source z > destination z, we write the pixel.

      depthStencilDesc.DepthEnable = FALSE;              // disable depth testing.
      depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // Turn off writes to the depth-stencil buffer.

      result = DX.m_device->lpVtbl->CreateDepthStencilState (DX.m_device, &depthStencilDesc, &DX.m_depthStencilStateDepth[i+0]);
      if (result < 0)
      {
        log_error ("CreateDepthStencilState(1)", result);
        return -1;
      }

      depthStencilDesc.DepthEnable = TRUE;              // enable depth testing.
      depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // Turn off writes to the depth-stencil buffer.

      result = DX.m_device->lpVtbl->CreateDepthStencilState (DX.m_device, &depthStencilDesc, &DX.m_depthStencilStateDepth[i+1]);
      if (result < 0)
      {
        log_error ("CreateDepthStencilState(2)", result);
        return -1;
      }

      depthStencilDesc.DepthEnable = FALSE;              // disable depth testing.
      depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;  // Turn on writes to the depth-stencil buffer.

      result = DX.m_device->lpVtbl->CreateDepthStencilState (DX.m_device, &depthStencilDesc, &DX.m_depthStencilStateDepth[i+2]);
      if (result < 0)
      {
        log_error ("CreateDepthStencilState(3)", result);
        return -1;
      }

      depthStencilDesc.DepthEnable = TRUE;              // enable depth testing.
      depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;  // Turn on writes to the depth-stencil buffer.

      result = DX.m_device->lpVtbl->CreateDepthStencilState (DX.m_device, &depthStencilDesc, &DX.m_depthStencilStateDepth[i+3]);
      if (result < 0)
      {
        log_error ("CreateDepthStencilState(4)", result);
        return -1;
      }
    }
  }

  {
    D3D11_RASTERIZER_DESC rasterDesc;

    // Setup the raster description which will determine how and what polygons will be drawn.
    clear rasterDesc;
    rasterDesc.FillMode = D3D11_FILL_SOLID;  // alternative is wireframe
    rasterDesc.CullMode = D3D11_CULL_BACK;   // Do not draw triangles that are back-facing (alternative: D3D11_CULL_NONE renders all triangles)
    // rasterDesc.FrontCounterClockwise = FALSE;
    // rasterDesc.DepthBias = 0;                // for displaying shadows properly
    // rasterDesc.DepthBiasClamp = 0.0f;
    // rasterDesc.SlopeScaledDepthBias = 0.0f;
    rasterDesc.DepthClipEnable = TRUE;       // clip z larger than back plane (can be disabled for some stencil operations)
    // rasterDesc.ScissorEnable = FALSE;        // can be used for scissor-rectangle culling
    rasterDesc.MultisampleEnable = (BOOL)(g_sampling_count > 1);
    // rasterDesc.AntialiasedLineEnable = FALSE;

    // create a rasterizer state
    result = DX.m_device->lpVtbl->CreateRasterizerState (DX.m_device, &rasterDesc, &DX.m_rasterState[0]);   // normal rendering
    if (result < 0)
    {
      log_error ("CreateRasterizerState()", result);
      return -1;
    }

// zero bias and scaled bias seems better ?
// rasterDesc.DepthBias = 260;
// rasterDesc.SlopeScaledDepthBias = 2.0;

    result = DX.m_device->lpVtbl->CreateRasterizerState (DX.m_device, &rasterDesc, &DX.m_rasterState[1]);    // for shadows
    if (result < 0)
    {
      log_error ("CreateRasterizerState()", result);
      return -1;
    }
  }


  {
    D3D11_BLEND_DESC  blendStateDescription;

    clear blendStateDescription;

    // Create an alpha enabled blend state description.
    // note: IndependentBlendEnable = TRUE; does not work on DirectX 10 : blending will be used for picking too.
    blendStateDescription.RenderTarget[0].BlendEnable = TRUE;
    blendStateDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendStateDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendStateDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendStateDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendStateDescription.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    // Create the blend state using the description.
    result = DX.m_device->lpVtbl->CreateBlendState (DX.m_device, &blendStateDescription, &DX.m_BlendingState[(int)BLENDING_TRANSLUCID]);
    if (result < 0)
    {
      log_error ("CreateBlendState(1)", result);
      return -1;
    }

    blendStateDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendStateDescription.RenderTarget[0].DestBlend = D3D11_BLEND_DEST_ALPHA;
    blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

    // Create the blend state using the description.
    result = DX.m_device->lpVtbl->CreateBlendState (DX.m_device, &blendStateDescription, &DX.m_BlendingState[(int)BLENDING_ADD]);
    if (result < 0)
    {
      log_error ("CreateBlendState(2)", result);
      return -1;
    }

    blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MIN;

    // Create the blend state using the description.
    result = DX.m_device->lpVtbl->CreateBlendState (DX.m_device, &blendStateDescription, &DX.m_BlendingState[(int)BLENDING_MIN]);
    if (result < 0)
    {
      log_error ("CreateBlendState(3)", result);
      return -1;
    }

    blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;

    // Create the blend state using the description.
    result = DX.m_device->lpVtbl->CreateBlendState (DX.m_device, &blendStateDescription, &DX.m_BlendingState[(int)BLENDING_MAX]);
    if (result < 0)
    {
      log_error ("CreateBlendState(4)", result);
      return -1;
    }

    // Modify the description to create an alpha disabled blend state description.
    blendStateDescription.RenderTarget[0].BlendEnable = FALSE;

    // Create the blend state using the description.
    result = DX.m_device->lpVtbl->CreateBlendState (DX.m_device, &blendStateDescription, &DX.m_BlendingState[(int)BLENDING_OFF]);
    if (result < 0)
    {
      log_error ("CreateBlendState(5)", result);
      return -1;
    }
  }

  g_blending = BLENDING_OFF;


  // Create a texture sampler state description.
  {
    D3D11_SAMPLER_DESC samplerDesc;

    // sampler to read from textures
    clear samplerDesc;
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    // samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    // samplerDesc.BorderColor[0] = 0.0;
    // samplerDesc.BorderColor[1] = 0.0;
    // samplerDesc.BorderColor[2] = 0.0;
    // samplerDesc.BorderColor[3] = 0.0;
    samplerDesc.MinLOD = -D3D11_FLOAT32_MAX;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

if (false)
{
  samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
  samplerDesc.MaxAnisotropy = 8;
  samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
}

    // Create the texture sampler state.
    result = DX.m_device->lpVtbl->CreateSamplerState (DX.m_device, &samplerDesc, &DX.m_sampleState[0]);   // normal rendering
    if (result < 0)
    {
      log_error ("CreateSamplerState(0)", result);
      return -1;
    }


    // sampler to read from shadow maps

    // filter: D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT
    //      or D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR
    //      or D3D11_FILTER_COMPARISON_ANISOTROPIC
    samplerDesc.Filter = D3D11_FILTER_COMPARISON_ANISOTROPIC;

    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    // samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
    // samplerDesc.BorderColor[0] = 0.0;
    // samplerDesc.BorderColor[1] = 0.0;
    // samplerDesc.BorderColor[2] = 0.0;
    // samplerDesc.BorderColor[3] = 0.0;
    samplerDesc.MinLOD = 0.0;
    samplerDesc.MaxLOD = 12.0;

    // Create the texture sampler state.
    result = DX.m_device->lpVtbl->CreateSamplerState (DX.m_device, &samplerDesc, &DX.m_sampleState[1]);
    if (result < 0)
    {
      log_error ("CreateSamplerState(1)", result);
      return -1;
    }
  }

  // Set the type of primitive that should be rendered, in this case always triangles.
  DX.m_deviceContext->lpVtbl->IASetPrimitiveTopology (DX.m_deviceContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


  // set shaders

  g_current_vertex_shader = -1;
  g_current_geometry_shader = -1;
  g_current_pixel_shader = -1;


  g_projection_matrix_needs_update = true;

  return 0;
}

//---------------------------------------------------------------------

void free_state ()
{
  int i;

  for (i=0; i<DX.m_sampleState'length; i++)
  {
    if (DX.m_sampleState[i] != null)
    {
      DX.m_sampleState[i]->lpVtbl->Release((LPVOID)DX.m_sampleState[i]);
      DX.m_sampleState[i] = null;
    }
  }

  for (i=0; i<DX.m_rasterState'length; i++)
  {
    if (DX.m_rasterState[i] != null)
    {
      DX.m_rasterState[i]->lpVtbl->Release((LPVOID)DX.m_rasterState[i]);
      DX.m_rasterState[i] = null;
    }
  }

  if (DX.m_DepthStencil_View != null)
  {
    DX.m_DepthStencil_View->lpVtbl->Release((LPVOID)DX.m_DepthStencil_View);
    DX.m_DepthStencil_View = null;
  }

  for (i=0; i<DX.m_depthStencilStateDepth'length; i++)
  {
    if (DX.m_depthStencilStateDepth[i] != null)
    {
      DX.m_depthStencilStateDepth[i]->lpVtbl->Release((LPVOID)DX.m_depthStencilStateDepth[i]);
      DX.m_depthStencilStateDepth[i] = null;
    }
  }

  if (DX.m_pickingBuffer != null)
  {
    DX.m_pickingBuffer->lpVtbl->Release((LPVOID)DX.m_pickingBuffer);
    DX.m_pickingBuffer = null;
  }

  if (DX.m_temp_multisampling != null)
  {
    DX.m_temp_multisampling->lpVtbl->Release((LPVOID)DX.m_temp_multisampling);
    DX.m_temp_multisampling = null;
  }

  if (DX.m_pickingOutputBuffer != null)
  {
    DX.m_pickingOutputBuffer->lpVtbl->Release((LPVOID)DX.m_pickingOutputBuffer);
    DX.m_pickingOutputBuffer = null;
  }

  if (DX.m_DepthStencil_Buffer != null)
  {
    DX.m_DepthStencil_Buffer->lpVtbl->Release((LPVOID)DX.m_DepthStencil_Buffer);
    DX.m_DepthStencil_Buffer = null;
  }

  if (DX.m_renderTargetView[0] != null)
  {
    DX.m_renderTargetView[0]->lpVtbl->Release((LPVOID)DX.m_renderTargetView[0]);
    DX.m_renderTargetView[0] = null;
  }

  if (DX.m_renderTargetView[1] != null)
  {
    DX.m_renderTargetView[1]->lpVtbl->Release((LPVOID)DX.m_renderTargetView[1]);
    DX.m_renderTargetView[1] = null;
  }

  for (i=0; i<=(int)BLENDING'last; i++)
  {
    if (DX.m_BlendingState[i] != null)
    {
      DX.m_BlendingState[i]->lpVtbl->Release((LPVOID)DX.m_BlendingState[i]);
      DX.m_BlendingState[i] = null;
    }
  }

  if (DX.m_msaaRenderTarget != null)
  {
    DX.m_msaaRenderTarget->lpVtbl->Release((LPVOID)DX.m_msaaRenderTarget);
    DX.m_msaaRenderTarget = null;
  }

  if (DX.m_msaaRenderTargetView != null)
  {
    DX.m_msaaRenderTargetView->lpVtbl->Release((LPVOID)DX.m_msaaRenderTargetView);
    DX.m_msaaRenderTargetView = null;
  }
}

//---------------------------------------------------------------------

// this is the function that closes DirectX

void close_DirectX ()
{
  int i;

  // Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
  if (DX.m_swapChain != null)
    DX.m_swapChain->lpVtbl->SetFullscreenState (DX.m_swapChain, Fullscreen => FALSE, null);

  free_state ();
  free_shadow_resources ();

  if (DX.m_deviceContext != null)
    DX.m_deviceContext->lpVtbl->Release((LPVOID)DX.m_deviceContext);

  if (DX.m_constantBufferStable != null)
  {
    DX.m_constantBufferStable->lpVtbl->Release((LPVOID)DX.m_constantBufferStable);
    DX.m_constantBufferStable = null;
  }

  if (DX.m_constantBufferDynamic != null)
  {
    DX.m_constantBufferDynamic->lpVtbl->Release((LPVOID)DX.m_constantBufferDynamic);
    DX.m_constantBufferDynamic = null;
  }

  for (i=0; i<g_vertex_shader'length; i++)
    close_vertex_shader (ref g_vertex_shader[i]);
  for (i=0; i<g_geometry_shader'length; i++)
    close_geometry_shader (ref g_geometry_shader[i]);
  for (i=0; i<g_pixel_shader'length; i++)
    close_pixel_shader (ref g_pixel_shader[i]);

  free_texture (g_1_pixel_texture);

  if (DX.m_device != null)
    DX.m_device->lpVtbl->Release((LPVOID)DX.m_device);

  if (DX.m_swapChain != null)
    DX.m_swapChain->lpVtbl->Release((LPVOID)DX.m_swapChain);
}

//---------------------------------------------------------------------

void set_chain_description (ref DXGI_SWAP_CHAIN_DESC  swapChainDesc,
                                bool                  flip_mode)
{
  if (flip_mode)
  {
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // new Win10 flip method
    g_flip_mode = true;

    swapChainDesc.BufferCount = 3; // minimum 2 for flip mode, 3 for standard directx games

    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB not allowed for flip mode

    // Turn multisampling off for flip mode (it is done later in presenter)
    swapChainDesc.SampleDesc.Count = 1;   // no MSAA for FLIP mode
    swapChainDesc.SampleDesc.Quality = 0;
  }
  else
  {
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;  // old Win7 copy method
    g_flip_mode = false;

    swapChainDesc.BufferCount = 1; // 1 buffer is enough for these old modes

    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;  // immediately use sRGB (old method)

    // Turn multisampling on or off.
    swapChainDesc.SampleDesc.Count = g_sampling_count;     // enable MSAA in swap chain buffers when not in flip mode
    swapChainDesc.SampleDesc.Quality = g_sampling_quality; // one less than the level returned by ID3D10Device::CheckMultisampleQualityLevels
  }
}

//---------------------------------------------------------------------
void Make2DMatrix (out XMMATRIX m);   // for rendering 2D objects in range -0.5 .. +0.5
//---------------------------------------------------------------------

// this function initializes and prepares Direct3D for use on main application window

int init_DirectX (bool debug, bool use_warp, bool old_discard_mode)
{
  HRESULT result;

  clear DX;

  compute_screen_size ();  // init g_back_buffer_x_res, g_back_buffer_y_res

  {
    const D3D_FEATURE_LEVEL featureLevel[5] =
                   {D3D_FEATURE_LEVEL_11_1,  // Win8, Win10, Win11
                    D3D_FEATURE_LEVEL_11_0,  // some Win7
                    D3D_FEATURE_LEVEL_10_1,  // some Win7
                    D3D_FEATURE_LEVEL_10_0,
                    D3D_FEATURE_LEVEL_9_1};

    DXGI_SWAP_CHAIN_DESC  swapChainDesc;
    D3D_FEATURE_LEVEL     actualfeaturelevel;
    uint                  fl;

    const string       dll_name  = "d3d11\0";
    const string       func_name = "D3D11CreateDeviceAndSwapChain\0";
    HMODULE            hinstLib;
    LPVOID             func;
    FD3D11CreateDeviceAndSwapChain D3D11CreateDeviceAndSwapChain;

    hinstLib = LoadLibraryA (&dll_name);
    if (hinstLib == 0)
    {
      log_error ("LoadLibraryA(d3d11)", 0);
      return -1;
    }

    func = GetProcAddress (hinstLib, &func_name);
    *(LPVOID*)&D3D11CreateDeviceAndSwapChain = *(LPVOID*)&func;
    if (D3D11CreateDeviceAndSwapChain == null)
    {
      log_error ("GetProcAddress(D3D11CreateDeviceAndSwapChain)", 0);
      return -1;
    }

    // Initialize the swap chain description.
    clear swapChainDesc;

    // Set the width and height of the back buffer.
    swapChainDesc.BufferDesc.Width = g_back_buffer_x_res;
    swapChainDesc.BufferDesc.Height = g_back_buffer_y_res;

    // Set the refresh rate of the back buffer.
//    swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
//    swapChainDesc.BufferDesc.RefreshRate.Denominator = 0;

    // Set the usage of the back buffer.
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    // Set the handle for the window to render to.
    swapChainDesc.OutputWindow = main_hWnd;

    // Set to windowed mode.
    swapChainDesc.Windowed = 1;

    set_chain_description (ref swapChainDesc, flip_mode => !old_discard_mode);

    // Set the scan line ordering and scaling to unspecified.
//    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
//    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    // Don't set the advanced flags.
    swapChainDesc.Flags = 0;

    result = -1;
    actualfeaturelevel = 0;
    for (fl=0; ; )  // due to a Windows weird behaviour, we need to retry
    {
      // Create the swap chain, Direct3D device, and Direct3D device context.
      result = D3D11CreateDeviceAndSwapChain (null,   // first adapter
                                              use_warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE,
                                              0,
                                              debug ? D3D11_CREATE_DEVICE_DEBUG : 0,
                                              &featureLevel[fl], (UINT)featureLevel'length - fl, D3D11_SDK_VERSION,
                                              &swapChainDesc, &DX.m_swapChain, &DX.m_device, &actualfeaturelevel, &DX.m_deviceContext);

      trace ("info: try DirectX feature level = %u.%u swap=%s samples=%u result=0x%x\n",
             featureLevel[fl] >> 12, (featureLevel[fl] >> 8) & 15,
             g_flip_mode ? "FLIP_DISCARD" : "DISCARD",
             g_sampling_count,
             result);

      if (result == 0)
        break;

      // invalid argument : try another

      if (g_flip_mode)   // new Win10 flip method not supported ?
      {
        set_chain_description (ref swapChainDesc, flip_mode => false);
      }
      else if (g_sampling_count > 1)   // try smaller sampling
      {
        g_sampling_count >>= 1;
        swapChainDesc.SampleDesc.Count = g_sampling_count;
      }
      else if (fl < (UINT)featureLevel'length)   // try older feature
      {
        fl++;
      }
      else
        break;
    }

    if (result < 0)
    {
      log_error ("D3D11CreateDeviceAndSwapChain()", result);
      return -1;
    }

    if (actualfeaturelevel < D3D_FEATURE_LEVEL_10_0)
    {
      log_error ("D3D11CreateDeviceAndSwapChain() - not enough features", -1);
      return -1;
    }
  }


  {
    UINT NumQualityLevels;
    int i;
    g_best_antialiasing_count = 1;
    g_best_antialiasing[0] = 1;
    for (i=2; i<=16; i<<=1)
    {
      result = DX.m_device->lpVtbl->CheckMultisampleQualityLevels (DX.m_device, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, SampleCount => (UINT)i, &NumQualityLevels);
      if (result < 0 || NumQualityLevels == 0)
        break;
      g_best_antialiasing[g_best_antialiasing_count++] = i;
    }
  }


  XMMatrixIdentity (&g_identityMatrix);
  Make2DMatrix (out g_default_ortho_matrix);

  // create constant buffers
  clear g_shader_constants_stable, g_shader_constants_dynamic;
  if (create_constant_buffers (g_shader_constants_stable, g_shader_constants_dynamic) < 0)
    return -1;

  // create shaders
  if (create_all_shaders () < 0)
    return -1;

  // create device context states
  if (create_state () < 0)
    return -1;

  // create g_1_pixel_texture
  {
    IMAGE_INFO img = {pixel => new byte[] ' {255, 255, 255, 255}, width => 1, height => 1};
    g_1_pixel_texture = create_texture (img);
    free img.pixel;
  }

  DX.m_DefaultTextures = {*(ID3D11ShaderResourceView**)&g_1_pixel_texture,
                          *(ID3D11ShaderResourceView**)&g_1_pixel_texture,
                          *(ID3D11ShaderResourceView**)&g_1_pixel_texture,
                          *(ID3D11ShaderResourceView**)&g_1_pixel_texture};

  assert (ShaderConstantsStable'size & 15) == 0;  // must be M16
  assert (ShaderConstantsDynamic'size & 15) == 0;  // must be M16

  return 0;
}

//---------------------------------------------------------------------

public void signal_DirectX_screen_was_resized (int width, int height)
{
  int w = max (1, width);
  int h = max (1, height);

  if (w == g_new_width && h == g_new_height)
    return;

  g_new_width = w;
  g_new_height = h;

  g_screen_was_resized = true;
}

//---------------------------------------------------------------------

void handle_screen_resize ()
{
  HRESULT result;

  if (DX.m_swapChain == null)
    return;

  result = DX.m_swapChain->lpVtbl->SetFullscreenState (DX.m_swapChain, Fullscreen => FALSE, null);
  if (result < 0)
    fatal_abort ("SetFullscreenState()", result);

  // reset any device context to the default settings : sets to NULL all resource slots, shaders, input layouts, predications,
  // scissor rectangles, depth-stencil state, rasterizer state, blend state, sampler state, and viewports.
  // the primitive topology is set to UNDEFINED.
  DX.m_deviceContext->lpVtbl->ClearState (DX.m_deviceContext);

  // Release all outstanding references to the swap chain's buffers.
  free_state ();

  // Preserve the existing buffer count and format.
  result = DX.m_swapChain->lpVtbl->ResizeBuffers (This           => DX.m_swapChain,
                                                  BufferCount    => 0,  // preserve buffer count
                                                  Width          => g_back_buffer_x_res,
                                                  Height         => g_back_buffer_y_res,
                                                  NewFormat      => 0,    // preserve existing format
                                                  SwapChainFlags => 0);
  if (result < 0)
    fatal_abort ("ResizeBuffers()", result);

  if (create_state () < 0)
    fatal_abort ("create_state()", -1);
}

//---------------------------------------------------------------------

public uint get_picking_object_id (int x, int y)
{
  HRESULT                  result;
  D3D11_MAPPED_SUBRESOURCE mappedResource;
  uint                     id;

  if ((uint)x >= g_back_buffer_x_res || (uint)y >= g_back_buffer_y_res)
    return 0xFFFFFF;

  // this call blocks until the above resource copy from previous frame was done
  result = DX.m_deviceContext->lpVtbl->Map (DX.m_deviceContext, (ID3D11Resource*)DX.m_pickingOutputBuffer, 0,
                                            D3D11_MAP_READ, 0, &mappedResource);
  if (result < 0)
    fatal_abort ("Map()", result);

  id = 0xFFFFFF & *(uint *)&mappedResource.pData[(x * 4) + (y * (int)mappedResource.RowPitch)];

  DX.m_deviceContext->lpVtbl->Unmap (DX.m_deviceContext, (ID3D11Resource*)DX.m_pickingOutputBuffer, 0);

  return id;
}

//---------------------------------------------------------------------

bool are_dynamic_arrays_supported ()
{
  bool arrays_supported;
  
  // clear the back buffers to background color.

  if (g_flip_mode && g_sampling_count > 1)   // mode FLIP and MSAA
    DX.m_deviceContext->lpVtbl->ClearRenderTargetView (DX.m_deviceContext, DX.m_msaaRenderTargetView, {0.0, 0.0, 0.0, 1.0});  // initial screen
  else
    DX.m_deviceContext->lpVtbl->ClearRenderTargetView (DX.m_deviceContext, DX.m_renderTargetView[0], {0.0, 0.0, 0.0, 1.0});  // initial screen

  DX.m_deviceContext->lpVtbl->ClearRenderTargetView (DX.m_deviceContext, DX.m_renderTargetView[1], {1.0, 1.0, 1.0, 1.0}); // initial picking buffer

  // clear the depth/stencil buffer
  DX.m_deviceContext->lpVtbl->ClearDepthStencilView (DX.m_deviceContext, DX.m_DepthStencil_View, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, INITIAL_DEPTH_VALUE, INITIAL_STENCIL_VALUE);

  //------------------------------

  clear g_shader_constants_stable;
  g_shader_constants_stable.TransposedView = g_identityMatrix;

  // for rendering 2D objects in range -0.5 .. +0.5
  MXXMatrixTranspose (g_default_ortho_matrix, out g_shader_constants_stable.TransposedProjection);

  g_shader_constants_stable.shader_supports_indexing = 1;
  g_shader_constants_stable.test_if_shader_supports_indexing = 1;
  g_shader_constants_stable_dirty = true;
  g_shader_constants_dynamic = DEFAULT_DYNAMIC_CONSTANT;
  g_shader_constants_dynamic.EmissiveColor = {1.0, 1.0, 1.0};

  g_depth_stencil_buffer_mode = 0;   // 1 = test depth,  2 = write depth
  intern_set_depth_stencil_state ();

  set_blending (BLENDING_TRANSLUCID);

  g_shader_pass = SHADER_PASS_RENDERING;
  g_next_geometry_shader = g_shader_pass + MAX_SHADER_PASSES * GEOMETRY_SHADER_DEFAULT;
  select_new_geometry_shader ();

  g_nb_of_instances = 1;

  // Bind the render target view (back buffer, picking buffer) and DepthStencil buffer to the output render pipeline.
  if (g_flip_mode && g_sampling_count > 1)   // mode FLIP and MSAA
  {
    ID3D11RenderTargetView* ptr[2] = {DX.m_msaaRenderTargetView, DX.m_renderTargetView[1]};
    DX.m_deviceContext->lpVtbl->OMSetRenderTargets (DX.m_deviceContext, 2, &ptr, DX.m_DepthStencil_View);
  }
  else
    DX.m_deviceContext->lpVtbl->OMSetRenderTargets (DX.m_deviceContext, 2, &DX.m_renderTargetView, DX.m_DepthStencil_View);

  // set the rasterizer state
  DX.m_deviceContext->lpVtbl->RSSetState (DX.m_deviceContext, DX.m_rasterState[0]);  // 0 = normal rendering, 1 = for shadows

  // set viewport
  {
    D3D11_VIEWPORT viewport;

    clear viewport;
    viewport.Width = (float)g_back_buffer_x_res;
    viewport.Height = (float)g_back_buffer_y_res;
    // viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    // viewport.TopLeftX = 0.0f;
    // viewport.TopLeftY = 0.0f;
    DX.m_deviceContext->lpVtbl->RSSetViewports (DX.m_deviceContext, 1, &viewport);
  }

  {
    RIGGED_VERTEX    v[12];
    RIGGED_VERTEX_ID rid;
    TEXTURE_ID       tid;
    BONE_BUFFER_ID   bone_id;
    matrix44[4]      bones;
    IMAGE_INFO       img;
    int              i;

    clear v;
    v[0].c = {0.0, -0.5, 0.5};
    v[0].n = {1.0, 0.0, 0.0};
    v[0].u = {1.0, 0.0, 0.0};
    v[0].t = {0.125, 0.4};
    v[0].bone = {0, 0, 0, 0};
    v[0].weight = {1.0, 0.0, 0.0, 0.0};

    v[1] = v[0];
    v[1].c = {0.0, 0.5, 0.0};
    v[1].t = {0.126, 0.4};

    v[2] = v[0];
    v[2].c = {0.5/4.0, 0.0, 0.0};
    v[2].t = {0.125, 0.6};

                            // red uses bone 0

    v[3:3] = v[0:3];
    for (i=3; i<6; i++)     // green uses bone 3
    {
      v[i].t[0] += 0.25;
      v[i].bone[0] = 3;
    }

    v[6:3] = v[3:3];
    for (i=6; i<9; i++)    // blue uses bone 1
    {
      v[i].t[0] += 0.25;
      v[i].bone[0] = 1;
    }

    v[9:3] = v[6:3];
    for (i=9; i<12; i++)   // white uses bone 2
    {
      v[i].t[0] += 0.25;
      v[i].bone[0] = 2;
    }

    rid = create_rigged_vertex_buffer (v);

    clear bones;
    bones[0] = M44_ONE;
    bones[1] = M44_ONE;
    bones[2] = M44_ONE;
    bones[3] = M44_ONE;

    bones[0][3][0] = 0.0;
    bones[1][3][0] = 0.125 * 1.0;
    bones[2][3][0] = 0.125 * 2.0;
    bones[3][3][0] = 0.125 * 3.0;

    bone_id = create_bone_buffer (nb_bones => 4);
    write_bone_buffer (bone_id, bones);

    img = {pixel => new byte[16], width => 4, height => 1};
    img.pixel^ = {255, 0,   0, 255,
                    0, 255, 0, 255,
                    0, 0, 255, 255,
                  255,255,255,255};

    tid = create_texture (img,
                          allow_update   => false,
                          use_mipmapping => true,
                          rgb_to_linear_conversion => false);

    draw_rigged_triangles (rid, 0, v'length, bone_id, {tid,0,0});

    free img.pixel;
    free_texture (tid);
    free_bone_buffer (bone_id);
    free_rigged_vertex_buffer (rid);
  }

  //------------------------------

  if (g_flip_mode && g_sampling_count > 1)   // mode FLIP and MSAA
  {
    HRESULT          result;
    ID3D11Resource*  backBufferPtr;

    // Get the pointer to the first back buffer.
    result = DX.m_swapChain->lpVtbl->GetBuffer (DX.m_swapChain, 0, IID_ID3D11Texture2D, (LPVOID*)&backBufferPtr);
    if (result < 0)
      fatal_abort ("GetBuffer()", result);

    DX.m_deviceContext->lpVtbl->ResolveSubresource (This           => DX.m_deviceContext,
                                                    pDstResource   => (ID3D11Resource *)backBufferPtr,
                                                    DstSubresource => 0,
                                                    pSrcResource   => (ID3D11Resource *)DX.m_msaaRenderTarget,
                                                    SrcSubresource => 0,
                                                    Format         => DXGI_FORMAT_R8G8B8A8_UNORM);

    // Release pointer to the back buffer as we no longer need it.
    backBufferPtr->lpVtbl->Release((LPVOID)backBufferPtr);
  }

  // queue an asynchronous command to copy the picking buffer to the picking output buffer
  if (g_sampling_count == 1)
  {
    DX.m_deviceContext->lpVtbl->CopyResource (This         => DX.m_deviceContext,
                                              pDstResource => (ID3D11Resource *)DX.m_pickingOutputBuffer,
                                              pSrcResource => (ID3D11Resource *)DX.m_pickingBuffer);
  }
  else   // multisample
  {
    DX.m_deviceContext->lpVtbl->ResolveSubresource (This           => DX.m_deviceContext,
                                                    pDstResource   => (ID3D11Resource *)DX.m_temp_multisampling,
                                                    DstSubresource => 0,
                                                    pSrcResource   => (ID3D11Resource *)DX.m_pickingBuffer,
                                                    SrcSubresource => 0,
                                                    Format         => DXGI_FORMAT_R8G8B8A8_UNORM);

    DX.m_deviceContext->lpVtbl->CopyResource (This         => DX.m_deviceContext,
                                              pDstResource => (ID3D11Resource *)DX.m_pickingOutputBuffer,
                                              pSrcResource => (ID3D11Resource *)DX.m_temp_multisampling);
  }

  DX.m_swapChain->lpVtbl->Present (DX.m_swapChain, (uint)0, 0);

  {
    int w = (int)g_back_buffer_x_res;
    int h = (int)g_back_buffer_y_res;
    const uint[4] EXPECTED = {0x0000ff, 0xff0000, 0xffffff, 0x00ff00};
    int  i;

    for (i=0; i<4; i++)
    {
      if (get_picking_object_id (x => w/2 + w/16 + i*w/8, y => h/2) != EXPECTED[i])
        break;
    }

    arrays_supported = (i == 4);
  }

  trace ("info: gpu supports dynamic arrays = %s\n", arrays_supported'string);

  return arrays_supported;
}

//---------------------------------------------------------------------

void user_rendering (FUNC prepare_frame, FUNC user_render_shadow, FUNC user_render_3D, FUNC user_render_2D)
{
  //------------------------------

  if (g_screen_was_resized)
  {
    g_screen_was_resized = false;

    g_back_buffer_x_res = (UINT)g_new_width;
    g_back_buffer_y_res = (UINT)g_new_height;

    handle_screen_resize ();
  }

  //------------------------------

  if (g_projection_matrix_needs_update)  // first run, or screen was resized, or far_z/near_z was changed.
  {
    g_projection_matrix_needs_update = false;

    // Build a left-handed perspective projection matrix based on a field of view.
    D3DXMMatrixPerspectiveFovLH
       (&g_default_projection_matrix,
        45.0 * (float)(PI / 180.0),                                // the horizontal field of view
        (float)g_back_buffer_x_res / (float)g_back_buffer_y_res,   // aspect ratio
        zn => g_far_z,                                             // the far view-plane (REVERSE NEAR FAR !!)
        zf => g_near_z);                                           // the near view-plane (REVERSE NEAR FAR !!)

    g_world_to_screen_factor = (float)g_back_buffer_y_res * g_default_projection_matrix.m[1][1];
    build_view_frustum (*(matrix44*)&g_default_view_matrix, *(matrix44*)&g_default_projection_matrix, out g_frustum);
  }

  //------------------------------

  // clear the back buffers to background color.

  if (g_flip_mode && g_sampling_count > 1)   // mode FLIP and MSAA
    DX.m_deviceContext->lpVtbl->ClearRenderTargetView (DX.m_deviceContext, DX.m_msaaRenderTargetView, g_background_color);  // screen
  else
    DX.m_deviceContext->lpVtbl->ClearRenderTargetView (DX.m_deviceContext, DX.m_renderTargetView[0], g_background_color);  // screen

  DX.m_deviceContext->lpVtbl->ClearRenderTargetView (DX.m_deviceContext, DX.m_renderTargetView[1], g_float_initial_picking); // picking buffer

  // clear the depth/stencil buffer
  DX.m_deviceContext->lpVtbl->ClearDepthStencilView (DX.m_deviceContext, DX.m_DepthStencil_View, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, INITIAL_DEPTH_VALUE, INITIAL_STENCIL_VALUE);

  //------------------------------

  prepare_frame ();   // set camera (generates g_default_view_matrix, g_default_camera_eye), create new & delete old objects, ..

  //------------------------------

  if (g_shadows_level != g_next_frame_shadows_level)
  {
    if (!g_shadow_failure)
    {
      free_shadow_resources ();
      g_shadows_level = g_next_frame_shadows_level;
    }
  }

  //------------------------------

  if (g_shadows_level > 0)
  {
    if (user_render_shadow != null)
    {
      if (DX.m_Shadow_depth_View == null)
      {
        if (allocate_shadow_ressources (trial => 1) < 0 &&
            allocate_shadow_ressources (trial => 2) < 0)
        {
          g_shadows_level = 0;       // turn off shadows !
          g_shadow_failure = true;   // never try again to prevent reallocating shadow ressources at each frame.
        }
      }

      if (DX.m_Shadow_depth_View != null)
      {
        // clear the shadow DepthStencil buffer
        DX.m_deviceContext->lpVtbl->ClearDepthStencilView (DX.m_deviceContext, DX.m_Shadow_depth_View, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, INITIAL_DEPTH_VALUE, INITIAL_STENCIL_VALUE);

        // Bind the render target view (0 output buffers) and DepthStencil buffer to the output render pipeline.
        DX.m_deviceContext->lpVtbl->OMSetRenderTargets (DX.m_deviceContext, 0, null, DX.m_Shadow_depth_View);

        clear g_shader_constants_stable;   // clear all constants, including sun, lights, fog, ..
        g_shader_constants_stable.eyePosition = g_default_camera_eye;   // used for specular light
        g_shader_constants_stable.CameraOffset = g_world_position;
        MXXMatrixTranspose (g_default_view_matrix,       out g_shader_constants_stable.TransposedView);
        MXXMatrixTranspose (g_default_projection_matrix, out g_shader_constants_stable.TransposedProjection);
        MXXMatrixTranspose (g_default_shadow_matrix,     out g_shader_constants_stable.ShadowTransposedTransform);
        g_shader_constants_stable.nb_of_shadow_maps        = g_shadows_level;
        g_shader_constants_stable.shader_supports_indexing = g_shader_supports_indexing ? 1 : 0;

        g_shader_constants_stable_dirty = true;
        g_shader_constants_dynamic = DEFAULT_DYNAMIC_CONSTANT;

        g_depth_stencil_buffer_mode = 3;  // depth buffer : test + write    stencil buffer : off
        intern_set_depth_stencil_state ();

        set_blending (BLENDING_OFF);

        g_shader_pass = SHADER_PASS_BUILD_SHADOW_MAP;
        g_next_geometry_shader = g_shader_pass + MAX_SHADER_PASSES * GEOMETRY_SHADER_DEFAULT;
        select_new_geometry_shader ();

        g_nb_of_instances = g_shadows_level;

        // set the rasterizer state
        DX.m_deviceContext->lpVtbl->RSSetState (DX.m_deviceContext, DX.m_rasterState[1]);  // 0 = normal rendering, 1 = for shadows

        // set white 1x1 texture
        DX.m_deviceContext->lpVtbl->PSSetShaderResources (DX.m_deviceContext, 0, 1, (ID3D11ShaderResourceView **)&g_1_pixel_texture);

        // set viewport
        {
          D3D11_VIEWPORT viewport;

          clear viewport;
          viewport.Width = SHADOW_MAP_SIZE;
          viewport.Height = SHADOW_MAP_SIZE;
          // viewport.MinDepth = 0.0f;
          viewport.MaxDepth = 1.0f;
          // viewport.TopLeftX = 0.0f;
          // viewport.TopLeftY = 0.0f;
          DX.m_deviceContext->lpVtbl->RSSetViewports (DX.m_deviceContext, 1, &viewport);
        }

        user_render_shadow ();
      }
    }
  }

  //------------------------------

  {
    clear g_shader_constants_stable;   // clear all constants, including sun, lights, fog, ..
    g_shader_constants_stable.eyePosition = g_default_camera_eye;   // used for specular light
    g_shader_constants_stable.CameraOffset = g_world_position;
    MXXMatrixTranspose (g_default_view_matrix, out g_shader_constants_stable.TransposedView);
    MXXMatrixTranspose (g_default_projection_matrix, out g_shader_constants_stable.TransposedProjection);
    if (g_shadows_level > 0)
      MXXMatrixTranspose (g_default_shadow_matrix, out g_shader_constants_stable.ShadowTransposedTransform);
    g_shader_constants_stable.nb_of_shadow_maps        = g_shadows_level;
    g_shader_constants_stable.shader_supports_indexing = g_shader_supports_indexing ? 1 : 0;
    g_shader_constants_stable_dirty = true;
    g_shader_constants_dynamic = DEFAULT_DYNAMIC_CONSTANT;

    g_depth_stencil_buffer_mode = 3;
    intern_set_depth_stencil_state ();

    set_blending (BLENDING_TRANSLUCID);

    g_shader_pass = SHADER_PASS_RENDERING;
    g_next_geometry_shader = g_shader_pass + MAX_SHADER_PASSES * GEOMETRY_SHADER_DEFAULT;
    select_new_geometry_shader ();

    g_nb_of_instances = 1;

    // Bind the render target view (back buffer, picking buffer) and DepthStencil buffer to the output render pipeline.
    if (g_flip_mode && g_sampling_count > 1)   // mode FLIP and MSAA
    {
      ID3D11RenderTargetView* ptr[2] = {DX.m_msaaRenderTargetView, DX.m_renderTargetView[1]};
      DX.m_deviceContext->lpVtbl->OMSetRenderTargets (DX.m_deviceContext, 2, &ptr, DX.m_DepthStencil_View);
    }
    else
      DX.m_deviceContext->lpVtbl->OMSetRenderTargets (DX.m_deviceContext, 2, &DX.m_renderTargetView, DX.m_DepthStencil_View);

    // set the rasterizer state
    DX.m_deviceContext->lpVtbl->RSSetState (DX.m_deviceContext, DX.m_rasterState[0]);  // 0 = normal rendering, 1 = for shadows

    // set viewport
    {
      D3D11_VIEWPORT viewport;

      clear viewport;
      viewport.Width = (float)g_back_buffer_x_res;
      viewport.Height = (float)g_back_buffer_y_res;
      // viewport.MinDepth = 0.0f;
      viewport.MaxDepth = 1.0f;
      // viewport.TopLeftX = 0.0f;
      // viewport.TopLeftY = 0.0f;
      DX.m_deviceContext->lpVtbl->RSSetViewports (DX.m_deviceContext, 1, &viewport);
    }

    user_render_3D ();
  }

  //------------------------------

  if (user_render_2D != null)
  {
    clear g_shader_constants_stable;   // clear all constants, including sun, lights, fog, ..
    g_shader_constants_stable.TransposedView = g_identityMatrix;
    MXXMatrixTranspose (g_default_ortho_matrix, out g_shader_constants_stable.TransposedProjection);
    g_shader_constants_stable.shader_supports_indexing = g_shader_supports_indexing ? 1 : 0;
    g_shader_constants_stable_dirty = true;
    g_shader_constants_dynamic = DEFAULT_DYNAMIC_CONSTANT;

    g_depth_stencil_buffer_mode = 0;
    intern_set_depth_stencil_state ();

    set_blending (BLENDING_TRANSLUCID);

    g_shader_pass = SHADER_PASS_RENDERING;

    user_render_2D ();
  }
}

//---------------------------------------------------------------------

void rendering_loop ()
{
  g_shader_supports_indexing = are_dynamic_arrays_supported ();

  for (;;)
  {
    g_frame_tick = ticks();

    user_rendering (g_callback_func.prepare_frame,
                    g_callback_func.render_shadow,
                    g_callback_func.render_3D,
                    g_callback_func.render_2D);

    if (g_flip_mode && g_sampling_count > 1)   // mode FLIP and MSAA
    {
      HRESULT          result;
      ID3D11Resource*  backBufferPtr;

      // Get the pointer to the first back buffer.
      result = DX.m_swapChain->lpVtbl->GetBuffer (DX.m_swapChain, 0, IID_ID3D11Texture2D, (LPVOID*)&backBufferPtr);
      if (result < 0)
        fatal_abort ("GetBuffer()", result);

      DX.m_deviceContext->lpVtbl->ResolveSubresource (This           => DX.m_deviceContext,
                                                      pDstResource   => (ID3D11Resource *)backBufferPtr,
                                                      DstSubresource => 0,
                                                      pSrcResource   => (ID3D11Resource *)DX.m_msaaRenderTarget,
                                                      SrcSubresource => 0,
                                                      Format         => DXGI_FORMAT_R8G8B8A8_UNORM);

      // Release pointer to the back buffer as we no longer need it.
      backBufferPtr->lpVtbl->Release((LPVOID)backBufferPtr);
    }

    // queue an asynchronous command to copy the picking buffer to the picking output buffer
    if (g_sampling_count == 1)
    {
      DX.m_deviceContext->lpVtbl->CopyResource (This         => DX.m_deviceContext,
                                                pDstResource => (ID3D11Resource *)DX.m_pickingOutputBuffer,
                                                pSrcResource => (ID3D11Resource *)DX.m_pickingBuffer);
    }
    else   // multisample
    {
      DX.m_deviceContext->lpVtbl->ResolveSubresource (This           => DX.m_deviceContext,
                                                      pDstResource   => (ID3D11Resource *)DX.m_temp_multisampling,
                                                      DstSubresource => 0,
                                                      pSrcResource   => (ID3D11Resource *)DX.m_pickingBuffer,
                                                      SrcSubresource => 0,
                                                      Format         => DXGI_FORMAT_R8G8B8A8_UNORM);

      DX.m_deviceContext->lpVtbl->CopyResource (This         => DX.m_deviceContext,
                                                pDstResource => (ID3D11Resource *)DX.m_pickingOutputBuffer,
                                                pSrcResource => (ID3D11Resource *)DX.m_temp_multisampling);
    }

    DX.m_swapChain->lpVtbl->Present (DX.m_swapChain, (uint)g_wait_for_sync, 0);

    if (g_stop_rendering)
      break;
  }

  g_callback_func.cleanup_objects();
  g_state = STOPPED;
}

//---------------------------------------------------------------------

public void limit_fps (bool on)   // default: on. (set to off if we don't want to wait for the monitor vertical sync)
{
  g_wait_for_sync = on;
}

//---------------------------------------------------------------------

public int start_DirectX_rendering_thread
   (CALLBACK_FUNCTIONS_3D func,
    bool debug = false,
    bool use_warp = false,
    int  antialiasing = 1,         // 1, 2, 4, 8, 16
    bool old_discard_mode = false) // use old DISCARD instead of more efficient FLIP_DISCARD
{
  int rc;

  if (g_state == RUNNING)
    return 0;

  g_callback_func = func;

  g_sampling_count = (UINT)antialiasing;
  if (g_sampling_count != 1 && g_sampling_count != 2 && g_sampling_count != 4 && g_sampling_count != 8 && g_sampling_count != 16)
    g_sampling_count = 1;

  if (init_DirectX (debug, use_warp, old_discard_mode) < 0)
    return -1;

  g_stop_rendering = false;
  g_state          = RUNNING;
  g_frame_tick     = ticks();
  g_wait_for_sync  = true;

  g_callback_func.init_objects ();

  // start rendering loop
  rc = run rendering_loop ();
  assert rc == 0;

  return 0;
}

//---------------------------------------------------------------------

public void stop_DirectX_rendering_thread ()
{
  if (g_state == STOPPED)
    return;

  g_stop_rendering = true;
  while (g_state != STOPPED)
    sleep 0.25;

  close_DirectX ();
}

//---------------------------------------------------------------------

public VERTEX_ID create_vertex_buffer (VERTEX[] v, bool allow_update)
{
  D3D11_BUFFER_DESC      vertexBufferDesc;
  D3D11_SUBRESOURCE_DATA vertexData;
  HRESULT                result;
  ID3D11Buffer*          vertexBuffer;

  // Set up the description of the static vertex buffer.
  clear vertexBufferDesc;
  vertexBufferDesc.Usage = allow_update ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
  vertexBufferDesc.ByteWidth = v'size;
  vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vertexBufferDesc.CPUAccessFlags = allow_update ? D3D11_CPU_ACCESS_WRITE : 0;
  // vertexBufferDesc.MiscFlags = 0;
  // vertexBufferDesc.StructureByteStride = 0;

  // Give the subresource structure a pointer to the vertex data.
  clear vertexData;
  vertexData.pSysMem = (byte*)&v[0];
  // vertexData.SysMemPitch = 0;
  // vertexData.SysMemSlicePitch = 0;

  // Now create the vertex buffer.
  result = DX.m_device->lpVtbl->CreateBuffer (DX.m_device, &vertexBufferDesc, &vertexData, &vertexBuffer);
  if (result < 0)
    fatal_abort ("CreateBuffer()", result);

  return *((VERTEX_ID *)&vertexBuffer);
}

//---------------------------------------------------------------------

public INDEX_ID create_large_index_buffer (uint4[] i, bool allow_update)
{
  D3D11_BUFFER_DESC      indexBufferDesc;
  D3D11_SUBRESOURCE_DATA indexData;
  HRESULT                result;
  ID3D11Buffer*          indexBuffer;

  // Set up the description of the static index buffer.
  clear indexBufferDesc;
  indexBufferDesc.Usage = allow_update ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
  indexBufferDesc.ByteWidth = i'size;
  indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  indexBufferDesc.CPUAccessFlags = allow_update ? D3D11_CPU_ACCESS_WRITE : 0;
  // indexBufferDesc.MiscFlags = 0;
  // indexBufferDesc.StructureByteStride = 0;

  // Give the subresource structure a pointer to the index data.
  clear indexData;
  indexData.pSysMem = (byte*)&i[0];
  // indexData.SysMemPitch = 0;
  // indexData.SysMemSlicePitch = 0;

  // Now create the vertex buffer.
  result = DX.m_device->lpVtbl->CreateBuffer (DX.m_device, &indexBufferDesc, &indexData, &indexBuffer);
  if (result < 0)
    fatal_abort ("CreateBuffer()", result);

  return *((INDEX_ID *)&indexBuffer);
}

//---------------------------------------------------------------------

public INDEX_ID create_small_index_buffer (uint2[] i, bool allow_update)
{
  D3D11_BUFFER_DESC      indexBufferDesc;
  D3D11_SUBRESOURCE_DATA indexData;
  HRESULT                result;
  ID3D11Buffer*          indexBuffer;

  // Set up the description of the static index buffer.
  clear indexBufferDesc;
  indexBufferDesc.Usage = allow_update ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
  indexBufferDesc.ByteWidth = i'size;
  indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  indexBufferDesc.CPUAccessFlags = allow_update ? D3D11_CPU_ACCESS_WRITE : 0;
  //  indexBufferDesc.MiscFlags = 0;
  //  indexBufferDesc.StructureByteStride = 0;

  // Give the subresource structure a pointer to the index data.
  clear indexData;
  indexData.pSysMem = (byte*)&i[0];
  // indexData.SysMemPitch = 0;
  // indexData.SysMemSlicePitch = 0;

  // Now create the vertex buffer.
  result = DX.m_device->lpVtbl->CreateBuffer (DX.m_device, &indexBufferDesc, &indexData, &indexBuffer);
  if (result < 0)
    fatal_abort ("CreateBuffer()", result);

  return 1 + *((INDEX_ID *)&indexBuffer);    // add 1 to make address odd
}

//---------------------------------------------------------------------

public void update_vertex_buffer (VERTEX_ID v, int offset, VERTEX[] vb, BUFFER_UPDATE_FLAG flag = OVERWRITE_ALL)
{
  D3D11_MAP                f;
  ID3D11Resource*          pResource;
  D3D11_MAPPED_SUBRESOURCE mappedResource;

  switch (flag)
  {
    case OVERWRITE_ALL:
      f = D3D11_MAP_WRITE_DISCARD;
      break;
    case OVERWRITE_PART:
      f = D3D11_MAP_WRITE;
      break;
    case APPEND_DATA:
      f = D3D11_MAP_WRITE_NO_OVERWRITE;
      break;
    default:
      abort;
  }

  pResource'byte = v'byte;

  DX.m_deviceContext->lpVtbl->Map (DX.m_deviceContext, pResource, 0, f, 0, &mappedResource);
  mappedResource.pData[(uint)offset * VERTEX'size : vb'size] = vb'byte;
  DX.m_deviceContext->lpVtbl->Unmap (DX.m_deviceContext, pResource, 0);
}

//---------------------------------------------------------------------

public void update_small_index_buffer (INDEX_ID id, uint offset, uint2[] ib, BUFFER_UPDATE_FLAG flag = OVERWRITE_ALL)
{
  INDEX_ID                 id2 = id;
  D3D11_MAP                f;
  ID3D11Resource*          pResource;
  D3D11_MAPPED_SUBRESOURCE mappedResource;

  switch (flag)
  {
    case OVERWRITE_ALL:
      f = D3D11_MAP_WRITE_DISCARD;
      break;
    case OVERWRITE_PART:
      f = D3D11_MAP_WRITE;
      break;
    case APPEND_DATA:
      f = D3D11_MAP_WRITE_NO_OVERWRITE;
      break;
    default:
      abort;
  }

  assert (id2 & 1) == 1;
  id2--;

  pResource'byte = id2'byte;

  DX.m_deviceContext->lpVtbl->Map (DX.m_deviceContext, pResource, 0, f, 0, &mappedResource);
  mappedResource.pData[(uint)offset * uint2'size : ib'size] = ib'byte;
  DX.m_deviceContext->lpVtbl->Unmap (DX.m_deviceContext, pResource, 0);
}

//---------------------------------------------------------------------

public void update_large_index_buffer (INDEX_ID id, uint offset, uint4[] ib, BUFFER_UPDATE_FLAG flag = OVERWRITE_ALL)
{
  D3D11_MAP                f;
  ID3D11Resource*          pResource;
  D3D11_MAPPED_SUBRESOURCE mappedResource;

  switch (flag)
  {
    case OVERWRITE_ALL:
      f = D3D11_MAP_WRITE_DISCARD;
      break;
    case OVERWRITE_PART:
      f = D3D11_MAP_WRITE;
      break;
    case APPEND_DATA:
      f = D3D11_MAP_WRITE_NO_OVERWRITE;
      break;
    default:
      abort;
  }

  assert (id & 1) == 0;
  pResource'byte = id'byte;

  DX.m_deviceContext->lpVtbl->Map (DX.m_deviceContext, pResource, 0, f, 0, &mappedResource);
  mappedResource.pData[(uint)offset * uint4'size : ib'size] = ib'byte;
  DX.m_deviceContext->lpVtbl->Unmap (DX.m_deviceContext, pResource, 0);
}

//---------------------------------------------------------------------

void flush_constant_buffers ()
{
  if (g_shader_constants_stable_dirty)
  {
    write_constant_buffer_stable (g_shader_constants_stable'byte
           [0 : g_shader_constants_stable'size
                - g_shader_constants_stable.Light'size
                + (uint)g_shader_constants_stable.NbLights * LIGHT'size]);
    g_shader_constants_stable_dirty = false;
  }

  write_constant_buffer_dynamic (g_shader_constants_dynamic);
}

//---------------------------------------------------------------------

void set_resources (TEXTURE_ID t_id[3],   // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
                    uint       object_id)
{
  if (g_shader_pass == SHADER_PASS_RENDERING)   // actual rendering pass (after shadows)
  {
    // set texture
    {
      ID3D11ShaderResourceView* t[4] = DX.m_DefaultTextures;
      bool                      has_bump, has_mro;

      if (t_id[0] != 0)
        t[0] = *(ID3D11ShaderResourceView **)&t_id[0];

      has_bump = (t_id[1] != 0);
      g_shader_constants_dynamic.has_bump_texture = (uint)has_bump;
      if (has_bump)
        t[1] = *(ID3D11ShaderResourceView **)&t_id[1];

      has_mro = (t_id[2] != 0);
      g_shader_constants_dynamic.has_mro_texture = (uint)has_mro;
      if (has_mro)
        t[2] = *(ID3D11ShaderResourceView **)&t_id[2];

      DX.m_deviceContext->lpVtbl->PSSetShaderResources
         (DX.m_deviceContext,
          0,
          (UINT)DX.m_DefaultTextures'length,   // 4 textures for final rendering  with shadows on
          &t);
    }

    if (object_id >= 0xFFFFFF)   // means no object_id provided
    {
      clear g_shader_constants_dynamic.objectid;   // zero alpha means no object_id provided
    }
    else
    {
      g_shader_constants_dynamic.objectid = {(float)(object_id'byte[0]) * (1.0 / 255.0),
                                             (float)(object_id'byte[1]) * (1.0 / 255.0),
                                             (float)(object_id'byte[2]) * (1.0 / 255.0),
                                             1.0};    // 1.0 means means valid object_id (will pass blending without change)
    }
  }
}

//---------------------------------------------------------------------

public
void draw_indexed_triangles (VERTEX_ID  v_id,
                             int        v_offset,
                             INDEX_ID   i_id,
                             int        i_offset,
                             int        i_count,
                             TEXTURE_ID t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
                             uint       object_id = 0xFFFFFF)
{
  INDEX_ID i_id2 = i_id;
  uint     stride, offset;

  i_id2 &= -2;    // set even

  g_next_vertex_shader = g_shader_pass + MAX_SHADER_PASSES * VERTEX_SHADER_NORMAL;
  g_next_pixel_shader = g_shader_pass + MAX_SHADER_PASSES * PIXEL_SHADER_SOLID;

  select_new_vertex_shader ();
  select_new_pixel_shader ();

  // set vertex buffer
  stride = VERTEX'size;
  offset = 0;
  DX.m_deviceContext->lpVtbl->IASetVertexBuffers (DX.m_deviceContext, 0, 1, ((ID3D11Buffer **)&v_id), &stride, &offset);

  // set index buffer
  DX.m_deviceContext->lpVtbl->IASetIndexBuffer (DX.m_deviceContext,
                                                *((ID3D11Buffer **)&i_id2),
                                                (i_id & 1) == 0 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT,
                                                0);

  set_resources (t_id, object_id);
  flush_constant_buffers ();

  // Render the triangles.
  DX.m_deviceContext->lpVtbl->DrawIndexedInstanced (This                  => DX.m_deviceContext,
                                                    IndexCountPerInstance => (UINT)i_count,
                                                    InstanceCount         => (UINT)g_nb_of_instances,
                                                    StartIndexLocation    => (UINT)i_offset,
                                                    BaseVertexLocation    => v_offset,
                                                    StartInstanceLocation => 0);
}

//---------------------------------------------------------------------

public void draw_triangles (VERTEX_ID  v_id,
                            int        v_offset,
                            int        v_count,
                            TEXTURE_ID t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
                            uint       object_id = 0xFFFFFF)
{
  uint stride, offset;

  g_next_vertex_shader = g_shader_pass + MAX_SHADER_PASSES * VERTEX_SHADER_NORMAL;
  g_next_pixel_shader = g_shader_pass + MAX_SHADER_PASSES * PIXEL_SHADER_SOLID;

  select_new_vertex_shader ();
  select_new_pixel_shader ();

  // set vertex buffer
  stride = VERTEX'size;
  offset = 0;
  DX.m_deviceContext->lpVtbl->IASetVertexBuffers (DX.m_deviceContext, 0, 1, ((ID3D11Buffer **)&v_id), &stride, &offset);

  set_resources (t_id, object_id);
  flush_constant_buffers ();

  // Render the triangles.
  DX.m_deviceContext->lpVtbl->DrawInstanced (DX.m_deviceContext,
                                             VertexCountPerInstance => (UINT)v_count,
                                             InstanceCount          => (UINT)g_nb_of_instances,
                                             StartVertexLocation    => (UINT)v_offset,
                                             StartInstanceLocation  => 0);
}

//---------------------------------------------------------------------

public
TEXTURE_ID create_texture (IMAGE_INFO img,
                           bool       allow_update = false,
                           bool       use_mipmapping = true,
                           bool       rgb_to_linear_conversion = true)  // GPU will converts RGB to linear
{
  HRESULT                         hResult;
  D3D11_SUBRESOURCE_DATA          subresource;
  D3D11_TEXTURE2D_DESC            textureDesc;
  ID3D11Texture2D*                m_texture;
  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  ID3D11ShaderResourceView*       m_textureView;

  clear subresource;
  subresource.pSysMem = &img.pixel^;
  subresource.SysMemPitch = img.width*4;

  // Setup the description of the texture.
  clear textureDesc;
  textureDesc.Height = img.height;
  textureDesc.Width = img.width;
  textureDesc.MipLevels = (UINT)(use_mipmapping ? 0 : 1);
  textureDesc.ArraySize = 1;
  textureDesc.Format = rgb_to_linear_conversion
                          ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB // converts SRGB to linear color space (for albedo texture)
                          : DXGI_FORMAT_R8G8B8A8_UNORM;     // no conversion (for bump, metallic, ..)

  textureDesc.SampleDesc.Count = 1;
//textureDesc.SampleDesc.Quality = 0;

  if (use_mipmapping)
  {
    textureDesc.Usage = D3D11_USAGE_DEFAULT;      // inaccessible to the CPU (the only one that works with mipmapping)
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
  }
  else
  {
    textureDesc.Usage = allow_update ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = allow_update ? D3D11_CPU_ACCESS_WRITE : 0;
  }

  hResult = DX.m_device->lpVtbl->CreateTexture2D (DX.m_device, &textureDesc, (use_mipmapping ? null : &subresource), &m_texture);
  if (hResult < 0)
    fatal_abort ("CreateTexture2D()", hResult);

  clear srvDesc;
  srvDesc.Format = textureDesc.Format;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  // srvDesc.array.Texture2D.MostDetailedMip = 0;
  srvDesc.array.Texture2D.MipLevels = (UINT)(use_mipmapping ? UINT'max : 1);

  hResult = DX.m_device->lpVtbl->CreateShaderResourceView (DX.m_device, *(ID3D11Resource**)&m_texture, &srvDesc, &m_textureView);
  if (hResult < 0)
    fatal_abort ("CreateShaderResourceView()", hResult);

  if (use_mipmapping)
  {
    DX.m_deviceContext->lpVtbl->UpdateSubresource(DX.m_deviceContext, *(ID3D11Resource**)&m_texture, 0, null, subresource.pSysMem, subresource.SysMemPitch, 0);
    DX.m_deviceContext->lpVtbl->GenerateMips (DX.m_deviceContext, m_textureView);
  }

  m_texture->lpVtbl->Release((LPVOID)m_texture);

  return *((TEXTURE_ID *)&m_textureView);
}

//---------------------------------------------------------------------

public void update_texture (TEXTURE_ID id, IMAGE_INFO img)
{
  ID3D11ShaderResourceView* texture = *((ID3D11ShaderResourceView**)&id);
  ID3D11Resource*           res;
  LPVOID                    ptr;
  ID3D11Texture2D*          m_texture;
  D3D11_TEXTURE2D_DESC      desc;

  texture->lpVtbl->GetResource (texture, &res);
  res->lpVtbl->QueryInterface ((LPVOID)res, IID_ID3D11Texture2D, out ptr);
  assert ptr != null;
  m_texture'byte = ptr'byte;

  m_texture->lpVtbl->GetDesc (This => m_texture, &desc);

  assert img.width == desc.Width && img.height == desc.Height;

  DX.m_deviceContext->lpVtbl->UpdateSubresource(DX.m_deviceContext, *(ID3D11Resource**)&m_texture, 0, null, &img.pixel^, img.width*4, 0);
  DX.m_deviceContext->lpVtbl->GenerateMips (DX.m_deviceContext, texture);

  m_texture->lpVtbl->Release((LPVOID)m_texture);
  res->lpVtbl->Release((LPVOID)res);
}

//---------------------------------------------------------------------

public void update_texture_part (TEXTURE_ID id, uint[2] offset, IMAGE_INFO img)
{
  ID3D11ShaderResourceView* texture = *((ID3D11ShaderResourceView**)&id);
  ID3D11Resource*           res;
  LPVOID                    ptr;
  ID3D11Texture2D*          m_texture;
  D3D11_TEXTURE2D_DESC      desc;
  D3D11_BOX                 box;

  texture->lpVtbl->GetResource (texture, &res);
  res->lpVtbl->QueryInterface ((LPVOID)res, IID_ID3D11Texture2D, out ptr);
  assert ptr != null;
  m_texture'byte = ptr'byte;

  m_texture->lpVtbl->GetDesc (This => m_texture, &desc);

  assert offset[0] + img.width <= desc.Width && offset[1] + img.height <= desc.Height;

#if 1

  clear box;
  box.left = offset[0];
  box.right = box.left + img.width;
  box.top = offset[1];
  box.bottom = box.top + img.height;
  box.back = 1;

  DX.m_deviceContext->lpVtbl->UpdateSubresource
     (DX.m_deviceContext,
      *(ID3D11Resource**)&m_texture,
      0,
      &box,
      &img.pixel^,
      img.width*4,
      0);

#else
  {
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    DX.m_deviceContext->lpVtbl->Map (DX.m_deviceContext, (ID3D11Resource*)m_texture, 0,
                                     D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

    {
      byte* psource = &img.pixel^;
      byte* ptarget = mappedResource.pData;
      uint  size = 4 * img.width;
      uint  ln;

      for (ln=0; ln<img.height; ln++)
      {
        ptarget[0:size] = psource[0:size];
        psource += size;
        ptarget += mappedResource.RowPitch;
      }
    }

    DX.m_deviceContext->lpVtbl->Unmap (DX.m_deviceContext, (ID3D11Resource*)m_texture, 0);
  }
#endif

  DX.m_deviceContext->lpVtbl->GenerateMips (DX.m_deviceContext, texture);

  m_texture->lpVtbl->Release((LPVOID)m_texture);
  res->lpVtbl->Release((LPVOID)res);
}

//---------------------------------------------------------------------

public void free_texture (TEXTURE_ID t)
{
  ID3D11ShaderResourceView* m_texture = *((ID3D11ShaderResourceView **)&t);

  if (m_texture != null)
  {
    m_texture->lpVtbl->Release((LPVOID)m_texture);
    m_texture = null;
  }
}

//---------------------------------------------------------------------

public void free_vertex_buffer (VERTEX_ID v)
{
  ID3D11Buffer* m_vertexBuffer = *((ID3D11Buffer **)&v);

  if (m_vertexBuffer != null)
  {
    m_vertexBuffer->lpVtbl->Release((LPVOID)m_vertexBuffer);
    m_vertexBuffer = null;
  }
}

//---------------------------------------------------------------------

public void free_index_buffer (INDEX_ID i)
{
  INDEX_ID      i2 = (i & -2);   // set even in case it's a small index buffer
  ID3D11Buffer* m_indexBuffer = *((ID3D11Buffer **)&i2);

  if (m_indexBuffer != null)
  {
    m_indexBuffer->lpVtbl->Release((LPVOID)m_indexBuffer);
    m_indexBuffer = null;
  }
}

//---------------------------------------------------------------------

public void set_world_transform (matrix44 m)
{
  transpose_44 (m, out g_shader_constants_dynamic.TransposedWorld);
}

//---------------------------------------------------------------------

public void clear_depth_buffer ()
{
  ID3D11DepthStencilView* view = (g_shader_pass == SHADER_PASS_BUILD_SHADOW_MAP) ? DX.m_Shadow_depth_View : DX.m_DepthStencil_View;
  DX.m_deviceContext->lpVtbl->ClearDepthStencilView (DX.m_deviceContext, view, D3D11_CLEAR_DEPTH, INITIAL_DEPTH_VALUE, INITIAL_STENCIL_VALUE);
}

//---------------------------------------------------------------------

public void clear_stencil_buffer ()
{
  ID3D11DepthStencilView* view = (g_shader_pass == SHADER_PASS_BUILD_SHADOW_MAP) ? DX.m_Shadow_depth_View : DX.m_DepthStencil_View;
  DX.m_deviceContext->lpVtbl->ClearDepthStencilView (DX.m_deviceContext, view, D3D11_CLEAR_STENCIL, INITIAL_DEPTH_VALUE, INITIAL_STENCIL_VALUE);
}

//---------------------------------------------------------------------

public RIGGED_VERTEX_ID create_rigged_vertex_buffer (RIGGED_VERTEX[] v)
{
  D3D11_BUFFER_DESC      vertexBufferDesc;
  D3D11_SUBRESOURCE_DATA vertexData;
  HRESULT                result;
  ID3D11Buffer*          vertexBuffer;

  // Set up the description of the static vertex buffer.
  clear vertexBufferDesc;
  vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
  vertexBufferDesc.ByteWidth = v'size;
  vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
//  vertexBufferDesc.CPUAccessFlags = 0;

  // Give the subresource structure a pointer to the vertex data.
  clear vertexData;
  vertexData.pSysMem = (byte*)&v[0];
// vertexData.SysMemPitch = 0;
// vertexData.SysMemSlicePitch = 0;

  // Now create the vertex buffer.
  result = DX.m_device->lpVtbl->CreateBuffer (DX.m_device, &vertexBufferDesc, &vertexData, &vertexBuffer);
  if (result < 0)
    fatal_abort ("CreateBuffer()", result);

  return *((RIGGED_VERTEX_ID *)&vertexBuffer);
}

//---------------------------------------------------------------------

public void free_rigged_vertex_buffer (RIGGED_VERTEX_ID v)
{
  ID3D11Buffer* m_vertexBuffer = *((ID3D11Buffer **)&v);

  if (m_vertexBuffer != null)
  {
    m_vertexBuffer->lpVtbl->Release((LPVOID)m_vertexBuffer);
    m_vertexBuffer = null;
  }
}

//---------------------------------------------------------------------

public BONE_BUFFER_ID create_bone_buffer (int nb_bones)
{
#if 0
  HRESULT                         hResult;
  D3D11_TEXTURE2D_DESC            textureDesc;
  ID3D11Texture2D*                m_texture;
  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  ID3D11ShaderResourceView*       m_textureView;
  BONE_BUFFER_ID                  id;

  clear textureDesc;
  textureDesc.Width = 4;
  textureDesc.Height = (uint)nb_bones;
  textureDesc.MipLevels = 1;
  textureDesc.ArraySize = 1;
  textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  textureDesc.Usage = D3D11_USAGE_DEFAULT;
  textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  textureDesc.SampleDesc.Count = 1;

  hResult = DX.m_device->lpVtbl->CreateTexture2D (DX.m_device, &textureDesc, null, &m_texture);
  if (hResult < 0)
    fatal_abort ("CreateTexture2D()", hResult);

  clear srvDesc;
  srvDesc.Format = textureDesc.Format;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  // srvDesc.array.Texture2D.MostDetailedMip = 0;
  srvDesc.array.Texture2D.MipLevels = 1;

  hResult = DX.m_device->lpVtbl->CreateShaderResourceView (DX.m_device, *(ID3D11Resource**)&m_texture, &srvDesc, &m_textureView);
  if (hResult < 0)
    fatal_abort ("CreateShaderResourceView()", hResult);

  m_texture->lpVtbl->Release((LPVOID)m_texture);

  id'byte = m_textureView'byte;

  return id;
#else
  HRESULT           result;
  D3D11_BUFFER_DESC constantBufferDesc;
  BONE_BUFFER_ID    id;

  assert nb_bones >= 0 && nb_bones <= 192;

  clear constantBufferDesc;
  constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;    // cpu write, GPU read
  constantBufferDesc.ByteWidth = (uint)(nb_bones * 64);
  constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  result = DX.m_device->lpVtbl->CreateBuffer (DX.m_device, &constantBufferDesc, pInitialData => null, (ID3D11Buffer**)&id);
  if (result < 0)
    fatal_abort ("CreateBuffer()", result);

  return id;
#endif
}

//---------------------------------------------------------------------

public void write_bone_buffer (BONE_BUFFER_ID id, matrix44[] m)
{
#if 0
  ID3D11ShaderResourceView* texture = *((ID3D11ShaderResourceView**)&id);
  ID3D11Resource*           res;
  LPVOID                    ptr;
  ID3D11Texture2D*          m_texture;
  D3D11_TEXTURE2D_DESC      desc;
  D3D11_BOX                 box;

  texture->lpVtbl->GetResource (texture, &res);
  res->lpVtbl->QueryInterface ((LPVOID)res, IID_ID3D11Texture2D, out ptr);
  assert ptr != null;
  m_texture'byte = ptr'byte;

  m_texture->lpVtbl->GetDesc (This => m_texture, &desc);

  clear box;
//  box.left = 0;
  box.right = 4;
//  box.top = 0;
  box.bottom = (uint)m'length;
// box.front = 0;
  box.back = 1;

  DX.m_deviceContext->lpVtbl->UpdateSubresource
     (DX.m_deviceContext,
      *(ID3D11Resource**)&m_texture,
      0,              // Subresource index
      &box,
      (byte*)&m,
      64,             // Row pitch (bytes per row)
      0);

  m_texture->lpVtbl->Release((LPVOID)m_texture);
  res->lpVtbl->Release((LPVOID)res);
#else
  uint size;

  assert m'length <= 192;

  size = (uint)(m'length * 64);

  {
    ID3D11Buffer*            p = *((ID3D11Buffer**)&id);
    HRESULT                  result;
    D3D11_MAPPED_SUBRESOURCE mappedResource;

    // Lock the constant buffer so it can be written to.
    result = DX.m_deviceContext->lpVtbl->Map (DX.m_deviceContext, (ID3D11Resource*)p, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (result < 0)
      fatal_abort ("Map()", result);

    // copy the new values into the constant buffer.
    mappedResource.pData[0 : size] = m'byte[0 : size];

    // Unlock the constant buffer.
    DX.m_deviceContext->lpVtbl->Unmap (DX.m_deviceContext, (ID3D11Resource*)p, 0);
  }
#endif
}

//---------------------------------------------------------------------

public void free_bone_buffer (BONE_BUFFER_ID id)
{
#if 0
  ID3D11ShaderResourceView* m_texture = *((ID3D11ShaderResourceView **)&id);
  if (m_texture != null)
    m_texture->lpVtbl->Release((LPVOID)m_texture);
#else
  ID3D11Buffer* p = *((ID3D11Buffer**)&id);
  if (p != null)
    p->lpVtbl->Release((LPVOID)p);
#endif
}

//---------------------------------------------------------------------

public void draw_rigged_triangles
    (RIGGED_VERTEX_ID  v_id,
     int               v_offset,
     int               v_count,
     BONE_BUFFER_ID    b_id,
     TEXTURE_ID        t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
     uint              object_id = 0xFFFFFF)
{
  uint stride, offset;

  g_next_vertex_shader = g_shader_pass + MAX_SHADER_PASSES * VERTEX_SHADER_RIGGED;
  g_next_pixel_shader = g_shader_pass + MAX_SHADER_PASSES * PIXEL_SHADER_SOLID;

  select_new_vertex_shader (b_id);
  select_new_pixel_shader ();

  stride = RIGGED_VERTEX'size;
  offset = 0;
  DX.m_deviceContext->lpVtbl->IASetVertexBuffers (DX.m_deviceContext, 0, 1, ((ID3D11Buffer **)&v_id), &stride, &offset);

  set_resources (t_id, object_id);
  flush_constant_buffers ();

  // Render the triangles.
  DX.m_deviceContext->lpVtbl->DrawInstanced (DX.m_deviceContext,
                                             VertexCountPerInstance => (UINT)v_count,
                                             InstanceCount          => (UINT)g_nb_of_instances,
                                             StartVertexLocation    => (UINT)v_offset,
                                             StartInstanceLocation  => 0);
}

//---------------------------------------------------------------------

public void draw_indexed_rigged_triangles
    (RIGGED_VERTEX_ID  v_id,
     int               v_offset,
     INDEX_ID          i_id,
     int               i_offset,
     int               i_count,
     BONE_BUFFER_ID    b_id,
     TEXTURE_ID        t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
     uint              object_id = 0xFFFFFF)
{
  INDEX_ID i_id2 = i_id;
  uint     stride, offset;

  i_id2 &= -2;    // set even

  g_next_vertex_shader = g_shader_pass + MAX_SHADER_PASSES * VERTEX_SHADER_RIGGED;
  g_next_pixel_shader = g_shader_pass + MAX_SHADER_PASSES * PIXEL_SHADER_SOLID;

  select_new_vertex_shader (b_id);
  select_new_pixel_shader ();

  // set vertex buffer
  stride = RIGGED_VERTEX'size;
  offset = 0;
  DX.m_deviceContext->lpVtbl->IASetVertexBuffers (DX.m_deviceContext, 0, 1, ((ID3D11Buffer **)&v_id), &stride, &offset);

  // set index buffer
  DX.m_deviceContext->lpVtbl->IASetIndexBuffer (DX.m_deviceContext,
                                                *((ID3D11Buffer **)&i_id2),
                                                (i_id & 1) == 0 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT,
                                                0);

  set_resources (t_id, object_id);
  flush_constant_buffers ();

  // Render the triangles.
  DX.m_deviceContext->lpVtbl->DrawIndexedInstanced (This                  => DX.m_deviceContext,
                                                    IndexCountPerInstance => (UINT)i_count,
                                                    InstanceCount         => (UINT)g_nb_of_instances,
                                                    StartIndexLocation    => (UINT)i_offset,
                                                    BaseVertexLocation    => v_offset,
                                                    StartInstanceLocation => 0);
}

//---------------------------------------------------------------------
#end unsafe

//---------------------------------------------------------------------

public void set_shadows_number_of_levels (int level)  // 0 = off
{
  if (g_next_frame_shadows_level != level)
  {
    int l = level;
    if (l < 0) l = 0;
    if (l > 5) l = 5;
    g_next_frame_shadows_level = l;
  }
}

//-------------------------------------------------------------------------------------------------

public void best_antialiasing_available (out int[5] samples, out int count)
{
  samples = g_best_antialiasing;
  count   = g_best_antialiasing_count;
}

//-------------------------------------------------------------------------------------------------

public int current_antialiasing_setting ()
{
  return (int)g_sampling_count;
}

//-------------------------------------------------------------------------------------------------
#elif ANDROID
//-------------------------------------------------------------------------------------------------

use android/android, android/opengl, android/egl;
use d3dutil, image, linear_algebra, logging, math, strings, thread;

//---------------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------------

CALLBACK_FUNCTIONS_3D g_func;
bool                  g_func_filled;
bool                  g_init_objects_required;

EGLDisplay    g_display;
EGLSurface    g_surface;
EGLContext    g_context;

int           g_back_buffer_x_res, g_back_buffer_y_res;
float         g_background_color[4] = {0.0, 0.0, 0.0, 1.0};
float         g_near_z = 0.0625;  // 6 cm
float         g_far_z  = 1024.0;  // 1024 m

XMMATRIX      g_default_view_matrix;
vector3       g_default_camera_eye;
vector3       g_world_position;
XMMATRIX      g_default_projection_matrix;         // DirectX
XMMATRIX      g_opengl_default_projection_matrix;  // OpenGL
XMMATRIX      g_default_ortho_matrix;              // 3D to 2D screen
XMMATRIX      g_default_dialogs_matrix;
XMMATRIX      g_default_shadow_matrix;
volatile bool g_projection_matrix_needs_update;
vector4       g_frustum[6];
bool          g_shader_constants_stable_dirty;

TEXTURE_ID    g_1_pixel_texture;

int           g_shadows_level;   // 0 = off, 1 to 4 = number of shadow maps

XMMATRIX      g_identityMatrix;

GLuint        g_stableUBO_nr, g_dynamicUBO_nr;
GLuint        g_prog_basic, g_prog_rigged;

int           g_current_shader;   // 0 = basic, 1 = rigged
const int SHADER_BASIC = 0;
const int SHADER_RIGGED = 1;

GLuint        g_my_fbo;

struct FRAME_BUF_IDS
{
  GLuint myRBOCol;
  GLuint myRBOId;
  GLuint mydepthstencilId;
}

FRAME_BUF_IDS g_frame_buffer_ids;

//---------------------------------------------------------------------

packed struct MODEL_VERTEX_ID
{
  GLuint VAOId;
  GLuint VBOId;
}

//---------------------------------------------------------------------

packed struct ShaderConstantsStable  // max allowed size : 16K, must be a multiple of 16 !
{
  XMMATRIX   View;         // 64 bytes, camera position, rotation (changes if camera moves - once per frame)
  XMMATRIX   Projection;   // 64 bytes, 3D to 2D screen (changes if screen is resized or z view changed - rare)
  XMMATRIX   ShadowTransposedTransform;   // 64 bytes

  vector3    eyePosition;   // used for specular lightning (changes if camera moves - once per frame)
  float      fogInvRange;   // 1.0/(end - start) [0 if no fog]

  vector3    fogColor;
  float      fogStart;

  vector3    CameraOffset;
  float      FogInvExponent;

  int[2]     filler1;
  int        nb_of_shadow_maps;
  int        shader_supports_indexing;   // OBSOLETE !

  vector3    AmbiantLightColor;
  float      filler2;

  vector3    SunLightColor;
  float      filler3;

  vector3    SunLightDirection;
  float      filler4;

  int        NbLights;
  int[3]     filler5;

  LIGHT      Light[MAX_LIGHTS];  // 48 x 128 = 6144 bytes
}

//-----------------------------------------------------------------------------------------------

packed struct ShaderConstantsDynamic  // max allowed size : 16K, must be a multiple of 16 !
{
  matrix44   World;     // object position, rotation (changes for each object) (64 bytes)

  float[4]   MaterialColor;       // (16 bytes)

  vector3    EmissiveColor;
  float      metallic;   // 0 .. 1

  OBJECT_UV_TRANSFORM uv_transform;  // 24 bytes
  float      tclock;        // current time in secs, used for perlin noise
  float      smooth;        // 0 .. 1  (avoid 1)

  uint       is_ambiantlight_on;
  uint       is_sunlight_on;
  uint       is_ambiant_downwards;  // 1 = ambiant 10% more intense when normal shines from above
  uint       disable_ambiant_sun_underground;

  uint       filler1a;
  uint       filler2a;
  uint       filler3a;
  float      parallax_factor;       // 0 = none, ex: -0.05

  float      perlin_amplitude;   // 1.0
  float      translucid;         // 0.0 to 1.0
  uint       has_bump_texture;
  uint       has_mro_texture;

  float[4]   object_id;          // unique object id set by user for picking (on 3 bytes only)
}

const ShaderConstantsDynamic DEFAULT_DYNAMIC_CONSTANT =
   {World             => M44_ONE,
    MaterialColor     => {1.0, 1.0, 1.0, 1.0},
    EmissiveColor     => {0.0, 0.0, 0.0},
    metallic          => 0.0,
    uv_transform      => {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    tclock            => 0.0,
    smooth            => 0.0,
    is_ambiantlight_on=> 0,
    is_sunlight_on    => 0,
    is_ambiant_downwards => 0,
    disable_ambiant_sun_underground => 0,
    filler1a          => 0,
    filler2a          => 0,
    filler3a          => 0,
    parallax_factor   => 0.0,
    perlin_amplitude  => 1.0,
    translucid        => 0.0,
    has_bump_texture  => 0,
    has_mro_texture   => 0,
    object_id         => {0.0, 0.0, 0.0, 0.0},
   };

//-----------------------------------------------------------------------------------------------

ShaderConstantsStable   g_shader_constants_stable;
ShaderConstantsDynamic  g_shader_constants_dynamic;

//-----------------------------------------------------------------------------------------------

const string LAYOUT_STABLE =            // all types aligned at M16

  "struct LIGHT " +  // 48 bytes (3 x 16)
  "{ " +
    "vec4   color_range; " +  // used for diffuse and specular
    "vec4   position_att; " +  // position of light
    "vec4   direction_cone; " +  // for spotlight (normal vector "light towards world")
  "}; " +

//---------------------------------------------------------------------

  "layout(std140, binding=0) uniform stable \n" +
  "{ " +
   "mat4  M44View;  "      +   // 64 bytes, camera position, rotation (changes if camera moves - once per frame)
   "mat4  M44Projection; " +   // 64 bytes, 3D to 2D screen (changes if screen is resized or z view changed - rare)
   "mat4  M44ShadowTransposedTransform; " +  // 64 bytes

   "vec4  eyePosition_fogInvRange;  " + // used for specular lightning (changes if camera moves - once per frame)
   "vec4  fogColor_fogstart; " +
   "vec4  CameraOffset_FogInvExponent; " +

   "int   filler1; " +
   "int   filler2; " +
   "int   nb_of_shadow_maps; " +
   "int   shader_supports_indexing; " +    // OBSOLETE !

   "vec4  AmbiantLightColor; " +
   "vec4  SunLightColor; " +
   "vec4  SunLightDirection; " +

   "int   NbLights; " +
   "int   filler3; " +
   "int   filler4; " +
   "int   filler5; " +

   "LIGHT Light[128];  " +
  "};";

//-----------------------------------------------------------------------------------------------

const string LAYOUT_DYNAMIC =            // all types aligned at M16
 "layout(std140, binding=1) uniform dynamic \n" +
 "{ " +
  "mat4  M44World;  "  +
  "vec4  MaterialColor; " +      // (16 bytes)
  "vec4  EmissiveColor_metallic; " +

  "float fact_u_min_1; " +
  "float fact_v_min_1; " +
  "float fact_u; " +
  "float fact_v; " +

  "float ofs_u; " +
  "float ofs_v; " +
  "float tclock;  " +       // current time in secs, used for perlin noise
  "float fsmooth;  " +      // 0 .. 1  (avoid 1)

  "int   is_ambiantlight_on; " +
  "int   is_sunlight_on; " +
  "int   is_ambiant_downwards;  " + // 1 = ambiant 10% more intense when normal shines from above
  "int   disable_ambiant_sun_underground; " +

  "uint  fillerd5; " +
  "int   fillerd6; " +
  "int   fillerd7; " +
  "float parallax_factor;  " +      // 0 = none, ex: -0.05

  "float perlin_amplitude;  " +  // 1.0
  "float translucid;   " +       // 0.0 to 1.0
  "int   has_bump_texture; " +
  "int   has_mro_texture; " +

  "vec4  object_id; " +          // unique object id set by user for picking (on 3 bytes only)
 "};";

//---------------------------------------------------------------------

const string BASIC_VERTEX_SHADER_SOURCE =
  "#version 310 es \n" +
  "precision highp float; \n" +
  "precision highp int; \n" +

  LAYOUT_STABLE + LAYOUT_DYNAMIC +

  "layout (location = 0)  in vec3 Position; \n" +
  "layout (location = 1)  in vec3 Normal; \n" +
  "layout (location = 2)  in vec3 Tangent; \n" +
  "layout (location = 3)  in vec4 Color; \n" +
  "layout (location = 4)  in vec2 Tex; \n" +

  "out vec4 fragdiffuse; \n" +
  "out vec2 fragtex; \n" +
  "out vec4 fragposW; \n" +
  "out vec3 fragnormal; \n" +
//  "out vec3 fragtangent; \n" +
//  "out vec3 fragShadowPos; \n" +

  "void main() \n" +
  "{ \n" +

  // Calculate the position of the vertex against the world, view, and projection matrices.
  "  fragposW = M44World * vec4(Position, 1.0); \n" +
  "  gl_Position = M44Projection * M44View * fragposW; \n" +

  // Calculate the normal vector against the world matrix only.

  " fragnormal = mat3(M44World) * Normal; " +
//  " fragtangent = mat3(M44World) * Tangent; " +
  " fragdiffuse = Color; " +

  // compute the texture coordinates for the pixel shader.

  "fragtex[0] = Tex[0] * (fact_u_min_1 + 1.0)   +   Tex[1] * fact_u   +   ofs_u; " +
  "fragtex[1] = Tex[1] * (fact_v_min_1 + 1.0)   +   Tex[0] * fact_v   +   ofs_v; " +

/*
  // compute shadow position
  "fragShadowPos = compute_shadow_position (output.posW, input.instance); " +
*/

  "} ";

//---------------------------------------------------------------------

const string GETBONEM44 =

 " uniform sampler2D boneTexture; " +

 " mat4 getBoneMatrix(int boneIndex) " +
 " { " +
 "   vec4 row0 = texelFetch(boneTexture, ivec2(0, boneIndex), 0); " +
 "   vec4 row1 = texelFetch(boneTexture, ivec2(1, boneIndex), 0); " +
 "   vec4 row2 = texelFetch(boneTexture, ivec2(2, boneIndex), 0); " +
 "   vec4 row3 = texelFetch(boneTexture, ivec2(3, boneIndex), 0); " +
 "   return mat4(row0, row1, row2, row3); " +
 " } ";

//---------------------------------------------------------------------

const string RIGGED_VERTEX_SHADER_SOURCE =
  "#version 310 es \n" +
  "precision highp float; \n" +
  "precision highp int; \n" +

  LAYOUT_STABLE + LAYOUT_DYNAMIC + GETBONEM44 +

  "layout (location = 0)  in vec3 Position; \n" +
  "layout (location = 1)  in vec3 Normal; \n" +
  "layout (location = 2)  in vec3 Tangent; \n" +
  "layout (location = 3)  in vec2 Tex; \n" +
  "layout (location = 4)  in uvec4 Bone; \n" +
  "layout (location = 5)  in vec4 Weight; \n" +

  "out vec4 fragdiffuse; \n" +
  "out vec2 fragtex; \n" +
  "out vec4 fragposW; \n" +
  "out vec3 fragnormal; \n" +
//  "out vec3 fragtangent; \n" +
//  "out vec3 fragShadowPos; \n" +

  "void main() \n" +
  "{ \n" +
    "int  i;       " +
    "vec3 position, normal, tangent; " +

    "position = vec3(0.0, 0.0, 0.0);  " +
    "normal   = vec3(0.0, 0.0, 0.0);  " +
  //    "tangent  = vec3(0.0, 0.0, 0.0);  " +

    "for (i=0; i<4; i++)  " +
    "{" +
      "if (Weight[i] <= 0.0) break; " +

      "mat4 m44 = getBoneMatrix(int(Bone[i])); " +

      "position += Weight[i] * vec3(m44 * vec4(Position, 1.0));  " +
      "normal   += Weight[i] * mat3(m44) * Normal;  " +
  //  "tangent  += Weight[i] * mat3(m44) * Tangent;  " +
    "}" +

    // Calculate the position of the vertex against the world, view, and projection matrices.
    "  fragposW = M44World * vec4(position, 1.0); \n" +
    "  gl_Position = M44Projection * M44View * fragposW; \n" +

    // Calculate the normal vector against the world matrix only.

    " fragnormal = mat3(M44World) * normal; " +
  //  " fragtangent = mat3(M44World) * tangent; " +
    " fragdiffuse = vec4(1.0, 1.0, 1.0, 1.0); " +

    // compute the texture coordinates for the pixel shader.

    "fragtex[0] = Tex[0] * (fact_u_min_1 + 1.0) + Tex[1] * fact_u + ofs_u; " +
    "fragtex[1] = Tex[1] * (fact_v_min_1 + 1.0) + Tex[0] * fact_v + ofs_v; " +

  /*
    // compute shadow position
    "fragShadowPos = compute_shadow_position (output.posW, input.instance); " +
  */

  "} ";

//---------------------------------------------------------------------

const string FRAGMENT_SHADER_SOURCE =
  "#version 310 es \n" +
  "precision highp float; \n" +
  "precision highp int; \n" +

  LAYOUT_STABLE + LAYOUT_DYNAMIC +
  "uniform sampler2D AlbedoTexture; \n" +

  "in vec4 fragdiffuse; \n" +
  "in vec2 fragtex; \n" +
  "in vec4 fragposW; \n" +
  "in vec3 fragnormal; \n" +
//  "in vec3 fragtangent; \n" +
 // "in vec3 fragShadowPos; \n" +

  "layout(location = 0) out vec4 outColor; \n" +
  "layout(location = 1) out vec4 outObjectid; \n" +

  //=================================================================================

  // returns PI too much light

  "vec3 pbr (vec3 albedo, vec3 L0, vec3 N, vec3 radiance) " +
  "{ " +
  " float NdotL = dot(N, L0); " +

  " if (NdotL <= 0.0)" +
  "  return vec3 (0.0, 0.0, 0.0); " +

  " return albedo * radiance * clamp(NdotL, 0.0, 1.0); " +
  "} " +

  //=================================================================================

 "void main()  \n" +
 "{ \n" +
   //----------------------------------------------------------

   "vec3 LightColor; " +
   "vec3 finalColor; " +
   "int  i; " +

   //----------------------------------------------------------

   // setup variables

   "vec3 toEye = normalize (eyePosition_fogInvRange.xyz - fragposW.xyz);" +

   // noticed a precision problem when specular spot did shake around a sphere, that went better when adding this, but now ok ?
   "vec3 fragnormal2 = normalize (fragnormal); " +

   //----------------------------------------------------------

   " vec4 albedo = texture(AlbedoTexture, fragtex) * MaterialColor * fragdiffuse; " +

   " float switch_ambiant_sun = 1.0 - (float(disable_ambiant_sun_underground) " +
    " * float(fragposW.y + CameraOffset_FogInvExponent.y < 0.0)); " +

   //----------------------------------------------------------

   // sun light

   "{ " +

     "float sun_factor; " +   // normally 1.0, otherwise 0.0 if shadows

     "sun_factor = float(is_sunlight_on) * switch_ambiant_sun; " +   // 0 or 1

     "vec3 radiance = sun_factor * SunLightColor.rgb;  " +

     "LightColor = pbr (albedo.rgb, SunLightDirection.xyz, fragnormal2, radiance); " +

   "} " +

   //----------------------------------------------------------

   "vec3 ambientLight = AmbiantLightColor.rgb * (float(is_ambiantlight_on) * switch_ambiant_sun); " +

   "if (is_ambiant_downwards > 0) " +
     "ambientLight *= (0.7 + 0.25*fragnormal2.y + 0.05*fragnormal2.x); " +  // byte 0 : ambiant light less intense when not shining downwards

   //----------------------------------------------------------

   // point lights

   "for (i=0; i<NbLights; i++) " +
   "{" +
      "vec3 toLight           = Light[i].position_att.xyz - vec3(fragposW);" +
      "float  LightVectorLen2 = dot (toLight, toLight);" +
      "float  LightRange      = Light[i].color_range.w; " +
      "if (LightVectorLen2 < LightRange * LightRange && LightVectorLen2 > 0.0) " +
      "{" +
        "float LightVectorLen = sqrt (LightVectorLen2);" +

        "toLight /= LightVectorLen;" +          // normalize light vector

        "float factor = pow((LightRange - LightVectorLen) / LightRange, Light[i].position_att.w);  " +

        "if (Light[i].direction_cone.w > 0.0f)" +
          "factor *= pow(max(dot(-toLight, Light[i].direction_cone.xyz), 0.0), Light[i].direction_cone.w); " +

        "vec3 radiance = factor * Light[i].color_range.rgb;  " +

        "LightColor += pbr (albedo.rgb, toLight, fragnormal2, radiance); " +

      "}" +
    "}" +

   //----------------------------------------------------------

   // add factor 1.5 here to make light a bit lighter, because real PBR has a white component that's missing here.
   "finalColor = clamp (2.0 * LightColor + (ambientLight + EmissiveColor_metallic.rgb) * albedo.rgb, 0.0, 1.0); " +

   //----------------------------------------------------------
/*
  // fog
  "if (eyePosition_fogInvRange.w != 0.0) " +  // static condition
  "{" +
    "float fogFactor = clamp ((length(eyePosition_fogInvRange.xyz - vec3(fragposW)) " +
        " - fogColor_fogstart.w) * eyePosition_fogInvRange.w, 0.0, 1.0); " +
    "float factor = pow (fogFactor, CameraOffset_FogInvExponent.w);" +
    "vec3 FogCol = fogColor_fogstart.rgb * float(switch_ambiant_sun);" +
    "finalColor = factor * FogCol + (1.0 - factor) * finalColor;" +
  "}" +
*/

  //----------------------------------------------------------

  // linear to sRGB space
  "outColor.rgb = pow(finalColor, vec3(1.0 / 2.2)); " + // Assuming gamma = 2.2
  "outColor.a = albedo.a; " +
  "outObjectid = object_id; " +
  "}";   // "

//---------------------------------------------------------------------

void log3d (string s)
{
  for (;;)
  {
    uint e = glGetError ();
    if (e != 0)
      log ("error: %s : error %u", s, e);
    else
      break;
  }
}

//---------------------------------------------------------------------

// returns shader id

GLuint create_shader (GLenum shaderType,      // GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
                      string shaderSource)
{
  GLuint  shader = glCreateShader (shaderType);
  GLchar* ptr;

  if (shader != 0)
  {
    GLint shaderCompiled, shaderLength;

    ptr = &shaderSource;
    shaderLength = strlen(shaderSource);
    glShaderSource (shader, 1, &ptr, &shaderLength);
    glCompileShader (shader);

    shaderCompiled = 0;
    glGetShaderiv (shader, GL_COMPILE_STATUS, &shaderCompiled);

    if (shaderCompiled == 0)
    {
      GLint infoLength = 0;

      glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &infoLength);

      if (infoLength != 0)
      {
         string^ pstr = new string (infoLength);
         glGetShaderInfoLog (shader, infoLength, null, &pstr^);
         log ("create_shader() error: %s", pstr^);
         free pstr;
      }

      glDeleteShader(shader);
      shader = 0;
    }
  }

  return shader;
}

//---------------------------------------------------------------------

// shaders can be deleted after programs were created.

void delete_shader (GLuint shader)
{
  glDeleteShader (shader);
  log3d ("glDeleteShader");
}

//---------------------------------------------------------------------

GLuint create_program (GLuint vertex_shader, GLuint fragment_shader)
{
  GLuint program = glCreateProgram ();
  GLint linkStatus;

  if (program != 0)
  {
    glAttachShader (program, vertex_shader);
    glAttachShader (program, fragment_shader);

    glLinkProgram (program);

    linkStatus = GL_FALSE;

    glGetProgramiv (program, GL_LINK_STATUS, &linkStatus);

    if (linkStatus != GL_TRUE)
    {
      GLint logLength = 0;

      glGetProgramiv (program, GL_INFO_LOG_LENGTH, &logLength);

      if (logLength != 0)
      {
        string^ pstr = new string (logLength);
        glGetProgramInfoLog (program, logLength, null, &pstr^);
        log ("create_program() error : %s", pstr^);
        free pstr;
      }

      glDeleteProgram (program);
    }
  }

  return program;
}

//---------------------------------------------------------------------

void close_program (GLuint program)
{
  glDeleteProgram (program);
}

//---------------------------------------------------------------------
void Make2DMatrix (out XMMATRIX m);   // for rendering 2D objects in range -0.5 .. +0.5
//---------------------------------------------------------------------

// https://learnopengl.com/Advanced-OpenGL/Framebuffers

void create_frame_buffers ()
{
  glBindFramebuffer (GL_FRAMEBUFFER, g_my_fbo);
  log3d ("glBindFramebuffer");

  glGenRenderbuffers (1, &g_frame_buffer_ids.myRBOCol);
  glBindRenderbuffer (GL_RENDERBUFFER, g_frame_buffer_ids.myRBOCol);
  log3d ("glBindRenderbuffer");
  glRenderbufferStorage (GL_RENDERBUFFER, GL_RGBA8, width => g_back_buffer_x_res, height => g_back_buffer_y_res);
  log3d ("glRenderbufferStorage");

  glGenRenderbuffers(1, &g_frame_buffer_ids.mydepthstencilId);
  glBindRenderbuffer(GL_RENDERBUFFER, g_frame_buffer_ids.mydepthstencilId);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width => g_back_buffer_x_res, height => g_back_buffer_y_res);

/*
GL_DEPTH24_STENCIL8
GL_DEPTH32F_STENCIL8 – Uses a 32-bit floating-point depth buffer with an 8-bit stencil buffer, offering improved precision for depth calculations.

Separate Depth and Stencil Buffers – Instead of using a combined format, you can use:

GL_DEPTH_COMPONENT32F for depth.

GL_STENCIL_INDEX8 for stencil. This allows independent control over each buffer.
*/

  glGenRenderbuffers (1, &g_frame_buffer_ids.myRBOId);
  glBindRenderbuffer (GL_RENDERBUFFER, g_frame_buffer_ids.myRBOId);
  log3d ("glBindRenderbuffer");

  glRenderbufferStorage (GL_RENDERBUFFER, GL_RGBA8, width => g_back_buffer_x_res, height => g_back_buffer_y_res);
  log3d ("glRenderbufferStorage");

  glBindRenderbuffer (GL_RENDERBUFFER, 0);


  // binds buffers for drawing
  glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, g_frame_buffer_ids.myRBOCol);
  log3d ("glFramebufferRenderbuffer");
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_frame_buffer_ids.mydepthstencilId);
  log3d ("glFramebufferRenderbuffer");
  glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, g_frame_buffer_ids.myRBOId);
  log3d ("glFramebufferRenderbuffer");

  {
    const GLenum buffers_to_render[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers (2, &buffers_to_render);
    log3d ("glDrawBuffers");
  }

  assert glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

//---------------------------------------------------------------------

void delete_frame_buffers ()
{
  if (g_frame_buffer_ids.myRBOCol == 0)
    return;

  glDeleteRenderbuffers (1, &g_frame_buffer_ids.myRBOCol);
  glDeleteRenderbuffers (1, &g_frame_buffer_ids.myRBOId);
  glDeleteRenderbuffers (1, &g_frame_buffer_ids.mydepthstencilId);
  log3d ("glDeleteRenderbuffers");

  clear g_frame_buffer_ids;
}

//---------------------------------------------------------------------

// 0 = OK, +1 = don't reinit objects, the EGL context is still there, -1 = error

int init_draw3D (android_app *app)
{
  // Choose your render attributes
  const EGLint[] attribs = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
            EGL_BLUE_SIZE,       8,
            EGL_GREEN_SIZE,      8,
            EGL_RED_SIZE,        8,
            EGL_DEPTH_SIZE,     24,
            EGL_STENCIL_SIZE,    8,
            EGL_NONE};

  EGLint       numConfigs;
  EGLConfig[]^ supportedConfigs;
  EGLConfig    config = null;
  int          idx;
  bool         found;

  // set projection_matrix invalid so it gets updated the first frame
  g_projection_matrix_needs_update = true;

  // The default display is probably what you want on Android
  g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize (g_display, null, null);

  // figure out how many configs there are
  eglChooseConfig (g_display, &attribs, null, 0, &numConfigs);

  // get the list of configurations
  supportedConfigs = new EGLConfig[numConfigs];
  eglChooseConfig (g_display, &attribs, &supportedConfigs^, numConfigs, &numConfigs);

  // Find a config we like.
  // Could likely just grab the first if we don't care about anything else in the config.
  // Otherwise hook in your own heuristic
  found = false;
  for (idx=0; idx<supportedConfigs^'length; idx++)
  {
    EGLint    red, green, blue, depth, stencil;
    EGLConfig c;

    c = supportedConfigs^[idx];

    if (eglGetConfigAttrib(g_display, c, EGL_RED_SIZE,     &red    ) != 0 &&
        eglGetConfigAttrib(g_display, c, EGL_GREEN_SIZE,   &green  ) != 0 &&
        eglGetConfigAttrib(g_display, c, EGL_BLUE_SIZE,    &blue   ) != 0 &&
        eglGetConfigAttrib(g_display, c, EGL_DEPTH_SIZE,   &depth  ) != 0 &&
        eglGetConfigAttrib(g_display, c, EGL_STENCIL_SIZE, &stencil) != 0)
    {
      log ("Found config with rgb=%d,%d,%d, depth=%d, stencil=%d", red, green, blue, depth, stencil);
      if (red == 8 && green == 8 && blue == 8 && depth == 24 && stencil == 8)
      {
        config = c;
        found = true;
        break;
      }
    }
  }

  free supportedConfigs;

  if (!found)
    return -1;   // error

  // create the proper window surface
  g_surface = eglCreateWindowSurface (g_display, config, app->window, null);



  if (g_context != EGL_NO_CONTEXT)   // context still exists
  {
    EGLBoolean b = eglMakeCurrent (g_display, g_surface, g_surface, g_context);
    if (b != 0)
    {
      log ("context still exists : return +1");
      return +1;   // no need to reinit EGL
    }
  }


  {
    // Create a new GLES 3 context
    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    g_context = eglCreateContext (g_display, config, null, &contextAttribs);
  }

  {
    EGLBoolean b = eglMakeCurrent (g_display, g_surface, g_surface, g_context);
    log3d ("eglMakeCurrent");
    assert b != 0;
  }

  XMMatrixIdentity (&g_identityMatrix);
  Make2DMatrix (out g_default_ortho_matrix);

/*
{
GLint n;
glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &n);
log ("GL_MAX_UNIFORM_BLOCK_SIZE = %d\n", n);      // 128 MB
glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &n);
log ("GL_MAX_UNIFORM_BUFFER_BINDINGS = %d\n", n);  // 72
glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &n);
log ("GL_MAX_VERTEX_UNIFORM_COMPONENTS = %d\n", n);  // 4096
}
*/

  {
    GLuint basic_vsid, rigged_vsid, fsid;

    basic_vsid = create_shader (shaderType   => GL_VERTEX_SHADER,
                                shaderSource => BASIC_VERTEX_SHADER_SOURCE);
    log3d ("create_shader1");

    rigged_vsid = create_shader (shaderType   => GL_VERTEX_SHADER,
                                 shaderSource => RIGGED_VERTEX_SHADER_SOURCE);
    log3d ("create_shader2");

    fsid = create_shader (shaderType   => GL_FRAGMENT_SHADER,
                          shaderSource => FRAGMENT_SHADER_SOURCE);
    log3d ("create_shader3");

    g_prog_basic = create_program (vertex_shader   => basic_vsid,
                                   fragment_shader => fsid);
    log3d ("create_program");

    g_prog_rigged = create_program (vertex_shader   => rigged_vsid,
                                    fragment_shader => fsid);
    log3d ("create_program");

    delete_shader (basic_vsid);
    delete_shader (rigged_vsid);
    delete_shader (fsid);
  }

  glClearDepthf (0.0);       // specify depth value used by glClear to clear the depth buffer (inversed for better precision)
  glEnable(GL_CULL_FACE);
  glCullFace (GL_BACK); // other is GL_FRONT
  glFrontFace (GL_CW);  // Specifies orientation of front-facing polygons. GL_CW and GL_CCW are accepted. initial value is GL_CCW.


  glGenBuffers(1, &g_stableUBO_nr);

  glBindBufferBase (target => GL_UNIFORM_BUFFER,
                    index  => 1,   // uniformBlockBinding
                    buffer => g_stableUBO_nr);

  {
    const string stable_str = "stable\0";
    glUniformBlockBinding (program              => g_prog_basic,
                           uniformBlockIndex    => glGetUniformBlockIndex (g_prog_basic, &stable_str),
                           uniformBlockBinding  => 1);
    glUniformBlockBinding (program              => g_prog_rigged,
                           uniformBlockIndex    => glGetUniformBlockIndex (g_prog_rigged, &stable_str),
                           uniformBlockBinding  => 1);
  }



  glGenBuffers(1, &g_dynamicUBO_nr);

  glBindBufferBase (target => GL_UNIFORM_BUFFER,
                    index  => 2,   // uniformBlockBinding
                    buffer => g_dynamicUBO_nr);

  {
    const string dynamic_str = "dynamic\0";
    glUniformBlockBinding (program              => g_prog_basic,
                           uniformBlockIndex    => glGetUniformBlockIndex (g_prog_basic, &dynamic_str),
                           uniformBlockBinding  => 2);
    glUniformBlockBinding (program              => g_prog_rigged,
                           uniformBlockIndex    => glGetUniformBlockIndex (g_prog_rigged, &dynamic_str),
                           uniformBlockBinding  => 2);
  }


  glUseProgram (g_prog_basic);
  {
    const string AlbedoTexture = "AlbedoTexture\0";
    GLint Location = glGetUniformLocation(g_prog_basic, &AlbedoTexture);
    glUniform1i(Location, 0); // GL_TEXTURE0
  }

  glUseProgram (g_prog_rigged);
  {
    const string AlbedoTexture = "AlbedoTexture\0";
    GLint Location = glGetUniformLocation(g_prog_rigged, &AlbedoTexture);
    glUniform1i(Location, 0); // GL_TEXTURE0
  }
  {
    const string boneTexture   = "boneTexture\0";
    GLint Location = glGetUniformLocation(g_prog_rigged, &boneTexture);
    glUniform1i(Location, 1); // GL_TEXTURE1
  }

  glUseProgram (0);
  g_current_shader = -1;



  glGenFramebuffers (1, &g_my_fbo);    // create frame buffer object


  // create g_1_pixel_texture
  {
    IMAGE_INFO img = {pixel => new byte[] ' {255, 255, 255, 255}, width => 1, height => 1};
    g_1_pixel_texture = create_texture (img);
    free img.pixel;
  }

  return 0;
}

//---------------------------------------------------------------------

void terminate_3d ()
{
  if (g_display != EGL_NO_DISPLAY)
  {
    assert eglMakeCurrent (g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) != 0;

    if (g_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (g_display, g_surface);
      g_surface = EGL_NO_SURFACE;
    }

    eglTerminate (g_display);
    g_display = EGL_NO_DISPLAY;
  }
}

//---------------------------------------------------------------------

public void get_desktop_resolution (out int x_size, out int y_size)
{
  x_size = g_back_buffer_x_res;
  y_size = g_back_buffer_y_res;
}

//---------------------------------------------------------------------

void OpenGLXMMatrixPerspectiveFovLH (XMMATRIX *pout,
                                     FLOAT    fovy,    // the vertical field of view (45° in radians)
                                     FLOAT    aspect,  // width / height
                                     FLOAT    zn,      // near z
                                     FLOAT    zf)      // far z
{
  XMMatrixIdentity (pout);
  pout->m[0][0] = 1.0 / (aspect * (float)tan(fovy * 0.5));  // DirectW was :
  pout->m[1][1] = 1.0 / (float)tan(fovy * 0.5);
  pout->m[2][2] = -(zf + zn) / (zf - zn);                  // zf / (zf - zn)
  pout->m[2][3] = -1.0;                                    // 1.0
  pout->m[3][2] = 2.0 * ((zf * zn) / (zn - zf));           // (zf * zn) / (zn - zf)
  pout->m[3][3] = 0.0;
}

//---------------------------------------------------------------------

// for rendering dialog windows

void XMMatrixDialogs (XMMATRIX *pout,
                      FLOAT    Width,
                      FLOAT    Height)
{
  clear *pout;
  pout->m[0][0] = 2.0 / Width;   // (0 -> width)  projected to (0 -> 2)
  pout->m[1][1] = -2.0 / Height;  // (0 -> height) projected to (0 -> -2)
  pout->m[3][0] = -1.0;
  pout->m[3][1] = +1.0;
  pout->m[3][3] = 1.0;
}

//---------------------------------------------------------------------
void build_view_frustum (    matrix44 m_view,
                             matrix44 m_projection,
                         out vector4  m_frustum[6]);  // 6 plane a, b, c, d
//---------------------------------------------------------------------

void directx_to_opengl (ref XMMATRIX m)
{
  // FOR OPENGL's NEGATIVE Z-AXIS, NEGATE THE Z COLUMN
  m.m[0][2] = -m.m[0][2];
  m.m[1][2] = -m.m[1][2];
  m.m[2][2] = -m.m[2][2];
  m.m[3][2] = -m.m[3][2];
}

//---------------------------------------------------------------------

void resize_screen ()
{
  EGLint width, height;

  eglQuerySurface (g_display, g_surface, EGL_WIDTH, &width);
  eglQuerySurface (g_display, g_surface, EGL_HEIGHT, &height);

  if (width != g_back_buffer_x_res || height != g_back_buffer_y_res)
  {
    g_back_buffer_x_res = width;
    g_back_buffer_y_res = height;
    g_projection_matrix_needs_update = true;

//    log ("draw3d: resolution is now %d x %d\n", g_back_buffer_x_res, g_back_buffer_y_res);
  }

  if (g_projection_matrix_needs_update)
  {
    g_projection_matrix_needs_update = false;

    delete_frame_buffers ();
    create_frame_buffers ();
    glViewport (0, 0, g_back_buffer_x_res, g_back_buffer_y_res);

    // a placeholder projection matrix allocated on the stack. Column-major memory layout
    D3DXMMatrixPerspectiveFovLH
         (&g_default_projection_matrix,
          45.0 * (float)(PI / 180.0),                                // the horizontal field of view
          (float)g_back_buffer_x_res / (float)g_back_buffer_y_res,   // aspect ratio
          zn => g_far_z,                                             // the far view-plane (REVERSE NEAR FAR !!)
          zf => g_near_z);                                           // the near view-plane (REVERSE NEAR FAR !!)

    OpenGLXMMatrixPerspectiveFovLH
         (&g_opengl_default_projection_matrix,
          45.0 * (float)(PI / 180.0),                                // the horizontal field of view
          (float)g_back_buffer_x_res / (float)g_back_buffer_y_res,   // aspect ratio
          zn => g_far_z,                                             // the far view-plane (REVERSE NEAR FAR !!)
          zf => g_near_z);                                           // the near view-plane (REVERSE NEAR FAR !!)

    XMMatrixDialogs (&g_default_dialogs_matrix,
                     Width  => (float)g_back_buffer_x_res,
                     Height => (float)g_back_buffer_y_res);

    g_world_to_screen_factor = (float)g_back_buffer_y_res * g_default_projection_matrix.m[1][1];

    build_view_frustum (*(matrix44*)&g_default_view_matrix, *(matrix44*)&g_default_projection_matrix, out g_frustum);
  }
}

//---------------------------------------------------------------------

void flush_constant_buffers ()    // UBO buffers
{
  if (g_shader_constants_stable_dirty)
  {
    g_shader_constants_stable_dirty = false;
    glBindBuffer(GL_UNIFORM_BUFFER, g_stableUBO_nr);
    glBufferData(GL_UNIFORM_BUFFER, g_shader_constants_stable'size, (byte*)&g_shader_constants_stable, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
  }

  glBindBuffer(GL_UNIFORM_BUFFER, g_dynamicUBO_nr);
  glBufferData(GL_UNIFORM_BUFFER, g_shader_constants_dynamic'size, (byte*)&g_shader_constants_dynamic, GL_STATIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

//---------------------------------------------------------------------

void select_new_shader (int shader_nr)
{
  if (shader_nr != g_current_shader)
  {
    g_current_shader = shader_nr;

    if (shader_nr == SHADER_BASIC)
      glUseProgram (g_prog_basic);
    else
      glUseProgram (g_prog_rigged);
  }
}

//---------------------------------------------------------------------

// register a list of functions for 3D

public void setup_3D (CALLBACK_FUNCTIONS_3D func)
{
  g_func = func;
  g_func_filled = true;
}

//---------------------------------------------------------------------

void init_objects_if_needed ()
{
  if (g_init_objects_required && g_func_filled)
  {
    g_init_objects_required = false;
    if (g_func.init_objects != null)
      g_func.init_objects ();
  }
}

//---------------------------------------------------------------------

// to be called in main android app

public void event_3D (android_app *app, int event_type)
{
  g_frame_tick = ticks();

  switch (event_type)
  {
    case EVENT_INITIALIZE:    // first run, or user reactivated this app after pause
      {
        int rc;

        rc = init_draw3D (app);  // 0 = OK, +1 = don't reinit objects, the EGL context is still there, -1 = error
        assert rc >= 0;

        g_init_objects_required = (rc == 0);
      }

      init_objects_if_needed ();

      resize_screen ();   // this will initialize g_back_buffer_x_res, g_back_buffer_y_res
      break;

    case EVENT_RENDER_FRAME:
      init_objects_if_needed ();

      resize_screen ();

      // Bind the framebuffer for dual output
      glBindFramebuffer (GL_FRAMEBUFFER, g_my_fbo);
      log3d ("glBindFramebuffer");

      // enable depth buffer mandatory on some GPU before clearing depth buffer !
      set_depth_buffer (testing => true, writing => true);
      glClear (GL_DEPTH_BUFFER_BIT);

      glClearBufferfv (GL_COLOR, 0, &g_background_color);   // clear only frame buffer 0, don't touch frame buffer 1

      clear_stencil_buffer ();
      set_stencil_buffer (testing => false, writing => false);    // default: off/off for render_3D, off/off for render_2D.

      if (g_func.prepare_frame != null)
        g_func.prepare_frame ();

      if (g_shadows_level > 0)
      {
        if (g_func.render_shadow != null)
        {
          // enable depth buffer mandatory on some GPU before clearing depth buffer !
          set_depth_buffer (testing => true, writing => true);
          glClear (GL_DEPTH_BUFFER_BIT);

          clear_stencil_buffer ();
          set_stencil_buffer (testing => false, writing => false);    // default: off/off for render_3D, off/off for render_2D.

          g_func.render_shadow ();
        }
      }

      if (g_func.render_3D != null)
      {
        clear g_shader_constants_stable;   // clear all constants, including sun, lights, fog, ..
        g_shader_constants_stable.eyePosition  = g_default_camera_eye;   // used for specular light
        g_shader_constants_stable.CameraOffset = g_world_position;
        g_shader_constants_stable.View         = g_default_view_matrix;   // 3d camera
        directx_to_opengl (ref g_shader_constants_stable.View);
        g_shader_constants_stable.Projection   = g_opengl_default_projection_matrix;

  /*
        if (g_shadows_level > 0)
          MXXMatrixTranspose (g_default_shadow_matrix, out g_shader_constants_stable.ShadowTransposedTransform);
        g_shader_constants_stable.nb_of_shadow_maps        = g_shadows_level;
  */

        g_shader_constants_stable_dirty = true;
        g_shader_constants_dynamic = DEFAULT_DYNAMIC_CONSTANT;

        set_depth_buffer (testing => true, writing => true);

        clear_stencil_buffer ();
        set_stencil_buffer (testing => false, writing => false);    // default: off/off for render_3D, off/off for render_2D.

        set_blending (BLENDING_TRANSLUCID);

        g_func.render_3D ();
      }

      // ------------------------------------------------------------------------------------------------------

      if (g_func.render_2D != null)
      {
        clear g_shader_constants_stable;   // clear all constants, including sun, lights, fog, ..
        g_shader_constants_stable.View       = g_identityMatrix;
//        directx_to_opengl (ref g_shader_constants_stable.View);
        g_shader_constants_stable.Projection = g_default_ortho_matrix;
        g_shader_constants_stable.AmbiantLightColor = {1.0, 1.0, 1.0};

        g_shader_constants_stable_dirty = true;
        g_shader_constants_dynamic = DEFAULT_DYNAMIC_CONSTANT;
        g_shader_constants_dynamic.is_ambiantlight_on = 1;

        set_depth_buffer (testing => false, writing => false);

        clear_stencil_buffer ();
        set_stencil_buffer (testing => false, writing => false);    // default: off/off for render_3D, off/off for render_2D.

        set_blending (BLENDING_TRANSLUCID);

        g_func.render_2D ();
      }

      // ------------------------------------------------------------------------------------------------------

      if (g_func_render_all_dialogs != null)
      {
        clear g_shader_constants_stable;   // clear all constants, including sun, lights, fog, ..
        g_shader_constants_stable.View       = g_identityMatrix;
//        directx_to_opengl (ref g_shader_constants_stable.View);
        g_shader_constants_stable.Projection = g_default_dialogs_matrix;
        g_shader_constants_stable.AmbiantLightColor = {1.0, 1.0, 1.0};

        g_shader_constants_stable_dirty = true;
        g_shader_constants_dynamic = DEFAULT_DYNAMIC_CONSTANT;
        g_shader_constants_dynamic.is_ambiantlight_on = 1;

        set_depth_buffer (testing => false, writing => false);

        clear_stencil_buffer ();
        set_stencil_buffer (testing => false, writing => false);    // default: off/off for render_3D, off/off for render_2D.

        set_blending (BLENDING_TRANSLUCID);

        g_func_render_all_dialogs ();
      }

      // ------------------------------------------------------------------------------------------------------

      // unbind all VAO
      glBindVertexArray(0);
      glUseProgram (0);
      g_current_shader = -1;

      // ------------------------------------------------------------------------------------------------------

      // Bind the source framebuffer
      glBindFramebuffer (GL_READ_FRAMEBUFFER, g_my_fbo);
      log3d ("glBindFramebuffer");

      glReadBuffer (GL_COLOR_ATTACHMENT0);
      log3d ("glReadBuffer");

      // Bind the destination framebuffer
      glBindFramebuffer (GL_DRAW_FRAMEBUFFER, 0);
      log3d ("glBindFramebuffer");

      glBlitFramebuffer (srcX0 => 0,
                         srcY0 => 0,
                         srcX1 => g_back_buffer_x_res,
                         srcY1 => g_back_buffer_y_res,
                         dstX0 => 0,
                         dstY0 => 0,
                         dstX1 => g_back_buffer_x_res,
                         dstY1 => g_back_buffer_y_res,
                         mask => GL_COLOR_BUFFER_BIT,
                         filter => (GLenum)GL_LINEAR);

      log3d ("glBlitFramebuffer");

      // Unbind the framebuffer
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      // ------------------------------------------------------------------------------------------------------

      // Present the rendered image (implicit glFlush)
      assert eglSwapBuffers (g_display, g_surface) == EGL_TRUE;

      // ------------------------------------------------------------------------------------------------------

      break;

    case EVENT_TERMINATE:        // user paused this app
      terminate_3d ();
      break;

    default:
      break;
  }
}

//---------------------------------------------------------------------

public
VERTEX_ID create_vertex_buffer (VERTEX[] v, bool allow_update)
{
  MODEL_VERTEX_ID id;

  clear id;

  // A Vertex Array Object (VAO) specifies how the vertex data is interpreted by shaders

  glGenVertexArrays(1, &id.VAOId);    // generate 1 unique identifier for a vertex buffer
  log3d ("glGenVertexArrays");

  glBindVertexArray (id.VAOId);       // bind this vertex buffer for implicit operations
  log3d ("glBindVertexArray v");


  // A Vertex Buffer Object (VBO) stores vertex data, such as positions, normals, texture coordinates,
  // or any custom attributes. It is essentially a chunk of memory on the GPU where the data
  // for rendering is kept.

  glGenBuffers(1, &id.VBOId);               // generate 1 unique identifier for a buffer
  log3d ("glGenBuffers");

  glBindBuffer(GL_ARRAY_BUFFER, id.VBOId);  // special bind buffer for Vertex attributes
  log3d ("glBindBuffer");

  glBufferData (GL_ARRAY_BUFFER, v'size, (byte*)&v, allow_update ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
  log3d ("glBufferData");


  glVertexAttribPointer (0, 3, GL_FLOAT,         GL_FALSE, VERTEX'size, 0);   // .c
  log3d ("glVertexAttribPointer");

  glVertexAttribPointer (1, 3, GL_FLOAT,         GL_FALSE, VERTEX'size, 12);  // .n
  log3d ("glVertexAttribPointer");

  glVertexAttribPointer (2, 3, GL_FLOAT,         GL_FALSE, VERTEX'size, 24);  // .u
  log3d ("glVertexAttribPointer");

  // GL_TRUE: Enables normalization, so 0-255 byte values are mapped to 0.0-1.0 in the shader.
  glVertexAttribPointer (3, 4, GL_UNSIGNED_BYTE, GL_TRUE,  VERTEX'size, 36);  // .o
  log3d ("glVertexAttribPointer");

  glVertexAttribPointer (4, 2, GL_FLOAT,         GL_FALSE, VERTEX'size, 40);  // .t
  log3d ("glVertexAttribPointer");

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glEnableVertexAttribArray(3);
  glEnableVertexAttribArray(4);
  log3d ("glEnableVertexAttribArray");

  glBindVertexArray (0);       // detach VAO
  log3d ("glBindVertexArray v0");

  return *(VERTEX_ID *)&id;
}

//---------------------------------------------------------------------

public
void update_vertex_buffer (VERTEX_ID v, int offset, VERTEX[] vb, BUFFER_UPDATE_FLAG flag = OVERWRITE_ALL)
{
  // $
}

//--------------------------------------------------------------------------

public
void free_vertex_buffer (VERTEX_ID v)
{
  glDeleteVertexArrays (1, &((MODEL_VERTEX_ID*)&v)->VAOId);
  log3d ("glDeleteVertexArrays");

  glDeleteBuffers (1, &((MODEL_VERTEX_ID*)&v)->VBOId);
  log3d ("glDeleteBuffers");
}

//---------------------------------------------------------------------

INDEX_ID create_index_buffer (byte[] i, bool allow_update)
{
  GLuint VAOId;
  GLuint VBelid;

  // we create a dummy VAO (because creating the index buffer without VAO does not work)
  glGenVertexArrays (1, &VAOId);    // generate 1 unique identifier
  log3d ("glGenVertexArrays");

  glBindVertexArray (VAOId);       // bind this vertex buffer for implicit operations
  log3d ("glBindVertexArray i");

  glGenBuffers (1, &VBelid);               // generate 1 unique identifier for a buffer
  log3d ("glGenBuffers");

  glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, VBelid);
  log3d ("glBindBuffer");

  glBufferData (GL_ELEMENT_ARRAY_BUFFER, i'size, (byte*)&i, allow_update ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
  log3d ("glBufferData");

  // we detach and delete the dummy VAO

  glBindVertexArray (0);       // detach VAO
  log3d ("glBindVertexArray i0");

  glDeleteVertexArrays (1, &VAOId);   // delete VAO
  log3d ("glDeleteVertexArrays");

  return VBelid;
}

//---------------------------------------------------------------------

public
INDEX_ID create_small_index_buffer (uint2[] i, bool allow_update)
{
  return create_index_buffer (i, allow_update);
}

//---------------------------------------------------------------------

public
INDEX_ID create_large_index_buffer (uint4[] i, bool allow_update)
{
  return 0x8000_0000 | create_index_buffer (i, allow_update);
}

//---------------------------------------------------------------------

public
void free_index_buffer (INDEX_ID i)
{
  GLuint id = (GLuint)i & 0x7FFF_FFFF;
  glDeleteBuffers (1, &id);
  log3d ("glDeleteBuffers");
}

//---------------------------------------------------------------------

public
TEXTURE_ID create_texture (IMAGE_INFO img,
                           bool       allow_update = false,
                           bool       use_mipmapping = true,
                           bool       rgb_to_linear_conversion = true)  // true = GPU will converts RGB to linear color, false = no conversion
{
  GLuint textureId;

  glGenTextures (1, &textureId);
  log3d ("glGenTexturesa");

  glBindTexture (GL_TEXTURE_2D, textureId);
  log3d ("glBindTexturea");

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);  // or GL_CLAMP_TO_EDGE
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  log3d ("glTexParameteria1");

  // When MINifying the image, use a LINEAR blend of two mipmaps, each filtered LINEARLY too
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  log3d ("glTexParameteria2");

  // When MAGnifying the image (no bigger mipmap available), use LINEAR filtering
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  log3d ("glTexParameteria3");

  // Load the texture into VRAM
  glTexImage2D (GL_TEXTURE_2D,          // target
                0,                      // mip level
                (GLint)(rgb_to_linear_conversion ? GL_SRGB8_ALPHA8 : GL_RGBA),
                (int)img.width,         // width of the texture
                (int)img.height,        // height of the texture
                0,                      // border (always 0)
                GL_RGBA,                // format
                GL_UNSIGNED_BYTE,       // type
                &img.pixel^);           // Data to upload
  log3d ("glTexImage2Da");

  if (use_mipmapping)
  {
    glGenerateMipmap (GL_TEXTURE_2D);
    log3d ("glGenerateMipmapa");
  }
  else
  {
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    log3d ("glTexParameteria4");

    // as GL_TEXTURE_MAX_LEVEL has a default value of 1000, the texture is considered incomplete and is displayed black, so set it to 0
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    log3d ("glTexParameteria5");
  }

  glBindTexture (GL_TEXTURE_2D, 0);    // detach texture from VAO ?
  log3d ("glBindTexture0a");

  return textureId;
}

//---------------------------------------------------------------------

public
void update_texture_part (TEXTURE_ID id, uint[2] offset, IMAGE_INFO img)
{
  glBindTexture (GL_TEXTURE_2D, (GLuint)id);

  // Load the texture into VRAM
  glTexSubImage2D (target  => GL_TEXTURE_2D,
                   level   => 0,
                   xoffset => (int)offset[0],
                   yoffset => (int)offset[1],
                   width   => (int)img.width,
                   height  => (int)img.height,
                   format  => GL_RGBA,
                   type    => GL_UNSIGNED_BYTE,
                   pixels  => &img.pixel^);

  glGenerateMipmap (GL_TEXTURE_2D);

  glBindTexture (GL_TEXTURE_2D, 0);    // detach texture from VAO ?
}

//--------------------------------------------------------------------------

public
void update_texture (TEXTURE_ID id, IMAGE_INFO img)
{
  update_texture_part (id, {0,0}, img);
}

//--------------------------------------------------------------------------

public
void free_texture (TEXTURE_ID t)
{
  glDeleteTextures(1, (GLuint *)&t);   // TEXTURE_ID uses 8 bytes, cast to GLuint which uses 4 bytes !
  log3d ("glDeleteTextures");
}

//---------------------------------------------------------------------

public
void limit_fps (bool on)     // default: true. (set to false if we don't want to wait for the monitor vertical sync)
{
  _unused on;
}

//---------------------------------------------------------------------

// returns a table with samples, for example 1, 2, 4, 8, 16. count indicates how many samples in table.
public
void best_antialiasing_available (out int[5] samples, out int count)
{
  clear samples;
  samples[0] = 1;
  count = 1; // $
}

//---------------------------------------------------------------------

public
int current_antialiasing_setting ()
{
  return 0; // $
}

//---------------------------------------------------------------------

// set nearest and farthest z-points to draw

public
void set_near_far_Z (float near_z = 0.0625,   // default: 6 cm
                     float far_z  = 1024.0)   // default: 1024 m
{
  g_near_z = near_z;
  g_far_z  = far_z;
  g_projection_matrix_needs_update = true;
}

//---------------------------------------------------------------------

public
void set_shadows_number_of_levels (int level)     // 0 = shadows off, 1..5 = number of shadow map levels
{
  _unused level;
  // $
}

//---------------------------------------------------------------------

public
void clear_depth_buffer ()
{
//  glClearDepthf (0.0);       // specify depth value used by glClear to clear the depth buffer (inversed for better precision)
  glClear (GL_DEPTH_BUFFER_BIT);   // clear the depth buffer with the depth value specified in glClearDepthf()
}

//---------------------------------------------------------------------

public
void set_depth_buffer (bool testing, bool writing = true)    // default: on/on for render_3D, off/off for render_2D.
{
  if (testing)
  {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);    // If source z > destination z, we write the pixel.
  }
  else
  {
    if (writing)
    {
      /*
      Disable depth test and allow depth writes
      In some cases, you might want to disable depth testing and still allow the depth buffer updated
      while you are rendering your objects. It turns out that if you disable depth testing (glDisable(GL_DEPTH_TEST)),
      GL also disables writes to the depth buffer. The correct solution is to tell GL to ignore the depth test
      results with glDepthFunc(GL_ALWAYS). Be careful because in this state, if you render a far away object last,
      the depth buffer will contain the values of that far object.
      */

      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_ALWAYS);      // ignore depth test but still enable writing
    }
    else
    {
      glDisable(GL_DEPTH_TEST);   // disable both depth testing and writing
      glDepthFunc(GL_GREATER);
    }
  }

  if (writing)
    glDepthMask(GL_TRUE);        // Specifies whether the depth buffer is enabled for writing.
  else
    glDepthMask(GL_FALSE);
}

//---------------------------------------------------------------------

public
void clear_stencil_buffer ()
{
  glClearStencil (0);         // initial stencil value is 0.
  glStencilMask(0x000000FF);  // mandatory, otherwise glClear has no effect
//  glDisable(GL_SCISSOR_TEST);
  glClear (GL_STENCIL_BUFFER_BIT);
}

//---------------------------------------------------------------------

// control stencil buffer.
// initial stencil value is 0.
// enable testing will render a pixel only if stencil value is 0.
// enable writing will change stencil value to value different than 0.

public
void set_stencil_buffer (bool testing, bool writing = false)    // default: off/off for render_3D, off/off for render_2D.
{
   if (testing | writing)
   {
     glEnable (GL_STENCIL_TEST);

     glStencilFuncSeparate (face => GL_FRONT,
                            func => testing ? GL_EQUAL : GL_ALWAYS,  // test if equal stencil reference value, or test always passes
                            rf   => 0,
                            mask => 0x000000FF);  // 8 bits for comparison

     glStencilFuncSeparate (face => GL_BACK,
                            func => GL_NEVER,
                            rf   => 0,
                            mask => 0x000000FF);  // 8 bits for comparison

     glStencilOpSeparate (face   => GL_FRONT,
                          sfail  => GL_KEEP,
                          dpfail => GL_KEEP,
                          dppass => writing ? GL_INCR : GL_KEEP);

     glStencilOpSeparate (face   => GL_BACK,
                          sfail  => GL_KEEP,
                          dpfail => GL_KEEP,
                          dppass => GL_KEEP);

     glStencilMask (0xFF);   // controls which bits are writable in stencil buffer
   }
   else
   {
     glDisable (GL_STENCIL_TEST);
   }
}

//---------------------------------------------------------------------

public
void set_blending (BLENDING blending)    // default: BLENDING_TRANSLUCID.
{
  switch (blending)
  {
    case BLENDING_TRANSLUCID:
      glEnable(GL_BLEND);
      glBlendFunci (buf       => 0,
                    sfactor   => GL_SRC_ALPHA,
                    dfactor   => GL_ONE_MINUS_SRC_ALPHA);
      glBlendEquationi (buf  => 0,
 	                      mode => GL_FUNC_ADD);
      break;

    case BLENDING_ADD:
      glEnable(GL_BLEND);
      glBlendFunci (buf       => 0,
                    sfactor   => GL_SRC_ALPHA,
                    dfactor   => GL_DST_ALPHA);
      glBlendEquationi (buf  => 0,
 	                      mode => GL_FUNC_ADD);
      break;

    case BLENDING_MIN:
      glEnable(GL_BLEND);
      glBlendFunci (buf       => 0,
                    sfactor   => GL_SRC_ALPHA,
                    dfactor   => GL_DST_ALPHA);
      glBlendEquationi (buf  => 0,
 	                      mode => GL_MIN);
      break;

    case BLENDING_MAX:
      glEnable(GL_BLEND);
      glBlendFunci (buf       => 0,
                    sfactor   => GL_SRC_ALPHA,
                    dfactor   => GL_DST_ALPHA);
      glBlendEquationi (buf  => 0,
 	                      mode => GL_MAX);
      break;

    case BLENDING_OFF:
      glDisable(GL_BLEND);
      break;

    default:
      abort;
  }
}

//--------------------------------------------------------------------------

// -- object settings --

// set object position/rotation
public
void set_world_transform (matrix44 m)
{
  g_shader_constants_dynamic.World = m;
}

//---------------------------------------------------------------------

public
void update_small_index_buffer (INDEX_ID id, uint offset, uint2[] ib, BUFFER_UPDATE_FLAG flag = OVERWRITE_ALL)
{
  // $  not used by planet
}

//--------------------------------------------------------------------------

public
void update_large_index_buffer (INDEX_ID id, uint offset, uint4[] ib, BUFFER_UPDATE_FLAG flag = OVERWRITE_ALL)
{
  // $  not used by planet
}

//--------------------------------------------------------------------------

public
RIGGED_VERTEX_ID create_rigged_vertex_buffer (RIGGED_VERTEX[] v)
{
  MODEL_VERTEX_ID id;

  clear id;

  // A Vertex Array Object (VAO) specifies how the vertex data is interpreted by shaders

  glGenVertexArrays(1, &id.VAOId);    // generate 1 unique identifier for a vertex buffer
  log3d ("glGenVertexArrays");

  glBindVertexArray (id.VAOId);       // bind this vertex buffer for implicit operations
  log3d ("glBindVertexArray v");


  // A Vertex Buffer Object (VBO) stores vertex data, such as positions, normals, texture coordinates,
  // or any custom attributes. It is essentially a chunk of memory on the GPU where the data
  // for rendering is kept.

  glGenBuffers(1, &id.VBOId);               // generate 1 unique identifier for a buffer
  log3d ("glGenBuffers");

  glBindBuffer(GL_ARRAY_BUFFER, id.VBOId);  // special bind buffer for Vertex attributes
  log3d ("glBindBuffer");

  glBufferData (GL_ARRAY_BUFFER, v'size, (byte*)&v, GL_STATIC_DRAW);
  log3d ("glBufferData");

  glVertexAttribPointer (0, 3, GL_FLOAT,         GL_FALSE, RIGGED_VERTEX'size, 0);   // .c
  log3d ("glVertexAttribPointer");

  glVertexAttribPointer (1, 3, GL_FLOAT,         GL_FALSE, RIGGED_VERTEX'size, 12);  // .n
  log3d ("glVertexAttribPointer");

  glVertexAttribPointer (2, 3, GL_FLOAT,         GL_FALSE, RIGGED_VERTEX'size, 24);  // .u
  log3d ("glVertexAttribPointer");

  glVertexAttribPointer (3, 2, GL_FLOAT,         GL_FALSE, RIGGED_VERTEX'size, 36);  // .t
  log3d ("glVertexAttribPointer");

  glVertexAttribPointer (4, 4, GL_UNSIGNED_BYTE, GL_FALSE,  RIGGED_VERTEX'size, 44); // .bones
  log3d ("glVertexAttribPointer");

  glVertexAttribPointer (5, 4, GL_FLOAT,         GL_FALSE, RIGGED_VERTEX'size, 48);  // .weights
  log3d ("glVertexAttribPointer");

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glEnableVertexAttribArray(3);
  glEnableVertexAttribArray(4);
  glEnableVertexAttribArray(5);
  log3d ("glEnableVertexAttribArray");

  glBindVertexArray (0);       // detach VAO
  log3d ("glBindVertexArray v0");

  return *(RIGGED_VERTEX_ID *)&id;
}

//--------------------------------------------------------------------------

public
void free_rigged_vertex_buffer (RIGGED_VERTEX_ID v)
{
  glDeleteVertexArrays (1, &((MODEL_VERTEX_ID*)&v)->VAOId);
  log3d ("glDeleteVertexArrays");

  glDeleteBuffers (1, &((MODEL_VERTEX_ID*)&v)->VBOId);
  log3d ("glDeleteBuffers");
}

//--------------------------------------------------------------------------

public
BONE_BUFFER_ID create_bone_buffer (int nb_bones)
{
  GLuint textureId;

  glGenTextures(1, &textureId);
  glBindTexture(GL_TEXTURE_2D, textureId);

  // Allocate storage for the texture
  glTexImage2D(
      GL_TEXTURE_2D,
      0,                  // mipmap level
      (GLint)GL_RGBA32F,  // internal format
      4,                  // texture width
      nb_bones,           // texture height
      0,                  // border (must be 0)
      GL_RGBA,            // format of the pixel data
      GL_FLOAT,           // data type of the pixel data
      null                // no initial data
  );

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // as GL_TEXTURE_MAX_LEVEL has a default value of 1000, the texture is considered incomplete and is displayed black, so set it to 0
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);

  glBindTexture (GL_TEXTURE_2D, 0);    // detach texture
  log3d ("glBindTexture0b");

  return textureId;
}

//--------------------------------------------------------------------------

public
void write_bone_buffer (BONE_BUFFER_ID id, matrix44[] m)
{
  glBindTexture (GL_TEXTURE_2D, (GLuint)id);

  glTexSubImage2D (target  => GL_TEXTURE_2D,
                   level   => 0,
                   xoffset => 0,
                   yoffset => 0,
                   width   => 4,
                   height  => m'length,
                   format  => GL_RGBA,
                   type    => GL_FLOAT,
                   pixels  => (byte*)&m);
  log3d ("glTexSubImage2D");

  glBindTexture (GL_TEXTURE_2D, 0);    // detach texture from VAO
}

//--------------------------------------------------------------------------

public
void free_bone_buffer (BONE_BUFFER_ID id)
{
  glDeleteTextures(1, (GLuint *)&id); // TEXTURE_ID uses 8 bytes, cast to GLuint which uses 4 bytes !
  log3d ("glDeleteTextures");
}

//--------------------------------------------------------------------------

void set_pixel_shader_ressources (uint object_id)
{
  if (object_id >= 0xFFFFFF)   // means no object_id provided
  {
    clear g_shader_constants_dynamic.object_id;   // zero alpha means no object_id provided
  }
  else
  {
    g_shader_constants_dynamic.object_id = {(float)(object_id'byte[0]) * (1.0 / 255.0),
                                            (float)(object_id'byte[1]) * (1.0 / 255.0),
                                            (float)(object_id'byte[2]) * (1.0 / 255.0),
                                            1.0};    // 1.0 means means valid object_id (will pass blending without change)
  }
}

//--------------------------------------------------------------------------

// uses 3 vertexes per triangle
public
void draw_triangles (VERTEX_ID  v_id,
                     int        v_offset,
                     int        v_count,
                     TEXTURE_ID t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
                     uint       object_id = 0xFFFFFF)   // user value 0 .. 2^24-2
{
  select_new_shader (SHADER_BASIC);

  set_pixel_shader_ressources (object_id);
  flush_constant_buffers ();   // UBO buffers

  glBindVertexArray (((MODEL_VERTEX_ID*)&v_id)->VAOId);
  log3d ("glBindVertexArray d");

  // Setup the texture
  glActiveTexture (GL_TEXTURE0);
  log3d ("glActiveTexture");

  if (t_id[0] == 0)
    glBindTexture (GL_TEXTURE_2D, (GLuint)g_1_pixel_texture);
  else
    glBindTexture (GL_TEXTURE_2D, (GLuint)t_id[0]);
  log3d ("glBindTexture");

  glDrawArrays(GL_TRIANGLES, v_offset, v_count);
  log3d ("glDrawArrays");

  glBindTexture (GL_TEXTURE_2D, 0);      // unbind texture
  log3d ("glBindTexture0c");

  glBindVertexArray(0);      // unbind VAO
  log3d ("glBindVertexArray d0");
}

//---------------------------------------------------------------------

// uses 3 indexes per triangle
public
void draw_indexed_triangles (VERTEX_ID  v_id,
                             int        v_offset,
                             INDEX_ID   i_id,
                             int        i_offset,
                             int        i_count,
                             TEXTURE_ID t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
                             uint       object_id = 0xFFFFFF)  // user value 0 .. 2^24-2
{
  INDEX_ID iid = i_id;

  select_new_shader (SHADER_BASIC);

  set_pixel_shader_ressources (object_id);
  flush_constant_buffers ();   // UBO buffers

  glBindVertexArray (((MODEL_VERTEX_ID*)&v_id)->VAOId);
  log3d ("glBindVertexArray di");

  // bind Index buffer
  glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, (GLuint)iid & 0x7FFF_FFFF);
  log3d ("glBindBufferI1");

  // Setup the texture
  glActiveTexture (GL_TEXTURE0);
  log3d ("glActiveTexture");

  if (t_id[0] == 0)
    glBindTexture (GL_TEXTURE_2D, (GLuint)g_1_pixel_texture);
  else
    glBindTexture (GL_TEXTURE_2D, (GLuint)t_id[0]);
  log3d ("glBindTexture");

  glDrawElements (GL_TRIANGLES,      // mode
                  i_count,           // count (nb of indexes)
                  iid < 0x8000_0000 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                  null);             // element array buffer offset
  log3d ("glDrawElements");

  glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);   // unbind index buffer
  log3d ("glBindBufferI2");

  glBindTexture (GL_TEXTURE_2D, 0);      // unbind texture
  log3d ("glBindTexture0d");

  glBindVertexArray(0);      // unbind VAO
  log3d ("glBindVertexArray di0");
}

//---------------------------------------------------------------------

public
void draw_indexed_rigged_triangles
     (RIGGED_VERTEX_ID  v_id,
      int               v_offset,
      INDEX_ID          i_id,
      int               i_offset,
      int               i_count,    // nb_vertexes
      BONE_BUFFER_ID    b_id,
      TEXTURE_ID        t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
      uint              object_id = 0xFFFFFF)
{
  INDEX_ID iid = i_id;

//$
  assert v_offset == 0;
  assert i_offset == 0;

  select_new_shader (SHADER_RIGGED);

  set_pixel_shader_ressources (object_id);
  flush_constant_buffers ();   // UBO buffers index 1 & 2

  glBindVertexArray (((MODEL_VERTEX_ID*)&v_id)->VAOId);
  log3d ("glBindVertexArray di");

  // bind index buffer
  glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, (GLuint)iid & 0x7FFF_FFFF);
  log3d ("glBindBufferI1");

  // texture
  glActiveTexture (GL_TEXTURE0);
  log3d ("glActiveTexture");

  if (t_id[0] == 0)
    glBindTexture (GL_TEXTURE_2D, (GLuint)g_1_pixel_texture);
  else
    glBindTexture (GL_TEXTURE_2D, (GLuint)t_id[0]);
  log3d ("glBindTexture");

  // bone texture
  glActiveTexture (GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, (GLuint)b_id);

  // draw
  glDrawElements (GL_TRIANGLES,      // mode
                  i_count,           // count (nb of indexes)
                  iid < 0x8000_0000 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                  null);             // element array buffer offset
  log3d ("glDrawElements");

  // unbind index
  glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  log3d ("glBindBufferI2");

  // unbind texture
  glActiveTexture (GL_TEXTURE1);
  glBindTexture (GL_TEXTURE_2D, 0);

  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, 0);

  // unbind VAO
  glBindVertexArray(0);
  log3d ("glBindVertexArray di0");
}

//--------------------------------------------------------------------------

public
void draw_rigged_triangles
   (RIGGED_VERTEX_ID  v_id,     // rigged vertex buffer id
    int               v_offset,
    int               v_count,
    BONE_BUFFER_ID    b_id,     // bone buffer id
    TEXTURE_ID        t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
    uint              object_id = 0xFFFFFF)  // user value 0 .. 2^24-2
{
  select_new_shader (SHADER_RIGGED);

  set_pixel_shader_ressources (object_id);
  flush_constant_buffers ();   // UBO buffers index 1 & 2

  glBindVertexArray (((MODEL_VERTEX_ID*)&v_id)->VAOId);
  log3d ("glBindVertexArray d");

  // Setup the texture
  glActiveTexture (GL_TEXTURE0);
  log3d ("glActiveTexture");

  if (t_id[0] == 0)
    glBindTexture (GL_TEXTURE_2D, (GLuint)g_1_pixel_texture);
  else
    glBindTexture (GL_TEXTURE_2D, (GLuint)t_id[0]);
  log3d ("glBindTexture");

  // bone texture
  glActiveTexture (GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, (GLuint)b_id);

  glDrawArrays(GL_TRIANGLES, v_offset, v_count);
  log3d ("glDrawArrays");

  glActiveTexture (GL_TEXTURE1);
  glBindTexture (GL_TEXTURE_2D, 0);      // unbind texture

  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, 0);      // unbind texture

  glBindVertexArray(0);      // unbind VAO
  log3d ("glBindVertexArray d0");
}

//--------------------------------------------------------------------------

// compute object_id at screen position x,y
// returns 0xFFFFFF if there is no object.

public
uint get_picking_object_id (int x, int y)
{
  uint data = 0xFFFFFF;

  glBindFramebuffer (GL_READ_FRAMEBUFFER, g_my_fbo);
  log3d ("glBindFramebuffer");

  glReadBuffer (GL_COLOR_ATTACHMENT1);
  log3d ("glReadBuffer");

  glReadPixels (x, g_back_buffer_y_res-1-y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (byte*)&data);
  log3d ("glReadPixels");

  return data & 0xFFFFFF;
}

//---------------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------------

#else
  bad

#endif

//---------------------------------------------------------------------

public void set_background_color (vector3 color)
{
  g_background_color[0:3] = color;
}

//---------------------------------------------------------------------

// ambiant light RGB color, usually a light grey.  linear color.
// note: after this call, call set_object_colors() with ambiant_enabled=true

public void set_ambiant_light (vector3 color)
{
  if (memcmp (g_shader_constants_stable.AmbiantLightColor, color) != 0)
  {
    g_shader_constants_stable.AmbiantLightColor = color;
    g_shader_constants_stable_dirty = true;
  }
}

//---------------------------------------------------------------------

// directional sunlight RGB color, usually white, yellow, red, .. shining down.  linear color.
// note: after this call, call set_object_colors() with sunlight_enabled=true

public void set_sun_light (vector3 color, vector3 direction)
{
  vector3 v;

  D3DXVec3Normalize (out v, {-direction[0], -direction[1], -direction[2]});

  if (memcmp (g_shader_constants_stable.SunLightColor, color) != 0 ||
      memcmp (g_shader_constants_stable.SunLightDirection, v) != 0)
  {
    g_shader_constants_stable.SunLightColor = color;
    g_shader_constants_stable.SunLightDirection = v;
    g_shader_constants_stable_dirty = true;
  }
}

//---------------------------------------------------------------------

void Make2DMatrix (out XMMATRIX m)   // for rendering 2D objects in range -0.5 .. +0.5
{
  clear m;
  m.m[0][0] = 2.0;
  m.m[1][1] = 2.0;
  m.m[3][3] = 1.0;
}

//---------------------------------------------------------------------

// important: when enabling fog, pass also the fog color to set_background_color().

public void set_fog (
   float   start_z,
   float   end_z,
   vector3 color = {0.62,0.62,0.62},
   float   attenuation = 1.0)
{
  g_shader_constants_stable.fogColor = color;

  if (end_z <= start_z)
    g_shader_constants_stable.fogInvRange = 0.0;
  else
  {
    g_shader_constants_stable.fogStart = start_z;
    g_shader_constants_stable.fogInvRange = 1.0 / (end_z - start_z);
    g_shader_constants_stable.FogInvExponent = attenuation;
  }

  g_shader_constants_stable_dirty = true;
}

//---------------------------------------------------------------------

public void set_object_colors (OBJECT_COLORS colors)
{
  g_shader_constants_dynamic.MaterialColor             = colors.MaterialColor;
  g_shader_constants_dynamic.EmissiveColor             = colors.EmissiveColor;
  g_shader_constants_dynamic.metallic                  = colors.metallic;
  g_shader_constants_dynamic.smooth                    = colors.smooth;
  g_shader_constants_dynamic.is_ambiantlight_on        = (uint)colors.ambiant_enabled;
  g_shader_constants_dynamic.is_sunlight_on            = (uint)colors.sunlight_enabled;
  g_shader_constants_dynamic.is_ambiant_downwards      = (uint)colors.ambiant_downwards;
  g_shader_constants_dynamic.disable_ambiant_sun_underground = (uint)colors.disable_ambiant_sun_underground;
  g_shader_constants_dynamic.translucid                = colors.translucid;
  g_shader_constants_dynamic.parallax_factor           = colors.parallax_factor;
}

//---------------------------------------------------------------------

public void set_perlin_noise (bool    enable,
                              float   perlin_amplitude = 1.0,
                              float   perlin_speed = 1.0)
{
  if (enable)
  {
    uint POW = 25;
    int tclock = (int)(g_frame_tick & ((1 << POW)-1));  // lower bits
    if ((g_frame_tick & (1 << POW)) != 0)               // overflow
      tclock = (int)(1 << POW) - (int)tclock;           // set a negative value
    g_shader_constants_dynamic.tclock = perlin_speed * (1.0 / 1003.0) * (float)(tclock + 1);  // add 1 to make sure it's never zero
    g_shader_constants_dynamic.perlin_amplitude = perlin_amplitude;
  }
  else
    g_shader_constants_dynamic.tclock = 0.0;
}

//---------------------------------------------------------------------

// u,v are transformed as follows :
// u' = u*(fact_u_min_1 + 1) + v*fact_u + ofs_u;
// v' = v*(fact_v_min_1 + 1) + u*fact_v + ofs_v;

public void set_uv_transform (OBJECT_UV_TRANSFORM uv_transform)
{
  g_shader_constants_dynamic.uv_transform = uv_transform;
}

//---------------------------------------------------------------------

// set lights
public void set_lights (LIGHT[] lights)
{
  if (lights'length == g_shader_constants_stable.NbLights &&
      memcmp (lights, g_shader_constants_stable.Light'byte[0:lights'size]) == 0)
    return;   // already identical

  g_shader_constants_stable.NbLights = lights'length;
  g_shader_constants_stable.Light'byte[0:lights'size] = lights'byte;
  g_shader_constants_stable_dirty = true;
}

//---------------------------------------------------------------------

public void set_shadow_matrix (matrix44 m)
{
#begin unsafe
  g_default_shadow_matrix = *(XMMATRIX*)&m;
#end unsafe
  if (g_shadows_level > 0)
    MXXMatrixTranspose (g_default_shadow_matrix, out g_shader_constants_stable.ShadowTransposedTransform);
  g_shader_constants_stable_dirty = true;
}

//---------------------------------------------------------------------

public void set_camera (vector3 eye, quaternion view, vector3 world_position = {0.0, 0.0, 0.0})
{
  float x2 = 2.0 * (float)view.x;
  float y2 = 2.0 * (float)view.y;
  float z2 = 2.0 * (float)view.z;

  float xx = x2 * (float)view.x;
  float xy = x2 * (float)view.y;
  float xz = x2 * (float)view.z;
  float xw = x2 * (float)view.w;
  float yy = y2 * (float)view.y;
  float yz = y2 * (float)view.z;
  float yw = y2 * (float)view.w;
  float zz = z2 * (float)view.z;
  float zw = z2 * (float)view.w;

  XMMATRIX vm;
  int      i;

  vm = {m =>
         {{1.0 - (yy + zz),  (xy - zw),        (xz + yw),         0.0},
          {(xy + zw),        1.0 - (xx + zz),  (yz - xw),         0.0},
          {(xz - yw),        (yz + xw),        1.0 - (xx + yy),   0.0},
          {0.0,              0.0,              0.0,               1.0}}};

  // add eye on matrix line 3
  for (i=0; i<3; i++)
    vm.m[3][i] = -vm.m[0][i] * eye[0] - vm.m[1][i] * eye[1] - vm.m[2][i] * eye[2];

  g_default_view_matrix = vm;   // used by renderer & by compute_picking_ray()
  g_default_camera_eye = eye;
  g_world_position = world_position;

#begin unsafe
  build_view_frustum (*(matrix44*)&g_default_view_matrix, *(matrix44*)&g_default_projection_matrix, out g_frustum);
#end unsafe
}

//---------------------------------------------------------------------

void intern_compute_picking_ray (    int         mouse_x,
                                     int         mouse_y,
                                     matrix44    view_matrix,
                                 out PICKING_RAY picking_ray)
{
  int       vPickx, vPicky;
  XMFLOAT3  vPickDir, vPickNear, vPickFar;
  float     vPickLen;
  XMMATRIX  m;
  XMFLOAT3  v;
  XMFLOAT3  vPickRayOrig;
  float     d;

  vPickx = mouse_x;
  vPicky = mouse_y;

  // Get the inverse view matrix
#begin unsafe
  D3DXMatrixInverse (&m, null, (XMMATRIX*)&view_matrix);
#end unsafe

#if 0
inverse is :

right.x right.y right.z 0
up.x    up.y    up.z    0
fwd.x   fwd.y   fwd.z   0
eye.x   eye.y   eye.z   1
#endif

  vPickRayOrig = {m.m[3][0], m.m[3][1], m.m[3][2]};     // eye origin

  v = {((float)(2*vPickx) / (float)g_back_buffer_x_res - 1.0) / g_default_projection_matrix.m[0][0],
       - ((float)(2*vPicky) / (float)g_back_buffer_y_res - 1.0) / g_default_projection_matrix.m[1][1],
       1.0f};

  vPickDir = {v[0]*m.m[0][0] + v[1]*m.m[1][0] + v[2]*m.m[2][0],
              v[0]*m.m[0][1] + v[1]*m.m[1][1] + v[2]*m.m[2][1],
              v[0]*m.m[0][2] + v[1]*m.m[1][2] + v[2]*m.m[2][2]};

  D3DXVec3Normalize (out vPickDir, vPickDir);

//  trace ("vPickRayOrig = {%f, %f, %f}\n", vPickRayOrig.x, vPickRayOrig.y, vPickRayOrig.z);
//  trace ("vPickDir     = {%f, %f, %f}\n", vPickDir.x, vPickDir.y, vPickDir.z);

  d = 0.0; // (float)sqrt (near_z*near_z + v.x*v.x + v.y*v.y);

  vPickNear = {vPickRayOrig[0] + d*vPickDir[0],
               vPickRayOrig[1] + d*vPickDir[1],
               vPickRayOrig[2] + d*vPickDir[2]};

//  trace ("vPickNear = {%f, %f, %f}\n", vPickNear.x, vPickNear.y, vPickNear.z);

  d = (float)sqrt (g_far_z*g_far_z + v[0]*v[0] + v[1]*v[1]);

  vPickFar = {vPickRayOrig[0] + d*vPickDir[0],
              vPickRayOrig[1] + d*vPickDir[1],
              vPickRayOrig[2] + d*vPickDir[2]};

//  trace ("vPickFar  = {%f, %f, %f}\n", vPickFar.x, vPickFar.y, vPickFar.z);

  vPickLen = length_3 ({vPickFar[0] - vPickNear[0],
                        vPickFar[1] - vPickNear[1],
                        vPickFar[2] - vPickNear[2]});

  picking_ray = { vPickNear => {vPickNear[0], vPickNear[1], vPickNear[2]},
                  vPickFar  => {vPickFar[0], vPickFar[1], vPickFar[2]},
                  vPickDir  => {vPickDir[0], vPickDir[1], vPickDir[2]},
                  vPickLen  => vPickLen };
}

//---------------------------------------------------------------------

public
void compute_picking_ray (int             mouse_x,
                          int             mouse_y,
                          out PICKING_RAY picking_ray)
{
#begin unsafe
  intern_compute_picking_ray (mouse_x, mouse_y, *(matrix44*)&g_default_view_matrix, out picking_ray);
#end unsafe
}

//---------------------------------------------------------------------

public
void compute_picking_ray_for_hud (int             mouse_x,
                                  int             mouse_y,
                                  out PICKING_RAY picking_ray)
{
  intern_compute_picking_ray (mouse_x, mouse_y, M44_ONE, out picking_ray);
}

//---------------------------------------------------------------------

public
bool ray_intersects_sphere (vector3     center,
                            float       squared_radius,  // radius * radius
                            PICKING_RAY ray)
{
  vector3 vh, vClosest, vToCenter;
  float   a;

  sub_3 (center, ray.vPickNear, out vh);

  a = dot_product_3 (vh, ray.vPickDir);

  // "a" equals the distance from the ray's near plane intersection
  // to the point on the ray nearest the center of the sphere.
  // If this point is outside the line segment contained in the frustum,
  // we constrain it to the near or far end of the segment.

  if (a < 0.0)
    a = 0.0;
  else if (a > ray.vPickLen)
    a = ray.vPickLen;

  mult_3_1 (ray.vPickDir, a, out vClosest);
  add_3 (vClosest, ray.vPickNear, out vClosest);

  sub_3 (center, vClosest, out vToCenter);

  // calculates the square of the distance from the closest point on the line segment
  // to the center of the bounding sphere.
  // If it is less than or equal to the square of the bounding sphere radius,
  // we have a possible hit.

  return (squared_radius >= dot_product_3 (vToCenter, vToCenter));
}

//---------------------------------------------------------------------

public
bool ray_intersects_triangle (    vector3     vert0,
                                  vector3     vert1,
                                  vector3     vert2,
                                  PICKING_RAY ray,
                              out float[2]    texture_info,
                              out float       distance,
                                  bool        test_both_sides = false)  // false is much faster
{
  const float EPSILON = 0.000001;

  vector3 edge1, edge2, tvec, pvec, qvec;
  float   u, v, det, inv_det;

  // find vectors for two edges sharing vert0
  sub_3 (vert1, vert0, out edge1);
  sub_3 (vert2, vert0, out edge2);

  // begin calculating determinant - also used to calculate u parameter
  cross_product_3 (ray.vPickDir, edge2, out pvec);

  // if determinant is near zero, ray lies in plane of triangle
  det = dot_product_3 (edge1, pvec);

  if (!test_both_sides)
  {
    if (det < EPSILON)
    {
      clear texture_info, distance;
      return false;
    }

    // calculate distance from vert0 to ray origin
    sub_3 (ray.vPickNear, vert0, out tvec);

    // calculate U parameter and test bounds
    u = dot_product_3 (tvec, pvec);
    if (u < 0.0 || u > det)
    {
      clear texture_info, distance;
      return false;
    }

    // prepare to test V parameter
    cross_product_3 (tvec, edge1, out qvec);

    // calculate V parameter and test bounds
    v = dot_product_3 (ray.vPickDir, qvec);
    if (v < 0.0 || u+v > det)
    {
      clear texture_info, distance;
      return false;
    }

    // calculate distance, scale uv
    distance = dot_product_3 (edge2, qvec);
    inv_det = 1.0 / det;
    distance *= inv_det;

    if (distance < 0.0 || distance > ray.vPickLen)
    {
      clear texture_info, distance;
      return false;
    }

    texture_info = {u * inv_det, v * inv_det};
  }
  else   // the non-culling branch
  {
    if (det > -EPSILON && det < EPSILON)
    {
      clear texture_info, distance;
      return false;
    }

    inv_det = 1.0 / det;

    // calculate distance from vert0 to ray origin
    sub_3 (ray.vPickNear, vert0, out tvec);

    // calculate U parameter and test bounds
    u = dot_product_3 (tvec, pvec) * inv_det;
    if (u < 0.0 || u > 1.0)
    {
      clear texture_info, distance;
      return false;
    }

    // prepare to test V parameter
    cross_product_3 (tvec, edge1, out qvec);

    // calculate V parameter and test bounds
    v = dot_product_3 (ray.vPickDir, qvec) * inv_det;
    if (v < 0.0 || u+v > 1.0)
    {
      clear texture_info, distance;
      return false;
    }

    // calculate distance
    distance = dot_product_3 (edge2, qvec) * inv_det;

    if (distance < 0.0 || distance > ray.vPickLen)
    {
      clear texture_info, distance;
      return false;
    }

    texture_info = {u, v};
  }

  return true;
}

//---------------------------------------------------------------------

public
void ray_compute_texture_uv (    vector2  tex0,           // .t components of triangle
                                 vector2  tex1,
                                 vector2  tex2,
                                 float[2] texture_info,   // result from ray_intersects_triangle()
                             out vector2  uv)             // texture coordinates
{
  uv = {tex0[0] + texture_info[0] * (tex1[0]-tex0[0]) + texture_info[1] * (tex2[0]-tex0[0]),
        tex0[1] + texture_info[0] * (tex1[1]-tex0[1]) + texture_info[1] * (tex2[1]-tex0[1])};
}

//---------------------------------------------------------------------

void build_view_frustum (    matrix44 m_view,
                             matrix44 m_projection,
                         out vector4  m_frustum[6])  // 6 plane a, b, c, d
{
  matrix44 viewProjection;
  int i;

  clear m_frustum;
  mult_44_44 (m_view, m_projection, out viewProjection);

  transpose_44 (viewProjection, out viewProjection);

  // Left plane
  add_4 (viewProjection[3], viewProjection[0], out m_frustum[0]);

  // Right plane
  sub_4 (viewProjection[3], viewProjection[0], out m_frustum[1]);

  // Top plane
  sub_4 (viewProjection[3], viewProjection[1], out m_frustum[2]);

  // Bottom plane
  add_4 (viewProjection[3], viewProjection[1], out m_frustum[3]);

  // Near plane
  m_frustum[4] = viewProjection[2];

  // Far plane
  sub_4 (viewProjection[3], viewProjection[2], out m_frustum[5]);

  // Normalize planes
  for (i=0; i<6; i++)
  {
    ref vector4 v = m_frustum[i];
    float f = (float)(1.0 / sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]));
    mult_4_1 (v, f, out v);
  }
}

//---------------------------------------------------------------------

public bool is_sphere_in_frustum (vector3 point, float radius)
{
  int i;
  for (i=0; i<6; i++)
  {
    if (signed_distance_to_plane (g_frustum[i], point) + radius < 0.0)   // outside plane
      return false;
  }
  return true;
}

//---------------------------------------------------------------------

public bool is_box_in_frustum (vector3 center, vector3 size)
{
  int     i;
  vector3 s;

  mult_3_1 (size, 0.5, out s);

  for (i=0; i<6; i++)
  {
    ref vector4 v = g_frustum[i];

    // check if all 8 points of the rectangle are outside the same plane of the view frustum.

    if (signed_distance_to_plane (v, {(center[0] - s[0]), (center[1] - s[1]), (center[2] - s[2])}) >= 0.0)
      continue;

    if (signed_distance_to_plane (v, {(center[0] + s[0]), (center[1] - s[1]), (center[2] - s[2])}) >= 0.0)
      continue;

    if (signed_distance_to_plane (v, {(center[0] - s[0]), (center[1] + s[1]), (center[2] - s[2])}) >= 0.0)
      continue;

    if (signed_distance_to_plane (v, {(center[0] + s[0]), (center[1] + s[1]), (center[2] - s[2])}) >= 0.0)
      continue;

    if (signed_distance_to_plane (v, {(center[0] - s[0]), (center[1] - s[1]), (center[2] + s[2])}) >= 0.0)
      continue;

    if (signed_distance_to_plane (v, {(center[0] + s[0]), (center[1] - s[1]), (center[2] + s[2])}) >= 0.0)
      continue;

    if (signed_distance_to_plane (v, {(center[0] - s[0]), (center[1] + s[1]), (center[2] + s[2])}) >= 0.0)
      continue;

    if (signed_distance_to_plane (v, {(center[0] + s[0]), (center[1] + s[1]), (center[2] + s[2])}) >= 0.0)
      continue;

    return false;
  }

  return true;
}

//---------------------------------------------------------------------

public bool is_shadow_in_frustum (vector3 position, vector3 shadow_points[4], float radius)
{
  vector3 p[4];
  int     i;

  clear p;
  for (i=0; i<4; i++)
    add_3 (position, shadow_points[i], out p[i]);

  // test if all points are outside same plane frustrum at at least radius distance,
  // then the shadow is not visible in frustrum.

  for (i=0; i<6; i++)
  {
    if (signed_distance_to_plane (g_frustum[i], p[0]) + radius < 0.0 &&   // p[0] outside plane
        signed_distance_to_plane (g_frustum[i], p[1]) + radius < 0.0 &&   // p[1] outside plane
        signed_distance_to_plane (g_frustum[i], p[2]) + radius < 0.0 &&   // p[2] outside plane
        signed_distance_to_plane (g_frustum[i], p[3]) + radius < 0.0)     // p[3] outside same plane
      return false;
  }

  return true;
}

//---------------------------------------------------------------------------------------

public void get_view_matrix (out matrix44 view)
{
#begin unsafe
  view = *((matrix44*)&g_default_view_matrix);
#end unsafe
}

//-------------------------------------------------------------------------------------------------
