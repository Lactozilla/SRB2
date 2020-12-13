// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file r_glcommon.c
/// \brief Common OpenGL functions shared by all of the OpenGL backends

#include "r_glcommon.h"

#include "../../doomdata.h"
#include "../../doomtype.h"
#include "../../doomdef.h"
#include "../../console.h"

#ifdef GL_SHADERS
#include "../shaders/gl_shaders.h"
#endif

#include <stdarg.h>

const GLubyte *GLVersion = NULL;
const GLubyte *GLRenderer = NULL;
const GLubyte *GLExtensions = NULL;

GLint GLMajorVersion = 0;
GLint GLMinorVersion = 0;

// ==========================================================================
//                                                                    GLOBALS
// ==========================================================================

RGBA_t GPUTexturePalette[256];
GLint  GPUTextureFormat;
GLint  GPUScreenWidth, GPUScreenHeight;
GLbyte GPUScreenDepth;
GLint  GPUMaximumAnisotropy;

FTextureInfo *TexCacheTail = NULL;
FTextureInfo *TexCacheHead = NULL;

GLuint CurrentTexture = 0;
GLuint BlankTexture = 0;

GLuint ScreenTexture = 0;
GLuint FinalScreenTexture = 0;
GLuint WipeStartTexture = 0;
GLuint WipeEndTexture = 0;

UINT32 CurrentPolyFlags;

GLboolean MipmappingEnabled = GL_FALSE;
GLboolean MipmappingSupported = GL_FALSE;
GLboolean ModelLightingEnabled = GL_FALSE;

GLint MipmapMinFilter = GL_LINEAR;
GLint MipmapMagFilter = GL_LINEAR;
GLint AnisotropicFilter = 0;

float NearClippingPlane = NZCLIP_PLANE;

// ==========================================================================
//                                                                 EXTENSIONS
// ==========================================================================

boolean GLExtension_multitexture;
boolean GLExtension_vertex_buffer_object;
boolean GLExtension_texture_filter_anisotropic;
boolean GLExtension_vertex_program;
boolean GLExtension_fragment_program;
boolean GLExtension_framebuffer_object;
boolean GLExtension_shaders; // Not an extension on its own, but it is set if multiple extensions are available.

static FExtensionList const ExtensionList[] = {
	{"GL_ARB_multitexture", &GLExtension_multitexture},
	{"GL_ARB_vertex_buffer_object", &GLExtension_vertex_buffer_object},
	{"GL_ARB_texture_filter_anisotropic", &GLExtension_texture_filter_anisotropic},
	{"GL_EXT_texture_filter_anisotropic", &GLExtension_texture_filter_anisotropic},
	{"GL_ARB_vertex_program", &GLExtension_vertex_program},
	{"GL_ARB_fragment_program", &GLExtension_fragment_program},
	{"GL_ARB_framebuffer_object", &GLExtension_framebuffer_object},
	{NULL, NULL}
};

static void PrintExtensions(const GLubyte *extensions);

// ==========================================================================
//                                                           OPENGL FUNCTIONS
// ==========================================================================

#ifndef STATIC_OPENGL
/* Miscellaneous */
PFNglClear pglClear;
PFNglGetFloatv pglGetFloatv;
PFNglGetIntegerv pglGetIntegerv;
PFNglGetString pglGetString;
PFNglClearColor pglClearColor;
PFNglColorMask pglColorMask;
PFNglAlphaFunc pglAlphaFunc;
PFNglBlendFunc pglBlendFunc;
PFNglCullFace pglCullFace;
PFNglPolygonOffset pglPolygonOffset;
PFNglEnable pglEnable;
PFNglDisable pglDisable;

/* Depth buffer */
PFNglDepthFunc pglDepthFunc;
PFNglDepthMask pglDepthMask;

/* Transformation */
PFNglViewport pglViewport;

/* Raster functions */
PFNglPixelStorei pglPixelStorei;
PFNglReadPixels pglReadPixels;

/* Texture mapping */
PFNglTexParameteri pglTexParameteri;
PFNglTexImage2D pglTexImage2D;
PFNglTexSubImage2D pglTexSubImage2D;

/* Drawing functions */
PFNglDrawArrays pglDrawArrays;
PFNglDrawElements pglDrawElements;

/* Texture objects */
PFNglGenTextures pglGenTextures;
PFNglDeleteTextures pglDeleteTextures;
PFNglBindTexture pglBindTexture;

/* Texture mapping */
PFNglCopyTexImage2D pglCopyTexImage2D;
PFNglCopyTexSubImage2D pglCopyTexSubImage2D;

/* 1.3 functions for multitexturing */
PFNglActiveTexture pglActiveTexture;
#endif

//
// Multi-texturing
//

#ifndef HAVE_GLES2
#ifndef STATIC_OPENGL
PFNglClientActiveTexture pglClientActiveTexture;
#endif
#endif

//
// Mipmapping
//

PFNglGenerateMipmap pglGenerateMipmap;

//
// Depth functions
//
PFNglClearDepth pglClearDepth;
PFNglDepthRange pglDepthRange;

PFNglClearDepthf pglClearDepthf;
PFNglDepthRangef pglDepthRangef;

//
// Legacy functions
//

#ifndef HAVE_GLES2
#ifndef STATIC_OPENGL
/* Transformation */
PFNglMatrixMode pglMatrixMode;
PFNglViewport pglViewport;
PFNglPushMatrix pglPushMatrix;
PFNglPopMatrix pglPopMatrix;
PFNglLoadIdentity pglLoadIdentity;
PFNglMultMatrixf pglMultMatrixf;
PFNglRotatef pglRotatef;
PFNglScalef pglScalef;
PFNglTranslatef pglTranslatef;

/* Drawing functions */
PFNglVertexPointer pglVertexPointer;
PFNglNormalPointer pglNormalPointer;
PFNglTexCoordPointer pglTexCoordPointer;
PFNglColorPointer pglColorPointer;
PFNglEnableClientState pglEnableClientState;
PFNglDisableClientState pglDisableClientState;

/* Lighting */
PFNglShadeModel pglShadeModel;
PFNglLightfv pglLightfv;
PFNglLightModelfv pglLightModelfv;
PFNglMaterialfv pglMaterialfv;

/* Texture mapping */
PFNglTexEnvi pglTexEnvi;
#endif
#endif // HAVE_GLES2

// Color
PFNglColor4f pglColor4f;
PFNglColor4ubv pglColor4ubv;

/* 1.5 functions for buffers */
PFNglGenBuffers pglGenBuffers;
PFNglBindBuffer pglBindBuffer;
PFNglBufferData pglBufferData;
PFNglDeleteBuffers pglDeleteBuffers;

/* 2.0 functions */
PFNglBlendEquation pglBlendEquation;

// ==========================================================================
//                                                                  FUNCTIONS
// ==========================================================================

#ifdef GL_SHADERS
static boolean ShadersInit = false;
#endif

boolean GLBackend_Init(void)
{
#if defined(__ANDROID__)
	GLBackend_InitContext();
#endif
	GPUTextureFormat = GL_RGBA;
	return GLBackend_LoadFunctions();
}

boolean GLBackend_InitContext(void)
{
	if (GLVersion == NULL || GLRenderer == NULL)
	{
		GLVersion = pglGetString(GL_VERSION);
		GLRenderer = pglGetString(GL_RENDERER);

		pglGetIntegerv(GL_MAJOR_VERSION, &GLMajorVersion);
		pglGetIntegerv(GL_MINOR_VERSION, &GLMinorVersion);

		GL_DBG_Printf("OpenGL %s\n", GLVersion);
		GL_DBG_Printf("Specification: %s\n", GPUInterface_GetAPIName());
		GL_DBG_Printf("Version: %d.%d\n", GLMajorVersion, GLMinorVersion);
		GL_DBG_Printf("GPU: %s\n", GLRenderer);

#if !defined(__ANDROID__)
		if (strcmp((const char*)GLRenderer, "GDI Generic") == 0 &&
			strcmp((const char*)GLVersion, "1.1.0") == 0)
		{
			// Oh no... Windows gave us the GDI Generic rasterizer, so something is wrong...
			// The game will crash later on when unsupported OpenGL commands are encountered.
			// Instead of a nondescript crash, show a more informative error message.
			// Also set the renderer variable back to software so the next launch won't
			// repeat this error.
			CV_StealthSet(&cv_renderer, "Software");
			I_Error("OpenGL Error: Failed to access the GPU. There may be an issue with your graphics drivers.");
		}
#endif
	}

	if (GLExtensions == NULL)
		GLExtension_Init();

	return true;
}

boolean GLBackend_InitShaders(void)
{
#ifdef GL_SHADERS
	if (ShadersInit)
		return true;

	Shader_LoadFunctions();

	if (!Shader_Compile())
		return false;

	ShadersInit = true;
#endif

	return true;
}

boolean GLBackend_LoadCommonFunctions(void)
{
	GETOPENGLFUNC(ClearColor)

	GETOPENGLFUNC(Clear)
	GETOPENGLFUNC(ColorMask)
	GETOPENGLFUNC(AlphaFunc)
	GETOPENGLFUNC(BlendFunc)
	GETOPENGLFUNC(CullFace)
	GETOPENGLFUNC(PolygonOffset)
	GETOPENGLFUNC(Enable)
	GETOPENGLFUNC(Disable)
	GETOPENGLFUNC(GetFloatv)
	GETOPENGLFUNC(GetIntegerv)
	GETOPENGLFUNC(GetString)

	GETOPENGLFUNC(DepthFunc)
	GETOPENGLFUNC(DepthMask)

	GETOPENGLFUNC(Viewport)

	GETOPENGLFUNC(DrawArrays)
	GETOPENGLFUNC(DrawElements)

	GETOPENGLFUNC(PixelStorei)
	GETOPENGLFUNC(ReadPixels)

	GETOPENGLFUNC(TexParameteri)
	GETOPENGLFUNC(TexImage2D)
	GETOPENGLFUNC(TexSubImage2D)

	GETOPENGLFUNC(GenTextures)
	GETOPENGLFUNC(DeleteTextures)
	GETOPENGLFUNC(BindTexture)

	GETOPENGLFUNC(CopyTexImage2D)
	GETOPENGLFUNC(CopyTexSubImage2D)

	return true;
}

boolean GLBackend_LoadLegacyFunctions(void)
{
#ifndef HAVE_GLES2
	GETOPENGLFUNC(MatrixMode)
	GETOPENGLFUNC(Viewport)
	GETOPENGLFUNC(PushMatrix)
	GETOPENGLFUNC(PopMatrix)
	GETOPENGLFUNC(LoadIdentity)
	GETOPENGLFUNC(MultMatrixf)
	GETOPENGLFUNC(Rotatef)
	GETOPENGLFUNC(Scalef)
	GETOPENGLFUNC(Translatef)

	GETOPENGLFUNC(ShadeModel)
	GETOPENGLFUNC(Lightfv)
	GETOPENGLFUNC(LightModelfv)
	GETOPENGLFUNC(Materialfv)

	GETOPENGLFUNC(TexEnvi)
#endif

	return true;
}

INT32 GLBackend_GetShaderType(INT32 type)
{
#ifdef GL_SHADERS
	// If using model lighting, set the appropriate shader.
	// However don't override a custom shader.
	if (type == SHADER_MODEL && ModelLightingEnabled
	&& !(ShaderObjects[SHADER_MODEL].custom && !ShaderObjects[SHADER_MODEL_LIGHTING].custom))
		return SHADER_MODEL_LIGHTING;
#endif

#ifdef HAVE_GLES2
	if (ShadersAllowed == GPU_SHADEROPTION_OFF)
	{
		switch (type)
		{
			case SHADER_FLOOR:
			case SHADER_WALL:
			case SHADER_SPRITE:
			case SHADER_MODEL:
			case SHADER_MODEL_LIGHTING:
			case SHADER_WATER:
			case SHADER_FOG:
			case SHADER_SKY:
				return SHADER_DEFAULT;
			default:
				break;
		}
	}
#endif

	return type;
}

void GLState_SetSurface(INT32 w, INT32 h)
{
	GPU->SetModelView(w, h);
	GPU->SetInitialStates();
	pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLState_SetPalette(RGBA_t *palette)
{
	size_t palsize = (sizeof(RGBA_t) * 256);
	// on a palette change, you have to reload all of the textures
	if (memcmp(&GPUTexturePalette, palette, palsize))
	{
		memcpy(&GPUTexturePalette, palette, palsize);
		GLTexture_Flush();
	}
}

// Sets the texture filtering mode.
void GLState_SetFilterMode(INT32 mode)
{
	switch (mode)
	{
		case GPU_TEXFILTER_TRILINEAR:
			MipmapMinFilter = GL_LINEAR_MIPMAP_LINEAR;
			MipmapMagFilter = GL_LINEAR;
			MipmappingEnabled = GL_TRUE;
			break;
		case GPU_TEXFILTER_BILINEAR:
			MipmapMinFilter = MipmapMagFilter = GL_LINEAR;
			MipmappingEnabled = GL_FALSE;
			break;
		case GPU_TEXFILTER_POINTSAMPLED:
			MipmapMinFilter = MipmapMagFilter = GL_NEAREST;
			MipmappingEnabled = GL_FALSE;
			break;
		case GPU_TEXFILTER_MIXED1:
			MipmapMinFilter = GL_NEAREST;
			MipmapMagFilter = GL_LINEAR;
			MipmappingEnabled = GL_FALSE;
			break;
		case GPU_TEXFILTER_MIXED2:
			MipmapMinFilter = GL_LINEAR;
			MipmapMagFilter = GL_NEAREST;
			MipmappingEnabled = GL_FALSE;
			break;
		case GPU_TEXFILTER_MIXED3:
			MipmapMinFilter = GL_LINEAR_MIPMAP_LINEAR;
			MipmapMagFilter = GL_NEAREST;
			MipmappingEnabled = GL_TRUE;
			break;
		default:
			MipmapMagFilter = GL_LINEAR;
			MipmapMinFilter = GL_NEAREST;
	}
}

void GLState_SetClamp(GLenum pname)
{
#ifndef HAVE_GLES
	pglTexParameteri(GL_TEXTURE_2D, pname, GL_CLAMP); // fallback clamp
#endif
	pglTexParameteri(GL_TEXTURE_2D, pname, GL_CLAMP_TO_EDGE);
}

void GLState_SetDepthBuffer(void)
{
	// Hurdler: all that are permanent states
	if (pglClearDepthf)
		pglClearDepthf(1.0f);
	else
		pglClearDepth(1.0f);

	if (pglDepthRangef)
		pglDepthRangef(0.0f, 1.0f);
	else
		pglDepthRange(0.0f, 1.0f);

	pglDepthFunc(GL_LEQUAL);
}

static void SetBlendEquation(GLenum mode)
{
	if (pglBlendEquation)
		pglBlendEquation(mode);
}

static void SetBlendMode(UINT32 flags)
{
	// Set blending function
	switch (flags)
	{
		case PF_Translucent & PF_Blending:
			pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // alpha = level of transparency
			break;
		case PF_Masked & PF_Blending:
			// Hurdler: does that mean lighting is only made by alpha src?
			// it sounds ok, but not for polygonsmooth
			pglBlendFunc(GL_SRC_ALPHA, GL_ZERO);                // 0 alpha = holes in texture
			break;
		case PF_Additive & PF_Blending:
		case PF_Subtractive & PF_Blending:
		case PF_ReverseSubtract & PF_Blending:
		case PF_Environment & PF_Blending:
			pglBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case PF_AdditiveSource & PF_Blending:
			pglBlendFunc(GL_SRC_ALPHA, GL_ONE); // src * alpha + dest
			break;
		case PF_Multiplicative & PF_Blending:
			pglBlendFunc(GL_DST_COLOR, GL_ZERO);
			break;
		case PF_Fog & PF_Fog:
			// Sryder: Fog
			// multiplies input colour by input alpha, and destination colour by input colour, then adds them
			pglBlendFunc(GL_SRC_ALPHA, GL_SRC_COLOR);
			break;
		default: // must be 0, otherwise it's an error
			// No blending
			pglBlendFunc(GL_ONE, GL_ZERO);   // the same as no blending
			break;
	}

	// Set blending equation
	switch (flags)
	{
		case PF_Subtractive & PF_Blending:
			SetBlendEquation(GL_FUNC_SUBTRACT);
			break;
		case PF_ReverseSubtract & PF_Blending:
			// good for shadow
			// not really but what else ?
			SetBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
			break;
		default:
			SetBlendEquation(GL_FUNC_ADD);
			break;
	}

	// Alpha testing
	switch (flags)
	{
		case PF_Masked & PF_Blending:
			pglAlphaFunc(GL_GREATER, 0.5f);
			break;
		case PF_Translucent & PF_Blending:
		case PF_Additive & PF_Blending:
		case PF_AdditiveSource & PF_Blending:
		case PF_Subtractive & PF_Blending:
		case PF_ReverseSubtract & PF_Blending:
		case PF_Environment & PF_Blending:
		case PF_Multiplicative & PF_Blending:
			pglAlphaFunc(GL_NOTEQUAL, 0.0f);
			break;
		case PF_Fog & PF_Fog:
			pglAlphaFunc(GL_ALWAYS, 0.0f); // Don't discard zero alpha fragments
			break;
		default:
			pglAlphaFunc(GL_GREATER, 0.5f);
			break;
	}
}

// PF_Masked - we could use an ALPHA_TEST of GL_EQUAL, and alpha ref of 0,
//             is it faster when pixels are discarded ?

void GLState_SetBlend(UINT32 PolyFlags)
{
	UINT32 Xor = CurrentPolyFlags^PolyFlags;

	if (Xor & (PF_Blending|PF_RemoveYWrap|PF_ForceWrapX|PF_ForceWrapY|PF_Occlude|PF_NoTexture|PF_Modulated|PF_NoDepthTest|PF_Decal|PF_Invisible))
	{
		if (Xor & PF_Blending) // if blending mode must be changed
			SetBlendMode(PolyFlags & PF_Blending);

#ifndef HAVE_GLES2
		if (Xor & PF_NoAlphaTest)
		{
			if (PolyFlags & PF_NoAlphaTest)
				pglDisable(GL_ALPHA_TEST);
			else
				pglEnable(GL_ALPHA_TEST);      // discard 0 alpha pixels (holes in texture)
		}
#endif

		if (Xor & PF_Decal)
		{
			if (PolyFlags & PF_Decal)
				pglEnable(GL_POLYGON_OFFSET_FILL);
			else
				pglDisable(GL_POLYGON_OFFSET_FILL);
		}

		if (Xor & PF_NoDepthTest)
		{
			if (PolyFlags & PF_NoDepthTest)
				pglDepthFunc(GL_ALWAYS);
			else
				pglDepthFunc(GL_LEQUAL);
		}

		if (Xor & PF_RemoveYWrap)
		{
			if (PolyFlags & PF_RemoveYWrap)
				GLState_SetClamp(GL_TEXTURE_WRAP_T);
		}

		if (Xor & PF_ForceWrapX)
		{
			if (PolyFlags & PF_ForceWrapX)
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		}

		if (Xor & PF_ForceWrapY)
		{
			if (PolyFlags & PF_ForceWrapY)
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}

#ifndef HAVE_GLES2
		if (Xor & PF_Modulated)
		{
			if (PolyFlags & PF_Modulated)
			{   // mix texture colour with Surface->PolyColor
				pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
			else
			{   // colour from texture is unchanged before blending
				pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			}
		}
#endif

		if (Xor & PF_Occlude) // depth test but (no) depth write
		{
			if (PolyFlags&PF_Occlude)
			{
				pglDepthMask(1);
			}
			else
				pglDepthMask(0);
		}
		////Hurdler: not used if we don't define POLYSKY
		if (Xor & PF_Invisible)
		{
			if (PolyFlags&PF_Invisible)
				pglBlendFunc(GL_ZERO, GL_ONE);         // transparent blending
			else
			{   // big hack: (TODO: manage that better)
				// we test only for PF_Masked because PF_Invisible is only used
				// (for now) with it (yeah, that's crappy, sorry)
				if ((PolyFlags&PF_Blending)==PF_Masked)
					pglBlendFunc(GL_SRC_ALPHA, GL_ZERO);
			}
		}
		if (PolyFlags & PF_NoTexture)
		{
			GLTexture_SetNoTexture();
		}
	}

	CurrentPolyFlags = PolyFlags;
}

void GLState_SetColor(const GLfloat red, const GLfloat green, const GLfloat blue, const GLfloat alpha)
{
#if defined(__ANDROID__)
	if (pglColor4f)
		pglColor4f(red, green, blue, alpha);
	else if (pglColor4ubv)
#endif
	{
		GLubyte color[4];

		color[0] = (unsigned char)(red * 255);
		color[1] = (unsigned char)(green * 255);
		color[2] = (unsigned char)(blue * 255);
		color[3] = (unsigned char)(alpha * 255);

		pglColor4ubv(color);
	}
}

void GLState_SetColorUBV(const GLubyte *v)
{
#if defined(__ANDROID__)
	if (pglColor4f)
	{
		INT32 i = 0;
		GLfloat color[4];

		for (; i < 4; i++)
		{
			color[i] = byte2float(*v);
			v++;
		}

		pglColor4f(color[0], color[1], color[2], color[3]);
	}
	else if (pglColor4ubv)
#endif
		pglColor4ubv(v);
}

void GLExtension_Init(void)
{
	INT32 i = 0;

	GLExtensions = pglGetString(GL_EXTENSIONS);

	GL_DBG_Printf("Extensions: ");
	PrintExtensions(GLExtensions);

	while (ExtensionList[i].name)
	{
		const FExtensionList *ext = &ExtensionList[i];

		if (GLExtension_Available(ext->name))
		{
			(*ext->extension) = true;
			GL_DBG_Printf("Extension %s is supported\n", ext->name);
		}
		else
		{
			(*ext->extension) = false;
			GL_DBG_Printf("Extension %s is unsupported\n", ext->name);
		}

		i++;
	}

#ifdef GL_SHADERS
	if (GLExtension_vertex_program && GLExtension_fragment_program && GLBackend_GetFunction("glUseProgram"))
		GLExtension_shaders = true;
#endif

	if (GLExtension_texture_filter_anisotropic)
		pglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &GPUMaximumAnisotropy);
	else
		GPUMaximumAnisotropy = 0;
}

boolean GLExtension_Available(const char *extension)
{
#if defined(HAVE_GLES) && defined(HAVE_SDL)
	return (SDL_GL_ExtensionSupported(extension) == SDL_TRUE ? true : false);
#else
	const GLubyte *start = GLExtensions;
	GLubyte       *where, *terminator;

	if (!extension || !start)
		return false;

	where = (GLubyte *) strchr(extension, ' ');
	if (where || *extension == '\0')
		return false;

	for (;;)
	{
		where = (GLubyte *) strstr((const char *) start, extension);
		if (!where)
			break;
		terminator = where + strlen(extension);
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return true;
		start = terminator;
	}

	return false;
#endif
}

static void PrintExtensions(const GLubyte *extensions)
{
	size_t size = strlen((const char *)extensions) + 1;
	char *tk, *ext = calloc(size, sizeof(char));

	memcpy(ext, extensions, size);
	tk = strtok(ext, " ");

	while (tk)
	{
		GL_DBG_Printf("%s", tk);
		tk = strtok(NULL, " ");
		if (tk)
			GL_DBG_Printf(" ", tk);
	}

	GL_DBG_Printf("\n");
	free(ext);
}

static size_t lerpBufferSize = 0;
float *vertBuffer = NULL;
float *normBuffer = NULL;

static size_t lerpTinyBufferSize = 0;
short *vertTinyBuffer = NULL;
char *normTinyBuffer = NULL;

// Static temporary buffer for doing frame interpolation
// 'size' is the vertex size
void GLModel_AllocLerpBuffer(size_t size)
{
	if (lerpBufferSize >= size)
		return;

	if (vertBuffer != NULL)
		free(vertBuffer);

	if (normBuffer != NULL)
		free(normBuffer);

	lerpBufferSize = size;
	vertBuffer = malloc(lerpBufferSize);
	normBuffer = malloc(lerpBufferSize);
}

// Static temporary buffer for doing frame interpolation
// 'size' is the vertex size
void GLModel_AllocLerpTinyBuffer(size_t size)
{
	if (lerpTinyBufferSize >= size)
		return;

	if (vertTinyBuffer != NULL)
		free(vertTinyBuffer);

	if (normTinyBuffer != NULL)
		free(normTinyBuffer);

	lerpTinyBufferSize = size;
	vertTinyBuffer = malloc(lerpTinyBufferSize);
	normTinyBuffer = malloc(lerpTinyBufferSize / 2);
}

static void CreateModelVBO(mesh_t *mesh, mdlframe_t *frame)
{
	int bufferSize = sizeof(vbo64_t)*mesh->numTriangles * 3;
	vbo64_t *buffer = (vbo64_t*)malloc(bufferSize);
	vbo64_t *bufPtr = buffer;

	float *vertPtr = frame->vertices;
	float *normPtr = frame->normals;
	float *tanPtr = frame->tangents;
	float *uvPtr = mesh->uvs;
	float *lightPtr = mesh->lightuvs;
	char *colorPtr = frame->colors;

	int i;
	for (i = 0; i < mesh->numTriangles * 3; i++)
	{
		bufPtr->x = *vertPtr++;
		bufPtr->y = *vertPtr++;
		bufPtr->z = *vertPtr++;

		bufPtr->nx = *normPtr++;
		bufPtr->ny = *normPtr++;
		bufPtr->nz = *normPtr++;

		bufPtr->s0 = *uvPtr++;
		bufPtr->t0 = *uvPtr++;

		if (tanPtr != NULL)
		{
			bufPtr->tan0 = *tanPtr++;
			bufPtr->tan1 = *tanPtr++;
			bufPtr->tan2 = *tanPtr++;
		}

		if (lightPtr != NULL)
		{
			bufPtr->s1 = *lightPtr++;
			bufPtr->t1 = *lightPtr++;
		}

		if (colorPtr)
		{
			bufPtr->r = *colorPtr++;
			bufPtr->g = *colorPtr++;
			bufPtr->b = *colorPtr++;
			bufPtr->a = *colorPtr++;
		}
		else
		{
			bufPtr->r = 255;
			bufPtr->g = 255;
			bufPtr->b = 255;
			bufPtr->a = 255;
		}

		bufPtr++;
	}

	pglGenBuffers(1, &frame->vboID);
	pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
	pglBufferData(GL_ARRAY_BUFFER, bufferSize, buffer, GL_STATIC_DRAW);
	free(buffer);

	// Don't leave the array buffer bound to the model,
	// since this is called mid-frame
	pglBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void CreateModelVBOTiny(mesh_t *mesh, tinyframe_t *frame)
{
	int bufferSize = sizeof(vbotiny_t)*mesh->numTriangles * 3;
	vbotiny_t *buffer = (vbotiny_t*)malloc(bufferSize);
	vbotiny_t *bufPtr = buffer;

	short *vertPtr = frame->vertices;
	char *normPtr = frame->normals;
	float *uvPtr = mesh->uvs;
	char *tanPtr = frame->tangents;

	int i;
	for (i = 0; i < mesh->numVertices; i++)
	{
		bufPtr->x = *vertPtr++;
		bufPtr->y = *vertPtr++;
		bufPtr->z = *vertPtr++;

		bufPtr->nx = *normPtr++;
		bufPtr->ny = *normPtr++;
		bufPtr->nz = *normPtr++;

		bufPtr->s0 = *uvPtr++;
		bufPtr->t0 = *uvPtr++;

		if (tanPtr)
		{
			bufPtr->tanx = *tanPtr++;
			bufPtr->tany = *tanPtr++;
			bufPtr->tanz = *tanPtr++;
		}

		bufPtr++;
	}

	pglGenBuffers(1, &frame->vboID);
	pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
	pglBufferData(GL_ARRAY_BUFFER, bufferSize, buffer, GL_STATIC_DRAW);
	free(buffer);

	// Don't leave the array buffer bound to the model,
	// since this is called mid-frame
	pglBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLModel_GenerateVBOs(model_t *model)
{
	int i;

	if (!GLExtension_vertex_buffer_object)
		return;

	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (mesh->frames)
		{
			int j;
			for (j = 0; j < model->meshes[i].numFrames; j++)
			{
				mdlframe_t *frame = &mesh->frames[j];
				if (frame->vboID)
					pglDeleteBuffers(1, &frame->vboID);
				frame->vboID = 0;
				CreateModelVBO(mesh, frame);
			}
		}
		else if (mesh->tinyframes)
		{
			int j;
			for (j = 0; j < model->meshes[i].numFrames; j++)
			{
				tinyframe_t *frame = &mesh->tinyframes[j];
				if (frame->vboID)
					pglDeleteBuffers(1, &frame->vboID);
				frame->vboID = 0;
				CreateModelVBOTiny(mesh, frame);
			}
		}
	}
}

// Sets the current texture.
void GLTexture_Set(HWRTexture_t *pTexInfo)
{
	if (!pTexInfo)
	{
		GLTexture_SetNoTexture();
		return;
	}
	else if (pTexInfo->downloaded)
	{
		if (pTexInfo->downloaded != CurrentTexture)
		{
			pglBindTexture(GL_TEXTURE_2D, pTexInfo->downloaded);
			CurrentTexture = pTexInfo->downloaded;
		}
	}
	else
	{
		FTextureInfo *newTex = calloc(1, sizeof (*newTex));

		GPU->UpdateTexture(pTexInfo);

		newTex->texture = pTexInfo;
		newTex->name = (UINT32)pTexInfo->downloaded;
		newTex->width = (UINT32)pTexInfo->width;
		newTex->height = (UINT32)pTexInfo->height;
		newTex->format = (UINT32)pTexInfo->format;

		// insertion at the tail
		if (TexCacheTail)
		{
			newTex->prev = TexCacheTail;
			TexCacheTail->next = newTex;
			TexCacheTail = newTex;
		}
		else // initialization of the linked list
			TexCacheTail = TexCacheHead = newTex;
	}
}

// Deletes a single texture.
void GLTexture_Delete(HWRTexture_t *pTexInfo)
{
	FTextureInfo *head = TexCacheHead;

	while (head)
	{
		if (head->name == pTexInfo->downloaded)
		{
			if (head->next)
				head->next->prev = head->prev;
			if (head->prev)
				head->prev->next = head->next;
			free(head);
			break;
		}

		head = head->next;
	}
}

// Updates a texture.
void GLTexture_Update(HWRTexture_t *pTexInfo)
{
	boolean update = true;
	static RGBA_t textureBuffer[2048 * 2048];
	const GLvoid *pTextureBuffer = textureBuffer;
	GLuint textureName = 0;

	if (!pTexInfo->downloaded)
	{
		pglGenTextures(1, &textureName);
		pTexInfo->downloaded = textureName;
		update = false;
	}
	else
		textureName = pTexInfo->downloaded;

	if (pTexInfo->format == GPU_TEXFMT_RGBA)
		pTextureBuffer = pTexInfo->data;
	else if ((pTexInfo->format == GPU_TEXFMT_P_8) || (pTexInfo->format == GPU_TEXFMT_AP_88))
		GLTexture_WritePalette(pTexInfo, textureBuffer);
	else if (pTexInfo->format == GPU_TEXFMT_ALPHA_INTENSITY_88)
		GLTexture_WriteLuminanceAlpha(pTexInfo, textureBuffer);
	else if (pTexInfo->format == GPU_TEXFMT_ALPHA_8) // Used for fade masks
	{
#ifdef HAVE_GLES2
		GLTexture_WriteFadeMaskR(pTexInfo, textureBuffer);
#else
		GLTexture_WriteFadeMaskA(pTexInfo, textureBuffer);
#endif
	}
	else
		GL_MSG_Warning("GLTexture_Update: bad format %d\n", pTexInfo->format);

	// the texture number was already generated by pglGenTextures
	pglBindTexture(GL_TEXTURE_2D, textureName);
	CurrentTexture = textureName;

	// disable texture filtering on any texture that has holes so there's no dumb borders or blending issues
	if (pTexInfo->flags & TF_TRANSPARENT)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, MipmapMagFilter);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, MipmapMinFilter);
	}

#ifdef HAVE_GLES
	GLTexture_UploadData(pTexInfo, pTextureBuffer, GPUTextureFormat, update);
#else
	if (GLTexture_CanGenerateMipmaps(pTexInfo) && !pglGenerateMipmap)
		pglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

	if (pTexInfo->format == GPU_TEXFMT_ALPHA_INTENSITY_88)
		GLTexture_UploadData(pTexInfo, pTextureBuffer, GL_LUMINANCE_ALPHA, update);
	else if (pTexInfo->format == GPU_TEXFMT_ALPHA_8)
		GLTexture_UploadData(pTexInfo, pTextureBuffer, GL_ALPHA, update);
	else
		GLTexture_UploadData(pTexInfo, pTextureBuffer, GPUTextureFormat, update);
#endif

	if (pTexInfo->flags & TF_WRAPX)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	else
		GLState_SetClamp(GL_TEXTURE_WRAP_S);

	if (pTexInfo->flags & TF_WRAPY)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	else
		GLState_SetClamp(GL_TEXTURE_WRAP_T);

	if (GLExtension_texture_filter_anisotropic && GPUMaximumAnisotropy)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, AnisotropicFilter);

	if (GLTexture_CanGenerateMipmaps(pTexInfo) && pglGenerateMipmap)
		pglGenerateMipmap(GL_TEXTURE_2D);
}

// Uploads texture data.
void GLTexture_UploadData(HWRTexture_t *pTexInfo, const GLvoid *pTextureBuffer, GLenum format, boolean update)
{
	if (update)
		pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pTexInfo->width, pTexInfo->height, GL_RGBA, GL_UNSIGNED_BYTE, pTextureBuffer);
	else
		pglTexImage2D(GL_TEXTURE_2D, 0, format, pTexInfo->width, pTexInfo->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pTextureBuffer);
}

// Writes a palettized texture.
void GLTexture_WritePalette(HWRTexture_t *pTexInfo, RGBA_t *textureBuffer)
{
	const GLubyte *pImgData = (const GLubyte *)pTexInfo->data;
	INT32 w = pTexInfo->width;
	INT32 h = pTexInfo->height;
	INT32 i, j;

	for (j = 0; j < h; j++)
	{
		for (i = 0; i < w; i++)
		{
			if ((*pImgData == GPU_PATCHES_CHROMAKEY_COLORINDEX) && (pTexInfo->flags & TF_CHROMAKEYED))
			{
				textureBuffer[w*j+i].s.red   = 0;
				textureBuffer[w*j+i].s.green = 0;
				textureBuffer[w*j+i].s.blue  = 0;
				textureBuffer[w*j+i].s.alpha = 0;
				pTexInfo->flags |= TF_TRANSPARENT; // there is a hole in it
			}
			else
			{
				textureBuffer[w*j+i].s.red   = GPUTexturePalette[*pImgData].s.red;
				textureBuffer[w*j+i].s.green = GPUTexturePalette[*pImgData].s.green;
				textureBuffer[w*j+i].s.blue  = GPUTexturePalette[*pImgData].s.blue;
				textureBuffer[w*j+i].s.alpha = GPUTexturePalette[*pImgData].s.alpha;
			}

			pImgData++;

			if (pTexInfo->format == GPU_TEXFMT_AP_88)
			{
				if (!(pTexInfo->flags & TF_CHROMAKEYED))
					textureBuffer[w*j+i].s.alpha = *pImgData;
				pImgData++;
			}
		}
	}
}

// Writes a texture where every pixel is a luminance/alpha pair.
void GLTexture_WriteLuminanceAlpha(HWRTexture_t *pTexInfo, RGBA_t *textureBuffer)
{
	const GLubyte *pImgData = (const GLubyte *)pTexInfo->data;
	INT32 w = pTexInfo->width;
	INT32 h = pTexInfo->height;
	INT32 i, j;

	for (j = 0; j < h; j++)
	{
		for (i = 0; i < w; i++)
		{
			textureBuffer[w*j+i].s.red   = *pImgData;
			textureBuffer[w*j+i].s.green = *pImgData;
			textureBuffer[w*j+i].s.blue  = *pImgData;
			pImgData++;
			textureBuffer[w*j+i].s.alpha = *pImgData;
			pImgData++;
		}
	}
}

// Writes a fade mask texture where every pixel is stored in the alpha channel.
void GLTexture_WriteFadeMaskA(HWRTexture_t *pTexInfo, RGBA_t *textureBuffer)
{
	const GLubyte *pImgData = (const GLubyte *)pTexInfo->data;
	INT32 w = pTexInfo->width;
	INT32 h = pTexInfo->height;
	INT32 i, j;

	for (j = 0; j < h; j++)
	{
		for (i = 0; i < w; i++)
		{
			textureBuffer[w*j+i].s.red   = 255; // 255 because the fade mask is modulated with the screen texture, so alpha affects it while the colours don't
			textureBuffer[w*j+i].s.green = 255;
			textureBuffer[w*j+i].s.blue  = 255;
			textureBuffer[w*j+i].s.alpha = *pImgData;
			pImgData++;
		}
	}
}

// Writes a fade mask texture where every pixel is stored in the red channel.
void GLTexture_WriteFadeMaskR(HWRTexture_t *pTexInfo, RGBA_t *textureBuffer)
{
	const GLubyte *pImgData = (const GLubyte *)pTexInfo->data;
	INT32 w = pTexInfo->width;
	INT32 h = pTexInfo->height;
	INT32 i, j;

	for (j = 0; j < h; j++)
	{
		for (i = 0; i < w; i++)
		{
			textureBuffer[w*j+i].s.red   = *pImgData;
			textureBuffer[w*j+i].s.green = 255;
			textureBuffer[w*j+i].s.blue  = 255;
			textureBuffer[w*j+i].s.alpha = 255;
			pImgData++;
		}
	}
}

// Deletes all textures.
void GLTexture_Flush(void)
{
	while (TexCacheHead)
	{
		FTextureInfo *pTexInfo = TexCacheHead;
		HWRTexture_t *texture = pTexInfo->texture;

		if (pTexInfo->name)
		{
			pglDeleteTextures(1, (GLuint *)&pTexInfo->name);
			pTexInfo->name = 0;
		}

		if (texture)
			texture->downloaded = 0;

		TexCacheHead = pTexInfo->next;
		free(pTexInfo);
	}

	TexCacheTail = TexCacheHead = NULL; // Hurdler: well, TexCacheHead is already NULL
	CurrentTexture = 0;
}

// Binds the current texture to a 1x1 white pixel.
void GLTexture_SetNoTexture(void)
{
	// Disable texture.
	if (CurrentTexture != BlankTexture)
	{
		if (BlankTexture == 0)
		{
			// Generate a 1x1 white pixel for the blank texture
			UINT8 white[4] = {255, 255, 255, 255};
			pglGenTextures(1, &BlankTexture);
			pglBindTexture(GL_TEXTURE_2D, BlankTexture);
			pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
		}
		else
			pglBindTexture(GL_TEXTURE_2D, BlankTexture);

		CurrentTexture = BlankTexture;
	}
}

// Generates a screen texture.
void GLTexture_GenerateScreenTexture(GLuint *name)
{
	INT32 texsize = 2048;
	boolean firstTime = ((*name) == 0);

	// Use a power of two texture, dammit
	if (GPUScreenWidth <= 512)
		texsize = 512;
	else if (GPUScreenWidth <= 1024)
		texsize = 1024;

	// Create screen texture
	if (firstTime)
		pglGenTextures(1, name);
	pglBindTexture(GL_TEXTURE_2D, *name);

	if (firstTime)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		GLState_SetClamp(GL_TEXTURE_WRAP_S);
		GLState_SetClamp(GL_TEXTURE_WRAP_T);
		pglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	CurrentTexture = *name;
}

// Sryder:	This needs to be called whenever the screen changes resolution in order to reset the screen textures to use
//			a new size
void GLTexture_FlushScreenTextures(void)
{
	pglDeleteTextures(1, &ScreenTexture);
	pglDeleteTextures(1, &FinalScreenTexture);
	pglDeleteTextures(1, &WipeStartTexture);
	pglDeleteTextures(1, &WipeEndTexture);

	ScreenTexture = FinalScreenTexture = 0;
	WipeStartTexture = WipeEndTexture = 0;
}

// Initializes mipmapping.
boolean GLTexture_InitMipmapping(void)
{
	if (GLExtension_framebuffer_object)
		pglGenerateMipmap = GLBackend_GetFunction("glGenerateMipmap");

	return (pglGenerateMipmap != NULL);
}

// Returns true if the texture can have mipmaps generated for it.
boolean GLTexture_CanGenerateMipmaps(HWRTexture_t *pTexInfo)
{
	return (MipmappingEnabled && !(pTexInfo->flags & TF_TRANSPARENT));
}

// Calculates total memory usage by textures, excluding mipmaps.
int GLTexture_GetMemoryUsage(FTextureInfo *head)
{
	int res = 0;

	while (head)
	{
		// Figure out the correct bytes-per-pixel for this texture
		// This follows format2bpp in hw_cache.c
		INT32 bpp = 1;
		UINT32 format = head->format;
		if (format == GPU_TEXFMT_RGBA)
			bpp = 4;
		else if (format == GPU_TEXFMT_ALPHA_INTENSITY_88 || format == GPU_TEXFMT_AP_88)
			bpp = 2;

		// Add it up!
		res += head->height*head->width*bpp;
		head = head->next;
	}

	return res;
}

// -----------------+
// GL_DBG_Printf    : Output debug messages to debug log if DEBUG_TO_FILE is defined,
//                  : else do nothing
// Returns          :
// -----------------+

#ifdef DEBUG_TO_FILE
FILE *gllogstream;
#endif

#define VA_DBG_BUF_SIZE 8192

#define VA_DBG_PRINT \
	static char str[VA_DBG_BUF_SIZE] = ""; \
	va_list arglist; \
	va_start(arglist, format); \
	vsnprintf(str, VA_DBG_BUF_SIZE, format, arglist); \
	va_end(arglist); \

#define VA_DBG_LOGFILE \
	if (!gllogstream) \
		gllogstream = fopen("ogllog.txt", "w"); \
	fwrite(str, strlen(str), 1, gllogstream);

void GL_DBG_Printf(const char *format, ...)
{
	VA_DBG_PRINT

#ifdef HAVE_SDL
	CONS_Debug(DBG_RENDER, "%s", str);
#endif

#ifdef DEBUG_TO_FILE
	VA_DBG_LOGFILE
#endif
}

// -----------------+
// GL_MSG_Warning   : Raises a warning.
//                  :
// Returns          :
// -----------------+

void GL_MSG_Warning(const char *format, ...)
{
	VA_DBG_PRINT

#ifdef HAVE_SDL
	CONS_Alert(CONS_WARNING, "%s", str);
#endif

#ifdef DEBUG_TO_FILE
	VA_DBG_LOGFILE
#endif
}

// -----------------+
// GL_MSG_Error     : Raises an error.
//                  :
// Returns          :
// -----------------+

void GL_MSG_Error(const char *format, ...)
{
	VA_DBG_PRINT

#ifdef HAVE_SDL
	CONS_Alert(CONS_ERROR, "%s", str);
#endif

#ifdef DEBUG_TO_FILE
	VA_DBG_LOGFILE
#endif
}

#undef VA_DBG_PRINT
#undef VA_DBG_LOGFILE
#undef VA_DBG_BUF_SIZE
