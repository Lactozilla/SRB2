/*
	From the 'Wizard2' engine by Spaddlewit Inc. ( http://www.spaddlewit.com )
	An experimental work-in-progress.

	Donated to Sonic Team Junior and adapted to work with
	Sonic Robo Blast 2. The license of this code matches whatever
	the licensing is for Sonic Robo Blast 2.
*/

#ifndef _R_MODEL_H_
#define _R_MODEL_H_

#include "doomdata.h"
#include "doomtype.h"
#include "doomstat.h"
#include "r_things.h"

extern char modelsfile[64];
extern char modelsfolder[64];

extern consvar_t cv_models;
extern consvar_t cv_modelinterpolation;
extern consvar_t cv_modelsfile;
extern consvar_t cv_modelsfolder;

#define USE_MODEL_NEXTFRAME
#define MODEL_INTERPOLATION_FLAG "+i"
#define MODEL_INTERPOLATION_LIMIT TICRATE/4

#define MODELSFOLDER "models"
#define MODELSFILE "models.dat"

typedef struct
{
	float x, y, z;
} vector_t;

typedef struct
{
	float ambient[4], diffuse[4], specular[4], emissive[4];
	float shininess;
	boolean spheremap;
//	Texture::texture_t *texture;
//	Texture::texture_t *lightmap;
} material_t;

typedef struct
{
	material_t *material; // Pointer to the allocated 'materials' list in model_t
	float *vertices;
	float *normals;
	float *tangents;
	char *colors;
	unsigned int vboID;
	vector_t *polyNormals;
} mdlframe_t;

typedef struct
{
	material_t *material;
	short *vertices;
	char *normals;
	char *tangents;
	unsigned int vboID;
} tinyframe_t;

// Equivalent to MD3's many 'surfaces'
typedef struct mesh_s
{
	int numVertices;
	int numTriangles;

	float *uvs;
	// if uv adjustment is needed, uvs is changed to point to adjusted ones and
	// this one retains the originals
	// note: this member has been added with the assumption that models are never freed.
	// (UnloadModel is called by nobody at the time of writing.)
	float *originaluvs;
	float *lightuvs;

	int numFrames;
	mdlframe_t *frames;
	tinyframe_t *tinyframes;
	unsigned short *indices;
} mesh_t;

typedef struct tag_s
{
	char name[64];
//	matrix_t transform;
} tag_t;

typedef struct
{
	INT32 frames[256];
	UINT8 numframes;
	boolean interpolate;
} modelspr2frames_t;

typedef struct model_s
{
	int maxNumFrames;

	int numMaterials;
	material_t *materials;
	int numMeshes;
	mesh_t *meshes;
	int numTags;
	tag_t *tags;

	char *framenames;
	boolean interpolate[256];
	modelspr2frames_t *spr2frames;

	// the max_s and max_t values that the uvs are currently adjusted to
	// (if a sprite is used as a texture)
	float max_s;
	float max_t;
	// These are the values that the uvs in the VBO have been adjusted to.
	// If they are not same as max_s and max_t, then the VBO won't be used.
	float vbo_max_s;
	float vbo_max_t;
} model_t;

typedef struct
{
	INT16 width;
	INT16 height;
	size_t size;
	void *data;
	boolean found;
} modeltexturedata_t;

typedef struct
{
	modeltexturedata_t *base;
	modeltexturedata_t *blend;
#ifdef HWRENDER
	void               *grpatch;
	void               *blendgrpatch;
#endif
#ifdef POLYRENDERER
	rsp_texture_t      rsp_tex;
	rsp_texture_t      rsp_blendtex[8][MAXSKINCOLORS];
#endif
} modeltexture_t;

typedef struct
{
	char            filename[32];
	float           scale;
	float           offset;
	model_t         *model;
	modeltexture_t  *texture;
	boolean         meshVBOs;
	boolean         notfound;
	INT32           skin;
	boolean         error;
} modelinfo_t;

extern modelinfo_t md2_models[NUMSPRITES];
extern modelinfo_t md2_playermodels[MAXSKINS];

// Model info initialization
void Model_Init(void);
void Model_SetupInfo(void);
void Model_ReloadSettings(void);

void Model_AddSkin(INT32 skin);
void Model_AddSprite(size_t spritenum);

void Model_UnloadInfo(modelinfo_t *model);
void Model_UnloadTextures(modelinfo_t *model);

void Model_UnloadAll(void);
void Model_ReloadAll(void);

// Model loading and unloading
model_t *Model_LoadFile(const char *filename);
model_t *Model_ReadFile(const char *filename, int ztag);
void Model_Unload(model_t *model);

// Model rendering
modelinfo_t *Model_IsAvailable(spritenum_t spritenum, skin_t *skin);
void Model_AdjustTextureCoords(model_t *model, float max_s, float max_t);
boolean Model_AllowRendering(mobj_t *mobj);
boolean Model_CanInterpolate(mobj_t *mobj, model_t *model);
boolean Model_CanInterpolateSprite2(modelspr2frames_t *spr2frame);
UINT8 Model_GetSprite2(modelinfo_t *md2, skin_t *skin, UINT8 spr2, player_t *player);

// Miscellaneous stuff
void Model_Optimize(model_t *model);
void Model_LoadInterpolationSettings(model_t *model);
void Model_LoadSprite2(model_t *model);
void Model_GenerateVertexNormals(model_t *model);
void Model_GeneratePolygonNormals(model_t *model, int ztag);
void Model_CreateVBOTiny(mesh_t *mesh, tinyframe_t *frame);
void Model_CreateVBO(mesh_t *mesh, mdlframe_t *frame);
void Model_DeleteVBOs(model_t *model);
tag_t *Model_GetTagByName(model_t *model, char *name, int frame);

#endif
