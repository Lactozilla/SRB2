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

rsp_state_t     rsp_state;
rsp_target_t    rsp_target;
rsp_viewpoint_t rsp_viewpoint;

rsp_matrix_t rsp_projectionviewmatrix;
rsp_matrix_t rsp_modelmatrix;

rsp_lightpos_t rsp_lightpos;

static S3L_Camera rsp_camera;

// Debugging info
#ifdef RSP_DEBUGGING
INT32 rsp_meshesdrawn = 0;
INT32 rsp_trisdrawn = 0;
#endif

// init the polygon renderer
void RSP_Init(void)
{
	// set pixel functions
	rsp_basepixelfunc = RSP_DrawPixel;
	rsp_transpixelfunc = RSP_DrawTranslucentPixel;

	// run other initialisation code
	RSP_SetDrawerFunctions();

	// S3L
	S3L_pixelFunction = RSP_ProcessFragment;
	S3L_initCamera(&rsp_camera);
}

static void RSP_SetupFrame(fixed_t x, fixed_t y, fixed_t z, angle_t angle)
{
	S3L_Transform3D *transform = &rsp_camera.transform;

	transform->translation.x = FixedDiv(x, RSP_INTERNALUNITDIVIDE);
	transform->translation.y = FixedDiv(z, RSP_INTERNALUNITDIVIDE);
	transform->translation.z = FixedDiv(y, RSP_INTERNALUNITDIVIDE);
	transform->rotation.x = 0;
	transform->rotation.y = RSP_AngleToInternalAngle(angle);
	transform->rotation.z = 0;
	transform->scale.z = rsp_target.fov;

	S3L_makeCameraMatrix(transform, &rsp_projectionviewmatrix);

	S3L_viewWindow[S3L_VIEW_WINDOW_X1] = viewwindowx;
	S3L_viewWindow[S3L_VIEW_WINDOW_X2] = viewwindowx + rsp_target.width;

	S3L_viewWindow[S3L_VIEW_WINDOW_Y1] = viewwindowy;
	S3L_viewWindow[S3L_VIEW_WINDOW_Y2] = viewwindowy + rsp_target.height;

	S3L_screenVertexOffsetX = viewwindowx;
	S3L_screenVertexOffsetY = viewwindowy + (centery - (viewheight/2));
}

// make the viewport, after resolution change
void RSP_Viewport(INT32 width, INT32 height)
{
	rsp_target.width = width;
	rsp_target.height = height;

	rsp_target.fov = FixedDiv(fovtan, RSP_INTERNALUNITDIVIDE);
	rsp_target.aspect = (float)rsp_target.width / (float)rsp_target.height;

	rsp_target.read = (RSP_READ_COLOR | RSP_READ_DEPTH);
	rsp_target.write = (RSP_WRITE_COLOR | RSP_WRITE_DEPTH);
	rsp_target.cull = RSP_CULL_FRONT;

	// Setup S3L
	S3L_resolutionX = rsp_target.width;
	S3L_resolutionY = rsp_target.height;

	if (S3L_zBuffer)
		Z_Free(S3L_zBuffer);
	S3L_zBuffer = Z_Calloc(width * height * sizeof(S3L_ZBUFFER_TYPE), PU_SOFTPOLY, NULL);
}

void RSP_ModelView(void)
{
	RSP_SetupFrame(viewx, viewy, viewz, viewangle);

	if (cv_modellighting.value)
	{
		rsp_target.read |= RSP_READ_LIGHT;
		rsp_target.write |= RSP_WRITE_LIGHT;
	}
	else
	{
		rsp_target.read &= ~RSP_READ_LIGHT;
		rsp_target.write &= ~RSP_WRITE_LIGHT;
	}

	RSP_SetDrawerFunctions();

	S3L_newFrame();
}

void RSP_SetDrawerFunctions(void)
{
	// Arkus: Set pixel drawer.
	rsp_curpixelfunc = rsp_basepixelfunc;
}

// on frame start
void RSP_OnFrame(void)
{
	RSP_ModelView();

	rsp_maskdraw = 0;
}

// on rendering end
void RSP_FinishRendering(void)
{
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
