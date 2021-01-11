// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2021 by Jaime Ita Passos.
// Copyright (C) 2019 by "Arkus-Kotan".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  swrast.h
/// \brief Software rasterizer

#include "../doomtype.h"
#include "../doomstat.h"
#include "../g_game.h"
#include "../i_video.h"
#include "../r_data.h"
#include "../r_picformats.h"
#include "../r_defs.h"
#include "../r_main.h"
#include "../r_draw.h"
#include "../r_things.h"
#include "../r_model.h"
#include "../p_local.h"
#include "../v_video.h"
#include "../z_zone.h"
#include "../w_wad.h"

#ifndef _SWRAST_H_
#define _SWRAST_H_

#include "swrast_defs.h"
#include "swrast_types.h"
#include "swrast_math.h"

void SWRast_Init(void);
void SWRast_OnFrame(void);
void SWRast_OnPlayerFrame(void);

SWRast_Texture *SWRast_GetTexture(void);
void SWRast_BindTexture(SWRast_Texture *texture);

UINT8 SWRast_GetFragmentRead(void);
UINT8 SWRast_GetFragmentWrite(void);

void SWRast_EnableBlending(void);
void SWRast_DisableBlending(void);
boolean SWRast_BlendingEnabled(void);

void SWRast_EnableFragmentRead(UINT8 flags);
void SWRast_EnableFragmentWrite(UINT8 flags);
void SWRast_DisableFragmentRead(UINT8 flags);
void SWRast_DisableFragmentWrite(UINT8 flags);

UINT32 SWRast_GetMask(void);
void SWRast_SetMask(UINT32 mask);

lighttable_t *SWRast_GetColormap(void);
void SWRast_SetColormap(lighttable_t *colormap);

lighttable_t *SWRast_GetTranslation(void);
void SWRast_SetTranslation(lighttable_t *translation);

UINT8 *SWRast_GetTranslucencyTable(void);
void SWRast_SetTranslucencyTable(UINT8 *table);

UINT8 SWRast_GetCullMode(void);
void SWRast_SetCullMode(UINT8 mode);

void SWRast_SetViewport(INT32 width, INT32 height);
void SWRast_SetModelView(void);
void SWRast_SetLightPos(fixed_t x, fixed_t y, fixed_t z);
void SWRast_SetVertexLights(fixed_t l0, fixed_t l1, fixed_t l2);

void SWRast_SetClipRanges(INT16 clipleft, INT16 clipright);
void SWRast_SetClipTables(INT16 *clipbot, INT16 *cliptop);

void SWRast_InitCamera(SWRast_Camera *cam);

void SWRast_RenderTriangle(SWRast_Triangle *tri);

UINT8 SWRast_ProcessFragment(SWRast_Fragment *p);

void SWRast_DrawPixel(INT32 x, INT32 y, UINT8 color);
void SWRast_DrawBlendedPixel(INT32 x, INT32 y, UINT8 color);

void SWRast_ZBufferClear(void);
void SWRast_StencilBufferClear(void);

void SWRast_ViewpointStore(void);
void SWRast_ViewpointRestore(void);
void SWRast_ViewpointStoreFromSprite(vissprite_t *spr);
void SWRast_ViewpointRestoreFromSprite(vissprite_t *spr);

boolean SWRast_RenderModel(vissprite_t *spr);
void SWRast_CreateModelTexture(modelinfo_t *model, INT32 tcnum, skincolornum_t skincolor);
void SWRast_FreeModelTexture(modelinfo_t *model);
void SWRast_FreeModelBlendTexture(modelinfo_t *model);

INT32 SWRast_GetNumRenderedMeshes(void);
INT32 SWRast_GetNumRenderedTriangles(void);

void SWRast_AddNumRenderedMeshes(void);
void SWRast_AddNumRenderedTriangles(void);

/** Draws a triangle according to given config. The vertices are specified in
	Screen Space space (pixels). If perspective correction is enabled, each
	vertex has to have a depth (Z position in camera space) specified in the Z
	component. */
void SWRast_RasterizeTriangle(
	SWRast_Vec4 point0,
	SWRast_Vec4 point1,
	SWRast_Vec4 point2);

/** Writes a value (not necessarily depth! depends on the format of z-buffer)
	to z-buffer (if enabled). Does NOT check boundaries! */
void SWRast_ZBufferWrite(INT16 x, INT16 y, fixed_t value);

/** Reads a value (not necessarily depth! depends on the format of z-buffer)
	from z-buffer (if enabled). Does NOT check boundaries! */
fixed_t SWRast_ZBufferRead(INT16 x, INT16 y);

/**
	Projects a triangle to the screen. If enabled, a triangle can be potentially
	subdivided into two if it crosses the near plane, in which case two projected
	triangles are returned (return value will be 1).
*/
UINT8 SWRast_ProjectTriangle(
	const SWRast_Triangle *triangle,
	SWRast_Mat4 *matrix,
	UINT32 focalLength,
	SWRast_Vec4 transformed[6]);

/** Determines the winding of a triangle, returns 1 (CW, clockwise), -1 (CCW,
	counterclockwise) or 0 (points lie on a single line). */
SINT8 SWRast_TriangleWinding(
	INT16 x0,
	INT16 y0,
	INT16 x1,
	INT16 y1,
	INT16 x2,
	INT16 y2);

#endif // _SWRAST_H_
