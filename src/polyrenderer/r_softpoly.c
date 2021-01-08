// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2021 by Jaime Ita Passos.
// Copyright (C) 2019 by "Arkus-Kotan".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_softpoly.c
/// \brief Polygon renderer

#include "r_softpoly.h"

rendertarget_t rsp_target;
viewpoint_t rsp_viewpoint;
fpmatrix16_t *rsp_projectionmatrix = NULL;

// Debugging info
#ifdef RSP_DEBUGGING
INT32 rsp_meshesdrawn = 0;
INT32 rsp_trisdrawn = 0;

consvar_t cv_rspdebugdepth = {"rsp_debugdepth", "Off", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
#endif

// init the polygon renderer
void RSP_Init(void)
{
	// set pixel functions
	rsp_basepixelfunc = RSP_DrawPixel;
	rsp_transpixelfunc = RSP_DrawTranslucentPixel;

	// run other initialisation code
	RSP_SetDrawerFunctions();

#ifdef RSP_DEBUGGING
	CV_RegisterVar(&cv_rspdebugdepth);
#endif
}

// make the viewport, after resolution change
void RSP_Viewport(INT32 width, INT32 height)
{
	fixed_t fzoom = R_GetViewMorphZoom();
	float zoom = FIXED_TO_FLOAT(fzoom);
	float fov = FIXED_TO_FLOAT(cv_fov.value);

	// viewport width and height
	rsp_target.width = width;
	rsp_target.height = height;

	// viewport aspect ratio and fov
	rsp_target.aspectratio = (float)rsp_target.width / (float)rsp_target.height;
	rsp_target.fov = fov;
	fov *= (M_PI / 180.f);

	// make depth buffer
	if (rsp_target.depthbuffer)
		Z_Free(rsp_target.depthbuffer);
	rsp_target.depthbuffer = Z_Malloc(sizeof(float) * (rsp_target.width * rsp_target.height), PU_SOFTPOLY, NULL);

	// renderer modes
	rsp_target.mode = (RENDERMODE_COLOR|RENDERMODE_DEPTH);
	rsp_target.cullmode = TRICULL_FRONT;

	// far and near plane (frustum clipping)
	rsp_target.far_plane = 32768.0f;
	rsp_target.near_plane = 16.0f;

	// make projection matrix
	RSP_MakePerspectiveMatrix(&rsp_viewpoint.projection_matrix, fov, zoom, rsp_target.aspectratio, 0.1f, rsp_target.far_plane);
}

// up vector
// right vector not needed
static fpvector4_t upvector = {0.0f, 1.0f, 0.0f, 1.0f};

// make all the vectors and matrixes
static void RSP_SetupFrame(fixed_t vx, fixed_t vy, fixed_t vz, angle_t vangle)
{
	fpmatrix16_t modelview;
	fixed_t angle = AngleFixed(vangle - ANGLE_90);
	float viewang = FIXED_TO_FLOAT(angle);

	// make position and target vectors
	RSP_MakeVector4(rsp_viewpoint.position_vector, FIXED_TO_FLOAT(vx), -FIXED_TO_FLOAT(vz), -FIXED_TO_FLOAT(vy));
	RSP_MakeVector4(rsp_viewpoint.target_vector, 0, 0, -1);

	// make view matrix
	RSP_VectorRotate(&rsp_viewpoint.target_vector, (viewang * M_PI / 180.0f), upvector.x, upvector.y, upvector.z);
	RSP_MakeViewMatrix(&rsp_viewpoint.view_matrix, &rsp_viewpoint.position_vector, &rsp_viewpoint.target_vector, &upvector);

	// make "model view projection" matrix
	// in reality, there is no model matrix
	modelview = RSP_MatrixMultiply(&rsp_viewpoint.view_matrix, &rsp_viewpoint.projection_matrix);
	if (rsp_projectionmatrix == NULL)
		rsp_projectionmatrix = Z_Malloc(sizeof(fpmatrix16_t), PU_SOFTPOLY, NULL);
	M_Memcpy(rsp_projectionmatrix, &modelview, sizeof(fpmatrix16_t));
}

void RSP_ModelView(void)
{
	// set drawer functions
	RSP_SetDrawerFunctions();

	// Clear the depth buffer, and setup the matrixes.
	RSP_ClearDepthBuffer();
	RSP_SetupFrame(viewx, viewy, viewz, viewangle);
}

void RSP_SetDrawerFunctions(void)
{
	// Arkus: Set pixel drawer.
	rsp_curpixelfunc = rsp_basepixelfunc;

	// also set triangle drawer
	rsp_curtrifunc = RSP_TexturedMappedTriangleFP;
}

// on frame start
void RSP_OnFrame(void)
{
	RSP_ModelView();
	rsp_viewwindowx = viewwindowx;
	rsp_viewwindowy = viewwindowy;
	rsp_target.aiming = true;
	rsp_maskdraw = 0;
#ifdef RSP_DEBUGGING
	rsp_meshesdrawn = 0;
	rsp_trisdrawn = 0;
#endif
}

// MASKING STUFF
UINT32 rsp_maskdraw = 0;

// Store the current viewpoint
void RSP_StoreViewpoint(void)
{
	rsp_viewpoint.viewx = viewx;
	rsp_viewpoint.viewy = viewy;
	rsp_viewpoint.viewz = viewz;
	rsp_viewpoint.viewangle = viewangle;
	rsp_viewpoint.aimingangle = aimingangle;
	rsp_viewpoint.viewcos = viewcos;
	rsp_viewpoint.viewsin = viewsin;
}

// Restore the stored viewpoint
void RSP_RestoreViewpoint(void)
{
	viewx = rsp_viewpoint.viewx;
	viewy = rsp_viewpoint.viewy;
	viewz = rsp_viewpoint.viewz;
	viewangle = rsp_viewpoint.viewangle;
	aimingangle = rsp_viewpoint.aimingangle;
	viewcos = rsp_viewpoint.viewcos;
	viewsin = rsp_viewpoint.viewsin;
	RSP_ModelView();
}

// Store a sprite's viewpoint
void RSP_StoreSpriteViewpoint(vissprite_t *spr)
{
	spr->viewx = viewx;
	spr->viewy = viewy;
	spr->viewz = viewz;
	spr->viewangle = viewangle;
	spr->aimingangle = aimingangle;
	spr->viewcos = viewcos;
	spr->viewsin = viewsin;
}

// Set viewpoint to a sprite's viewpoint
void RSP_RestoreSpriteViewpoint(vissprite_t *spr)
{
	viewx = spr->viewx;
	viewy = spr->viewy;
	viewz = spr->viewz;
	viewangle = spr->viewangle;
	aimingangle = spr->aimingangle;
	viewcos = spr->viewcos;
	viewsin = spr->viewsin;
	RSP_ModelView();
}

// clear the depth buffer
void RSP_ClearDepthBuffer(void)
{
	memset(rsp_target.depthbuffer, 0, sizeof(float) * rsp_target.width * rsp_target.height);
}
