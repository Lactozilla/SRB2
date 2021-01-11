// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2021 by Jaime Ita Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  swrast_types.h
/// \brief Software rasterizer types

#include "../doomtype.h"
#include "../doomstat.h"
#include "../r_picformats.h"
#include "../r_defs.h"

#ifndef _SWRAST_TYPES_H_
#define _SWRAST_TYPES_H_

//
// Defines
//

#if SWRAST_Z_BUFFER == 1
	#define SWRAST_MAX_ZBUFFER_DEPTH INT32_MAX
	#define SWRast_ZBuffer fixed_t
	#define SWRast_ZBufferFormat(depth) (depth)
#elif SWRAST_Z_BUFFER == 2
	#define SWRAST_MAX_ZBUFFER_DEPTH UINT8_MAX
	#define SWRast_ZBuffer UINT8
	#define SWRast_ZBufferFormat(depth)\
		SWRast_min(255,(depth) >> SWRAST_REDUCED_Z_BUFFER_GRANULARITY)
#endif

//
// Enums
//

enum
{
	SWRAST_READ_COLOR    = 1,
	SWRAST_READ_DEPTH    = 1<<1,
	SWRAST_READ_STENCIL  = 1<<2,

	SWRAST_WRITE_COLOR   = 1,
	SWRAST_WRITE_DEPTH   = 1<<1,
	SWRAST_WRITE_STENCIL = 1<<2,
};

enum
{
	SWRAST_CULL_NONE,
	SWRAST_CULL_BACK,
	SWRAST_CULL_FRONT
};

enum
{
	SWRAST_VIEW_WINDOW_X1,
	SWRAST_VIEW_WINDOW_X2,
	SWRAST_VIEW_WINDOW_Y1,
	SWRAST_VIEW_WINDOW_Y2,
	SWRAST_VIEW_WINDOW_SIDES,
};

enum
{
	SWRAST_FRAGMENT_DISCARDED = 0,
	SWRAST_FRAGMENT_DRAWN     = 1,
	SWRAST_FRAGMENT_SET_DEPTH = 1<<1,
};

//
// 3D structures
//

/** Vector that consists of four scalars and can represent homogenous
	coordinates, but is generally also used as Vec3 and Vec2 for various
	purposes. */
typedef struct
{
	fixed_t x, y, z, w;
} SWRast_Vec4;

typedef struct
{
	angle_t x, y, z, w;
} SWRast_AngVec4;

typedef struct
{
	fixed_t u, v;
} SWRast_UVUnit;

typedef struct
{
	SWRast_Vec4 position;
	SWRast_Vec4 normal;
	SWRast_UVUnit uv;
} SWRast_Vertex;

typedef struct
{
	SWRast_Vertex vertices[3];
} SWRast_Triangle;

/** 4x4 matrix, used mostly for 3D transforms. The indexing is this:
		matrix[column][row]. */
typedef fixed_t SWRast_Mat4[4][4];

typedef struct
{
	SWRast_Vec4 translation;
	SWRast_AngVec4 rotation; /**< Euler angles. Rotation is applied in this order:
								1. z = by z (roll) CW looking along z+
								2. x = by x (pitch) CW looking along x+
								3. y = by y (yaw) CW looking along y+ */
	SWRast_Vec4 scale;
} SWRast_Transform3D;

typedef struct
{
	fixed_t focalLength;       ///< Defines the field of view (FOV).
	SWRast_Transform3D transform;
} SWRast_Camera;

typedef struct
{
	INT16 x;          ///< Screen X coordinate.
	INT16 y;          ///< Screen Y coordinate.

	fixed_t barycentric[3]; /**< Barycentric coords correspond to the three
								vertices. These serve to locate the pixel on a
								triangle and interpolate values between it's
								three points. Each one goes from 0 to
								FRACUNITS (including), but due to
								rounding error may fall outside this range (you
								can use SWRast_CorrectBarycentricCoords to fix this
								for the price of some performance). The sum of
								the three coordinates will always be exactly
								FRACUNITS. */
	fixed_t depth;         ///< Depth (only if depth is turned on).
	fixed_t previousZ;     /**< Z-buffer value (not necessarily world depth in
								FRACUNITs!) that was in the z-buffer on the
								pixels position before this pixel was
								rasterized. This can be used to set the value
								back, e.g. for transparency. */
	INT16 triangleSize[2]; /**< Rasterized triangle width and height, can be used e.g. for MIP mapping. */
} SWRast_Fragment;         /**< Used to pass the info about a rasterized pixel (fragment) to the user-defined drawing func. */

//
// Main rendering structures
//

/** Describes a framebuffer. */
typedef struct
{
	INT32 width, height;
	UINT8 fragmentRead, fragmentWrite;
	UINT8 *colorBuffer;
#if SWRAST_Z_BUFFER != 0
	SWRast_ZBuffer *zBuffer;
#endif
#if SWRAST_STENCIL_BUFFER
	UINT8 *stencilBuffer;
	size_t stencilBufferSize;
#endif
} SWRast_RenderTarget;

typedef struct
{
	fixed_t viewx, viewy, viewz;
	angle_t viewangle, aimingangle;
	fixed_t viewcos, viewsin;
} SWRast_Viewpoint;

typedef struct
{
	SWRast_RenderTarget renderTarget;
	SWRast_Viewpoint viewpoint;
	SWRast_Camera camera;

	fixed_t fov;
	UINT8 cullMode;

	SWRast_Mat4 projectionViewMatrix;
	SWRast_Mat4 modelMatrix;

	boolean computeLight;
	boolean modelLighting;
	SWRast_Vec4 modelLightPos;

	UINT32 maskDraw;

	UINT16 viewWindow[SWRAST_VIEW_WINDOW_SIDES];
	UINT16 screenVertexOffset[2];

	INT16 *spanBottomClip;
	INT16 *spanTopClip;
	INT32 spanPortalClip[2];

	void (*vertexShader)(SWRast_Vertex *vertex);
	UINT8 (*fragmentShader)(SWRast_Fragment *fragment);
	void (*pixelFunction)(INT32 x, INT32 y, UINT8 color);

	SWRast_Triangle *triangle;
	SWRast_Texture *texture;
	lighttable_t *colormap;
	lighttable_t *translation;
	UINT8 *translucencyTable;
	fixed_t vertexLights[3];

	INT32 numRenderedMeshes, numRenderedTriangles;
} SWRast_State;

extern SWRast_State *SWRastState;

#define SWRAST_COLOR_BUFFER_POINTER (SWRastState->renderTarget.colorBuffer)
#define SWRAST_DEPTH_BUFFER_POINTER (SWRastState->renderTarget.zBuffer)
#define SWRAST_STENCIL_BUFFER_POINTER (SWRastState->renderTarget.stencilBuffer)

//
// Floating point math
//

typedef struct
{
	float x, y, z, w;
} fpvector4_t;

typedef struct
{
	float x, y, z, w;
} fpquaternion_t;

#endif // _SWRAST_TYPES_H_
