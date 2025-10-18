
// draw3d.h : draw in 3D (DirectX 11 for Windows, OpenGL ES 3.0 for Android)

use image, linear_algebra;

#if ANDROID
  use android/android;
#endif

//--------------------------------------------------------------------------

typedef void FUNC ();

struct CALLBACK_FUNCTIONS_3D
{
  // these functions are called by the rendering thread :
  FUNC init_objects;    // called once when initializing (create textures, ..)
  FUNC prepare_frame;   // called at each frame (for set_camera, move physics objects, ..)
  FUNC render_shadow;   // called at each frame (for rendering shadows)
  FUNC render_3D;       // called at each frame (for rendering 3D objects)
  FUNC render_2D;       // called at each frame (for rendering 2D objects in range -0.5 .. +0.5)
  FUNC cleanup_objects; // called once before stopping rendering thread (free textures, ..)
}

#if WINDOWS
  // starts a thread that renders 3D on main window
  // debug=true requires d3d11_3SDKLayers.dll
  // use_warp=true uses Windows Warp software layer for rendering instead of hardware
  // returns 0 if OK, -1 if error
  int start_DirectX_rendering_thread
     (CALLBACK_FUNCTIONS_3D func,
      bool debug = false,
      bool use_warp = false,
      int  antialiasing = 1,    // 1, 2, 4, 8, 16
      bool old_discard_mode = false);  // use old DISCARD instead of more efficient FLIP_DISCARD

  // stops the render thread
  void stop_DirectX_rendering_thread ();

  // to be called when a screen resize occurs.
  // provide width & height of client area (excluding window border)
  void signal_DirectX_screen_was_resized (int width, int height);
#endif

#if ANDROID
  // register a list of functions for 3D
  void setup_3D (CALLBACK_FUNCTIONS_3D func);

#begin unsafe
  // to be called in main android app for all events
  void event_3D (android_app *app, int event_type);
#end unsafe

  typedef void FUNC_RENDER_ALL_DIALOGS ();
  FUNC_RENDER_ALL_DIALOGS g_func_render_all_dialogs;

  void get_desktop_resolution (out int x_size, out int y_size);

  typedef void FUNC_DIALOGS_NEED_REPAINT ();
  FUNC_DIALOGS_NEED_REPAINT g_dialogs_need_repaint;

#endif


volatile uint g_frame_tick;   // tick when starting rendering this frame, in msec. (READ-ONLY)
void limit_fps (bool on);     // default: true. (set to false if we don't want to wait for the monitor vertical sync)

// returns a table with samples, for example 1, 2, 4, 8, 16. count indicates how many samples in table.
void best_antialiasing_available (out int[5] samples, out int count);
int current_antialiasing_setting ();

//===============================================================================================================
// the following functions must be called within the above callback functions :
//===============================================================================================================

// to be called in prepare_frame :

// set nearest and farthest z-points to draw
void set_near_far_Z (float near_z = 0.0625,   // default: 6 cm
                     float far_z  = 1024.0);  // default: 1024 m

// note: use linear_algebra.euler_to_quaternion() to compute view.
// world_position is used for set_perlin_noise() and .disable_ambiant_sun_underground
void set_camera (vector3 eye, quaternion view, vector3 world_position = {0.0, 0.0, 0.0});

void set_background_color (vector3 color);      // usually black or blue sky (linear color)

void set_shadows_number_of_levels (int level);     // 0 = shadows off, 1..5 = number of shadow map levels
void set_shadow_matrix (matrix44 m);

//===============================================================================================================

// to be called in render_3D / render_2D :

// control depth buffer.
void set_depth_buffer (bool testing, bool writing = true);    // default: on/on for render_3D, off/off for render_2D.
void clear_depth_buffer ();

// control stencil buffer.
// initial stencil value is 0.
// enable testing will render a pixel only if stencil value is 0.
// enable writing will change stencil value to value different than 0.
void set_stencil_buffer (bool testing, bool writing = false);    // default: off/off for render_3D, off/off for render_2D.
void clear_stencil_buffer ();

// control blending.
enum BLENDING {BLENDING_TRANSLUCID, BLENDING_ADD, BLENDING_MIN, BLENDING_MAX, BLENDING_OFF};
void set_blending (BLENDING blending);    // default: BLENDING_TRANSLUCID.

//--------------------------------------------------------------------------

// -- scene settings --

// ambiant light RGB color, usually a light grey.  linear color.
// note: after this call, call set_object_colors() with ambiant_enabled=true
void set_ambiant_light (vector3 color);

// directional sunlight RGB color, usually white, yellow, red, .. shining down.  linear color.
// note: after this call, call set_object_colors() with sunlight_enabled=true
void set_sun_light (vector3 color, vector3 direction);


packed struct LIGHT   // 48 bytes - for point light and spot light
{
  vector3 color;        // RGB light color. linear color.
  float   range;        // distance on which it has effect
  vector3 position;     // world position of light
  float   attenuation;  // attenuation exponent : 0(no attenuation with distance) .. 1(linear attenutation)
  vector3 direction;    // for spotlight : {0.0, 1.0, 0.0} means spot shines upwards
  float   cone;         // =0 : point light, >0 : spot light  (0.001 = large cone, 100.0 = small cone)
}

const int MAX_LIGHTS = 128;    // max lights
  
// set point and spot lights
void set_lights (LIGHT[] lights);

// important: when enabling fog, pass also the fog color to set_background_color().
void set_fog (
   float   start_z,   // no fog when start_z >= end_z
   float   end_z,
   vector3 color = {0.62,0.62,0.62},   // fog color, ex: light grey. linear color.
   float   attenuation = 1.0);         // 0.1 (nearby fog) .., 1(linear), .., 10(distant fog)

//--------------------------------------------------------------------------

// -- object settings --

// set object position/rotation
void set_world_transform (matrix44 m);


struct OBJECT_COLORS
{
  // material RGBA color and transparency of the object. It is multiplied by the texture color. Default is white and fully visible.
  // byte A is the transparency component (0 = pixel not visible, 255 = pixel fully visible)
  vector4  MaterialColor;    // should be {1.0, 1.0, 1.0, 1.0} linear color

  // extra RGB light
  vector3  EmissiveColor;    // should be {0.0, 0.0, 0.0} linear color

  float    metallic;   // 0 .. 1
  float    smooth;     // 0 .. 1  (avoid 1)  (1.0 - roughness)
  float    translucid; // 0 .. 1

  bool     ambiant_enabled;   // use ambiant color
  bool     sunlight_enabled;  // use sunlight color
  bool     ambiant_downwards; // true = ambiant 15% less intense when normal points from side, 30% from below
  bool     disable_ambiant_sun_underground;  // disable ambiant and sun light for pixels having z < 0.0

  float    parallax_factor;   // 0 = none, ex: -0.05
}

// to be called just before drawing triangles
void set_object_colors (OBJECT_COLORS colors);

//--------------------------------------------------------------------------

packed struct OBJECT_UV_TRANSFORM  // 24 bytes
{
  float fact_u_min_1;
  float fact_v_min_1;
  float fact_u;
  float fact_v;
  float ofs_u;
  float ofs_v;
}

// u,v are transformed as follows :
// u' = u*(fact_u_min_1 + 1) + v*fact_u + ofs_u;
// v' = v*(fact_v_min_1 + 1) + u*fact_v + ofs_v;

void set_uv_transform (OBJECT_UV_TRANSFORM uv_transform);

//--------------------------------------------------------------------------

void set_perlin_noise (bool    enable,
                       float   perlin_amplitude = 1.0,
                       float   perlin_speed = 1.0);

//--------------------------------------------------------------------------

#if WINDOWS
  #if MEM32
    typedef int4 VERTEX_ID;
    typedef int4 INDEX_ID;
    typedef int4 TEXTURE_ID;
    typedef int4 RIGGED_VERTEX_ID;
    typedef int4 BONE_BUFFER_ID;
  #else  // 64-bit
    typedef int8 VERTEX_ID;
    typedef int8 INDEX_ID;
    typedef int8 TEXTURE_ID;
    typedef int8 RIGGED_VERTEX_ID;
    typedef int8 BONE_BUFFER_ID;
  #endif
#endif

#if ANDROID
  typedef int8 VERTEX_ID;     // int8 because we store two handles inside
  typedef int8 TEXTURE_ID;
  typedef int8 INDEX_ID;
  typedef int8 RIGGED_VERTEX_ID;
  typedef int8 BONE_BUFFER_ID;
#endif

//--------------------------------------------------------------------------

packed struct VERTEX   // 48 bytes
{
  vector3 c;  // coordinate (x,y,z)
  vector3 n;  // normal vector (x,y,z)
  vector3 u;  // tangent vector
  uint    o;  // diffuse color and transparency - set to 0xFFFFFFFF (linear color)
  vector2 t;  // texture (u,v)
}

VERTEX_ID create_vertex_buffer (VERTEX[] v, bool allow_update);

enum BUFFER_UPDATE_FLAG {OVERWRITE_ALL, OVERWRITE_PART, APPEND_DATA};
void update_vertex_buffer (VERTEX_ID v, int offset, VERTEX[] vb, BUFFER_UPDATE_FLAG flag = OVERWRITE_ALL);

void free_vertex_buffer (VERTEX_ID v);

//--------------------------------------------------------------------------

INDEX_ID create_small_index_buffer (uint2[] i, bool allow_update);
INDEX_ID create_large_index_buffer (uint4[] i, bool allow_update);

void update_small_index_buffer (INDEX_ID id, uint offset, uint2[] ib, BUFFER_UPDATE_FLAG flag = OVERWRITE_ALL);
void update_large_index_buffer (INDEX_ID id, uint offset, uint4[] ib, BUFFER_UPDATE_FLAG flag = OVERWRITE_ALL);

void free_index_buffer (INDEX_ID i);

//--------------------------------------------------------------------------

// use textures as small as possible.
// 'use_mipmapping' should be true for 3D drawing to blur aliasing artifacts, and false for 2D drawing.
TEXTURE_ID create_texture (IMAGE_INFO img,
                           bool       allow_update = false,
                           bool       use_mipmapping = true,
                           bool       rgb_to_linear_conversion = true);  // true = GPU will converts RGB to linear color, false = no conversion

void update_texture (TEXTURE_ID id, IMAGE_INFO img);
void update_texture_part (TEXTURE_ID id, uint[2] offset, IMAGE_INFO img);

void free_texture (TEXTURE_ID t);

//--------------------------------------------------------------------------

// IMPORTANT: make as few draw calls as possible per frame, try to use large vertex buffers instead.

// uses 3 indexes per triangle
void draw_indexed_triangles (VERTEX_ID  v_id,
                             int        v_offset,
                             INDEX_ID   i_id,
                             int        i_offset,
                             int        i_count,
                             TEXTURE_ID t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
                             uint       object_id = 0xFFFFFF);  // user value 0 .. 2^24-2

// uses 3 vertexes per triangle
void draw_triangles (VERTEX_ID  v_id,
                     int        v_offset,
                     int        v_count,
                     TEXTURE_ID t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
                     uint       object_id = 0xFFFFFF);  // user value 0 .. 2^24-2

//--------------------------------------------------------------------------
// avatar skinning
//--------------------------------------------------------------------------

packed struct RIGGED_VERTEX   // 64 bytes
{
  vector3  c;      // coordinate (x,y,z)
  vector3  n;      // normal vector (x,y,z)
  vector3  u;      // tangent vector
  vector2  t;      // texture (u,v)
  byte[4]  bone;   // bone index
  float[4] weight; // 0.0 = no bone left
}

RIGGED_VERTEX_ID create_rigged_vertex_buffer (RIGGED_VERTEX[] v);
void free_rigged_vertex_buffer (RIGGED_VERTEX_ID v);

// bone transformation matrices
BONE_BUFFER_ID create_bone_buffer (int nb_bones);
void write_bone_buffer (BONE_BUFFER_ID id, matrix44[] m);
void free_bone_buffer (BONE_BUFFER_ID id);

void draw_indexed_rigged_triangles
     (RIGGED_VERTEX_ID  v_id,
      int               v_offset,
      INDEX_ID          i_id,
      int               i_offset,
      int               i_count,
      BONE_BUFFER_ID    b_id,
      TEXTURE_ID        t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
      uint              object_id = 0xFFFFFF);

void draw_rigged_triangles
   (RIGGED_VERTEX_ID  v_id,     // rigged vertex buffer id
    int               v_offset,
    int               v_count,
    BONE_BUFFER_ID    b_id,     // bone buffer id
    TEXTURE_ID        t_id[3],  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
    uint              object_id = 0xFFFFFF);  // user value 0 .. 2^24-2

//--------------------------------------------------------------------------

// a world object of length L and depth Z will have a length of (L/Z*g_world_to_screen_factor) pixels on the screen, L goes for X or Y.
// g_world_to_screen_factor is adapted each time the screen height changes.
// to be used in prepare_frame() or render_3D().

float g_world_to_screen_factor;  // (READ-ONLY)

//--------------------------------------------------------------------------
// these functions should be called after a Windows mouse click, to determine the clicked triangle.
// they must be called within the render thread !
//--------------------------------------------------------------------------

// compute object_id at screen position x,y
// returns 0xFFFFFF if there is no object.
uint get_picking_object_id (int x, int y);

//--------------------------------------------------------------------------

struct PICKING_RAY
{
  vector3 vPickNear, vPickFar, vPickDir;
  float vPickLen;
}

// picking ray in world space
void compute_picking_ray (int             mouse_x,
                          int             mouse_y,
                          out PICKING_RAY picking_ray);

// picking ray in camera relative space
void compute_picking_ray_for_hud (int             mouse_x,
                                  int             mouse_y,
                                  out PICKING_RAY picking_ray);

//--------------------------------------------------------------------------

// usually used as preliminary test to see if ray is close to object

bool ray_intersects_sphere (vector3     center,
                            float       squared_radius,  // radius * radius
                            PICKING_RAY ray);

//--------------------------------------------------------------------------

bool ray_intersects_triangle (    vector3     vert0,         // triangle in world
                                  vector3     vert1,
                                  vector3     vert2,
                                  PICKING_RAY ray,
                              out float[2]    texture_info,  // used in function below
                              out float       distance,      // distance to triangle (user picked nearest one if several)
                                  bool        test_both_sides = false);  // false is much faster

//--------------------------------------------------------------------------

void ray_compute_texture_uv (    vector2   tex0,           // .t components of triangle
                                 vector2   tex1,
                                 vector2   tex2,
                                 float[2]  texture_info,   // result from ray_intersects_triangle()
                             out vector2   uv);            // texture coordinates (between tex0, tex1, tex2)

//-------------------------------------------------------------------------------------------------
//----- frustum tests : test if world geometry is visible on screen, or besides/behind camera -----
//-------------------------------------------------------------------------------------------------

// test if the world sphere is inside the view frustum - this test is cheap.
bool is_sphere_in_frustum (vector3 point, float radius);

// test if the world box is inside the view frustum - this test is about 10 times more costly than for a sphere.
bool is_box_in_frustum (vector3 center, vector3 size);

bool is_shadow_in_frustum (vector3 position, vector3 shadow_points[4], float radius);

//-------------------------------------------------------------------------------------------------

void get_view_matrix (out matrix44 view);

//-------------------------------------------------------------------------------------------------
