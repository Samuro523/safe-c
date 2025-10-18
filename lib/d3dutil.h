
// d3dutil.h

#begin unsafe

packed struct XMMATRIX   // float _11, _12, _13, _14, _21, _22, _23, _24, _31, _32, _33, _34, _41, _42, _43, _44
{
  float m[4][4];
}

typedef float FLOAT;
typedef float[4] XMVECTOR;
typedef float[3] XMFLOAT3;

void D3DXVec3Normalize (out XMFLOAT3 o, XMFLOAT3 i);
void D3DXVec3Subtract (XMFLOAT3 *r, XMFLOAT3 a, XMFLOAT3 b);
void D3DXVec3Cross (XMFLOAT3 *r, XMFLOAT3 a, XMFLOAT3 b);
float D3DXVec3Dot (XMFLOAT3 a, XMFLOAT3 b);
void XMMatrixIdentity (XMMATRIX *pout);
void D3DXMatrixTranslation (XMMATRIX *pout, FLOAT x, FLOAT y, FLOAT z);
void D3DXMatrixLookAtLH (XMMATRIX *pout,
                         XMFLOAT3 peye,
                         XMFLOAT3 pat,
                         XMFLOAT3 pup);
void D3DXMMatrixPerspectiveFovLH (XMMATRIX *pout, FLOAT fovy,
                                  FLOAT aspect, FLOAT zn, FLOAT zf);
void D3DXMatrixPerspectiveLH(XMMATRIX *pout, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf);
void D3DXMatrixOrthoLH (XMMATRIX *pout, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf);
void D3DXVec4Cross (XMVECTOR *pout, XMVECTOR pv1, XMVECTOR pv2, XMVECTOR pv3);
float D3DXMatrixfDeterminant (XMMATRIX* pm);
bool D3DXMatrixInverse (XMMATRIX* pout, float* pdeterminant, XMMATRIX* pm);
void D3DXVec4Transform (XMVECTOR* pout, XMVECTOR pv, XMMATRIX* pm);
void MXXMatrixTranspose (XMMATRIX input, out XMMATRIX output);

#end unsafe
