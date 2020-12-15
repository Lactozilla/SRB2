// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file hw_gpu.h
/// \brief GPU low-level interface API

#ifndef __HWR_GPU_H__
#define __HWR_GPU_H__

#include "../screen.h"
#include "hw_data.h"
#include "hw_defs.h"
#include "hw_md2.h"

struct GPURenderingAPI
{
	boolean (*Init) (void);

	void (*SetInitialStates) (void);
	void (*SetModelView) (INT32 w, INT32 h);
	void (*SetState) (INT32 State, INT32 Value);
	void (*SetTransform) (FTransform *ptransform);
	void (*SetBlend) (UINT32 PolyFlags);
	void (*SetPalette) (RGBA_t *palette);
	void (*SetDepthBuffer) (void);

	void (*DrawPolygon) (FSurfaceInfo *pSurf, FOutVector *pOutVerts, UINT32 iNumPts, UINT32 PolyFlags);
	void (*DrawIndexedTriangles) (FSurfaceInfo *pSurf, FOutVector *pOutVerts, UINT32 iNumPts, UINT32 PolyFlags, UINT32 *IndexArray);
	void (*Draw2DLine) (F2DCoord *v1, F2DCoord *v2, RGBA_t Color);
	void (*DrawModel) (model_t *model, INT32 frameIndex, INT32 duration, INT32 tics, INT32 nextFrameIndex, FTransform *pos, float scale, UINT8 flipped, UINT8 hflipped, FSurfaceInfo *Surface);
	void (*DrawSkyDome) (FSkyDome *sky);

	void (*SetTexture) (HWRTexture_t *TexInfo);
	void (*UpdateTexture) (HWRTexture_t *TexInfo);
	void (*DeleteTexture) (HWRTexture_t *TexInfo);

	void (*ClearTextureCache) (void);
	INT32 (*GetTextureUsed) (void);

	void (*CreateModelVBOs) (model_t *model);

	void (*ReadRect) (INT32 x, INT32 y, INT32 width, INT32 height, INT32 dst_stride, UINT16 *dst_data);
	void (*GClipRect) (INT32 minx, INT32 miny, INT32 maxx, INT32 maxy, float nearclip);
	void (*ClearBuffer) (boolean ColorMask, boolean DepthMask, FRGBAFloat *ClearColor);

	void (*MakeScreenTexture) (void);
	void (*MakeScreenFinalTexture) (void);
	void (*FlushScreenTextures) (void);

	void (*StartScreenWipe) (void);
	void (*EndScreenWipe) (void);
	void (*DoScreenWipe) (void);
	void (*DoTintedWipe) (boolean istowhite, boolean isfadingin);
	void (*DrawIntermissionBG) (void);
	void (*DrawFinalScreenTexture) (INT32 width, INT32 height);

	void (*PostImgRedraw) (float points[GPU_POSTIMGVERTS][GPU_POSTIMGVERTS][2]);

	boolean (*CompileShaders) (void);
	void (*CleanShaders) (void);
	void (*SetShader) (int type);
	void (*UnSetShader) (void);

	void (*SetShaderInfo) (INT32 info, INT32 value);
	void (*StoreShader) (FShaderProgram *program, UINT32 number, char *code, size_t size, UINT8 stage);
	void (*StoreCustomShader) (FShaderProgram *program, UINT32 number, char *code, size_t size, UINT8 stage);
};

extern struct GPURenderingAPI *GPU;

void GPUInterface_Load(struct GPURenderingAPI **api);
const char *GPUInterface_GetAPIName(void);

#endif // __HWR_GPU_H__
