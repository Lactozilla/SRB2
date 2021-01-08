// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2021 by Jaime Ita Passos.
// Copyright (C) 2019 by "Arkus-Kotan".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  rsp_model.c
/// \brief Model loading

#include "r_softpoly.h"

#ifdef __GNUC__
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../doomdef.h"
#include "../doomstat.h"
#include "../d_main.h"
#include "../r_bsp.h"
#include "../r_main.h"
#include "../m_misc.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../r_things.h"
#include "../v_video.h"
#include "../r_draw.h"
#include "../p_tick.h"
#include "../r_model.h"
#include "../r_modeltextures.h"

// Macros to improve code readability
#define MD3_XYZ_SCALE   (1.0f / 64.0f)
#define VERTEX_OFFSET   ((i * 9) + (j * 3))		/* (i * 9) = (XYZ coords * vertex count) */
#define UV_OFFSET       ((i * 6) + (j * 2))		/* (i * 6) = (UV coords * vertex count) */

static fpquaternion_t *modelquaternion = NULL;

boolean RSP_RenderModel(vissprite_t *spr)
{
	INT32 frameIndex = 0;
	INT32 nextFrameIndex = -1;
	INT32 mod;
	UINT8 spr2 = 0;
	modelinfo_t *md2;
	INT32 meshnum;

	// Not funny id Software, didn't laugh.
	unsigned short idx = 0;
	boolean useTinyFrames;

	mobj_t *mobj = spr->mobj;
	if (!mobj)
		return false;

	// Moment of truth. Is the model VISIBLE?
	if (spr->x2 <= spr->x1)
		return true;

	// load sprite viewpoint
	if (rsp_maskdraw)
		RSP_RestoreSpriteViewpoint(spr);

	if (modelquaternion == NULL)
	{
		fpquaternion_t quat = RSP_QuaternionFromEuler(0.0f, 0.0f, -90.0f);
		modelquaternion = Z_Malloc(sizeof(fpquaternion_t), PU_SOFTPOLY, NULL);
		M_Memcpy(modelquaternion, &quat, sizeof(fpquaternion_t));
	}

	// Look at R_ProjectSprite for more
	{
		fpvector4_t modelvec;
		rsp_texture_t *texture, sprtex;
		rsp_spritetexture_t *sprtexp;
		INT32 durs = mobj->state->tics;
		INT32 tics = mobj->tics;
		boolean vflip = (!(mobj->eflags & MFE_VERTICALFLIP) != !(mobj->frame & FF_VERTICALFLIP));
		boolean papersprite = (mobj->frame & FF_PAPERSPRITE);
		angle_t ang;
		angle_t rollangle = 0;
		fpquaternion_t rollquaternion;
		fpvector4_t rolltranslation;
		spritedef_t *sprdef;
		spriteframe_t *sprframe;
#ifdef ROTSPRITE
		spriteinfo_t *sprinfo;
#endif
		float finalscale;
		float pol = 0.0f;
		INT32 skinnum = 0;
		INT32 tc = 0, textc = 0;

		skincolornum_t skincolor = SKINCOLOR_NONE;
		UINT8 *translation = NULL;

#define RESETVIEW { \
	if (rsp_maskdraw) \
		RSP_RestoreViewpoint(); \
}

		md2 = (modelinfo_t *)spr->model;
		if (!md2)
		{
			RESETVIEW
			return false;
		}

		// Lactozilla: Disallow certain models from rendering
		if (!Model_AllowRendering(mobj))
		{
			RESETVIEW
			return false;
		}

		// texture blending
		if (mobj->color)
			skincolor = (skincolornum_t)mobj->color;
		else if (mobj->sprite == SPR_PLAY) // Looks like a player, but doesn't have a color? Get rid of green sonic syndrome.
			skincolor = (skincolornum_t)skins[0].prefcolor;

		// load normal texture
		if (!md2->texture->rsp_tex.data)
		{
			if (mobj->skin)
				skinnum = (skin_t*)mobj->skin-skins;
			Model_LoadTexture(md2);
			Model_LoadBlendTexture(md2);
		}

		// set translation
		if ((mobj->flags & (MF_ENEMY|MF_BOSS)) && (mobj->flags2 & MF2_FRET) && !(mobj->flags & MF_GRENADEBOUNCE) && (leveltime & 1)) // Bosses "flash"
		{
			if (mobj->type == MT_CYBRAKDEMON)
				tc = TC_ALLWHITE;
			else if (mobj->type == MT_METALSONIC_BATTLE)
				tc = TC_METALSONIC;
			else
				tc = TC_BOSS;
		}
		else if (mobj->colorized)
			translation = R_GetTranslationColormap(TC_RAINBOW, mobj->color, GTC_CACHE);
		else if (mobj->player && mobj->player->dashmode >= DASHMODE_THRESHOLD
			&& (mobj->player->charflags & SF_DASHMODE)
			&& ((leveltime/2) & 1))
		{
			if (mobj->player->charflags & SF_MACHINE)
				tc = TC_DASHMODE;
			else
				translation = R_GetTranslationColormap(TC_RAINBOW, mobj->color, GTC_CACHE);
		}
		else
		{
			if (mobj->color)
				tc = TC_DEFAULT; // intentional
		}

		// load translated texture
		textc = -tc;

		// TC_METALSONIC includes an actual skincolor translation, on top of its flashing.
		if (textc == TC_METALSONIC)
			skincolor = SKINCOLOR_COBALT;

		if (textc && md2->texture->rsp_blendtex[textc][skincolor].data == NULL)
			RSP_CreateModelTexture(md2, textc, skincolor);

		// use corresponding texture for this model
		if (md2->texture->rsp_blendtex[textc][skincolor].data != NULL)
			texture = &md2->texture->rsp_blendtex[textc][skincolor];
		else
			texture = &md2->texture->rsp_tex;

		if (mobj->skin && mobj->sprite == SPR_PLAY)
		{
			sprdef = &((skin_t *)mobj->skin)->sprites[mobj->sprite2];
#ifdef ROTSPRITE
			sprinfo = &((skin_t *)mobj->skin)->sprinfo[mobj->sprite2];
#endif
		}
		else
		{
			sprdef = &sprites[mobj->sprite];
#ifdef ROTSPRITE
			sprinfo = &spriteinfo[mobj->sprite];
#endif
		}

		sprframe = &sprdef->spriteframes[mobj->frame & FF_FRAMEMASK];

		if (!texture->data)
		{
			unsigned rot;
			UINT8 flip;
			lumpcache_t *lumpcache;
			lumpnum_t lumpnum;
			UINT16 wad, lump;

			if (sprframe->rotate != SRF_SINGLE || papersprite)
				ang = R_PointToAngle (mobj->x, mobj->y) - (mobj->player ? mobj->player->drawangle : mobj->angle);

			if (sprframe->rotate == SRF_SINGLE)
			{
				// use single rotation for all views
				rot = 0;                        //Fab: for vis->patch below
				flip = sprframe->flip; // Will only be 0x00 or 0xFF
			}
			else
			{
				// choose a different rotation based on player view
				if ((sprframe->rotate & SRF_RIGHT) && (ang < ANGLE_180)) // See from right
					rot = 6; // F7 slot
				else if ((sprframe->rotate & SRF_LEFT) && (ang >= ANGLE_180)) // See from left
					rot = 2; // F3 slot
				else // Normal behaviour
					rot = (ang+ANGLE_202h)>>29;

				flip = sprframe->flip & (1<<rot);
			}

			// get rsp_texture
			lumpnum = sprframe->lumppat[rot];
			wad = WADFILENUM(lumpnum);
			lump = LUMPNUM(lumpnum);

			if (!wadfiles[wad]->patchcache)
			{
				RESETVIEW
				return false;
			}

			lumpcache = wadfiles[wad]->patchcache->software;
			if (!lumpcache[lump])
			{
				RESETVIEW
				return false;
			}

			sprtexp = lumpcache[lump];
			sprtexp += rot;
			if (!sprtexp)
			{
				RESETVIEW
				return false;
			}

			sprtex.width = sprtexp->width;
			sprtex.height = sprtexp->height;

			// make the patch
			if (!sprtexp->data)
			{
				patch_t *source;
				size_t size;

				// uuhhh.....
				if (!sprtexp->lumpnum)
				{
					RESETVIEW
					return false;
				}

				// still not a patch yet
				// (R_CheckIfPatch has most likely failed)
				if ((sprtexp->width) < 1 || (sprtexp->height < 1))
				{
					RESETVIEW
					return false;
				}

				// cache the source patch
				source = (patch_t *)W_CacheLumpNum(sprtexp->lumpnum, PU_STATIC);

				// make the buffer
				size = (sprtexp->width * sprtexp->height);
				sprtexp->data = Z_Calloc(size * sizeof(UINT16), PU_SOFTPOLY, NULL);

				// generate the texture, then clear lumpnum
				R_GenerateSpriteTexture(source, sprtexp->data, 0, 0, sprtexp->width, sprtexp->height, flip, NULL, NULL);
				sprtexp->lumpnum = 0;

				// aight bro u have lost yuor cache privileges
				Z_Free(source);
			}

			// Set the translation here because no blend texture was made
			if (tc == TC_DEFAULT && (mobj->skin))
				tc = skinnum;
			translation = R_GetTranslationColormap(tc, mobj->color, GTC_CACHE);

			sprtex.data = sprtexp->data;
			texture = &sprtex;
		}

		if (mobj->frame & FF_ANIMATE)
		{
			// set duration and tics to be the correct values for FF_ANIMATE states
			durs = mobj->state->var2;
			tics = mobj->anim_duration;
		}

		frameIndex = (mobj->frame & FF_FRAMEMASK);
		if (mobj->skin && mobj->sprite == SPR_PLAY && md2->model->spr2frames)
		{
			spr2 = Model_GetSprite2(md2, mobj->skin, mobj->sprite2, mobj->player);
			mod = md2->model->spr2frames[spr2].numframes;
#ifndef DONTHIDEDIFFANIMLENGTH // by default, different anim length is masked by the mod
			if (mod > (INT32)((skin_t *)mobj->skin)->sprites[spr2].numframes)
				mod = ((skin_t *)mobj->skin)->sprites[spr2].numframes;
#endif
			if (!mod)
				mod = 1;
			frameIndex = md2->model->spr2frames[spr2].frames[frameIndex%mod];
		}
		else
		{
			mod = md2->model->meshes[0].numFrames;
			if (!mod)
				mod = 1;
		}

#ifdef USE_MODEL_NEXTFRAME
		if (cv_modelinterpolation.value && tics <= durs && tics <= MODEL_INTERPOLATION_LIMIT)
		{
			if (durs > MODEL_INTERPOLATION_LIMIT)
				durs = MODEL_INTERPOLATION_LIMIT;

			if (mobj->skin && mobj->sprite == SPR_PLAY && md2->model->spr2frames)
			{
				if (Model_CanInterpolateSprite2(&md2->model->spr2frames[spr2])
					&& (mobj->frame & FF_ANIMATE
					|| (mobj->state->nextstate != S_NULL
					&& states[mobj->state->nextstate].sprite == SPR_PLAY
					&& ((P_GetSkinSprite2(mobj->skin, (((mobj->player && mobj->player->powers[pw_super]) ? FF_SPR2SUPER : 0)|states[mobj->state->nextstate].frame) & FF_FRAMEMASK, mobj->player) == mobj->sprite2)))))
				{
					nextFrameIndex = (mobj->frame & FF_FRAMEMASK) + 1;
					if (nextFrameIndex >= mod)
						nextFrameIndex = 0;
					if (frameIndex || !(mobj->state->frame & FF_SPR2ENDSTATE))
						nextFrameIndex = md2->model->spr2frames[spr2].frames[nextFrameIndex];
					else
						nextFrameIndex = -1;
				}
			}
			else if (Model_CanInterpolate(mobj, md2->model))
			{
				// frames are handled differently for states with FF_ANIMATE, so get the next frame differently for the interpolation
				if (mobj->frame & FF_ANIMATE)
				{
					nextFrameIndex = (mobj->frame & FF_FRAMEMASK) + 1;
					if (nextFrameIndex >= (INT32)(mobj->state->var1 + (mobj->state->frame & FF_FRAMEMASK)))
						nextFrameIndex = (mobj->state->frame & FF_FRAMEMASK) % mod;
				}
				else
				{
					if (mobj->state->nextstate != S_NULL && states[mobj->state->nextstate].sprite != SPR_NULL
					&& !(mobj->player && (mobj->state->nextstate == S_PLAY_WAIT) && mobj->state == &states[S_PLAY_STND]))
						nextFrameIndex = (states[mobj->state->nextstate].frame & FF_FRAMEMASK) % mod;
				}
			}

			// don't interpolate if instantaneous or infinite in length
			if (durs != 0 && durs != -1 && tics != -1)
			{
				UINT32 newtime = (durs - tics);

				pol = (newtime)/(float)durs;

				if (pol > 1.0f)
					pol = 1.0f;

				if (pol < 0.0f)
					pol = 0.0f;
			}
		}
#endif

		// SRB2CBTODO: MD2 scaling support
		finalscale = md2->scale * FIXED_TO_FLOAT(mobj->scale);
		finalscale *= 0.5f;

#ifdef ROTSPRITE
		if (mobj->rollangle)
		{
			int rollflip = 1;
			rotaxis_t rotaxis = ROTAXIS_Y;
			float roll;
			fixed_t froll;
			float modelcenterx, modelcentery;

			rollangle = mobj->rollangle;
			froll = AngleFixed(rollangle);
			roll = FIXED_TO_FLOAT(froll);

			// rotation axis
			if (sprinfo->available)
				rotaxis = (UINT8)(sprinfo->pivot[(mobj->frame & FF_FRAMEMASK)].rotaxis);

			// for NiGHTS specifically but should work everywhere else
			ang = R_PointToAngle(mobj->x, mobj->y) - (mobj->player ? mobj->player->drawangle : mobj->angle);
			if ((sprframe->rotate & SRF_RIGHT) && (ang < ANGLE_180)) // See from right
				rollflip = 1;
			else if ((sprframe->rotate & SRF_LEFT) && (ang >= ANGLE_180)) // See from left
				rollflip = -1;

			roll *= rollflip;
			if (rotaxis == 2) // Z
				rollquaternion = RSP_QuaternionFromEuler(0.0f, roll, 0.0f);
			else if (rotaxis == 1) // Y
				rollquaternion = RSP_QuaternionFromEuler(0.0f, 0.0f, roll);
			else // X
				rollquaternion = RSP_QuaternionFromEuler(roll, 0.0f, 0.0f);

			modelcenterx = FIXED_TO_FLOAT(mobj->radius/2);
			modelcentery = FIXED_TO_FLOAT(mobj->height/2);
			RSP_MakeVector4(rolltranslation, modelcenterx, 0.0f, modelcentery);
		}
#endif

		// Render every mesh
		for (meshnum = 0; meshnum < md2->model->numMeshes; meshnum++)
		{
			mesh_t *mesh = &md2->model->meshes[meshnum];
			rsp_triangle_t triangle;
			float theta, cs, sn;
			fixed_t model_angle;
			UINT16 i, j;
			float scale = finalscale;

			mdlframe_t *frame = NULL, *nextframe = NULL;
			tinyframe_t *tinyframe = NULL, *tinynextframe = NULL;

			useTinyFrames = md2->model->meshes[meshnum].tinyframes != NULL;
			if (useTinyFrames)
			{
				tinyframe = &mesh->tinyframes[frameIndex % mesh->numFrames];
				if (nextFrameIndex != -1)
					tinynextframe = &mesh->tinyframes[nextFrameIndex % mesh->numFrames];
				scale *= MD3_XYZ_SCALE;
			}
			else
			{
				frame = &mesh->frames[frameIndex % mesh->numFrames];
				if (nextFrameIndex != -1)
					nextframe = &mesh->frames[nextFrameIndex % mesh->numFrames];
			}

			// clear triangle struct
			// avoid undefined behaviour.............
			memset(&triangle, 0x00, sizeof(rsp_triangle_t));

			// set triangle texture
			if (texture->data)
				triangle.texture = texture;

			// set colormap, translation and transmap
			triangle.colormap = spr->colormap;
			if (spr->extra_colormap)
			{
				if (!triangle.colormap)
					triangle.colormap = spr->extra_colormap->colormap;
				else
					triangle.colormap = &spr->extra_colormap->colormap[triangle.colormap - colormaps];
			}
			triangle.translation = translation;
			triangle.transmap = spr->transmap;

			// vertical flip
			triangle.flipped = vflip;

			// set model angle
			if ((sprframe->rotate == SRF_SINGLE) && (!papersprite))
				ang = (R_PointToAngle(mobj->x, mobj->y) - ANGLE_180);
			else if (mobj->player)
				ang = mobj->player->drawangle;
			else
				ang = mobj->angle;

			// convert to fixed-point, then to radians in float
			model_angle = AngleFixed(ang);
			theta = -(FIXED_TO_FLOAT(model_angle) * M_PI / 180.0f);
			cs = cos(theta);
			sn = sin(theta);

			// render every triangle
			for (i = 0; i < mesh->numTriangles; i++)
			{
				float x, y, z;
				float s, t;
				float *uv = mesh->uvs;

				x = FIXED_TO_FLOAT(mobj->x);
				y = FIXED_TO_FLOAT(mobj->y) + md2->offset;

				if (mobj->eflags & MFE_VERTICALFLIP)
					z = FIXED_TO_FLOAT(mobj->z + mobj->height);
				else
					z = FIXED_TO_FLOAT(mobj->z);

				// Rotate the triangle -90 degrees and invert the Z coordinate
				#define FIXTRIANGLE(rotx, roty, rotz) \
				{ \
					RSP_MakeVector4(modelvec, rotx, roty, rotz); \
					RSP_QuaternionRotateVector(&modelvec, modelquaternion); \
					if (rollangle) \
					{ \
						modelvec = RSP_VectorAdd(&modelvec, &rolltranslation); \
						RSP_QuaternionRotateVector(&modelvec, &rollquaternion); \
						modelvec = RSP_VectorSubtract(&modelvec, &rolltranslation); \
					} \
					rotx = modelvec.x; \
					roty = modelvec.y; \
					rotz = modelvec.z; \
					roty *= -1.0; \
				} \

				for (j = 0; j < 3; j++)
				{
					if (useTinyFrames)
					{
						idx = mesh->indices[(i * 3) + j];
						s = *(uv + (idx * 2));
						t = *(uv + (idx * 2) + 1);
					}
					else
					{
						s = uv[UV_OFFSET];
						t = uv[UV_OFFSET+1];
					}

					if (!(nextframe || tinynextframe) || fpclassify(pol) == FP_ZERO)
					{
						float vx, vy, vz;
						float mx, my, mz;

						if (useTinyFrames)
						{
							short *vert = tinyframe->vertices;
							vx = *(vert + (idx * 3)) * scale;
							vy = *(vert + (idx * 3) + 1) * scale;
							vz = *(vert + (idx * 3) + 2) * scale;
							FIXTRIANGLE(vx, vy, vz)
						}
						else
						{
							vx = frame->vertices[VERTEX_OFFSET] * scale;
							vy = frame->vertices[VERTEX_OFFSET+1] * scale;
							vz = frame->vertices[VERTEX_OFFSET+2] * scale;
							FIXTRIANGLE(vx, vy, vz)
						}

						// QUICK MATHS
						mx = (vx * cs) - (vy * sn);
						my = (vx * sn) + (vy * cs);
						mz = vz * (vflip ? -1 : 1);

						RSP_MakeVector4(triangle.vertices[j].position, (x + mx), (-z + mz), (-y + my));
					}
					else
					{
						float px1, py1, pz1;
						float px2, py2, pz2;
						float mx1, my1, mz1;
						float mx2, my2, mz2;
						float lx, ly, lz;

						// Interpolate
						if (useTinyFrames)
						{
							short *vert = tinyframe->vertices;
							short *nvert = tinynextframe->vertices;
							px1 = *(vert + (idx * 3)) * scale;
							py1 = *(vert + (idx * 3) + 1) * scale;
							pz1 = *(vert + (idx * 3) + 2) * scale;
							px2 = *(nvert + (idx * 3)) * scale;
							py2 = *(nvert + (idx * 3) + 1) * scale;
							pz2 = *(nvert + (idx * 3) + 2) * scale;
							FIXTRIANGLE(px1, py1, pz1)
							FIXTRIANGLE(px2, py2, pz2)
						}
						else
						{
							px1 = frame->vertices[VERTEX_OFFSET] * scale;
							py1 = frame->vertices[VERTEX_OFFSET+1] * scale;
							pz1 = frame->vertices[VERTEX_OFFSET+2] * scale;
							px2 = nextframe->vertices[VERTEX_OFFSET] * scale;
							py2 = nextframe->vertices[VERTEX_OFFSET+1] * scale;
							pz2 = nextframe->vertices[VERTEX_OFFSET+2] * scale;
							FIXTRIANGLE(px1, py1, pz1)
							FIXTRIANGLE(px2, py2, pz2)
						}

						// QUICK MATHS
						mx1 = (px1 * cs) - (py1 * sn);
						my1 = (px1 * sn) + (py1 * cs);
						mz1 = pz1 * (vflip ? -1 : 1);

						mx2 = (px2 * cs) - (py2 * sn);
						my2 = (px2 * sn) + (py2 * cs);
						mz2 = pz2 * (vflip ? -1 : 1);

						// interpolate
						lx = FloatLerp(mx1, mx2, pol);
						ly = FloatLerp(my1, my2, pol);
						lz = FloatLerp(mz1, mz2, pol);

						RSP_MakeVector4(triangle.vertices[j].position, (x + lx), (-z + lz), (-y + ly));
					}

					triangle.vertices[j].uv.u = s;
					triangle.vertices[j].uv.v = t;
				}

				RSP_TransformTriangle(&triangle);
			}

#ifdef RSP_DEBUGGING
			rsp_meshesdrawn++;
#endif
		}
	}

#undef FIXTRIANGLE
#undef RESETVIEW

	return true;
}

// Define for getting accurate color brightness readings according to how the human eye sees them.
// https://en.wikipedia.org/wiki/Relative_luminance
// 0.2126 to red
// 0.7152 to green
// 0.0722 to blue
#define SETBRIGHTNESS(brightness,r,g,b) \
	brightness = (UINT8)(((1063*(UINT16)(r))/5000) + ((3576*(UINT16)(g))/5000) + ((361*(UINT16)(b))/5000))

static colorlookup_t rsp_colorlookup;

static boolean BlendTranslations(UINT16 *px, RGBA_t *sourcepx, RGBA_t *blendpx, INT32 translation)
{
	if (translation == TC_BOSS)
	{
		// Turn everything below a certain threshold white
		if ((sourcepx->s.red == sourcepx->s.green) && (sourcepx->s.green == sourcepx->s.blue) && sourcepx->s.blue <= 82)
		{
			// Lactozilla: Invert the colors
			UINT8 invcol = (255 - sourcepx->s.blue);
			*px = GetColorLUT(&rsp_colorlookup, invcol, invcol, invcol);
		}
		else
			*px = GetColorLUT(&rsp_colorlookup, sourcepx->s.red, sourcepx->s.green, sourcepx->s.blue);
		*px |= 0xFF00;
		return true;
	}
	else if (translation == TC_DASHMODE && blendpx)
	{
		if (sourcepx->s.alpha == 0 && blendpx->s.alpha == 0)
		{
			// Don't bother with blending the pixel if the alpha of the blend pixel is 0
			*px = GetColorLUT(&rsp_colorlookup, sourcepx->s.red, sourcepx->s.green, sourcepx->s.blue);
		}
		else
		{
			UINT8 ialpha = 255 - blendpx->s.alpha, balpha = blendpx->s.alpha;
			RGBA_t icolor = *sourcepx, bcolor;

			memset(&bcolor, 0x00, sizeof(RGBA_t));

			if (blendpx->s.alpha)
			{
				bcolor.s.blue = 0;
				bcolor.s.red = 255;
				bcolor.s.green = (blendpx->s.red + blendpx->s.green + blendpx->s.blue) / 3;
			}
			if (sourcepx->s.alpha && sourcepx->s.red > sourcepx->s.green << 1) // this is pretty arbitrary, but it works well for Metal Sonic
			{
				icolor.s.red = sourcepx->s.blue;
				icolor.s.blue = sourcepx->s.red;
			}
			*px = GetColorLUT(&rsp_colorlookup,
				(ialpha * icolor.s.red + balpha * bcolor.s.red)/255,
				(ialpha * icolor.s.green + balpha * bcolor.s.green)/255,
				(ialpha * icolor.s.blue + balpha * bcolor.s.blue)/255
			);
		}
		*px |= 0xFF00;
		return true;
	}
	else if (translation == TC_ALLWHITE)
	{
		// Turn everything white
		*px = (0xFF00 | GetColorLUT(&rsp_colorlookup, 255, 255, 255));
		return true;
	}
	return false;
}

void RSP_CreateModelTexture(modelinfo_t *model, INT32 tcnum, skincolornum_t skincolor)
{
	modeltexturedata_t *texture = model->texture->base;
	modeltexturedata_t *blendtexture = model->texture->blend;
	rsp_texture_t *ttex;
	rsp_texture_t *ntex;
	size_t i, size = 0;

	UINT16 translation[16]; // First the color index
	UINT8 cutoff[16]; // Brightness cutoff before using the next color
	UINT8 translen = 0;

	// get texture size
	if (texture)
		size = (texture->width * texture->height);

	// has skincolor but no blend texture?
	// don't try to make translated textures
	if (skincolor && ((!blendtexture) || (blendtexture && !blendtexture->data)))
		skincolor = 0;

	ttex = &model->texture->rsp_blendtex[tcnum][skincolor];
	ntex = &model->texture->rsp_tex;

	InitColorLUT(&rsp_colorlookup, pMasterPalette, false);

	// base texture
	if (!tcnum)
	{
		RGBA_t *image = texture->data;
		// doesn't exist?
		if (!image)
			return;

		ntex->width = texture->width;
		ntex->height = texture->height;

		if (ntex->data)
			Z_Free(ntex->data);
		ntex->data = Z_Calloc(size * sizeof(UINT16), PU_SOFTPOLY, NULL);

		for (i = 0; i < size; i++)
			ntex->data[i] = ((image[i].s.alpha << 8) | GetColorLUT(&rsp_colorlookup, image[i].s.red, image[i].s.green, image[i].s.blue));
	}
	else
	{
		// create translations
		RGBA_t blendcolor;

		ttex->width = 1;
		ttex->height = 1;
		ttex->data = NULL;

		// doesn't exist?
		if (!texture->data)
			return;
		if (!blendtexture->data)
			return;

		ttex->width = texture->width;
		ttex->height = texture->height;
		ttex->data = Z_Calloc(size * sizeof(UINT16), PU_SOFTPOLY, NULL);

		blendcolor = V_GetColor(0); // initialize
		memset(translation, 0, sizeof(translation));
		memset(cutoff, 0, sizeof(cutoff));

		if (skincolor != SKINCOLOR_NONE && skincolor < numskincolors)
		{
			UINT8 numdupes = 1;

			translation[translen] = skincolors[skincolor].ramp[0];
			cutoff[translen] = 255;

			for (i = 1; i < 16; i++)
			{
				if (translation[translen] == skincolors[skincolor].ramp[i])
				{
					numdupes++;
					continue;
				}

				if (translen > 0)
				{
					cutoff[translen] = cutoff[translen-1] - (256 / (16 / numdupes));
				}

				numdupes = 1;
				translen++;

				translation[translen] = (UINT16)skincolors[skincolor].ramp[i];
			}

			translen++;
		}

		for (i = 0; i < size; i++)
		{
			RGBA_t *image = texture->data;
			RGBA_t *blendimage = blendtexture->data;

			if (image[i].s.alpha < 1)
				ttex->data[i] = 0x0000;
			else if (!BlendTranslations(&ttex->data[i], &image[i], &blendimage[i], -tcnum))
			{
				// All settings that use skincolors!
				UINT16 brightness;
				INT32 r = image[i].s.red, g = image[i].s.green, b = image[i].s.blue;
				INT32 tempcolor;

				if (translen <= 0)
					goto skippixel;

				// Don't bother with blending the pixel if the alpha of the blend pixel is 0
				if (blendimage[i].s.alpha == 0)
					goto skippixel; // for metal sonic blend
				else
				{
					SETBRIGHTNESS(brightness,blendimage[i].s.red,blendimage[i].s.green,blendimage[i].s.blue);
				}

				// Calculate a sort of "gradient" for the skincolor
				// (Me splitting this into a function didn't work, so I had to ruin this entire function's groove...)
				{
					RGBA_t nextcolor;
					UINT8 firsti, secondi, mul, mulmax, j;

					// Just convert brightness to a skincolor value, use distance to next position to find the gradient multipler
					firsti = 0;

					for (j = 1; j < translen; j++)
					{
						if (brightness >= cutoff[j])
							break;
						firsti = j;
					}

					secondi = firsti+1;

					mulmax = cutoff[firsti];
					if (secondi < translen)
						mulmax -= cutoff[secondi];

					mul = cutoff[firsti] - brightness;

					blendcolor = V_GetColor(translation[firsti]);

					if (secondi >= translen)
						mul = 0;

					if (mul > 0) // If it's 0, then we only need the first color.
					{
#if 0
						if (secondi >= translen)
						{
							// blend to black
							nextcolor = V_GetColor(31);
						}
						else
#endif
							nextcolor = V_GetColor(translation[secondi]);

						// Find difference between points
						r = (INT32)(nextcolor.s.red - blendcolor.s.red);
						g = (INT32)(nextcolor.s.green - blendcolor.s.green);
						b = (INT32)(nextcolor.s.blue - blendcolor.s.blue);

						// Find the gradient of the two points
						r = ((mul * r) / mulmax);
						g = ((mul * g) / mulmax);
						b = ((mul * b) / mulmax);

						// Add gradient value to color
						blendcolor.s.red += r;
						blendcolor.s.green += g;
						blendcolor.s.blue += b;
					}
				}

				// Color strength depends on image alpha
				tempcolor = ((image[i].s.red * (255-blendimage[i].s.alpha)) / 255) + ((blendcolor.s.red * blendimage[i].s.alpha) / 255);
				tempcolor = min(255, tempcolor);
				r = (UINT8)tempcolor;

				tempcolor = ((image[i].s.green * (255-blendimage[i].s.alpha)) / 255) + ((blendcolor.s.green * blendimage[i].s.alpha) / 255);
				tempcolor = min(255, tempcolor);
				g = (UINT8)tempcolor;

				tempcolor = ((image[i].s.blue * (255-blendimage[i].s.alpha)) / 255) + ((blendcolor.s.blue * blendimage[i].s.alpha) / 255);
				tempcolor = min(255, tempcolor);
				b = (UINT8)tempcolor;

skippixel:

				// *Now* we can do Metal Sonic's flashing
				if (-tcnum == TC_METALSONIC)
				{
					// Blend dark blue into white
					if (image[i].s.alpha > 0 && r == 0 && g == 0 && b < 255 && b > 31)
					{
						// Sal: Invert non-blue
						r = g = (255 - b);
						b = 255;
					}
				}

				ttex->data[i] = ((image[i].s.alpha << 8) | GetColorLUT(&rsp_colorlookup, r, g, b));
			}
		}
	}
}

#undef SETBRIGHTNESS

void RSP_FreeModelTexture(modelinfo_t *model)
{
	modeltexturedata_t *texture = model->texture->base;
	if (texture)
	{
		if (texture->data)
			Z_Free(texture->data);
		texture->data = NULL;
	}

	// Free polyrenderer memory.
	if (model->texture->rsp_tex.data)
		Z_Free(model->texture->rsp_tex.data);
	model->texture->rsp_tex.data = NULL;
	model->texture->rsp_tex.width = 1;
	model->texture->rsp_tex.height = 1;
}

void RSP_FreeModelBlendTexture(modelinfo_t *model)
{
	modeltexturedata_t *blendtexture = model->texture->blend;
	if (blendtexture)
	{
		if (blendtexture->data)
			Z_Free(blendtexture->data);
		blendtexture->data = NULL;
	}
}
