// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_modeltextures.c
/// \brief 3D model texture loading.

#include "doomdef.h"
#include "doomdata.h"
#include "doomstat.h"

#include "r_modeltextures.h"

#ifdef SWRASTERIZER
#include "swrasterizer/swrast.h"
#endif

#ifdef HWRENDER
#include "hardware/hw_md2.h"
#endif

#include "d_main.h"
#include "w_wad.h"
#include "z_zone.h"
#include "v_video.h"
#include "i_video.h"

#ifdef HAVE_PNG

#ifndef _MSC_VER
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#endif

#ifndef _LFS64_LARGEFILE
#define _LFS64_LARGEFILE
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 0
#endif

 #include "png.h"
 #ifndef PNG_READ_SUPPORTED
 #undef HAVE_PNG
 #endif
 #if PNG_LIBPNG_VER < 100207
 //#undef HAVE_PNG
 #endif

static void Model_PNGError(png_structp PNG, png_const_charp pngtext)
{
	(void)PNG;
	CONS_Debug(DBG_RENDER, "Model_PNGError: %s", pngtext);
}

static void Model_PNGWarning(png_structp PNG, png_const_charp pngtext)
{
	(void)PNG;
	CONS_Debug(DBG_RENDER, "Model_PNGWarning: %s", pngtext);
}

//
// Model_PNGLoad
// Loads a texture for a model, from a PNG file.
//
static RGBA_t *Model_PNGLoad(const char *filename, int *rwidth, int *rheight, int *size)
{
	png_structp png_ptr;
	png_infop png_info_ptr;
	png_uint_32 width, height;
	int bit_depth, color_type;
	png_bytep PNG_image;
	int imgsize;
#ifdef PNG_SETJMP_SUPPORTED
#ifdef USE_FAR_KEYWORD
	jmp_buf jmpbuf;
#endif
#endif
	png_FILE_p png_FILE;
	//Filename checking fixed ~Monster Iestyn and Golden
	char *pngfilename = va("%s"PATHSEP"%s"PATHSEP"%s", srb2home, modelsfolder, filename);

	FIL_ForceExtension(pngfilename, ".png");
	png_FILE = fopen(pngfilename, "rb");
	if (!png_FILE)
	{
		CONS_Debug(DBG_RENDER, "Model_PNGLoad: Error on opening \"%s\" for loading\n", filename);
		return NULL;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, Model_PNGError, Model_PNGWarning);
	if (!png_ptr)
	{
		CONS_Debug(DBG_RENDER, "Model_PNGLoad: Initialization error\n");
		fclose(png_FILE);
		return NULL;
	}

	png_info_ptr = png_create_info_struct(png_ptr);
	if (!png_info_ptr)
	{
		CONS_Debug(DBG_RENDER, "Model_PNGLoad: Allocation error\n");
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		fclose(png_FILE);
		return NULL;
	}

#ifdef USE_FAR_KEYWORD
	if (setjmp(jmpbuf))
#else
	if (setjmp(png_jmpbuf(png_ptr)))
#endif
	{
		CONS_Debug(DBG_RENDER, "libpng load error on %s\n", filename);
		png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);
		fclose(png_FILE);
		return NULL;
	}
#ifdef USE_FAR_KEYWORD
	png_memcpy(png_jmpbuf(png_ptr), jmpbuf, sizeof jmp_buf);
#endif

	png_init_io(png_ptr, png_FILE);

#ifdef PNG_SET_USER_LIMITS_SUPPORTED
	png_set_user_limits(png_ptr, 2048, 2048);
#endif

	png_read_info(png_ptr, png_info_ptr);

	png_get_IHDR(png_ptr, png_info_ptr, &width, &height, &bit_depth, &color_type,
	 NULL, NULL, NULL);

	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);
	else if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

	if (png_get_valid(png_ptr, png_info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);
	else if (color_type != PNG_COLOR_TYPE_RGB_ALPHA && color_type != PNG_COLOR_TYPE_GRAY_ALPHA)
	{
#if PNG_LIBPNG_VER < 10207
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
#else
		png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
#endif
	}

	png_read_update_info(png_ptr, png_info_ptr);

	{
		png_uint_32 i, pitch = png_get_rowbytes(png_ptr, png_info_ptr);
		png_bytepp row_pointers;

		imgsize = pitch*height;
		PNG_image = Z_Malloc(imgsize, PU_STATIC, NULL);
		row_pointers = png_malloc(png_ptr, height * sizeof (png_bytep));

		for (i = 0; i < height; i++)
			row_pointers[i] = PNG_image + i*pitch;
		png_read_image(png_ptr, row_pointers);
		png_free(png_ptr, (png_voidp)row_pointers);
	}

	png_destroy_read_struct(&png_ptr, &png_info_ptr, NULL);

	fclose(png_FILE);
	*rwidth = (int)width;
	*rheight = (int)height;
	*size = imgsize;
	return (RGBA_t *)PNG_image;
}
#endif // HAVE_PNG

typedef struct
{
	UINT8 manufacturer;
	UINT8 version;
	UINT8 encoding;
	UINT8 bitsPerPixel;
	INT16 xmin;
	INT16 ymin;
	INT16 xmax;
	INT16 ymax;
	INT16 hDpi;
	INT16 vDpi;
	UINT8 colorMap[48];
	UINT8 reserved;
	UINT8 numPlanes;
	INT16 bytesPerLine;
	INT16 paletteInfo;
	INT16 hScreenSize;
	INT16 vScreenSize;
	UINT8 filler[54];
} PcxHeader;

//
// Model_PCXLoad
// Loads a texture for a model, from a PCX file.
//
static RGBA_t *Model_PCXLoad(const char *filename, int *rwidth, int *rheight, int *rsize)
{
	PcxHeader header;
#define PALSIZE 768
	UINT8 palette[PALSIZE];
	const UINT8 *pal;
	RGBA_t *image;
	size_t pw, ph, size, ptr = 0;
	INT32 ch, rep;
	FILE *file;
	//Filename checking fixed ~Monster Iestyn and Golden
	char *pcxfilename = va("%s"PATHSEP"%s"PATHSEP"%s", srb2home, modelsfolder, filename);

	FIL_ForceExtension(pcxfilename, ".pcx");
	file = fopen(pcxfilename, "rb");
	if (!file)
	{
		CONS_Debug(DBG_RENDER, "Model_PCXLoad: Error on opening \"%s\" for loading\n", filename);
		return NULL;
	}

	if (fread(&header, sizeof (PcxHeader), 1, file) != 1)
	{
		CONS_Debug(DBG_RENDER, "Model_PCXLoad: Invalid header in file \"%s\"\n", filename);
		fclose(file);
		return NULL;
	}

	if (header.bitsPerPixel != 8)
	{
		CONS_Debug(DBG_RENDER, "Model_PCXLoad: Unsupported bits per pixel value in file \"%s\"\n", filename);
		fclose(file);
		return NULL;
	}

	fseek(file, -PALSIZE, SEEK_END);

	pw = *rwidth = header.xmax - header.xmin + 1;
	ph = *rheight = header.ymax - header.ymin + 1;
	*rsize = pw*ph*4;
	image = Z_Malloc(*rsize, PU_STATIC, NULL);

	if (fread(palette, sizeof (UINT8), PALSIZE, file) != PALSIZE)
	{
		Z_Free(image);
		fclose(file);
		return NULL;
	}
	fseek(file, sizeof (PcxHeader), SEEK_SET);

	size = pw * ph;
	while (ptr < size)
	{
		ch = fgetc(file);
		if (ch >= 192)
		{
			rep = ch - 192;
			ch = fgetc(file);
		}
		else
		{
			rep = 1;
		}
		while (rep--)
		{
			pal = palette + ch*3;
			image[ptr].s.red   = *pal++;
			image[ptr].s.green = *pal++;
			image[ptr].s.blue  = *pal++;
			image[ptr].s.alpha = 0xFF;
			ptr++;
		}
	}
	fclose(file);
	return (RGBA_t*)image;
}

//
// Model_LoadTexture
// Download a PNG or PCX texture for models.
//
boolean Model_LoadTexture(modelinfo_t *model)
{
	modeltexturedata_t *texture = NULL;
	const char *filename = model->filename;
	int w = 0, h = 0;
	int size = 0;
	RGBA_t *image;
	RGBA_t *loadedimg;
#ifdef HWRENDER
	GLPatch_t *grpatch = NULL;
#endif

	// make new texture
	if (!model->texture->base)
	{
		texture = Z_Calloc(sizeof *texture, PU_STATIC, &(model->texture->base));
		texture->found = true;
	}
	else
		texture = model->texture->base;

#ifdef SWRASTERIZER
	if (rendermode == render_soft)
		SWRast_FreeModelTexture(model);
	else
#endif
#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		if (model->texture->grpatch)
		{
			grpatch = model->texture->grpatch;
			Z_Free(grpatch->mipmap->data);
		}
		else
		{
			grpatch = Z_Calloc(sizeof *grpatch, PU_HWRPATCHINFO, &(model->texture->grpatch));
			grpatch->mipmap = Z_Calloc(sizeof (GLMipmap_t), PU_HWRPATCHINFO, NULL);
		}
	}
#endif

	// We already failed trying to find the texture file, so don't do it again.
	if (!texture->found)
		return false;

	// load texture
	if (!texture->data)
	{
#ifdef HAVE_PNG
		loadedimg = Model_PNGLoad(filename, &w, &h, &size);
		if (!loadedimg)
#endif
		{
			loadedimg = Model_PCXLoad(filename, &w, &h, &size);
			if (!loadedimg)
			{
				// Not found? Return.
				texture->found = false;
				return false;
			}
		}

		Z_Calloc(size, PU_STATIC, &texture->data);

		// copy texture
		image = texture->data;
		M_Memcpy(image, loadedimg, size);
		Z_Free(loadedimg);

		texture->width = (INT16)w;
		texture->height = (INT16)h;
		texture->size = size;
	}

	// copy texture into renderer memory
#ifdef SWRASTERIZER
	if (rendermode == render_soft)
	{
		// create base texture
		SWRast_CreateModelTexture(model, 0, 0);
		return true;
	}
#endif

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		if (!grpatch->mipmap->downloaded && !grpatch->mipmap->data)
		{
			// texture is RGBA, right??
			Z_Calloc(texture->size, PU_HWRMODELTEXTURE, &grpatch->mipmap->data);

			// copy texture
			image = grpatch->mipmap->data;
			M_Memcpy(image, texture->data, texture->size);

			grpatch->mipmap->format = GL_TEXFMT_RGBA;
			grpatch->mipmap->downloaded = 0;
			grpatch->mipmap->flags = 0;

			grpatch->width = (INT16)texture->width;
			grpatch->height = (INT16)texture->height;
			grpatch->mipmap->width = (UINT16)texture->width;
			grpatch->mipmap->height = (UINT16)texture->height;

			// Lactozilla: Apply colour cube
			size = w*h;
			while (size--)
			{
				V_CubeApply(&image->s.red, &image->s.green, &image->s.blue);
				image++;
			}
		}
		return true;
	}
#endif

	return false;
}

//
// Model_LoadBlendTexture
// Download a PNG or PCX texture for blending models.
//
boolean Model_LoadBlendTexture(modelinfo_t *model)
{
	modeltexturedata_t *texture = NULL;
	int w = 0, h = 0;
	int size = 0;
	RGBA_t *image;
	RGBA_t *loadedimg;
#ifdef HWRENDER
	GLPatch_t *grpatch = NULL;
#endif

	// make filename
	char *filename = Z_Malloc(strlen(model->filename)+7, PU_STATIC, NULL);
	strcpy(filename, model->filename);
	FIL_ForceExtension(filename, "_blend.png");

	// make new texture
	if (!model->texture->blend)
	{
		texture = Z_Calloc(sizeof *texture, PU_STATIC, &(model->texture->blend));
		texture->found = true;
	}
	else
		texture = model->texture->blend;

#ifdef SWRASTERIZER
	if (rendermode == render_soft)
		SWRast_FreeModelBlendTexture(model);
	else
#endif
#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		if (model->texture->blendgrpatch)
		{
			grpatch = model->texture->blendgrpatch;
			Z_Free(grpatch->mipmap->data);
		}
		else
		{
			grpatch = Z_Calloc(sizeof *grpatch, PU_HWRPATCHINFO, &(model->texture->blendgrpatch));
			grpatch->mipmap = Z_Calloc(sizeof (GLMipmap_t), PU_HWRPATCHINFO, NULL);
		}
	}
#endif

	// We already failed trying to find the texture file, so don't do it again.
	if (!texture->found)
		return false;

	// load texture
	if (!texture->data)
	{
#ifdef HAVE_PNG
		loadedimg = Model_PNGLoad(filename, &w, &h, &size);
		if (!loadedimg)
#endif
		{
			loadedimg = Model_PCXLoad(filename, &w, &h, &size);
			if (!loadedimg)
			{
				// Not found? Return.
				texture->found = false;
				Z_Free(filename);
				return false;
			}
		}

		Z_Calloc(size, PU_STATIC, &texture->data);

		// copy texture
		image = texture->data;
		M_Memcpy(image, loadedimg, size);
		Z_Free(loadedimg);

		texture->width = (INT16)w;
		texture->height = (INT16)h;
		texture->size = size;
	}

	// copy texture into renderer memory
#ifdef SWRASTERIZER
	if (rendermode == render_soft)
	{
		// nothing to do here
		Z_Free(filename);
		return true;
	}
#endif

#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		if (!grpatch->mipmap->downloaded && !grpatch->mipmap->data)
		{
			// texture is RGBA, right??
			Z_Calloc(texture->size, PU_HWRMODELTEXTURE, &grpatch->mipmap->data);

			// copy texture
			image = grpatch->mipmap->data;
			M_Memcpy(image, texture->data, texture->size);

			grpatch->mipmap->format = GL_TEXFMT_RGBA;
			grpatch->mipmap->downloaded = 0;
			grpatch->mipmap->flags = 0;

			grpatch->width = (INT16)texture->width;
			grpatch->height = (INT16)texture->height;
			grpatch->mipmap->width = (UINT16)texture->width;
			grpatch->mipmap->height = (UINT16)texture->height;
		}

		Z_Free(filename);
		return true;
	}
#endif

	Z_Free(filename);
	return false;
}
