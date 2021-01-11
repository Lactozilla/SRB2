// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2021 by Jaime Ita Passos.
// Copyright (C) 2019 by "Arkus-Kotan".
// Copyright (C) 2017 by Krzysztof Kondrak.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  swrast_math.c
/// \brief Math

#include "swrast.h"

void SWRast_InitVec4(SWRast_Vec4 *v)
{
	v->x = v->y = v->z = 0;
	v->w = FRACUNIT;
}

void SWRast_InitAngVec4(SWRast_AngVec4 *v)
{
	v->x = v->y = v->z = v->w = 0;
}

void SWRast_SetVec4(SWRast_Vec4 *v, fixed_t x, fixed_t y, fixed_t z, fixed_t w)
{
	v->x = x;
	v->y = y;
	v->z = z;
	v->w = w;
}

void SWRast_SetAngVec4(SWRast_AngVec4 *v, angle_t x, angle_t y, angle_t z, angle_t w)
{
	v->x = x;
	v->y = y;
	v->z = z;
	v->w = w;
}

void SWRast_Vec3Add(SWRast_Vec4 *result, SWRast_Vec4 added)
{
	result->x += added.x;
	result->y += added.y;
	result->z += added.z;
}

void SWRast_Vec3Sub(SWRast_Vec4 *result, SWRast_Vec4 subtracted)
{
	result->x -= subtracted.x;
	result->y -= subtracted.y;
	result->z -= subtracted.z;
}

void SWRast_InitMat4(SWRast_Mat4 *m)
{
	#define M(x,y) (*m)[x][y]
	#define S FRACUNIT

	M(0,0) = S; M(1,0) = 0; M(2,0) = 0; M(3,0) = 0;
	M(0,1) = 0; M(1,1) = S; M(2,1) = 0; M(3,1) = 0;
	M(0,2) = 0; M(1,2) = 0; M(2,2) = S; M(3,2) = 0;
	M(0,3) = 0; M(1,3) = 0; M(2,3) = 0; M(3,3) = S;

	#undef M
	#undef S
}

fixed_t SWRast_DotProductVec3(SWRast_Vec4 *a, SWRast_Vec4 *b)
{
	return FixedMul(a->x, b->x) + FixedMul(a->y, b->y) + FixedMul(a->z, b->z);
}

void SWRast_Reflect(SWRast_Vec4 *toLight, SWRast_Vec4 *normal, SWRast_Vec4 *result)
{
	fixed_t d = 2 * SWRast_DotProductVec3(toLight,normal);

	result->x = FixedMul(normal->x, d) - toLight->x;
	result->y = FixedMul(normal->y, d) - toLight->y;
	result->z = FixedMul(normal->z, d) - toLight->z;
}

void SWRast_CrossProduct(SWRast_Vec4 *a, SWRast_Vec4 *b, SWRast_Vec4 *result)
{
	result->x = FixedMul(a->y, b->z) - FixedMul(a->z, b->y);
	result->y = FixedMul(a->z, b->x) - FixedMul(a->x, b->z);
	result->z = FixedMul(a->x, b->y) - FixedMul(a->y, b->x);
}

void SWRast_TriangleNormal(SWRast_Vec4 *t0, SWRast_Vec4 *t1, SWRast_Vec4 *t2, SWRast_Vec4 *n)
{
	SWRast_Vec4 tn1, tn2;

	#define ANTI_OVERFLOW 32

	tn1.x = (t1->x - t0->x) / ANTI_OVERFLOW;
	tn1.y = (t1->y - t0->y) / ANTI_OVERFLOW;
	tn1.z = (t1->z - t0->z) / ANTI_OVERFLOW;

	tn2.x = (t2->x - t0->x) / ANTI_OVERFLOW;
	tn2.y = (t2->y - t0->y) / ANTI_OVERFLOW;
	tn2.z = (t2->z - t0->z) / ANTI_OVERFLOW;

	#undef ANTI_OVERFLOW

	SWRast_CrossProduct(&tn1, &tn2, n);
	SWRast_NormalizeVec3(n);
}

void SWRast_MultVec4Mat4(SWRast_Vec4 *v, SWRast_Mat4 *m)
{
	SWRast_Vec4 vBackup;

	vBackup.x = v->x;
	vBackup.y = v->y;
	vBackup.z = v->z;
	vBackup.w = v->w;

	#define dotCol(col)\
		(FixedMul(vBackup.x, (*m)[col][0]) +\
		 FixedMul(vBackup.y, (*m)[col][1]) +\
		 FixedMul(vBackup.z, (*m)[col][2]) +\
		 FixedMul(vBackup.w, (*m)[col][3]));

	v->x = dotCol(0);
	v->y = dotCol(1);
	v->z = dotCol(2);
	v->w = dotCol(3);
}

void SWRast_MultVec3Mat4(SWRast_Vec4 *v, SWRast_Mat4 *m)
{
	SWRast_Vec4 vBackup;

	#undef dotCol
	#define dotCol(col)\
		FixedMul(vBackup.x, (*m)[col][0]) +\
		FixedMul(vBackup.y, (*m)[col][1]) +\
		FixedMul(vBackup.z, (*m)[col][2]) +\
		(*m)[col][3]

	vBackup.x = v->x;
	vBackup.y = v->y;
	vBackup.z = v->z;
	vBackup.w = v->w;

	v->x = dotCol(0);
	v->y = dotCol(1);
	v->z = dotCol(2);
	v->w = FRACUNIT;
}

#undef dotCol

// general helper functions
fixed_t SWRast_abs(fixed_t value)
{
	return value * (((value >= 0) << 1) - 1);
}

fixed_t SWRast_min(fixed_t v1, fixed_t v2)
{
	return v1 >= v2 ? v2 : v1;
}

fixed_t SWRast_max(fixed_t v1, fixed_t v2)
{
	return v1 >= v2 ? v1 : v2;
}

fixed_t SWRast_clamp(fixed_t v, fixed_t v1, fixed_t v2)
{
	return v >= v1 ? (v <= v2 ? v : v2) : v1;
}

fixed_t SWRast_zeroClamp(fixed_t value)
{
	return (value * (value >= 0));
}

fixed_t SWRast_wrap(fixed_t value, fixed_t mod)
{
	return value >= 0 ? (value % mod) : (mod + (value % mod) - 1);
}

fixed_t SWRast_NonZero(fixed_t value)
{
	return (value + (value == 0));
}

/** Interpolated between two values, v1 and v2, in the same ratio as t is to
	tMax. Does NOT prevent zero division. */
fixed_t SWRast_Interpolate(fixed_t v1, fixed_t v2, fixed_t t, fixed_t tMax)
{
	return v1 + FixedDiv(FixedMul((v2 - v1), t), tMax);
}

/** Like SWRast_Interpolate, but uses a parameter that goes from 0 to
	FRACUNIT - 1, which can be faster. */
fixed_t SWRast_InterpolateByUnit(fixed_t v1, fixed_t v2, fixed_t t)
{
	return v1 + FixedMul((v2 - v1), t);
}

/** Same as SWRast_InterpolateByUnit but with v1 == 0. Should be faster. */
fixed_t SWRast_InterpolateByUnitFrom0(fixed_t v2, fixed_t t)
{
	return FixedMul(v2, t);
}

/** Same as SWRast_Interpolate but with v1 == 0. Should be faster. */
fixed_t SWRast_InterpolateFrom0(fixed_t v2, fixed_t t, fixed_t tMax)
{
	return FixedDiv(FixedMul(v2, t), tMax);
}

fixed_t SWRast_DistanceManhattan(SWRast_Vec4 a, SWRast_Vec4 b)
{
	return
		SWRast_abs(a.x - b.x) +
		SWRast_abs(a.y - b.y) +
		SWRast_abs(a.z - b.z);
}

void SWRast_MultiplyMatrices(SWRast_Mat4 *m1, SWRast_Mat4 *m2)
{
	SWRast_Mat4 mat1;
	UINT16 row, col, i;

	for (row = 0; row < 4; ++row)
		for (col = 0; col < 4; ++col)
			mat1[col][row] = (*m1)[col][row];

	for (row = 0; row < 4; ++row)
		for (col = 0; col < 4; ++col)
		{
			(*m1)[col][row] = 0;

			for (i = 0; i < 4; ++i)
				(*m1)[col][row] += FixedMul(mat1[i][row], (*m2)[col][i]);
		}
}

/** Returns a value interpolated between the three triangle vertices based on
	barycentric coordinates. */
fixed_t SWRast_InterpolateBarycentric(
	fixed_t value0,
	fixed_t value1,
	fixed_t value2,
	fixed_t barycentric[3])
{
	return
		FixedMul(value0, barycentric[0]) +
		FixedMul(value1, barycentric[1]) +
		FixedMul(value2, barycentric[2]);
}

/** Corrects barycentric coordinates so that they exactly meet the defined
	conditions (each fall into <0,FRACUNIT>, sum =
	FRACUNIT). Note that doing this per-pixel can slow the program
	down significantly. */
void SWRast_CorrectBarycentricCoords(fixed_t barycentric[3])
{
	fixed_t d;

	barycentric[0] = SWRast_clamp(barycentric[0],0,FRACUNIT);
	barycentric[1] = SWRast_clamp(barycentric[1],0,FRACUNIT);

	d = FRACUNIT - barycentric[0] - barycentric[1];

	if (d < 0)
	{
		barycentric[0] += d;
		barycentric[2] = 0;
	}
	else
		barycentric[2] = d;
}

void SWRast_MakeTranslationMatrix(
	fixed_t offsetX,
	fixed_t offsetY,
	fixed_t offsetZ,
	SWRast_Mat4 *m)
{
	#define M(x,y) (*m)[x][y]
	#define S FRACUNIT

	M(0,0) = S;       M(1,0) = 0;       M(2,0) = 0;       M(3,0) = 0;
	M(0,1) = 0;       M(1,1) = S;       M(2,1) = 0;       M(3,1) = 0;
	M(0,2) = 0;       M(1,2) = 0;       M(2,2) = S;       M(3,2) = 0;
	M(0,3) = offsetX; M(1,3) = offsetY; M(2,3) = offsetZ; M(3,3) = S;

	#undef M
	#undef S
}

void SWRast_MakeScaleMatrix(
	fixed_t scaleX,
	fixed_t scaleY,
	fixed_t scaleZ,
	SWRast_Mat4 *m)
{
	#define M(x,y) (*m)[x][y]

	M(0,0) = scaleX; M(1,0) = 0;      M(2,0) = 0;      M(3,0) = 0;
	M(0,1) = 0;      M(1,1) = scaleY; M(2,1) = 0;      M(3,1) = 0;
	M(0,2) = 0;      M(1,2) = 0;      M(2,2) = scaleZ; M(3,2) = 0;
	M(0,3) = 0;      M(1,3) = 0;      M(2,3) = 0;      M(3,3) = FRACUNIT;

	#undef M
}

void SWRast_MakeRotationMatrixZXY(
	angle_t byX,
	angle_t byY,
	angle_t byZ,
	SWRast_Mat4 *m)
{
	fixed_t sx, sy, sz;
	fixed_t cx, cy, cz;

	sx = FINESINE(byX >> ANGLETOFINESHIFT);
	sy = FINESINE(byY >> ANGLETOFINESHIFT);
	sz = FINESINE(byZ >> ANGLETOFINESHIFT);

	cx = FINECOSINE(byX >> ANGLETOFINESHIFT);
	cy = FINECOSINE(byY >> ANGLETOFINESHIFT);
	cz = FINECOSINE(byZ >> ANGLETOFINESHIFT);

	#define M(x,y) (*m)[x][y]

	M(0,0) = FixedMul(cy, cz) + FixedMul(FixedMul(sy, sx), sz);
	M(1,0) = FixedMul(cx, sz);
	M(2,0) = FixedMul(FixedMul(cy, sx), sz) - FixedMul(cz, sy);
	M(3,0) = 0;

	M(0,1) = FixedMul(FixedMul(cz, sy), sx) - FixedMul(cy, sz);
	M(1,1) = FixedMul(cx, cz);
	M(2,1) = FixedMul(FixedMul(cy, cz), sx) + FixedMul(sy, sz);
	M(3,1) = 0;

	M(0,2) = FixedMul(cx, sy);
	M(1,2) = -1 * sx;
	M(2,2) = FixedMul(cy, cx);
	M(3,2) = 0;

	M(0,3) = 0;
	M(1,3) = 0;
	M(2,3) = 0;
	M(3,3) = FRACUNIT;

	#undef M
	#undef S
}

void SWRast_MakeWorldMatrix(SWRast_Transform3D *worldTransform, SWRast_Mat4 *m)
{
	SWRast_Mat4 t;

	SWRast_MakeScaleMatrix(
		worldTransform->scale.x,
		worldTransform->scale.y,
		worldTransform->scale.z,
		m
	);

	SWRast_MakeRotationMatrixZXY(
		worldTransform->rotation.x,
		worldTransform->rotation.y,
		worldTransform->rotation.z,
		&t);

	SWRast_MultiplyMatrices(m,&t);

	SWRast_MakeTranslationMatrix(
		worldTransform->translation.x,
		worldTransform->translation.y,
		worldTransform->translation.z,
		&t);

	SWRast_MultiplyMatrices(m,&t);
}

void SWRast_TransposeMat4(SWRast_Mat4 *m)
{
	UINT8 x, y;
	fixed_t tmp;

	for (y = 0; y < 3; ++y)
		for (x = 1 + y; x < 4; ++x)
		{
			tmp = (*m)[x][y];
			(*m)[x][y] = (*m)[y][x];
			(*m)[y][x] = tmp;
		}
}

void SWRast_MakeCameraMatrix(SWRast_Transform3D *cameraTransform, SWRast_Mat4 *m)
{
	SWRast_Mat4 r;

	SWRast_MakeTranslationMatrix(
		-1 * cameraTransform->translation.x,
		-1 * cameraTransform->translation.y,
		-1 * cameraTransform->translation.z,
		m);

	SWRast_MakeRotationMatrixZXY(
		cameraTransform->rotation.x,
		cameraTransform->rotation.y,
		cameraTransform->rotation.z,
		&r);

	SWRast_TransposeMat4(&r); // transposing creates an inverse transform

	SWRast_MultiplyMatrices(m,&r);

	SWRast_MakeScaleMatrix(
		cameraTransform->scale.x,
		cameraTransform->scale.y,
		cameraTransform->scale.z,
		&r
	);

	SWRast_MultiplyMatrices(m,&r);
}

fixed_t SWRast_Vec3Length(SWRast_Vec4 *v)
{
	fixed_t xs = FixedMul(v->x, v->x);
	fixed_t ys = FixedMul(v->y, v->y);
	fixed_t zs = FixedMul(v->z, v->z);
	return FixedSqrt(xs + ys + zs);
}

void SWRast_NormalizeVec3(SWRast_Vec4 *v)
{
	fixed_t l = SWRast_Vec3Length(v);

	if (l == 0)
		return;

	v->x = FixedDiv(v->x, l);
	v->y = FixedDiv(v->y, l);
	v->z = FixedDiv(v->z, l);
}

void SWRast_NormalizeVec3_Safe(SWRast_Vec4 *v)
{
	float x = FixedToFloat(v->x);
	float y = FixedToFloat(v->y);
	float z = FixedToFloat(v->z);
	float xs = x * x;
	float ys = y * y;
	float zs = z * z;
	float length = sqrtf(xs + ys + zs);
	v->x = FloatToFixed(x * length);
	v->y = FloatToFixed(y * length);
	v->z = FloatToFixed(z * length);
}

void SWRast_InitTransform3D(SWRast_Transform3D *t)
{
	SWRast_InitVec4(&(t->translation));
	SWRast_InitAngVec4(&(t->rotation));
	t->scale.x = FRACUNIT;
	t->scale.y = FRACUNIT;
	t->scale.z = FRACUNIT;
	t->scale.w = 0;
}

/** Performs perspecive division (z-divide). Does NOT check for division by
	zero. */
void SWRast_PerspectiveDivide(SWRast_Vec4 *vector, fixed_t focalLength)
{
	vector->x = FixedDiv(FixedMul(vector->x, focalLength), vector->z);
	vector->y = FixedDiv(FixedMul(vector->y, focalLength), vector->z);
}

void SWRast_Project3DPointToScreen(
	SWRast_Vec4 point,
	SWRast_Camera cam,
	SWRast_Vec4 *result)
{
	SWRast_Mat4 m;
	fixed_t s;
	INT16 x, y;

	SWRast_MakeCameraMatrix(&cam.transform,&m);

	s = point.w;

	point.w = FRACUNIT;

	SWRast_MultVec3Mat4(&point,&m);

	point.z = SWRast_NonZero(point.z);

	SWRast_PerspectiveDivide(&point,cam.focalLength);
	SWRast_MapProjectionPlaneToScreen(point,&x,&y);

	result->x = x;
	result->y = y;
	result->z = point.z;

	result->w = (point.z <= 0) ? 0 : FixedDiv(FixedMul(FixedMul(s, cam.focalLength), SWRAST_RESOLUTION_X<<FRACBITS), point.z);
}

void SWRast_MapProjectionPlaneToScreen(
	SWRast_Vec4 point,
	INT16 *screenX,
	INT16 *screenY)
{
	*screenX = SWRAST_HALF_RESOLUTION_X + FixedInt(FixedMul(point.x, SWRAST_HALF_RESOLUTION_X<<FRACBITS));
	*screenY = SWRAST_HALF_RESOLUTION_Y - FixedInt(FixedMul(point.y, SWRAST_HALF_RESOLUTION_X<<FRACBITS));
}

//
// Floating point math
//

void FPVectorAdd(fpvector4_t *v1, fpvector4_t *v2)
{
	v1->x += v2->x;
	v1->y += v2->y;
	v1->z += v2->z;
}

void FPVectorSubtract(fpvector4_t *v1, fpvector4_t *v2)
{
	v1->x -= v2->x;
	v1->y -= v2->y;
	v1->z -= v2->z;
}

static void FPQuaternionMultiply(fpquaternion_t *r, fpquaternion_t *q1, fpquaternion_t *q2)
{
	r->x = q1->w*q2->x + q1->x*q2->w + q1->y*q2->z - q1->z*q2->y;
	r->y = q1->w*q2->y - q1->x*q2->z + q1->y*q2->w + q1->z*q2->x;
	r->z = q1->w*q2->z + q1->x*q2->y - q1->y*q2->x + q1->z*q2->w;
	r->w = q1->w*q2->w - q1->x*q2->x - q1->y*q2->y - q1->z*q2->z;
}

static void FPQuaternionConjugate(fpquaternion_t *r, fpquaternion_t *q)
{
	r->x = -q->x;
	r->y = -q->y;
	r->z = -q->z;
	r->w = q->w;
}

void FPQuaternionFromEuler(fpquaternion_t *r, float z, float y, float x)
{
	float yaw[2];
	float pitch[2];
	float roll[2];

#define deg2rad(d) ((d*M_PI)/180.0)
	z = deg2rad(z);
	y = deg2rad(y);
	x = deg2rad(x);
#undef deg2rad

	z /= 2.0f;
	y /= 2.0f;
	x /= 2.0f;

	yaw[0] = cos(z);
	yaw[1] = sin(z);

	pitch[0] = cos(y);
	pitch[1] = sin(y);

	roll[0] = cos(x);
	roll[1] = sin(x);

	r->w = yaw[0] * pitch[0] * roll[0] + yaw[1] * pitch[1] * roll[1];
	r->x = yaw[0] * pitch[0] * roll[1] - yaw[1] * pitch[1] * roll[0];
	r->y = yaw[1] * pitch[0] * roll[1] + yaw[0] * pitch[1] * roll[0];
	r->z = yaw[1] * pitch[0] * roll[0] - yaw[0] * pitch[1] * roll[1];
}

void FPQuaternionRotateVector(fpvector4_t *v, fpquaternion_t *q)
{
	fpquaternion_t r, qv, qc, quaternion;

	quaternion.x = v->x;
	quaternion.y = v->y;
	quaternion.z = v->z;
	quaternion.w = 0.0f;

	FPQuaternionConjugate(&qc, q);
	FPQuaternionMultiply(&qv, q, &quaternion);
	FPQuaternionMultiply(&r, &qv, &qc);

	v->x = r.x;
	v->y = r.y;
	v->z = r.z;
}
