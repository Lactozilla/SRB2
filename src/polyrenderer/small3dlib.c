/*
  Simple realtime 3D software rasterization renderer. It is fast, focused on
  resource-limited computers, located in a single C header file, with no
  dependencies, using only 32bit integer arithmetics.

  author: Miloslav Ciz
  license: CC0 1.0 (public domain)
           found at https://creativecommons.org/publicdomain/zero/1.0/
           + additional waiver of all IP
  version: 0.852d

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
*/

#include "small3dlib.h"
#include "rsp_types.h"

UINT16 S3L_resolutionX = 512;
UINT16 S3L_resolutionY = 512;

UINT16 S3L_viewWindow[S3L_VIEW_WINDOW_SIDES];

UINT16 S3L_screenVertexOffsetX = 0;
UINT16 S3L_screenVertexOffsetY = 0;

INT16 *S3L_spanFloorClip = NULL;
INT16 *S3L_spanCeilingClip = NULL;
INT32 S3L_spanPortalClip[2];

static inline void S3L_mapProjectionPlaneToScreen(
  S3L_Vec4 point,
  S3L_ScreenCoord *screenX,
  S3L_ScreenCoord *screenY);

/** Determines the winding of a triangle, returns 1 (CW, clockwise), -1 (CCW,
  counterclockwise) or 0 (points lie on a single line). */
static inline SINT8 S3L_triangleWinding(
  S3L_ScreenCoord x0,
  S3L_ScreenCoord y0,
  S3L_ScreenCoord x1,
  S3L_ScreenCoord y1,
  S3L_ScreenCoord x2,
  S3L_ScreenCoord y2);

//=============================================================================
// privates

#define S3L_UNUSED(what) (void)(what) ///< helper macro for unused vars

#define S3L_HALF_RESOLUTION_X (S3L_RESOLUTION_X >> 1)
#define S3L_HALF_RESOLUTION_Y (S3L_RESOLUTION_Y >> 1)

#if S3L_Z_BUFFER != 0
S3L_ZBUFFER_TYPE *S3L_zBuffer = NULL;
#endif

#if S3L_Z_BUFFER
static inline SINT8 S3L_zTest(
  S3L_ScreenCoord x,
  S3L_ScreenCoord y,
  S3L_Unit depth)
{
  UINT32 index = y * S3L_RESOLUTION_X + x;

  depth = S3L_zBufferFormat(depth);

#if S3L_Z_BUFFER == 2
  #define cmp <= /* For reduced z-buffer we need equality test, because
                    otherwise pixels at the maximum depth (255) would never be
                    drawn over the background (which also has the depth of
                    255). */
#else
  #define cmp <  /* For normal z-buffer we leave out equality test to not waste
                    time by drawing over already drawn pixls. */
#endif

  if (depth cmp S3L_zBuffer[index])
  {
    if (rsp_target.write & RSP_WRITE_DEPTH)
      S3L_zBuffer[index] = depth;
    return 1;
  }

#undef cmp

  return 0;
}
#endif

S3L_Unit S3L_zBufferRead(S3L_ScreenCoord x, S3L_ScreenCoord y)
{
#if S3L_Z_BUFFER
  return S3L_zBuffer[y * S3L_RESOLUTION_X + x];
#else
  S3L_UNUSED(x);
  S3L_UNUSED(y);

  return 0;
#endif
}

void S3L_zBufferWrite(S3L_ScreenCoord x, S3L_ScreenCoord y, S3L_Unit value)
{
#if S3L_Z_BUFFER
  S3L_zBuffer[y * S3L_RESOLUTION_X + x] = value;
#else
  S3L_UNUSED(x);
  S3L_UNUSED(y);
  S3L_UNUSED(value);
#endif
}

#if S3L_STENCIL_BUFFER
  #define S3L_STENCIL_BUFFER_SIZE\
    ((S3L_RESOLUTION_X * S3L_RESOLUTION_Y - 1) / 8 + 1)

UINT8 S3L_stencilBuffer[S3L_STENCIL_BUFFER_SIZE];

static inline SINT8 S3L_stencilTest(
  S3L_ScreenCoord x,
  S3L_ScreenCoord y)
{
  UINT32 index = y * S3L_RESOLUTION_X + x;
  UINT32 bit = (index & 0x00000007);
  index = index >> 3;

  UINT8 val = S3L_stencilBuffer[index];

  if ((val >> bit) & 0x1)
    return 0;

  S3L_stencilBuffer[index] = val | (0x1 << bit);

  return 1;
}
#endif

#define S3L_COMPUTE_LERP_DEPTH\
  (S3L_COMPUTE_DEPTH && (S3L_PERSPECTIVE_CORRECTION == 0))

#define S3L_SIN_TABLE_LENGTH 128

static const S3L_Unit S3L_sinTable[S3L_SIN_TABLE_LENGTH] =
{
  /* 511 was chosen here as a highest number that doesn't overflow during
     compilation for S3L_FRACTIONS_PER_UNIT == 1024 */

  (0*S3L_FRACTIONS_PER_UNIT)/511, (6*S3L_FRACTIONS_PER_UNIT)/511,
  (12*S3L_FRACTIONS_PER_UNIT)/511, (18*S3L_FRACTIONS_PER_UNIT)/511,
  (25*S3L_FRACTIONS_PER_UNIT)/511, (31*S3L_FRACTIONS_PER_UNIT)/511,
  (37*S3L_FRACTIONS_PER_UNIT)/511, (43*S3L_FRACTIONS_PER_UNIT)/511,
  (50*S3L_FRACTIONS_PER_UNIT)/511, (56*S3L_FRACTIONS_PER_UNIT)/511,
  (62*S3L_FRACTIONS_PER_UNIT)/511, (68*S3L_FRACTIONS_PER_UNIT)/511,
  (74*S3L_FRACTIONS_PER_UNIT)/511, (81*S3L_FRACTIONS_PER_UNIT)/511,
  (87*S3L_FRACTIONS_PER_UNIT)/511, (93*S3L_FRACTIONS_PER_UNIT)/511,
  (99*S3L_FRACTIONS_PER_UNIT)/511, (105*S3L_FRACTIONS_PER_UNIT)/511,
  (111*S3L_FRACTIONS_PER_UNIT)/511, (118*S3L_FRACTIONS_PER_UNIT)/511,
  (124*S3L_FRACTIONS_PER_UNIT)/511, (130*S3L_FRACTIONS_PER_UNIT)/511,
  (136*S3L_FRACTIONS_PER_UNIT)/511, (142*S3L_FRACTIONS_PER_UNIT)/511,
  (148*S3L_FRACTIONS_PER_UNIT)/511, (154*S3L_FRACTIONS_PER_UNIT)/511,
  (160*S3L_FRACTIONS_PER_UNIT)/511, (166*S3L_FRACTIONS_PER_UNIT)/511,
  (172*S3L_FRACTIONS_PER_UNIT)/511, (178*S3L_FRACTIONS_PER_UNIT)/511,
  (183*S3L_FRACTIONS_PER_UNIT)/511, (189*S3L_FRACTIONS_PER_UNIT)/511,
  (195*S3L_FRACTIONS_PER_UNIT)/511, (201*S3L_FRACTIONS_PER_UNIT)/511,
  (207*S3L_FRACTIONS_PER_UNIT)/511, (212*S3L_FRACTIONS_PER_UNIT)/511,
  (218*S3L_FRACTIONS_PER_UNIT)/511, (224*S3L_FRACTIONS_PER_UNIT)/511,
  (229*S3L_FRACTIONS_PER_UNIT)/511, (235*S3L_FRACTIONS_PER_UNIT)/511,
  (240*S3L_FRACTIONS_PER_UNIT)/511, (246*S3L_FRACTIONS_PER_UNIT)/511,
  (251*S3L_FRACTIONS_PER_UNIT)/511, (257*S3L_FRACTIONS_PER_UNIT)/511,
  (262*S3L_FRACTIONS_PER_UNIT)/511, (268*S3L_FRACTIONS_PER_UNIT)/511,
  (273*S3L_FRACTIONS_PER_UNIT)/511, (278*S3L_FRACTIONS_PER_UNIT)/511,
  (283*S3L_FRACTIONS_PER_UNIT)/511, (289*S3L_FRACTIONS_PER_UNIT)/511,
  (294*S3L_FRACTIONS_PER_UNIT)/511, (299*S3L_FRACTIONS_PER_UNIT)/511,
  (304*S3L_FRACTIONS_PER_UNIT)/511, (309*S3L_FRACTIONS_PER_UNIT)/511,
  (314*S3L_FRACTIONS_PER_UNIT)/511, (319*S3L_FRACTIONS_PER_UNIT)/511,
  (324*S3L_FRACTIONS_PER_UNIT)/511, (328*S3L_FRACTIONS_PER_UNIT)/511,
  (333*S3L_FRACTIONS_PER_UNIT)/511, (338*S3L_FRACTIONS_PER_UNIT)/511,
  (343*S3L_FRACTIONS_PER_UNIT)/511, (347*S3L_FRACTIONS_PER_UNIT)/511,
  (352*S3L_FRACTIONS_PER_UNIT)/511, (356*S3L_FRACTIONS_PER_UNIT)/511,
  (361*S3L_FRACTIONS_PER_UNIT)/511, (365*S3L_FRACTIONS_PER_UNIT)/511,
  (370*S3L_FRACTIONS_PER_UNIT)/511, (374*S3L_FRACTIONS_PER_UNIT)/511,
  (378*S3L_FRACTIONS_PER_UNIT)/511, (382*S3L_FRACTIONS_PER_UNIT)/511,
  (386*S3L_FRACTIONS_PER_UNIT)/511, (391*S3L_FRACTIONS_PER_UNIT)/511,
  (395*S3L_FRACTIONS_PER_UNIT)/511, (398*S3L_FRACTIONS_PER_UNIT)/511,
  (402*S3L_FRACTIONS_PER_UNIT)/511, (406*S3L_FRACTIONS_PER_UNIT)/511,
  (410*S3L_FRACTIONS_PER_UNIT)/511, (414*S3L_FRACTIONS_PER_UNIT)/511,
  (417*S3L_FRACTIONS_PER_UNIT)/511, (421*S3L_FRACTIONS_PER_UNIT)/511,
  (424*S3L_FRACTIONS_PER_UNIT)/511, (428*S3L_FRACTIONS_PER_UNIT)/511,
  (431*S3L_FRACTIONS_PER_UNIT)/511, (435*S3L_FRACTIONS_PER_UNIT)/511,
  (438*S3L_FRACTIONS_PER_UNIT)/511, (441*S3L_FRACTIONS_PER_UNIT)/511,
  (444*S3L_FRACTIONS_PER_UNIT)/511, (447*S3L_FRACTIONS_PER_UNIT)/511,
  (450*S3L_FRACTIONS_PER_UNIT)/511, (453*S3L_FRACTIONS_PER_UNIT)/511,
  (456*S3L_FRACTIONS_PER_UNIT)/511, (459*S3L_FRACTIONS_PER_UNIT)/511,
  (461*S3L_FRACTIONS_PER_UNIT)/511, (464*S3L_FRACTIONS_PER_UNIT)/511,
  (467*S3L_FRACTIONS_PER_UNIT)/511, (469*S3L_FRACTIONS_PER_UNIT)/511,
  (472*S3L_FRACTIONS_PER_UNIT)/511, (474*S3L_FRACTIONS_PER_UNIT)/511,
  (476*S3L_FRACTIONS_PER_UNIT)/511, (478*S3L_FRACTIONS_PER_UNIT)/511,
  (481*S3L_FRACTIONS_PER_UNIT)/511, (483*S3L_FRACTIONS_PER_UNIT)/511,
  (485*S3L_FRACTIONS_PER_UNIT)/511, (487*S3L_FRACTIONS_PER_UNIT)/511,
  (488*S3L_FRACTIONS_PER_UNIT)/511, (490*S3L_FRACTIONS_PER_UNIT)/511,
  (492*S3L_FRACTIONS_PER_UNIT)/511, (494*S3L_FRACTIONS_PER_UNIT)/511,
  (495*S3L_FRACTIONS_PER_UNIT)/511, (497*S3L_FRACTIONS_PER_UNIT)/511,
  (498*S3L_FRACTIONS_PER_UNIT)/511, (499*S3L_FRACTIONS_PER_UNIT)/511,
  (501*S3L_FRACTIONS_PER_UNIT)/511, (502*S3L_FRACTIONS_PER_UNIT)/511,
  (503*S3L_FRACTIONS_PER_UNIT)/511, (504*S3L_FRACTIONS_PER_UNIT)/511,
  (505*S3L_FRACTIONS_PER_UNIT)/511, (506*S3L_FRACTIONS_PER_UNIT)/511,
  (507*S3L_FRACTIONS_PER_UNIT)/511, (507*S3L_FRACTIONS_PER_UNIT)/511,
  (508*S3L_FRACTIONS_PER_UNIT)/511, (509*S3L_FRACTIONS_PER_UNIT)/511,
  (509*S3L_FRACTIONS_PER_UNIT)/511, (510*S3L_FRACTIONS_PER_UNIT)/511,
  (510*S3L_FRACTIONS_PER_UNIT)/511, (510*S3L_FRACTIONS_PER_UNIT)/511,
  (510*S3L_FRACTIONS_PER_UNIT)/511, (510*S3L_FRACTIONS_PER_UNIT)/511
};

#define S3L_SIN_TABLE_UNIT_STEP\
  (S3L_FRACTIONS_PER_UNIT / (S3L_SIN_TABLE_LENGTH * 4))

static inline void S3L_initVec4(S3L_Vec4 *v)
{
  v->x = 0; v->y = 0; v->z = 0; v->w = S3L_FRACTIONS_PER_UNIT;
}

static inline void S3L_setVec4(S3L_Vec4 *v, S3L_Unit x, S3L_Unit y, S3L_Unit z, S3L_Unit w)
{
  v->x = x;
  v->y = y;
  v->z = z;
  v->w = w;
}

static inline void S3L_vec3Add(S3L_Vec4 *result, S3L_Vec4 added)
{
  result->x += added.x;
  result->y += added.y;
  result->z += added.z;
}

static inline void S3L_vec3Sub(S3L_Vec4 *result, S3L_Vec4 substracted)
{
  result->x -= substracted.x;
  result->y -= substracted.y;
  result->z -= substracted.z;
}

static inline void S3L_initMat4(S3L_Mat4 *m)
{
  #define M(x,y) (*m)[x][y]
  #define S S3L_FRACTIONS_PER_UNIT

  M(0,0) = S; M(1,0) = 0; M(2,0) = 0; M(3,0) = 0;
  M(0,1) = 0; M(1,1) = S; M(2,1) = 0; M(3,1) = 0;
  M(0,2) = 0; M(1,2) = 0; M(2,2) = S; M(3,2) = 0;
  M(0,3) = 0; M(1,3) = 0; M(2,3) = 0; M(3,3) = S;

  #undef M
  #undef S
}

S3L_Unit S3L_dotProductVec3(S3L_Vec4 *a, S3L_Vec4 *b)
{
  return (a->x * b->x + a->y * b->y + a->z * b->z) / S3L_FRACTIONS_PER_UNIT;
}

void S3L_reflect(S3L_Vec4 *toLight, S3L_Vec4 *normal, S3L_Vec4 *result)
{
  S3L_Unit d = 2 * S3L_dotProductVec3(toLight,normal);

  result->x = (normal->x * d) / S3L_FRACTIONS_PER_UNIT - toLight->x;
  result->y = (normal->y * d) / S3L_FRACTIONS_PER_UNIT - toLight->y;
  result->z = (normal->z * d) / S3L_FRACTIONS_PER_UNIT - toLight->z;
}

void S3L_crossProduct(S3L_Vec4 a, S3L_Vec4 b, S3L_Vec4 *result)
{
  result->x = a.y * b.z - a.z * b.y;
  result->y = a.z * b.x - a.x * b.z;
  result->z = a.x * b.y - a.y * b.x;
}

void S3L_triangleNormal(S3L_Vec4 t0, S3L_Vec4 t1, S3L_Vec4 t2, S3L_Vec4 *n)
{
  #define ANTI_OVERFLOW 32

  t1.x = (t1.x - t0.x) / ANTI_OVERFLOW;
  t1.y = (t1.y - t0.y) / ANTI_OVERFLOW;
  t1.z = (t1.z - t0.z) / ANTI_OVERFLOW;

  t2.x = (t2.x - t0.x) / ANTI_OVERFLOW;
  t2.y = (t2.y - t0.y) / ANTI_OVERFLOW;
  t2.z = (t2.z - t0.z) / ANTI_OVERFLOW;

  #undef ANTI_OVERFLOW

  S3L_crossProduct(t1,t2,n);

  S3L_normalizeVec3(n);
}

void S3L_vec4Xmat4(S3L_Vec4 *v, S3L_Mat4 *m)
{
  S3L_Vec4 vBackup;

  vBackup.x = v->x;
  vBackup.y = v->y;
  vBackup.z = v->z;
  vBackup.w = v->w;

  #define dotCol(col)\
    ((vBackup.x * (*m)[col][0]) +\
     (vBackup.y * (*m)[col][1]) +\
     (vBackup.z * (*m)[col][2]) +\
     (vBackup.w * (*m)[col][3])) / S3L_FRACTIONS_PER_UNIT

  v->x = dotCol(0);
  v->y = dotCol(1);
  v->z = dotCol(2);
  v->w = dotCol(3);
}

void S3L_vec3Xmat4(S3L_Vec4 *v, S3L_Mat4 *m)
{
  S3L_Vec4 vBackup;

  #undef dotCol
  #define dotCol(col)\
    (vBackup.x * (*m)[col][0]) / S3L_FRACTIONS_PER_UNIT +\
    (vBackup.y * (*m)[col][1]) / S3L_FRACTIONS_PER_UNIT +\
    (vBackup.z * (*m)[col][2]) / S3L_FRACTIONS_PER_UNIT +\
    (*m)[col][3]

  vBackup.x = v->x;
  vBackup.y = v->y;
  vBackup.z = v->z;
  vBackup.w = v->w;

  v->x = dotCol(0);
  v->y = dotCol(1);
  v->z = dotCol(2);
  v->w = S3L_FRACTIONS_PER_UNIT;
}

#undef dotCol

// general helper functions
S3L_Unit S3L_abs(S3L_Unit value)
{
  return value * (((value >= 0) << 1) - 1);
}

S3L_Unit S3L_min(S3L_Unit v1, S3L_Unit v2)
{
  return v1 >= v2 ? v2 : v1;
}

S3L_Unit S3L_max(S3L_Unit v1, S3L_Unit v2)
{
  return v1 >= v2 ? v1 : v2;
}

S3L_Unit S3L_clamp(S3L_Unit v, S3L_Unit v1, S3L_Unit v2)
{
  return v >= v1 ? (v <= v2 ? v : v2) : v1;
}

S3L_Unit S3L_zeroClamp(S3L_Unit value)
{
  return (value * (value >= 0));
}

S3L_Unit S3L_wrap(S3L_Unit value, S3L_Unit mod)
{
  return value >= 0 ? (value % mod) : (mod + (value % mod) - 1);
}

S3L_Unit S3L_nonZero(S3L_Unit value)
{
  return (value + (value == 0));
}

/** Interpolated between two values, v1 and v2, in the same ratio as t is to
  tMax. Does NOT prevent zero division. */
static inline S3L_Unit S3L_interpolate(S3L_Unit v1, S3L_Unit v2, S3L_Unit t, S3L_Unit tMax)
{
  return v1 + ((v2 - v1) * t) / tMax;
}

/** Like S3L_interpolate, but uses a parameter that goes from 0 to
  S3L_FRACTIONS_PER_UNIT - 1, which can be faster. */
static inline S3L_Unit S3L_interpolateByUnit(S3L_Unit v1, S3L_Unit v2, S3L_Unit t)
{
  return v1 + ((v2 - v1) * t) / S3L_FRACTIONS_PER_UNIT;
}

/** Same as S3L_interpolateByUnit but with v1 == 0. Should be faster. */
static inline S3L_Unit S3L_interpolateByUnitFrom0(S3L_Unit v2, S3L_Unit t)
{
  return (v2 * t) / S3L_FRACTIONS_PER_UNIT;
}

/** Same as S3L_interpolate but with v1 == 0. Should be faster. */
static inline S3L_Unit S3L_interpolateFrom0(S3L_Unit v2, S3L_Unit t, S3L_Unit tMax)
{
  return (v2 * t) / tMax;
}

static inline S3L_Unit S3L_distanceManhattan(S3L_Vec4 a, S3L_Vec4 b)
{
  return
    S3L_abs(a.x - b.x) +
    S3L_abs(a.y - b.y) +
    S3L_abs(a.z - b.z);
}

void S3L_mat4Xmat4(S3L_Mat4 *m1, S3L_Mat4 *m2)
{
  S3L_Mat4 mat1;

  for (UINT16 row = 0; row < 4; ++row)
    for (UINT16 col = 0; col < 4; ++col)
      mat1[col][row] = (*m1)[col][row];

  for (UINT16 row = 0; row < 4; ++row)
    for (UINT16 col = 0; col < 4; ++col)
    {
      (*m1)[col][row] = 0;

      for (UINT16 i = 0; i < 4; ++i)
      {
        INT64 result = ((INT64)(mat1[i][row]) * (INT64)((*m2)[col][i])); // Lactozilla: Prevent an integer overflow
        (*m1)[col][row] += (result / S3L_FRACTIONS_PER_UNIT);
      }
    }
}

S3L_Unit S3L_sin(S3L_Unit x)
{
  SINT8 positive = 1;
  x = S3L_wrap(x / S3L_SIN_TABLE_UNIT_STEP,S3L_SIN_TABLE_LENGTH * 4);

  if (x < S3L_SIN_TABLE_LENGTH)
  {
  }
  else if (x < S3L_SIN_TABLE_LENGTH * 2)
  {
    x = S3L_SIN_TABLE_LENGTH * 2 - x - 1;
  }
  else if (x < S3L_SIN_TABLE_LENGTH * 3)
  {
    x = x - S3L_SIN_TABLE_LENGTH * 2;
    positive = 0;
  }
  else
  {
    x = S3L_SIN_TABLE_LENGTH - (x - S3L_SIN_TABLE_LENGTH * 3) - 1;
    positive = 0;
  }

  return positive ? S3L_sinTable[x] : -1 * S3L_sinTable[x];
}

S3L_Unit S3L_asin(S3L_Unit x)
{
  SINT8 sign = 1;
  INT16 low = 0;
  INT16 high = S3L_SIN_TABLE_LENGTH -1;
  INT16 middle;

  x = S3L_clamp(x,-S3L_FRACTIONS_PER_UNIT,S3L_FRACTIONS_PER_UNIT);

  if (x < 0)
  {
    sign = -1;
    x *= -1;
  }

  while (low <= high) // binary search
  {
    S3L_Unit v;

    middle = (low + high) / 2;

    v = S3L_sinTable[middle];

    if (v > x)
      high = middle - 1;
    else if (v < x)
      low = middle + 1;
    else
      break;
  }

  middle *= S3L_SIN_TABLE_UNIT_STEP;

  return sign * middle;
}

static inline S3L_Unit S3L_cos(S3L_Unit x)
{
  return S3L_sin(x + S3L_FRACTIONS_PER_UNIT / 4);
}

/** Corrects barycentric coordinates so that they exactly meet the defined
  conditions (each fall into <0,S3L_FRACTIONS_PER_UNIT>, sum =
  S3L_FRACTIONS_PER_UNIT). Note that doing this per-pixel can slow the program
  down significantly. */
static inline void S3L_correctBarycentricCoords(S3L_Unit barycentric[3])
{
  S3L_Unit d;

  barycentric[0] = S3L_clamp(barycentric[0],0,S3L_FRACTIONS_PER_UNIT);
  barycentric[1] = S3L_clamp(barycentric[1],0,S3L_FRACTIONS_PER_UNIT);

  d = S3L_FRACTIONS_PER_UNIT - barycentric[0] - barycentric[1];

  if (d < 0)
  {
    barycentric[0] += d;
    barycentric[2] = 0;
  }
  else
    barycentric[2] = d;
}

void S3L_makeTranslationMat(
  S3L_Unit offsetX,
  S3L_Unit offsetY,
  S3L_Unit offsetZ,
  S3L_Mat4 *m)
{
  #define M(x,y) (*m)[x][y]
  #define S S3L_FRACTIONS_PER_UNIT

  M(0,0) = S; M(1,0) = 0; M(2,0) = 0; M(3,0) = 0;
  M(0,1) = 0; M(1,1) = S; M(2,1) = 0; M(3,1) = 0;
  M(0,2) = 0; M(1,2) = 0; M(2,2) = S; M(3,2) = 0;
  M(0,3) = offsetX; M(1,3) = offsetY; M(2,3) = offsetZ; M(3,3) = S;

  #undef M
  #undef S
}

void S3L_makeScaleMatrix(
  S3L_Unit scaleX,
  S3L_Unit scaleY,
  S3L_Unit scaleZ,
  S3L_Mat4 *m)
{
  #define M(x,y) (*m)[x][y]

  M(0,0) = scaleX; M(1,0) = 0;      M(2,0) = 0;      M(3,0) = 0;
  M(0,1) = 0;      M(1,1) = scaleY; M(2,1) = 0;      M(3,1) = 0;
  M(0,2) = 0;      M(1,2) = 0;      M(2,2) = scaleZ; M(3,2) = 0;
  M(0,3) = 0;      M(1,3) = 0;      M(2,3) = 0;      M(3,3) = S3L_FRACTIONS_PER_UNIT;

  #undef M
}

static void _S3L_makeRotationMatrixZXYInternal(
  S3L_Unit sx, S3L_Unit sy, S3L_Unit sz,
  S3L_Unit cx, S3L_Unit cy, S3L_Unit cz,
  S3L_Mat4 *m)
{
  #define M(x,y) (*m)[x][y]
  #define S S3L_FRACTIONS_PER_UNIT

  M(0,0) = (cy * cz) / S + (sy * sx * sz) / (S * S);
  M(1,0) = (cx * sz) / S;
  M(2,0) = (cy * sx * sz) / (S * S) - (cz * sy) / S;
  M(3,0) = 0;

  M(0,1) = (cz * sy * sx) / (S * S) - (cy * sz) / S;
  M(1,1) = (cx * cz) / S;
  M(2,1) = (cy * cz * sx) / (S * S) + (sy * sz) / S;
  M(3,1) = 0;

  M(0,2) = (cx * sy) / S;
  M(1,2) = -1 * sx;
  M(2,2) = (cy * cx) / S;
  M(3,2) = 0;

  M(0,3) = 0;
  M(1,3) = 0;
  M(2,3) = 0;
  M(3,3) = S3L_FRACTIONS_PER_UNIT;

  #undef M
  #undef S
}

void S3L_makeRotationMatrixZXY(
  S3L_Unit byX,
  S3L_Unit byY,
  S3L_Unit byZ,
  S3L_Mat4 *m)
{
  S3L_Unit sx, sy, sz;
  S3L_Unit cx, cy, cz;

  byX *= -1;
  byY *= -1;
  byZ *= -1;

  sx = S3L_sin(byX);
  sy = S3L_sin(byY);
  sz = S3L_sin(byZ);

  cx = S3L_cos(byX);
  cy = S3L_cos(byY);
  cz = S3L_cos(byZ);

  _S3L_makeRotationMatrixZXYInternal(sx, sy, sz, cx, cy, cz, m);
}

void S3L_makeRotationMatrixZXYPrecise(
  S3L_Unit byX,
  S3L_Unit byY,
  S3L_Unit byZ,
  S3L_Mat4 *m)
{
  S3L_Unit sx, sy, sz;
  S3L_Unit cx, cy, cz;

  byX *= -1;
  byY *= -1;
  byZ *= -1;

#define PrecSin(x) FINESINE(FixedAngle(x) >> ANGLETOFINESHIFT) >> S3L_UNIT_SHIFT
#define PrecCos(x) FINECOSINE(FixedAngle(x) >> ANGLETOFINESHIFT) >> S3L_UNIT_SHIFT

  sx = PrecSin(byX);
  sy = PrecSin(byY);
  sz = PrecSin(byZ);

  cx = PrecCos(byX);
  cy = PrecCos(byY);
  cz = PrecCos(byZ);

  _S3L_makeRotationMatrixZXYInternal(sx, sy, sz, cx, cy, cz, m);

#undef PrecSin
#undef PrecCos
}

S3L_Unit S3L_sqrt(S3L_Unit value)
{
  SINT8 sign = 1;
  UINT32 result = 0;
  UINT32 a = value;
  UINT32 b = 1u << 30;

  if (value < 0)
  {
    sign = -1;
    value *= -1;
  }

  while (b > a)
    b >>= 2;

  while (b != 0)
  {
    if (a >= result + b)
    {
      a -= result + b;
      result = result +  2 * b;
    }

    b >>= 2;
    result >>= 1;
  }

  return result * sign;
}

S3L_Unit S3L_vec3Length(S3L_Vec4 v)
{
  return S3L_sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

S3L_Unit S3L_vec2Length(S3L_Vec4 v)
{
  return S3L_sqrt(v.x * v.x + v.y * v.y);
}

void S3L_normalizeVec3(S3L_Vec4 *v)
{
  S3L_Unit l;

  #define SCALE 16
  #define BOTTOM_LIMIT 16
  #define UPPER_LIMIT 900

  /* Here we try to decide if the vector is too small and would cause
     inaccurate result due to very its inaccurate length. If so, we scale
     it up. We can't scale up everything as big vectors overflow in length
     calculations. */

  if (
    S3L_abs(v->x) <= BOTTOM_LIMIT &&
    S3L_abs(v->y) <= BOTTOM_LIMIT &&
    S3L_abs(v->z) <= BOTTOM_LIMIT)
  {
    v->x *= SCALE;
    v->y *= SCALE;
    v->z *= SCALE;
  }
  else if (
    S3L_abs(v->x) > UPPER_LIMIT ||
    S3L_abs(v->y) > UPPER_LIMIT ||
    S3L_abs(v->z) > UPPER_LIMIT)
  {
    v->x /= SCALE;
    v->y /= SCALE;
    v->z /= SCALE;
  }

  #undef SCALE
  #undef BOTTOM_LIMIT
  #undef UPPER_LIMIT

  l = S3L_vec3Length(*v);

  if (l == 0)
    return;

  v->x = (v->x * S3L_FRACTIONS_PER_UNIT) / l;
  v->y = (v->y * S3L_FRACTIONS_PER_UNIT) / l;
  v->z = (v->z * S3L_FRACTIONS_PER_UNIT) / l;
}

/** Like S3L_normalizeVec3, but doesn't perform any checks on the input vector,
  which is faster, but can be very innacurate or overflowing. You are supposed
  to provide a "nice" vector (not too big or small). */
static inline void S3L_normalizeVec3Fast(S3L_Vec4 *v)
{
  S3L_Unit l = S3L_vec3Length(*v);

  if (l == 0)
    return;

  v->x = (v->x * S3L_FRACTIONS_PER_UNIT) / l;
  v->y = (v->y * S3L_FRACTIONS_PER_UNIT) / l;
  v->z = (v->z * S3L_FRACTIONS_PER_UNIT) / l;
}

void S3L_initTransform3D(S3L_Transform3D *t)
{
  S3L_initVec4(&(t->translation));
  S3L_initVec4(&(t->rotation));
  t->scale.x = S3L_FRACTIONS_PER_UNIT;
  t->scale.y = S3L_FRACTIONS_PER_UNIT;
  t->scale.z = S3L_FRACTIONS_PER_UNIT;
  t->scale.w = 0;
}

/** Performs perspecive division (z-divide). Does NOT check for division by
  zero. */
static inline void S3L_perspectiveDivide(S3L_Vec4 *vector,
  S3L_Unit focalLength)
{
  vector->x = (vector->x * focalLength) / vector->z;
  vector->y = (vector->y * focalLength) / vector->z;
}

void project3DPointToScreen(
  S3L_Vec4 point,
  S3L_Camera cam,
  S3L_Vec4 *result)
{
  S3L_Mat4 m;
  S3L_Unit s;
  S3L_ScreenCoord x, y;

  S3L_makeCameraMatrix(&cam.transform,&m);

  s = point.w;

  point.w = S3L_FRACTIONS_PER_UNIT;

  S3L_vec3Xmat4(&point,&m);

  point.z = S3L_nonZero(point.z);

  S3L_perspectiveDivide(&point,cam.focalLength);

  S3L_mapProjectionPlaneToScreen(point,&x,&y);

  result->x = x;
  result->y = y;
  result->z = point.z;

  result->w =
    (point.z <= 0) ? 0 :
    (
      (s * cam.focalLength * S3L_RESOLUTION_X) /
        (point.z * S3L_FRACTIONS_PER_UNIT)
    );
}

void S3L_lookAt(S3L_Vec4 pointTo, S3L_Transform3D *t)
{
  S3L_Vec4 v;
  S3L_Unit dx;
  S3L_Unit l;

  v.x = pointTo.x - t->translation.x;
  v.y = pointTo.z - t->translation.z;

  dx = v.x;
  l = S3L_vec2Length(v);

  dx = (v.x * S3L_FRACTIONS_PER_UNIT) / S3L_nonZero(l); // normalize

  t->rotation.y = -1 * S3L_asin(dx);

  if (v.y < 0)
    t->rotation.y = S3L_FRACTIONS_PER_UNIT / 2 - t->rotation.y;

  v.x = pointTo.y - t->translation.y;
  v.y = l;

  l = S3L_vec2Length(v);

  dx = (v.x * S3L_FRACTIONS_PER_UNIT) / S3L_nonZero(l);

  t->rotation.x = S3L_asin(dx);
}

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
  S3L_Transform3D *t)
{
  t->translation.x = tx;
  t->translation.y = ty;
  t->translation.z = tz;

  t->rotation.x = rx;
  t->rotation.y = ry;
  t->rotation.z = rz;

  t->scale.x = sx;
  t->scale.y = sy;
  t->scale.z = sz;
}

void S3L_initCamera(S3L_Camera *cam)
{
  cam->focalLength = S3L_FRACTIONS_PER_UNIT;
  S3L_initTransform3D(&(cam->transform));
}

void S3L_rotationToDirections(
  S3L_Vec4 rotation,
  S3L_Unit length,
  S3L_Vec4 *forw,
  S3L_Vec4 *right,
  S3L_Vec4 *up)
{
  S3L_Mat4 m;

  S3L_makeRotationMatrixZXY(rotation.x,rotation.y,rotation.z,&m);

  if (forw != 0)
  {
    forw->x = 0;
    forw->y = 0;
    forw->z = length;
    S3L_vec3Xmat4(forw,&m);
  }

  if (right != 0)
  {
    right->x = length;
    right->y = 0;
    right->z = 0;
    S3L_vec3Xmat4(right,&m);
  }

  if (up != 0)
  {
    up->x = 0;
    up->y = length;
    up->z = 0;
    S3L_vec3Xmat4(up,&m);
  }
}

static inline void S3L_initPixelInfo(S3L_PixelInfo *p)
{
  p->x = 0;
  p->y = 0;
  p->barycentric[0] = S3L_FRACTIONS_PER_UNIT;
  p->barycentric[1] = 0;
  p->barycentric[2] = 0;
  p->depth = 0;
  p->previousZ = 0;
}

void (*S3L_pixelFunction)(void *pixel) = NULL;

/** Serves to accelerate linear interpolation for performance-critical
  code. Functions such as S3L_interpolate require division to compute each
  interpolated value, while S3L_FastLerpState only requires a division for
  the initiation and a shift for retrieving each interpolated value.

  S3L_FastLerpState stores a value and a step, both scaled (shifted by
  S3L_FAST_LERP_QUALITY) to increase precision. The step is being added to the
  value, which achieves the interpolation. This will only be useful for
  interpolations in which we need to get the interpolated value in every step.

  BEWARE! Shifting a negative value is undefined, so handling shifting of
  negative values has to be done cleverly. */
typedef struct
{
  S3L_Unit valueScaled;
  S3L_Unit stepScaled;
} S3L_FastLerpState;

#define S3L_getFastLerpValue(state)\
  (state.valueScaled >> S3L_FAST_LERP_QUALITY)

#define S3L_stepFastLerp(state)\
  state.valueScaled += state.stepScaled

/** Returns a value interpolated between the three triangle vertices based on
  barycentric coordinates. */
S3L_Unit S3L_interpolateBarycentric(
  S3L_Unit value0,
  S3L_Unit value1,
  S3L_Unit value2,
  S3L_Unit barycentric[3])
{
  return
    (
      (value0 * barycentric[0]) +
      (value1 * barycentric[1]) +
      (value2 * barycentric[2])
    ) / S3L_FRACTIONS_PER_UNIT;
}

static inline void S3L_mapProjectionPlaneToScreen(
  S3L_Vec4 point,
  S3L_ScreenCoord *screenX,
  S3L_ScreenCoord *screenY)
{
  *screenX =
    S3L_HALF_RESOLUTION_X +
    (point.x * S3L_HALF_RESOLUTION_X) / S3L_FRACTIONS_PER_UNIT;

  *screenY =
    S3L_HALF_RESOLUTION_Y -
    (point.y * S3L_HALF_RESOLUTION_X) / S3L_FRACTIONS_PER_UNIT;
}

void S3L_zBufferClear(void)
{
#if S3L_Z_BUFFER
  for (UINT32 i = 0; i < S3L_RESOLUTION_X * S3L_RESOLUTION_Y; ++i)
    S3L_zBuffer[i] = S3L_MAX_DEPTH;
#endif
}

void S3L_stencilBufferClear(void)
{
#if S3L_STENCIL_BUFFER
  for (UINT32 i = 0; i < S3L_STENCIL_BUFFER_SIZE; ++i)
    S3L_stencilBuffer[i] = 0;
#endif
}

void S3L_newFrame(void)
{
  S3L_zBufferClear();
  S3L_stencilBufferClear();
}

void S3L_drawTriangle(
  S3L_Vec4 point0,
  S3L_Vec4 point1,
  S3L_Vec4 point2)
{
  S3L_PixelInfo p;

  S3L_Vec4 *tPointSS, *lPointSS, *rPointSS; /* points in Screen Space (in
                                               S3L_Units, normalized by
                                               S3L_FRACTIONS_PER_UNIT) */

  S3L_Unit *barycentric0; // bar. coord that gets higher from L to R
  S3L_Unit *barycentric1; // bar. coord that gets higher from R to L
  S3L_Unit *barycentric2; // bar. coord that gets higher from bottom up

  S3L_ScreenCoord splitY; // Y of the vertically middle point of the triangle
  S3L_ScreenCoord endY;   // bottom Y of the whole triangle
  INT32 splitOnLeft;      /* whether splitY is the y coord. of left or right
                             point */

  S3L_ScreenCoord currentY;

  INT16
    /* triangle side:
    left     right */
    lX,      rX,       // current x position on the screen
    lDx,     rDx,      // dx (end point - start point)
    lDy,     rDy,      // dy (end point - start point)
    lInc,    rInc,     // direction in which to increment (1 or -1)
    lErr,    rErr,     // current error (Bresenham)
    lErrCmp, rErrCmp,  // helper for deciding comparison (> vs >=)
    lErrAdd, rErrAdd,  // error value to add in each Bresenham cycle
    lErrSub, rErrSub;  // error value to substract when moving in x direction

  S3L_FastLerpState lSideFLS, rSideFLS;

#if S3L_COMPUTE_LERP_DEPTH
  S3L_FastLerpState lDepthFLS, rDepthFLS;
#endif

#if S3L_PERSPECTIVE_CORRECTION != 0
  S3L_Unit
    tPointRecipZ, lPointRecipZ, rPointRecipZ, /* Reciprocals of the depth of
                                                 each triangle point. */
    lRecip0, lRecip1, rRecip0, rRecip1;       /* Helper variables for swapping
                                                 the above after split. */
#endif

  S3L_initPixelInfo(&p);

  // sort the vertices:

  #define assignPoints(t,a,b)\
    {\
      tPointSS = &point##t;\
      barycentric2 = &(p.barycentric[t]);\
      if (S3L_triangleWinding(point##t.x,point##t.y,point##a.x,point##a.y,\
        point##b.x,point##b.y) >= 0)\
      {\
        lPointSS = &point##a; rPointSS = &point##b;\
        barycentric0 = &(p.barycentric[b]);\
        barycentric1 = &(p.barycentric[a]);\
      }\
      else\
      {\
        lPointSS = &point##b; rPointSS = &point##a;\
        barycentric0 = &(p.barycentric[a]);\
        barycentric1 = &(p.barycentric[b]);\
      }\
    }

  if (point0.y <= point1.y)
  {
    if (point0.y <= point2.y)
      assignPoints(0,1,2)
    else
      assignPoints(2,0,1)
  }
  else
  {
    if (point1.y <= point2.y)
      assignPoints(1,0,2)
    else
      assignPoints(2,0,1)
  }

  #undef assignPoints

#if S3L_FLAT
  *barycentric0 = S3L_FRACTIONS_PER_UNIT / 3;
  *barycentric1 = S3L_FRACTIONS_PER_UNIT / 3;
  *barycentric2 = S3L_FRACTIONS_PER_UNIT - 2 * (S3L_FRACTIONS_PER_UNIT / 3);
#endif

  p.triangleSize[0] = rPointSS->x - lPointSS->x;
  p.triangleSize[1] =
    (rPointSS->y > lPointSS->y ? rPointSS->y : lPointSS->y) - tPointSS->y;

  // now draw the triangle line by line:

  if (rPointSS->y <= lPointSS->y)
  {
    splitY = rPointSS->y;
    splitOnLeft = 0;
    endY = lPointSS->y;
  }
  else
  {
    splitY = lPointSS->y;
    splitOnLeft = 1;
    endY = rPointSS->y;
  }

  currentY = tPointSS->y;

  /* We'll be using an algorithm similar to Bresenham line algorithm. The
     specifics of this algorithm are among others:

     - drawing possibly NON-CONTINUOUS line
     - NOT tracing the line exactly, but rather rasterizing one the right
       side of it, according to the pixel CENTERS, INCLUDING the pixel
       centers

     The principle is this:

     - Move vertically by pixels and accumulate the error (abs(dx/dy)).
     - If the error is greater than one (crossed the next pixel center), keep
       moving horizontally and substracting 1 from the error until it is less
       than 1 again.
     - To make this INTEGER ONLY, scale the case so that distance between
       pixels is equal to dy (instead of 1). This way the error becomes
       dx/dy * dy == dx, and we're comparing the error to (and potentially
       substracting) 1 * dy == dy. */

#if S3L_COMPUTE_LERP_DEPTH
  #define initDepthFLS(s,p1,p2)\
    s##DepthFLS.valueScaled = p1##PointSS->z << S3L_FAST_LERP_QUALITY;\
    s##DepthFLS.stepScaled = ((p2##PointSS->z << S3L_FAST_LERP_QUALITY) -\
      s##DepthFLS.valueScaled) / (s##Dy != 0 ? s##Dy : 1);
#else
  #define initDepthFLS(s,p1,p2) ;
#endif

  /* init side for the algorithm, params:
     s - which side (l or r)
     p1 - point from (t, l or r)
     p2 - point to (t, l or r)
     down - whether the side coordinate goes top-down or vice versa */
  #define initSide(s,p1,p2,down)\
    s##X = p1##PointSS->x;\
    s##Dx = p2##PointSS->x - p1##PointSS->x;\
    s##Dy = p2##PointSS->y - p1##PointSS->y;\
    initDepthFLS(s,p1,p2)\
    s##SideFLS.stepScaled = (S3L_FRACTIONS_PER_UNIT << S3L_FAST_LERP_QUALITY)\
                      / (s##Dy != 0 ? s##Dy : 1);\
    s##SideFLS.valueScaled = 0;\
    if (!down)\
    {\
      s##SideFLS.valueScaled =\
        S3L_FRACTIONS_PER_UNIT << S3L_FAST_LERP_QUALITY;\
      s##SideFLS.stepScaled *= -1;\
    }\
    s##Inc = s##Dx >= 0 ? 1 : -1;\
    if (s##Dx < 0)\
      {s##Err = 0;     s##ErrCmp = 0;}\
    else\
      {s##Err = s##Dy; s##ErrCmp = 1;}\
    s##ErrAdd = S3L_abs(s##Dx);\
    s##ErrSub = s##Dy != 0 ? s##Dy : 1; /* don't allow 0, could lead to an
                                           infinite substracting loop */

  #define stepSide(s)\
    while (s##Err - s##Dy >= s##ErrCmp)\
    {\
      s##X += s##Inc;\
      s##Err -= s##ErrSub;\
    }\
    s##Err += s##ErrAdd;

  initSide(r,t,r,1)
  initSide(l,t,l,1)

#if S3L_PERSPECTIVE_CORRECTION != 0
  /* PC is done by linearly interpolating reciprocals from which the corrected
     velues can be computed. See
     http://www.lysator.liu.se/~mikaelk/doc/perspectivetexture/ */

  #if S3L_PERSPECTIVE_CORRECTION == 1
    #define Z_RECIP_NUMERATOR\
      (S3L_FRACTIONS_PER_UNIT * S3L_FRACTIONS_PER_UNIT * S3L_FRACTIONS_PER_UNIT)
  #elif S3L_PERSPECTIVE_CORRECTION == 2
    #define Z_RECIP_NUMERATOR\
      (S3L_FRACTIONS_PER_UNIT * S3L_FRACTIONS_PER_UNIT)
  #endif
  /* ^ This numerator is a number by which we divide values for the
     reciprocals. For PC == 2 it has to be lower because linear interpolation
     scaling would make it overflow -- this results in lower depth precision
     in bigger distance for PC == 2. */

  tPointRecipZ = Z_RECIP_NUMERATOR / S3L_nonZero(tPointSS->z);
  lPointRecipZ = Z_RECIP_NUMERATOR / S3L_nonZero(lPointSS->z);
  rPointRecipZ = Z_RECIP_NUMERATOR / S3L_nonZero(rPointSS->z);

  lRecip0 = tPointRecipZ;
  lRecip1 = lPointRecipZ;
  rRecip0 = tPointRecipZ;
  rRecip1 = rPointRecipZ;

  #define manageSplitPerspective(b0,b1)\
    b1##Recip0 = b0##PointRecipZ;\
    b1##Recip1 = b1##PointRecipZ;\
    b0##Recip0 = b0##PointRecipZ;\
    b0##Recip1 = tPointRecipZ;
#else
  #define manageSplitPerspective(b0,b1) ;
#endif

  // clip to the screen in y dimension:

  endY = S3L_min(endY,S3L_viewWindow[S3L_VIEW_WINDOW_Y2]);

  /* Clipping above the screen (y < 0) can't be easily done here, will be
     handled inside the loop. */

  while (currentY < endY)   /* draw the triangle from top to bottom -- the
                               bottom-most row is left out because, following
                               from the rasterization rules (see start of the
                               file), it is to never be rasterized. */
  {
    if (currentY == splitY) // reached a vertical split of the triangle?
    {
      S3L_Unit *tmp;

      #define manageSplit(b0,b1,s0,s1)\
        tmp = barycentric##b0;\
        barycentric##b0 = barycentric##b1;\
        barycentric##b1 = tmp;\
        s0##SideFLS.valueScaled = (S3L_FRACTIONS_PER_UNIT\
           << S3L_FAST_LERP_QUALITY) - s0##SideFLS.valueScaled;\
        s0##SideFLS.stepScaled *= -1;\
        manageSplitPerspective(s0,s1)

      if (splitOnLeft)
      {
        initSide(l,l,r,0);
        manageSplit(0,2,r,l)
      }
      else
      {
        initSide(r,r,l,0);
        manageSplit(1,2,l,r)
      }
    }

    stepSide(r)
    stepSide(l)

    if (currentY >= S3L_viewWindow[S3L_VIEW_WINDOW_Y1]) /* clipping of pixels whose y < 0 (can't be easily done
                                                           outside the loop because of the Bresenham-like
                                                           algorithm steps) */
    {
      // draw the horizontal line
      S3L_ScreenCoord rXClipped, lXClipped;

#if S3L_PERSPECTIVE_CORRECTION != 0
      S3L_ScreenCoord i;
#endif

#if S3L_PERSPECTIVE_CORRECTION == 2
      S3L_FastLerpState
        depthPC, // interpolates depth between row segments
        b0PC,    // interpolates barycentric0 between row segments
        b1PC;    // interpolates barycentric1 between row segments

      /* ^ These interpolate values between row segments (lines of pixels
           of S3L_PC_APPROX_LENGTH length). After each row segment perspective
           correction is recomputed. */

      SINT8 rowCount;
#endif

#if S3L_Z_BUFFER
      UINT32 zBufferIndex;
#endif

#if !S3L_FLAT
      S3L_Unit rowLength = S3L_nonZero(rX - lX - 1); // prevent zero div

  #if S3L_PERSPECTIVE_CORRECTION != 0
      S3L_Unit lOverZ, lRecipZ, rOverZ, rRecipZ, lT, rT;

      lT = S3L_getFastLerpValue(lSideFLS);
      rT = S3L_getFastLerpValue(rSideFLS);

      lOverZ  = S3L_interpolateByUnitFrom0(lRecip1,lT);
      lRecipZ = S3L_interpolateByUnit(lRecip0,lRecip1,lT);

      rOverZ  = S3L_interpolateByUnitFrom0(rRecip1,rT);
      rRecipZ = S3L_interpolateByUnit(rRecip0,rRecip1,rT);
  #else
      S3L_FastLerpState b0FLS, b1FLS;

    #if S3L_COMPUTE_LERP_DEPTH
      S3L_FastLerpState  depthFLS;

      depthFLS.valueScaled = lDepthFLS.valueScaled;
      depthFLS.stepScaled =
        (rDepthFLS.valueScaled - lDepthFLS.valueScaled) / rowLength;
    #endif

      b0FLS.valueScaled = 0;
      b1FLS.valueScaled = lSideFLS.valueScaled;

      b0FLS.stepScaled = rSideFLS.valueScaled / rowLength;
      b1FLS.stepScaled = -1 * lSideFLS.valueScaled / rowLength;
  #endif
#endif

      p.y = currentY;

      // clip to the screen in x dimension:

      rXClipped = S3L_min(rX,S3L_RESOLUTION_X);
      lXClipped = lX;

      if (lXClipped < 0)
      {
        lXClipped = 0;

#if !S3L_PERSPECTIVE_CORRECTION && !S3L_FLAT
        b0FLS.valueScaled -= lX * b0FLS.stepScaled;
        b1FLS.valueScaled -= lX * b1FLS.stepScaled;

  #if S3L_COMPUTE_LERP_DEPTH
        depthFLS.valueScaled -= lX * depthFLS.stepScaled;
  #endif
#endif
      }

#if S3L_PERSPECTIVE_CORRECTION != 0
      i = lXClipped - lX;  /* helper var to save one
                              substraction in the inner
                              loop */
#endif

#if S3L_PERSPECTIVE_CORRECTION == 2
      depthPC.valueScaled =
        (Z_RECIP_NUMERATOR /
        S3L_nonZero(S3L_interpolate(lRecipZ,rRecipZ,i,rowLength)))
        << S3L_FAST_LERP_QUALITY;

       b0PC.valueScaled =
           (
             S3L_interpolateFrom0(rOverZ,i,rowLength)
             * depthPC.valueScaled
           ) / (Z_RECIP_NUMERATOR / S3L_FRACTIONS_PER_UNIT);

       b1PC.valueScaled =
           (
             (lOverZ - S3L_interpolateFrom0(lOverZ,i,rowLength))
             * depthPC.valueScaled
           ) / (Z_RECIP_NUMERATOR / S3L_FRACTIONS_PER_UNIT);

      rowCount = S3L_PC_APPROX_LENGTH;
#endif

#if S3L_Z_BUFFER
      zBufferIndex = p.y * S3L_RESOLUTION_X + lXClipped;
#endif

      // draw the row -- inner loop:

      for (S3L_ScreenCoord x = lXClipped; x < rXClipped; ++x)
      {
        SINT8 testsPassed = 1;

        if (x < S3L_spanPortalClip[0] || x > S3L_spanPortalClip[1])
        {
#if S3L_PERSPECTIVE_CORRECTION == 1
          continue;
#else
          testsPassed = 0;
#endif
        }

        if (testsPassed && S3L_spanFloorClip && S3L_spanCeilingClip)
        {
          if (p.y >= S3L_spanFloorClip[x])
#if S3L_PERSPECTIVE_CORRECTION == 1
            continue;
#else
            testsPassed = 0;
#endif
          if (p.y <= S3L_spanCeilingClip[x])
#if S3L_PERSPECTIVE_CORRECTION == 1
            continue;
#else
            testsPassed = 0;
#endif
        }

#if S3L_STENCIL_BUFFER
        if (testsPassed && !S3L_stencilTest(x,p.y))
          testsPassed = 0;
#endif
        p.x = x;

#if S3L_COMPUTE_DEPTH
  #if S3L_PERSPECTIVE_CORRECTION == 1
        p.depth = Z_RECIP_NUMERATOR /
          S3L_nonZero(S3L_interpolate(lRecipZ,rRecipZ,i,rowLength));
  #elif S3L_PERSPECTIVE_CORRECTION == 2
        if (rowCount >= S3L_PC_APPROX_LENGTH)
        {
          // init the linear interpolation to the next PC correct value

          S3L_Unit nextI = i + S3L_PC_APPROX_LENGTH;

          rowCount = 0;

          if (nextI < rowLength)
          {
            S3L_Unit nextValue, nextDepthScaled =
              (
              Z_RECIP_NUMERATOR /
              S3L_nonZero(S3L_interpolate(lRecipZ,rRecipZ,nextI,rowLength))
              ) << S3L_FAST_LERP_QUALITY;

            depthPC.stepScaled =
              (nextDepthScaled - depthPC.valueScaled) / S3L_PC_APPROX_LENGTH;

            nextValue =
             (
               S3L_interpolateFrom0(rOverZ,nextI,rowLength)
               * nextDepthScaled
             ) / (Z_RECIP_NUMERATOR / S3L_FRACTIONS_PER_UNIT);

            b0PC.stepScaled =
              (nextValue - b0PC.valueScaled) / S3L_PC_APPROX_LENGTH;

            nextValue =
             (
               (lOverZ - S3L_interpolateFrom0(lOverZ,nextI,rowLength))
               * nextDepthScaled
             ) / (Z_RECIP_NUMERATOR / S3L_FRACTIONS_PER_UNIT);

            b1PC.stepScaled =
              (nextValue - b1PC.valueScaled) / S3L_PC_APPROX_LENGTH;
          }
          else
          {
            /* A special case where we'd be interpolating outside the triangle.
               It seems like a valid approach at first, but it creates a bug
               in a case when the rasaterized triangle is near screen 0 and can
               actually never reach the extrapolated screen position. So we
               have to clamp to the actual end of the triangle here. */

            S3L_Unit maxI = S3L_nonZero(rowLength - i);

            S3L_Unit nextValue, nextDepthScaled =
              (
              Z_RECIP_NUMERATOR /
              S3L_nonZero(rRecipZ)
              ) << S3L_FAST_LERP_QUALITY;

            depthPC.stepScaled =
              (nextDepthScaled - depthPC.valueScaled) / maxI;

            nextValue =
             (
               rOverZ
               * nextDepthScaled
             ) / (Z_RECIP_NUMERATOR / S3L_FRACTIONS_PER_UNIT);

            b0PC.stepScaled =
              (nextValue - b0PC.valueScaled) / maxI;

            b1PC.stepScaled =
              -1 * b1PC.valueScaled / maxI;
          }
        }

        p.depth = S3L_getFastLerpValue(depthPC);
  #else
        p.depth = S3L_getFastLerpValue(depthFLS);
        S3L_stepFastLerp(depthFLS);
  #endif
#else   // !S3L_COMPUTE_DEPTH
        p.depth = (tPointSS->z + lPointSS->z + rPointSS->z) / 3;
#endif

#if S3L_Z_BUFFER
        p.previousZ = S3L_zBuffer[zBufferIndex];

        zBufferIndex++;

        if (testsPassed && (rsp_target.read & RSP_READ_DEPTH) && !S3L_zTest(p.x,p.y,p.depth))
          testsPassed = 0;
#endif

        if (testsPassed)
        {
#if !S3L_FLAT
  #if S3L_PERSPECTIVE_CORRECTION == 0
          *barycentric0 = S3L_getFastLerpValue(b0FLS);
          *barycentric1 = S3L_getFastLerpValue(b1FLS);
  #elif S3L_PERSPECTIVE_CORRECTION == 1
          *barycentric0 =
           (
             S3L_interpolateFrom0(rOverZ,i,rowLength)
             * p.depth
           ) / (Z_RECIP_NUMERATOR / S3L_FRACTIONS_PER_UNIT);

          *barycentric1 =
           (
             (lOverZ - S3L_interpolateFrom0(lOverZ,i,rowLength))
             * p.depth
           ) / (Z_RECIP_NUMERATOR / S3L_FRACTIONS_PER_UNIT);
  #elif S3L_PERSPECTIVE_CORRECTION == 2
          *barycentric0 = S3L_getFastLerpValue(b0PC);
          *barycentric1 = S3L_getFastLerpValue(b1PC);
  #endif

          *barycentric2 =
            S3L_FRACTIONS_PER_UNIT - *barycentric0 - *barycentric1;
#endif
          S3L_pixelFunction((void *)(&p));
        } // tests passed

#if !S3L_FLAT
  #if S3L_PERSPECTIVE_CORRECTION != 0
          i++;
    #if S3L_PERSPECTIVE_CORRECTION == 2
          rowCount++;

          S3L_stepFastLerp(depthPC);
          S3L_stepFastLerp(b0PC);
          S3L_stepFastLerp(b1PC);
    #endif
  #else
          S3L_stepFastLerp(b0FLS);
          S3L_stepFastLerp(b1FLS);
  #endif
#endif
      } // inner loop
    } // y clipping

#if !S3L_FLAT
    S3L_stepFastLerp(lSideFLS);
    S3L_stepFastLerp(rSideFLS);

  #if S3L_COMPUTE_LERP_DEPTH
    S3L_stepFastLerp(lDepthFLS);
    S3L_stepFastLerp(rDepthFLS);
  #endif
#endif

    ++currentY;
  } // row drawing

  #undef manageSplit
  #undef initPC
  #undef initSide
  #undef stepSide
  #undef Z_RECIP_NUMERATOR
}

static inline void S3L_rotate2DPoint(S3L_Unit *x, S3L_Unit *y, S3L_Unit angle)
{
  S3L_Unit angleSin;
  S3L_Unit angleCos;

  S3L_Unit xBackup;

  if (angle < S3L_SIN_TABLE_UNIT_STEP)
    return; // no visible rotation

  angleSin = S3L_sin(angle);
  angleCos = S3L_cos(angle);

  xBackup = *x;

  *x =
    (angleCos * (*x)) / S3L_FRACTIONS_PER_UNIT -
    (angleSin * (*y)) / S3L_FRACTIONS_PER_UNIT;

  *y =
    (angleSin * xBackup) / S3L_FRACTIONS_PER_UNIT +
    (angleCos * (*y)) / S3L_FRACTIONS_PER_UNIT;
}

void S3L_makeWorldMatrix(S3L_Transform3D *worldTransform, S3L_Mat4 *m)
{
  S3L_Mat4 t;

  S3L_makeScaleMatrix(
    worldTransform->scale.x,
    worldTransform->scale.y,
    worldTransform->scale.z,
    m
  );

#if S3L_PRECISE_ANGLES != 0
  S3L_makeRotationMatrixZXYPrecise(
#else
  S3L_makeRotationMatrixZXY(
#endif
    worldTransform->rotation.x,
    worldTransform->rotation.y,
    worldTransform->rotation.z,
    &t);

  S3L_mat4Xmat4(m,&t);

  S3L_makeTranslationMat(
    worldTransform->translation.x,
    worldTransform->translation.y,
    worldTransform->translation.z,
    &t);

  S3L_mat4Xmat4(m,&t);
}

void S3L_transposeMat4(S3L_Mat4 *m)
{
  S3L_Unit tmp;

  for (UINT8 y = 0; y < 3; ++y)
    for (UINT8 x = 1 + y; x < 4; ++x)
    {
      tmp = (*m)[x][y];
      (*m)[x][y] = (*m)[y][x];
      (*m)[y][x] = tmp;
    }
}

void S3L_makeCameraMatrix(S3L_Transform3D *cameraTransform, S3L_Mat4 *m)
{
  S3L_Mat4 r;

  S3L_makeTranslationMat(
    -1 * cameraTransform->translation.x,
    -1 * cameraTransform->translation.y,
    -1 * cameraTransform->translation.z,
    m);

#if S3L_PRECISE_ANGLES != 0
  S3L_makeRotationMatrixZXYPrecise(
#else
  S3L_makeRotationMatrixZXY(
#endif
    cameraTransform->rotation.x,
    cameraTransform->rotation.y,
    cameraTransform->rotation.z,
    &r);

  S3L_transposeMat4(&r); // transposing creates an inverse transform

  S3L_mat4Xmat4(m,&r);

  S3L_makeScaleMatrix(
    cameraTransform->scale.x,
    cameraTransform->scale.y,
    cameraTransform->scale.z,
    &r
  );

  S3L_mat4Xmat4(m,&r);
}

static inline SINT8 S3L_triangleWinding(
  S3L_ScreenCoord x0,
  S3L_ScreenCoord y0,
  S3L_ScreenCoord x1,
  S3L_ScreenCoord y1,
  S3L_ScreenCoord x2,
  S3L_ScreenCoord y2)
{
  INT32 winding =
    (y1 - y0) * (x2 - x1) - (x1 - x0) * (y2 - y1);
    // ^ cross product for points with z == 0

  return winding > 0 ? 1 : (winding < 0 ? -1 : 0);
}

/**
  Checks if given triangle (in Screen Space) is at least partially visible,
  i.e. returns false if the triangle is either completely outside the frustum
  (left, right, top, bottom, near) or is invisible due to backface culling.
*/
SINT8 S3L_triangleIsVisible(
  S3L_Vec4 p0,
  S3L_Vec4 p1,
  S3L_Vec4 p2,
  UINT8 backfaceCulling)
{
  #define clipTest(c,cmp,v)\
    (p0.c cmp (v) && p1.c cmp (v) && p2.c cmp (v))

  if ( // outside frustum?
#if S3L_NEAR_CROSS_STRATEGY == 0
      p0.z <= S3L_NEAR || p1.z <= S3L_NEAR || p2.z <= S3L_NEAR
      // ^ partially in front of NEAR?
#else
      clipTest(z,<=,S3L_NEAR) // completely in front of NEAR?
#endif
#if 0
      || clipTest(x-S3L_screenVertexOffsetX,<,S3L_viewWindow[S3L_VIEW_WINDOW_X1]) ||
         clipTest(x-S3L_screenVertexOffsetX,>=,S3L_viewWindow[S3L_VIEW_WINDOW_X2]) ||
         clipTest(y-S3L_screenVertexOffsetY,<,S3L_viewWindow[S3L_VIEW_WINDOW_Y1]) ||
         clipTest(y-S3L_screenVertexOffsetY,>,S3L_viewWindow[S3L_VIEW_WINDOW_Y2])
#endif
    )
    return 0;

  #undef clipTest

  if (backfaceCulling != 0)
  {
    SINT8 winding =
      S3L_triangleWinding(p0.x,p0.y,p1.x,p1.y,p2.x,p2.y);

    if ((backfaceCulling == 1 && winding > 0) ||
        (backfaceCulling == 2 && winding < 0))
      return 0;
  }

  return 1;
}

static void _S3L_projectVertex(
  const rsp_vertex_t *vertex,
  S3L_Mat4 *projectionMatrix,
  S3L_Vec4 *result)
{
  result->x = vertex->position.x;
  result->y = vertex->position.y;
  result->z = vertex->position.z;
  result->w = S3L_FRACTIONS_PER_UNIT; // needed for translation

  S3L_vec3Xmat4(result,projectionMatrix);

  result->w = result->z;
  /* We'll keep the non-clamped z in w for sorting. */
}

static void _S3L_mapProjectedVertexToScreen(S3L_Vec4 *vertex, S3L_Unit focalLength)
{
  S3L_ScreenCoord sX, sY;

  vertex->z = vertex->z >= S3L_NEAR ? vertex->z : S3L_NEAR;
  /* ^ This firstly prevents zero division in the follwoing z-divide and
    secondly "pushes" vertices that are in front of near a little bit forward,
    which makes them behave a bit better. If all three vertices end up exactly
    on NEAR, the triangle will be culled. */

  S3L_perspectiveDivide(vertex,focalLength);

  S3L_mapProjectionPlaneToScreen(*vertex,&sX,&sY);

  vertex->x = sX + S3L_screenVertexOffsetX;
  vertex->y = sY + S3L_screenVertexOffsetY;
}

/**
  Projects a triangle to the screen. If enabled, a triangle can be potentially
  subdivided into two if it crosses the near plane, in which case two projected
  triangles are returned (return value will be 1).
*/
UINT8 S3L_projectTriangle(
  const void *tri,
  S3L_Mat4 *matrix,
  UINT32 focalLength,
  S3L_Vec4 transformed[6])
{
  const rsp_triangle_t *triangle = tri;
  UINT8 result = 0;

#if S3L_NEAR_CROSS_STRATEGY == 2
  UINT8 infront = 0;
  UINT8 behind = 0;
  UINT8 infrontI[3];
  UINT8 behindI[3];

  S3L_Unit ratio;
#endif

  _S3L_projectVertex(&triangle->vertices[0],matrix,&(transformed[0]));
  _S3L_projectVertex(&triangle->vertices[1],matrix,&(transformed[1]));
  _S3L_projectVertex(&triangle->vertices[2],matrix,&(transformed[2]));

#if S3L_NEAR_CROSS_STRATEGY == 2
  for (UINT8 i = 0; i < 3; ++i)
    if (transformed[i].z < S3L_NEAR)
    {
      infrontI[infront] = i;
      infront++;
    }
    else
    {
      behindI[behind] = i;
      behind++;
    }

#define interpolateVertex \
  ratio =\
    ((transformed[be].z - S3L_NEAR) * S3L_FRACTIONS_PER_UNIT) /\
    (transformed[be].z - transformed[in].z);\
  transformed[in].x = transformed[be].x - \
    ((transformed[be].x - transformed[in].x) * ratio) /\
      S3L_FRACTIONS_PER_UNIT;\
  transformed[in].y = transformed[be].y -\
    ((transformed[be].y - transformed[in].y) * ratio) /\
      S3L_FRACTIONS_PER_UNIT;\
  transformed[in].z = S3L_NEAR;

  if (infront == 2)
  {
    // shift the two vertices forward along the edge
    for (UINT8 i = 0; i < 2; ++i)
    {
      UINT8 be = behindI[0], in = infrontI[i];

      interpolateVertex
    }
  }
  else if (infront == 1)
  {
    // create another triangle and do the shifts
    transformed[3] = transformed[behindI[1]];
    transformed[4] = transformed[infrontI[0]];
    transformed[5] = transformed[infrontI[0]];

    for (UINT8 i = 0; i < 2; ++i)
    {
      UINT8 be = behindI[i], in = i + 4;

      interpolateVertex
    }

    transformed[infrontI[0]] = transformed[4];

    _S3L_mapProjectedVertexToScreen(&transformed[3],focalLength);
    _S3L_mapProjectedVertexToScreen(&transformed[4],focalLength);
    _S3L_mapProjectedVertexToScreen(&transformed[5],focalLength);

    result = 1;
  }

#undef interpolateVertex
#endif // S3L_NEAR_CROSS_STRATEGY == 2

  _S3L_mapProjectedVertexToScreen(&transformed[0],focalLength);
  _S3L_mapProjectedVertexToScreen(&transformed[1],focalLength);
  _S3L_mapProjectedVertexToScreen(&transformed[2],focalLength);

  return result;
}
