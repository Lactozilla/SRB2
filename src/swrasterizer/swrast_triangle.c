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

#include "swrast.h"

static void MapProjectedVertexToScreen(SWRast_Vec4 *vertex)
{
	INT16 sX, sY;

	// This firstly prevents zero division in the following z-divide and
	// secondly "pushes" vertices that are in front of near a little bit forward,
	// which makes them behave a bit better. If all three vertices end up exactly
	// on the near plane, the triangle will be culled.
	vertex->z = vertex->z >= SWRAST_NEAR_CLIPPING_PLANE ? vertex->z : SWRAST_NEAR_CLIPPING_PLANE;

	SWRast_PerspectiveDivide(vertex);
	SWRast_MapProjectionPlaneToScreen(vertex, &sX, &sY);

	vertex->x = sX + SWRastState->screenVertexOffset[0];
	vertex->y = sY + SWRastState->screenVertexOffset[1];
}

static void ProjectVertex(const SWRast_Vertex *vertex, SWRast_Mat4 *projectionMatrix, SWRast_Vec4 *result)
{
	result->x = vertex->position.x;
	result->y = vertex->position.y;
	result->z = vertex->position.z;
	result->w = FRACUNIT; // needed for translation

	SWRast_MultVec3Mat4(result, projectionMatrix);

	result->w = result->z;
}

UINT8 SWRast_ProjectTriangle(const SWRast_Triangle *triangle, SWRast_Mat4 *matrix, SWRast_Vec4 transformed[6])
{
	UINT8 result = 0;

#if SWRAST_NEAR_CROSS_STRATEGY == 2
	UINT8 infront = 0;
	UINT8 behind = 0;
	UINT8 infrontI[3];
	UINT8 behindI[3];

	fixed_t ratio;
	UINT8 i;
#endif

	ProjectVertex(&triangle->vertices[0],matrix,&(transformed[0]));
	ProjectVertex(&triangle->vertices[1],matrix,&(transformed[1]));
	ProjectVertex(&triangle->vertices[2],matrix,&(transformed[2]));

#if SWRAST_NEAR_CROSS_STRATEGY == 2
	for (i = 0; i < 3; ++i)
		if (transformed[i].z < SWRAST_NEAR_CLIPPING_PLANE)
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
		FixedDiv((transformed[be].z - SWRAST_NEAR_CLIPPING_PLANE),\
		(transformed[be].z - transformed[in].z));\
	transformed[in].x = transformed[be].x - \
		FixedDiv((transformed[be].x - transformed[in].x), ratio);\
	transformed[in].y = transformed[be].y -\
		FixedDiv((transformed[be].y - transformed[in].y), ratio);\
	transformed[in].z = SWRAST_NEAR_CLIPPING_PLANE;

	if (infront == 2)
	{
		// shift the two vertices forward along the edge
		for (i = 0; i < 2; ++i)
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

		for (i = 0; i < 2; ++i)
		{
			UINT8 be = behindI[i], in = i + 4;

			interpolateVertex
		}

		transformed[infrontI[0]] = transformed[4];

		MapProjectedVertexToScreen(&transformed[3]);
		MapProjectedVertexToScreen(&transformed[4]);
		MapProjectedVertexToScreen(&transformed[5]);

		result = 1;
	}

#undef interpolateVertex
#endif // SWRAST_NEAR_CROSS_STRATEGY == 2

	MapProjectedVertexToScreen(&transformed[0]);
	MapProjectedVertexToScreen(&transformed[1]);
	MapProjectedVertexToScreen(&transformed[2]);

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
			p0.z <= SWRAST_NEAR_CLIPPING_PLANE || p1.z <= SWRAST_NEAR_CLIPPING_PLANE || p2.z <= SWRAST_NEAR_CLIPPING_PLANE
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
	// cross product for points with z == 0
	INT32 winding = (y1 - y0) * (x2 - x1) - (x1 - x0) * (y2 - y1);
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

	SWRast_MultiplyMatrices(&matrix, &SWRastState->viewProjectionMatrix);

	split = SWRast_ProjectTriangle(tri, &matrix, transformed);

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
