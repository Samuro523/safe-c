
// linear_algebra.c

use math;

// -----------------------------------------------------------------

// 1) vector2

public void add_2 (vector2 a, vector2 b, out vector2 c)
{
  c = {a[0]+b[0], a[1]+b[1]};
}


public void sub_2 (vector2 a, vector2 b, out vector2 c)
{
  c = {a[0]-b[0], a[1]-b[1]};
}


public void mult_2_1 (vector2 a, float b, out vector2 c)
{
  c = {a[0]*b, a[1]*b};
}


public float length_2 (vector2 a)
{
  return (float)sqrt (a[0]*a[0] + a[1]*a[1]);
}

public void inverse_2 (vector2 a, out vector2 c)
{
  float f = 1.0 / (a[0]*a[1]);
  c = {f*a[1], f*a[0]};
}

public void normalize_2 (ref vector2 a)
{
  float lengthInv = 1.0 / (float)sqrt (a[0]*a[0] + a[1]*a[1]);
  a[0] *= lengthInv;
  a[1] *= lengthInv;
}


public float dot_product_2 (vector2 a, vector2 b)
{
  return a[0]*b[0] + a[1]*b[1];
}


// -----------------------------------------------------------------

// 2) vector3

public void add_3 (vector3 a, vector3 b, out vector3 c)
{
  c = {a[0]+b[0], a[1]+b[1], a[2]+b[2]};
}


public void sub_3 (vector3 a, vector3 b, out vector3 c)
{
  c = {a[0]-b[0], a[1]-b[1], a[2]-b[2]};
}


public void mult_3_1 (vector3 a, float b, out vector3 c)
{
  c = {a[0]*b, a[1]*b, a[2]*b};
}


public float length_3 (vector3 a)
{
  return (float)sqrt (a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
}


public void inverse_3 (vector3 a, out vector3 c)
{
  float a01 = a[0] * a[1];
  float f = 1.0 / (a01 * a[2]);
  c = {f * a[1] * a[2],   f * a[0] * a[2],  f * a01};
}


public void normalize_3 (ref vector3 a)
{
  float lengthInv = 1.0 / (float)sqrt (a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
  a[0] *= lengthInv;
  a[1] *= lengthInv;
  a[2] *= lengthInv;
}


public float dot_product_3 (vector3 a, vector3 b)
{
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}


public void cross_product_3 (vector3 a, vector3 b, out vector3 c)
{
  c = {a[1]*b[2] - a[2]*b[1],
       a[2]*b[0] - a[0]*b[2],
       a[0]*b[1] - a[1]*b[0]};
}

// -----------------------------------------------------------------

// 3) vector4

public void add_4 (vector4 a, vector4 b, out vector4 c)
{
  c = {a[0]+b[0], a[1]+b[1], a[2]+b[2], a[3]+b[3]};
}


public void sub_4 (vector4 a, vector4 b, out vector4 c)
{
  c = {a[0]-b[0], a[1]-b[1], a[2]-b[2], a[3]-b[3]};
}


public void mult_4_1 (vector4 a, float b, out vector4 c)
{
  c = {a[0]*b, a[1]*b, a[2]*b, a[3]*b};
}


public float length_4 (vector4 a)
{
  return (float)sqrt (a[0]*a[0] + a[1]*a[1] + a[2]*a[2] + a[3]*a[3]);
}


public void inverse_4 (vector4 a, out vector4 c)
{
  float a01 = a[0] * a[1];
  float a23 = a[2] * a[3];
  float f = 1.0 / (a01*a23);
  c = {f*a[1]*a23, f*a[0]*a23, f*a01*a[3], f*a01*a[2]};
}


public void normalize_4 (ref vector4 a)
{
  float lengthInv = 1.0 / (float)sqrt (a[0]*a[0] + a[1]*a[1] + a[2]*a[2] + a[3]*a[3]);
  a[0] *= lengthInv;
  a[1] *= lengthInv;
  a[2] *= lengthInv;
  a[3] *= lengthInv;
}


public float dot_product_4 (vector4 a, vector4 b)
{
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
}

// -----------------------------------------------------------------

// 4) matrix22

public void add_22 (matrix22 a, matrix22 b, out matrix22 c)
{
  c = {{a[0][0] + b[0][0], a[0][1] + b[0][1]},
       {a[1][0] + b[1][0], a[1][1] + b[1][1]}};
}

public void sub_22 (matrix22 a, matrix22 b, out matrix22 c)
{
  c = {{a[0][0] - b[0][0], a[0][1] - b[0][1]},
       {a[1][0] - b[1][0], a[1][1] - b[1][1]}};
}

public void mult_22_1  (matrix22 a, float b, out matrix22 c)
{
  c = {{a[0][0] * b, a[0][1] * b},
       {a[1][0] * b, a[1][1] * b}};
}

public void mult_21_12 (vector2  a, vector2  b, out matrix22 c)
{
  c = {{a[0] * b[0], a[0] * b[1]},
       {a[1] * b[0], a[1] * b[1]}};
}

public void mult_22_21 (matrix22 a, vector2  b, out vector2  c)
{
  c = {a[0][0]*b[0] + a[0][1]*b[1],
       a[1][0]*b[0] + a[1][1]*b[1]};
}

public void mult_22_22 (matrix22 a, matrix22 b, out matrix22 c)
{
  c = {{a[0][0]*b[0][0]+a[0][1]*b[1][0],
        a[0][0]*b[0][1]+a[0][1]*b[1][1]},
       {a[1][0]*b[0][0]+a[1][1]*b[1][0],
        a[1][0]*b[0][1]+a[1][1]*b[1][1]}};
}

public void mult_12_22 (vector2  a, matrix22 b, out vector2 c)
{
  c = {a[0]*b[0][0] + a[1]*b[1][0],
       a[0]*b[0][1] + a[1]*b[1][1]};
}

public float determinant_22 (matrix22 a)
{
  return a[0][0] * a[1][1] - a[0][1] * a[1][0];
}

public void inverse_22 (matrix22 a, out matrix22 c)
{
  mult_22_1
    ({{a[1][1] , -a[0][1]},
      {-a[1][0],  a[0][0]}},
     1.0 / determinant_22 (a),
     out c);
}

public void transpose_22 (matrix22 a, out matrix22 c)
{
  c = {{a[0][0], a[1][0]},
       {a[0][1], a[1][1]}};
}

// -----------------------------------------------------------------

// 5) matrix33

public void add_33 (matrix33 a, matrix33 b, out matrix33 c)
{
  c = {{a[0][0] + b[0][0], a[0][1] + b[0][1], a[0][2] + b[0][2]},
       {a[1][0] + b[1][0], a[1][1] + b[1][1], a[1][2] + b[1][2]},
       {a[2][0] + b[2][0], a[2][1] + b[2][1], a[2][2] + b[2][2]}};
}


public void sub_33 (matrix33 a, matrix33 b, out matrix33 c)
{
  c = {{a[0][0] - b[0][0], a[0][1] - b[0][1], a[0][2] - b[0][2]},
       {a[1][0] - b[1][0], a[1][1] - b[1][1], a[1][2] - b[1][2]},
       {a[2][0] - b[2][0], a[2][1] - b[2][1], a[2][2] - b[2][2]}};
}


public void transpose_33 (matrix33 a, out matrix33 c)
{
  c = {{a[0][0], a[1][0], a[2][0]},
       {a[0][1], a[1][1], a[2][1]},
       {a[0][2], a[1][2], a[2][2]}};
}


public float determinant_33 (matrix33 a)
{
  return a[0][0] * a[1][1] * a[2][2]  +  a[0][1] * a[1][2] * a[2][0]  +  a[0][2] * a[1][0] * a[2][1]
       - a[0][2] * a[1][1] * a[2][0]  -  a[1][2] * a[2][1] * a[0][0]  -  a[2][2] * a[0][1] * a[1][0];
}


public void inverse_33 (matrix33 a, out matrix33 c)
{
  mult_33_1
    ({{a[1][1]*a[2][2]-a[1][2]*a[2][1], a[0][2]*a[2][1]-a[0][1]*a[2][2], a[0][1]*a[1][2]-a[0][2]*a[1][1]},
      {a[1][2]*a[2][0]-a[1][0]*a[2][2], a[0][0]*a[2][2]-a[0][2]*a[2][0], a[0][2]*a[1][0]-a[0][0]*a[1][2]},
      {a[1][0]*a[2][1]-a[1][1]*a[2][0], a[0][1]*a[2][0]-a[0][0]*a[2][1], a[0][0]*a[1][1]-a[0][1]*a[1][0]}},
     1.0 / determinant_33 (a),
     out c);
}


public void mult_33_33 (matrix33 a, matrix33 b, out matrix33 c)
{
  c = {{a[0][0]*b[0][0]+a[0][1]*b[1][0]+a[0][2]*b[2][0],
        a[0][0]*b[0][1]+a[0][1]*b[1][1]+a[0][2]*b[2][1],
        a[0][0]*b[0][2]+a[0][1]*b[1][2]+a[0][2]*b[2][2]},
       {a[1][0]*b[0][0]+a[1][1]*b[1][0]+a[1][2]*b[2][0],
        a[1][0]*b[0][1]+a[1][1]*b[1][1]+a[1][2]*b[2][1],
        a[1][0]*b[0][2]+a[1][1]*b[1][2]+a[1][2]*b[2][2]},
       {a[2][0]*b[0][0]+a[2][1]*b[1][0]+a[2][2]*b[2][0],
        a[2][0]*b[0][1]+a[2][1]*b[1][1]+a[2][2]*b[2][1],
        a[2][0]*b[0][2]+a[2][1]*b[1][2]+a[2][2]*b[2][2]}};
}


public void mult_31_13 (vector3 a, vector3 b, out matrix33 c)
{
  c = {{a[0] * b[0], a[0] * b[1], a[0] * b[2]},
       {a[1] * b[0], a[1] * b[1], a[1] * b[2]},
       {a[2] * b[0], a[2] * b[1], a[2] * b[2]}};
}


public void mult_33_31 (matrix33 a, vector3 b, out vector3 c)
{
  c = {a[0][0]*b[0] + a[0][1]*b[1] + a[0][2]*b[2],
       a[1][0]*b[0] + a[1][1]*b[1] + a[1][2]*b[2],
       a[2][0]*b[0] + a[2][1]*b[1] + a[2][2]*b[2]};
}


public void mult_33_1 (matrix33 a, float b, out matrix33 c)
{
  c = {{a[0][0] * b, a[0][1] * b, a[0][2] * b},
       {a[1][0] * b, a[1][1] * b, a[1][2] * b},
       {a[2][0] * b, a[2][1] * b, a[2][2] * b}};
}


public void mult_13_33 (vector3 a, matrix33 b, out vector3 c)
{
  c = {a[0]*b[0][0] + a[1]*b[1][0] + a[2]*b[2][0],
       a[0]*b[0][1] + a[1]*b[1][1] + a[2]*b[2][1],
       a[0]*b[0][2] + a[1]*b[1][2] + a[2]*b[2][2]};
}


// multiply matrix33 by triangular matrix33 expressed as vector3

public void mult_33_tri3 (matrix33 a, vector3 b, out matrix33 c)
{
  c = {{a[0][0]*b[0],   a[0][1]*b[1],   a[0][2]*b[2]},
       {a[1][0]*b[0],   a[1][1]*b[1],   a[1][2]*b[2]},
       {a[2][0]*b[0],   a[2][1]*b[1],   a[2][2]*b[2]}};
}


public void star_3 (vector3 a, out matrix33 c)
{
  c = {{0.0,  -a[2],  a[1]},
       {a[2],   0.0, -a[0]},
       {-a[1], a[0],   0.0}};
}

// -----------------------------------------------------------------

// 6) matrix44

public void mult_44_44 (matrix44 a, matrix44 b, out matrix44 c)
{
  c = {{a[0][0]*b[0][0]+a[0][1]*b[1][0]+a[0][2]*b[2][0]+a[0][3]*b[3][0],
        a[0][0]*b[0][1]+a[0][1]*b[1][1]+a[0][2]*b[2][1]+a[0][3]*b[3][1],
        a[0][0]*b[0][2]+a[0][1]*b[1][2]+a[0][2]*b[2][2]+a[0][3]*b[3][2],
        a[0][0]*b[0][3]+a[0][1]*b[1][3]+a[0][2]*b[2][3]+a[0][3]*b[3][3]},
       {a[1][0]*b[0][0]+a[1][1]*b[1][0]+a[1][2]*b[2][0]+a[1][3]*b[3][0],
        a[1][0]*b[0][1]+a[1][1]*b[1][1]+a[1][2]*b[2][1]+a[1][3]*b[3][1],
        a[1][0]*b[0][2]+a[1][1]*b[1][2]+a[1][2]*b[2][2]+a[1][3]*b[3][2],
        a[1][0]*b[0][3]+a[1][1]*b[1][3]+a[1][2]*b[2][3]+a[1][3]*b[3][3]},
       {a[2][0]*b[0][0]+a[2][1]*b[1][0]+a[2][2]*b[2][0]+a[2][3]*b[3][0],
        a[2][0]*b[0][1]+a[2][1]*b[1][1]+a[2][2]*b[2][1]+a[2][3]*b[3][1],
        a[2][0]*b[0][2]+a[2][1]*b[1][2]+a[2][2]*b[2][2]+a[2][3]*b[3][2],
        a[2][0]*b[0][3]+a[2][1]*b[1][3]+a[2][2]*b[2][3]+a[2][3]*b[3][3]},
       {a[3][0]*b[0][0]+a[3][1]*b[1][0]+a[3][2]*b[2][0]+a[3][3]*b[3][0],
        a[3][0]*b[0][1]+a[3][1]*b[1][1]+a[3][2]*b[2][1]+a[3][3]*b[3][1],
        a[3][0]*b[0][2]+a[3][1]*b[1][2]+a[3][2]*b[2][2]+a[3][3]*b[3][2],
        a[3][0]*b[0][3]+a[3][1]*b[1][3]+a[3][2]*b[2][3]+a[3][3]*b[3][3]}};
}

public void mult_44_41 (matrix44 a, vector4 b, out vector4 c)
{
  c = {a[0][0]*b[0] + a[0][1]*b[1] + a[0][2]*b[2] + a[0][3]*b[3],
       a[1][0]*b[0] + a[1][1]*b[1] + a[1][2]*b[2] + a[1][3]*b[3],
       a[2][0]*b[0] + a[2][1]*b[1] + a[2][2]*b[2] + a[2][3]*b[3],
       a[3][0]*b[0] + a[3][1]*b[1] + a[3][2]*b[2] + a[3][3]*b[3]};
}

public void mult_14_44 (vector4 a,  matrix44 b, out vector4  c)
{
  c = {a[0]*b[0][0] + a[1]*b[1][0] + a[2]*b[2][0] + a[3]*b[3][0],
       a[0]*b[0][1] + a[1]*b[1][1] + a[2]*b[2][1] + a[3]*b[3][1],
       a[0]*b[0][2] + a[1]*b[1][2] + a[2]*b[2][2] + a[3]*b[3][2],
       a[0]*b[0][3] + a[1]*b[1][3] + a[2]*b[2][3] + a[3]*b[3][3]};
}


public void inverse_44 (matrix44 a, out matrix44 c)
{
  float r[4][4], invdet;
  int i;

  clear r;
  r[0][0] = a[1][1]  * a[2][2] * a[3][3] -
         a[1][1]  * a[2][3] * a[3][2] -
         a[2][1]  * a[1][2]  * a[3][3] +
         a[2][1]  * a[1][3]  * a[3][2] +
         a[3][1] * a[1][2]  * a[2][3] -
         a[3][1] * a[1][3]  * a[2][2] ;

  r[1][0] = -a[1][0]  * a[2][2] * a[3][3] +
          a[1][0]  * a[2][3] * a[3][2] +
          a[2][0]  * a[1][2]  * a[3][3] -
          a[2][0]  * a[1][3]  * a[3][2] -
          a[3][0] * a[1][2]  * a[2][3] +
          a[3][0] * a[1][3]  * a[2][2] ;

  r[2][0] = a[1][0]  * a[2][1] * a[3][3] -
         a[1][0]  * a[2][3] * a[3][1] -
         a[2][0]  * a[1][1] * a[3][3] +
         a[2][0]  * a[1][3] * a[3][1] +
         a[3][0] * a[1][1] * a[2][3] -
         a[3][0] * a[1][3] * a[2][1] ;

  r[3][0] = -a[1][0]  * a[2][1] * a[3][2] +
           a[1][0]  * a[2][2] * a[3][1] +
           a[2][0]  * a[1][1] * a[3][2] -
           a[2][0]  * a[1][2] * a[3][1] -
           a[3][0] * a[1][1] * a[2][2] +
           a[3][0] * a[1][2] * a[2][1] ;

  r[0][1] = -a[0][1]  * a[2][2] * a[3][3] +
          a[0][1]  * a[2][3] * a[3][2] +
          a[2][1]  * a[0][2] * a[3][3] -
          a[2][1]  * a[0][3] * a[3][2] -
          a[3][1] * a[0][2] * a[2][3] +
          a[3][1] * a[0][3] * a[2][2] ;

  r[1][1] = a[0][0]  * a[2][2] * a[3][3] -
         a[0][0]  * a[2][3] * a[3][2] -
         a[2][0]  * a[0][2] * a[3][3] +
         a[2][0]  * a[0][3] * a[3][2] +
         a[3][0] * a[0][2] * a[2][3] -
         a[3][0] * a[0][3] * a[2][2] ;

  r[2][1] = -a[0][0]  * a[2][1] * a[3][3] +
          a[0][0]  * a[2][3] * a[3][1] +
          a[2][0]  * a[0][1] * a[3][3] -
          a[2][0]  * a[0][3] * a[3][1] -
          a[3][0] * a[0][1] * a[2][3] +
          a[3][0] * a[0][3] * a[2][1] ;

  r[3][1] = a[0][0]  * a[2][1] * a[3][2] -
          a[0][0]  * a[2][2] * a[3][1] -
          a[2][0]  * a[0][1] * a[3][2] +
          a[2][0]  * a[0][2] * a[3][1] +
          a[3][0] * a[0][1] * a[2][2] -
          a[3][0] * a[0][2] * a[2][1] ;

  r[0][2] = a[0][1]  * a[1][2] * a[3][3] -
         a[0][1]  * a[1][3] * a[3][2] -
         a[1][1]  * a[0][2] * a[3][3] +
         a[1][1]  * a[0][3] * a[3][2] +
         a[3][1] * a[0][2] * a[1][3] -
         a[3][1] * a[0][3] * a[1][2] ;

  r[1][2] = -a[0][0]  * a[1][2] * a[3][3] +
          a[0][0]  * a[1][3] * a[3][2] +
          a[1][0]  * a[0][2] * a[3][3] -
          a[1][0]  * a[0][3] * a[3][2] -
          a[3][0] * a[0][2] * a[1][3] +
          a[3][0] * a[0][3] * a[1][2] ;

  r[2][2] = a[0][0]  * a[1][1] * a[3][3] -
          a[0][0]  * a[1][3] * a[3][1] -
          a[1][0]  * a[0][1] * a[3][3] +
          a[1][0]  * a[0][3] * a[3][1] +
          a[3][0] * a[0][1] * a[1][3] -
          a[3][0] * a[0][3] * a[1][1] ;

  r[3][2] = -a[0][0]  * a[1][1] * a[3][2] +
           a[0][0]  * a[1][2] * a[3][1] +
           a[1][0]  * a[0][1] * a[3][2] -
           a[1][0]  * a[0][2] * a[3][1] -
           a[3][0] * a[0][1] * a[1][2] +
           a[3][0] * a[0][2] * a[1][1] ;

  r[0][3] = -a[0][1] * a[1][2] * a[2][3] +
          a[0][1] * a[1][3] * a[2][2] +
          a[1][1] * a[0][2] * a[2][3] -
          a[1][1] * a[0][3] * a[2][2] -
          a[2][1] * a[0][2] * a[1][3] +
          a[2][1] * a[0][3] * a[1][2] ;

  r[1][3] = a[0][0] * a[1][2] * a[2][3] -
         a[0][0] * a[1][3] * a[2][2] -
         a[1][0] * a[0][2] * a[2][3] +
         a[1][0] * a[0][3] * a[2][2] +
         a[2][0] * a[0][2] * a[1][3] -
         a[2][0] * a[0][3] * a[1][2] ;

  r[2][3] = -a[0][0] * a[1][1] * a[2][3] +
           a[0][0] * a[1][3] * a[2][1] +
           a[1][0] * a[0][1] * a[2][3] -
           a[1][0] * a[0][3] * a[2][1] -
           a[2][0] * a[0][1] * a[1][3] +
           a[2][0] * a[0][3] * a[1][1] ;

  r[3][3] = a[0][0] * a[1][1] * a[2][2] -
          a[0][0] * a[1][2] * a[2][1] -
          a[1][0] * a[0][1] * a[2][2] +
          a[1][0] * a[0][2] * a[2][1] +
          a[2][0] * a[0][1] * a[1][2] -
          a[2][0] * a[0][2] * a[1][1] ;

  invdet = 1.0 / ( a[0][0] * r[0][0] + a[0][1] * r[1][0] + a[0][2] * r[2][0] + a[0][3] * r[3][0] );

  clear c;
  for (i=0; i < 4; i++)
  {
     c[i][0] = r[i][0] * invdet;
     c[i][1] = r[i][1] * invdet;
     c[i][2] = r[i][2] * invdet;
     c[i][3] = r[i][3] * invdet;
   }
}


public void transpose_44 (matrix44 a, out matrix44 c)
{
  c = {{a[0][0], a[1][0], a[2][0], a[3][0]},
       {a[0][1], a[1][1], a[2][1], a[3][1]},
       {a[0][2], a[1][2], a[2][2], a[3][2]},
       {a[0][3], a[1][3], a[2][3], a[3][3]}};
}

// -----------------------------------------------------------------

// 7) quaternion
// [12 mults]

public void quaternion_to_m33 (quaternion q, out matrix33 m)
{
  float x2 = 2.0 * q.x;
  float y2 = 2.0 * q.y;
  float z2 = 2.0 * q.z;

  float xx = x2 * q.x;
  float xy = x2 * q.y;
  float xz = x2 * q.z;
  float xw = x2 * q.w;
  float yy = y2 * q.y;
  float yz = y2 * q.z;
  float yw = y2 * q.w;
  float zz = z2 * q.z;
  float zw = z2 * q.w;

#if 1
  const float eek = 0.99999994;

  // fix for angles multiple of 90°
  // 2.0 * sqrt(0.5) * sqrt(0.5) should yield 1.0
  if (xx == eek) xx = 1.0;
  if (xy == eek) xy = 1.0;
  if (xz == eek) xz = 1.0;
  if (xw == eek) xw = 1.0;
  if (yy == eek) yy = 1.0;
  if (yz == eek) yz = 1.0;
  if (yw == eek) yw = 1.0;
  if (zz == eek) zz = 1.0;
  if (zw == eek) zw = 1.0;

  if (xx == -eek) xx = -1.0;
  if (xy == -eek) xy = -1.0;
  if (xz == -eek) xz = -1.0;
  if (xw == -eek) xw = -1.0;
  if (yy == -eek) yy = -1.0;
  if (yz == -eek) yz = -1.0;
  if (yw == -eek) yw = -1.0;
  if (zz == -eek) zz = -1.0;
  if (zw == -eek) zw = -1.0;
#endif

  m =  {{1.0 - (yy + zz),  (xy + zw),        (xz - yw) },
        {(xy - zw),        1.0 - (xx + zz),  (yz + xw)},
        {(xz + yw),        (yz - xw),        1.0 - (xx + yy)}};
}


// [12 mults]
public void quaternion_to_m44 (quaternion q, out matrix44 m)
{
  float x2 = 2.0 * q.x;
  float y2 = 2.0 * q.y;
  float z2 = 2.0 * q.z;

  float xx = x2 * q.x;
  float xy = x2 * q.y;
  float xz = x2 * q.z;
  float xw = x2 * q.w;
  float yy = y2 * q.y;
  float yz = y2 * q.z;
  float yw = y2 * q.w;
  float zz = z2 * q.z;
  float zw = z2 * q.w;

#if 1
  const float eek = 0.99999994;

  // fix for angles multiple of 90°
  // 2.0 * sqrt(0.5) * sqrt(0.5) should yield 1.0
  if (xx == eek) xx = 1.0;
  if (xy == eek) xy = 1.0;
  if (xz == eek) xz = 1.0;
  if (xw == eek) xw = 1.0;
  if (yy == eek) yy = 1.0;
  if (yz == eek) yz = 1.0;
  if (yw == eek) yw = 1.0;
  if (zz == eek) zz = 1.0;
  if (zw == eek) zw = 1.0;

  if (xx == -eek) xx = -1.0;
  if (xy == -eek) xy = -1.0;
  if (xz == -eek) xz = -1.0;
  if (xw == -eek) xw = -1.0;
  if (yy == -eek) yy = -1.0;
  if (yz == -eek) yz = -1.0;
  if (yw == -eek) yw = -1.0;
  if (zz == -eek) zz = -1.0;
  if (zw == -eek) zw = -1.0;
#endif

  m =  {{1.0 - (yy + zz),  (xy + zw),        (xz - yw),         0.0},
        {(xy - zw),        1.0 - (xx + zz),  (yz + xw),         0.0},
        {(xz + yw),        (yz - xw),        1.0 - (xx + yy),   0.0},
        {0.0,              0.0,              0.0,               1.0}};
}

public void m33_to_quaternion (matrix33 m, out quaternion q)
{
  double t;

  if (m[2][2] < 0.0)
  {
    if (m[0][0] > m[1][1])
    {
      t = 1.0 + m[0][0] - m[1][1] - m[2][2];
      q = {x => (float)t,  y => m[0][1] + m[1][0], z => m[2][0] + m[0][2], w => m[1][2] - m[2][1]};
    }
    else
    {
      t = 1.0 - m[0][0] + m[1][1] - m[2][2];
      q = {x => m[0][1] + m[1][0], y => (float)t, z => m[1][2] + m[2][1], w => m[2][0] - m[0][2]};
    }
  }
  else
  {
    if (m[0][0] < -m[1][1])
    {
      t = 1.0 - m[0][0] - m[1][1] + m[2][2];
      q = {x => m[2][0] + m[0][2], y => m[1][2] + m[2][1], z => (float)t, w => m[0][1] - m[1][0]};
    }
    else
    {
      t = 1.0 + m[0][0] + m[1][1] + m[2][2];
      q = {x => m[1][2] - m[2][1], y => m[2][0] - m[0][2], z => m[0][1] - m[1][0], w => (float)t};
    }
  }

  t = 0.5 / sqrt(t);

  q.x = (float)(q.x * t);
  q.y = (float)(q.y * t);
  q.z = (float)(q.z * t);
  q.w = (float)(q.w * t);
}


// precision sin - gives exact results for multiples of 90°
float psin (double angle)
{
  double a = angle;

  while (a > PI)
    a -= 2.0*PI;

  while (a < -PI)
    a += 2.0*PI;

  if (a > PI/2.0)
    a = PI - a;
  else if (a < -PI/2.0)
    a = -PI - a;

  return (float)sin (a);
}

// precision cos - gives exact results for multiples of 90°
float pcos (double angle)
{
  return psin (angle + PI/2.0);
}

// [38 mult]
public void euler_to_quaternion (double direction, double height, double transversal, out quaternion q)
{
  quaternion qx = { psin(height*0.5), 0.0, 0.0, pcos(height*0.5) }; // height
  quaternion qy = { 0.0, psin(direction*0.5), 0.0, pcos(direction*0.5) };  // direction
  quaternion qz = { 0.0, 0.0, psin(transversal*0.5), pcos(transversal*0.5) }; // transversal

  // apply transv, height, dir
  mult_quaternion (qy, qx, out q);
  mult_quaternion (q, qz, out q);
}


void fix_sqr (ref float f)
{
  // for exact angles of 90°, sqrt(0.5) * sqrt(0.5) should yield 0.5
  const float eek = 0.49999997;

  // sqrt(0.5) * sqrt(0.5) should yield 0.5
  if (f ==  eek) f = 0.5;
  if (f == -eek) f = -0.5;
}

public void quaternion_to_euler (quaternion q, out double direction, out double height, out double transversal)
{
  float sqx = q.x*q.x;
  float sqy = q.y*q.y;
  float sqz = q.z*q.z;
  float xw = q.x*q.w;
  float zy = q.z*q.y;
  float test;

  fix_sqr (ref sqx);
  fix_sqr (ref sqy);
  fix_sqr (ref sqz);
  fix_sqr (ref xw);
  fix_sqr (ref zy);

  test = xw - zy;

  if (test > 0.4999)  // singularity at north pole
  {
		direction = 2.0 * atan2(-q.z,q.w);
    if (direction < -PI)
      direction += 2.0*PI;
    if (direction > PI)
      direction -= 2.0*PI;
		height      = PI/2.0;
		transversal = 0.0;
	}
	else if (test < -0.4999) // singularity at south pole
  {
		direction = -2.0 * atan2(-q.z,q.w);
    if (direction < -PI)
      direction += 2.0*PI;
    if (direction > PI)
      direction -= 2.0*PI;
		height      = -PI/2.0;
		transversal = 0.0;
	}
  else
  {
    float xy = q.x*q.y;
    float yw = q.y*q.w;
    float xz = q.x*q.z;
    float wz = q.w*q.z;

    fix_sqr (ref xy);
    fix_sqr (ref yw);
    fix_sqr (ref xz);
    fix_sqr (ref wz);

    direction   = atan2 (2.0*(yw + xz), 1.0 - 2.0*(sqy + sqx));
    height      = asin (2.0*test);
	  transversal = atan2 (2.0*(wz + xy), 1.0 - 2.0*(sqx + sqz));
  }
}

// [16 mults]
public void mult_quaternion (quaternion a, quaternion b, out quaternion c)
{
  c = {x => a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
       y => a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
       z => a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
       w => a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z};

  {
    // for exact angles of 90°, sqrt(0.5) * sqrt(0.5) should yield 0.5
    const float eek = 0.49999997;

    // sqrt(0.5) * sqrt(0.5) should yield 0.5
    if (c.x ==  eek) c.x = 0.5;
    if (c.x == -eek) c.x = -0.5;
    if (c.y ==  eek) c.y = 0.5;
    if (c.y == -eek) c.y = -0.5;
    if (c.z ==  eek) c.z = 0.5;
    if (c.z == -eek) c.z = -0.5;
    if (c.w ==  eek) c.w = 0.5;
    if (c.w == -eek) c.w = -0.5;
  }
}

public void negate_quaternion (quaternion a, out quaternion c)
{
  c = {x => -a.x, y => -a.y, z => -a.z, w => a.w};
}


// [32 mults]
public void rotate_vector_quaternion (vector3 a, quaternion q, out vector3 b)
{
  quaternion p_quat = {x => a[0], y => a[1], z => a[2], w => 0.0};
  quaternion q_conj = {x => -q.x, y => -q.y, z => -q.z, w => q.w};
  quaternion temp;
  
  mult_quaternion (q,    p_quat, out temp);
  mult_quaternion (temp, q_conj, out temp);
  
  b = {temp.x, temp.y, temp.z};
}


public float length_quaternion (quaternion q)
{
  return (float)sqrt (q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
}


public void normalize_quaternion (ref quaternion q)
{
  float lengthInv = 1.0 / (float)sqrt (q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);

  q.x *= lengthInv;
  q.y *= lengthInv;
  q.z *= lengthInv;
  q.w *= lengthInv;
}

// spherical interpolation between quaternions q1 and q2 with time t in [0 .. 1]
public void slerp (quaternion q1, quaternion q2, float t, out quaternion q)
{
  float dot, s1, s2;
  bool  flip;

  dot = q1.x*q2.x + q1.y*q2.y + q1.z*q2.z + q1.w*q2.w;

  if (dot < 0.0)  // q1 and q2 are more than 90 degrees apart
  {
    dot = -dot;   // invert to avoid spinning
    flip = true;
  }
  else
  {
    flip = false;
  }

  if (dot >= 0.95)  // if the angle is small, use linear interpolation
  {
    s1 = 1.0 - t;
    s2 = t;
  }
  else
  {
    float angle = (float)acos(dot);
    s1 = (float)sin(angle * (1.0-t));
    s2 = (float)sin(angle * t);
  }

  if (flip)
    s2 = -s2;

  q = {x => q1.x * s1 + q2.x * s2,
       y => q1.y * s1 + q2.y * s2,
       z => q1.z * s1 + q2.z * s2,
       w => q1.w * s1 + q2.w * s2};

  normalize_quaternion (ref q);
}


#if 0
  // code to add maybe later :

  quaternion(float real, const vector3f &i): s(real), v(i) {}

//! returns the logarithm of a quaternion = v*a where q = [cos(a),v*sin(a)]
	quaternion log() const
	{
		float a = (float)acos(s);
		float sina = (float)sin(a);
		quaternion ret;

		ret.s = 0;
		if (sina > 0)
		{
			ret.v.x = a*v.x/sina;
			ret.v.y = a*v.y/sina;
			ret.v.z = a*v.z/sina;
		} else {
			ret.v.x= ret.v.y= ret.v.z= 0;
		}
		return ret;
	}

	//! returns e^quaternion = exp(v*a) = [cos(a),vsin(a)]
	quaternion exp() const
	{
		float a = (float)v.length();
		float sina = (float)sin(a);
		float cosa = (float)cos(a);
		quaternion ret;

		ret.s = cosa;
		if (a > 0)
		{
			ret.v.x = sina * v.x / a;
			ret.v.y = sina * v.y / a;
			ret.v.z = sina * v.z / a;
		} else {
			ret.v.x = ret.v.y = ret.v.z = 0;
		}
		return ret;
	}

	//! Given 3 quaternions, qn-1,qn and qn+1, calculate a control point to be used in spline interpolation
	static quaternion spline(const quaternion &qnm1,const quaternion &qn,const quaternion &qnp1)
	{
		quaternion qni(qn.s, -qn.v);
		return qn * (( (qni*qnm1).log()+(qni*qnp1).log() )/-4).exp();
	}

	//! This version of slerp, used by squad, does not check for theta > 90.
	static quaternion slerpNoInvert(const quaternion &q1, const quaternion &q2, float t)
	{
		float dot = quaternion::dot(q1, q2);

		if (dot > -0.95f && dot < 0.95f)
		{
			float angle = acosf(dot);
			return (q1*sinf(angle*(1-t)) + q2*sinf(angle*t))/sinf(angle);
		} else  // if the angle is small, use linear interpolation
			return lerp(q1,q2,t);
	}

	//! spherical cubic interpolation
	static quaternion squad(const quaternion &q1,const quaternion &q2,const quaternion &a,const quaternion &b,float t)
	{
		quaternion c= slerpNoInvert(q1,q2,t),
			       d= slerpNoInvert(a,b,t);
		return slerpNoInvert(c,d,2*t*(1-t));
	}

	//! Shoemake-Bezier interpolation using De Castlejau algorithm
	static quaternion bezier(const quaternion &q1,const quaternion &q2,const quaternion &a,const quaternion &b,float t)
	{
		// level 1
		quaternion q11= slerpNoInvert(q1,a,t),
				q12= slerpNoInvert(a,b,t),
				q13= slerpNoInvert(b,q2,t);
		// level 2 and 3
		return slerpNoInvert(slerpNoInvert(q11,q12,t), slerpNoInvert(q12,q13,t), t);
	}

should not exp(q) also multiply by exp(Re(q)) ?
I beleive for
q = [s, v], were s= Re(q) and v = Im(q)
the formula is:
exp(q) = exp(s)*( cos(|v|) + sin(|v|)*v/|v|);
#endif


// assertion: f in range -sqrt(0.5) .. sqrt(0.5) (largest of 3 smallest), result in range 0 .. 1023
uint b10 (float f, bool invsign)
{
  uint sign = (uint)(((f < 0.0) ^ invsign) ? 512 : 0);

  // sqrt(0.5) = 0,70710678118654752440084436210485
  // 511   / sqrt(0.5) -> 722,66313037265156993766293807116
  // 511.5 / sqrt(0.5) -> 723,37023715383811746206378243326
  // 512   / sqrt(0.5) -> 724,07734393502466498646462679537

  uint value = (uint)(fabs(f) * 723.37023715383811746206378243326);
  if (value > 511)    // clamp value in case quaternon was not properly normalized
    value = 511;
  if (value == 0)
    sign = 0;
  return sign + value;
}

// extracts lower 10 bits and convert back into float
float f10 (uint u)
{
  float fsign = ((u & 512) != 0) ? -1.0 : 1.0;

  // value 511 should yield sqrt(0.5)
  // factor is : sqrt(0.5) / 511 = 0.0013837706089756311632110457184

  return fsign * (float)(u & 511) * 0.0013837706089756311632110457184;
}

public void pack_rotation_quaternion4 (quaternion q, out packed_quaternion4 pq)
{
  // assertion: x*x + y*y + z*z + w*w = 1, all are in range -1..1
  // - store only 3 smallest absolute values to avoid precision problems with later sqrt()
  // - if largest is negative, invert all.
  // -> 10 bits per float + 2 bits to select the largest (which is not stored)

  if (fabs(q.x) >= fabs(q.y) && fabs(q.x) >= fabs(q.z) && fabs(q.x) >= fabs(q.w))
  {
    // q.x is largest
    bool is = q.x < 0.0;  // inverse sign
    pq = 1 | (b10(q.y, is) << 2) | (b10(q.z, is) << 12) | (b10(q.w, is) << 22);
  }
  else if (fabs(q.y) >= fabs(q.z) && fabs(q.y) >= fabs(q.w))
  {
    // q.y is largest
    bool is = q.y < 0.0;  // inverse sign
    pq = 2 | (b10(q.x, is) << 2) | (b10(q.z, is) << 12) | (b10(q.w, is) << 22);
  }
  else if (fabs(q.z) >= fabs(q.w))
  {
    // q.z is largest
    bool is = q.z < 0.0;  // inverse sign
    pq = 3 | (b10(q.x, is) << 2) | (b10(q.y, is) << 12) | (b10(q.w, is) << 22);
  }
  else
  {
    // q.w is largest
    bool is = q.w < 0.0;  // inverse sign
    pq = (b10(q.x, is) << 2) | (b10(q.y, is) << 12) | (b10(q.z, is) << 22);
  }
}

void unpack_last4 (ref quaternion q, ref float q_c)
{
  float sum = (float)(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
  if (sum <= 1.0)
  {
    q_c = (float)sqrt (1.0 - sum);
    return;
  }

  // quaternion was not normalized
  q_c = 1.0;
  normalize_quaternion (ref q);
}

public void unpack_rotation_quaternion4 (packed_quaternion4 pq, out quaternion q)
{
  clear q;
  switch (pq & 3)
  {
    case 1:  // q.x was largest
       q.y = f10 (pq >> 2);
       q.z = f10 (pq >> 12);
       q.w = f10 (pq >> 22);
       unpack_last4 (ref q, ref q.x);
       break;

    case 2:  // q.y was largest
       q.x = f10 (pq >> 2);
       q.z = f10 (pq >> 12);
       q.w = f10 (pq >> 22);
       unpack_last4 (ref q, ref q.y);
       break;

    case 3:  // q.z was largest
       q.x = f10 (pq >> 2);
       q.y = f10 (pq >> 12);
       q.w = f10 (pq >> 22);
       unpack_last4 (ref q, ref q.z);
       break;

    default: // q.w was largest
       q.x = f10 (pq >> 2);
       q.y = f10 (pq >> 12);
       q.z = f10 (pq >> 22);
       unpack_last4 (ref q, ref q.w);
       break;
  }
}

// ===================================================================

// assertion: f in range -sqrt(0.5) .. sqrt(0.5) (largest of 3 smallest), result in range [0 .. 1<<20[
int b20 (float f, bool invsign)
{
  int sign = ((f < 0.0) ^ invsign) ? (1<<19) : 0;

  // sqrt(0.5) = 0,70710678118654752440084436210485
  // ((1<<19)-1)   / sqrt(0.5) -> 741453,78597590288385109097614973
  // ((1<<19)-0.5) / sqrt(0.5) -> 741454,49308268407039861537699409
  //  (1<<19)      / sqrt(0.5) -> 741455,20018946525694613977783845

  int value = (int)(fabs(f) * 741454.49308268407039861537699409);
  if (value >= (1<<19))    // clamp value in case quaternon was not properly normalized
    value = (1<<19)-1;
  if (value == 0)
    sign = 0;
  return sign + value;
}

// extracts lower 20 bits and convert back into float
float f20 (int n)
{
  int mantissa = n & ((1<<19)-1);

  // value 524287 should yield sqrt(0.5)
  // factor is : sqrt(0.5) / 524287 = 0.0000013487017247929998729719492608149

  float fvalue = (float)mantissa * 0.0000013487017247929998729719492608149;
  return mantissa != 0 && ((n & (1<<19)) != 0) ? -fvalue : fvalue;
}

// note: 2 highest bits of pq are unused
public void pack_rotation_quaternion8 (quaternion q, out packed_quaternion8 pq)
{
  // assertion: x*x + y*y + z*z + w*w = 1, all are in range -1..1
  // - store only 3 smallest absolute values to avoid precision problems with later sqrt()
  // - if largest is negative, invert all.
  // -> 20 bits per float + 2 bits to select the largest (which is not stored)

  if (fabs(q.x) >= fabs(q.y) && fabs(q.x) >= fabs(q.z) && fabs(q.x) >= fabs(q.w))
  {
    // q.x is largest
    bool is = q.x < 0.0;  // inverse sign
    pq = 1 | (b20(q.y, is) << 2) | (b20(q.z, is) << 22L) | (b20(q.w, is) << 42L);
  }
  else if (fabs(q.y) >= fabs(q.z) && fabs(q.y) >= fabs(q.w))
  {
    // q.y is largest
    bool is = q.y < 0.0;  // inverse sign
    pq = 2 | (b20(q.x, is) << 2) | (b20(q.z, is) << 22L) | (b20(q.w, is) << 42L);
  }
  else if (fabs(q.z) >= fabs(q.w))
  {
    // q.z is largest
    bool is = q.z < 0.0;  // inverse sign
    pq = 3 | (b20(q.x, is) << 2) | (b20(q.y, is) << 22L) | (b20(q.w, is) << 42L);
  }
  else
  {
    // q.w is largest
    bool is = q.w < 0.0;  // inverse sign
    pq = (b20(q.x, is) << 2) | (b20(q.y, is) << 22L) | (b20(q.z, is) << 42L);
  }
}

void unpack_last8 (ref quaternion q, ref float q_c)
{
  float sum = (float)(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
  if (sum <= 1.0)
  {
    q_c = (float)sqrt (1.0 - sum);
    return;
  }

  // quaternion was not normalized
  q_c = 1.0;
  normalize_quaternion (ref q);
}

// note: 2 highest bits of pq are unused
public void unpack_rotation_quaternion8 (packed_quaternion8 pq, out quaternion q)
{
  clear q;
  switch (pq & 3)
  {
    case 1:  // q.x was largest
       q.y = f20 ((int)(pq >> 2));
       q.z = f20 ((int)(pq >> 22));
       q.w = f20 ((int)(pq >> 42));
       unpack_last8 (ref q, ref q.x);
       break;

    case 2:  // q.y was largest
       q.x = f20 ((int)(pq >> 2));
       q.z = f20 ((int)(pq >> 22));
       q.w = f20 ((int)(pq >> 42));
       unpack_last8 (ref q, ref q.y);
       break;

    case 3:  // q.z was largest
       q.x = f20 ((int)(pq >> 2));
       q.y = f20 ((int)(pq >> 22));
       q.w = f20 ((int)(pq >> 42));
       unpack_last8 (ref q, ref q.z);
       break;

    default: // q.w was largest
       q.x = f20 ((int)(pq >> 2));
       q.y = f20 ((int)(pq >> 22));
       q.z = f20 ((int)(pq >> 42));
       unpack_last8 (ref q, ref q.w);
       break;
  }
}

// ===================================================================
// Jacobi Transformations of a Symmetric Matrix
// ===================================================================

// sort the eigenvalues d into descending order
// and rearranges the columns of v accordingly.

void eigsrt (ref vector3 d, ref matrix33 v)
{
  const int n = d'length;

  int k, i, j;

  for (i=0; i<n-1; i++)
  {
    float p = d[i];

    k = i;

    for (j=i; j<n; j++)
    {
      if (d[j] >= p)
      {
        k = j;
        p = d[j];
      }
    }

    if (k != i)
    {
      d[k] = d[i];
      d[i] = p;

      // optional part, done if v available
      for (j=0; j<n; j++)
      {
        p = v[j][i];
        v[j][i] = v[j][k];
        v[j][k] = p;
      }
    }
  }
}

// ===================================================================

void rot (ref matrix33 a, float s, float tau, int i, int j, int k, int l)
{
  float g = a[i][j];
  float h = a[k][l];
  a[i][j] = g - s*(h+g*tau);
  a[k][l] = h + s*(g-h*tau);
}

// ===================================================================

public void jacobi (    matrix33 m,
                    out matrix33 v,      // normalized eigenvectors
                    out vector3  d)      // eigenvalues
{
  const int n = m'length;

  matrix33 a = m;
  int      i, j, ip, iq, nrot;
  float    tresh, theta, tau, t, sn, s, h, g, c;
  vector3  b, z;

  nrot = 0;

  // initialize v to the identity matrix
  v = M33_ONE;

  clear b, d, z;  // z will accumulate terms

  // initialize b and d to the diagonal of a
  for (ip=0; ip<n; ip++)
  {
    t = a[ip][ip];
    d[ip] = t;
    b[ip] = t;
  }


  for (i=1; i<=50; i++)
  {
    sn = 0.0;

    // sum magnitude of off-diagonal elements
    for (ip=0; ip<n-1; ip++)
      for (iq=ip+1; iq<n; iq++)
        sn += (float)fabs(a[ip][iq]);

    // normal return, which relies on quadratic convergence to machine underflow
    if (sn == 0.0)
    {
      eigsrt (ref d, ref v);
      return;
    }

    // on the first 3 sweeps ...
    if (i < 4)
      tresh = 0.2 * sn / (float)(n * n);
    else
      tresh = 0.0;

    for (ip=0; ip<n-1; ip++)
    {
      for (iq=ip+1; iq<n; iq++)
      {
        g = 100.0 * (float)fabs (a[ip][iq]);

        // after 4 sweeps, skip the rotation if the off-diagonal element is small

//        if (i > 4 && g <= EPS * fabs(d[ip]) && g <= EPS * fabs(d[iq]))
        if (i > 4 && fabs(d[ip])+g == fabs(d[ip]) && fabs(d[iq])+g == fabs(d[iq]))

          a[ip][iq] = 0.0;
        else if (fabs (a[ip][iq]) > tresh)
        {
          h = d[iq] - d[ip];

//          if (g <= EPS * fabs (h))
          if (fabs(h)+g == fabs(h))

            t = (a[ip][iq]) / h;      // t = 1 / 2*rho
          else
          {
            theta = 0.5 * h / a[ip][iq];
            t = 1.0 / (float)(fabs (theta) + sqrt (1.0 + theta*theta));
            if (theta < 0.0)
              t = -t;
          }

          c = 1.0 / (float)sqrt (1.0 + t*t);
          s = t * c;
          tau = s / (1.0 + c);
          h = t * a[ip][iq];
          z[ip] -= h;
          z[iq] += h;
          d[ip] -= h;
          d[iq] += h;
          a[ip][iq] = 0.0;

          // case of rotations 0 <= j < p.
          for (j=0; j<ip; j++)
            rot (ref a, s, tau, j, ip, j, iq);

          // case of rotations p < j < q.
          for (j=ip+1; j<iq; j++)
            rot (ref a, s, tau, ip, j, j, iq);

          // case of rotations q < j < n.
          for (j=iq+1; j<n; j++)
            rot (ref a, s, tau, ip, j, iq, j);

          for (j=0; j<n; j++)
            rot (ref v, s, tau, j, ip, j, iq);

          nrot++;
        }
      }
    }

    // update d with sum of ta and reinitialize z
    add_3 (b, z, out b);
    d = b;
    clear z;
  }

  abort;  // too many iterations : no convergence
}

// ===================================================================

// returns true if both lines intersect, false if they are parallel.

public
bool lines_intersect2 (    LINE2   line1,
                           LINE2   line2,
                       out vector2 pi)   // intersection
{
  float dx1 = (line1.p1[0] - line1.p2[0]);
  float dy1 = (line1.p1[1] - line1.p2[1]);

  float dx2 = (line2.p1[0] - line2.p2[0]);
  float dy2 = (line2.p1[1] - line2.p2[1]);

  float den = dx1 * dy2 - dy1 * dx2;

  float f1, f2, invden;

  if (fabs(den) > 0.00001)
  {
    f1 = (line1.p1[0] * line1.p2[1] - line1.p1[1] * line1.p2[0]);
    f2 = (line2.p1[0] * line2.p2[1] - line2.p1[1] * line2.p2[0]);

    invden = 1.0 / den;

    pi = {(f1 * dx2 - dx1 * f2) * invden,
          (f1 * dy2 - dy1 * f2) * invden};
    return true;
  }

  clear pi;
  return false;
}

//--------------------------------------------------------------------

float xy_den (vector3 p1, vector3 p2, vector3 p3, vector3 p4)
{
  const int A = 0;
  const int B = 1;
  return (p1[A] - p2[A]) * (p3[B] - p4[B]) - (p1[B] - p2[B]) * (p3[A] - p4[A]);
}

//--------------------------------------------------------------------

float xz_den (vector3 p1, vector3 p2, vector3 p3, vector3 p4)
{
  const int A = 0;
  const int B = 2;
  return (p1[A] - p2[A]) * (p3[B] - p4[B]) - (p1[B] - p2[B]) * (p3[A] - p4[A]);
}

//--------------------------------------------------------------------

float yz_den (vector3 p1, vector3 p2, vector3 p3, vector3 p4)
{
  const int A = 1;
  const int B = 2;
  return (p1[A] - p2[A]) * (p3[B] - p4[B]) - (p1[B] - p2[B]) * (p3[A] - p4[A]);
}

//--------------------------------------------------------------------

// returns true if both lines intersect, false if they are parallel.

public
bool lines_intersect3 (    LINE3   line1,
                           LINE3   line2,
                       out vector3 pi)   // intersection
{
  float d_xy = (float)fabs(xy_den (line1.p1, line1.p2, line2.p1, line2.p2));
  float d_xz = (float)fabs(xz_den (line1.p1, line1.p2, line2.p1, line2.p2));
  float d_yz = (float)fabs(yz_den (line1.p1, line1.p2, line2.p1, line2.p2));

  int a, b, r, s;

  if (d_xy < d_xz)
  {
    if (d_xz < d_yz)
    {
      a = 1;
      b = 2;
      r = 0;
    }
    else
    {
      a = 0;
      b = 2;
      r = 1;
    }
  }
  else
  {
    if (d_xy < d_yz)
    {
      a = 1;
      b = 2;
      r = 0;
    }
    else
    {
      a = 0;
      b = 1;
      r = 2;
    }
  }

  {
    vector2 c1, c2, c3, c4, ci;

    c1 = {line1.p1[a], line1.p1[b]};
    c2 = {line1.p2[a], line1.p2[b]};
    c3 = {line2.p1[a], line2.p1[b]};
    c4 = {line2.p2[a], line2.p2[b]};

    if (!lines_intersect2 ({c1, c2}, {c3, c4}, out ci))
    {
      clear pi;
      return false;
    }

    if (fabs (line1.p1[a]-line1.p2[a]) > fabs (line1.p1[b]-line1.p2[b]))  // 'a' component is larger
      s = a;
    else
      s = b;

    clear pi;
    pi[a] = ci[0];
    pi[b] = ci[1];
    pi[r] = line1.p1[r] + (line1.p2[r] - line1.p1[r]) * (pi[s] - line1.p1[s]) / (line1.p2[s] - line1.p1[s]);
  }

  return true;
}

//--------------------------------------------------------------------

public void make_plane3 (vector3 p[3], out PLANE3 plane)
{
  vector3 u, v, vec;

  sub_3 (p[1], p[0], out u);
  sub_3 (p[2], p[0], out v);

  cross_product_3 (u, v, out vec);

  normalize_3 (ref vec);
  
  plane = {vec[0], vec[1], vec[2], - dot_product_3 (vec, p[0])};
}

//--------------------------------------------------------------------

public float signed_distance_to_plane (PLANE3 plane, vector3 point)
{
  return plane[0]*point[0] + plane[1]*point[1] + plane[2]*point[2] + plane[3];
}

//--------------------------------------------------------------------

public bool line_plane_intersect (PLANE3 plane, LINE3 line, out vector3 intersection)
{
  float den = plane[0] * (line.p1[0] - line.p2[0])
            + plane[1] * (line.p1[1] - line.p2[1])
            + plane[2] * (line.p1[2] - line.p2[2]);

  if (fabs(den) > 0.00001)   // not parallel
  {
    float invden = 1.0 / den;

    float f1 = line.p2[0]*line.p1[1] - line.p1[0]*line.p2[1];
    float f2 = line.p1[0]*line.p2[2] - line.p2[0]*line.p1[2];
    float f3 = line.p2[1]*line.p1[2] - line.p1[1]*line.p2[2];

    intersection = {(plane[1] * f1 - plane[2] * f2 + plane[3] * (line.p2[0] - line.p1[0])) * invden,
                    (plane[2] * f3 - plane[0] * f1 + plane[3] * (line.p2[1] - line.p1[1])) * invden,
                    (plane[0] * f2 - plane[1] * f3 + plane[3] * (line.p2[2] - line.p1[2])) * invden};
    return true;
  }

  clear intersection;
  return false;
}

//--------------------------------------------------------------------
