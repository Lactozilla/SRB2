/*
	From the 'Wizard2' engine by Spaddlewit Inc. ( http://www.spaddlewit.com )
	An experimental work-in-progress.

	Donated to Sonic Team Junior and adapted to work with
	Sonic Robo Blast 2. The license of this code matches whatever
	the licensing is for Sonic Robo Blast 2.
*/

#include "doomdef.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_main.h"
#include "info.h"
#include "i_video.h"
#include "m_argv.h"
#include "r_things.h"
#include "r_model.h"
#include "r_md2load.h"
#include "r_md3load.h"
#include "u_list.h"
#include "z_zone.h"
#include <string.h>

#ifndef errno
#include "errno.h"
#endif

#ifdef SWRASTERIZER
#include "swrasterizer/swrast.h"
#endif

#ifdef HWRENDER
#include "hardware/hw_glob.h"
#include "hardware/hw_drv.h"
#endif

char modelsfile[64];
char modelsfolder[64];

modelinfo_t md2_models[NUMSPRITES];
modelinfo_t md2_playermodels[MAXSKINS];

static CV_PossibleValue_t modelinterpolation_cons_t[] = {{0, "Off"}, {1, "Sometimes"}, {2, "Always"}, {0, NULL}};

static void CV_Models_OnChange(void);
static void CV_ModelsFile_OnChange(void);
static void CV_ModelsFolder_OnChange(void);

consvar_t cv_models = {"models", "Off", CV_CALL|CV_SAVE|CV_NOINIT, CV_OnOff, CV_Models_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_modelinterpolation = {"modelinterpolation", "Sometimes", CV_SAVE, modelinterpolation_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_modellighting = {"modellighting", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_modelsfile = {"modelsfile", "models.dat", CV_CALL|CV_SAVE, NULL, CV_ModelsFile_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_modelsfolder = {"modelsfolder", "models", CV_CALL|CV_SAVE, NULL, CV_ModelsFolder_OnChange, 0, NULL, NULL, 0, 0, NULL};

static void CV_Models_OnChange(void)
{
#ifdef SWRASTERIZER
	if (rendermode == render_soft)
		R_SetViewSize();
#endif
}

static void CV_ModelsFile_OnChange(void)
{
	// Write the new filename
	strncpy(modelsfile, cv_modelsfile.string, 64);
	FIL_ForceExtension(modelsfile, ".dat");

	// Reload every model
	Model_ReloadAll();
}

static void CV_ModelsFolder_OnChange(void)
{
	// Write the new folder name
	strncpy(modelsfolder, cv_modelsfolder.string, 64);

	// Reload every model
	Model_ReloadAll();
}

//
// Model_Init
//
void Model_Init(void)
{
	CV_RegisterVar(&cv_modelsfolder);
	CV_RegisterVar(&cv_modelsfile);
	CV_RegisterVar(&cv_modellighting);
	CV_RegisterVar(&cv_modelinterpolation);
	CV_RegisterVar(&cv_models);

	Model_SetupInfo();

	strncpy(modelsfile, MODELSFILE, 64);
	strncpy(modelsfolder, MODELSFOLDER, 64);

#ifdef SWRASTERIZER
	if (M_CheckParm("-noswmodels"))
	{
		swrasterizer = false;
		noswrasterizer = true;
	}
#endif
}

//
// Model_SetupInfo
//
void Model_SetupInfo(void)
{
	size_t i;
	INT32 s;

	for (s = 0; s < MAXSKINS; s++)
	{
		md2_playermodels[s].scale = -1.0f;
		md2_playermodels[s].model = NULL;
		md2_playermodels[s].texture = NULL;
		md2_playermodels[s].meshVBOs = false;
		md2_playermodels[s].skin = -1;
		md2_playermodels[s].notfound = true;
		md2_playermodels[s].error = false;
	}

	for (i = 0; i < NUMSPRITES; i++)
	{
		md2_models[i].scale = -1.0f;
		md2_models[i].model = NULL;
		md2_models[i].texture = NULL;
		md2_models[i].meshVBOs = false;
		md2_models[i].skin = -1;
		md2_models[i].notfound = true;
		md2_models[i].error = false;
	}
}

//
// Model_AddSkin
// Adds a model for a player.
//
void Model_AddSkin(int skin)
{
	FILE *f;
	char name[18], filename[32];
	float scale, offset;
	size_t prefixlen;

	// read the models.dat file
	//Filename checking fixed ~Monster Iestyn and Golden
	f = fopen(va("%s"PATHSEP"%s", srb2home, modelsfile), "rt");
	if (!f)
		return;

	// length of the player model prefix
	prefixlen = strlen(PLAYERMODELPREFIX);

	// Check for any models that match the names of player skins!
	while (fscanf(f, "%25s %31s %f %f", name, filename, &scale, &offset) == 4)
	{
		char *skinname = name;
		size_t len = strlen(name);

		// ignore the player model prefix.
		if (!strnicmp(name, PLAYERMODELPREFIX, prefixlen) && (len > prefixlen))
			skinname += prefixlen;

		if (stricmp(skinname, skins[skin].name) == 0)
		{
			md2_playermodels[skin].skin = skin;
			md2_playermodels[skin].scale = scale;
			md2_playermodels[skin].offset = offset;
			md2_playermodels[skin].notfound = false;
			strcpy(md2_playermodels[skin].filename, filename);
			goto playermodelfound;
		}
	}

	md2_playermodels[skin].notfound = true;
playermodelfound:
	fclose(f);
}

//
// Model_AddSprite
// Adds a model for a sprite.
//
void Model_AddSprite(size_t spritenum)
{
	FILE *f;
	// name[24] is used to check for names in the models.dat file that match with sprites or player skins
	// sprite names are always 4 characters long, and names is for player skins can be up to 19 characters long
	// PLAYERMODELPREFIX is 6 characters long
	char name[24], filename[32];
	float scale, offset;

	// Read the models.dat file
	//Filename checking fixed ~Monster Iestyn and Golden
	f = fopen(va("%s"PATHSEP"%s", srb2home, modelsfile), "rt");
	if (!f)
		return;

	// Check for any models that match the names of sprite names!
	while (fscanf(f, "%25s %31s %f %f", name, filename, &scale, &offset) == 4)
	{
		// length of the sprite name
		size_t len = strlen(name);
		if (len != 4) // must be 4 characters long exactly. otherwise it's not a sprite name.
			continue;

		// check for the player model prefix.
		if (!strnicmp(name, PLAYERMODELPREFIX, strlen(PLAYERMODELPREFIX)))
			continue; // that's not a sprite...

		if (stricmp(name, sprnames[spritenum]) == 0)
		{
			md2_models[spritenum].scale = scale;
			md2_models[spritenum].offset = offset;
			md2_models[spritenum].notfound = false;
			strcpy(md2_models[spritenum].filename, filename);
			goto spritemodelfound;
		}
	}

	md2_models[spritenum].notfound = true;
spritemodelfound:
	fclose(f);
}

//
// Model_UnloadInfo
// Unloads model info.
//
void Model_UnloadInfo(modelinfo_t *md2)
{
	Model_UnloadTextures(md2);
	if (md2->model)
		Model_Unload(md2->model);
	md2->meshVBOs = false;
}

//
// Model_UnloadAll
// Unload every sprite and skin model.
// Calls Model_UnloadInfo.
//
void Model_UnloadAll(void)
{
	size_t i;
	INT32 s;
	modelinfo_t *md2;

	for (s = 0; s < numskins; s++)
	{
		md2 = &md2_playermodels[s];
		Model_UnloadInfo(md2);
	}

	for (i = 0; i < NUMSPRITES; i++)
	{
		md2 = &md2_models[i];
		Model_UnloadInfo(md2);
	}
}

//
// Model_ReloadAll
// Reloads every sprite and skin model.
// Calls Model_UnloadAll first.
//
void Model_ReloadAll(void)
{
	size_t i;
	INT32 s;

	// Check if .dat file exists
	if (!FIL_FileExists(va("%s"PATHSEP"%s", srb2home, modelsfile)))
	{
		CONS_Alert(CONS_ERROR, M_GetText("Model info file \"%s\" doesn't exist\n"), modelsfile);
		return;
	}

	// Check if models folder exists
	if (!FIL_FileExists(va("%s"PATHSEP"%s", srb2home, modelsfolder)))
	{
		CONS_Alert(CONS_ERROR, M_GetText("Model folder \"%s\" doesn't exist\n"), modelsfolder);
		return;
	}

	Model_UnloadAll();
	Model_SetupInfo();

	for (s = 0; s < numskins; s++)
		Model_AddSkin(s);

	for (i = 0; i < NUMSPRITES; i++)
		Model_AddSprite(i);
}

//
// Model_ReloadSettings
// Reloads model settings.
//
void Model_ReloadSettings(void)
{
	size_t i;
	INT32 s;

	for (s = 0; s < MAXSKINS; s++)
	{
		if (md2_playermodels[s].model)
			Model_LoadSprite2(md2_playermodels[s].model);
	}

	for (i = 0; i < NUMSPRITES; i++)
	{
		if (md2_models[i].model)
			Model_LoadInterpolationSettings(md2_models[i].model);
	}
}

//
// Model_LoadFile
// Loads a model from a file.
//
model_t *Model_LoadFile(const char *filename)
{
	//Filename checking fixed ~Monster Iestyn and Golden
	return Model_ReadFile(va("%s"PATHSEP"%s", srb2home, filename), PU_MODEL);
}

//
// Model_ReadFile
// Loads a model from a file and converts it to the internal format.
//
model_t *Model_ReadFile(const char *filename, int ztag)
{
	model_t *model;

	// What type of file?
	const char *extension = NULL;
	int i;
	for (i = (int)strlen(filename)-1; i >= 0; i--)
	{
		if (filename[i] != '.')
			continue;

		extension = &filename[i];
		break;
	}

	if (!extension)
	{
		CONS_Alert(CONS_ERROR, "Model \"%s\" is lacking a file extension, unable to determine type!\n", filename);
		return NULL;
	}

	if (!strcmp(extension, ".md3"))
	{
		if (!(model = MD3_LoadModel(filename, ztag, false)))
			return NULL;
	}
	else if (!strcmp(extension, ".md3s")) // MD3 that will be converted in memory to use full floats
	{
		if (!(model = MD3_LoadModel(filename, ztag, true)))
			return NULL;
	}
	else if (!strcmp(extension, ".md2"))
	{
		if (!(model = MD2_LoadModel(filename, ztag, false)))
			return NULL;
	}
	else if (!strcmp(extension, ".md2s"))
	{
		if (!(model = MD2_LoadModel(filename, ztag, true)))
			return NULL;
	}
	else
	{
		CONS_Alert(CONS_ERROR, "Unknown model format \"%s\"\n", extension);
		return NULL;
	}

	Model_Optimize(model);
	Model_GeneratePolygonNormals(model, ztag);
	Model_LoadSprite2(model);
	if (!model->spr2frames)
		Model_LoadInterpolationSettings(model);

	// Default material properties
	for (i = 0 ; i < model->numMaterials; i++)
	{
		material_t *material = &model->materials[i];
		material->ambient[0] = 0.7686f;
		material->ambient[1] = 0.7686f;
		material->ambient[2] = 0.7686f;
		material->ambient[3] = 1.0f;
		material->diffuse[0] = 0.5863f;
		material->diffuse[1] = 0.5863f;
		material->diffuse[2] = 0.5863f;
		material->diffuse[3] = 1.0f;
		material->specular[0] = 0.4902f;
		material->specular[1] = 0.4902f;
		material->specular[2] = 0.4902f;
		material->specular[3] = 1.0f;
		material->shininess = 25.0f;
	}

	// Set originaluvs to point to uvs
	for (i = 0; i < model->numMeshes; i++)
		model->meshes[i].originaluvs = model->meshes[i].uvs;

	model->max_s = 1.0;
	model->max_t = 1.0;
	model->vbo_max_s = 1.0;
	model->vbo_max_t = 1.0;

	return model;
}

//
// Model_Unload
// Wouldn't it be great if C just had destructors?
//
void Model_Unload(model_t *model)
{
	int i;

	if (model == NULL)
		return;

	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (mesh->frames)
		{
			int j;
			for (j = 0; j < mesh->numFrames; j++)
			{
				if (mesh->frames[j].normals)
					Z_Free(mesh->frames[j].normals);

				if (mesh->frames[j].tangents)
					Z_Free(mesh->frames[j].tangents);

				if (mesh->frames[j].vertices)
					Z_Free(mesh->frames[j].vertices);

				if (mesh->frames[j].colors)
					Z_Free(mesh->frames[j].colors);
			}

			Z_Free(mesh->frames);
		}
		else if (mesh->tinyframes)
		{
			int j;
			for (j = 0; j < mesh->numFrames; j++)
			{
				if (mesh->tinyframes[j].normals)
					Z_Free(mesh->tinyframes[j].normals);

				if (mesh->tinyframes[j].tangents)
					Z_Free(mesh->tinyframes[j].tangents);

				if (mesh->tinyframes[j].vertices)
					Z_Free(mesh->tinyframes[j].vertices);
			}

			if (mesh->indices)
				Z_Free(mesh->indices);

			Z_Free(mesh->tinyframes);
		}

		if (mesh->uvs)
			Z_Free(mesh->uvs);

		if (mesh->lightuvs)
			Z_Free(mesh->lightuvs);
	}

	if (model->meshes)
		Z_Free(model->meshes);

	if (model->tags)
		Z_Free(model->tags);

	if (model->materials)
		Z_Free(model->materials);

	Model_DeleteVBOs(model);
	Z_Free(model);
}

//
// Model_UnloadTextures
//
void Model_UnloadTextures(modelinfo_t *model)
{
	if (model->texture)
	{
#ifdef HWRENDER
		GLPatch_t *grpatch = NULL;

		// Macro for freeing base and blend mipmaps
		#define FREETEX(tex) \
		{ \
			if (model->texture->tex) \
			{ \
				grpatch = model->texture->tex; \
				if (grpatch) \
				{ \
					Z_Free(grpatch->mipmap->data); \
					if (grpatch->mipmap) \
						Z_Free(grpatch->mipmap); \
					Z_Free(grpatch); \
				} \
			} \
		}

		FREETEX(grpatch)
		FREETEX(blendgrpatch)

		#undef FREETEX
#endif

#ifdef SWRASTERIZER
		SWRast_FreeModelTexture(model);
		SWRast_FreeModelBlendTexture(model);
#endif

		if (model->texture->base)
			Z_Free(model->texture->base);
		if (model->texture->blend)
			Z_Free(model->texture->blend);
		Z_Free(model->texture);
	}
}

// Returns a model, if available.
modelinfo_t *Model_IsAvailable(spritenum_t spritenum, skin_t *skin)
{
	char filename[64];
	modelinfo_t *md2;

	// invalid sprite number
	if ((unsigned)spritenum >= NUMSPRITES || (unsigned)spritenum == SPR_NULL)
		return NULL;

	if (skin && spritenum == SPR_PLAY) // Use the player model list if the mobj has a skin and is using the player sprites
	{
		md2 = &md2_playermodels[skin-skins];
		md2->skin = skin-skins;
	}
	else
		md2 = &md2_models[spritenum];

	if (md2->notfound)
		return NULL;

	if (!md2->model)
	{
		sprintf(filename, "%s/%s", modelsfolder, md2->filename);
		md2->model = Model_LoadFile(filename);

		if (!md2->model)
		{
			md2->notfound = true;
			return NULL;
		}
	}

	// Allocate texture data
	if (!md2->texture)
		md2->texture = Z_Calloc(sizeof(modeltexture_t), PU_MODEL, NULL);

	return md2;
}

// Adjust texture coords of model to fit into a patch's max_s and max_t
void Model_AdjustTextureCoords(model_t *model, float max_s, float max_t)
{
	int i;
	for (i = 0; i < model->numMeshes; i++)
	{
		int j;
		mesh_t *mesh = &model->meshes[i];
		int numVertices;
		float *uvReadPtr = mesh->originaluvs;
		float *uvWritePtr;

		// i dont know if this is actually possible, just logical conclusion of structure in CreateModelVBOs
		if (!mesh->frames && !mesh->tinyframes) continue;

		if (mesh->frames) // again CreateModelVBO and CreateModelVBOTiny iterate like this so I'm gonna do that too
			numVertices = mesh->numTriangles * 3;
		else
			numVertices = mesh->numVertices;

		// if originaluvs points to uvs, we need to allocate new memory for adjusted uvs
		// the old uvs are kept around for use in possible readjustments
		if (mesh->uvs == mesh->originaluvs)
			mesh->uvs = Z_Malloc(numVertices * 2 * sizeof(float), PU_STATIC, NULL);

		uvWritePtr = mesh->uvs;

		// fix uvs (texture coordinates) to take into account that the actual texture
		// has empty space added until the next power of two
		for (j = 0; j < numVertices; j++)
		{
			*uvWritePtr++ = *uvReadPtr++ * max_s;
			*uvWritePtr++ = *uvReadPtr++ * max_t;
		}
	}
	// Save the values we adjusted the uvs for
	model->max_s = max_s;
	model->max_t = max_t;
}

boolean Model_AllowRendering(mobj_t *mobj)
{
	// Signpost overlay. Not needed.
	if (mobj->state-states == S_PLAY_SIGN)
		return false;

	// Otherwise, render the model.
	return true;
}

boolean Model_CanInterpolate(mobj_t *mobj, model_t *model)
{
	if (cv_modelinterpolation.value == 2) // Always interpolate
		return true;
	return model->interpolate[(mobj->frame & FF_FRAMEMASK)];
}

boolean Model_CanInterpolateSprite2(modelspr2frames_t *spr2frame)
{
	if (cv_modelinterpolation.value == 2) // Always interpolate
		return true;
	return spr2frame->interpolate;
}

//
// Model_GetSprite2 (see P_GetSkinSprite2)
// For non-super players, tries each sprite2's immediate predecessor until it finds one with a number of frames or ends up at standing.
// For super players, does the same as above - but tries the super equivalent for each sprite2 before the non-super version.
//
UINT8 Model_GetSprite2(modelinfo_t *md2, skin_t *skin, UINT8 spr2, player_t *player)
{
	UINT8 super = 0, i = 0;

	if (!md2 || !md2->model || !md2->model->spr2frames || !skin)
		return 0;

	if ((playersprite_t)(spr2 & ~FF_SPR2SUPER) >= free_spr2)
		return 0;

	while (!md2->model->spr2frames[spr2].numframes
		&& spr2 != SPR2_STND
		&& ++i != 32) // recursion limiter
	{
		if (spr2 & FF_SPR2SUPER)
		{
			super = FF_SPR2SUPER;
			spr2 &= ~FF_SPR2SUPER;
			continue;
		}

		switch(spr2)
		{
		// Normal special cases.
		case SPR2_JUMP:
			spr2 = ((player
					? player->charflags
					: skin->flags)
					& SF_NOJUMPSPIN) ? SPR2_SPNG : SPR2_ROLL;
			break;
		case SPR2_TIRE:
			spr2 = ((player
					? player->charability
					: skin->ability)
					== CA_SWIM) ? SPR2_SWIM : SPR2_FLY;
			break;
		// Use the handy list, that's what it's there for!
		default:
			spr2 = spr2defaults[spr2];
			break;
		}

		spr2 |= super;
	}

	if (i >= 32) // probably an infinite loop...
		return 0;

	return spr2;
}

tag_t *Model_GetTagByName(model_t *model, char *name, int frame)
{
	if (frame < model->maxNumFrames)
	{
		tag_t *iterator = &model->tags[frame * model->numTags];

		int i;
		for (i = 0; i < model->numTags; i++)
		{
			if (!stricmp(iterator[i].name, name))
				return &iterator[i];
		}
	}

	return NULL;
}

void Model_LoadInterpolationSettings(model_t *model)
{
	INT32 i;
	INT32 numframes = model->meshes[0].numFrames;
	char *framename = model->framenames;

	if (!framename)
		return;

	#define GET_OFFSET \
		memcpy(&interpolation_flag, framename + offset, 2); \
		model->interpolate[i] = (!memcmp(interpolation_flag, MODEL_INTERPOLATION_FLAG, 2));

	for (i = 0; i < numframes; i++)
	{
		int offset = (strlen(framename) - 4);
		char interpolation_flag[3];
		memset(&interpolation_flag, 0x00, 3);

		// find the +i on the frame name
		// ANIM+i00
		// so the offset is (frame name length - 4)
		GET_OFFSET;

		// maybe the frame had three digits?
		// ANIM+i000
		// so the offset is (frame name length - 5)
		if (!model->interpolate[i])
		{
			offset--;
			GET_OFFSET;
		}

		framename += 16;
	}

	#undef GET_OFFSET
}

void Model_LoadSprite2(model_t *model)
{
	INT32 i;
	modelspr2frames_t *spr2frames = NULL;
	INT32 numframes = model->meshes[0].numFrames;
	char *framename = model->framenames;

	if (!framename)
		return;

	for (i = 0; i < numframes; i++)
	{
		char prefix[6];
		char name[5];
		char interpolation_flag[3];
		char framechars[4];
		UINT8 frame = 0;
		UINT8 spr2idx;
		boolean interpolate = false;

		memset(&prefix, 0x00, 6);
		memset(&name, 0x00, 5);
		memset(&interpolation_flag, 0x00, 3);
		memset(&framechars, 0x00, 4);

		if (strlen(framename) >= 9)
		{
			boolean super;
			char *modelframename = framename;
			memcpy(&prefix, modelframename, 5);
			modelframename += 5;
			memcpy(&name, modelframename, 4);
			modelframename += 4;
			// Oh look
			memcpy(&interpolation_flag, modelframename, 2);
			if (!memcmp(interpolation_flag, MODEL_INTERPOLATION_FLAG, 2))
			{
				interpolate = true;
				modelframename += 2;
			}
			memcpy(&framechars, modelframename, 3);

			if ((super = (!memcmp(prefix, "SUPER", 5))) || (!memcmp(prefix, "SPR2_", 5)))
			{
				spr2idx = 0;
				while (spr2idx < free_spr2)
				{
					if (!memcmp(spr2names[spr2idx], name, 4))
					{
						if (!spr2frames)
							spr2frames = (modelspr2frames_t*)Z_Calloc(sizeof(modelspr2frames_t)*NUMPLAYERSPRITES*2, PU_MODEL, NULL);
						if (super)
							spr2idx |= FF_SPR2SUPER;
						if (framechars[0])
						{
							frame = atoi(framechars);
							if (spr2frames[spr2idx].numframes < frame+1)
								spr2frames[spr2idx].numframes = frame+1;
						}
						else
						{
							frame = spr2frames[spr2idx].numframes;
							spr2frames[spr2idx].numframes++;
						}
						spr2frames[spr2idx].frames[frame] = i;
						spr2frames[spr2idx].interpolate = interpolate;
						break;
					}
					spr2idx++;
				}
			}
		}

		framename += 16;
	}

	if (model->spr2frames)
		Z_Free(model->spr2frames);
	model->spr2frames = spr2frames;
}

//
// GenerateVertexNormals
//
// Creates a new normal for a vertex using the average of all of the polygons it belongs to.
//
void Model_GenerateVertexNormals(model_t *model)
{
	int i;
	for (i = 0; i < model->numMeshes; i++)
	{
		int j;

		mesh_t *mesh = &model->meshes[i];

		if (!mesh->frames)
			continue;

		for (j = 0; j < mesh->numFrames; j++)
		{
			mdlframe_t *frame = &mesh->frames[j];
			int memTag = PU_MODEL;
			float *newNormals = (float*)Z_Malloc(sizeof(float)*3*mesh->numTriangles*3, memTag, 0);
			int k;
			float *vertPtr = frame->vertices;
			float *oldNormals;

			M_Memcpy(newNormals, frame->normals, sizeof(float)*3*mesh->numTriangles*3);

/*			if (!systemSucks)
			{
				memTag = Z_GetTag(frame->tangents);
				float *newTangents = (float*)Z_Malloc(sizeof(float)*3*mesh->numTriangles*3, memTag);
				M_Memcpy(newTangents, frame->tangents, sizeof(float)*3*mesh->numTriangles*3);
			}*/

			for (k = 0; k < mesh->numVertices; k++)
			{
				float x, y, z;
				int vCount = 0;
				vector_t normal;
				int l;
				float *testPtr = frame->vertices;

				x = *vertPtr++;
				y = *vertPtr++;
				z = *vertPtr++;

				normal.x = normal.y = normal.z = 0;

				for (l = 0; l < mesh->numVertices; l++)
				{
					float testX, testY, testZ;
					testX = *testPtr++;
					testY = *testPtr++;
					testZ = *testPtr++;

					if (fabsf(x - testX) > FLT_EPSILON
						|| fabsf(y - testY) > FLT_EPSILON
						|| fabsf(z - testZ) > FLT_EPSILON)
						continue;

					// Found a vertex match! Add it...
					normal.x += frame->normals[3 * l + 0];
					normal.y += frame->normals[3 * l + 1];
					normal.z += frame->normals[3 * l + 2];
					vCount++;
				}

				if (vCount > 1)
				{
//					Vector::Normalize(&normal);
					newNormals[3 * k + 0] = (float)normal.x;
					newNormals[3 * k + 1] = (float)normal.y;
					newNormals[3 * k + 2] = (float)normal.z;

/*					if (!systemSucks)
					{
						Vector::vector_t tangent;
						Vector::Tangent(&normal, &tangent);
						newTangents[3 * k + 0] = tangent.x;
						newTangents[3 * k + 1] = tangent.y;
						newTangents[3 * k + 2] = tangent.z;
					}*/
				}
			}

			oldNormals = frame->normals;
			frame->normals = newNormals;
			Z_Free(oldNormals);

/*			if (!systemSucks)
			{
				float *oldTangents = frame->tangents;
				frame->tangents = newTangents;
				Z_Free(oldTangents);
			}*/
		}
	}
}

typedef struct materiallist_s
{
	struct materiallist_s *next;
	struct materiallist_s *prev;
	material_t *material;
} materiallist_t;

static boolean AddMaterialToList(materiallist_t **head, material_t *material)
{
	materiallist_t *node, *newMatNode;
	for (node = *head; node; node = node->next)
	{
		if (node->material == material)
			return false;
	}

	// Didn't find it, so add to the list
	newMatNode = (materiallist_t*)Z_Malloc(sizeof(materiallist_t), PU_CACHE, 0);
	newMatNode->material = material;
	ListAdd(newMatNode, (listitem_t**)head);
	return true;
}

//
// Model_Optimize
//
// Groups triangles from meshes in the model
// Only works for models with 1 frame
//
void Model_Optimize(model_t *model)
{
	int numMeshes = 0;
	int i;
	materiallist_t *matListHead = NULL;
	int memTag;
	mesh_t *newMeshes;
	materiallist_t *node;

	if (model->numMeshes <= 1)
		return; // No need

	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *curMesh = &model->meshes[i];

		if (curMesh->numFrames > 1)
			return; // Can't optimize models with > 1 frame

		if (!curMesh->frames)
			return; // Don't optimize tinyframe models (no need)

		// We are condensing to 1 mesh per material, so
		// the # of materials we use will be the new
		// # of meshes
		if (AddMaterialToList(&matListHead, curMesh->frames[0].material))
			numMeshes++;
	}

	memTag = PU_MODEL;
	newMeshes = (mesh_t*)Z_Calloc(sizeof(mesh_t) * numMeshes, memTag, 0);

	i = 0;
	for (node = matListHead; node; node = node->next)
	{
		material_t *curMat = node->material;
		mesh_t *newMesh = &newMeshes[i];
		mdlframe_t *curFrame;
		int uvCount;
		int vertCount;
		int colorCount;

		// Find all triangles with this material and count them
		int numTriangles = 0;
		int j;
		for (j = 0; j < model->numMeshes; j++)
		{
			mesh_t *curMesh = &model->meshes[j];

			if (curMesh->frames[0].material == curMat)
				numTriangles += curMesh->numTriangles;
		}

		newMesh->numFrames = 1;
		newMesh->numTriangles = numTriangles;
		newMesh->numVertices = numTriangles * 3;
		newMesh->uvs = (float*)Z_Malloc(sizeof(float)*2*numTriangles*3, memTag, 0);
//		if (node->material->lightmap)
//			newMesh->lightuvs = (float*)Z_Malloc(sizeof(float)*2*numTriangles*3, memTag, 0);
		newMesh->frames = (mdlframe_t*)Z_Calloc(sizeof(mdlframe_t), memTag, 0);
		curFrame = &newMesh->frames[0];

		curFrame->material = curMat;
		curFrame->normals = (float*)Z_Malloc(sizeof(float)*3*numTriangles*3, memTag, 0);
//		if (!systemSucks)
//			curFrame->tangents = (float*)Z_Malloc(sizeof(float)*3*numTriangles*3, memTag, 0);
		curFrame->vertices = (float*)Z_Malloc(sizeof(float)*3*numTriangles*3, memTag, 0);
		curFrame->colors = (char*)Z_Malloc(sizeof(char)*4*numTriangles*3, memTag, 0);

		// Now traverse the meshes of the model, adding in
		// vertices/normals/uvs that match the current material
		uvCount = 0;
		vertCount = 0;
		colorCount = 0;
		for (j = 0; j < model->numMeshes; j++)
		{
			mesh_t *curMesh = &model->meshes[j];

			if (curMesh->frames[0].material == curMat)
			{
				float *dest;
				float *src;
				char *destByte;
				char *srcByte;

				M_Memcpy(&newMesh->uvs[uvCount],
					curMesh->uvs,
					sizeof(float)*2*curMesh->numTriangles*3);

/*				if (node->material->lightmap)
				{
					M_Memcpy(&newMesh->lightuvs[uvCount],
						curMesh->lightuvs,
						sizeof(float)*2*curMesh->numTriangles*3);
				}*/
				uvCount += 2*curMesh->numTriangles*3;

				dest = (float*)newMesh->frames[0].vertices;
				src = (float*)curMesh->frames[0].vertices;
				M_Memcpy(&dest[vertCount],
					src,
					sizeof(float)*3*curMesh->numTriangles*3);

				dest = (float*)newMesh->frames[0].normals;
				src = (float*)curMesh->frames[0].normals;
				M_Memcpy(&dest[vertCount],
					src,
					sizeof(float)*3*curMesh->numTriangles*3);

/*				if (!systemSucks)
				{
					dest = (float*)newMesh->frames[0].tangents;
					src = (float*)curMesh->frames[0].tangents;
					M_Memcpy(&dest[vertCount],
						src,
						sizeof(float)*3*curMesh->numTriangles*3);
				}*/

				vertCount += 3 * curMesh->numTriangles * 3;

				destByte = (char*)newMesh->frames[0].colors;
				srcByte = (char*)curMesh->frames[0].colors;

				if (srcByte)
				{
					M_Memcpy(&destByte[colorCount],
						srcByte,
						sizeof(char)*4*curMesh->numTriangles*3);
				}
				else
				{
					memset(&destByte[colorCount],
						255,
						sizeof(char)*4*curMesh->numTriangles*3);
				}

				colorCount += 4 * curMesh->numTriangles * 3;
			}
		}

		i++;
	}

	//CONS_Printf("Model::Optimize(): Model reduced from %d to %d meshes.\n", model->numMeshes, numMeshes);
	model->meshes = newMeshes;
	model->numMeshes = numMeshes;
}

void Model_GeneratePolygonNormals(model_t *model, int ztag)
{
	int i;
	for (i = 0; i < model->numMeshes; i++)
	{
		int j;
		mesh_t *mesh = &model->meshes[i];

		if (!mesh->frames)
			continue;

		for (j = 0; j < mesh->numFrames; j++)
		{
			int k;
			mdlframe_t *frame = &mesh->frames[j];
			const float *vertices = frame->vertices;
			vector_t *polyNormals;

			frame->polyNormals = (vector_t*)Z_Malloc(sizeof(vector_t) * mesh->numTriangles, ztag, 0);

			polyNormals = frame->polyNormals;

			for (k = 0; k < mesh->numTriangles; k++)
			{
//				Vector::Normal(vertices, polyNormals);
				vertices += 3 * 3;
				polyNormals++;
			}
		}
	}
}

void Model_DeleteVBOs(model_t *model)
{
#ifdef HWRENDER
	// This just means VBOs get lost in GPU memory
	// if you happen to not be in OpenGL. Oh well.
	if (rendermode == render_opengl)
		HWD.pfnDeleteModelVBOs(model);
#else
	(void)model;
#endif
}
