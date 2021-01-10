#ifndef SMALL3DLIB_H
#define SMALL3DLIB_H

/*
  Simple realtime 3D software rasterization renderer. It is fast, focused on
  resource-limited computers, located in a single C header file, with no
  dependencies, using only 32bit integer arithmetics.

  author: Miloslav Ciz
  license: CC0 1.0 (public domain)
           found at https://creativecommons.org/publicdomain/zero/1.0/
           + additional waiver of all IP
  version: 0.852d

  Before including the library, define S3L_PIXEL_FUNCTION to the name of the
  function you'll be using to draw single pixels (this function will be called
  by the library to render the frames). Also either init S3L_resolutionX and
  S3L_resolutionY or define S3L_RESOLUTION_X and S3L_RESOLUTION_Y.

  You'll also need to decide what rendering strategy and other settings you
  want to use, depending on your specific usecase. You may want to use a
  z-buffer (full or reduced, S3L_Z_BUFFER), sorted-drawing (S3L_SORT), or even
  none of these. See the description of the options in this file.

  The rendering itself is done with S3L_drawScene, usually preceded by
  S3L_newFrame (for clearing zBuffer etc.).

  The library is meant to be used in not so huge programs that use single
  translation unit and so includes both declarations and implementation at once.
  If you for some reason use multiple translation units (which include the
  library), you'll have to handle this yourself (e.g. create a wrapper, manually
  split the library into .c and .h etc.).

  --------------------

  This work's goal is to never be encumbered by any exclusive intellectual
  property rights. The work is therefore provided under CC0 1.0 + additional
  WAIVER OF ALL INTELLECTUAL PROPERTY RIGHTS that waives the rest of
  intellectual property rights not already waived by CC0 1.0. The WAIVER OF ALL
  INTELLECTUAL PROPERTY RGHTS is as follows:

  Each contributor to this work agrees that they waive any exclusive rights,
  including but not limited to copyright, patents, trademark, trade dress,
  industrial design, plant varieties and trade secrets, to any and all ideas,
  concepts, processes, discoveries, improvements and inventions conceived,
  discovered, made, designed, researched or developed by the contributor either
  solely or jointly with others, which relate to this work or result from this
  work. Should any waiver of such right be judged legally invalid or
  ineffective under applicable law, the contributor hereby grants to each
  affected person a royalty-free, non transferable, non sublicensable, non
  exclusive, irrevocable and unconditional license to this right.

  --------------------

  CONVENTIONS:

  This library should never draw pixels outside the specified screen
  boundaries, so you don't have to check this (that would cost CPU time)!

  You can safely assume that triangles are rasterized one by one and from top
  down, left to right (so you can utilize e.g. various caches), and if sorting
  is disabled the order of rasterization will be that specified in the scene
  structure and model arrays (of course, some triangles and models may be
  skipped due to culling etc.).

  Angles are in S3L_Units, a full angle (2 pi) is S3L_FRACTIONS_PER_UNITs.

  We use row vectors.

  In 3D space, a left-handed coord. system is used. One spatial unit is split
  into S3L_FRACTIONS_PER_UNIT fractions (fixed point arithmetic).

     y ^
       |   _
       |   /| z
       |  /
       | /
  [0,0,0]-------> x

  Untransformed camera is placed at [0,0,0], looking forward along +z axis. The
  projection plane is centered at [0,0,0], stretrinch from
  -S3L_FRACTIONS_PER_UNIT to S3L_FRACTIONS_PER_UNIT horizontally (x),
  vertical size (y) depends on the aspect ratio (S3L_RESOLUTION_X and
  S3L_RESOLUTION_Y). Camera FOV is defined by focal length in S3L_Units.

           y ^
             |  _
             |  /| z
         ____|_/__
        |    |/   |
     -----[0,0,0]-|-----> x
        |____|____|
             |
             |

  Rotations use Euler angles and are generally in the extrinsic Euler angles in
  ZXY order (by Z, then by X, then by Y). Positive rotation about an axis
  rotates CW (clock-wise) when looking in the direction of the axis.

  Coordinates of pixels on the screen start at the top left, from [0,0].

  There is NO subpixel accuracy (screen coordinates are only integer).

  Triangle rasterization rules are these (mostly same as OpenGL, D3D etc.):

  - Let's define:
    - left side:
      - not exactly horizontal, and on the left side of triangle
      - exactly horizontal and above the topmost
      (in other words: its normal points at least a little to the left or
       completely up)
    - right side: not left side
  - Pixel centers are at integer coordinates and triangle for drawing are
    specified with integer coordinates of pixel centers.
  - A pixel is rasterized:
    - if its center is inside the triangle OR
    - if its center is exactly on the triangle side which is left and at the
      same time is not on the side that's right (case of a triangle that's on
      a single line) OR
    - if its center is exactly on the triangle corner of sides neither of which
      is right.

  These rules imply among others:

  - Adjacent triangles don't have any overlapping pixels, nor gaps between.
  - Triangles of points that lie on a single line are NOT rasterized.
  - A single "long" triangle CAN be rasterized as isolated islands of pixels.
  - Transforming (e.g. mirroring, rotating by 90 degrees etc.) a result of
    rasterizing triangle A is NOT generally equal to applying the same
    transformation to triangle A first and then rasterizing it. Even the number
    of rasterized pixels is usually different.
  - If specifying a triangle with integer coordinates (which we are), then:
    - The bottom-most corner (or side) of a triangle is never rasterized
      (because it is connected to a right side).
    - The top-most corner can only be rasterized on completely horizontal side
      (otherwise it is connected to a right side).
    - Vertically middle corner is rasterized if and only if it is on the left
      of the triangle and at the same time is also not the bottom-most corner.
*/

#include <stdint.h>

#include "../screen.h"
#include "../doomtype.h"

extern UINT16 S3L_resolutionX; /**< If a static resolution is not set with
                                    S3L_RESOLUTION_X, this variable can be
                                    used to change X resolution at runtime,
                                    in which case S3L_MAX_PIXELS has to be
                                    defined (to allocate zBuffer etc.)! */

extern UINT16 S3L_resolutionY; /**< Same as S3L_resolutionX, but for Y
                                    resolution. */

#define S3L_RESOLUTION_X S3L_resolutionX
#define S3L_RESOLUTION_Y S3L_resolutionY

enum
{
	S3L_VIEW_WINDOW_X1,
	S3L_VIEW_WINDOW_X2,
	S3L_VIEW_WINDOW_Y1,
	S3L_VIEW_WINDOW_Y2,
	S3L_VIEW_WINDOW_SIDES,
};

extern UINT16 S3L_viewWindow[S3L_VIEW_WINDOW_SIDES];

extern UINT16 S3L_screenVertexOffsetX;
extern UINT16 S3L_screenVertexOffsetY;

extern INT16 *S3L_spanFloorClip;
extern INT16 *S3L_spanCeilingClip;
extern INT32 S3L_spanPortalClip[2];

/** Units of measurement in 3D space. There is S3L_FRACTIONS_PER_UNIT in one
spatial unit. By dividing the unit into fractions we effectively achieve a
fixed point arithmetic. The number of fractions is a constant that serves as
1.0 in floating point arithmetic (normalization etc.). */

typedef INT32 S3L_Unit; // Lactozilla: Needs to be able to fit fixed_t values, for higher precision numbers.

/** How many fractions a spatial unit is split into. This is NOT SUPPOSED TO
BE REDEFINED, so rather don't do it (otherwise things may overflow etc.). */

#define S3L_UNIT_BITS 9

#define S3L_FRACTIONS_PER_UNIT (1<<S3L_UNIT_BITS) // 512

/** How much to shift a fixed point number down to get a S3L unit. */

#define S3L_UNIT_SHIFT (16 - S3L_UNIT_BITS)

typedef INT16 S3L_ScreenCoord;

#ifndef S3L_PRECISE_ANGLES
  /** Uses 16.16 fixed point math instead of 3D units for angle math. */

  #define S3L_PRECISE_ANGLES 1
#endif

#ifndef S3L_NEAR_CROSS_STRATEGY
  /** Specifies how the library will handle triangles that partially cross the
  near plane. These are problematic and require special handling. Possible
  values:

    0: Strictly cull any triangle crossing the near plane. This will make such
       triangles disappear. This is good for performance or models viewed only
       from at least small distance.
    1: Forcefully push the vertices crossing near plane in front of it. This is
       a cheap technique that can be good enough for displaying simple
       environments on slow devices, but texturing and geometric artifacts/warps
       will appear.
    2: Geometrically correct the triangles crossing the near plane. This may
       result in some triangles being subdivided into two and is a little more
       expensive, but the results will be geometrically correct, even though
       barycentric correction is not performed so texturing artifacts will
       appear. Can be ideal with S3L_FLAT.
    3: NOT IMPLEMENTED YET
       Perform both geometrical and barycentric correction of triangle crossing
       the near plane. This is significantly more expensive but results in
       correct rendering.
  */

  #define S3L_NEAR_CROSS_STRATEGY 0
#endif

#ifndef S3L_FLAT
  /** If on, disables computation of per-pixel values such as barycentric
  coordinates and depth -- these will still be available but will be the same
  for the whole triangle. This can be used to create flat-shaded renders and
  will be a lot faster. With this option on you will probably want to use
  sorting instead of z-buffer. */

  #define S3L_FLAT 0
#endif

#if S3L_FLAT
  #define S3L_COMPUTE_DEPTH 0
  #define S3L_PERSPECTIVE_CORRECTION 0
  // don't disable z-buffer, it makes sense to use it with no sorting
#endif

#ifndef S3L_PERSPECTIVE_CORRECTION
  /** Specifies what type of perspective correction (PC) to use. Remember this
  is an expensive operation! Possible values:

  - 0: No perspective correction. Fastest, inaccurate from most angles.
  - 1: Per-pixel perspective correction, accurate but very expensive.
  - 2: Approximation (computing only at every S3L_PC_APPROX_LENGTHth pixel).
       Quake-style approximation is used, which only computes the PC after
       S3L_PC_APPROX_LENGTH pixels. This is reasonably accurate and fast. */

  #define S3L_PERSPECTIVE_CORRECTION 0
#endif

#ifndef S3L_PC_APPROX_LENGTH
  /** For S3L_PERSPECTIVE_CORRECTION == 2, this specifies after how many pixels
  PC is recomputed. Should be a power of two to keep up the performance.
  Smaller is nicer but slower. */

  #define S3L_PC_APPROX_LENGTH 32
#endif

#if S3L_PERSPECTIVE_CORRECTION != 0
#define S3L_COMPUTE_DEPTH 1  // PC inevitably computes depth, so enable it
#endif

#ifndef S3L_COMPUTE_DEPTH
  /** Whether to compute depth for each pixel (fragment). Some other options
  may turn this on automatically. If you don't need depth information, turning
  this off can save performance. Depth will still be accessible in
  S3L_PixelInfo, but will be constant -- equal to center point depth -- over
  the whole triangle. */
  #define S3L_COMPUTE_DEPTH 1
#endif

#ifndef S3L_Z_BUFFER
  /** What type of z-buffer (depth buffer) to use for visibility determination.
  Possible values:

  - 0: Don't use z-buffer. This saves a lot of memory, but visibility checking
       won't be pixel-accurate and has to mostly be done by other means
       (typically sorting).
  - 1: Use full z-buffer (of S3L_Units) for visibiltiy determination. This is
       the most accurate option (and also a fast one), but requires a big
       amount of memory.
  - 2: Use reduced-size z-buffer (of bytes). This is fast and somewhat
       accurate, but inaccuracies can occur and a considerable amount of memory
       is needed. */

  #define S3L_Z_BUFFER 1
#endif

#ifndef S3L_REDUCED_Z_BUFFER_GRANULARITY
  /** For S3L_Z_BUFFER == 2 this sets the reduced z-buffer granularity. */

  #define S3L_REDUCED_Z_BUFFER_GRANULARITY 5
#endif

#ifndef S3L_STENCIL_BUFFER
  /** Whether to use stencil buffer for drawing -- with this a pixel that would
  be resterized over an already rasterized pixel (within a frame) will be
  discarded. This is mostly for front-to-back sorted drawing. */

  #define S3L_STENCIL_BUFFER 0
#endif

#ifndef S3L_NEAR
  /** Distance of the near clipping plane. Points in front or EXATLY ON this
  plane are considered outside the frustum. This must be >= 0. */

  #define S3L_NEAR (S3L_FRACTIONS_PER_UNIT / 4)
#endif

#if S3L_NEAR <= 0
#define S3L_NEAR 1 // Can't be <= 0.
#endif

#ifndef S3L_FAST_LERP_QUALITY
  /** Quality (scaling) of SOME (stepped) linear interpolations. 0 will most
  likely be a tiny bit faster, but artifacts can occur for bigger tris, while
  higher values can fix this -- in theory all higher values will have the same
  speed (it is a shift value), but it mustn't be too high to prevent
  overflow. */

  #define S3L_FAST_LERP_QUALITY 11
#endif

/** Vector that consists of four scalars and can represent homogenous
  coordinates, but is generally also used as Vec3 and Vec2 for various
  purposes. */
typedef struct
{
  S3L_Unit x;
  S3L_Unit y;
  S3L_Unit z;
  S3L_Unit w;
} S3L_Vec4;

/** Normalizes Vec3. Note that this function tries to normalize correctly
  rather than quickly! If you need to normalize quickly, do it yourself in a
  way that best fits your case. */
void S3L_normalizeVec3(S3L_Vec4 *v);

S3L_Unit S3L_vec2Length(S3L_Vec4 v);
void S3L_crossProduct(S3L_Vec4 a, S3L_Vec4 b, S3L_Vec4 *result);
S3L_Unit S3L_dotProductVec3(S3L_Vec4 *a, S3L_Vec4 *b);

/** Computes a reflection direction (typically used e.g. for specular component
  in Phong illumination). The input vectors must be normalized. The result will
  be normalized as well. */
void S3L_reflect(S3L_Vec4 *toLight, S3L_Vec4 *normal, S3L_Vec4 *result);

typedef struct
{
  S3L_Vec4 translation;
  S3L_Vec4 rotation; /**< Euler angles. Rortation is applied in this order:
                          1. z = by z (roll) CW looking along z+
                          2. x = by x (pitch) CW looking along x+
                          3. y = by y (yaw) CW looking along y+ */
  S3L_Vec4 scale;
} S3L_Transform3D;

void S3L_lookAt(S3L_Vec4 pointTo, S3L_Transform3D *t);

void S3L_initTransform3D(S3L_Transform3D *t);

void S3L_setTransform3D(
  S3L_Unit tx,
  S3L_Unit ty,
  S3L_Unit tz,
  S3L_Unit rx,
  S3L_Unit ry,
  S3L_Unit rz,
  S3L_Unit sx,
  S3L_Unit sy,
  S3L_Unit sz,
  S3L_Transform3D *t);

/** Converts rotation transformation to three direction vectors of given length
  (any one can be NULL, in which case it won't be computed). */
void S3L_rotationToDirections(
  S3L_Vec4 rotation,
  S3L_Unit length,
  S3L_Vec4 *forw,
  S3L_Vec4 *right,
  S3L_Vec4 *up);

/** 4x4 matrix, used mostly for 3D transforms. The indexing is this:
    matrix[column][row]. */
typedef S3L_Unit S3L_Mat4[4][4];

void S3L_transposeMat4(S3L_Mat4 *m);

void S3L_makeTranslationMat(
  S3L_Unit offsetX,
  S3L_Unit offsetY,
  S3L_Unit offsetZ,
  S3L_Mat4 *m);

/** Makes a scaling matrix. DON'T FORGET: scale of 1.0 is set with
  S3L_FRACTIONS_PER_UNIT! */
void S3L_makeScaleMatrix(
  S3L_Unit scaleX,
  S3L_Unit scaleY,
  S3L_Unit scaleZ,
  S3L_Mat4 *m);

/** Makes a matrix for rotation in the ZXY order. */
void S3L_makeRotationMatrixZXY(
  S3L_Unit byX,
  S3L_Unit byY,
  S3L_Unit byZ,
  S3L_Mat4 *m);

void S3L_makeRotationMatrixZXYPrecise(
  S3L_Unit byX,
  S3L_Unit byY,
  S3L_Unit byZ,
  S3L_Mat4 *m);

void S3L_makeWorldMatrix(S3L_Transform3D *worldTransform, S3L_Mat4 *m);
void S3L_makeCameraMatrix(S3L_Transform3D *cameraTransform, S3L_Mat4 *m);

/** Multiplies a vector by a matrix with normalization by
  S3L_FRACTIONS_PER_UNIT. Result is stored in the input vector. */
void S3L_vec4Xmat4(S3L_Vec4 *v, S3L_Mat4 *m);

/** Same as S3L_vec4Xmat4 but faster, because this version doesn't compute the
  W component of the result, which is usually not needed. */
void S3L_vec3Xmat4(S3L_Vec4 *v, S3L_Mat4 *m);

/** Multiplies two matrices with normalization by S3L_FRACTIONS_PER_UNIT.
  Result is stored in the first matrix. The result represents a transformation
  that has the same effect as applying the transformation represented by m1 and
  then m2 (in that order). */
void S3L_mat4Xmat4(S3L_Mat4 *m1, S3L_Mat4 *m2);

typedef struct
{
  S3L_Unit focalLength;       ///< Defines the field of view (FOV).
  S3L_Transform3D transform;
} S3L_Camera;

void S3L_initCamera(S3L_Camera *cam);

typedef struct
{
  S3L_ScreenCoord x;          ///< Screen X coordinate.
  S3L_ScreenCoord y;          ///< Screen Y coordinate.

  S3L_Unit barycentric[3]; /**< Barycentric coords correspond to the three
                              vertices. These serve to locate the pixel on a
                              triangle and interpolate values between it's
                              three points. Each one goes from 0 to
                              S3L_FRACTIONS_PER_UNIT (including), but due to
                              rounding error may fall outside this range (you
                              can use S3L_correctBarycentricCoords to fix this
                              for the price of some performance). The sum of
                              the three coordinates will always be exactly
                              S3L_FRACTIONS_PER_UNIT. */
  S3L_Unit depth;         ///< Depth (only if depth is turned on).
  S3L_Unit previousZ;     /**< Z-buffer value (not necessarily world depth in
                               S3L_Units!) that was in the z-buffer on the
                               pixels position before this pixel was
                               rasterized. This can be used to set the value
                               back, e.g. for transparency. */
  S3L_ScreenCoord triangleSize[2]; /**< Rasterized triangle width and height,
                              can be used e.g. for MIP mapping. */
} S3L_PixelInfo;         /**< Used to pass the info about a rasterized pixel
                              (fragment) to the user-defined drawing func. */

extern void (*S3L_pixelFunction)(void *pixel);

// general helper functions
S3L_Unit S3L_abs(S3L_Unit value);
S3L_Unit S3L_min(S3L_Unit v1, S3L_Unit v2);
S3L_Unit S3L_max(S3L_Unit v1, S3L_Unit v2);
S3L_Unit S3L_clamp(S3L_Unit v, S3L_Unit v1, S3L_Unit v2);
S3L_Unit S3L_wrap(S3L_Unit value, S3L_Unit mod);
S3L_Unit S3L_nonZero(S3L_Unit value);
S3L_Unit S3L_zeroClamp(S3L_Unit value);

S3L_Unit S3L_sin(S3L_Unit x);
S3L_Unit S3L_asin(S3L_Unit x);

S3L_Unit S3L_vec3Length(S3L_Vec4 v);
S3L_Unit S3L_sqrt(S3L_Unit value);

/** Projects a single point from 3D space to the screen space (pixels), which
  can be useful e.g. for drawing sprites. The w component of input and result
  holds the point size. If this size is 0 in the result, the sprite is outside
  the view. */
void project3DPointToScreen(
  S3L_Vec4 point,
  S3L_Camera cam,
  S3L_Vec4 *result);

/** Computes a normalized normal of given triangle. */
void S3L_triangleNormal(S3L_Vec4 t0, S3L_Vec4 t1, S3L_Vec4 t2,
  S3L_Vec4 *n);

/** Draws a triangle according to given config. The vertices are specified in
  Screen Space space (pixels). If perspective correction is enabled, each
  vertex has to have a depth (Z position in camera space) specified in the Z
  component. */
void S3L_drawTriangle(
  S3L_Vec4 point0,
  S3L_Vec4 point1,
  S3L_Vec4 point2);

/** This should be called before rendering each frame. The function clears
  buffers and does potentially other things needed for the frame. */
void S3L_newFrame(void);

#if S3L_Z_BUFFER == 1
  #define S3L_MAX_DEPTH 2147483647
  #define S3L_ZBUFFER_TYPE S3L_Unit
  #define S3L_zBufferFormat(depth) (depth)
#elif S3L_Z_BUFFER == 2
  #define S3L_MAX_DEPTH 255
  #define S3L_ZBUFFER_TYPE UINT8
  #define S3L_zBufferFormat(depth)\
    S3L_min(255,(depth) >> S3L_REDUCED_Z_BUFFER_GRANULARITY)
#endif

#if S3L_Z_BUFFER != 0
extern S3L_ZBUFFER_TYPE *S3L_zBuffer;
#endif

void S3L_zBufferClear(void);
void S3L_stencilBufferClear(void);

/** Writes a value (not necessarily depth! depends on the format of z-buffer)
  to z-buffer (if enabled). Does NOT check boundaries! */
void S3L_zBufferWrite(S3L_ScreenCoord x, S3L_ScreenCoord y, S3L_Unit value);

/** Reads a value (not necessarily depth! depends on the format of z-buffer)
  from z-buffer (if enabled). Does NOT check boundaries! */
S3L_Unit S3L_zBufferRead(S3L_ScreenCoord x, S3L_ScreenCoord y);

/** Predefined vertices of a cube to simply insert in an array. These come with
    S3L_CUBE_TRIANGLES and S3L_CUBE_TEXCOORDS. */
#define S3L_CUBE_VERTICES(m)\
 /* 0 front, bottom, right */\
 m/2, -m/2, -m/2,\
 /* 1 front, bottom, left */\
-m/2, -m/2, -m/2,\
 /* 2 front, top,    right */\
 m/2,  m/2, -m/2,\
 /* 3 front, top,    left */\
-m/2,  m/2, -m/2,\
 /* 4 back,  bottom, right */\
 m/2, -m/2,  m/2,\
 /* 5 back,  bottom, left */\
-m/2, -m/2,  m/2,\
 /* 6 back,  top,    right */\
 m/2,  m/2,  m/2,\
 /* 7 back,  top,    left */\
-m/2,  m/2,  m/2

#define S3L_CUBE_VERTEX_COUNT 8

/** Predefined triangle indices of a cube, to be used with S3L_CUBE_VERTICES
    and S3L_CUBE_TEXCOORDS. */
#define S3L_CUBE_TRIANGLES\
  3, 0, 2, /* front  */\
  1, 0, 3,\
  0, 4, 2, /* right  */\
  2, 4, 6,\
  4, 5, 6, /* back   */\
  7, 6, 5,\
  3, 7, 1, /* left   */\
  1, 7, 5,\
  6, 3, 2, /* top    */\
  7, 3, 6,\
  1, 4, 0, /* bottom */\
  5, 4, 1

#define S3L_CUBE_TRIANGLE_COUNT 12

/** Predefined texture coordinates of a cube, corresponding to triangles (NOT
    vertices), to be used with S3L_CUBE_VERTICES and S3L_CUBE_TRIANGLES. */
#define S3L_CUBE_TEXCOORDS(m)\
  0,0,  m,m,  m,0,\
  0,m,  m,m,  0,0,\
  m,m,  m,0,  0,m,\
  0,m,  m,0,  0,0,\
  m,0,  0,0,  m,m,\
  0,m,  m,m,  0,0,\
  0,0,  0,m,  m,0,\
  m,0,  0,m,  m,m,\
  0,0,  m,m,  m,0,\
  0,m,  m,m,  0,0,\
  m,0,  0,m,  m,m,\
  0,0,  0,m,  m,0

/**
  Projects a triangle to the screen. If enabled, a triangle can be potentially
  subdivided into two if it crosses the near plane, in which case two projected
  triangles are returned (return value will be 1).
*/
UINT8 S3L_projectTriangle(
  const void *tri,
  S3L_Mat4 *matrix,
  UINT32 focalLength,
  S3L_Vec4 transformed[6]);

/**
  Checks if given triangle (in Screen Space) is at least partially visible,
  i.e. returns false if the triangle is either completely outside the frustum
  (left, right, top, bottom, near) or is invisible due to backface culling.
*/
SINT8 S3L_triangleIsVisible(
  S3L_Vec4 p0,
  S3L_Vec4 p1,
  S3L_Vec4 p2,
  UINT8 backfaceCulling);

/** Returns a value interpolated between the three triangle vertices based on
  barycentric coordinates. */
S3L_Unit S3L_interpolateBarycentric(
  S3L_Unit value0,
  S3L_Unit value1,
  S3L_Unit value2,
  S3L_Unit barycentric[3]);

#endif // guard
