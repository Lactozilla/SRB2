// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file hw_shaders.c
/// \brief OpenGL shaders.

#ifdef HWRENDER

#include "hw_defs.h"
#include "hw_data.h"
#include "hw_gpu.h"
#include "hw_glob.h"
#include "hw_shaders.h"

#include "../z_zone.h"
#include "../fastcmp.h"

static FShaderProgram *ShaderList;
static INT32 ShaderCount;

FShaderBuiltInList BuiltInShaders[] =
{
	{"Default", SHADER_DEFAULT},
	{"Flat", SHADER_FLOOR},
	{"WallTexture", SHADER_WALL},
	{"Sprite", SHADER_SPRITE},
	{"Model", SHADER_MODEL},
	{"ModelLighting", SHADER_MODEL_LIGHTING},
	{"WaterRipple", SHADER_WATER},
	{"Fog", SHADER_FOG},
	{"Sky", SHADER_SKY},
	{"Wipe", SHADER_FADEMASK},
	{"WipeAdditiveAndSubtractive", SHADER_FADEMASK_ADDITIVEANDSUBTRACTIVE},
	{SHADER_INCLUDES_SOFTWARE, -1},
	{NULL, 0},
};

static void HWR_UploadShader(FShaderProgram *shader)
{
	void (*ShaderUploadFunction) (FShaderProgram *, UINT32, char *, size_t, UINT8) = NULL;

	if (shader->isInclude)
		return;

	if (!shader->builtin)
		ShaderUploadFunction = GPU->StoreCustomShader;
	else
		ShaderUploadFunction = GPU->StoreShader;

	if (shader->source.vertex)
		ShaderUploadFunction(shader, shader->type, shader->source.vertex, strlen(shader->source.vertex) + 1, SHADER_STAGE_VERTEX);
	if (shader->source.fragment)
		ShaderUploadFunction(shader, shader->type, shader->source.fragment, strlen(shader->source.fragment) + 1, SHADER_STAGE_FRAGMENT);
}

boolean HWR_CompileShaders(void)
{
	INT32 i = 0;

	for (; i < ShaderCount; i++)
	{
		FShaderProgram *shader = &ShaderList[i];
		if (shader->valid)
			HWR_UploadShader(shader);
	}

	return GPU->CompileShaders();
}

static void InitShaderList(void)
{
	if (ShaderList)
		Z_Free(ShaderList);

	ShaderCount = (SHADER_BUILTIN_LAST + 1);
	ShaderList = Z_Calloc(ShaderCount * sizeof(FShaderProgram), PU_STATIC, NULL);
}

static FShaderProgram *CreateShaderProgram(void)
{
	FShaderProgram *shader;

	ShaderCount++;
	ShaderList = Z_Realloc(ShaderList, ShaderCount * sizeof(FShaderProgram), PU_STATIC, NULL);

	shader = &ShaderList[ShaderCount - 1];
	memset(shader, 0x00, sizeof(FShaderProgram));

	return shader;
}

static FShaderProgram *IterShaders(INT32 i, const char *name)
{
	FShaderProgram *shader = &ShaderList[i];

	if (!shader->valid)
		return NULL;
	else if (!stricmp(shader->name, name))
		return shader;

	return NULL;
}

FShaderProgram *HWR_FindFirstShaderProgram(const char *name, INT32 *id)
{
	INT32 i = 0;

	for (; i < ShaderCount; i++)
	{
		FShaderProgram *shader = IterShaders(i, name);

		if (shader)
		{
			if (id)
				*id = i;
			return shader;
		}
	}

	return NULL;
}

FShaderProgram *HWR_FindLastShaderProgram(const char *name, INT32 *id)
{
	INT32 i = ShaderCount - 1;

	for (; i >= 0; i--)
	{
		FShaderProgram *shader = IterShaders(i, name);

		if (shader)
		{
			if (id)
				*id = i;
			return shader;
		}
	}

	return NULL;
}

static FShaderProgram *FindShaderProgramOrCreate(const char *name, INT32 *id)
{
	FShaderProgram *shader = HWR_FindLastShaderProgram(name, id);

	if (shader)
		return shader;

	if (id)
		*id = ShaderCount;
	return CreateShaderProgram();
}

static INT32 GetBuiltInShaderID(const char *name)
{
	INT32 i;

	for (i = 0; BuiltInShaders[i].type; i++)
	{
		if (!stricmp(name, BuiltInShaders[i].type))
			return BuiltInShaders[i].id;
	}

	return -1;
}

static FShaderProgram *GetBuiltInShader(INT32 id)
{
	if (ShaderCount <= id || !ShaderCount)
	{
		FShaderProgram *shader;

		ShaderCount = (id + 1);
		ShaderList = Z_Realloc(ShaderList, ShaderCount * sizeof(FShaderProgram), PU_STATIC, NULL);

		shader = &ShaderList[id];
		memset(shader, 0x00, sizeof(FShaderProgram));

		return shader;
	}

	return &ShaderList[id];
}

const char *HWR_GetShaderName(INT32 shader)
{
	if (shader >= 0 && shader < ShaderCount)
		return ShaderList[shader].name;
	return "Unknown";
}

void HWR_LoadShaders(void)
{
	INT32 i;

	for (i = 0; i < numwadfiles; i++)
	{
		HWR_ReadShaderDefinitions(i, wadfiles[i]->numlumps);
		HWR_LoadLegacyShadersFromFile(i, (wadfiles[i]->type == RET_PK3));
	}
}

void HWR_WriteShaderSource(char **dest, UINT8 stage, FShaderProgram *program, char *code, size_t size)
{
	size_t len = size, offs = 0, totaloffs = 0;
	size_t *offsList = NULL;
	char **includesList = NULL;
	INT32 numIncludes = 0;

	if (*dest)
		Z_Free(*dest);

	if (program && program->includes.count)
	{
		FShaderIncludes *includes = &program->includes;
		INT32 i = 0;

		for (; i < includes->count; i++)
		{
			FShaderProgram *builtin = NULL;

			if (!stricmp(includes->list[i], SHADER_INCLUDES_SOFTWARE) && program->builtin)
				builtin = HWR_FindFirstShaderProgram(SHADER_INCLUDES_SOFTWARE, NULL);
			else
				builtin = HWR_FindLastShaderProgram(includes->list[i], NULL);

			if (builtin)
			{
				char *src = NULL;

				if (stage == SHADER_STAGE_VERTEX && includes->stages[i] == SHADER_STAGE_VERTEX)
					src = builtin->source.vertex;
				else if (stage == SHADER_STAGE_FRAGMENT && includes->stages[i] == SHADER_STAGE_FRAGMENT)
					src = builtin->source.fragment;

				if (src)
				{
					numIncludes++;

					offsList = Z_Realloc(offsList, numIncludes * sizeof(size_t), PU_STATIC, NULL);
					includesList = Z_Realloc(includesList, numIncludes * sizeof(char *), PU_STATIC, NULL);

					offs = (strlen(src) + 1);
					offsList[numIncludes - 1] = totaloffs;

					if (builtin->includes.count) // Recursive inclusion
					{
						char **buf = &includesList[numIncludes - 1];
						(*buf) = NULL;
						HWR_WriteShaderSource(buf, stage, builtin, src, offs - 1);
						offs = (strlen((*buf)) + 1);
					}
					else
						includesList[numIncludes - 1] = src;

					totaloffs += offs;
					len += offs;
				}
			}
		}
	}

	*dest = Z_Malloc(len + 1, PU_STATIC, NULL);

	if (numIncludes)
	{
		char *buf;
		INT32 i = 0;

		for (; i < numIncludes; i++)
		{
			offs = offsList[i];
			buf = (*dest) + offs;

			strncpy(buf, includesList[i], strlen(includesList[i]));
			buf[strlen(includesList[i])] = '\n';
		}
	}

	strncpy((*dest) + totaloffs, code, size);
	(*dest)[len] = 0;

	if (offsList)
		Z_Free(offsList);
	if (includesList)
		Z_Free(includesList);
}

static char *CurShaderDefsText = NULL;

static void ParseError(const char *err, ...)
{
	va_list argptr;
	char buf[8192];

	va_start(argptr, err);
	vsprintf(buf, err, argptr);
	va_end(argptr);

	CONS_Alert(CONS_ERROR, "Error parsing shader definitions: %s\n", buf);
}

static char **includesList = NULL;
static UINT8 *includesStages = NULL;
static INT32 includesCount = 0;

static void InsertIntoIncludesList(const char *include, INT32 stage)
{
	includesCount++;
	includesList = Z_Realloc(includesList, includesCount * sizeof(char *), PU_STATIC, NULL);
	includesStages = Z_Realloc(includesStages, includesCount * sizeof(UINT8), PU_STATIC, NULL);
	includesList[includesCount - 1] = Z_StrDup(include);
	includesStages[includesCount - 1] = stage;
}

static boolean ParseShaderDefinitions(size_t lumplength, boolean mainfile)
{
	FShaderProgram *shader = NULL;
	boolean builtin = false, isInclude = false, replace = false, removesoftwareshaders = false;
	char *sources[NUMSHADERSTAGES - 1];
	INT32 id, type = 0;

	char *tk = NULL, *name = NULL;
	size_t len;

	char *stagetype = NULL;
	boolean success = false;

	tk = M_GetToken(NULL);
	if (tk == NULL)
	{
		ParseError("EOF where shader name should be");
		goto failure;
	}

	len = strlen(tk);
	name = calloc(len + 1, sizeof(char));
	M_Memcpy(name, tk, len);

	Z_Free(tk);

	// Left Curly Brace
	tk = M_GetToken(NULL);
	if (tk == NULL)
	{
		ParseError("EOF where open curly brace for shader \"%s\" should be", name);
		goto failure;
	}

	includesList = NULL;
	includesStages = NULL;
	includesCount = 0;
	memset(sources, 0x00, sizeof(char *) * (NUMSHADERSTAGES - 1));

	if (!strcmp(tk, "{"))
	{
		Z_Free(tk);

		tk = M_GetToken(NULL);
		if (tk == NULL)
		{
			ParseError("EOF where definition for shader \"%s\" should be", name);
			goto failure;
		}

		while (strcmp(tk, "}"))
		{
			if (!stricmp(tk, "Source"))
			{
				UINT8 stage = SHADER_STAGE_NONE;
				INT32 brackets = 0;

				Z_Free(tk);

				tk = M_GetToken(NULL);
				if (tk == NULL)
				{
					ParseError("EOF where shader stage for shader \"%s\" should be", name);
					goto failure;
				}

				if (!stricmp(tk, "Vertex"))
					stage = SHADER_STAGE_VERTEX;
				else if (!stricmp(tk, "Fragment"))
					stage = SHADER_STAGE_FRAGMENT;
				else if (!stricmp(tk, "{"))
				{
					ParseError("Expected a shader stage for shader \"%s\", got a left curly brace instead", name);
					goto failure;
				}
				else
				{
					ParseError("Unknown shader stage \"%s\" in shader \"%s\"", tk, name);
					goto failure;
				}

				stagetype = Z_StrDup(tk);

				Z_Free(tk);
				tk = M_GetToken(NULL);
				if (tk == NULL)
				{
					ParseError("EOF where a left curly brace for shader \"%s\" should be", name);
					goto failure;
				}

				if (!strcmp(tk, "{"))
				{
					UINT32 start = M_GetTokenPos() + 1, end;

					Z_Free(tk);
					tk = M_GetToken(NULL);
					if (tk == NULL)
					{
						ParseError("EOF where shader source for shader \"%s\" should be", name);
						goto failure;
					}

					while (tk && M_GetTokenPos() < lumplength)
					{
						if (fastcmp(tk, "}"))
						{
							brackets--;
							if (brackets < 0)
							{
								M_UnGetToken();
								break;
							}
						}
						else if (fastcmp(tk, "{"))
							brackets++;

						Z_Free(tk);
						tk = M_GetToken(NULL);
					}

					if (brackets >= 0)
					{
						ParseError("Unclosed brackets in shader \"%s\"", name);
						goto failure;
					}
					else
					{
						end = M_GetTokenPos();
						len = (end - start);
						sources[stage - 1] = Z_Calloc((len + 1) * sizeof(char), PU_STATIC, NULL);
						M_Memcpy(sources[stage - 1], CurShaderDefsText + start, len);
						sources[stage - 1][len] = '\0';
					}

					Z_Free(tk);
					tk = M_GetToken(NULL);
				}
				else if (!strcmp(tk, "UseDefault"))
				{
					FShaderProgram *def = (ShaderCount) ? &ShaderList[SHADER_DEFAULT] : NULL;

					if (def)
					{
						char *src = NULL;

						if (stage == SHADER_STAGE_VERTEX)
							src = def->source.vertex;
						else if (stage == SHADER_STAGE_FRAGMENT)
							src = def->source.fragment;

						if (src == NULL)
						{
							ParseError("Shader \"%s\" specified a default source for shader stage \"%s\", but the default shader source was not defined", stagetype, name);
							goto failure;
						}

						HWR_WriteShaderSource(&sources[stage - 1], stage, def, src, strlen(src));
					}
					else
					{
						ParseError("Shader \"%s\" specified a default source for shader stage \"%s\", but the default shader program was not defined", name, stagetype);
						goto failure;
					}
				}
				else
				{
					ParseError("Expected \"{\" for shader \"%s\", got \"%s\" instead", name, tk);
					goto failure;
				}
			}
			else if (!stricmp(tk, "BuiltIn"))
			{
				Z_Free(tk);

				if (!mainfile)
				{
					ParseError("Cannot define shader \"%s\" as a built-in", name);
					goto failure;
				}

				builtin = true;
			}
			else if (!stricmp(tk, "IsInclude"))
			{
				Z_Free(tk);
				isInclude = true;
			}
			else if (!stricmp(tk, "Replace"))
			{
				Z_Free(tk);

				if (!isInclude)
				{
					ParseError("Shader %s cannot replace a non-include program", name);
					goto failure;
				}

				replace = true;
			}
			else if (!stricmp(tk, "Include"))
			{
				char *inc = NULL, *src = NULL;
				INT32 i = 0, numSources = 1, addedSources = 0;
				FShaderProgram *prg = NULL;
				UINT8 stage = SHADER_STAGE_NONE;

				Z_Free(tk);
				tk = M_GetToken(NULL);
				if (tk == NULL)
				{
					ParseError("EOF where shader include for shader \"%s\" should be", name);
					goto failure;
				}

				inc = Z_StrDup(tk);
				prg = HWR_FindLastShaderProgram(inc, NULL);

				if (prg)
				{
					Z_Free(tk);
					tk = M_GetToken(NULL);
					if (tk == NULL)
					{
						ParseError("EOF where shader stage for shader \"%s\" included by shader \"%s\" should be", inc, name);
						Z_Free(inc);
						goto failure;
					}

					if (!stricmp(tk, "Vertex"))
						stage = SHADER_STAGE_VERTEX;
					else if (!stricmp(tk, "Fragment"))
						stage = SHADER_STAGE_FRAGMENT;
					else
					{
						M_UnGetToken();
						numSources = 2;
					}

					if (numSources)
						stagetype = Z_StrDup(tk);

					for (; i < numSources; i++)
					{
						if (numSources > 1)
						{
							switch (i)
							{
								case 0:
									stage = SHADER_STAGE_VERTEX;
									break;
								case 1:
									stage = SHADER_STAGE_FRAGMENT;
									break;
								default:
									break;
							}
						}

						if (stage == SHADER_STAGE_VERTEX)
							src = prg->source.vertex;
						else if (stage == SHADER_STAGE_FRAGMENT)
							src = prg->source.fragment;

						if (src)
						{
							InsertIntoIncludesList(inc, stage);
							addedSources++;
						}
						else if (numSources == 1)
						{
							ParseError("Shader \"%s\" included by shader \"%s\" has no source for shader stage \"%s\"", inc, name, stagetype);
							Z_Free(inc);
							goto failure;
						}
					}

					if (!addedSources)
					{
						ParseError("Shader \"%s\" included by shader \"%s\" has no shader stages", inc, name);
						Z_Free(inc);
						goto failure;
					}

					Z_Free(inc);
				}
				else
				{
					ParseError("Shader \"%s\" tried to include shader \"%s\", but it does not exist", stagetype, src);
					Z_Free(src);
					goto failure;
				}
			}
			else if (!stricmp(tk, "IncludeSoftwareShaders"))
			{
				Z_Free(tk);
				InsertIntoIncludesList(SHADER_INCLUDES_SOFTWARE, SHADER_STAGE_FRAGMENT);
				removesoftwareshaders = false;
			}
			else if (!stricmp(tk, "RemoveSoftwareShaders"))
			{
				Z_Free(tk);
				removesoftwareshaders = true;
			}
			else
			{
				ParseError("Unknown keyword \"%s\" in shader %s", tk, name);
				goto failure;
			}

			tk = M_GetToken(NULL);
			if (tk == NULL)
			{
				ParseError("EOF where shader info or right curly brace for shader \"%s\" should be", name);
				goto failure;
			}
		}
	}
	else
	{
		ParseError("Expected \"{\" for shader \"%s\", got \"%s\" instead", name, tk);
		goto failure;
	}

	success = true;

	if (ShaderList == NULL)
		InitShaderList();

	id = GetBuiltInShaderID(name);

	if (!builtin && !replace)
	{
		FShaderProgram *copy = HWR_FindLastShaderProgram(name, NULL);
		FShaderProgram last = {0};

		if (id < 0)
			type = ShaderCount;
		else
			type = id;

		// The Z_Realloc that CreateShaderProgram would make the *copy pointer invalid,
		// so instead, copy that shader program into the stack.
		if (copy)
			M_Memcpy(&last, copy, sizeof(FShaderProgram));

		shader = CreateShaderProgram();

		if (copy)
		{
			shader->name = Z_StrDup(last.name);
			shader->includes = last.includes;
			shader->isInclude = last.isInclude;

			if (last.source.vertex)
				shader->source.vertex = Z_StrDup(last.source.vertex);
			if (last.source.fragment)
				shader->source.fragment = Z_StrDup(last.source.fragment);

			if (last.includes.count)
			{
				INT32 i = 0, count = last.includes.count;

				shader->includes.list = Z_Malloc(count * sizeof(char *), PU_STATIC, NULL);
				shader->includes.stages = Z_Malloc(count * sizeof(INT32), PU_STATIC, NULL);
				shader->includes.count = count;

				for (; i < count; i++)
				{
					shader->includes.list[i] = Z_StrDup(last.includes.list[i]);
					shader->includes.stages[i] = last.includes.stages[i];
				}
			}
		}
	}
	else
	{
		if (id < 0)
			shader = FindShaderProgramOrCreate(name, &type);
		else
		{
			shader = GetBuiltInShader(id);
			type = id;
		}
	}

	if (shader->name == NULL)
	{
		len = strlen(name);
		shader->name = Z_Calloc((len + 1) * sizeof(char), PU_STATIC, NULL);
		M_Memcpy(shader->name, name, len);
	}

	if (includesCount)
	{
		INT32 i = 0, j, count = includesCount;

		for (; i < count; i++)
		{
			j = shader->includes.count;
			shader->includes.list = Z_Realloc(shader->includes.list, (j + 1) * sizeof(char *), PU_STATIC, NULL);
			shader->includes.stages = Z_Realloc(shader->includes.stages, (j + 1) * sizeof(UINT8), PU_STATIC, NULL);
			shader->includes.list[j] = includesList[i];
			shader->includes.stages[j] = includesStages[i];
			shader->includes.count++;
		}

		Z_Free(includesList);
		Z_Free(includesStages);
	}

	if (removesoftwareshaders)
	{
		INT32 i = 0, count = shader->includes.count;

		for (; i < count; i++)
		{
			if (!stricmp(SHADER_INCLUDES_SOFTWARE, shader->includes.list[i]))
			{
				memmove(shader->includes.list + i, shader->includes.list + (i + 1), (shader->includes.count - i) * sizeof(char *));
				memmove(shader->includes.stages + i, shader->includes.stages + (i + 1), (shader->includes.count - i) * sizeof(UINT8));
				shader->includes.count--;
				shader->includes.list = Z_Realloc(shader->includes.list, shader->includes.count * sizeof(char *), PU_STATIC, NULL);
				shader->includes.stages = Z_Realloc(shader->includes.stages, shader->includes.count * sizeof(UINT8), PU_STATIC, NULL);
			}
		}
	}

	if (sources[SHADER_STAGE_VERTEX - 1])
	{
		if (shader->source.vertex)
			Z_Free(shader->source.vertex);
		shader->source.vertex = Z_StrDup(sources[SHADER_STAGE_VERTEX - 1]);
	}

	if (sources[SHADER_STAGE_FRAGMENT - 1])
	{
		if (shader->source.fragment)
			Z_Free(shader->source.fragment);
		shader->source.fragment = Z_StrDup(sources[SHADER_STAGE_FRAGMENT - 1]);
	}

	if (!isInclude)
	{
		if (!shader->source.vertex)
			I_Error("ParseShaderDefinitions: Shader \"%s\" has no vertex stage", shader->name);
		if (!shader->source.fragment)
			I_Error("ParseShaderDefinitions: Shader \"%s\" has no fragment stage", shader->name);
	}

	shader->valid = true;
	shader->type = type;
	shader->builtin = builtin;
	shader->isInclude = isInclude;

failure:
	if (sources[SHADER_STAGE_VERTEX - 1])
		Z_Free(sources[SHADER_STAGE_VERTEX - 1]);
	if (sources[SHADER_STAGE_FRAGMENT - 1])
		Z_Free(sources[SHADER_STAGE_FRAGMENT - 1]);

	if (name)
		free(name);
	if (stagetype)
		Z_Free(stagetype);
	if (tk)
		Z_Free(tk);

	return success;
}

static void ReadShaderDefinitionsLump(UINT16 wadnum, UINT16 lumpnum, boolean builtin)
{
	char *lump = W_CacheLumpNumPwad(wadnum, lumpnum, PU_STATIC);
	size_t len = W_LumpLengthPwad(wadnum, lumpnum);
	char *tk;

	CurShaderDefsText = Z_Malloc((len + 1) * sizeof(char), PU_STATIC, NULL);
	M_Memcpy(CurShaderDefsText, lump, len);
	CurShaderDefsText[len] = '\0';

	Z_Free(lump);
	tk = M_GetToken(CurShaderDefsText);

	while (tk != NULL)
	{
		if (!stricmp(tk, "Shader"))
		{
			if (!ParseShaderDefinitions(len, builtin))
			{
				Z_Free(tk);
				break;
			}
		}
		else
		{
			ParseError("Unknown keyword \"%s\"", tk);
			Z_Free(tk);
			break;
		}

		Z_Free(tk);
		tk = M_GetToken(NULL);
	}

	Z_Free(CurShaderDefsText);
	CurShaderDefsText = NULL;
}

void HWR_ReadShaderDefinitions(UINT16 wadnum, UINT16 numlumps)
{
	lumpinfo_t *lumpinfo = wadfiles[wadnum]->lumpinfo;
	UINT16 i;

	for (i = 0; i < numlumps; i++, lumpinfo++)
	{
		if (!memcmp(lumpinfo->name, "SHDRDEFS", 8))
			ReadShaderDefinitionsLump(wadnum, i, wadfiles[wadnum]->mainfile);
	}
}

// jimita 18032019
UINT16 HWR_FindLegacyShaderDefs(UINT16 wadnum, UINT32 start)
{
	lumpinfo_t *lump_p = wadfiles[wadnum]->lumpinfo;
	UINT16 i;

	if (start >= UINT16_MAX)
		return UINT16_MAX;

	for (i = start; i < wadfiles[wadnum]->numlumps; i++, lump_p++)
		if (memcmp(lump_p->name, "SHADERS", 7) == 0)
			return i;

	return UINT16_MAX;
}

void HWR_LoadLegacyShadersFromFile(UINT16 wadnum, boolean PK3)
{
	UINT16 lump = HWR_FindLegacyShaderDefs(wadnum, 0);

	char *shaderdef, *line;
	char *stoken;
	char *value;

	size_t size;
	int linenum = 1;
	int i;

	UINT8 stage = SHADER_STAGE_NONE;

	while (lump != UINT16_MAX)
	{
		shaderdef = W_CacheLumpNumPwad(wadnum, lump, PU_CACHE);
		size = W_LumpLengthPwad(wadnum, lump);

		line = Z_Malloc(size+1, PU_STATIC, NULL);
		M_Memcpy(line, shaderdef, size);
		line[size] = '\0';

		stoken = strtok(line, "\r\n ");
		while (stoken)
		{
			if ((stoken[0] == '/' && stoken[1] == '/')
				|| (stoken[0] == '#'))// skip comments
			{
				stoken = strtok(NULL, "\r\n");
				goto skip_field;
			}

			if (!stricmp(stoken, "GLSL"))
			{
				value = strtok(NULL, "\r\n ");
				if (!value)
				{
					CONS_Alert(CONS_WARNING, "HWR_LoadLegacyShadersFromFile: Missing shader type (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
					stoken = strtok(NULL, "\r\n"); // skip end of line
					goto skip_lump;
				}

				if (!stricmp(value, "VERTEX"))
					stage = SHADER_STAGE_VERTEX;
				else if (!stricmp(value, "FRAGMENT"))
					stage = SHADER_STAGE_FRAGMENT;
				else
				{
					CONS_Alert(CONS_WARNING, "HWR_LoadLegacyShadersFromFile: Unknown shader type \"%s\" (file %s, line %d)\n", value, wadfiles[wadnum]->filename, linenum);
					stoken = strtok(NULL, "\r\n"); // skip end of line
					goto skip_lump;
				}

	skip_lump:
				stoken = strtok(NULL, "\r\n ");
				linenum++;
			}
			else
			{
				value = strtok(NULL, "\r\n= ");
				if (!value)
				{
					CONS_Alert(CONS_WARNING, "HWR_LoadLegacyShadersFromFile: Missing shader target (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
					stoken = strtok(NULL, "\r\n"); // skip end of line
					goto skip_field;
				}

				if (!stage)
				{
					CONS_Alert(CONS_ERROR, "HWR_LoadLegacyShadersFromFile: Missing shader stage (file %s, line %d)\n", wadfiles[wadnum]->filename, linenum);
					Z_Free(line);
					break;
				}

				for (i = 0; BuiltInShaders[i].type; i++)
				{
					FShaderBuiltInList *builtin = &BuiltInShaders[i];
					INT32 id = builtin->id;

					if (id < SHADER_LEVEL_FIRST || id > SHADER_LEVEL_LAST)
						continue;

					if (!stricmp(builtin->type, stoken))
					{
						size_t shader_size;
						char *shader_source;
						char *shader_lumpname;
						UINT16 shader_lumpnum;

						if (PK3)
						{
							shader_lumpname = Z_Malloc(strlen(value) + 12, PU_STATIC, NULL);
							strcpy(shader_lumpname, "Shaders/sh_");
							strcat(shader_lumpname, value);
							shader_lumpnum = W_CheckNumForFullNamePK3(shader_lumpname, wadnum, 0);
						}
						else
						{
							shader_lumpname = Z_Malloc(strlen(value) + 4, PU_STATIC, NULL);
							strcpy(shader_lumpname, "SH_");
							strcat(shader_lumpname, value);
							shader_lumpnum = W_CheckNumForNamePwad(shader_lumpname, wadnum, 0);
						}

						if (shader_lumpnum == INT16_MAX)
						{
							CONS_Alert(CONS_ERROR, "HWR_LoadLegacyShadersFromFile: Missing shader source %s (file %s, line %d)\n", shader_lumpname, wadfiles[wadnum]->filename, linenum);
							Z_Free(shader_lumpname);
							continue;
						}

						shader_size = W_LumpLengthPwad(wadnum, shader_lumpnum);
						shader_source = Z_Malloc(shader_size, PU_STATIC, NULL);
						W_ReadLumpPwad(wadnum, shader_lumpnum, shader_source);

						GPU->StoreCustomShader(NULL, id, shader_source, shader_size, stage);

						Z_Free(shader_source);
						Z_Free(shader_lumpname);
					}
				}

	skip_field:
				stoken = strtok(NULL, "\r\n= ");
				linenum++;
			}
		}

		Z_Free(line);
		lump = HWR_FindLegacyShaderDefs(wadnum, lump + 1);
	}
}
#endif
