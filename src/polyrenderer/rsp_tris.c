// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2021 by Jaime Ita Passos.
// Copyright (C) 2019 by "Arkus-Kotan".
// Copyright (C) 2017 by Krzysztof Kondrak.
// Copyright (C) 2018 by "Javidx9".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  rsp_tris.c
/// \brief Triangle drawers

#include "r_softpoly.h"

// a triangle is degenerate if v0 == v1 and v0 == v2
static boolean is_degenerate_triangle(rsp_vertex_t v0, rsp_vertex_t v1, rsp_vertex_t v2)
{
	fixed_t v0x = FLOAT_TO_FIXED(v0.position.x);
	fixed_t v1x = FLOAT_TO_FIXED(v1.position.x);
	fixed_t v2x = FLOAT_TO_FIXED(v2.position.x);
	fixed_t v0y = FLOAT_TO_FIXED(v0.position.y);
	fixed_t v1y = FLOAT_TO_FIXED(v1.position.y);
	fixed_t v2y = FLOAT_TO_FIXED(v2.position.y);
	// x
	if (v0x == v1x && v0x == v2x)
		return true;
	// y
	else if (v0y == v1y && v0y == v2y)
		return true;
	// not degenerate
	return false;
}

static boolean is_behind_view_frustum(rsp_vertex_t v0, rsp_vertex_t v1, rsp_vertex_t v2)
{
	const float near_plane = rsp_target.near_plane / 2.0f;
	// behind near plane
	if ((v0.position.z < near_plane) || (v1.position.z < near_plane) || (v2.position.z < near_plane))
		return true;
	// z < 0
	if ((v0.position.z < 1.0f) || (v1.position.z < 1.0f) || (v2.position.z < 1.0f))
		return true;
	// z > w
	if ((v0.position.z > v0.position.w) && (v1.position.z > v1.position.w) && (v2.position.z > v2.position.w))
		return true;
	return false;
}

// determine if either coordinate of all vertices is offscreen
static boolean tri_x_clipped(fpvector4_t *p0, fpvector4_t *p1, fpvector4_t *p2)
{
	return ((p0->x < -p0->w && p1->x < -p1->w && p2->x < -p2->w) || (p0->x > p0->w && p1->x > p1->w && p2->x > p2->w));
}

#if 0
static boolean tri_y_clipped(fpvector4_t *p0, fpvector4_t *p1, fpvector4_t *p2)
{
	return ((p0->y < -p0->w && p1->y < -p1->w && p2->y < -p2->w) || (p0->y > p0->w && p1->y > p1->w && p2->y > p2->w));
}
#endif

static boolean is_triangle_offscreen(rsp_vertex_t *v0, rsp_vertex_t *v1, rsp_vertex_t *v2)
{
	return (tri_x_clipped(&v0->position, &v1->position, &v2->position));
}

#ifdef RSP_CLIPTRIANGLES
typedef struct tpoint_s
{
	fpvector4_t *coord[3];
	fpvector2_t *tex[3];
	INT32 count;
} tpoint_t;

static INT32 clip_triangle(fpvector4_t pp, fpvector4_t pn, rsp_triangle_t in, rsp_triangle_t *out1, rsp_triangle_t *out2)
{
	// INSIDE!! OUTSIDE!!
	// S-O-N-I-C, GO!
	tpoint_t inside;
	tpoint_t outside;
	INT32 i;

	float dotproducts[3];
	float t = 0.0f;

	RSP_VectorNormalize(&pn);

	for (i = 0; i < 3; i++)
		dotproducts[i] = RSP_VectorDistance(in.vertices[i].position, pp, pn);

#define STORE_VERTEX(vertex) \
	if (dotproducts[vertex] >= 0) \
	{ \
		inside.coord[inside.count] = &in.vertices[vertex].position; \
		inside.tex[inside.count] = &in.vertices[vertex].uv; \
		inside.count++; \
	} \
	else \
	{ \
		outside.coord[outside.count] = &in.vertices[vertex].position; \
		outside.tex[outside.count] = &in.vertices[vertex].uv; \
		outside.count++; \
	}

	inside.count = 0;
	outside.count = 0;

	for (i = 0; i < 3; i++)
		STORE_VERTEX(i)

#undef STORE_VERTEX

	// triangle is behind view frustum
	if (inside.count < 1)
		return 0;
	// triangle is completely inside view frustum
	else if (inside.count >= 3)
	{
		*out1 = in;
		return 1;
	}
	else if (inside.count == 1 && outside.count == 2)
	{
		out1->vertices[0].position = *inside.coord[0];
		out1->vertices[0].uv = *inside.tex[0];

		// intersect this plane and get interpolant
		out1->vertices[1].position = RSP_IntersectPlane(pp, pn, *inside.coord[0], *outside.coord[0], &t);
		out1->vertices[2].position = RSP_IntersectPlane(pp, pn, *inside.coord[0], *outside.coord[1], &t);

		// interpolate UV
		out1->vertices[1].uv.u = t * (outside.tex[0]->u - inside.tex[0]->u) + inside.tex[0]->u;
		out1->vertices[1].uv.v = t * (outside.tex[0]->v - inside.tex[0]->v) + inside.tex[0]->v;
		out1->vertices[2].uv.u = t * (outside.tex[1]->u - inside.tex[0]->u) + inside.tex[0]->u;
		out1->vertices[2].uv.v = t * (outside.tex[1]->v - inside.tex[0]->v) + inside.tex[0]->v;
		return 1;
	}
	else if (inside.count == 2 && outside.count == 1)
	{
		out1->vertices[0].position = *inside.coord[0];
		out1->vertices[1].position = *inside.coord[1];
		out1->vertices[0].uv = *inside.tex[0];
		out1->vertices[1].uv = *inside.tex[1];

		out1->vertices[2].position = RSP_IntersectPlane(pp, pn, *inside.coord[0], *outside.coord[0], &t);
		out1->vertices[2].uv.u = t * (outside.tex[0]->u - inside.tex[0]->u) + inside.tex[0]->u;
		out1->vertices[2].uv.v = t * (outside.tex[0]->v - inside.tex[0]->v) + inside.tex[0]->v;

		out2->vertices[0].position = *inside.coord[1];
		out2->vertices[1].position = out1->vertices[2].position;
		out2->vertices[2].position = RSP_IntersectPlane(pp, pn, *inside.coord[1], *outside.coord[0], &t);

		out2->vertices[0].uv = *inside.tex[1];
		out2->vertices[1].uv = out1->vertices[2].uv;
		out2->vertices[2].uv.u = t * (outside.tex[0]->u - inside.tex[1]->u) + inside.tex[1]->u;
		out2->vertices[2].uv.v = t * (outside.tex[0]->v - inside.tex[1]->v) + inside.tex[1]->v;

		return 2;
	}

	// invalid triangle
	return -1;
}

void RSP_ClipTriangle(rsp_triangle_t *tri)
{
	fpvector4_t pp;
	fpvector4_t pn;
	rsp_triangle_t clipped[2];
	INT32 tricount = 0;

	memset(&pp, 0x00, sizeof(pp));
	memset(&pn, 0x00, sizeof(pn));

	// near plane
	pp.z = rsp_target.near_plane;
	pp.w = 1.0f;

	// near plane normal
	pn.z = 1.0f;
	pn.w = 1.0f;

	// clip against view frustum
	tricount = clip_triangle(pp, pn, *tri, &clipped[0], &clipped[1]);
	if (tricount < 1)
		return;

	// draw one or more triangles
	RSP_DrawTriangleList(tri, clipped, tricount);
}
#endif

void RSP_DrawTriangle(rsp_triangle_t *tri)
{
	float aspect = rsp_target.aspectratio;
	rsp_vertex_t v0 = tri->vertices[0];
	rsp_vertex_t v1 = tri->vertices[1];
	rsp_vertex_t v2 = tri->vertices[2];

	// avoid a crash here
	if (!rsp_curtrifunc)
		I_Error("RSP_DrawTriangle: no triangle drawer set!");

	// transform x and y of each vertex to screen coordinates
	#define TRANSFORM_VERTEX(vertex) \
	{ \
		vertex.position.x /= (vertex.position.z / aspect); \
		vertex.position.y /= (vertex.position.z / aspect); \
		vertex.position.x = ((vertex.position.x + 1.0f) / 2.0f) * rsp_target.width; \
		vertex.position.y = ((vertex.position.y + 1.0f) / 2.0f) * rsp_target.height; \
	}

	TRANSFORM_VERTEX(v0)
	TRANSFORM_VERTEX(v1)
	TRANSFORM_VERTEX(v2)

	#undef TRANSFORM_VERTEX

	if (is_behind_view_frustum(v0, v1, v2))
		return;

	// sort vertices so that v0 is topmost, then v2, then v1
	if (v2.position.y > v1.position.y)
		RSP_SwapVertex(v1, v2)
	if (v0.position.y > v1.position.y)
		RSP_SwapVertex(v0, v1)
	if (v0.position.y > v2.position.y)
		RSP_SwapVertex(v0, v2)

	// discard degenerate triangle
	if (is_degenerate_triangle(v0, v1, v2))
		return;

	// "Non-trivial" triangles will be broken down into a composition of flat bottom and flat top triangles.
	// For this, we need to calculate a new vertex, v3 and interpolate its UV coordinates.
	{
		rsp_vertex_t v3;
		fpvector4_t diff, diff2;
		float ratioU = 1, ratioV = 1;

		// calculate v3.x with Intercept Theorem, y is the same as v2
		v3.position.x = v0.position.x + (v1.position.x - v0.position.x) * (v2.position.y - v0.position.y) / (v1.position.y - v0.position.y);
		v3.position.y = v2.position.y;
		// setting this to avoid undefined behavior when subtracting vectors
		v3.position.z = 0;

		diff = RSP_VectorSubtract(&v1.position, &v0.position);
		diff2 = RSP_VectorSubtract(&v3.position, &v0.position);

		if (fpclassify(diff.x) != FP_ZERO)
			ratioU = diff2.x / diff.x;
		if (fpclassify(diff.y) != FP_ZERO)
			ratioV = diff2.y / diff.y;

		// lerp 1/Z and UV for v3. For perspective texture mapping calculate u/z, v/z.
		{
			float invV0Z = 1.0f / v0.position.z;
			float invV1Z = 1.0f / v1.position.z;
			fixed_t v0x = FLOAT_TO_FIXED(v0.position.x);
			fixed_t v1x = FLOAT_TO_FIXED(v1.position.x);

			// get v3.z value by interpolating 1/z (it's lerp-able)
			if (v0x - v1x)
				v3.position.z = 1.0f / FloatLerp(invV1Z, invV0Z, (v3.position.x - v1.position.x) / (v0.position.x - v1.position.x));
			else
				v3.position.z = v0.position.z;

			v3.uv.u = v3.position.z * FloatLerp(v0.uv.u * invV0Z, v1.uv.u * invV1Z, ratioU);
			v3.uv.v = v3.position.z * FloatLerp(v0.uv.v * invV0Z, v1.uv.v * invV1Z, ratioV);
		}

		// this swap is done to maintain consistent renderer behavior
		if (v3.position.x < v2.position.x)
			RSP_SwapVertex(v3, v2)

		// draw the composition of both triangles to form the desired shape
		if (!is_degenerate_triangle(v0, v3, v2))
		{
			tri->vertices[0] = v0;
			tri->vertices[1] = v3;
			tri->vertices[2] = v2;
			rsp_curtrifunc(tri, TRI_FLATBOTTOM);
#ifdef RSP_DEBUGGING
			rsp_trisdrawn++;
#endif
		}

		if (!is_degenerate_triangle(v1, v3, v2))
		{
			tri->vertices[0] = v1;
			tri->vertices[1] = v3;
			tri->vertices[2] = v2;
			rsp_curtrifunc(tri, TRI_FLATTOP);
#ifdef RSP_DEBUGGING
			rsp_trisdrawn++;
#endif
		}
	}
}

void RSP_DrawTriangleList(rsp_triangle_t *tri, rsp_triangle_t *list, INT32 count)
{
	INT32 i;
	for (i = 0; i < count; i++)
	{
		memcpy(tri->vertices, list[i].vertices, sizeof(rsp_vertex_t) * 3);
		RSP_DrawTriangle(tri);
	}
}

void RSP_TransformTriangle(rsp_triangle_t *tri)
{
	if (rsp_projectionmatrix == NULL)
		I_Error("RSP_TransformTriangle: no projection matrix!");

	// transform triangle to projection matrix
	tri->vertices[0].position = RSP_MatrixMultiplyVector(rsp_projectionmatrix, &tri->vertices[0].position);
	tri->vertices[1].position = RSP_MatrixMultiplyVector(rsp_projectionmatrix, &tri->vertices[1].position);
	tri->vertices[2].position = RSP_MatrixMultiplyVector(rsp_projectionmatrix, &tri->vertices[2].position);

	if (is_triangle_offscreen(&tri->vertices[0], &tri->vertices[1], &tri->vertices[2]))
		return;

	// cull triangle by "normal" direction
	if (rsp_target.cullmode != TRICULL_NONE)
	{
		fpvector4_t d1 = RSP_VectorSubtract(&tri->vertices[1].position, &tri->vertices[0].position);
		fpvector4_t d2 = RSP_VectorSubtract(&tri->vertices[2].position, &tri->vertices[0].position);
		fpvector4_t n = RSP_VectorCrossProduct(&d1, &d2);
		float dot = RSP_VectorDotProduct(&tri->vertices[0].position, &n);
		dot *= -1.0f;
		if (tri->flipped)
			dot *= -1.0f;
		// backside cull
		if (rsp_target.cullmode == TRICULL_BACK && (dot >= 0))
			return;
		// frontside cull
		if (rsp_target.cullmode == TRICULL_FRONT && (dot < 0))
			return;
	}

#ifdef RSP_CLIPTRIANGLES
	if (frustumclipping)
		RSP_ClipTriangle(tri); // clip transformed triangle
	else
#endif
		RSP_DrawTriangle(tri); // draw triangle
}
