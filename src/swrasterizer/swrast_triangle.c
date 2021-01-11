// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2021 by Jaime Ita Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  swrast_tris.c
/// \brief Triangle rendering

#include "swrast.h"

static void MapProjectedVertexToScreen(SWRast_Vec4 *vertex, fixed_t focalLength)
{
	INT16 sX, sY;

	vertex->z = vertex->z >= SWRAST_NEAR ? vertex->z : SWRAST_NEAR;
	/* ^ This firstly prevents zero division in the follwoing z-divide and
		secondly "pushes" vertices that are in front of near a little bit forward,
		which makes them behave a bit better. If all three vertices end up exactly
		on NEAR, the triangle will be culled. */

	SWRast_PerspectiveDivide(vertex,focalLength);
	SWRast_MapProjectionPlaneToScreen(*vertex,&sX,&sY);

	vertex->x = sX + SWRastState->screenVertexOffset[0];
	vertex->y = sY + SWRastState->screenVertexOffset[1];
}

static void ProjectVertex(
	const SWRast_Vertex *vertex,
	SWRast_Mat4 *projectionMatrix,
	SWRast_Vec4 *result)
{
	result->x = vertex->position.x;
	result->y = vertex->position.y;
	result->z = vertex->position.z;
	result->w = FRACUNIT; // needed for translation

	SWRast_MultVec3Mat4(result,projectionMatrix);

	result->w = result->z;
	/* We'll keep the non-clamped z in w for sorting. */
}

/**
	Projects a triangle to the screen. If enabled, a triangle can be potentially
	subdivided into two if it crosses the near plane, in which case two projected
	triangles are returned (return value will be 1).
*/
UINT8 SWRast_ProjectTriangle(
	const SWRast_Triangle *triangle,
	SWRast_Mat4 *matrix,
	UINT32 focalLength,
	SWRast_Vec4 transformed[6])
{
	UINT8 result = 0;

#if SWRAST_NEAR_CROSS_STRATEGY == 2
	UINT8 infront = 0;
	UINT8 behind = 0;
	UINT8 infrontI[3];
	UINT8 behindI[3];

	fixed_t ratio;
#endif

	ProjectVertex(&triangle->vertices[0],matrix,&(transformed[0]));
	ProjectVertex(&triangle->vertices[1],matrix,&(transformed[1]));
	ProjectVertex(&triangle->vertices[2],matrix,&(transformed[2]));

#if SWRAST_NEAR_CROSS_STRATEGY == 2
	for (UINT8 i = 0; i < 3; ++i)
		if (transformed[i].z < SWRAST_NEAR)
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
		FixedDiv((transformed[be].z - SWRAST_NEAR),\
		(transformed[be].z - transformed[in].z));\
	transformed[in].x = transformed[be].x - \
		FixedDiv((transformed[be].x - transformed[in].x), ratio);\
	transformed[in].y = transformed[be].y -\
		FixedDiv((transformed[be].y - transformed[in].y), ratio);\
	transformed[in].z = SWRAST_NEAR;

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

		MapProjectedVertexToScreen(&transformed[3],focalLength);
		MapProjectedVertexToScreen(&transformed[4],focalLength);
		MapProjectedVertexToScreen(&transformed[5],focalLength);

		result = 1;
	}

#undef interpolateVertex
#endif // SWRAST_NEAR_CROSS_STRATEGY == 2

	MapProjectedVertexToScreen(&transformed[0],focalLength);
	MapProjectedVertexToScreen(&transformed[1],focalLength);
	MapProjectedVertexToScreen(&transformed[2],focalLength);

	return result;
}

/**
	Checks if given triangle (in Screen Space) is at least partially visible,
	i.e. returns false if the triangle is either completely outside the frustum
	(left, right, top, bottom, near) or is invisible due to backface culling.
*/
static SINT8 TriangleIsVisible(
	SWRast_Vec4 p0,
	SWRast_Vec4 p1,
	SWRast_Vec4 p2,
	UINT8 backfaceCulling)
{
	#define clipTest(c,cmp,v)\
		(p0.c cmp (v) && p1.c cmp (v) && p2.c cmp (v))

	if ( // outside frustum?
#if SWRAST_NEAR_CROSS_STRATEGY == 0
			p0.z <= SWRAST_NEAR || p1.z <= SWRAST_NEAR || p2.z <= SWRAST_NEAR
			// ^ partially in front of NEAR?
#else
			clipTest(z,<=,SWRAST_NEAR) // completely in front of NEAR?
#endif
#if 0
			|| clipTest(x-SWRastState->screenVertexOffset[0],<,SWRastState->viewWindow[SWRAST_VIEW_WINDOW_X1]) ||
				 clipTest(x-SWRastState->screenVertexOffset[0],>=,SWRastState->viewWindow[SWRAST_VIEW_WINDOW_X2]) ||
				 clipTest(y-SWRastState->screenVertexOffset[1],<,SWRastState->viewWindow[SWRAST_VIEW_WINDOW_Y1]) ||
				 clipTest(y-SWRastState->screenVertexOffset[2],>,SWRastState->viewWindow[SWRAST_VIEW_WINDOW_Y2])
#endif
		)
		return 0;

	#undef clipTest

	if (backfaceCulling != SWRAST_CULL_NONE)
	{
		SINT8 winding = SWRast_TriangleWinding(p0.x,p0.y,p1.x,p1.y,p2.x,p2.y);

		if ((backfaceCulling == SWRAST_CULL_BACK && winding > 0)
		|| (backfaceCulling == SWRAST_CULL_FRONT && winding < 0))
			return 0;
	}

	return 1;
}

SINT8 SWRast_TriangleWinding(
	INT16 x0,
	INT16 y0,
	INT16 x1,
	INT16 y1,
	INT16 x2,
	INT16 y2)
{
	INT32 winding =
		(y1 - y0) * (x2 - x1) - (x1 - x0) * (y2 - y1);
		// ^ cross product for points with z == 0

	return winding > 0 ? 1 : (winding < 0 ? -1 : 0);
}

void SWRast_RenderTriangle(SWRast_Triangle *tri)
{
	SWRast_Mat4 matrix;
	SWRast_Vec4 transformed[6]; // transformed triangle coords, for 2 triangles
	UINT8 split, i, j;

	SWRastState->triangle = tri;

	for (j = 0; j < 4; ++j)
		for (i = 0; i < 4; ++i)
			matrix[i][j] = SWRastState->modelMatrix[i][j];

	SWRast_MultiplyMatrices(&matrix, &SWRastState->projectionViewMatrix);

	split = SWRast_ProjectTriangle(tri, &matrix, FRACUNIT, transformed);

	if (TriangleIsVisible(transformed[0], transformed[1], transformed[2], SWRast_GetCullMode()))
	{
		SWRast_RasterizeTriangle(transformed[0], transformed[1], transformed[2]);
		SWRast_AddNumRenderedTriangles();

		// draw potential subtriangle
		if (split)
		{
			SWRast_RasterizeTriangle(transformed[3], transformed[4], transformed[5]);
			SWRast_AddNumRenderedTriangles();
		}
	}
}
