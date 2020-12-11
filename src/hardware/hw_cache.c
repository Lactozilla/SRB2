// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file hw_cache.c
/// \brief load and convert graphics to the hardware format

#include "../doomdef.h"

#ifdef HWRENDER
#include "hw_glob.h"
#include "hw_gpu.h"
#include "hw_batching.h"

#include "../doomstat.h"    //gamemode
#include "../i_video.h"     //rendermode
#include "../r_data.h"
#include "../r_textures.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../v_video.h"
#include "../r_draw.h"
#include "../r_patch.h"
#include "../r_picformats.h"
#include "../p_setup.h"

INT32 gl_patchformat = GPU_TEXFMT_AP_88; // use alpha for holes
INT32 gl_textureformat = GPU_TEXFMT_P_8; // use chromakey for hole

static INT32 format2bpp(GLTextureFormat_t format)
{
	if (format == GPU_TEXFMT_RGBA)
		return 4;
	else if (format == GPU_TEXFMT_ALPHA_INTENSITY_88 || format == GPU_TEXFMT_AP_88)
		return 2;
	else
		return 1;
}

// This code was originally placed directly in HWR_DrawPatchInCache.
// It is now split from it for my sanity! (and the sanity of others)
// -- Monster Iestyn (13/02/19)
static void HWR_DrawColumnInCache(const column_t *patchcol, UINT8 *block, HWRTexture_t *texture,
								INT32 pblockheight, INT32 blockmodulo,
								fixed_t yfracstep, fixed_t scale_y,
								texpatch_t *originPatch, INT32 patchheight,
								INT32 bpp)
{
	fixed_t yfrac, position, count;
	UINT8 *dest;
	const UINT8 *source;
	INT32 topdelta, prevdelta = -1;
	INT32 originy = 0;

	// for writing a pixel to dest
	RGBA_t colortemp;
	UINT8 alpha;
	UINT8 texel;
	UINT16 texelu16;

	(void)patchheight; // This parameter is unused

	if (originPatch) // originPatch can be NULL here, unlike in the software version
		originy = originPatch->originy;

	while (patchcol->topdelta != 0xff)
	{
		topdelta = patchcol->topdelta;
		if (topdelta <= prevdelta)
			topdelta += prevdelta;
		prevdelta = topdelta;
		source = (const UINT8 *)patchcol + 3;
		count  = ((patchcol->length * scale_y) + (FRACUNIT/2)) >> FRACBITS;
		position = originy + topdelta;

		yfrac = 0;
		//yfracstep = (patchcol->length << FRACBITS) / count;
		if (position < 0)
		{
			yfrac = -position<<FRACBITS;
			count += (((position * scale_y) + (FRACUNIT/2)) >> FRACBITS);
			position = 0;
		}

		position = ((position * scale_y) + (FRACUNIT/2)) >> FRACBITS;

		if (position < 0)
			position = 0;

		if (position + count >= pblockheight)
			count = pblockheight - position;

		dest = block + (position*blockmodulo);
		while (count > 0)
		{
			count--;

			texel = source[yfrac>>FRACBITS];
			alpha = 0xFF;
			// Make pixel transparent if chroma keyed
			if ((texture->flags & TF_CHROMAKEYED) && (texel == GPU_PATCHES_CHROMAKEY_COLORINDEX))
				alpha = 0x00;

			//Hurdler: 25/04/2000: now support colormap in hardware mode
			if (texture->colormap)
				texel = texture->colormap[texel];

			// hope compiler will get this switch out of the loops (dreams...)
			// gcc do it ! but vcc not ! (why don't use cygwin gcc for win32 ?)
			// Alam: SRB2 uses Mingw, HUGS
			switch (bpp)
			{
				case 2 : // uhhhhhhhh..........
						 if ((originPatch != NULL) && (originPatch->style != AST_COPY))
							 texel = ASTBlendPaletteIndexes(*(dest+1), texel, originPatch->style, originPatch->alpha);
						 texelu16 = (UINT16)((alpha<<8) | texel);
						 memcpy(dest, &texelu16, sizeof(UINT16));
						 break;
				case 3 : colortemp = V_GetColor(texel);
						 if ((originPatch != NULL) && (originPatch->style != AST_COPY))
						 {
							 RGBA_t rgbatexel;
							 rgbatexel.rgba = *(UINT32 *)dest;
							 colortemp.rgba = ASTBlendTexturePixel(rgbatexel, colortemp, originPatch->style, originPatch->alpha);
						 }
						 memcpy(dest, &colortemp, sizeof(RGBA_t)-sizeof(UINT8));
						 break;
				case 4 : colortemp = V_GetColor(texel);
						 colortemp.s.alpha = alpha;
						 if ((originPatch != NULL) && (originPatch->style != AST_COPY))
						 {
							 RGBA_t rgbatexel;
							 rgbatexel.rgba = *(UINT32 *)dest;
							 colortemp.rgba = ASTBlendTexturePixel(rgbatexel, colortemp, originPatch->style, originPatch->alpha);
						 }
						 memcpy(dest, &colortemp, sizeof(RGBA_t));
						 break;
				// default is 1
				default:
						 if ((originPatch != NULL) && (originPatch->style != AST_COPY))
							 *dest = ASTBlendPaletteIndexes(*dest, texel, originPatch->style, originPatch->alpha);
						 else
							 *dest = texel;
						 break;
			}

			dest += blockmodulo;
			yfrac += yfracstep;
		}
		patchcol = (const column_t *)((const UINT8 *)patchcol + patchcol->length + 4);
	}
}

static void HWR_DrawFlippedColumnInCache(const column_t *patchcol, UINT8 *block, HWRTexture_t *texture,
								INT32 pblockheight, INT32 blockmodulo,
								fixed_t yfracstep, fixed_t scale_y,
								texpatch_t *originPatch, INT32 patchheight,
								INT32 bpp)
{
	fixed_t yfrac, position, count;
	UINT8 *dest;
	const UINT8 *source;
	INT32 topdelta, prevdelta = -1;
	INT32 originy = 0;

	// for writing a pixel to dest
	RGBA_t colortemp;
	UINT8 alpha;
	UINT8 texel;
	UINT16 texelu16;

	if (originPatch) // originPatch can be NULL here, unlike in the software version
		originy = originPatch->originy;

	while (patchcol->topdelta != 0xff)
	{
		topdelta = patchcol->topdelta;
		if (topdelta <= prevdelta)
			topdelta += prevdelta;
		prevdelta = topdelta;
		topdelta = patchheight-patchcol->length-topdelta;
		source = (const UINT8 *)patchcol + 3;
		count  = ((patchcol->length * scale_y) + (FRACUNIT/2)) >> FRACBITS;
		position = originy + topdelta;

		yfrac = (patchcol->length-1) << FRACBITS;

		if (position < 0)
		{
			yfrac += position<<FRACBITS;
			count += (((position * scale_y) + (FRACUNIT/2)) >> FRACBITS);
			position = 0;
		}

		position = ((position * scale_y) + (FRACUNIT/2)) >> FRACBITS;

		if (position < 0)
			position = 0;

		if (position + count >= pblockheight)
			count = pblockheight - position;

		dest = block + (position*blockmodulo);
		while (count > 0)
		{
			count--;

			texel = source[yfrac>>FRACBITS];
			alpha = 0xFF;
			// Make pixel transparent if chroma keyed
			if ((texture->flags & TF_CHROMAKEYED) && (texel == GPU_PATCHES_CHROMAKEY_COLORINDEX))
				alpha = 0x00;

			//Hurdler: 25/04/2000: now support colormap in hardware mode
			if (texture->colormap)
				texel = texture->colormap[texel];

			// hope compiler will get this switch out of the loops (dreams...)
			// gcc do it ! but vcc not ! (why don't use cygwin gcc for win32 ?)
			// Alam: SRB2 uses Mingw, HUGS
			switch (bpp)
			{
				case 2 : // uhhhhhhhh..........
						 if ((originPatch != NULL) && (originPatch->style != AST_COPY))
							 texel = ASTBlendPaletteIndexes(*(dest+1), texel, originPatch->style, originPatch->alpha);
						 texelu16 = (UINT16)((alpha<<8) | texel);
						 memcpy(dest, &texelu16, sizeof(UINT16));
						 break;
				case 3 : colortemp = V_GetColor(texel);
						 if ((originPatch != NULL) && (originPatch->style != AST_COPY))
						 {
							 RGBA_t rgbatexel;
							 rgbatexel.rgba = *(UINT32 *)dest;
							 colortemp.rgba = ASTBlendTexturePixel(rgbatexel, colortemp, originPatch->style, originPatch->alpha);
						 }
						 memcpy(dest, &colortemp, sizeof(RGBA_t)-sizeof(UINT8));
						 break;
				case 4 : colortemp = V_GetColor(texel);
						 colortemp.s.alpha = alpha;
						 if ((originPatch != NULL) && (originPatch->style != AST_COPY))
						 {
							 RGBA_t rgbatexel;
							 rgbatexel.rgba = *(UINT32 *)dest;
							 colortemp.rgba = ASTBlendTexturePixel(rgbatexel, colortemp, originPatch->style, originPatch->alpha);
						 }
						 memcpy(dest, &colortemp, sizeof(RGBA_t));
						 break;
				// default is 1
				default:
						 if ((originPatch != NULL) && (originPatch->style != AST_COPY))
							 *dest = ASTBlendPaletteIndexes(*dest, texel, originPatch->style, originPatch->alpha);
						 else
							 *dest = texel;
						 break;
			}

			dest += blockmodulo;
			yfrac -= yfracstep;
		}
		patchcol = (const column_t *)((const UINT8 *)patchcol + patchcol->length + 4);
	}
}


// Simplified patch caching function
// for use by sprites and other patches that are not part of a wall texture
// no alpha or flipping should be present since we do not want non-texture graphics to have them
// no offsets are used either
// -- Monster Iestyn (13/02/19)
static void HWR_DrawPatchInCache(HWRTexture_t *texture,
	INT32 pblockwidth, INT32 pblockheight,
	INT32 pwidth, INT32 pheight,
	const patch_t *realpatch)
{
	INT32 ncols;
	fixed_t xfrac, xfracstep;
	fixed_t yfracstep, scale_y;
	const column_t *patchcol;
	UINT8 *block = texture->data;
	INT32 bpp;
	INT32 blockmodulo;

	if (pwidth <= 0 || pheight <= 0)
		return;

	ncols = pwidth;

	// source advance
	xfrac = 0;
	xfracstep = FRACUNIT;
	yfracstep = FRACUNIT;
	scale_y   = FRACUNIT;

	bpp = format2bpp(texture->format);

	if (bpp < 1 || bpp > 4)
		I_Error("HWR_DrawPatchInCache: no drawer defined for this bpp (%d)\n",bpp);

	// NOTE: should this actually be pblockwidth*bpp?
	blockmodulo = pblockwidth*bpp;

	// Draw each column to the block cache
	for (; ncols--; block += bpp, xfrac += xfracstep)
	{
		patchcol = (const column_t *)((const UINT8 *)realpatch->columns + (realpatch->columnofs[xfrac>>FRACBITS]));

		HWR_DrawColumnInCache(patchcol, block, texture,
								pblockheight, blockmodulo,
								yfracstep, scale_y,
								NULL, pheight, // not that pheight is going to get used anyway...
								bpp);
	}
}

// This function we use for caching patches that belong to textures
static void HWR_DrawTexturePatchInCache(HWRTexture_t *hwrTexture,
	INT32 pblockwidth, INT32 pblockheight,
	texture_t *texture, texpatch_t *patch,
	const softwarepatch_t *realpatch)
{
	INT32 x, x1, x2;
	INT32 col, ncols;
	fixed_t xfrac, xfracstep;
	fixed_t yfracstep, scale_y;
	const column_t *patchcol;
	UINT8 *block = hwrTexture->data;
	INT32 bpp;
	INT32 blockmodulo;
	INT32 width, height;
	// Column drawing function pointer.
	static void (*ColumnDrawerPointer)(const column_t *patchcol, UINT8 *block, HWRTexture_t *texture,
								INT32 pblockheight, INT32 blockmodulo,
								fixed_t yfracstep, fixed_t scale_y,
								texpatch_t *originPatch, INT32 patchheight,
								INT32 bpp);

	if (texture->width <= 0 || texture->height <= 0)
		return;

	ColumnDrawerPointer = (patch->flip & 2) ? HWR_DrawFlippedColumnInCache : HWR_DrawColumnInCache;

	x1 = patch->originx;
	width = SHORT(realpatch->width);
	height = SHORT(realpatch->height);
	x2 = x1 + width;

	if (x1 > texture->width || x2 < 0)
		return; // patch not located within texture's x bounds, ignore

	if (patch->originy > texture->height || (patch->originy + height) < 0)
		return; // patch not located within texture's y bounds, ignore

	// patch is actually inside the texture!
	// now check if texture is partly off-screen and adjust accordingly

	// left edge
	if (x1 < 0)
		x = 0;
	else
		x = x1;

	// right edge
	if (x2 > texture->width)
		x2 = texture->width;


	col = x * pblockwidth / texture->width;
	ncols = ((x2 - x) * pblockwidth) / texture->width;

/*
	CONS_Debug(DBG_RENDER, "patch %dx%d texture %dx%d block %dx%d\n",
															width, height,
															texture->width,          texture->height,
															pblockwidth,             pblockheight);
	CONS_Debug(DBG_RENDER, "      col %d ncols %d x %d\n", col, ncols, x);
*/

	// source advance
	xfrac = 0;
	if (x1 < 0)
		xfrac = -x1<<FRACBITS;

	xfracstep = (texture->width << FRACBITS) / pblockwidth;
	yfracstep = (texture->height<< FRACBITS) / pblockheight;
	scale_y   = (pblockheight  << FRACBITS) / texture->height;

	bpp = format2bpp(hwrTexture->format);

	if (bpp < 1 || bpp > 4)
		I_Error("HWR_DrawTexturePatchInCache: no drawer defined for this bpp (%d)\n",bpp);

	// NOTE: should this actually be pblockwidth*bpp?
	blockmodulo = pblockwidth*bpp;

	// Draw each column to the block cache
	for (block += col*bpp; ncols--; block += bpp, xfrac += xfracstep)
	{
		if (patch->flip & 1)
			patchcol = (const column_t *)((const UINT8 *)realpatch + LONG(realpatch->columnofs[(width-1)-(xfrac>>FRACBITS)]));
		else
			patchcol = (const column_t *)((const UINT8 *)realpatch + LONG(realpatch->columnofs[xfrac>>FRACBITS]));

		ColumnDrawerPointer(patchcol, block, hwrTexture,
								pblockheight, blockmodulo,
								yfracstep, scale_y,
								patch, height,
								bpp);
	}
}

static UINT8 *MakeBlock(HWRTexture_t *hwrTexture)
{
	UINT8 *block;
	INT32 bpp, i;
	UINT16 bu16 = ((0x00 <<8) | GPU_PATCHES_CHROMAKEY_COLORINDEX);
	INT32 blocksize = (hwrTexture->width * hwrTexture->height);

	bpp =  format2bpp(hwrTexture->format);
	block = Z_Malloc(blocksize*bpp, PU_HWRCACHE, &(hwrTexture->data));

	switch (bpp)
	{
		case 1: memset(block, GPU_PATCHES_CHROMAKEY_COLORINDEX, blocksize); break;
		case 2:
				// fill background with chromakey, alpha = 0
				for (i = 0; i < blocksize; i++)
				//[segabor]
					memcpy(block+i*sizeof(UINT16), &bu16, sizeof(UINT16));
				break;
		case 4: memset(block, 0x00, blocksize*sizeof(UINT32)); break;
	}

	return block;
}

//
// Create a composite texture from patches, adapt the texture size to a power of 2
// height and width for the hardware texture cache.
//
static void HWR_GenerateTexture(INT32 texnum, GLMapTexture_t *grtex)
{
	UINT8 *block;
	texture_t *texture;
	texpatch_t *patch;
	softwarepatch_t *realpatch;
	UINT8 *pdata;
	INT32 blockwidth, blockheight, blocksize;

	INT32 i;
	boolean skyspecial = false; //poor hack for Legacy large skies..

	texture = textures[texnum];

	// hack the Legacy skies..
	if (texture->name[0] == 'S' &&
	    texture->name[1] == 'K' &&
	    texture->name[2] == 'Y' &&
	    (texture->name[4] == 0 ||
	     texture->name[5] == 0)
	   )
	{
		skyspecial = true;
		grtex->texture.flags = TF_WRAPXY; // don't use the chromakey for sky
	}
	else
		grtex->texture.flags = TF_CHROMAKEYED | TF_WRAPXY;

	grtex->texture.width = (UINT16)texture->width;
	grtex->texture.height = (UINT16)texture->height;
	grtex->texture.format = gl_textureformat;

	blockwidth = texture->width;
	blockheight = texture->height;
	blocksize = (blockwidth * blockheight);
	block = MakeBlock(&grtex->texture);

	if (skyspecial) //Hurdler: not efficient, but better than holes in the sky (and it's done only at level loading)
	{
		INT32 j;
		RGBA_t col;

		col = V_GetColor(GPU_PATCHES_CHROMAKEY_COLORINDEX);
		for (j = 0; j < blockheight; j++)
		{
			for (i = 0; i < blockwidth; i++)
			{
				block[4*(j*blockwidth+i)+0] = col.s.red;
				block[4*(j*blockwidth+i)+1] = col.s.green;
				block[4*(j*blockwidth+i)+2] = col.s.blue;
				block[4*(j*blockwidth+i)+3] = 0xff;
			}
		}
	}

	// Composite the columns together.
	for (i = 0, patch = texture->patches; i < texture->patchcount; i++, patch++)
	{
		boolean dealloc = true;
		size_t lumplength = W_LumpLengthPwad(patch->wad, patch->lump);
		pdata = W_CacheLumpNumPwad(patch->wad, patch->lump, PU_CACHE);
		realpatch = (softwarepatch_t *)pdata;

#ifndef NO_PNG_LUMPS
		if (Picture_IsLumpPNG((UINT8 *)realpatch, lumplength))
			realpatch = (softwarepatch_t *)Picture_PNGConvert(pdata, PICFMT_DOOMPATCH, NULL, NULL, NULL, NULL, lumplength, NULL, 0);
		else
#endif
#ifdef WALLFLATS
		if (texture->type == TEXTURETYPE_FLAT)
			realpatch = (softwarepatch_t *)Picture_Convert(PICFMT_FLAT, pdata, PICFMT_DOOMPATCH, 0, NULL, texture->width, texture->height, 0, 0, 0);
		else
#endif
		{
			(void)lumplength;
			dealloc = false;
		}

		HWR_DrawTexturePatchInCache(&grtex->texture, blockwidth, blockheight, texture, patch, realpatch);

		if (dealloc)
			Z_Unlock(realpatch);
	}
	//Hurdler: not efficient at all but I don't remember exactly how HWR_DrawPatchInCache works :(
	if (format2bpp(grtex->texture.format)==4)
	{
		for (i = 3; i < blocksize*4; i += 4) // blocksize*4 because blocksize doesn't include the bpp
		{
			if (block[i] == 0)
			{
				grtex->texture.flags |= TF_TRANSPARENT;
				break;
			}
		}
	}

	grtex->scaleX = 1.0f/(texture->width*FRACUNIT);
	grtex->scaleY = 1.0f/(texture->height*FRACUNIT);
}

// patch may be NULL if hwrTexture has been initialised already and makebitmap is false
void HWR_MakePatch (const patch_t *patch, GLPatch_t *grPatch, HWRTexture_t *hwrTexture, boolean makebitmap)
{
	if (hwrTexture->width == 0)
	{
		hwrTexture->width = hwrTexture->height = 1;
		while (hwrTexture->width < patch->width) hwrTexture->width <<= 1;
		while (hwrTexture->height < patch->height) hwrTexture->height <<= 1;

		// no wrap around, no chroma key
		hwrTexture->flags = 0;

		// setup the texture info
		hwrTexture->format = gl_patchformat;

		grPatch->max_s = (float)patch->width / (float)hwrTexture->width;
		grPatch->max_t = (float)patch->height / (float)hwrTexture->height;
	}

	Z_Free(hwrTexture->data);
	hwrTexture->data = NULL;

	if (makebitmap)
	{
		MakeBlock(hwrTexture);

		HWR_DrawPatchInCache(hwrTexture,
			hwrTexture->width, hwrTexture->height,
			patch->width, patch->height,
			patch);
	}
}


// =================================================
//             CACHING HANDLING
// =================================================

static size_t gl_numtextures = 0; // Texture count
static GLMapTexture_t *gl_textures; // For all textures
static GLMapTexture_t *gl_flats; // For all (texture) flats, as normal flats don't need to be cached
boolean gl_maptexturesloaded = false;

void HWR_FreeTexture(patch_t *patch)
{
	if (!patch)
		return;

	if (patch->hardware)
	{
		GLPatch_t *grPatch = patch->hardware;

		HWR_FreeTextureColormaps(patch);

		if (grPatch->texture)
		{
			if (vid.glstate == VID_GL_LIBRARY_LOADED)
				GPU->DeleteTexture(grPatch->texture);
			if (grPatch->texture->data)
				Z_Free(grPatch->texture->data);
			Z_Free(grPatch->texture);
		}

		Z_Free(patch->hardware);
	}

	patch->hardware = NULL;
}

// Called by FreeTextureCache.
void HWR_FreeTextureColormaps(patch_t *patch)
{
	GLPatch_t *pat;

	// The patch must be valid, obviously
	if (!patch)
		return;

	pat = patch->hardware;
	if (!pat)
		return;

	// The texture must be valid, obviously
	while (pat->texture)
	{
		// Confusing at first, but pat->texture->nextcolormap
		// at the beginning of the loop is the first colormap
		// from the linked list of colormaps.
		HWRTexture_t *next = NULL;

		// No texture in this patch, break out of the loop.
		if (!pat->texture)
			break;

		// No colormap textures either.
		if (!pat->texture->nextcolormap)
			break;

		// Set the first colormap to the one that comes after it.
		next = pat->texture->nextcolormap;
		pat->texture->nextcolormap = next->nextcolormap;

		// Free image data from memory.
		if (next->data)
			Z_Free(next->data);
		next->data = NULL;

		if (vid.glstate == VID_GL_LIBRARY_LOADED)
			GPU->DeleteTexture(next);

		// Free the old colormap texture from memory.
		free(next);
	}
}

static boolean FreeTextureCallback(void *mem)
{
	patch_t *patch = (patch_t *)mem;
	HWR_FreeTexture(patch);
	return false;
}

static boolean FreeColormapsCallback(void *mem)
{
	patch_t *patch = (patch_t *)mem;
	HWR_FreeTextureColormaps(patch);
	return false;
}

static void FreeTextureCache(boolean freeall)
{
	boolean (*callback)(void *mem) = FreeTextureCallback;

	if (!freeall)
		callback = FreeColormapsCallback;

	Z_IterateTags(PU_PATCH, PU_PATCH_ROTATED, callback);
	Z_IterateTags(PU_SPRITE, PU_HUDGFX, callback);
}

void HWR_ClearAllTextures(void)
{
	GPU->ClearTextureCache(); // free references to the textures
	FreeTextureCache(true);
}

// free all texture colormaps after each level
void HWR_ClearColormapCache(void)
{
	FreeTextureCache(false);
}

void HWR_InitMapTextures(void)
{
	gl_textures = NULL;
	gl_flats = NULL;
	gl_maptexturesloaded = false;
}

static void FreeMapTexture(GLMapTexture_t *tex)
{
	GPU->DeleteTexture(&tex->texture);
	if (tex->texture.data)
		Z_Free(tex->texture.data);
	tex->texture.data = NULL;
}

void HWR_FreeMapTextures(void)
{
	size_t i;

	for (i = 0; i < gl_numtextures; i++)
	{
		FreeMapTexture(&gl_textures[i]);
		FreeMapTexture(&gl_flats[i]);
	}

	// now the heap don't have any 'user' pointing to our
	// texturecache info, we can free it
	if (gl_textures)
		free(gl_textures);
	if (gl_flats)
		free(gl_flats);
	gl_textures = NULL;
	gl_flats = NULL;
	gl_numtextures = 0;
	gl_maptexturesloaded = false;
}

void HWR_LoadMapTextures(size_t pnumtextures)
{
	// we must free it since numtextures may have changed
	HWR_FreeMapTextures();

	gl_numtextures = pnumtextures;
	gl_textures = calloc(gl_numtextures, sizeof(*gl_textures));
	gl_flats = calloc(gl_numtextures, sizeof(*gl_flats));

	if ((gl_textures == NULL) || (gl_flats == NULL))
		I_Error("HWR_LoadMapTextures: ran out of memory for OpenGL textures");

	gl_maptexturesloaded = true;
}

void HWR_SetPalette(RGBA_t *palette)
{
	GPU->SetPalette(palette);

	// hardware driver will flush there own cache if cache is non paletized
	// now flush data texture cache so 32 bit texture are recomputed
	if (gl_patchformat == GPU_TEXFMT_RGBA || gl_textureformat == GPU_TEXFMT_RGBA)
	{
		Z_FreeTag(PU_HWRCACHE);
		Z_FreeTag(PU_HWRCACHE_UNLOCKED);
	}
}

// --------------------------------------------------------------------------
// Make sure texture is downloaded and set it as the source
// --------------------------------------------------------------------------
GLMapTexture_t *HWR_GetTexture(INT32 tex)
{
	GLMapTexture_t *grtex;
#ifdef PARANOIA
	if ((unsigned)tex >= gl_numtextures)
		I_Error("HWR_GetTexture: tex >= numtextures\n");
#endif

	// Every texture in memory, stored in the
	// hardware renderer's bit depth format. Wow!
	grtex = &gl_textures[tex];

	// Generate texture if missing from the cache
	if (!grtex->texture.data && !grtex->texture.downloaded)
		HWR_GenerateTexture(tex, grtex);

	// If hardware does not have the texture, then call pfnSetTexture to upload it
	if (!grtex->texture.downloaded)
		GPU->SetTexture(&grtex->texture);
	HWR_SetCurrentTexture(&grtex->texture);

	// The system-memory data can be purged now.
	Z_ChangeTag(grtex->texture.data, PU_HWRCACHE_UNLOCKED);

	return grtex;
}

static void HWR_CacheFlat(HWRTexture_t *hwrTexture, lumpnum_t flatlumpnum)
{
	size_t size, pflatsize;

	// setup the texture info
	hwrTexture->format = GPU_TEXFMT_P_8;
	hwrTexture->flags = TF_WRAPXY|TF_CHROMAKEYED;

	size = W_LumpLength(flatlumpnum);

	switch (size)
	{
		case 4194304: // 2048x2048 lump
			pflatsize = 2048;
			break;
		case 1048576: // 1024x1024 lump
			pflatsize = 1024;
			break;
		case 262144:// 512x512 lump
			pflatsize = 512;
			break;
		case 65536: // 256x256 lump
			pflatsize = 256;
			break;
		case 16384: // 128x128 lump
			pflatsize = 128;
			break;
		case 1024: // 32x32 lump
			pflatsize = 32;
			break;
		default: // 64x64 lump
			pflatsize = 64;
			break;
	}

	hwrTexture->width  = (UINT16)pflatsize;
	hwrTexture->height = (UINT16)pflatsize;

	// the flat raw data needn't be converted with palettized textures
	W_ReadLump(flatlumpnum, Z_Malloc(W_LumpLength(flatlumpnum),
		PU_HWRCACHE, &hwrTexture->data));
}

static void HWR_CacheTextureAsFlat(HWRTexture_t *hwrTexture, INT32 texturenum)
{
	UINT8 *flat;
	UINT8 *converted;
	size_t size;

	// setup the texture info
	hwrTexture->format = GPU_TEXFMT_P_8;
	hwrTexture->flags = TF_WRAPXY|TF_CHROMAKEYED;

	hwrTexture->width  = (UINT16)textures[texturenum]->width;
	hwrTexture->height = (UINT16)textures[texturenum]->height;
	size = (hwrTexture->width * hwrTexture->height);

	flat = Z_Malloc(size, PU_HWRCACHE, &hwrTexture->data);
	converted = (UINT8 *)Picture_TextureToFlat(texturenum);
	M_Memcpy(flat, converted, size);
	Z_Free(converted);
}

// Download a Doom 'flat' to the hardware cache and make it ready for use
void HWR_LiterallyGetFlat(lumpnum_t flatlumpnum)
{
	HWRTexture_t *hwrtex;
	patch_t *patch;

	if (flatlumpnum == LUMPERROR)
		return;

	patch = HWR_GetCachedGLPatch(flatlumpnum);
	hwrtex = ((GLPatch_t *)Patch_AllocateHardwarePatch(patch))->texture;
	if (!hwrtex->downloaded && !hwrtex->data)
		HWR_CacheFlat(hwrtex, flatlumpnum);

	// If hardware does not have the texture, then call pfnSetTexture to upload it
	if (!hwrtex->downloaded)
		GPU->SetTexture(hwrtex);
	HWR_SetCurrentTexture(hwrtex);

	// The system-memory data can be purged now.
	Z_ChangeTag(hwrtex->data, PU_HWRCACHE_UNLOCKED);
}

void HWR_GetLevelFlat(levelflat_t *levelflat)
{
	// Who knows?
	if (levelflat == NULL)
		return;

	if (levelflat->type == LEVELFLAT_FLAT)
		HWR_LiterallyGetFlat(levelflat->u.flat.lumpnum);
	else if (levelflat->type == LEVELFLAT_TEXTURE)
	{
		GLMapTexture_t *grtex;
		INT32 texturenum = levelflat->u.texture.num;
#ifdef PARANOIA
		if ((unsigned)texturenum >= gl_numtextures)
			I_Error("HWR_GetLevelFlat: texturenum >= numtextures");
#endif

		// Who knows?
		if (texturenum == 0 || texturenum == -1)
			return;

		// Every texture in memory, stored as a 8-bit flat. Wow!
		grtex = &gl_flats[texturenum];

		// Generate flat if missing from the cache
		if (!grtex->texture.data && !grtex->texture.downloaded)
			HWR_CacheTextureAsFlat(&grtex->texture, texturenum);

		// If hardware does not have the texture, then call pfnSetTexture to upload it
		if (!grtex->texture.downloaded)
			GPU->SetTexture(&grtex->texture);
		HWR_SetCurrentTexture(&grtex->texture);

		// The system-memory data can be purged now.
		Z_ChangeTag(grtex->texture.data, PU_HWRCACHE_UNLOCKED);
	}
	else if (levelflat->type == LEVELFLAT_PATCH)
	{
		patch_t *patch = W_CachePatchNum(levelflat->u.flat.lumpnum, PU_CACHE);
		levelflat->width = (UINT16)(patch->width);
		levelflat->height = (UINT16)(patch->height);
		HWR_GetPatch(patch);
	}
#ifndef NO_PNG_LUMPS
	else if (levelflat->type == LEVELFLAT_PNG)
	{
		INT32 pngwidth = 0, pngheight = 0;
		HWRTexture_t *texture = levelflat->hwrTexture;
		UINT8 *flat;
		size_t size;

		// Cache the picture.
		if (!levelflat->picture)
		{
			levelflat->picture = Picture_PNGConvert(W_CacheLumpNum(levelflat->u.flat.lumpnum, PU_CACHE), PICFMT_FLAT, &pngwidth, &pngheight, NULL, NULL, W_LumpLength(levelflat->u.flat.lumpnum), NULL, 0);
			levelflat->width = (UINT16)pngwidth;
			levelflat->height = (UINT16)pngheight;
		}

		// Make the texture.
		if (texture == NULL)
		{
			texture = Z_Calloc(sizeof(HWRTexture_t), PU_LEVEL, NULL);
			texture->format = GPU_TEXFMT_P_8;
			texture->flags = TF_WRAPXY|TF_CHROMAKEYED;
			levelflat->hwrTexture = texture;
		}

		if (!texture->data && !texture->downloaded)
		{
			texture->width = levelflat->width;
			texture->height = levelflat->height;
			size = (texture->width * texture->height);
			flat = Z_Malloc(size, PU_LEVEL, &texture->data);
			if (levelflat->picture == NULL)
				I_Error("HWR_GetLevelFlat: levelflat->picture == NULL");
			M_Memcpy(flat, levelflat->picture, size);
		}

		// Tell the hardware driver to bind the current texture to the flat's texture
		GPU->SetTexture(texture);
	}
#endif
	else // set no texture
		HWR_SetCurrentTexture(NULL);
}

// --------------------+
// HWR_LoadPatchTexture : Generates a patch into a texture, usually the texture inside the patch itself
// --------------------+
static void HWR_LoadPatchTexture(patch_t *patch, HWRTexture_t *hwrTexture)
{
	GLPatch_t *grPatch = patch->hardware;
	if (!hwrTexture->downloaded && !hwrTexture->data)
		HWR_MakePatch(patch, grPatch, hwrTexture, true);

	// If hardware does not have the texture, then call pfnSetTexture to upload it
	if (!hwrTexture->downloaded)
		GPU->SetTexture(hwrTexture);
	HWR_SetCurrentTexture(hwrTexture);

	// The system-memory data can be purged now.
	Z_ChangeTag(hwrTexture->data, PU_HWRCACHE_UNLOCKED);
}

// -----------------+
// HWR_GetPatch     : Download a patch to the hardware cache and make it ready for use
// -----------------+
void HWR_GetPatch(patch_t *patch)
{
	if (!patch->hardware)
		Patch_CreateGL(patch);
	HWR_LoadPatchTexture(patch, ((GLPatch_t *)patch->hardware)->texture);
}

// -------------------+
// HWR_GetMappedPatch : Same as HWR_GetPatch for sprite color
// -------------------+
void HWR_GetMappedPatch(patch_t *patch, const UINT8 *colormap)
{
	GLPatch_t *grPatch;
	HWRTexture_t *hwrTexture, *newTexture;

	if (!patch->hardware)
		Patch_CreateGL(patch);
	grPatch = patch->hardware;

	if (colormap == colormaps || colormap == NULL)
	{
		// Load the default (green) color in hardware cache
		HWR_GetPatch(patch);
		return;
	}

	// search for the mimmap
	// skip the first (no colormap translated)
	for (hwrTexture = grPatch->texture; hwrTexture->nextcolormap; )
	{
		hwrTexture = hwrTexture->nextcolormap;
		if (hwrTexture->colormap == colormap)
		{
			HWR_LoadPatchTexture(patch, hwrTexture);
			return;
		}
	}

	// not found, create it!
	// If we are here, the sprite with the current colormap is not already in hardware memory
	newTexture = calloc(1, sizeof (*newTexture));
	if (newTexture == NULL)
		I_Error("%s: Out of memory", "HWR_GetMappedPatch");
	hwrTexture->nextcolormap = newTexture;

	newTexture->colormap = colormap;
	HWR_LoadPatchTexture(patch, newTexture);
}

void HWR_UnlockCachedPatch(GLPatch_t *gpatch)
{
	if (!gpatch)
		return;

	Z_ChangeTag(gpatch->texture->data, PU_HWRCACHE_UNLOCKED);
	Z_ChangeTag(gpatch, PU_HWRPATCHINFO_UNLOCKED);
}

static const INT32 picmode2GR[] =
{
	GPU_TEXFMT_P_8,                // PALETTE
	0,                             // INTENSITY          (unsupported yet)
	GPU_TEXFMT_ALPHA_INTENSITY_88, // INTENSITY_ALPHA    (corona use this)
	0,                             // RGB24              (unsupported yet)
	GPU_TEXFMT_RGBA,               // RGBA32
};

static void HWR_DrawPicInCache(UINT8 *block, INT32 pblockwidth, INT32 pblockheight,
	INT32 blockmodulo, pic_t *pic, INT32 bpp)
{
	INT32 i,j;
	fixed_t posx, posy, stepx, stepy;
	UINT8 *dest, *src, texel;
	UINT16 texelu16;
	INT32 picbpp;
	RGBA_t col;

	stepy = ((INT32)SHORT(pic->height)<<FRACBITS)/pblockheight;
	stepx = ((INT32)SHORT(pic->width)<<FRACBITS)/pblockwidth;
	picbpp = format2bpp(picmode2GR[pic->mode]);
	posy = 0;
	for (j = 0; j < pblockheight; j++)
	{
		posx = 0;
		dest = &block[j*blockmodulo];
		src = &pic->data[(posy>>FRACBITS)*SHORT(pic->width)*picbpp];
		for (i = 0; i < pblockwidth;i++)
		{
			switch (pic->mode)
			{ // source bpp
				case PALETTE :
					texel = src[(posx+FRACUNIT/2)>>FRACBITS];
					switch (bpp)
					{ // destination bpp
						case 1 :
							*dest++ = texel; break;
						case 2 :
							texelu16 = (UINT16)(texel | 0xff00);
							memcpy(dest, &texelu16, sizeof(UINT16));
							dest += sizeof(UINT16);
							break;
						case 3 :
							col = V_GetColor(texel);
							memcpy(dest, &col, sizeof(RGBA_t)-sizeof(UINT8));
							dest += sizeof(RGBA_t)-sizeof(UINT8);
							break;
						case 4 :
							memcpy(dest, &V_GetColor(texel), sizeof(RGBA_t));
							dest += sizeof(RGBA_t);
							break;
					}
					break;
				case INTENSITY :
					*dest++ = src[(posx+FRACUNIT/2)>>FRACBITS];
					break;
				case INTENSITY_ALPHA : // assume dest bpp = 2
					memcpy(dest, src + ((posx+FRACUNIT/2)>>FRACBITS)*sizeof(UINT16), sizeof(UINT16));
					dest += sizeof(UINT16);
					break;
				case RGB24 :
					break;  // not supported yet
				case RGBA32 : // assume dest bpp = 4
					dest += sizeof(UINT32);
					memcpy(dest, src + ((posx+FRACUNIT/2)>>FRACBITS)*sizeof(UINT32), sizeof(UINT32));
					break;
			}
			posx += stepx;
		}
		posy += stepy;
	}
}

// -----------------+
// HWR_GetPic       : Download a Doom pic (raw row encoded with no 'holes')
// Returns          :
// -----------------+
patch_t *HWR_GetPic(lumpnum_t lumpnum)
{
	patch_t *patch = HWR_GetCachedGLPatch(lumpnum);
	GLPatch_t *grPatch = (GLPatch_t *)(patch->hardware);

	if (!grPatch->texture->downloaded && !grPatch->texture->data)
	{
		pic_t *pic;
		UINT8 *block;
		size_t len;

		pic = W_CacheLumpNum(lumpnum, PU_CACHE);
		patch->width = SHORT(pic->width);
		patch->height = SHORT(pic->height);
		len = W_LumpLength(lumpnum) - sizeof (pic_t);

		grPatch->texture->width = (UINT16)patch->width;
		grPatch->texture->height = (UINT16)patch->height;

		if (pic->mode == PALETTE)
			grPatch->texture->format = gl_textureformat; // can be set by driver
		else
			grPatch->texture->format = picmode2GR[pic->mode];

		Z_Free(grPatch->texture->data);

		// allocate block
		block = MakeBlock(grPatch->texture);

		if (patch->width  == SHORT(pic->width) &&
			patch->height == SHORT(pic->height) &&
			format2bpp(grPatch->texture->format) == format2bpp(picmode2GR[pic->mode]))
		{
			// no conversion needed
			M_Memcpy(grPatch->texture->data, pic->data,len);
		}
		else
			HWR_DrawPicInCache(block, SHORT(pic->width), SHORT(pic->height),
			                   SHORT(pic->width)*format2bpp(grPatch->texture->format),
			                   pic,
			                   format2bpp(grPatch->texture->format));

		Z_Unlock(pic);
		Z_ChangeTag(block, PU_HWRCACHE_UNLOCKED);

		grPatch->texture->flags = 0;
		grPatch->max_s = grPatch->max_t = 1.0f;
	}
	GPU->SetTexture(grPatch->texture);
	//CONS_Debug(DBG_RENDER, "picloaded at %x as texture %d\n",grPatch->texture->data, grPatch->texture->downloaded);

	return patch;
}

patch_t *HWR_GetCachedGLPatchPwad(UINT16 wadnum, UINT16 lumpnum)
{
	lumpcache_t *lumpcache = wadfiles[wadnum]->patchcache;
	if (!lumpcache[lumpnum])
	{
		void *ptr = Z_Calloc(sizeof(patch_t), PU_PATCH, &lumpcache[lumpnum]);
		Patch_Create(NULL, 0, ptr);
		Patch_AllocateHardwarePatch(ptr);
	}
	return (patch_t *)(lumpcache[lumpnum]);
}

patch_t *HWR_GetCachedGLPatch(lumpnum_t lumpnum)
{
	return HWR_GetCachedGLPatchPwad(WADFILENUM(lumpnum),LUMPNUM(lumpnum));
}

// Need to do this because they aren't powers of 2
static void HWR_DrawFadeMaskInCache(HWRTexture_t *texture, INT32 pblockwidth, INT32 pblockheight,
	lumpnum_t fademasklumpnum, UINT16 fmwidth, UINT16 fmheight)
{
	INT32 i,j;
	fixed_t posx, posy, stepx, stepy;
	UINT8 *block = texture->data; // places the data directly into here
	UINT8 *flat;
	UINT8 *dest, *src, texel;
	RGBA_t col;

	// Place the flats data into flat
	W_ReadLump(fademasklumpnum, Z_Malloc(W_LumpLength(fademasklumpnum),
		PU_HWRCACHE, &flat));

	stepy = ((INT32)fmheight<<FRACBITS)/pblockheight;
	stepx = ((INT32)fmwidth<<FRACBITS)/pblockwidth;
	posy = 0;
	for (j = 0; j < pblockheight; j++)
	{
		posx = 0;
		dest = &block[j*(texture->width)]; // 1bpp
		src = &flat[(posy>>FRACBITS)*SHORT(fmwidth)];
		for (i = 0; i < pblockwidth;i++)
		{
			// fademask bpp is always 1, and is used just for alpha
			texel = src[(posx)>>FRACBITS];
			col = V_GetColor(texel);
			*dest = col.s.red; // take the red level of the colour and use it for alpha, as fademasks do

			dest++;
			posx += stepx;
		}
		posy += stepy;
	}

	Z_Free(flat);
}

static void HWR_CacheFadeMask(HWRTexture_t *hwrTexture, lumpnum_t fademasklumpnum)
{
	size_t size;
	UINT16 fmheight = 0, fmwidth = 0;

	// setup the texture info
	hwrTexture->format = GPU_TEXFMT_ALPHA_8; // put the correct alpha levels straight in so I don't need to convert it later
	hwrTexture->flags = 0;

	size = W_LumpLength(fademasklumpnum);

	switch (size)
	{
		// None of these are powers of 2, so I'll need to do what is done for textures and make them powers of 2 before they can be used
		case 256000: // 640x400
			fmwidth = 640;
			fmheight = 400;
			break;
		case 64000: // 320x200
			fmwidth = 320;
			fmheight = 200;
			break;
		case 16000: // 160x100
			fmwidth = 160;
			fmheight = 100;
			break;
		case 4000: // 80x50 (minimum)
			fmwidth = 80;
			fmheight = 50;
			break;
		default: // Bad lump
			CONS_Alert(CONS_WARNING, "Fade mask lump of incorrect size, ignored\n"); // I should avoid this by checking the lumpnum in HWR_RunWipe
			break;
	}

	// Thankfully, this will still work for this scenario
	hwrTexture->width  = fmwidth;
	hwrTexture->height = fmheight;

	MakeBlock(hwrTexture);

	HWR_DrawFadeMaskInCache(hwrTexture, fmwidth, fmheight, fademasklumpnum, fmwidth, fmheight);

	// I DO need to convert this because it isn't power of 2 and we need the alpha
}


void HWR_GetFadeMask(lumpnum_t fademasklumpnum)
{
	patch_t *patch = HWR_GetCachedGLPatch(fademasklumpnum);
	HWRTexture_t *hwrtex = ((GLPatch_t *)Patch_AllocateHardwarePatch(patch))->texture;
	if (!hwrtex->downloaded && !hwrtex->data)
		HWR_CacheFadeMask(hwrtex, fademasklumpnum);

	GPU->SetTexture(hwrtex);

	// The system-memory data can be purged now.
	Z_ChangeTag(hwrtex->data, PU_HWRCACHE_UNLOCKED);
}

#endif //HWRENDER
