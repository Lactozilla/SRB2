// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2021 by Jaime Ita Passos.
// Copyright (C) 2019 by "Arkus-Kotan".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_softpoly.h
/// \brief Polygon renderer

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

#ifndef _R_SOFTPOLY_H_
#define _R_SOFTPOLY_H_

#include "rsp_types.h"

// S3L
#include "small3dlib.h"

// Polygon renderer
#define RSP_DEBUGGING

#define RSP_INTERNALUNITSHIFT     S3L_UNIT_SHIFT
#define RSP_INTERNALUNITDIVIDE    ((1<<RSP_INTERNALUNITSHIFT)<<FRACBITS)

#define RSP_MAXANGLE_FP           0x0167FFFC // 359<<FRACBITS
#define RSP_MASKDRAWBIT           0x80000000

void RSP_TransformTriangle(rsp_triangle_t *tri);
void RSP_ProcessFragment(void *pixel);

// pixel drawer functions
extern void (*rsp_curpixelfunc)(void);
extern void (*rsp_basepixelfunc)(void);
extern void (*rsp_transpixelfunc)(void);

extern INT16 rsp_xpix;
extern INT16 rsp_ypix;
extern UINT8 rsp_cpix;

void RSP_DrawPixel(void);
void RSP_DrawTranslucentPixel(void);

typedef S3L_Mat4 rsp_matrix_t;
typedef S3L_Vec4 rsp_lightpos_t;

extern rsp_matrix_t rsp_projectionviewmatrix;
extern rsp_matrix_t rsp_modelmatrix;

extern rsp_lightpos_t rsp_lightpos;

void RSP_Init(void);
void RSP_Viewport(INT32 width, INT32 height);
void RSP_OnFrame(void);
void RSP_ModelView(void);
void RSP_FinishRendering(void);
void RSP_SetDrawerFunctions(void);

fpvector4_t RSP_VectorAdd(fpvector4_t *v1, fpvector4_t *v2);
fpvector4_t RSP_VectorSubtract(fpvector4_t *v1, fpvector4_t *v2);
fpvector4_t RSP_VectorMultiply(fpvector4_t *v, float x);
fpvector4_t RSP_VectorCrossProduct(fpvector4_t *v1, fpvector4_t *v2);
float RSP_VectorDotProduct(fpvector4_t *v1, fpvector4_t *v2);
float RSP_VectorLength(fpvector4_t *v);
float RSP_VectorInverseLength(fpvector4_t *v);
float RSP_VectorDistance(fpvector4_t p, fpvector4_t pn, fpvector4_t pp);
void RSP_VectorNormalize(fpvector4_t *v);
void RSP_VectorRotate(fpvector4_t *v, float angle, float x, float y, float z);

fpquaternion_t RSP_QuaternionMultiply(fpquaternion_t *q1, fpquaternion_t *q2);
fpquaternion_t RSP_QuaternionConjugate(fpquaternion_t *q);
fpquaternion_t RSP_QuaternionFromEuler(float z, float y, float x);
fpvector4_t RSP_QuaternionMultiplyVector(fpquaternion_t *q, fpvector4_t *v);
void RSP_QuaternionNormalize(fpquaternion_t *q);
void RSP_QuaternionRotateVector(fpvector4_t *v, fpquaternion_t *q);

#define RSP_MakeVector4(vec, vx, vy, vz) { vec.x = vx; vec.y = vy; vec.z = vz; vec.w = 1.0; }

#define RSP_AngleToFixedPoint(x, offset) AngleFixed((x) - offset)
#define RSP_AngleToInternalUnit(x, offset) FixedDiv(FixedDiv(RSP_AngleToFixedPoint(x, offset), RSP_MAXANGLE_FP), RSP_INTERNALUNITDIVIDE)

#if S3L_PRECISE_ANGLES != 0
#define RSP_AngleToInternalAngle(x) RSP_AngleToFixedPoint(x, ANGLE_90)
#else
#define RSP_AngleToInternalAngle(x) RSP_AngleToInternalUnit(x, ANGLE_90)
#endif

#define FloatLerp(start, end, r) ( (start) * (1.0 - (r)) + (end) * (r) )

// Debugging info
#ifdef RSP_DEBUGGING
extern INT32 rsp_meshesdrawn;
extern INT32 rsp_trisdrawn;
#endif

// MASKING STUFF
void RSP_StoreViewpoint(void);
void RSP_RestoreViewpoint(void);
void RSP_StoreSpriteViewpoint(vissprite_t *spr);
void RSP_RestoreSpriteViewpoint(vissprite_t *spr);

extern UINT32 rsp_maskdraw;

// 3D models
boolean RSP_RenderModel(vissprite_t *spr);
void RSP_CreateModelTexture(modelinfo_t *model, INT32 tcnum, skincolornum_t skincolor);
void RSP_FreeModelTexture(modelinfo_t *model);
void RSP_FreeModelBlendTexture(modelinfo_t *model);

#endif
