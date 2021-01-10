// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2021 by Jaime Ita Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  rsp_draw.c
/// \brief Pixel drawers

#include "r_softpoly.h"

void (*rsp_curpixelfunc)(void);
void (*rsp_basepixelfunc)(void);
void (*rsp_transpixelfunc)(void);

INT16 rsp_xpix = -1;
INT16 rsp_ypix = -1;
UINT8 rsp_cpix = 0;

void RSP_ProcessFragment(void *pixel)
{
	S3L_PixelInfo *p = pixel;
	rsp_triangle_t *curTri = rsp_state.triangle;
	fixed_t width, height;
	S3L_Unit u, v;
	UINT16 colidx;

	if (!(rsp_target.write & RSP_WRITE_COLOR))
		return;

	width = rsp_state.texture->width << FRACBITS;
	height = rsp_state.texture->height << FRACBITS;

	u = S3L_interpolateBarycentric(
									curTri->vertices[0].uv.u,
									curTri->vertices[1].uv.u,
									curTri->vertices[2].uv.u,
									p->barycentric);

	v = S3L_interpolateBarycentric(
									curTri->vertices[0].uv.v,
									curTri->vertices[1].uv.v,
									curTri->vertices[2].uv.v,
									p->barycentric);

	u = FixedMul(u<<RSP_INTERNALUNITSHIFT, width);
	v = FixedMul(v<<RSP_INTERNALUNITSHIFT, height);
	u = S3L_clamp(u>>FRACBITS, 0, rsp_state.texture->width);
	v = S3L_clamp(v>>FRACBITS, 0, rsp_state.texture->height);

	colidx = rsp_state.texture->data[(v * rsp_state.texture->width) + u];

	if (colidx & 0xFF00)
	{
		colidx &= 0x00FF;

		if (rsp_state.translation)
			colidx = rsp_state.translation[colidx];
		if (rsp_state.colormap)
			colidx = rsp_state.colormap[colidx];

		rsp_xpix = p->x;
		rsp_ypix = p->y;
		rsp_cpix = colidx;

		// Compute light
		if (rsp_target.write & RSP_WRITE_LIGHT)
		{
			const UINT8 lights = 32;
			const UINT8 lightdiv = 2;
			fixed_t l = S3L_interpolateBarycentric(rsp_state.light[0],rsp_state.light[1],rsp_state.light[2],p->barycentric)<<RSP_INTERNALUNITSHIFT;
			INT32 compute = min(max(0, FixedInt(FixedMul(l, lights<<FRACBITS) / lightdiv)), lights-1) * 256;
			rsp_cpix = colormaps[rsp_cpix + compute];
		}

		rsp_curpixelfunc();
	}
}

void RSP_DrawPixel(void)
{
	UINT8 *dest = screens[0] + (rsp_ypix * vid.width) + rsp_xpix;
	*dest = rsp_cpix;
}

void RSP_DrawTranslucentPixel(void)
{
	UINT8 *dest = screens[0] + (rsp_ypix * vid.width) + rsp_xpix;
	*dest = *(rsp_state.transmap + ((UINT8)rsp_cpix<<8) + *dest);
}
