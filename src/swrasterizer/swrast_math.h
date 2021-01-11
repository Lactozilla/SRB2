/*
	Simple realtime 3D software rasterization renderer. It is fast, focused on
	resource-limited computers, with no dependencies, using only 32bit integer arithmetic.

	author: Miloslav Ciz
	license: CC0 1.0 (public domain) found at https://creativecommons.org/publicdomain/zero/1.0/ + additional waiver of all IP
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

#include "../doomtype.h"
#include "../doomstat.h"

#ifndef _SWRAST_MATH_H_
#define _SWRAST_MATH_H_

#include "swrast_defs.h"
#include "swrast_types.h"

void SWRast_InitVec4(SWRast_Vec4 *v);
void SWRast_InitAngVec4(SWRast_AngVec4 *v);
void SWRast_SetVec4(SWRast_Vec4 *v, fixed_t x, fixed_t y, fixed_t z, fixed_t w);
void SWRast_SetAngVec4(SWRast_AngVec4 *v, angle_t x, angle_t y, angle_t z, angle_t w);
void SWRast_Vec3Add(SWRast_Vec4 *result, SWRast_Vec4 added);
void SWRast_Vec3Sub(SWRast_Vec4 *result, SWRast_Vec4 subtracted);
void SWRast_InitMat4(SWRast_Mat4 *m);
void SWRast_InitTransform3D(SWRast_Transform3D *t);

// Performs perspecive division (z-divide). Does NOT check for division by zero.
void SWRast_PerspectiveDivide(SWRast_Vec4 *vector);

// Normalizes a vector. Note that this function tries to normalize correctly rather than quickly!
// If you need to normalize quickly, do it yourself in a way that best fits your case.
void SWRast_NormalizeVec3(SWRast_Vec4 *v);

// Normalizes a vector. Does the same as SWRast_NormalizeVec3, but with floating point math. Prevents overflows.
void SWRast_NormalizeVec3_Safe(SWRast_Vec4 *v);

// Calculates the cross product of two vectors.
void SWRast_CrossProduct(SWRast_Vec4 *a, SWRast_Vec4 *b, SWRast_Vec4 *result);

// Calculates the length of a vector.
fixed_t SWRast_Vec3Length(SWRast_Vec4 *v);

// Calculates the dot product of a vector.
fixed_t SWRast_DotProductVec3(SWRast_Vec4 *a, SWRast_Vec4 *b);

// Computes a reflection direction (typically used e.g. for specular component
// in Phong illumination). The input vectors must be normalized. The result will
// be normalized as well.
void SWRast_Reflect(SWRast_Vec4 *toLight, SWRast_Vec4 *normal, SWRast_Vec4 *result);

// Transposes a matrix.
void SWRast_TransposeMat4(SWRast_Mat4 *m);

// Makes a translation matrix.
void SWRast_MakeTranslationMatrix(fixed_t offsetX, fixed_t offsetY, fixed_t offsetZ, SWRast_Mat4 *m);

// Makes a scaling matrix.
void SWRast_MakeScaleMatrix(fixed_t scaleX, fixed_t scaleY, fixed_t scaleZ, SWRast_Mat4 *m);

// Makes a matrix for rotation in the ZXY order.
void SWRast_MakeRotationMatrixZXY(angle_t byX, angle_t byY, angle_t byZ, SWRast_Mat4 *m);

// Used to build model and view projection matrices.
void SWRast_MakeModelMatrix(SWRast_Transform3D *worldTransform, SWRast_Mat4 *m);
void SWRast_MakeViewProjectionMatrix(SWRast_Transform3D *cameraTransform, SWRast_Mat4 *m);

// Transforms a projected point into window space.
void SWRast_MapProjectionPlaneToScreen(SWRast_Vec4 *point, INT16 *screenX, INT16 *screenY);

// Multiplies a vector by a matrix with normalization by
// FRACUNIT. Result is stored in the input vector.
void SWRast_MultVec4Mat4(SWRast_Vec4 *v, SWRast_Mat4 *m);

// Same as SWRast_MultVec4Mat4 but faster, because this version doesn't compute the
// W component of the result, which is usually not needed.
void SWRast_MultVec3Mat4(SWRast_Vec4 *v, SWRast_Mat4 *m);

// Multiplies two matrices with normalization by FRACUNIT.
// Result is stored in the first matrix. The result represents a transformation
// that has the same effect as applying the transformation represented by m1 and
// then m2 (in that order).
void SWRast_MultiplyMatrices(SWRast_Mat4 *m1, SWRast_Mat4 *m2);

// general helper functions
fixed_t SWRast_abs(fixed_t value);
fixed_t SWRast_min(fixed_t v1, fixed_t v2);
fixed_t SWRast_max(fixed_t v1, fixed_t v2);
fixed_t SWRast_clamp(fixed_t v, fixed_t v1, fixed_t v2);
fixed_t SWRast_wrap(fixed_t value, fixed_t mod);
fixed_t SWRast_NonZero(fixed_t value);
fixed_t SWRast_zeroClamp(fixed_t value);

// Projects a single point from 3D space to the screen space (pixels), which
// can be useful e.g. for drawing sprites. The w component of input and result
// holds the point size. If this size is 0 in the result, the sprite is outside
// the view.
void SWRast_Project3DPointToScreen(SWRast_Vec4 *point, SWRast_Transform3D *transform, SWRast_Vec4 *result);

// Computes a normalized normal of given triangle.
void SWRast_TriangleNormal(SWRast_Vec4 *t0, SWRast_Vec4 *t1, SWRast_Vec4 *t2, SWRast_Vec4 *n);

// Interpolated between two values, v1 and v2, in the same ratio as t is to tMax.
// Does NOT prevent zero division.
fixed_t SWRast_Interpolate(fixed_t v1, fixed_t v2, fixed_t t, fixed_t tMax);

// Like SWRast_Interpolate, but uses a parameter that goes from
// 0 to FRACUNIT - 1, which can be faster.
fixed_t SWRast_InterpolateByUnit(fixed_t v1, fixed_t v2, fixed_t t);

// Same as SWRast_InterpolateByUnit but with v1 == 0. Should be faster.
fixed_t SWRast_InterpolateByUnitFrom0(fixed_t v2, fixed_t t);

// Same as SWRast_Interpolate but with v1 == 0. Should be faster.
fixed_t SWRast_InterpolateFrom0(fixed_t v2, fixed_t t, fixed_t tMax);

// Returns a value interpolated between the three triangle vertices based on
// barycentric coordinates.
fixed_t SWRast_InterpolateBarycentric(fixed_t value0, fixed_t value1, fixed_t value2, fixed_t barycentric[3]);

// Corrects barycentric coordinates so that they exactly meet the defined conditions
// (each fall into <0,FRACUNIT>, sum = FRACUNIT).
// Note that doing this per-pixel can slow the program down significantly.
void SWRast_CorrectBarycentricCoords(fixed_t barycentric[3]);

fixed_t SWRast_DistanceManhattan(SWRast_Vec4 a, SWRast_Vec4 b);

//
// Floating point math
//

void FPVectorAdd(fpvector4_t *v1, fpvector4_t *v2);
void FPVectorSubtract(fpvector4_t *v1, fpvector4_t *v2);
void FPQuaternionFromEuler(fpquaternion_t *r, float z, float y, float x);
void FPQuaternionRotateVector(fpvector4_t *v, fpquaternion_t *q);

#define FPVector4(vec, vx, vy, vz) { vec.x = vx; vec.y = vy; vec.z = vz; vec.w = 1.0; }
#define FPInterpolate(start, end, r) ( (start) * (1.0 - (r)) + (end) * (r) )

#endif // _SWRAST_MATH_H_
