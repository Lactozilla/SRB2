// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2019-2021 by Jaime Ita Passos.
// Copyright (C) 2019 by "Arkus-Kotan".
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  swrast_model.c
/// \brief Model rendering

#include "swrast.h"

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

#define MD3_XYZ_SCALE   (1.0f / 64.0f)
#define NORMAL_SCALE    (1.0f / 256.0f)

#define VERTEX_OFFSET   ((i * 9) + (j * 3))		/* (i * 9) = (XYZ coords * vertex count) */
#define NORMAL_OFFSET   ((i * 9) + (j * 3))		/* (i * 9) = (XYZ normals * vertex count) */
#define UV_OFFSET       ((i * 6) + (j * 2))		/* (i * 6) = (UV coords * vertex count) */

static SWRast_Texture mSpriteTexture;
static lighttable_t *mSpriteTranslation;

static boolean mPaperSprite, mMirrored;
static boolean mHorizontalFlip, mVerticalFlip;

#ifdef ROTSPRITE
static angle_t mRollAngle;
static fpquaternion_t mRollQuaternion;
static fpvector4_t mRollTranslation;
#endif

static void SetModelStates(vissprite_t *spr)
{
	UINT8 *colormap = spr->colormap;

	if (spr->transmap && (SWRast_GetFragmentRead() & SWRAST_READ_COLOR))
	{
		SWRast_EnableBlending();
		SWRast_SetTranslucencyTable(spr->transmap);
	}
	else
		SWRast_DisableBlending();

	if (spr->extra_colormap)
	{
		if (!colormap)
			colormap = spr->extra_colormap->colormap;
		else
			colormap = &spr->extra_colormap->colormap[colormap - colormaps];
	}

	SWRast_SetColormap(colormap);
	SWRast_SetTranslation(mSpriteTranslation);

	if (mHorizontalFlip ^ mVerticalFlip)
		SWRast_SetCullMode(SWRAST_CULL_BACK);
	else
		SWRast_SetCullMode(SWRAST_CULL_FRONT);
}

static void SetTransform(mobj_t *mobj, modelinfo_t *md2, spriteframe_t *sprframe)
{
	static SWRast_Transform3D *mTransform = NULL;

	fixed_t x = mobj->x;
	fixed_t y = mobj->z;
	fixed_t z = mobj->y + FloatToFixed(md2->offset);
	angle_t ang;

	if (mTransform == NULL)
	{
		mTransform = Z_Malloc(sizeof(SWRast_Transform3D), PU_SWRASTERIZER, NULL);
		SWRast_InitTransform3D(mTransform);
	}

	// set model angle
	if ((sprframe->rotate == SRF_SINGLE) && (!mPaperSprite))
		ang = R_PointToAngle(mobj->x, mobj->y) - ANGLE_180;
	else if (mobj->player)
		ang = mobj->player->drawangle;
	else
		ang = mobj->angle;

	mTransform->translation.x = x;
	mTransform->translation.y = y;
	mTransform->translation.z = z;
	mTransform->rotation.y = ANGLE_180 - ang;

	if (mobj->eflags & MFE_VERTICALFLIP)
		mTransform->translation.y += mobj->height;

	SWRast_MakeWorldMatrix(mTransform, &SWRastState->modelMatrix);

	if (SWRastState->modelLighting)
	{
		y = mobj->height + (4<<FRACBITS);
		SWRast_SetLightPos(0, -y, 0);
	}
}

#ifdef ROTSPRITE
static void SetModelRotation(mobj_t *mobj, spriteinfo_t *sprinfo, spriteframe_t *sprframe)
{
	rotaxis_t rotaxis = ROTAXIS_Y;
	float xpivot, ypivot;
	float roll = FixedToFloat(AngleFixed(mRollAngle));
	INT32 flip = 1;

	angle_t ang = R_PointToAngle(mobj->x, mobj->y) - (mobj->player ? mobj->player->drawangle : mobj->angle);
	if ((sprframe->rotate & SRF_RIGHT) && (ang < ANGLE_180)) // See from right
		flip = 1;
	else if ((sprframe->rotate & SRF_LEFT) && (ang >= ANGLE_180)) // See from left
		flip = -1;

	roll *= flip;

	if (sprinfo->available)
		rotaxis = (UINT8)(sprinfo->pivot[(mobj->frame & FF_FRAMEMASK)].rotaxis);

	switch (rotaxis)
	{
		case ROTAXIS_Z: // Z
			FPQuaternionFromEuler(&mRollQuaternion, 0.0f, roll, 0.0f);
			break;
		case ROTAXIS_Y: // Y
			FPQuaternionFromEuler(&mRollQuaternion, 0.0f, 0.0f, roll);
			break;
		default: // X
			FPQuaternionFromEuler(&mRollQuaternion, roll, 0.0f, 0.0f);
			break;
	}

	xpivot = FixedToFloat(mobj->radius) / 2.0f;
	ypivot = FixedToFloat(mobj->height) / 2.0f;

	FPVector4(mRollTranslation, xpivot, 0.0f, ypivot);
}
#endif

static void TransformCoordinates(float *x, float *y, float *z)
{
	static fpquaternion_t *coordQuat = NULL;

	fpvector4_t mVecFP;
	FPVector4(mVecFP, *x, *y, *z);

	if (coordQuat == NULL)
	{
		coordQuat = Z_Malloc(sizeof(fpquaternion_t), PU_SWRASTERIZER, NULL);
		FPQuaternionFromEuler(coordQuat, 0.0f, 0.0f, -90.0f);
	}

	FPQuaternionRotateVector(&mVecFP, coordQuat);

#ifdef ROTSPRITE
	if (mRollAngle)
	{
		FPVectorAdd(&mVecFP, &mRollTranslation);
		FPQuaternionRotateVector(&mVecFP, &mRollQuaternion);
		FPVectorSubtract(&mVecFP, &mRollTranslation);
	}
#endif

	*x = -1.0f * mVecFP.x;
	*y = -1.0f * mVecFP.y;
	*z = -1.0f * mVecFP.z;
}

static boolean SetTextureToSprite(SWRast_Texture **texture, mobj_t *mobj, spriteframe_t *sprframe)
{
	// TODO: Remove this after merging 91ed56ef40e96629d65a28201ff9b6589e641f2a
	SWRast_SprTex *spriteTexture;
	angle_t ang = 0;
	unsigned rot;
	UINT8 flip;
	lumpcache_t *lumpcache;
	lumpnum_t lumpnum;
	UINT16 wad, lump;

	if (sprframe->rotate != SRF_SINGLE || mPaperSprite)
	{
		ang = R_PointToAngle (mobj->x, mobj->y) - (mobj->player ? mobj->player->drawangle : mobj->angle);
		if (mMirrored)
			ang = InvAngle(ang);
	}

	if (sprframe->rotate == SRF_SINGLE)
	{
		// use single rotation for all views
		rot = 0;                        //Fab: for vis->patch below
		lump = sprframe->lumpid[0];     //Fab: see note above
		flip = sprframe->flip; 			// Will only be 0 or 0xFFFF
	}
	else
	{
		// choose a different rotation based on player view
		if ((sprframe->rotate & SRF_RIGHT) && (ang < ANGLE_180)) // See from right
			rot = 6; // F7 slot
		else if ((sprframe->rotate & SRF_LEFT) && (ang >= ANGLE_180)) // See from left
			rot = 2; // F3 slot
		else if (sprframe->rotate & SRF_3DGE) // 16-angle mode
		{
			rot = (ang+ANGLE_180+ANGLE_11hh)>>28;
			rot = ((rot & 1)<<3)|(rot>>1);
		}
		else // Normal behaviour
			rot = (ang+ANGLE_202h)>>29;

		//Fab: lumpid is the index for spritewidth,spriteoffset... tables
		lump = sprframe->lumpid[rot];
		flip = sprframe->flip & (1<<rot);
	}

	flip = !flip != !mHorizontalFlip;

	lumpnum = sprframe->lumppat[rot];
	wad = WADFILENUM(lumpnum);
	lump = LUMPNUM(lumpnum);

	if (!wadfiles[wad]->patchcache)
		return false;

	lumpcache = wadfiles[wad]->patchcache->software;
	if (!lumpcache[lump])
		return false;

	spriteTexture = lumpcache[lump];
	spriteTexture += rot;
	if (!spriteTexture)
		return false;

	mSpriteTexture.width = spriteTexture->width;
	mSpriteTexture.height = spriteTexture->height;

	// make the patch
	if (!spriteTexture->data)
	{
		patch_t *source;

		if (!spriteTexture->lumpnum || spriteTexture->width < 1 || spriteTexture->height < 1)
			return false;

		source = (patch_t *)W_CacheLumpNum(spriteTexture->lumpnum, PU_CACHE);

		spriteTexture->data = Picture_Convert(PICFMT_PATCH, source, PICFMT_FLAT16, 0, NULL, spriteTexture->width, spriteTexture->height, 0, 0, (flip) ? PICFLAGS_XFLIP : 0);
		spriteTexture->lumpnum = 0;

		Z_ChangeTag(spriteTexture->data, PU_SWRASTERIZER);
	}

	mSpriteTexture.data = spriteTexture->data;
	*texture = &mSpriteTexture;

	return true;
}

static fixed_t CalcVertexLight(SWRast_Vertex *vertex)
{
	fixed_t dot = SWRast_DotProductVec3(&(vertex->normal), &(SWRastState->modelLightPos));
	return (FRACUNIT / 2) + SWRast_clamp(dot, -FRACUNIT, FRACUNIT) / 2;
}

static void ResetViewpoint(void)
{
	if (SWRast_GetMask())
		SWRast_ViewpointRestore();
}

boolean SWRast_RenderModel(vissprite_t *spr)
{
	SWRast_Texture *texture;
	INT32 durs, tics;
	float z1, z2;
	float finalscale;
	float pol = 0.0f;

	spritedef_t *sprdef;
	spriteframe_t *sprframe;
#ifdef ROTSPRITE
	spriteinfo_t *sprinfo;
#endif

	INT32 skinnum = 0;
	INT32 tc = 0, textc = 0;

	skincolornum_t skincolor = SKINCOLOR_NONE;

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
	if (SWRast_GetMask())
		SWRast_ViewpointRestoreFromSprite(spr);

	md2 = (modelinfo_t *)spr->model;
	if (!md2)
	{
		ResetViewpoint();
		return false;
	}

	// Lactozilla: Disallow certain models from rendering
	if (!Model_AllowRendering(mobj))
	{
		ResetViewpoint();
		return false;
	}

	// texture blending
	if (mobj->color)
		skincolor = (skincolornum_t)mobj->color;
	else if (mobj->sprite == SPR_PLAY) // Looks like a player, but doesn't have a color? Get rid of green sonic syndrome.
		skincolor = (skincolornum_t)skins[0].prefcolor;

	// load normal texture
	if (!md2->texture->swrastTexture.data)
	{
		if (mobj->skin)
			skinnum = (skin_t*)mobj->skin-skins;
		Model_LoadTexture(md2);
		Model_LoadBlendTexture(md2);
	}

	// set translation
	mSpriteTranslation = NULL;

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
		mSpriteTranslation = R_GetTranslationColormap(TC_RAINBOW, mobj->color, GTC_CACHE);
	else if (mobj->player && mobj->player->dashmode >= DASHMODE_THRESHOLD
		&& (mobj->player->charflags & SF_DASHMODE)
		&& ((leveltime/2) & 1))
	{
		if (mobj->player->charflags & SF_MACHINE)
			tc = TC_DASHMODE;
		else
			mSpriteTranslation = R_GetTranslationColormap(TC_RAINBOW, mobj->color, GTC_CACHE);
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

	if (textc && md2->texture->swrastBlendTexture[textc][skincolor].data == NULL)
		SWRast_CreateModelTexture(md2, textc, skincolor);

	// use corresponding texture for this model
	if (md2->texture->swrastBlendTexture[textc][skincolor].data != NULL)
		texture = &md2->texture->swrastBlendTexture[textc][skincolor];
	else
		texture = &md2->texture->swrastTexture;

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

	mPaperSprite = (mobj->frame & FF_PAPERSPRITE);
	mMirrored = mobj->mirrored;
	mVerticalFlip = (!(mobj->eflags & MFE_VERTICALFLIP) != !(mobj->frame & FF_VERTICALFLIP));
	mHorizontalFlip = (!(mobj->frame & FF_HORIZONTALFLIP) != !mMirrored);

	if (!texture->data)
	{
		if (!SetTextureToSprite(&texture, mobj, sprframe))
		{
			ResetViewpoint();
			return false;
		}

		// Set the translation here, since no blend texture was made
		if (tc == TC_DEFAULT && mobj->skin)
			tc = skinnum;

		mSpriteTranslation = R_GetTranslationColormap(tc, mobj->color, GTC_CACHE);
	}

	SWRast_BindTexture(texture);

	if (mobj->frame & FF_ANIMATE)
	{
		// set duration and tics to be the correct values for FF_ANIMATE states
		durs = mobj->state->var2;
		tics = mobj->anim_duration;
	}
	else
	{
		durs = mobj->state->tics;
		tics = mobj->tics;
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
	finalscale = md2->scale * FixedToFloat(mobj->scale);
	finalscale *= 0.5f;

#ifdef ROTSPRITE
	mRollAngle = mobj->rollangle;

	if (mRollAngle)
		SetModelRotation(mobj, sprinfo, sprframe);
#endif

	SetModelStates(spr);
	SetTransform(mobj, md2, sprframe);

	// Render every mesh
	for (meshnum = 0; meshnum < md2->model->numMeshes; meshnum++)
	{
		mesh_t *mesh = &md2->model->meshes[meshnum];
		SWRast_Triangle triangle;

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

		// render every triangle
		for (i = 0; i < mesh->numTriangles; i++)
		{
			float s, t;
			float *uv = mesh->uvs;

			for (j = 0; j < 3; j++)
			{
				float nx = 0.0f, ny = 0.0f, nz = 0.0f;

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

					if (useTinyFrames)
					{
						short *vert = tinyframe->vertices;
						char *norm = tinyframe->normals;

						vx = *(vert + (idx * 3)) * scale;
						vy = *(vert + (idx * 3) + 1) * scale;
						vz = *(vert + (idx * 3) + 2) * scale;

						TransformCoordinates(&vx, &vy, &vz);

						if (SWRastState->modelLighting)
						{
							nx = (float)(*(norm + (idx * 3))) * NORMAL_SCALE;
							ny = (float)(*(norm + (idx * 3) + 1)) * NORMAL_SCALE;
							nz = (float)(*(norm + (idx * 3) + 2)) * NORMAL_SCALE;
						}
					}
					else
					{
						vx = frame->vertices[VERTEX_OFFSET] * scale;
						vy = frame->vertices[VERTEX_OFFSET+1] * scale;
						vz = frame->vertices[VERTEX_OFFSET+2] * scale;

						TransformCoordinates(&vx, &vy, &vz);

						if (SWRastState->modelLighting)
						{
							nx = frame->normals[NORMAL_OFFSET];
							ny = frame->normals[NORMAL_OFFSET+1];
							nz = frame->normals[NORMAL_OFFSET+2];
						}
					}

					z1 = vz * (mVerticalFlip ? -1 : 1);

					triangle.vertices[j].position.x = FloatToFixed(vx);
					triangle.vertices[j].position.y = FloatToFixed(z1);
					triangle.vertices[j].position.z = FloatToFixed(vy);
				}
				else
				{
					// Interpolate
					float px1, py1, pz1;
					float px2, py2, pz2;
					float nx1 = 0.0f, ny1 = 0.0f, nz1 = 0.0f;
					float nx2 = 0.0f, ny2 = 0.0f, nz2 = 0.0f;
					float lx, ly, lz;

					if (useTinyFrames)
					{
						short *vert = tinyframe->vertices;
						short *nvert = tinynextframe->vertices;
						char *norm = tinyframe->normals;
						char *nnorm = tinynextframe->normals;

						px1 = *(vert + (idx * 3)) * scale;
						py1 = *(vert + (idx * 3) + 1) * scale;
						pz1 = *(vert + (idx * 3) + 2) * scale;
						px2 = *(nvert + (idx * 3)) * scale;
						py2 = *(nvert + (idx * 3) + 1) * scale;
						pz2 = *(nvert + (idx * 3) + 2) * scale;

						TransformCoordinates(&px1, &py1, &pz1);
						TransformCoordinates(&px2, &py2, &pz2);

						if (SWRastState->modelLighting)
						{
							nx1 = (float)(*(norm + (idx * 3))) * NORMAL_SCALE;
							ny1 = (float)(*(norm + (idx * 3) + 1)) * NORMAL_SCALE;
							nz1 = (float)(*(norm + (idx * 3) + 2)) * NORMAL_SCALE;
							nx2 = (float)(*(nnorm + (idx * 3))) * NORMAL_SCALE;
							ny2 = (float)(*(nnorm + (idx * 3) + 1)) * NORMAL_SCALE;
							nz2 = (float)(*(nnorm + (idx * 3) + 2)) * NORMAL_SCALE;
						}
					}
					else
					{
						px1 = frame->vertices[VERTEX_OFFSET] * scale;
						py1 = frame->vertices[VERTEX_OFFSET+1] * scale;
						pz1 = frame->vertices[VERTEX_OFFSET+2] * scale;
						px2 = nextframe->vertices[VERTEX_OFFSET] * scale;
						py2 = nextframe->vertices[VERTEX_OFFSET+1] * scale;
						pz2 = nextframe->vertices[VERTEX_OFFSET+2] * scale;

						TransformCoordinates(&px1, &py1, &pz1);
						TransformCoordinates(&px2, &py2, &pz2);

						if (SWRastState->modelLighting)
						{
							nx1 = frame->normals[NORMAL_OFFSET];
							ny1 = frame->normals[NORMAL_OFFSET+1];
							nz1 = frame->normals[NORMAL_OFFSET+2];
							nx2 = nextframe->normals[NORMAL_OFFSET];
							ny2 = nextframe->normals[NORMAL_OFFSET+1];
							nz2 = nextframe->normals[NORMAL_OFFSET+2];
						}
					}

					z1 = pz1 * (mVerticalFlip ? -1 : 1);
					z2 = pz2 * (mVerticalFlip ? -1 : 1);

					lx = FPInterpolate(px1, px2, pol);
					ly = FPInterpolate(py1, py2, pol);
					lz = FPInterpolate(z1, z2, pol);

					if (SWRastState->modelLighting)
					{
						nx = FPInterpolate(nx1, nx2, pol);
						ny = FPInterpolate(ny1, ny2, pol);
						nz = FPInterpolate(nz1, nz2, pol);
					}

					triangle.vertices[j].position.x = FloatToFixed(lx);
					triangle.vertices[j].position.y = FloatToFixed(lz);
					triangle.vertices[j].position.z = FloatToFixed(ly);
				}

				if (SWRastState->modelLighting)
				{
					triangle.vertices[j].normal.x = FloatToFixed(nx);
					triangle.vertices[j].normal.y = FloatToFixed(ny);
					triangle.vertices[j].normal.z = FloatToFixed(nz);
				}

				triangle.vertices[j].uv.u = FloatToFixed(s);
				triangle.vertices[j].uv.v = FloatToFixed(t);
			}

			// Compute light
			if (SWRastState->modelLighting)
			{
				fixed_t l0 = CalcVertexLight(&triangle.vertices[0]);
				fixed_t l1 = CalcVertexLight(&triangle.vertices[1]);
				fixed_t l2 = CalcVertexLight(&triangle.vertices[2]);
				SWRast_SetVertexLights(l0, l1, l2);
			}

			SWRast_RenderTriangle(&triangle);
		}

		SWRast_AddNumRenderedMeshes();
	}

	return true;
}

// Define for getting accurate color brightness readings according to how the human eye sees them.
// https://en.wikipedia.org/wiki/Relative_luminance
// 0.2126 to red
// 0.7152 to green
// 0.0722 to blue
#define SETBRIGHTNESS(brightness,r,g,b) \
	brightness = (UINT8)(((1063*(UINT16)(r))/5000) + ((3576*(UINT16)(g))/5000) + ((361*(UINT16)(b))/5000))

static colorlookup_t swrtex_colorlookup;

static boolean BlendTranslations(UINT16 *px, RGBA_t *sourcepx, RGBA_t *blendpx, INT32 translation)
{
	if (translation == TC_BOSS)
	{
		// Turn everything below a certain threshold white
		if ((sourcepx->s.red == sourcepx->s.green) && (sourcepx->s.green == sourcepx->s.blue) && sourcepx->s.blue <= 82)
		{
			// Lactozilla: Invert the colors
			UINT8 invcol = (255 - sourcepx->s.blue);
			*px = GetColorLUT(&swrtex_colorlookup, invcol, invcol, invcol);
		}
		else
			*px = GetColorLUT(&swrtex_colorlookup, sourcepx->s.red, sourcepx->s.green, sourcepx->s.blue);
		*px |= 0xFF00;
		return true;
	}
	else if (translation == TC_DASHMODE && blendpx)
	{
		if (sourcepx->s.alpha == 0 && blendpx->s.alpha == 0)
		{
			// Don't bother with blending the pixel if the alpha of the blend pixel is 0
			*px = GetColorLUT(&swrtex_colorlookup, sourcepx->s.red, sourcepx->s.green, sourcepx->s.blue);
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
			*px = GetColorLUT(&swrtex_colorlookup,
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
		*px = (0xFF00 | GetColorLUT(&swrtex_colorlookup, 255, 255, 255));
		return true;
	}
	return false;
}

void SWRast_CreateModelTexture(modelinfo_t *model, INT32 tcnum, skincolornum_t skincolor)
{
	modeltexturedata_t *texture = model->texture->base;
	modeltexturedata_t *blendtexture = model->texture->blend;
	SWRast_Texture *ttex, *ntex;
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

	ttex = &model->texture->swrastBlendTexture[tcnum][skincolor];
	ntex = &model->texture->swrastTexture;

	InitColorLUT(&swrtex_colorlookup, pMasterPalette, false);

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
		ntex->data = Z_Calloc(size * sizeof(UINT16), PU_SWRASTERIZER, NULL);

		for (i = 0; i < size; i++)
			ntex->data[i] = ((image[i].s.alpha << 8) | GetColorLUT(&swrtex_colorlookup, image[i].s.red, image[i].s.green, image[i].s.blue));
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
		ttex->data = Z_Calloc(size * sizeof(UINT16), PU_SWRASTERIZER, NULL);

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

				ttex->data[i] = ((image[i].s.alpha << 8) | GetColorLUT(&swrtex_colorlookup, r, g, b));
			}
		}
	}
}

#undef SETBRIGHTNESS

void SWRast_FreeModelTexture(modelinfo_t *model)
{
	modeltexturedata_t *texture = model->texture->base;
	if (texture)
	{
		if (texture->data)
			Z_Free(texture->data);
		texture->data = NULL;
	}

	if (model->texture->swrastTexture.data)
		Z_Free(model->texture->swrastTexture.data);
	model->texture->swrastTexture.data = NULL;
	model->texture->swrastTexture.width = 1;
	model->texture->swrastTexture.height = 1;
}

void SWRast_FreeModelBlendTexture(modelinfo_t *model)
{
	modeltexturedata_t *blendtexture = model->texture->blend;
	if (blendtexture)
	{
		if (blendtexture->data)
			Z_Free(blendtexture->data);
		blendtexture->data = NULL;
	}
}
