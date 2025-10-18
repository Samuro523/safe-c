
// compdxshaders.c : create file dxshaders.h with DirectX precompiled shaders

from std use console, files, strings, exception;
use windows, directx11;
use shader_vs, shader_gs, shader_ps, shader_defs, shader_perlin;

//---------------------------------------------------------------------

const string SHADER_VERSION = "_4_1";
// const string SHADER_VERSION = "_5_0";
// const string SHADER_VERSION = "_5_1";  // causes crash at execution!

//---------------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------------

void output_blob (ref FILE f, string name, ID3D10Blob* b)
{
  LPVOID p = b->lpVtbl->GetBufferPointer (b);
  uint s = (uint)b->lpVtbl->GetBufferSize (b);
  uint i;

printf ("%s : %u bytes\n", name, s);

  fprintf (ref f, "\n");
  fprintf (ref f, "const byte[%u] %s = {\n", s, name);

  for (i=0; i<s; i++)
  {
    fprintf (ref f, "%u,", p[i]);
    if ((i & 63) == 63)
      fprintf (ref f, "\n");
  }
  fprintf (ref f, "};\n\n");
}

//---------------------------------------------------------------------

int compile_vertex_shader (string vertex_shader_source, string name, ref FILE f)
{
  const string VS = "vs" + SHADER_VERSION + "\0";
  const string VS_ENTRY = "MyVertexShader\0";  // entry point name required !
  ID3D10Blob* vertexShaderBuffer = null;
  ID3D10Blob* errorMessage = null;
  HRESULT result;


  // Compile the vertex shader code.
  result = D3DCompile (&vertex_shader_source, (uint)strlen(vertex_shader_source), null, null, null, pEntrypoint => &VS_ENTRY,
                       pTarget => &VS, Flags1 => D3DCOMPILE_ENABLE_STRICTNESS, Flags2 => 0,
                       &vertexShaderBuffer, &errorMessage);
  if (result < 0)
  {
    printf ("%s\n", ((char*)(errorMessage->lpVtbl->GetBufferPointer(errorMessage)))[0:(uint)errorMessage->lpVtbl->GetBufferSize(errorMessage)]);
    errorMessage->lpVtbl->Release((LPVOID)errorMessage);
    return -1;
  }

  if (errorMessage != null)
    errorMessage->lpVtbl->Release((LPVOID)errorMessage);

  output_blob (ref f, name, vertexShaderBuffer);

    // Release the vertex shader buffer since it is no longer needed.
  vertexShaderBuffer->lpVtbl->Release((LPVOID)vertexShaderBuffer);

  return 0;
}

//---------------------------------------------------------------------

int compile_geometry_shader (string geometry_shader_source, string name, ref FILE f)
{
  const string GS = "gs" + SHADER_VERSION + "\0";
  const string GS_ENTRY = "MyGeometryShader\0";  // entry point name required !
  ID3D10Blob* geometryShaderBuffer = null;
  ID3D10Blob* errorMessage = null;
  HRESULT result;

  // Compile the geometry shader code.
  result = D3DCompile (&geometry_shader_source, (uint)strlen(geometry_shader_source), null, null, null, pEntrypoint => &GS_ENTRY,
                       pTarget => &GS, Flags1 => D3DCOMPILE_ENABLE_STRICTNESS, Flags2 => 0,
                       &geometryShaderBuffer, &errorMessage);
  if (result < 0)
  {
    printf ("%s\n", ((char*)(errorMessage->lpVtbl->GetBufferPointer(errorMessage)))[0:(uint)errorMessage->lpVtbl->GetBufferSize(errorMessage)]);
    errorMessage->lpVtbl->Release((LPVOID)errorMessage);
    return -1;
  }

  if (errorMessage != null)
    errorMessage->lpVtbl->Release((LPVOID)errorMessage);

  output_blob (ref f, name, geometryShaderBuffer);

    // Release the geometry shader buffer since it is no longer needed.
  geometryShaderBuffer->lpVtbl->Release((LPVOID)geometryShaderBuffer);

  return 0;
}

//---------------------------------------------------------------------

int compile_pixel_shader (string pixel_shader_source, string name, ref FILE f)
{
  const string PS = "ps" + SHADER_VERSION + "\0";
  const string PS_ENTRY = "MyPixelShader\0";   // entry point name required !
  ID3D10Blob* pixelShaderBuffer = null;
  ID3D10Blob* errorMessage = null;
  HRESULT result;

  // Compile the pixel shader code.
  result = D3DCompile (&pixel_shader_source, (uint)strlen(pixel_shader_source), null, null, null, pEntrypoint => &PS_ENTRY,
                       pTarget => &PS, Flags1 => D3DCOMPILE_ENABLE_STRICTNESS, Flags2 => 0,
                       &pixelShaderBuffer, &errorMessage);
  if (result < 0)
  {
    printf ("%s\n", ((char*)(errorMessage->lpVtbl->GetBufferPointer(errorMessage)))[0:(uint)errorMessage->lpVtbl->GetBufferSize(errorMessage)]);
    errorMessage->lpVtbl->Release((LPVOID)errorMessage);
    return -1;
  }

  if (errorMessage != null)
    errorMessage->lpVtbl->Release((LPVOID)errorMessage);

  output_blob (ref f, name, pixelShaderBuffer);

  // Release the vertex shader buffer and pixel shader buffer since they are no longer needed.
  pixelShaderBuffer->lpVtbl->Release((LPVOID)pixelShaderBuffer);

  return 0;
}

//---------------------------------------------------------------------

void create_all_shaders (ref FILE f)
{

////////////////////////////////////////////////////////////////////////////////
// basic vertex shader
////////////////////////////////////////////////////////////////////////////////

  {
    const string vs = StructPointLight + VarConstantBuffer0 + VarConstantBuffer1 
                    + VertexInputType + PixelInputType 
                    + ShadowVertexInputType + ShadowVertexOutputType;
    int rc;
    rc = compile_vertex_shader (vs + VERTEX_SHADER_BASIC_RENDERING, "vertex_shader_basic_rendering", ref f);
    assert rc == 0;
    rc = compile_vertex_shader (vs + VERTEX_SHADER_BASIC_BUILD_SHADOW_MAP, "vertex_shader_basic_build_shadow_map", ref f);
    assert rc == 0;
  }

////////////////////////////////////////////////////////////////////////////////
// vertex shader for rigged mesh
////////////////////////////////////////////////////////////////////////////////

  {
    const string DEFS = StructPointLight + VarConstantBuffer0 + VarConstantBuffer1 + VarConstantBuffer2
                     + VertexInputTypeRiggedMesh + PixelInputType 
                     + ShadowVertexInputTypeRiggedMesh + ShadowVertexOutputType;
    int  rc;

    rc = compile_vertex_shader (DEFS + VERTEX_SHADER_RIGGED_RENDERING, "vertex_shader_rigged_rendering", ref f);
    assert rc == 0;

    rc = compile_vertex_shader (DEFS + VERTEX_SHADER_RIGGED_BUILD_SHADOW_MAP, "vertex_shader_rigged_build_shadow_map", ref f);
    assert rc == 0;
  }


////////////////////////////////////////////////////////////////////////////////
// geometry shader
////////////////////////////////////////////////////////////////////////////////


  // Geometry Shader
  {
    const string DEFS = ShadowVertexOutputType + ShadowGeometricOutputType;
    int  rc;

    rc = compile_geometry_shader (DEFS + GEOMETRY_SHADER_SHADOWS, "geometry_shader_shadows", ref f);
    assert rc == 0;
 }


////////////////////////////////////////////////////////////////////////////////
// basic pixel shader
////////////////////////////////////////////////////////////////////////////////


  // Basic Pixel Shader
  {
    const string DEFS = StructPointLight + VarConstantBuffer0 + VarConstantBuffer1 +
                        PixelInputType + PixelOutputType + PERLIN_NOISE;
    int  rc;

    rc = compile_pixel_shader (DEFS + PIXEL_SHADER_SOLID_RENDERING, "pixel_shader_solid_rendering", ref f);
    assert rc == 0;
 }
}

//---------------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------------

void main()
{
  FILE f;
  arm_exception_handler ();
  printf ("generating file dxshaders.h ..\n");
  assert fcreate (out f, "dxshaders.h", ANSI) == 0;
  fprintf (ref f, "\n");
  fprintf (ref f, "// dxshaders.h : compiled shaders (this file is automatically generated by compdxshaders.c)\n");
  create_all_shaders (ref f);

  fprintf (ref f, "const byte[] vertex_shader_basic[2]  = {vertex_shader_basic_rendering,  vertex_shader_basic_build_shadow_map}; \n");
  fprintf (ref f, "const byte[] vertex_shader_rigged[2] = {vertex_shader_rigged_rendering, vertex_shader_rigged_build_shadow_map}; \n");
  fprintf (ref f, "const byte[] geometry_shader[2]      = {{},                           geometry_shader_shadows}; \n");
  fprintf (ref f, "const byte[] pixel_shader_solid[2]   = {pixel_shader_solid_rendering, {}}; \n");

  fclose (ref f);
  printf ("ok\n");
}

//---------------------------------------------------------------------
