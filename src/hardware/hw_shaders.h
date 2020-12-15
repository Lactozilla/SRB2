// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file hw_shaders.h
/// \brief OpenGL shaders.

#ifndef __HWR_SHADERS_H__
#define __HWR_SHADERS_H__

#include "hw_defs.h"
#include "hw_data.h"
#include "hw_gpu.h"
#include "hw_glob.h"

#define SHADER_INCLUDES_SOFTWARE "SoftwareShaderIncludes"

void HWR_LoadShaders(void);
void HWR_ReadShaderDefinitions(UINT16 wadnum, UINT16 numlumps);

boolean HWR_CompileShaders(void);

const char *HWR_GetShaderName(INT32 shader);
void HWR_WriteShaderSource(char **dest, UINT8 stage, FShaderProgram *program, char *code, size_t size);

FShaderProgram *HWR_FindFirstShaderProgram(const char *name, INT32 *id);
FShaderProgram *HWR_FindLastShaderProgram(const char *name, INT32 *id);

UINT16 HWR_FindLegacyShaderDefs(UINT16 wadnum, UINT32 start);
void HWR_LoadLegacyShadersFromFile(UINT16 wadnum, boolean PK3);

extern FShaderBuiltInList BuiltInShaders[];

#endif
