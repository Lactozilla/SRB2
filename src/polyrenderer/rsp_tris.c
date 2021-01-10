// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2021 by Jaime Ita Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  rsp_tris.c
/// \brief Triangle drawer

#include "r_softpoly.h"

void RSP_TransformTriangle(rsp_triangle_t *tri)
{
	S3L_Mat4 matrix;
	S3L_Vec4 transformed[6]; // transformed triangle coords, for 2 triangles
	UINT8 split, i, j;

	rsp_state.triangle = tri;

	for (j = 0; j < 4; ++j)
		for (i = 0; i < 4; ++i)
			matrix[i][j] = rsp_modelmatrix[i][j];

	S3L_mat4Xmat4(&matrix, &rsp_projectionviewmatrix);

	S3L_spanPortalClip[0] = rsp_portalclip[0];
	S3L_spanPortalClip[1] = rsp_portalclip[1];
	S3L_spanFloorClip     = rsp_mfloorclip;
	S3L_spanCeilingClip   = rsp_mceilingclip;

	split = S3L_projectTriangle(tri, &matrix, S3L_FRACTIONS_PER_UNIT, transformed);

	if (S3L_triangleIsVisible(transformed[0], transformed[1], transformed[2], rsp_target.cull))
	{
		S3L_drawTriangle(transformed[0], transformed[1], transformed[2]);

#ifdef RSP_DEBUGGING
		rsp_trisdrawn++;
#endif

		// draw potential subtriangle
		if (split)
		{
			S3L_drawTriangle(transformed[3], transformed[4], transformed[5]);
#ifdef RSP_DEBUGGING
			rsp_trisdrawn++;
#endif
		}
	}
}
