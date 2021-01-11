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
	fixed_t fnum = FLOAT_TO_FIXED(number);
	fixed_t sqnum = FixedSqrt(fnum);
	return FIXED_TO_FLOAT(sqnum);
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

fpvector4_t RSP_MatrixMultiplyVector(fpmatrix16_t *m, fpvector4_t *v)
{
	fpvector4_t vector;
	vector.x = m->m[0] * v->x + m->m[4] * v->y + m->m[8] * v->z + m->m[12] * v->w;
	vector.y = m->m[1] * v->x + m->m[5] * v->y + m->m[9] * v->z + m->m[13] * v->w;
	vector.z = m->m[2] * v->x + m->m[6] * v->y + m->m[10] * v->z + m->m[14] * v->w;
	vector.w = m->m[3] * v->x + m->m[7] * v->y + m->m[11] * v->z + m->m[15] * v->w;
	return vector;
}

fpmatrix16_t RSP_MatrixMultiply(fpmatrix16_t *m1, fpmatrix16_t *m2)
{
	fpmatrix16_t matrix;
	matrix.m[0] = m1->m[0] * m2->m[0] + m1->m[1] * m2->m[4] + m1->m[2] * m2->m[8]  + m1->m[3] * m2->m[12];
	matrix.m[1] = m1->m[0] * m2->m[1] + m1->m[1] * m2->m[5] + m1->m[2] * m2->m[9]  + m1->m[3] * m2->m[13];
	matrix.m[2] = m1->m[0] * m2->m[2] + m1->m[1] * m2->m[6] + m1->m[2] * m2->m[10] + m1->m[3] * m2->m[14];
	matrix.m[3] = m1->m[0] * m2->m[3] + m1->m[1] * m2->m[7] + m1->m[2] * m2->m[11] + m1->m[3] * m2->m[15];
	matrix.m[4] = m1->m[4] * m2->m[0] + m1->m[5] * m2->m[4] + m1->m[6] * m2->m[8]  + m1->m[7] * m2->m[12];
	matrix.m[5] = m1->m[4] * m2->m[1] + m1->m[5] * m2->m[5] + m1->m[6] * m2->m[9]  + m1->m[7] * m2->m[13];
	matrix.m[6] = m1->m[4] * m2->m[2] + m1->m[5] * m2->m[6] + m1->m[6] * m2->m[10] + m1->m[7] * m2->m[14];
	matrix.m[7] = m1->m[4] * m2->m[3] + m1->m[5] * m2->m[7] + m1->m[6] * m2->m[11] + m1->m[7] * m2->m[15];
	matrix.m[8] = m1->m[8] * m2->m[0] + m1->m[9] * m2->m[4] + m1->m[10] * m2->m[8]  + m1->m[11] * m2->m[12];
	matrix.m[9] = m1->m[8] * m2->m[1] + m1->m[9] * m2->m[5] + m1->m[10] * m2->m[9]  + m1->m[11] * m2->m[13];
	matrix.m[10] = m1->m[8] * m2->m[2] + m1->m[9] * m2->m[6] + m1->m[10] * m2->m[10] + m1->m[11] * m2->m[14];
	matrix.m[11] = m1->m[8] * m2->m[3] + m1->m[9] * m2->m[7] + m1->m[10] * m2->m[11] + m1->m[11] * m2->m[15];
	matrix.m[12] = m1->m[12] * m2->m[0] + m1->m[13] * m2->m[4] + m1->m[14] * m2->m[8]  + m1->m[15] * m2->m[12];
	matrix.m[13] = m1->m[12] * m2->m[1] + m1->m[13] * m2->m[5] + m1->m[14] * m2->m[9]  + m1->m[15] * m2->m[13];
	matrix.m[14] = m1->m[12] * m2->m[2] + m1->m[13] * m2->m[6] + m1->m[14] * m2->m[10] + m1->m[15] * m2->m[14];
	matrix.m[15] = m1->m[12] * m2->m[3] + m1->m[13] * m2->m[7] + m1->m[14] * m2->m[11] + m1->m[15] * m2->m[15];
	return matrix;
}

void RSP_MatrixTranspose(fpmatrix16_t *input)
{
	int i, j;
	fpmatrix16_t matrix;
	for (i = 0; i < 16; i++)
		matrix.m[i] = input->m[i];
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			input->m[i * 4 + j] = matrix.m[i + j * 4];
}

void RSP_MakeIdentityMatrix(fpmatrix16_t *matrix)
{
	int i;
	for (i = 1; i < 16; ++i)
		matrix->m[i] = 0.0f;
	matrix->m[0] = matrix->m[5] = matrix->m[10] = matrix->m[15] = 1.0f;
}

void RSP_MakePerspectiveMatrix(fpmatrix16_t *m, float fov, float zoomneeded, float aspectratio, float np, float fp)
{
	float tfov = tan(fov / 2.0f) * zoomneeded;
	float deltaZ = (fp - np);
	int i;

	for (i = 0; i < 16; ++i)
		m->m[i] = 0.f;

	m->m[0]  = 1.0f / (aspectratio * tfov);
	m->m[5]  = 1.0f / tfov;
	m->m[10] = -(fp + np) / deltaZ;
	m->m[11] = -1.0f;
	m->m[14] = -2.0f * fp * np / deltaZ;
}

void RSP_MakeViewMatrix(fpmatrix16_t *m, fpvector4_t *eye, fpvector4_t *target, fpvector4_t *up)
{
	fpvector4_t x,y,z;

	z.x = target->x;
	z.y = target->y;
	z.z = target->z;
	z.w = target->w;

	RSP_VectorNormalize(&z);
	x = RSP_VectorCrossProduct(&z, up);
	RSP_VectorNormalize(&x);
	y = RSP_VectorCrossProduct(&x, &z);

	RSP_MakeIdentityMatrix(m);

	m->m[0] = x.x;
	m->m[4] = x.y;
	m->m[8] = x.z;

	m->m[1] = y.x;
	m->m[5] = y.y;
	m->m[9] = y.z;

	m->m[2] = -z.x;
	m->m[6] = -z.y;
	m->m[10] = -z.z;

	m->m[12] = -RSP_VectorDotProduct(&x, eye);
	m->m[13] = -RSP_VectorDotProduct(&y, eye);
	m->m[14] = RSP_VectorDotProduct(&z, eye);
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

fpvector4_t RSP_IntersectPlane(fpvector4_t pp, fpvector4_t pn, fpvector4_t start, fpvector4_t end, float *t)
{
	fpvector4_t delta, intersect;
	float pd, ad, bd;

	pd = -RSP_VectorDotProduct(&pn, &pp);
	ad = RSP_VectorDotProduct(&start, &pn);
	bd = RSP_VectorDotProduct(&end, &pn);

	*t = (-pd - ad) / (bd - ad);

	delta = RSP_VectorSubtract(&end, &start);
	intersect = RSP_VectorMultiply(&delta, *t);

	return RSP_VectorAdd(&start, &intersect);
}
