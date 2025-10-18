
// d3dutil.c

use math;

#begin unsafe

public void D3DXVec3Normalize (out XMFLOAT3 o, XMFLOAT3 i)
{
  float f = (float)(1.0 / (float)sqrt (i[0]*i[0] + i[1]*i[1] + i[2]*i[2]));
  o = {i[0] * f, i[1] * f, i[2] * f};
}

public void D3DXVec3Subtract (XMFLOAT3* r, XMFLOAT3 a, XMFLOAT3 b)
{
  (*r)[0] = a[0] - b[0];
  (*r)[1] = a[1] - b[1];
  (*r)[2] = a[2] - b[2];
}

public void D3DXVec3Cross (XMFLOAT3 *r, XMFLOAT3 a, XMFLOAT3 b)
{
  (*r)[0] = a[1]*b[2] - a[2]*b[1];
  (*r)[1] = a[2]*b[0] - a[0]*b[2];
  (*r)[2] = a[0]*b[1] - a[1]*b[0];
}

public float D3DXVec3Dot (XMFLOAT3 a, XMFLOAT3 b)
{
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

public void XMMatrixIdentity (XMMATRIX *pout)
{
  clear *pout;
  pout->m[0][0] = 1.0;
  pout->m[1][1] = 1.0;
  pout->m[2][2] = 1.0;
  pout->m[3][3] = 1.0;
}

public void D3DXMatrixTranslation (XMMATRIX *pout, FLOAT x, FLOAT y, FLOAT z)
{
  XMMatrixIdentity(pout);
  pout->m[3][0] = x;
  pout->m[3][1] = y;
  pout->m[3][2] = z;
}

public void D3DXMatrixLookAtLH (XMMATRIX* pout,
                                XMFLOAT3 peye,
                                XMFLOAT3 pat,
                                XMFLOAT3 pup)
{
  XMFLOAT3 right, rightn, up, upn, vec, vec2;

  D3DXVec3Subtract((XMFLOAT3*)&vec2, pat, peye);
  D3DXVec3Normalize (out vec, vec2);  // vec   : on z-axis
  D3DXVec3Cross((XMFLOAT3*)&right, pup, vec);   // right : on x-axes
  D3DXVec3Cross((XMFLOAT3*)&up, vec, right);   // up    : on y-axes
  D3DXVec3Normalize (out rightn, right);
  D3DXVec3Normalize (out upn, up);

  pout->m[0][0] = rightn[0];
  pout->m[1][0] = rightn[1];
  pout->m[2][0] = rightn[2];
  pout->m[3][0] = -D3DXVec3Dot(rightn,peye);
  pout->m[0][1] = upn[0];
  pout->m[1][1] = upn[1];
  pout->m[2][1] = upn[2];
  pout->m[3][1] = -D3DXVec3Dot(upn, peye);
  pout->m[0][2] = vec[0];
  pout->m[1][2] = vec[1];
  pout->m[2][2] = vec[2];
  pout->m[3][2] = -D3DXVec3Dot(vec, peye);
  pout->m[0][3] = 0.0f;
  pout->m[1][3] = 0.0f;
  pout->m[2][3] = 0.0f;
  pout->m[3][3] = 1.0f;
}


public void D3DXMMatrixPerspectiveFovLH (XMMATRIX *pout, FLOAT fovy,
                                         FLOAT aspect, FLOAT zn, FLOAT zf)
{
  XMMatrixIdentity (pout);
 
  pout->m[0][0] = 1.0f / (aspect * (float)tan(fovy/2.0f));
  pout->m[1][1] = 1.0f / (float)tan(fovy/2.0f);
  pout->m[2][2] = zf / (zf - zn);
  pout->m[2][3] = 1.0f;
  pout->m[3][2] = (zf * zn) / (zn - zf);
  pout->m[3][3] = 0.0f;
}

public void D3DXMatrixPerspectiveLH(XMMATRIX *pout, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf)
{
    XMMatrixIdentity(pout);
    pout->m[0][0] = 2.0f * zn / w;
    pout->m[1][1] = 2.0f * zn / h;
    pout->m[2][2] = zf / (zf - zn);
    pout->m[3][2] = (zn * zf) / (zn - zf);
    pout->m[2][3] = 1.0f;
    pout->m[3][3] = 0.0f;
}

public void D3DXMatrixOrthoLH (XMMATRIX *pout, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf)
{
    XMMatrixIdentity(pout);
    pout->m[0][0] = 2.0f / w;
    pout->m[1][1] = 2.0f / h;
    pout->m[2][2] = 1.0f / (zf - zn);
    pout->m[3][2] = zn / (zn - zf);
}


public
void D3DXVec4Cross (XMVECTOR *pout, XMVECTOR pv1, XMVECTOR pv2, XMVECTOR pv3)
{
  (*pout)[0] = pv1[1] * (pv2[2] * pv3[3] - pv3[2] * pv2[3]) - pv1[2] * (pv2[1] * pv3[3] - pv3[1] * pv2[3]) + pv1[3] * (pv2[1] * pv3[2] - pv2[2] *pv3[1]);
  (*pout)[1] = -(pv1[0] * (pv2[2] * pv3[3] - pv3[2] * pv2[3]) - pv1[2] * (pv2[0] * pv3[3] - pv3[0] * pv2[3]) + pv1[3] * (pv2[0] * pv3[2] - pv3[0] * pv2[2]));
  (*pout)[2] = pv1[0] * (pv2[1] * pv3[3] - pv3[1] * pv2[3]) - pv1[1] * (pv2[0] *pv3[3] - pv3[0] * pv2[3]) + pv1[3] * (pv2[0] * pv3[1] - pv3[0] * pv2[1]);
  (*pout)[3] = -(pv1[0] * (pv2[1] * pv3[2] - pv3[1] * pv2[2]) - pv1[1] * (pv2[0] * pv3[2] - pv3[0] *pv2[2]) + pv1[2] * (pv2[0] * pv3[1] - pv3[0] * pv2[1]));
}

public
float D3DXMatrixfDeterminant (XMMATRIX* pm)
{
  XMVECTOR minor, v1, v2, v3;
  float       det;

  v1 = {pm->m[0][0], pm->m[1][0], pm->m[2][0], pm->m[3][0]};
  v2 = {pm->m[0][1], pm->m[1][1], pm->m[2][1], pm->m[3][1]};
  v3 = {pm->m[0][2], pm->m[1][2], pm->m[2][2], pm->m[3][2]};
  D3DXVec4Cross((XMVECTOR*)&minor,v1,v2,v3);
  det =  - (pm->m[0][3] * minor[0] + pm->m[1][3] * minor[1] + pm->m[2][3] * minor[2] + pm->m[3][3] * minor[3]);
  return det;
}


public
bool D3DXMatrixInverse (XMMATRIX* pout, float* pdeterminant, XMMATRIX* pm)
{
  int      a, i, j;
  XMVECTOR v, vec[3];
  float    cofactor, det, idet, sign;

  det = D3DXMatrixfDeterminant (pm);
  if (det == 0.0)
    return false;

  if (pdeterminant != null)
   *pdeterminant = det;

  idet = 1.0 / det;

  clear vec;

  for (i=0,sign=1.0; i<4; i++,sign=-sign)
  {
    for (j=0; j<4; j++)
    {
      if (j != i)
      {
        a = j;
        if (j > i)
          a = a-1;
        vec[a][0] = pm->m[j][0];
        vec[a][1] = pm->m[j][1];
        vec[a][2] = pm->m[j][2];
        vec[a][3] = pm->m[j][3];
      }
    }

    D3DXVec4Cross ((XMVECTOR*)&v, vec[0], vec[1], vec[2]);

    for (j=0; j<4; j++)
    {
      switch (j)
      {
        case 0: cofactor = v[0]; break;
        case 1: cofactor = v[1]; break;
        case 2: cofactor = v[2]; break;
        case 3: cofactor = v[3]; break;
        default: abort;
      }
      pout->m[j][i] = sign * cofactor * idet;
    }
  }

  return true;
}


public
void D3DXVec4Transform (XMVECTOR* pout, XMVECTOR pv, XMMATRIX* pm)
{
  (*pout)[0] = pm->m[0][0] * pv[0] + pm->m[1][0] * pv[1] + pm->m[2][0] * pv[2] + pm->m[3][0] * pv[3];
  (*pout)[1] = pm->m[0][1] * pv[0] + pm->m[1][1] * pv[1] + pm->m[2][1] * pv[2] + pm->m[3][1] * pv[3];
  (*pout)[2] = pm->m[0][2] * pv[0] + pm->m[1][2] * pv[1] + pm->m[2][2] * pv[2] + pm->m[3][2] * pv[3];
  (*pout)[3] = pm->m[0][3] * pv[0] + pm->m[1][3] * pv[1] + pm->m[2][3] * pv[2] + pm->m[3][3] * pv[3];
}

public
void MXXMatrixTranspose (XMMATRIX input, out XMMATRIX output)
{
  int i, j;
  XMMATRIX* p = &output;  // dummy to avoid clear output
  _unused p;
  for (i=0; i<4; i++)
  {
    for (j=0; j<4; j++)
      output.m[i][j] = input.m[j][i];
  }
}

#end unsafe


