
// directxdef.h

use draw3d, win/directx11, win/windows;


#begin unsafe

struct DirectX
{
  IDXGISwapChain*           m_swapChain;
  ID3D11Device*             m_device;
  ID3D11DeviceContext*      m_deviceContext;

  ID3D11RenderTargetView*   m_renderTargetView[2], m_msaaRenderTargetView;   // (render, picking), msaa
  ID3D11Texture2D*          m_DepthStencil_Buffer, m_Shadow_depth_Buffer, m_pickingBuffer, m_pickingOutputBuffer, m_temp_multisampling, m_msaaRenderTarget;
  ID3D11DepthStencilView*   m_DepthStencil_View, m_Shadow_depth_View;

  ID3D11DepthStencilState*  m_depthStencilStateDepth[16];
  ID3D11RasterizerState*    m_rasterState[2];        // 0 = normal rendering, 1 = for shadows
  ID3D11BlendState*         m_BlendingState[1+(int)BLENDING'last];
  ID3D11Buffer*             m_constantBufferStable, m_constantBufferDynamic;
  ID3D11SamplerState*       m_sampleState[2];       // 0=pixel shader albedo, 1=shadows
  ID3D11ShaderResourceView* m_shadowTextureView;
  ID3D11ShaderResourceView* m_DefaultTextures[4];   // color texture, bump texture, metal texture, and shadow texture or 1x1 white
}

DirectX DX;

#end unsafe


void fatal_abort (string message, HRESULT error);
void log_error (string message, HRESULT error);
