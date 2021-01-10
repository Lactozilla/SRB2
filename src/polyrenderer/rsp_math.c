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
/// \file  rsp_math.c
/// \brief Floating point math

#include "r_softpoly.h"

static float FastInverseSquareRoot(float number)
{
	return FixedToFloat(FixedSqrt(FloatToFixed(number)));
}

fpvector4_t RSP_VectorCrossProduct(fpvector4_t *v1, fpvector4_t *v2)
{
	fpvector4_t vector;
	vector.x = v1->y*v2->z - v1->z*v2->y;
	vector.y = v1->z*v2->x - v1->x*v2->z;
	vector.z = v1->x*v2->y - v1->y*v2->x;
	vector.w = 1.0f;
	return vector;
}

fpvector4_t RSP_VectorAdd(fpvector4_t *v1, fpvector4_t *v2)
{
	fpvector4_t vector;
	vector.x = v1->x + v2->x;
	vector.y = v1->y + v2->y;
	vector.z = v1->z + v2->z;
	vector.w = 1.0f;
	return vector;
}

fpvector4_t RSP_VectorSubtract(fpvector4_t *v1, fpvector4_t *v2)
{
	fpvector4_t vector;
	vector.x = v1->x - v2->x;
	vector.y = v1->y - v2->y;
	vector.z = v1->z - v2->z;
	vector.w = 1.0f;
	return vector;
}

fpvector4_t RSP_VectorMultiply(fpvector4_t *v, float x)
{
	fpvector4_t vector;
	vector.x = v->x * x;
	vector.y = v->y * x;
	vector.z = v->z * x;
	vector.w = 1.0f;
	return vector;
}

float RSP_VectorDotProduct(fpvector4_t *v1, fpvector4_t *v2)
{
	float a = (v1->x * v2->x);
	float b = (v1->y * v2->y);
	float c = (v1->z * v2->z);
	return a + b + c;
}

float RSP_VectorLength(fpvector4_t *v)
{
	float a = (v->x * v->x);
	float b = (v->y * v->y);
	float c = (v->z * v->z);
	return a + b + c;
}

float RSP_VectorInverseLength(fpvector4_t *v)
{
	return FastInverseSquareRoot(RSP_VectorLength(v));
}

float RSP_VectorDistance(fpvector4_t p, fpvector4_t pn, fpvector4_t pp)
{
	float a = pn.x * p.x;
	float b = pn.y * p.y;
	float c = pn.z * p.z;
	return (a + b + c - RSP_VectorDotProduct(&pn, &pp));
}

void RSP_VectorNormalize(fpvector4_t *v)
{
	float n = FastInverseSquareRoot(RSP_VectorLength(v));
	v->x *= n;
	v->y *= n;
	v->z *= n;
}

fpquaternion_t RSP_QuaternionMultiply(fpquaternion_t *q1, fpquaternion_t *q2)
{
	fpquaternion_t r;
	r.x = q1->w*q2->x + q1->x*q2->w + q1->y*q2->z - q1->z*q2->y;
	r.y = q1->w*q2->y - q1->x*q2->z + q1->y*q2->w + q1->z*q2->x;
	r.z = q1->w*q2->z + q1->x*q2->y - q1->y*q2->x + q1->z*q2->w;
	r.w = q1->w*q2->w - q1->x*q2->x - q1->y*q2->y - q1->z*q2->z;
	return r;
}

fpquaternion_t RSP_QuaternionConjugate(fpquaternion_t *q)
{
	fpquaternion_t r;
	r.x = -q->x;
	r.y = -q->y;
	r.z = -q->z;
	r.w = q->w;
	return r;
}

fpquaternion_t RSP_QuaternionFromEuler(float z, float y, float x)
{
	fpquaternion_t r;

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

	r.w = yaw[0] * pitch[0] * roll[0] + yaw[1] * pitch[1] * roll[1];
	r.x = yaw[0] * pitch[0] * roll[1] - yaw[1] * pitch[1] * roll[0];
	r.y = yaw[1] * pitch[0] * roll[1] + yaw[0] * pitch[1] * roll[0];
	r.z = yaw[1] * pitch[0] * roll[0] - yaw[0] * pitch[1] * roll[1];

	return r;
}

fpvector4_t RSP_QuaternionMultiplyVector(fpquaternion_t *q, fpvector4_t *v)
{
	fpvector4_t vn, r;
	fpquaternion_t vq, cq, rq, rq2;
	vn.x = v->x;
	vn.y = v->y;
	vn.z = v->z;
	RSP_VectorNormalize(&vn);

	vq.x = vn.x;
	vq.y = vn.y;
	vq.z = vn.z;
	vq.w = 0.f;

	cq = RSP_QuaternionConjugate(q);
	rq = RSP_QuaternionMultiply(&vq, &cq);
	rq2 = RSP_QuaternionMultiply(q, &rq);

	r.x = rq2.x;
	r.y = rq2.y;
	r.z = rq2.z;

	return r;
}

void RSP_QuaternionNormalize(fpquaternion_t *q)
{
	float a = (q->x * q->x);
	float b = (q->y * q->y);
	float c = (q->z * q->z);
	float d = (q->w * q->w);
	float n = FastInverseSquareRoot(a + b + c + d);
	q->x *= n;
	q->y *= n;
	q->z *= n;
	q->w *= n;
}

void RSP_VectorRotate(fpvector4_t *v, float angle, float x, float y, float z)
{
	fpquaternion_t q;
	float h = angle / 2.0f;
	q.x = x * sin(h);
	q.y = y * sin(h);
	q.z = z * sin(h);
	q.w = cos(h);
	RSP_QuaternionRotateVector(v, &q);
}

void RSP_QuaternionRotateVector(fpvector4_t *v, fpquaternion_t *q)
{
	fpquaternion_t r, qv, qc, quaternion;

	quaternion.x = v->x;
	quaternion.y = v->y;
	quaternion.z = v->z;
	quaternion.w = 0.0f;

	qc = RSP_QuaternionConjugate(q);
	qv = RSP_QuaternionMultiply(q, &quaternion);
	r = RSP_QuaternionMultiply(&qv, &qc);

	v->x = r.x;
	v->y = r.y;
	v->z = r.z;
}
