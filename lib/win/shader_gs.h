
// shader_gs.h

//---------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// geometry shader
////////////////////////////////////////////////////////////////////////////////

const string GEOMETRY_SHADER_SHADOWS = 
  "[maxvertexcount(3)] " +
  "void MyGeometryShader (triangle ShadowVertexOutputType In[3], inout TriangleStream<ShadowGeometricOutputType> triStream) " +
  "{ " +
  "  ShadowGeometricOutputType Out; " +

     // Set render target index.
  "   Out.RenderTargetIndex = In[0].RenderTargetIndex; " +

     // Pass triangle through.
  "   Out.position = In[0].position; " +
  "   triStream.Append(Out); " +

  "   Out.position = In[1].position; " +
  "   triStream.Append(Out); " +

  "   Out.position = In[2].position; " +
  "   triStream.Append(Out); " +

  "   triStream.RestartStrip(); " +
  "} " +
  "";

//---------------------------------------------------------------------
