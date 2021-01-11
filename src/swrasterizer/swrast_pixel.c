// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2021 by Jaime Ita Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  swrast_draw.c
/// \brief Pixel drawers

#include "swrast.h"

static UINT8 FragmentShadePixel(SWRast_Fragment *p, UINT8 colidx)
{
	UINT8 *colormap = SWRast_GetColormap();
	UINT8 *translation = SWRast_GetTranslation();

	if (translation)
		colidx = translation[colidx];
	if (colormap)
		colidx = colormap[colidx];

	if (SWRastState->computeLight)
	{
		const UINT8 lights = 32;
		const UINT8 lightdiv = 2;
		fixed_t l = SWRast_InterpolateBarycentric(SWRastState->vertexLights[0], SWRastState->vertexLights[1], SWRastState->vertexLights[2], p->barycentric);
		INT32 compute = min(max(0, FixedInt(FixedMul(l, lights<<FRACBITS) / lightdiv)), lights-1) * 256;
		colidx = colormaps[colidx + compute];
	}

	return colidx;
}

UINT8 SWRast_ProcessFragment(SWRast_Fragment *p)
{
	SWRast_Triangle *curTri = SWRastState->triangle;
	fixed_t width, height;
	INT16 iwidth, iheight;
	fixed_t u, v;
	UINT16 colidx;
	UINT8 status = 0;

	if (!(SWRast_GetFragmentWrite() & SWRAST_WRITE_COLOR))
		return 0;

	iwidth = SWRastState->texture->width;
	iheight = SWRastState->texture->height;
	width = iwidth << FRACBITS;
	height = iheight << FRACBITS;

	u = SWRast_InterpolateBarycentric(
									curTri->vertices[0].uv.u,
									curTri->vertices[1].uv.u,
									curTri->vertices[2].uv.u,
									p->barycentric);

	v = SWRast_InterpolateBarycentric(
									curTri->vertices[0].uv.v,
									curTri->vertices[1].uv.v,
									curTri->vertices[2].uv.v,
									p->barycentric);

	u = SWRast_clamp(FixedMul(u, width)>>FRACBITS, 0, iwidth);
	v = SWRast_clamp(FixedMul(v, height)>>FRACBITS, 0, iheight);

	colidx = SWRastState->texture->data[(v * iwidth) + u];

	if (colidx & 0xFF00)
	{
		colidx &= 0x00FF;
		colidx = FragmentShadePixel(p, colidx);

		SWRastState->pixelFunction(p->x, p->y, colidx);
		status |= SWRAST_FRAGMENT_DRAWN;

		if (!SWRast_BlendingEnabled())
			status |= SWRAST_FRAGMENT_SET_DEPTH;
	}

	return status;
}

void SWRast_DrawPixel(INT32 x, INT32 y, UINT8 color)
{
	UINT8 *dest = SWRAST_COLOR_BUFFER_POINTER + (y * vid.width) + x;
	*dest = color;
}

void SWRast_DrawBlendedPixel(INT32 x, INT32 y, UINT8 color)
{
	UINT8 *dest = SWRAST_COLOR_BUFFER_POINTER + (y * vid.width) + x;
	*dest = *(SWRastState->translucencyTable + ((UINT8)color<<8) + *dest);
}
