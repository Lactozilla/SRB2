// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2021 by Jaime Ita Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  rsp_types.h
/// \brief Polygon renderer types

#include "../doomtype.h"
#include "../doomstat.h"
#include "../r_picformats.h"
#include "../r_defs.h"

#ifndef _RSP_TYPES_H_
#define _RSP_TYPES_H_

//
// Enums
//

enum
{
	RSP_READ_COLOR  = 1,
	RSP_READ_DEPTH  = 1<<1,
	RSP_READ_LIGHT  = 1<<2,

	RSP_WRITE_COLOR = 1,
	RSP_WRITE_DEPTH = 1<<1,
	RSP_WRITE_LIGHT = 1<<2,
};

enum
{
	RSP_CULL_NONE,
	RSP_CULL_BACK,
	RSP_CULL_FRONT
};

//
// 3D structures
//

typedef struct
{
	INT32 x, y, z;
} rsp_posunit_t;

typedef struct
{
	INT32 u, v;
} rsp_uvunit_t;

typedef struct
{
	rsp_posunit_t position;
	rsp_posunit_t normal;
	rsp_uvunit_t uv;
} rsp_vertex_t;

typedef struct
{
	rsp_vertex_t vertices[3];
} rsp_triangle_t;

//
// Main rendering structures
//

typedef struct
{
	rsp_triangle_t *triangle;
	rsp_texture_t *texture;
	boolean flipped;
	lighttable_t *colormap;
	lighttable_t *translation;
	UINT8 *transmap;
	UINT16 light[3];
} rsp_state_t;

extern rsp_state_t rsp_state;

typedef struct
{
	INT32 width, height;

	fixed_t fov;
	float aspect;

	UINT8 read, write;
	UINT8 cull;
} rsp_target_t;

extern rsp_target_t rsp_target;

typedef struct
{
	fixed_t viewx, viewy, viewz;
	angle_t viewangle, aimingangle;
	fixed_t viewcos, viewsin;
} rsp_viewpoint_t;

extern rsp_viewpoint_t rsp_viewpoint;

//
// Floating point math
//

typedef struct
{
	float x, y, z, w;
} fpvector4_t;

typedef struct
{
	float m[16];
} fpmatrix16_t;

typedef struct
{
	float x, y, z, w;
} fpquaternion_t;

#endif // _RSP_TYPES_H_
