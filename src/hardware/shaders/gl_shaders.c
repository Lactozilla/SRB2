// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file gl_shaders.c
/// \brief OpenGL shaders

#include "gl_shaders.h"

#include "../r_glcommon/r_glcommon.h"

#include "../hw_dll.h"

#ifdef HAVE_GLES
#include "../r_gles/r_gles.h"
#else
#include "../r_opengl/r_opengl.h"
#endif

#include "../../doomdef.h"
#include "../../doomtype.h"
#include "../../doomdata.h"

GLboolean ShadersEnabled = GL_FALSE;
INT32 ShadersAllowed = GPU_SHADEROPTION_OFF;

#ifdef GL_SHADERS

typedef GLuint (R_GL_APIENTRY *PFNglCreateShader)       (GLenum);
typedef void   (R_GL_APIENTRY *PFNglShaderSource)       (GLuint, GLsizei, const GLchar**, GLint*);
typedef void   (R_GL_APIENTRY *PFNglCompileShader)      (GLuint);
typedef void   (R_GL_APIENTRY *PFNglGetShaderiv)        (GLuint, GLenum, GLint*);
typedef void   (R_GL_APIENTRY *PFNglGetShaderInfoLog)   (GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (R_GL_APIENTRY *PFNglDeleteShader)       (GLuint);
typedef GLuint (R_GL_APIENTRY *PFNglCreateProgram)      (void);
typedef void   (R_GL_APIENTRY *PFNglDeleteProgram)      (GLuint);
typedef void   (R_GL_APIENTRY *PFNglAttachShader)       (GLuint, GLuint);
typedef void   (R_GL_APIENTRY *PFNglLinkProgram)        (GLuint);
typedef void   (R_GL_APIENTRY *PFNglGetProgramiv)       (GLuint, GLenum, GLint*);
typedef void   (R_GL_APIENTRY *PFNglUseProgram)         (GLuint);
typedef void   (R_GL_APIENTRY *PFNglUniform1i)          (GLint, GLint);
typedef void   (R_GL_APIENTRY *PFNglUniform1f)          (GLint, GLfloat);
typedef void   (R_GL_APIENTRY *PFNglUniform2f)          (GLint, GLfloat, GLfloat);
typedef void   (R_GL_APIENTRY *PFNglUniform3f)          (GLint, GLfloat, GLfloat, GLfloat);
typedef void   (R_GL_APIENTRY *PFNglUniform4f)          (GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void   (R_GL_APIENTRY *PFNglUniform1fv)         (GLint, GLsizei, const GLfloat*);
typedef void   (R_GL_APIENTRY *PFNglUniform2fv)         (GLint, GLsizei, const GLfloat*);
typedef void   (R_GL_APIENTRY *PFNglUniform3fv)         (GLint, GLsizei, const GLfloat*);
typedef void   (R_GL_APIENTRY *PFNglUniformMatrix4fv)   (GLint, GLsizei, GLboolean, const GLfloat *);
typedef GLint  (R_GL_APIENTRY *PFNglGetUniformLocation) (GLuint, const GLchar*);
typedef GLint  (R_GL_APIENTRY *PFNglGetAttribLocation)  (GLuint, const GLchar*);

static PFNglCreateShader pglCreateShader;
static PFNglShaderSource pglShaderSource;
static PFNglCompileShader pglCompileShader;
static PFNglGetShaderiv pglGetShaderiv;
static PFNglGetShaderInfoLog pglGetShaderInfoLog;
static PFNglDeleteShader pglDeleteShader;
static PFNglCreateProgram pglCreateProgram;
static PFNglDeleteProgram pglDeleteProgram;
static PFNglAttachShader pglAttachShader;
static PFNglLinkProgram pglLinkProgram;
static PFNglGetProgramiv pglGetProgramiv;
static PFNglUseProgram pglUseProgram;
static PFNglUniform1i pglUniform1i;
static PFNglUniform1f pglUniform1f;
static PFNglUniform2f pglUniform2f;
static PFNglUniform3f pglUniform3f;
static PFNglUniform4f pglUniform4f;
static PFNglUniform1fv pglUniform1fv;
static PFNglUniform2fv pglUniform2fv;
static PFNglUniform3fv pglUniform3fv;
static PFNglUniformMatrix4fv pglUniformMatrix4fv;
static PFNglGetUniformLocation pglGetUniformLocation;
static PFNglGetAttribLocation pglGetAttribLocation;

FShaderObject ShaderObjects[HWR_MAXSHADERS];
FShaderObject UserShaderObjects[HWR_MAXSHADERS];

FShaderSource ShaderSources[HWR_MAXSHADERS];
FShaderSource CustomShaders[HWR_MAXSHADERS];

FShaderState ShaderState;

static GLRGBAFloat ShaderDefaultColor = {1.0f, 1.0f, 1.0f, 1.0f};

// Shader info
static INT32 ShaderLevelTime = 0;

void Shader_LoadFunctions(void)
{
	pglCreateShader = GLBackend_GetFunction("glCreateShader");
	pglShaderSource = GLBackend_GetFunction("glShaderSource");
	pglCompileShader = GLBackend_GetFunction("glCompileShader");
	pglGetShaderiv = GLBackend_GetFunction("glGetShaderiv");
	pglGetShaderInfoLog = GLBackend_GetFunction("glGetShaderInfoLog");
	pglDeleteShader = GLBackend_GetFunction("glDeleteShader");
	pglCreateProgram = GLBackend_GetFunction("glCreateProgram");
	pglDeleteProgram = GLBackend_GetFunction("glDeleteProgram");
	pglAttachShader = GLBackend_GetFunction("glAttachShader");
	pglLinkProgram = GLBackend_GetFunction("glLinkProgram");
	pglGetProgramiv = GLBackend_GetFunction("glGetProgramiv");
	pglUseProgram = GLBackend_GetFunction("glUseProgram");
	pglUniform1i = GLBackend_GetFunction("glUniform1i");
	pglUniform1f = GLBackend_GetFunction("glUniform1f");
	pglUniform2f = GLBackend_GetFunction("glUniform2f");
	pglUniform3f = GLBackend_GetFunction("glUniform3f");
	pglUniform4f = GLBackend_GetFunction("glUniform4f");
	pglUniform1fv = GLBackend_GetFunction("glUniform1fv");
	pglUniform2fv = GLBackend_GetFunction("glUniform2fv");
	pglUniform3fv = GLBackend_GetFunction("glUniform3fv");
	pglUniformMatrix4fv = GLBackend_GetFunction("glUniformMatrix4fv");
	pglGetUniformLocation = GLBackend_GetFunction("glGetUniformLocation");
	pglGetAttribLocation = GLBackend_GetFunction("glGetAttribLocation");
}

#ifdef HAVE_GLES2
int Shader_AttribLoc(int loc)
{
	FShaderObject *shader = ShaderState.current;
	int pos, attrib;

	EShaderAttribute LOC_TO_ATTRIB[attrib_max] =
	{
		attrib_position,     // LOC_POSITION
		attrib_texcoord,     // LOC_TEXCOORD + LOC_TEXCOORD0
		attrib_normal,       // LOC_NORMAL
		attrib_colors,       // LOC_COLORS
		attrib_fadetexcoord, // LOC_TEXCOORD1
	};

	if (shader == NULL)
		I_Error("Shader_AttribLoc: shader not set");

	attrib = LOC_TO_ATTRIB[loc];
	pos = shader->attributes[attrib];

	if (pos == -1)
		return 0;

	return pos;
}
#endif

//
// Shader info
// Those are given to the uniforms.
//

void Shader_SetInfo(INT32 info, INT32 value)
{
	switch (info)
	{
		case GPU_SHADERINFO_LEVELTIME:
			ShaderLevelTime = value;
			break;
		default:
			break;
	}
}

//
// Shader source loading
//
static void StoreShaderSource(FShaderProgram *program, FShaderSource *list, int number, char *code, size_t size, UINT8 stage)
{
	FShaderSource *shader;

	if (!GLExtension_shaders)
		return;

	if (number > HWR_MAXSHADERS)
		I_Error("StoreShaderSource: cannot load shader %d (min 0, max %d)", number, HWR_MAXSHADERS);
	else if (code == NULL)
		I_Error("StoreShaderSource: empty shader source");

	shader = &list[number];

	if (stage == SHADER_STAGE_VERTEX)
		HWR_WriteShaderSource(&shader->vertex, stage, program, code, size);
	else if (stage == SHADER_STAGE_FRAGMENT)
		HWR_WriteShaderSource(&shader->fragment, stage, program, code, size);
}

void Shader_StoreSource(FShaderProgram *program, UINT32 number, char *code, size_t size, UINT8 stage)
{
	StoreShaderSource(program, ShaderSources, number, code, size, stage);
}

void Shader_StoreCustomSource(FShaderProgram *program, UINT32 number, char *code, size_t size, UINT8 stage)
{
	if (number < 1)
		I_Error("Shader_StoreCustomSource: cannot load shader %d (min 1, max %d)", number, HWR_MAXSHADERS);
	StoreShaderSource(program, CustomShaders, number, code, size, stage);
}

void Shader_Set(int type)
{
	FShaderObject *shader = ShaderState.current;

#ifndef HAVE_GLES2
	if (ShadersAllowed == GPU_SHADEROPTION_OFF)
		return;
#endif

	if ((shader == NULL) || (GLuint)type != ShaderState.type)
	{
		FShaderObject *baseshader = &ShaderObjects[type];
		FShaderObject *usershader = &UserShaderObjects[type];

		if (usershader->program)
			shader = (ShadersAllowed == GPU_SHADEROPTION_NOCUSTOM) ? baseshader : usershader;
		else
			shader = baseshader;

		ShaderState.current = shader;
		ShaderState.type = type;
		ShaderState.changed = true;
	}

	if (ShaderState.program != shader->program)
	{
		ShaderState.program = shader->program;
		ShaderState.changed = true;
	}

#ifdef HAVE_GLES2
	Shader_SetTransform();
	ShadersEnabled = true;
#else
	ShadersEnabled = (shader->program != 0);
#endif
}

void Shader_UnSet(void)
{
#ifdef HAVE_GLES2
	Shader_Set(SHADER_DEFAULT);
	Shader_SetUniforms(NULL, NULL, NULL, NULL);
#else
	ShaderState.current = NULL;
	ShaderState.type = 0;
	ShaderState.program = 0;

	if (pglUseProgram)
		pglUseProgram(0);
	ShadersEnabled = false;
#endif
}

void Shader_Clean(void)
{
	INT32 i;

	for (i = 1; i < HWR_MAXSHADERS; i++)
	{
		FShaderSource *shader = &CustomShaders[i];

		if (shader->vertex)
			free(shader->vertex);

		if (shader->fragment)
			free(shader->fragment);

		shader->vertex = NULL;
		shader->fragment = NULL;
	}
}

#ifdef HAVE_GLES2
#define Shader_ErrorMessage I_Error
#else
#define Shader_ErrorMessage GL_MSG_Error
#endif

static void Shader_CompileError(const char *message, GLuint program, INT32 shadernum)
{
	GLchar *infoLog = NULL;
	GLint logLength;

	pglGetShaderiv(program, GL_INFO_LOG_LENGTH, &logLength);

	if (logLength)
	{
		infoLog = malloc(logLength);
		pglGetShaderInfoLog(program, logLength, NULL, infoLog);
	}

	Shader_ErrorMessage("Shader_CompileProgram: %s (%s)\n%s", message, HWR_GetShaderName(shadernum), (infoLog ? infoLog : ""));

	if (infoLog)
		free(infoLog);
}

static boolean Shader_CompileProgram(FShaderObject *shader, GLint i, const GLchar *vert_shader, const GLchar *frag_shader)
{
	GLuint gl_vertShader, gl_fragShader;
	GLint result;

	//
	// Load and compile vertex shader
	//
	gl_vertShader = pglCreateShader(GL_VERTEX_SHADER);
	if (!gl_vertShader)
	{
		Shader_ErrorMessage("Shader_CompileProgram: Error creating vertex shader %s\n", HWR_GetShaderName(i));
		return false;
	}

	pglShaderSource(gl_vertShader, 1, &vert_shader, NULL);
	pglCompileShader(gl_vertShader);

	// check for compile errors
	pglGetShaderiv(gl_vertShader, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE)
	{
		Shader_CompileError("Error compiling vertex shader", gl_vertShader, i);
		pglDeleteShader(gl_vertShader);
		return false;
	}

	//
	// Load and compile fragment shader
	//
	gl_fragShader = pglCreateShader(GL_FRAGMENT_SHADER);
	if (!gl_fragShader)
	{
		Shader_ErrorMessage("Shader_CompileProgram: Error creating fragment shader %s\n", HWR_GetShaderName(i));
		pglDeleteShader(gl_vertShader);
		pglDeleteShader(gl_fragShader);
		return false;
	}

	pglShaderSource(gl_fragShader, 1, &frag_shader, NULL);
	pglCompileShader(gl_fragShader);

	// check for compile errors
	pglGetShaderiv(gl_fragShader, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE)
	{
		Shader_CompileError("Error compiling fragment shader", gl_fragShader, i);
		pglDeleteShader(gl_vertShader);
		pglDeleteShader(gl_fragShader);
		return false;
	}

	shader->program = pglCreateProgram();
	pglAttachShader(shader->program, gl_vertShader);
	pglAttachShader(shader->program, gl_fragShader);
	pglLinkProgram(shader->program);

	// check link status
	pglGetProgramiv(shader->program, GL_LINK_STATUS, &result);

	// delete the shader objects
	pglDeleteShader(gl_vertShader);
	pglDeleteShader(gl_fragShader);

	// couldn't link?
	if (result != GL_TRUE)
	{
		Shader_ErrorMessage("Shader_CompileProgram: Error linking shader program %s\n", HWR_GetShaderName(i));
		pglDeleteProgram(shader->program);
		return false;
	}

#define GETUNI(uniform) pglGetUniformLocation(shader->program, uniform)

#ifdef HAVE_GLES2
	memset(shader->projMatrix, 0x00, sizeof(fmatrix4_t));
	memset(shader->viewMatrix, 0x00, sizeof(fmatrix4_t));
	memset(shader->modelMatrix, 0x00, sizeof(fmatrix4_t));

	// transform
	shader->uniforms[uniform_model]       = GETUNI("u_model");
	shader->uniforms[uniform_view]        = GETUNI("u_view");
	shader->uniforms[uniform_projection]  = GETUNI("u_projection");

	// samplers
	shader->uniforms[uniform_startscreen] = GETUNI("t_startscreen");
	shader->uniforms[uniform_endscreen]   = GETUNI("t_endscreen");
	shader->uniforms[uniform_fademask]    = GETUNI("t_fademask");

	// misc.
	shader->uniforms[uniform_isfadingin]  = GETUNI("is_fading_in");
	shader->uniforms[uniform_istowhite]   = GETUNI("is_to_white");
#endif

	// lighting
	shader->uniforms[uniform_poly_color]  = GETUNI("poly_color");
	shader->uniforms[uniform_tint_color]  = GETUNI("tint_color");
	shader->uniforms[uniform_fade_color]  = GETUNI("fade_color");
	shader->uniforms[uniform_lighting]    = GETUNI("lighting");
	shader->uniforms[uniform_fade_start]  = GETUNI("fade_start");
	shader->uniforms[uniform_fade_end]    = GETUNI("fade_end");

	// misc. (custom shaders)
	shader->uniforms[uniform_leveltime]   = GETUNI("leveltime");

#undef GETUNI

#ifdef HAVE_GLES2

#define GETATTRIB(attribute) pglGetAttribLocation(shader->program, attribute)

	shader->attributes[attrib_position]     = GETATTRIB("a_position");
	shader->attributes[attrib_texcoord]     = GETATTRIB("a_texcoord");
	shader->attributes[attrib_normal]       = GETATTRIB("a_normal");
	shader->attributes[attrib_colors]       = GETATTRIB("a_colors");
	shader->attributes[attrib_fadetexcoord] = GETATTRIB("a_fademasktexcoord");

#undef GETATTRIB

#endif

	return true;
}

boolean Shader_Compile(void)
{
	GLint i = 0;

	if (!GLExtension_shaders)
		return false;

	CustomShaders[SHADER_DEFAULT].vertex = NULL;
	CustomShaders[SHADER_DEFAULT].fragment = NULL;

	for (; i < HWR_MAXSHADERS; i++)
	{
		FShaderObject *shader, *usershader;
		const GLchar *vert_shader = ShaderSources[i].vertex;
		const GLchar *frag_shader = ShaderSources[i].fragment;

		if (!(ShaderSources[i].vertex && ShaderSources[i].fragment))
			continue;

#ifndef HAVE_GLES2
		if (i == SHADER_FADEMASK || i == SHADER_FADEMASK_ADDITIVEANDSUBTRACTIVE)
			continue;
#endif

		shader = &ShaderObjects[i];
		usershader = &UserShaderObjects[i];

		if (shader->program)
			pglDeleteProgram(shader->program);
		if (usershader->program)
			pglDeleteProgram(usershader->program);

		shader->program = 0;
		usershader->program = 0;

		if (!Shader_CompileProgram(shader, i, vert_shader, frag_shader))
			shader->program = 0;

		// Compile custom shader
		if ((i == SHADER_DEFAULT) || (CustomShaders[i].vertex == NULL && CustomShaders[i].fragment == NULL))
			continue;

		// 18032019
		if (CustomShaders[i].vertex)
			vert_shader = CustomShaders[i].vertex;
		if (CustomShaders[i].fragment)
			frag_shader = CustomShaders[i].fragment;

		if (!Shader_CompileProgram(usershader, i, vert_shader, frag_shader))
		{
			GL_MSG_Warning("Shader_Compile: Could not compile custom shader program for %s\n", HWR_GetShaderName(i));
			usershader->program = 0;
		}
	}

	Shader_Set(SHADER_DEFAULT);

#ifdef HAVE_GLES2
	pglUseProgram(ShaderState.program);
	ShaderState.changed = false;
#endif

	return true;
}

void Shader_SetProgram(void)
{
	pglUseProgram(ShaderState.current->program);
}

void Shader_SetProgramIfChanged(void)
{
	if (ShaderState.changed)
	{
		pglUseProgram(ShaderState.current->program);
		ShaderState.changed = false;
	}
}

#ifdef HAVE_GLES2
void Shader_SetTransform(void)
{
	FShaderObject *shader = ShaderState.current;
	if (!shader)
		return;

	Shader_SetProgramIfChanged();

	if (memcmp(projMatrix, shader->projMatrix, sizeof(fmatrix4_t)))
	{
		memcpy(shader->projMatrix, projMatrix, sizeof(fmatrix4_t));
		pglUniformMatrix4fv(shader->uniforms[uniform_projection], 1, GL_FALSE, (float *)projMatrix);
	}

	if (memcmp(viewMatrix, shader->viewMatrix, sizeof(fmatrix4_t)))
	{
		memcpy(shader->viewMatrix, viewMatrix, sizeof(fmatrix4_t));
		pglUniformMatrix4fv(shader->uniforms[uniform_view], 1, GL_FALSE, (float *)viewMatrix);
	}

	if (memcmp(modelMatrix, shader->modelMatrix, sizeof(fmatrix4_t)))
	{
		memcpy(shader->modelMatrix, modelMatrix, sizeof(fmatrix4_t));
		pglUniformMatrix4fv(shader->uniforms[uniform_model], 1, GL_FALSE, (float *)modelMatrix);
	}
}
#endif

void Shader_SetUniforms(FSurfaceInfo *Surface, GLRGBAFloat *poly, GLRGBAFloat *tint, GLRGBAFloat *fade)
{
	FShaderObject *shader = ShaderState.current;

	if (ShadersEnabled && (shader != NULL) && pglUseProgram)
	{
		if (!shader->program)
		{
			pglUseProgram(0);
			return;
		}

		Shader_SetProgramIfChanged();

		// Color uniforms can be left NULL and will be set to white (1.0f, 1.0f, 1.0f, 1.0f)
		if (poly == NULL)
			poly = &ShaderDefaultColor;
		if (tint == NULL)
			tint = &ShaderDefaultColor;
		if (fade == NULL)
			fade = &ShaderDefaultColor;

		#define UNIFORM_1(uniform, a, function) \
			if (uniform != -1) \
				function (uniform, a);

		#define UNIFORM_2(uniform, a, b, function) \
			if (uniform != -1) \
				function (uniform, a, b);

		#define UNIFORM_3(uniform, a, b, c, function) \
			if (uniform != -1) \
				function (uniform, a, b, c);

		#define UNIFORM_4(uniform, a, b, c, d, function) \
			if (uniform != -1) \
				function (uniform, a, b, c, d);

		UNIFORM_4(shader->uniforms[uniform_poly_color], poly->red, poly->green, poly->blue, poly->alpha, pglUniform4f);
		UNIFORM_4(shader->uniforms[uniform_tint_color], tint->red, tint->green, tint->blue, tint->alpha, pglUniform4f);
		UNIFORM_4(shader->uniforms[uniform_fade_color], fade->red, fade->green, fade->blue, fade->alpha, pglUniform4f);

		if (Surface != NULL)
		{
			UNIFORM_1(shader->uniforms[uniform_lighting], Surface->LightInfo.LightLevel, pglUniform1f);
			UNIFORM_1(shader->uniforms[uniform_fade_start], Surface->LightInfo.FadeStart, pglUniform1f);
			UNIFORM_1(shader->uniforms[uniform_fade_end], Surface->LightInfo.FadeEnd, pglUniform1f);
		}

		UNIFORM_1(shader->uniforms[uniform_leveltime], ((float)ShaderLevelTime) / TICRATE, pglUniform1f);

		#undef UNIFORM_1
		#undef UNIFORM_2
		#undef UNIFORM_3
		#undef UNIFORM_4
	}
}

void Shader_SetSampler(EShaderUniform uniform, GLint value)
{
	FShaderObject *shader = ShaderState.current;
	if (!shader)
		return;

	Shader_SetProgramIfChanged();

	if (shader->uniforms[uniform] != -1)
		pglUniform1i(shader->uniforms[uniform], value);
}

#endif // GL_SHADERS
