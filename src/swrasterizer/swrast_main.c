// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2021 by Jaime Ita Passos.
// Copyright (C) 2019 by "Arkus-Kotan".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  swrast_main.c
/// \brief Software rasterizer

#include "swrast.h"

SWRast_State *SWRastState = NULL;

static SWRast_RenderTarget *renderTarget = NULL;
static SWRast_Viewpoint *sceneViewpoint = NULL;

static void InitRenderTarget(SWRast_RenderTarget *target);

void SWRast_Init(void)
{
	SWRastState = Z_Calloc(sizeof(SWRast_State), PU_SWRASTERIZER, NULL);

	renderTarget = &(SWRastState->renderTarget);
	sceneViewpoint = &(SWRastState->viewpoint);

	SWRast_SetColormap(NULL);
	SWRast_SetTranslation(NULL);
	SWRast_SetTranslucencyTable(NULL);

	SWRast_SetClipRanges(0, 0);
	SWRast_SetClipTables(NULL, NULL);

	SWRast_SetMask(0);
	SWRast_SetCullMode(SWRAST_CULL_FRONT);

	SWRast_BindTexture(NULL);
	SWRast_DisableBlending();

	SWRastState->vertexShader = NULL;
	SWRastState->fragmentShader = SWRast_ProcessFragment;

	SWRast_InitTransform3D(&(sceneViewpoint->transform));

	InitRenderTarget(&SWRastState->renderTarget);

	SWRastState->numRenderedMeshes = SWRastState->numRenderedTriangles = 0;
}

static void InitRenderTarget(SWRast_RenderTarget *target)
{
	target->fragmentRead = target->fragmentWrite = 0;
}

void SWRast_EnableBlending(void)
{
	if (SWRastState)
		SWRastState->pixelFunction = SWRast_DrawBlendedPixel;
}

void SWRast_DisableBlending(void)
{
	if (SWRastState)
		SWRastState->pixelFunction = SWRast_DrawPixel;
}

boolean SWRast_BlendingEnabled(void)
{
	if (SWRastState == NULL)
		return false;

	return (SWRastState->pixelFunction == SWRast_DrawBlendedPixel);
}

UINT8 SWRast_GetFragmentRead(void)
{
	if (SWRastState == NULL)
		return 0;

	return SWRastState->renderTarget.fragmentRead;
}

UINT8 SWRast_GetFragmentWrite(void)
{
	if (SWRastState == NULL)
		return 0;

	return SWRastState->renderTarget.fragmentWrite;
}

UINT32 SWRast_GetMask(void)
{
	if (SWRastState == NULL)
		return 0;

	return SWRastState->maskDraw;
}

void SWRast_SetMask(UINT32 mask)
{
	if (SWRastState)
		SWRastState->maskDraw = mask;
}

lighttable_t *SWRast_GetColormap(void)
{
	if (SWRastState == NULL)
		return NULL;

	return SWRastState->colormap;
}

void SWRast_SetColormap(lighttable_t *colormap)
{
	if (SWRastState)
		SWRastState->colormap = colormap;
}

lighttable_t *SWRast_GetTranslation(void)
{
	if (SWRastState == NULL)
		return NULL;

	return SWRastState->translation;
}

void SWRast_SetTranslation(lighttable_t *translation)
{
	if (SWRastState)
		SWRastState->translation = translation;
}

UINT8 *SWRast_GetTranslucencyTable(void)
{
	if (SWRastState == NULL)
		return NULL;

	return SWRastState->translucencyTable;
}

void SWRast_SetTranslucencyTable(UINT8 *table)
{
	if (SWRastState)
		SWRastState->translucencyTable = table;
}

SWRast_Texture *SWRast_GetTexture(void)
{
	if (SWRastState == NULL)
		return NULL;

	return SWRastState->texture;
}

void SWRast_BindTexture(SWRast_Texture *texture)
{
	if (SWRastState)
		SWRastState->texture = texture;
}

void SWRast_SetClipRanges(INT16 clipleft, INT16 clipright)
{
	if (SWRastState == NULL)
		return;

	SWRastState->spanPortalClip[0] = clipleft;
	SWRastState->spanPortalClip[1] = clipright;
}

void SWRast_SetClipTables(INT16 *clipbot, INT16 *cliptop)
{
	if (SWRastState == NULL)
		return;

	SWRastState->spanBottomClip = clipbot;
	SWRastState->spanTopClip = cliptop;
}

void SWRast_SetLightPos(fixed_t x, fixed_t y, fixed_t z)
{
	if (SWRastState && SWRastState->modelLighting)
	{
		SWRastState->modelLightPos.x = x;
		SWRastState->modelLightPos.y = y;
		SWRastState->modelLightPos.z = z;
		SWRast_NormalizeVec3_Safe(&(SWRastState->modelLightPos));
	}
}

void SWRast_SetVertexLights(fixed_t l0, fixed_t l1, fixed_t l2)
{
	if (SWRastState && SWRastState->computeLight)
	{
		SWRastState->vertexLights[0] = l0;
		SWRastState->vertexLights[1] = l1;
		SWRastState->vertexLights[2] = l2;
	}
}

UINT8 SWRast_GetCullMode(void)
{
	if (SWRastState == NULL)
		return 0;

	return SWRastState->cullMode;
}

void SWRast_SetCullMode(UINT8 mode)
{
	if (SWRastState)
		SWRastState->cullMode = mode;
}

void SWRast_EnableFragmentRead(UINT8 flags)
{
	if (SWRastState)
		SWRastState->renderTarget.fragmentRead |= flags;
}

void SWRast_EnableFragmentWrite(UINT8 flags)
{
	if (SWRastState)
		SWRastState->renderTarget.fragmentWrite |= flags;
}

void SWRast_DisableFragmentRead(UINT8 flags)
{
	if (SWRastState)
		SWRastState->renderTarget.fragmentRead &= ~flags;
}

void SWRast_DisableFragmentWrite(UINT8 flags)
{
	if (SWRastState)
		SWRastState->renderTarget.fragmentWrite &= ~flags;
}

static void SWRast_SetViewTransform(fixed_t x, fixed_t y, fixed_t z, angle_t angle)
{
	SWRast_State *state = SWRastState;
	SWRast_Transform3D *transform;

	if (state == NULL)
		return;

	transform = &(sceneViewpoint->transform);
	transform->translation.x = x;
	transform->translation.y = z;
	transform->translation.z = y;
	transform->rotation.x = 0;
	transform->rotation.y = ANGLE_90 - angle;
	transform->rotation.z = 0;
	transform->scale.z = state->fov;

	SWRast_MakeViewProjectionMatrix(transform, &state->viewProjectionMatrix);

	state->viewWindow[SWRAST_VIEW_WINDOW_X1] = viewwindowx;
	state->viewWindow[SWRAST_VIEW_WINDOW_X2] = viewwindowx + state->renderTarget.width;

	state->viewWindow[SWRAST_VIEW_WINDOW_Y1] = viewwindowy;
	state->viewWindow[SWRAST_VIEW_WINDOW_Y2] = viewwindowy + state->renderTarget.height;

	state->screenVertexOffset[0] = viewwindowx;
	state->screenVertexOffset[1] = viewwindowy + (centery - (viewheight/2));
}

void SWRast_SetViewport(INT32 width, INT32 height)
{
	if (SWRastState == NULL)
		return;

	SWRastState->fov = fovtan;

	SWRast_SetCullMode(SWRAST_CULL_FRONT);

	InitRenderTarget(renderTarget);

	renderTarget->width = width;
	renderTarget->height = height;

	SWRast_EnableFragmentRead(SWRAST_READ_COLOR | SWRAST_READ_DEPTH);
	SWRast_EnableFragmentWrite(SWRAST_WRITE_COLOR | SWRAST_WRITE_DEPTH);

	renderTarget->colorBuffer = screens[0];

#if SWRAST_Z_BUFFER != 0
	if (renderTarget->zBuffer)
		Z_Free(renderTarget->zBuffer);
	renderTarget->zBuffer = Z_Calloc(vid.width * vid.height * sizeof(SWRast_ZBuffer), PU_SWRASTERIZER, NULL);
#endif

#if SWRAST_STENCIL_BUFFER
	if (renderTarget->stencilBuffer)
		Z_Free(renderTarget->stencilBuffer);

	renderTarget->stencilBufferSize = ((vid.width * vid.height - 1) / 8 + 1) * sizeof(UINT8);
	renderTarget->stencilBuffer = Z_Calloc(renderTarget->stencilBufferSize, PU_SWRASTERIZER, NULL);
#endif
}

void SWRast_SetModelView(void)
{
	if (SWRastState == NULL)
		return;

	SWRast_SetViewTransform(viewx, viewy, viewz, viewangle);

	SWRast_DisableBlending(); // Arkus: Set pixel drawer.

	SWRast_ZBufferClear();
	SWRast_StencilBufferClear();

	SWRastState->modelLighting = !!(cv_modellighting.value);
	SWRastState->computeLight = SWRastState->modelLighting;
}

void SWRast_OnFrame(void)
{
	if (SWRastState == NULL)
		return;

	SWRastState->numRenderedMeshes = 0;
	SWRastState->numRenderedTriangles = 0;
}

void SWRast_OnPlayerFrame(void)
{
	if (SWRastState == NULL)
		return;

	SWRast_SetModelView();
	SWRast_SetMask(0);
}

void SWRast_ZBufferClear(void)
{
#if SWRAST_Z_BUFFER != 0
	INT32 i;

	if (renderTarget == NULL)
		return;

	for (i = 0; i < vid.width * vid.height; i++)
		renderTarget->zBuffer[i] = SWRAST_MAX_ZBUFFER_DEPTH;
#endif
}

void SWRast_StencilBufferClear(void)
{
#if SWRAST_STENCIL_BUFFER
	size_t i;

	if (renderTarget == NULL)
		return;

	for (i = 0; i < renderTarget->stencilBufferSize; i++)
		renderTarget->stencilBuffer[i] = 0;
#endif
}

#define MACRO_UNUSED(what) (void)(what) ///< helper macro for unused vars

fixed_t SWRast_ZBufferRead(INT16 x, INT16 y)
{
#if SWRAST_Z_BUFFER != 0
	if (!SWRAST_DEPTH_BUFFER_POINTER)
		return SWRAST_MAX_ZBUFFER_DEPTH;

	return SWRAST_DEPTH_BUFFER_POINTER[y * SWRAST_RESOLUTION_X + x];
#else
	MACRO_UNUSED(x);
	MACRO_UNUSED(y);

	return 0;
#endif
}

void SWRast_ZBufferWrite(INT16 x, INT16 y, fixed_t value)
{
#if SWRAST_Z_BUFFER != 0
	if (SWRAST_DEPTH_BUFFER_POINTER)
		SWRAST_DEPTH_BUFFER_POINTER[y * SWRAST_RESOLUTION_X + x] = value;
#else
	MACRO_UNUSED(x);
	MACRO_UNUSED(y);
	MACRO_UNUSED(value);
#endif
}

// Store the current viewpoint
void SWRast_ViewpointStore(void)
{
	if (sceneViewpoint == NULL)
		return;

	sceneViewpoint->viewx = viewx;
	sceneViewpoint->viewy = viewy;
	sceneViewpoint->viewz = viewz;
	sceneViewpoint->viewangle = viewangle;
	sceneViewpoint->aimingangle = aimingangle;
	sceneViewpoint->viewcos = viewcos;
	sceneViewpoint->viewsin = viewsin;
}

// Restore the stored viewpoint
void SWRast_ViewpointRestore(void)
{
	if (sceneViewpoint == NULL)
		return;

	viewx = sceneViewpoint->viewx;
	viewy = sceneViewpoint->viewy;
	viewz = sceneViewpoint->viewz;
	viewangle = sceneViewpoint->viewangle;
	aimingangle = sceneViewpoint->aimingangle;
	viewcos = sceneViewpoint->viewcos;
	viewsin = sceneViewpoint->viewsin;

	SWRast_SetModelView();
}

// Store a sprite's viewpoint
void SWRast_ViewpointStoreFromSprite(vissprite_t *spr)
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
void SWRast_ViewpointRestoreFromSprite(vissprite_t *spr)
{
	viewx = spr->viewx;
	viewy = spr->viewy;
	viewz = spr->viewz;
	viewangle = spr->viewangle;
	aimingangle = spr->aimingangle;
	viewcos = spr->viewcos;
	viewsin = spr->viewsin;
	SWRast_SetModelView();
}

INT32 SWRast_GetNumRenderedMeshes(void)
{
	if (SWRastState == NULL)
		return 0;

	return SWRastState->numRenderedMeshes;
}

INT32 SWRast_GetNumRenderedTriangles(void)
{
	if (SWRastState == NULL)
		return 0;

	return SWRastState->numRenderedTriangles;
}

void SWRast_AddNumRenderedMeshes(void)
{
	if (SWRastState)
		SWRastState->numRenderedMeshes++;
}

void SWRast_AddNumRenderedTriangles(void)
{
	if (SWRastState)
		SWRastState->numRenderedTriangles++;
}
