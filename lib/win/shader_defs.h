
// shader_defs.h

const string StructPointLight =
  "struct LIGHT " +        // 48 bytes
  "{ " +
    "float3 color;       " +  // used for diffuse and specular
    "float  range;       " +  // distance on which it has effect
    "float3 position;    " +  // position of light
    "float  attenuation; " +  // attenuation exponent : 0(no attenuation with distance) .. 1(linear attenuation)
    "float3 direction;   " +  // for spotlight (normal vector "light towards world")
    "float  cone;        " +  // for spotlight (0.0 = no spotlight)
  "}; ";

const string VarConstantBuffer0 =    // stable data, does not change often, both for vertex and pixel shaders.
  "cbuffer ConstantBuffer0 : register(b0) " +
  "{ " +
    "matrix<float,4,4> viewMatrix;         " +
    "matrix<float,4,4> projectionMatrix;   " +
    "matrix<float,4,4> shadowMatrix; " +

    "float3 eyePosition;         " +
    "float  fogInvRange;         " +

    "float3 fogColor;            " +
    "float  fogStart;            " +

    "float3 CameraOffset;        " +   // used for perlin noise water and for disable_ambiant_sun_underground
    "float  FogInvExponent;      " +

    "float1 filler1;             " +
    "int    test_indexing;       " +
    "int    nb_of_shadow_maps;   " +
    "int    indexes_supported;   " +   // 0 = no dynamic indexing (old AMD card), 1 = dynamic indexing supported

    "float3 AmbiantLightColor; " +
    "float  filler3; " +

    "float3 SunLightColor; " +
    "float  filler2; " +

    "float3 toSun;  " +               // normal vector "world towards sun"
    "int    NbLights;            " +

    "LIGHT  Light[128];          " +  // 6144 = 96 x 64
  "}; ";

const string VarConstantBuffer1 =   // dynamic data, changes for each draw, both for vertex and pixel shaders.
  "cbuffer ConstantBuffer1 : register(b1) " +
  "{ " +
    "matrix<float,4,4> worldMatrix; " +

    "float4 MaterialColor; " +

    "float3 EmissiveLightColor; " +
    "float  in_metallic; " +         // 0 .. 1

    "float  fact_u_min_1; " +
    "float  fact_v_min_1; " +
    "float  fact_u; " +
    "float  fact_v; " +

    "float  ofs_u; " +
    "float  ofs_v; " +
    "float  tclock; " +
    "float  in_smooth; " +     // 0 .. 1  (avoid 1)

    "int    is_ambiantlight_on; " +
    "int    is_sunlight_on; " +
    "int    is_ambiant_downwards; " +
    "int    disable_ambiant_sun_underground; " +

    "float  ddummy1; " +
    "float  ddummy2; " +
    "float  ddummy3; " +
    "float  parallax_factor; " +    // 0 = none, example -0.05

    "float  perlin_amplitude; " +   // 1.0
    "float  translucid; " +  // 0.0 to 1.0
    "int    has_bump_texture; " +   // byte 1 = bump texture
    "int    has_mro_texture; " +    // R:occlusion, G:roughness, B:metal texture

    "float4 object_id; " +
  "}; ";

//-------------------------------------------------------------------------

const string VarConstantBuffer2 =   // bone matrices, only for rigged mesh vertex shader.
  "cbuffer ConstantBuffer2 : register(b2) " +
  "{ " +
    "matrix<float,4,4> g_bone[192]; " +
  "}; ";

//-------------------------------------------------------------------------

const string VertexInputType =      // for basic vertex shader
  "struct VertexInputType " +
  "{ " +
    "float4 position : POSITION;  " +  // automatically converted from float3
    "float3 normal   : NORMAL;    " +
    "float3 tangent  : TANGENT;   " +
    "float4 diffuse  : COLOR;     " +  // diffuse color (uint will be converted to float4)
    "float2 tex      : TEXCOORD;  " +
    "uint   instance : SV_InstanceID; " +
  "}; ";

const string ShadowVertexInputType =
  "struct ShadowVertexInputType " +
  "{ " +
    "float4 position : POSITION;  " +  // automatically converted from float3
    "float3 normal   : NORMAL;    " +  // automatically converted from float3
    "float3 tangent  : TANGENT;   " +  // automatically converted from float3
    "float4 diffuse  : COLOR;     " +  // diffuse color (uint will be converted to float4)
    "float2 tex      : TEXCOORD;  " +
    "uint   instance : SV_InstanceID; " +
   "}; ";

//-------------------------------------------------------------------------

const string VertexInputTypeRiggedMesh =   // for rigged mesh shader
  "struct VertexInputTypeRiggedMesh " +
  "{ " +
    "float4 position  : POSITION;   " +  // automatically converted from float3
    "float3 normal    : NORMAL;     " +
    "float3 tangent   : TANGENT;    " +  // automatically converted from float3
    "float2 tex       : TEXCOORD;   " +
    "uint   bone      : BONE;       " +
    "float  weight[4] : WEIGHT;     " +  // 0.0 = none
    "uint   instance  : SV_InstanceID; " +
  "}; ";


const string ShadowVertexInputTypeRiggedMesh =   // for rigged mesh shader
  "struct ShadowVertexInputTypeRiggedMesh " +
  "{ " +
    "float4 position  : POSITION;   " +  // automatically converted from float3
    "float3 normal    : NORMAL;     " +
    "float3 tangent   : TANGENT;    " +
    "float2 tex       : TEXCOORD;   " +
    "uint   bone      : BONE;       " +
    "float  weight[4] : WEIGHT;     " +  // 0.0 = none
    "uint   instance  : SV_InstanceID; " +
  "}; ";

//-------------------------------------------------------------------------

const string ShadowVertexOutputType =
  "struct ShadowVertexOutputType " +
  "{ " +
    "float4 position          : SV_POSITION;  " +
    "uint   RenderTargetIndex : INT; " +
  "}; ";

const string ShadowGeometricOutputType =
  "struct ShadowGeometricOutputType " +
  "{ " +
    "float4 position          : SV_POSITION;  " +
    "uint   RenderTargetIndex : SV_RenderTargetArrayIndex; " +
  "}; ";

//-------------------------------------------------------------------------

const string PixelInputType =
  "struct PixelInputType " +
  "{ " +
    "float4 position  : SV_POSITION;  " +
    "float4 diffuse   : COLOR;        " + // diffuse color
    "float2 tex       : TEXCOORD;     " +
    "float3 posW      : POSITION;     " + // world position
    "float3 normal    : NORMAL;       " +
    "float3 tangent   : TANGENT;      " +
    "float3 ShadowPos : POSITION2;    " + // shadow position
  "}; ";

//-------------------------------------------------------------------------

const string PixelOutputType =
  "struct PixelOutputType " +
  "{ " +
    "float4 color    : SV_Target0; " +
    "float4 objectid : SV_Target1; " +
  " }; ";

//-------------------------------------------------------------------------
