// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
// Copyright (C) 1998-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file gl_shaders.h
/// \brief OpenGL shaders

#ifndef _GL_SHADERS_H_
#define _GL_SHADERS_H_

#include "../../doomdef.h"
#include "../hw_gpu.h"

#include "../r_glcommon/r_glcommon.h"

extern GLboolean ShadersEnabled;
extern INT32 ShadersAllowed;

#ifdef GL_SHADERS

void Shader_LoadFunctions(void);

enum
{
	LOC_POSITION  = 0,
	LOC_TEXCOORD  = 1,
	LOC_NORMAL    = 2,
	LOC_COLORS    = 3,

	LOC_TEXCOORD0 = LOC_TEXCOORD,
	LOC_TEXCOORD1 = 4
};

#define MAXSHADERS 16
#define MAXSHADERPROGRAMS 16

// 08072020
typedef enum EShaderUniform
{
#ifdef HAVE_GLES2
	// transform
	uniform_model,
	uniform_view,
	uniform_projection,

	// samplers
	uniform_startscreen,
	uniform_endscreen,
	uniform_fademask,
#endif

	// lighting
	uniform_poly_color,
	uniform_tint_color,
	uniform_fade_color,
	uniform_lighting,
	uniform_fade_start,
	uniform_fade_end,

	// misc.
#ifdef HAVE_GLES2
	uniform_isfadingin,
	uniform_istowhite,
#endif
	uniform_leveltime,

	uniform_max,
} EShaderUniform;

// 27072020
#ifdef HAVE_GLES2
typedef enum EShaderAttribute
{
	attrib_position,     // LOC_POSITION
	attrib_texcoord,     // LOC_TEXCOORD + LOC_TEXCOORD0
	attrib_normal,       // LOC_NORMAL
	attrib_colors,       // LOC_COLORS
	attrib_fadetexcoord, // LOC_TEXCOORD1

	attrib_max,
} EShaderAttribute;
#endif

typedef struct FShaderObject
{
	GLuint program;
	boolean custom;

	GLint uniforms[uniform_max+1];
#ifdef HAVE_GLES2
	GLint attributes[attribute_max+1];
#endif

#ifdef HAVE_GLES2
	fmatrix4_t projMatrix;
	fmatrix4_t viewMatrix;
	fmatrix4_t modelMatrix;
#endif
} FShaderObject;

extern FShaderObject ShaderObjects[HWR_MAXSHADERS];
extern FShaderObject UserShaderObjects[HWR_MAXSHADERS];
extern FShaderSource CustomShaders[HWR_MAXSHADERS];

// 09102020
typedef struct FShaderState
{
	FShaderObject *current;
	GLuint type;
	GLuint program;
	boolean changed;
} FShaderState;
extern FShaderState ShaderState;

void Shader_Set(int type);
void Shader_UnSet(void);

#ifdef HAVE_GLES2
void Shader_SetTransform(void);
#endif

void Shader_LoadCustom(int number, char *code, size_t size, boolean isfragment);

boolean Shader_Compile(void);
void Shader_Clean(void);

void Shader_SetUniforms(FSurfaceInfo *Surface, GLRGBAFloat *poly, GLRGBAFloat *tint, GLRGBAFloat *fade);
void Shader_SetSampler(EShaderUniform uniform, GLint value);
#define Shader_SetIntegerUniform Shader_SetSampler
void Shader_SetInfo(INT32 info, INT32 value);

#ifdef HAVE_GLES2
int Shader_AttribLoc(int loc);
#endif

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif

#endif // GL_SHADERS

#endif // _GL_SHADERS_H_
