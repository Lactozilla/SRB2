// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_draw8_rotated.c
/// \brief 8bpp column drawer functions for rotated sprites
/// \note  no includes because this is included as part of r_draw.c

// ==========================================================================
// COLUMNS
// ==========================================================================

void R_DrawRotatedColumn_8(void)
{
	INT32 count;
	UINT8 *dest;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
		return;
#endif

	count = dc_yh - dc_yl;
	if (count < 0)
		return;

	dest = &topleft[dc_yl*vid.width + dc_x];
	count++;

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	{
		patch_t *source = dc_patch;
		lighttable_t *colormap = dc_colormap;

		INT32 x, y;
		INT32 lastx = INT32_MAX, lasty = INT32_MAX;
		UINT8 *px = NULL;

		do
		{
			x = dc_rotation.x>>FRACBITS;
			y = dc_rotation.y>>FRACBITS;

			if (lastx != x || lasty != y)
			{
				px = Picture_GetPatchPixel(source, PICFMT_PATCH, x, y, dc_rotation.flip);
				lastx = x;
				lasty = y;
			}

			if (px != NULL)
				*dest = colormap[(*px)];
			dest += vid.width;

			dc_rotation.x += dc_rotation.xstep;
			dc_rotation.y += dc_rotation.ystep;
		} while (--count);
	}
}

void R_DrawRotatedTranslucentColumn_8(void)
{
	INT32 count;
	UINT8 *dest;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
		return;
#endif

	count = dc_yh - dc_yl;
	if (count < 0)
		return;

	dest = &topleft[dc_yl*vid.width + dc_x];
	count++;

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	{
		patch_t *source = dc_patch;
		lighttable_t *colormap = dc_colormap;
		UINT8 *transmap = dc_transmap;

		INT32 x, y;
		INT32 lastx = INT32_MAX, lasty = INT32_MAX;
		UINT8 *px = NULL;

		do
		{
			x = dc_rotation.x>>FRACBITS;
			y = dc_rotation.y>>FRACBITS;

			if (lastx != x || lasty != y)
			{
				px = Picture_GetPatchPixel(source, PICFMT_PATCH, x, y, dc_rotation.flip);
				lastx = x;
				lasty = y;
			}

			if (px != NULL)
				*dest = *(transmap + (colormap[(*px)]<<8) + (*dest));
			dest += vid.width;

			dc_rotation.x += dc_rotation.xstep;
			dc_rotation.y += dc_rotation.ystep;
		} while (--count);
	}
}

void R_DrawRotatedTranslatedTranslucentColumn_8(void)
{
	INT32 count;
	UINT8 *dest;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
		return;
#endif

	count = dc_yh - dc_yl;
	if (count < 0)
		return;

	dest = &topleft[dc_yl*vid.width + dc_x];
	count++;

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	{
		patch_t *source = dc_patch;
		lighttable_t *colormap = dc_colormap;
		lighttable_t *translation = dc_translation;
		UINT8 *transmap = dc_transmap;

		INT32 x, y;
		INT32 lastx = INT32_MAX, lasty = INT32_MAX;
		UINT8 *px = NULL;

		do
		{
			x = dc_rotation.x>>FRACBITS;
			y = dc_rotation.y>>FRACBITS;

			if (lastx != x || lasty != y)
			{
				px = Picture_GetPatchPixel(source, PICFMT_PATCH, x, y, dc_rotation.flip);
				lastx = x;
				lasty = y;
			}

			if (px != NULL)
				*dest = *(transmap + (colormap[translation[(*px)]]<<8) + (*dest));
			dest += vid.width;

			dc_rotation.x += dc_rotation.xstep;
			dc_rotation.y += dc_rotation.ystep;
		} while (--count);
	}
}

void R_DrawRotatedTranslatedColumn_8(void)
{
	INT32 count;
	UINT8 *dest;

#ifdef RANGECHECK
	if ((unsigned)dc_x >= (unsigned)vid.width || dc_yl < 0 || dc_yh >= vid.height)
		return;
#endif

	count = dc_yh - dc_yl;
	if (count < 0)
		return;

	dest = &topleft[dc_yl*vid.width + dc_x];
	count++;

	// Inner loop that does the actual texture mapping, e.g. a DDA-like scaling.
	// This is as fast as it gets.
	{
		patch_t *source = dc_patch;
		lighttable_t *colormap = dc_colormap;
		lighttable_t *translation = dc_translation;
		INT32 x, y;
		INT32 lastx = INT32_MAX, lasty = INT32_MAX;
		UINT8 *px = NULL;

		do
		{
			x = dc_rotation.x>>FRACBITS;
			y = dc_rotation.y>>FRACBITS;

			if (lastx != x || lasty != y)
			{
				px = Picture_GetPatchPixel(source, PICFMT_PATCH, x, y, dc_rotation.flip);
				lastx = x;
				lasty = y;
			}

			if (px != NULL)
				*dest = colormap[translation[(*px)]];
			dest += vid.width;

			dc_rotation.x += dc_rotation.xstep;
			dc_rotation.y += dc_rotation.ystep;
		} while (--count);
	}
}
