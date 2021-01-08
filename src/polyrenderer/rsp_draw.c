// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2021 by Jaime Ita Passos.
// Copyright (C) 2017 by Krzysztof Kondrak.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  rsp_spans.c
/// \brief Pixel drawers

#include "r_softpoly.h"

void (*rsp_curpixelfunc)(void);
void (*rsp_basepixelfunc)(void);
void (*rsp_transpixelfunc)(void);

INT16 rsp_xpix = -1;
INT16 rsp_ypix = -1;
UINT8 rsp_cpix = 0;
float rsp_zpix;
UINT8 *rsp_tpix = NULL;

INT32 rsp_viewwindowx = 0, rsp_viewwindowy = 0;

void RSP_DrawPixel(void)
{
	INT16 xpix = 0, ypix = 0;
	UINT8 *dest;
	float *depth = NULL;
	boolean depth_only = ((rsp_target.mode & (RENDERMODE_DEPTH|RENDERMODE_COLOR)) == RENDERMODE_DEPTH);
	UINT8 pixel = rsp_cpix;

	if (rsp_xpix >= rsp_target.width || rsp_xpix < 0 || rsp_ypix >= rsp_target.height || rsp_ypix < 0)
		return;

	if (rsp_target.mode & RENDERMODE_DEPTH)
	{
		depth = rsp_target.depthbuffer + (rsp_xpix + rsp_ypix * rsp_target.width);
		if (*depth >= rsp_zpix)
			return;
	}

	xpix = (rsp_xpix + rsp_viewwindowx);
	ypix = (rsp_ypix + rsp_viewwindowy);
	if (ypix >= vid.width || ypix < 0 || ypix >= vid.height || ypix < 0)
		return;

	dest = screens[0] + (ypix * vid.width) + xpix;
	if (rsp_target.mode & RENDERMODE_DEPTH)
		*depth = rsp_zpix;
	if (!depth_only)
		*dest = pixel;
}

void RSP_DrawTranslucentPixel(void)
{
	INT16 xpix = 0, ypix = 0;
	UINT8 *dest;
	float *depth = NULL;
	boolean depth_only = ((rsp_target.mode & (RENDERMODE_DEPTH|RENDERMODE_COLOR)) == RENDERMODE_DEPTH);
	UINT8 pixel = rsp_cpix;

	if (!rsp_tpix)
		I_Error("RSP_DrawTranslucentPixel: NULL transmap");

	if (rsp_xpix >= rsp_target.width || rsp_xpix < 0 || rsp_ypix >= rsp_target.height || rsp_ypix < 0)
		return;

	if (rsp_target.mode & RENDERMODE_DEPTH)
	{
		depth = rsp_target.depthbuffer + (rsp_xpix + rsp_ypix * rsp_target.width);
		if (*depth >= rsp_zpix)
			return;
	}

	xpix = (rsp_xpix + rsp_viewwindowx);
	ypix = (rsp_ypix + rsp_viewwindowy);
	if (ypix >= vid.width || ypix < 0 || ypix >= vid.height || ypix < 0)
		return;

	if (!depth_only)
	{
		dest = screens[0] + (ypix * vid.width) + xpix;
		*dest = *(rsp_tpix + ((UINT8)pixel<<8) + *dest);
	}
}
