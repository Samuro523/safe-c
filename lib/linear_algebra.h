
// linear_algebra.c

// -----------------------------------------------------------------

// 1) vector2

typedef float[2] vector2;

void add_2 (vector2 a, vector2 b, out vector2 c);
void sub_2 (vector2 a, vector2 b, out vector2 c);

void mult_2_1 (vector2 a, float b, out vector2 c);

float length_2 (vector2 a);
void normalize_2 (ref vector2 a);
void inverse_2 (vector2 a, out vector2 c);

float dot_product_2 (vector2 a, vector2 b);

// -----------------------------------------------------------------

// 2) vector3

typedef float[3] vector3;

void add_3 (vector3 a, vector3 b, out vector3 c);
void sub_3 (vector3 a, vector3 b, out vector3 c);

void mult_3_1 (vector3 a, float b, out vector3 c);   // scaling

float length_3 (vector3 a);
void normalize_3 (ref vector3 a);
void inverse_3 (vector3 a, out vector3 c);

float dot_product_3 (vector3 a, vector3 b);
void cross_product_3 (vector3 a, vector3 b, out vector3 c);

// -----------------------------------------------------------------

// 3) vector4

typedef float[4] vector4;

void add_4 (vector4 a, vector4 b, out vector4 c);
void sub_4 (vector4 a, vector4 b, out vector4 c);

void mult_4_1 (vector4 a, float b, out vector4 c);

float length_4 (vector4 a);
void normalize_4 (ref vector4 a);
void inverse_4 (vector4 a, out vector4 c);

float dot_product_4 (vector4 a, vector4 b);

// -----------------------------------------------------------------

// 4) matrix22

typedef float[2][2] matrix22;

const matrix22 M22_ONE = {{1.0, 0.0},
                          {0.0, 1.0}};

void add_22     (matrix22 a, matrix22 b, out matrix22 c);
void sub_22     (matrix22 a, matrix22 b, out matrix22 c);

void mult_22_1  (matrix22 a, float    b, out matrix22 c);
void mult_21_12 (vector2  a, vector2  b, out matrix22 c);
void mult_22_21 (matrix22 a, vector2  b, out vector2  c);
void mult_22_22 (matrix22 a, matrix22 b, out matrix22 c);
void mult_12_22 (vector2  a, matrix22 b, out vector2  c);

float determinant_22 (matrix22 a);
void inverse_22      (matrix22 a, out matrix22 c);
void transpose_22    (matrix22 a, out matrix22 c);

// -----------------------------------------------------------------

// 5) matrix33

typedef float[3][3] matrix33;

const matrix33 M33_ONE = {{1.0, 0.0, 0.0},
                          {0.0, 1.0, 0.0},
                          {0.0, 0.0, 1.0}};

void add_33     (matrix33 a, matrix33 b, out matrix33 c);
void sub_33     (matrix33 a, matrix33 b, out matrix33 c);

void mult_33_1  (matrix33 a, float    b, out matrix33 c);  // scaling (multiply all elements)
void mult_31_13 (vector3  a, vector3  b, out matrix33 c);
void mult_33_31 (matrix33 a, vector3  b, out vector3  c);
void mult_33_33 (matrix33 a, matrix33 b, out matrix33 c);  // applies the transformations in this order : a, b
void mult_13_33 (vector3  a, matrix33 b, out vector3  c);

// multiply matrix33 by triangular matrix33 expressed as vector3
void mult_33_tri3  (matrix33 a, vector3 b, out matrix33 c);

float determinant_33 (matrix33 a);
void inverse_33      (matrix33 a, out matrix33 c);
void transpose_33    (matrix33 a, out matrix33 c);

// returns matrix {{0.0, -a[2], a[1]}, {a[2], 0.0, -a[0]}, {-a[1], a[0], 0.0}}
void star_3 (vector3 a, out matrix33 c);

void jacobi (    matrix33 m,
             out matrix33 v,   // rotation matrix (eigenvectors)
             out vector3  d);  // triangular result matrix (eigenvalues)

// -----------------------------------------------------------------

// 6) matrix44

typedef float[4][4] matrix44;

const matrix44 M44_ONE = {{1.0, 0.0, 0.0, 0.0},
                          {0.0, 1.0, 0.0, 0.0},
                          {0.0, 0.0, 1.0, 0.0},
                          {0.0, 0.0, 0.0, 1.0}};

void mult_44_44 (matrix44 a, matrix44 b, out matrix44 c);

void mult_44_41 (matrix44 a, vector4 b, out vector4 c);
void mult_14_44 (vector4 a, matrix44 b, out vector4 c);

void inverse_44   (matrix44 a, out matrix44 c);
void transpose_44 (matrix44 a, out matrix44 c);

// -----------------------------------------------------------------

// 7) quaternion

packed struct quaternion { float x, y, z,  w; }

const quaternion NO_ROTATION = {x=>0.0, y=>0.0, z=>0.0,  w=>1.0};

// conjugate
void negate_quaternion (quaternion a, out quaternion c);

float length_quaternion (quaternion q);
void normalize_quaternion (ref quaternion q);

// applies the transformations in this order : b, a
// [16 mults]
void mult_quaternion (quaternion a, quaternion b, out quaternion c);

// [32 mults]
void rotate_vector_quaternion (vector3 a, quaternion q, out vector3 b);

// spherical interpolation between quaternions q1 and q2 with time t in [0 .. 1]
void slerp (quaternion q1, quaternion q2, float t, out quaternion q);

// note: transpose matrix if you're using open_gl
// [12 mults]
void quaternion_to_m33 (quaternion q, out matrix33 m);
void quaternion_to_m44 (quaternion q, out matrix44 m);

// [4 mults, 1 div, 1 sqrroot]
void m33_to_quaternion (matrix33 m, out quaternion q);

// applies the transformations in this order : transversal, height, direction.
// [38 mult]
void euler_to_quaternion (double direction, double height, double transversal, out quaternion q);

// [3 atan2]
void quaternion_to_euler (quaternion q, out double direction, out double height, out double transversal);

typedef uint packed_quaternion4;  // pack each x,y,z,w with 10 bits
void pack_rotation_quaternion4 (quaternion q, out packed_quaternion4 pq);
void unpack_rotation_quaternion4 (packed_quaternion4 pq, out quaternion q);

typedef int8 packed_quaternion8;  // pack each x,y,z,w with 20 bits
// note: 2 highest bits of pq are unused, can be used by application to store flags.
void pack_rotation_quaternion8 (quaternion q, out packed_quaternion8 pq);
void unpack_rotation_quaternion8 (packed_quaternion8 pq, out quaternion q);

// -----------------------------------------------------------------
// 8) geometry
//----------------------------------------------------------------------

struct LINE2
{
  vector2 p1, p2;
}

// returns true if both 2D lines intersect, false if they are parallel.
bool lines_intersect2 (    LINE2   line1,
                           LINE2   line2,
                       out vector2 pi);  // intersection

//----------------------------------------------------------------------

typedef vector4 PLANE3;  // defined by x*v[0] + y*v[1] + z*v[2] + v[3] = 0

struct LINE3
{
  vector3 p1, p2;
}

//----------------------------------------------------------------------

void make_plane3 (vector3 p[3], out PLANE3 plane);

//----------------------------------------------------------------------

// returns positive or negative distance, depends on which "side" the point is.
float signed_distance_to_plane (PLANE3 plane, vector3 point);

//----------------------------------------------------------------------

// returns true if both 3D lines intersect, false if they are parallel.
// if both lines do not touch closely, we compute an intersection on line1.

bool lines_intersect3 (    LINE3   line1,
                           LINE3   line2,
                       out vector3 pi);  // intersection
                
//----------------------------------------------------------------------

// returns false if there is no intersection.
bool line_plane_intersect (PLANE3 plane, LINE3 line, out vector3 intersection);

//----------------------------------------------------------------------
