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

//#define RSP_CLIPTRIANGLES
#define RSP_DEBUGGING
#define RSP_SPANSTEPPING

#define FixedLerp(start, end, r) ( FixedMul(start, (FRACUNIT - (r))) + FixedMul(end, r) )
#define FloatLerp(start, end, r) ( (start) * (1.0 - (r)) + (end) * (r) )

typedef struct
{
	float x, y, z;
} fpvector3_t;

typedef struct
{
	float x, y, z, w;
} fpvector4_t;

typedef struct
{
	float x, y, z, w;
} fpquaternion_t;

typedef struct
{
	float u, v;
} fpvector2_t;

typedef struct
{
	float m[16];
} fpmatrix16_t;

#define RSP_MakeVector4(vec, vx, vy, vz) { vec.x = vx; vec.y = vy; vec.z = vz; vec.w = 1.0; }
#define RSP_MakeVector3(vec, vx, vy, vz) { vec.x = vx; vec.y = vy; vec.z = vz; }
#define RSP_MakeVector2(vec, vu, vv) { vec.u = vu; vec.y = vv; }

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

fpvector4_t RSP_MatrixMultiplyVector(fpmatrix16_t *m, fpvector4_t *v);
fpmatrix16_t RSP_MatrixMultiply(fpmatrix16_t *m1, fpmatrix16_t *m2);
void RSP_MatrixTranspose(fpmatrix16_t *m);

void RSP_MakeIdentityMatrix(fpmatrix16_t *m);
void RSP_MakePerspectiveMatrix(fpmatrix16_t *m, float fov, float zoomneeded, float aspectratio, float np, float fp);
void RSP_MakeViewMatrix(fpmatrix16_t *m, fpvector4_t *eye, fpvector4_t *target, fpvector4_t *up);
fpvector4_t RSP_IntersectPlane(fpvector4_t pp, fpvector4_t pn, fpvector4_t start, fpvector4_t end, float *t);

fpquaternion_t RSP_QuaternionMultiply(fpquaternion_t *q1, fpquaternion_t *q2);
fpquaternion_t RSP_QuaternionConjugate(fpquaternion_t *q);
fpquaternion_t RSP_QuaternionFromEuler(float z, float y, float x);
fpvector4_t RSP_QuaternionMultiplyVector(fpquaternion_t *q, fpvector4_t *v);
void RSP_QuaternionNormalize(fpquaternion_t *q);
void RSP_QuaternionRotateVector(fpvector4_t *v, fpquaternion_t *q);

#define RSP_SwapVertex(v1, v2) { rsp_vertex_t s = v2; v2 = v1; v1 = s; }

// =======================
//      Render target
// =======================

typedef enum
{
	RENDERMODE_COLOR = 1,
	RENDERMODE_DEPTH = 2,
} rsp_mode_t;

typedef enum
{
	TRI_FLATTOP,
	TRI_FLATBOTTOM,
} rsp_trimode_t;

typedef enum
{
	TRICULL_NONE,
	TRICULL_FRONT,
	TRICULL_BACK,
} rsp_cullmode_t;

typedef struct
{
	fpvector4_t position;
	fpvector2_t uv;
} rsp_vertex_t;

typedef struct
{
	rsp_vertex_t vertices[3];
	rsp_texture_t *texture;
	boolean flipped;
	lighttable_t *colormap;
	lighttable_t *translation;
	UINT8 *transmap;
} rsp_triangle_t;

typedef struct
{
	INT32 width, height;
	float aspectratio, fov;
	float far_plane, near_plane;

	rsp_mode_t mode;
	rsp_trimode_t trianglemode;
	rsp_cullmode_t cullmode;
	boolean aiming;

	float *depthbuffer;
} rendertarget_t;

void RSP_TransformTriangle(rsp_triangle_t *tri);
void RSP_ClipTriangle(rsp_triangle_t *tri);
void RSP_DrawTriangle(rsp_triangle_t *tri);
void RSP_DrawTriangleList(rsp_triangle_t *tri, rsp_triangle_t *list, INT32 count);

// pixel drawer functions
extern void (*rsp_curpixelfunc)(void);
extern void (*rsp_basepixelfunc)(void);
extern void (*rsp_transpixelfunc)(void);

extern INT16 rsp_xpix;
extern INT16 rsp_ypix;
extern UINT8 rsp_cpix;
extern float rsp_zpix;
extern UINT8 *rsp_tpix;

extern INT32 rsp_viewwindowx, rsp_viewwindowy;
#define SOFTWARE_AIMING (centery - (viewheight/2))

void RSP_DrawPixel(void);
void RSP_DrawTranslucentPixel(void);

// triangle drawer functions
extern void (*rsp_curtrifunc)(rsp_triangle_t *tri, rsp_trimode_t type);
void RSP_TexturedMappedTriangleFP(rsp_triangle_t *tri, rsp_trimode_t type);

typedef struct
{
	fixed_t viewx, viewy, viewz;
	angle_t viewangle, aimingangle;
	fixed_t viewcos, viewsin;
	// 3D math
	fpvector4_t position_vector;
	fpvector4_t target_vector;
	fpmatrix16_t view_matrix;
	fpmatrix16_t projection_matrix;
} viewpoint_t;

extern rendertarget_t rsp_target;
extern viewpoint_t rsp_viewpoint;
extern fpmatrix16_t *rsp_projectionmatrix;

void RSP_Init(void);
void RSP_Viewport(INT32 width, INT32 height);
void RSP_OnFrame(void);
void RSP_ModelView(void);
void RSP_SetDrawerFunctions(void);
void RSP_DebugRender(INT32 model);
void RSP_ClearDepthBuffer(void);

// Debugging info
#ifdef RSP_DEBUGGING
extern INT32 rsp_meshesdrawn;
extern INT32 rsp_trisdrawn;

extern consvar_t cv_rspdebugdepth;
#endif

// MASKING STUFF
void RSP_StoreViewpoint(void);
void RSP_RestoreViewpoint(void);
void RSP_StoreSpriteViewpoint(vissprite_t *spr);
void RSP_RestoreSpriteViewpoint(vissprite_t *spr);

extern UINT32 rsp_maskdraw;
#define RSP_MASKDRAWBIT 0x80000000

// 3D models
boolean RSP_RenderModel(vissprite_t *spr);
void RSP_CreateModelTexture(modelinfo_t *model, INT32 tcnum, skincolornum_t skincolor);
void RSP_FreeModelTexture(modelinfo_t *model);
void RSP_FreeModelBlendTexture(modelinfo_t *model);

#endif
