// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file r_glcommon.h
/// \brief Common OpenGL functions shared by all of the OpenGL backends

#ifndef _R_GLCOMMON_H_
#define _R_GLCOMMON_H_

#define GL_GLEXT_PROTOTYPES

#ifdef HAVE_SDL
	#define _MATH_DEFINES_DEFINED

	#ifdef _MSC_VER
	#pragma warning(disable : 4214 4244)
	#endif

	#include "SDL.h" // For GLExtension_Available
	#include "SDL_opengl.h" // Alam_GBC: Simple, yes?

	#ifdef _MSC_VER
	#pragma warning(default : 4214 4244)
	#endif
#else
	#include <GL/gl.h>
	#include <GL/glu.h>
	#if defined(STATIC_OPENGL)
		#include <GL/glext.h>
	#endif
#endif

#ifdef HAVE_GLES
#include "../r_gles/r_gleslib.h"
#endif

#include "../../doomdata.h"
#include "../../doomtype.h"
#include "../../doomdef.h"

#include "../../z_zone.h"

#include "../../hardware/hw_gpu.h"     // GPURenderingAPI
#include "../../hardware/hw_data.h"    // HWRTexture_t
#include "../../hardware/hw_defs.h"    // FTextureInfo
#include "../../hardware/hw_model.h"   // model_t / mesh_t / mdlframe_t
#include "../../hardware/hw_shaders.h" // HWR_GetShaderName

#include "../r_opengl/r_vbo.h"

void GL_DBG_Printf(const char *format, ...);
void GL_MSG_Warning(const char *format, ...);
void GL_MSG_Error(const char *format, ...);

#ifdef DEBUG_TO_FILE
extern FILE *gllogstream;
#endif

#ifndef R_GL_APIENTRY
	#if defined(_WIN32)
		#define R_GL_APIENTRY APIENTRY
	#else
		#define R_GL_APIENTRY
	#endif
#endif

// ==========================================================================
//                                                                     PROTOS
// ==========================================================================

#ifdef STATIC_OPENGL
/* Miscellaneous */
#define pglClear glClear
#define pglGetFloatv glGetFloatv
#define pglGetIntegerv glGetIntegerv
#define pglGetString glGetString
#define pglClearColor glClearColor
#define pglColorMask glColorMask
#define pglAlphaFunc glAlphaFunc
#define pglBlendFunc glBlendFunc
#define pglCullFace glCullFace
#define pglPolygonOffset glPolygonOffset
#define pglEnable glEnable
#define pglDisable glDisable

/* Depth buffer */
#define pglDepthMask glDepthMask
#define pglDepthRange glDepthRange

/* Transformation */
#define pglViewport glViewport

/* Raster functions */
#define pglPixelStorei glPixelStorei
#define pglReadPixels glReadPixels

/* Texture mapping */
#define pglTexParameteri glTexParameteri
#define pglTexImage2D glTexImage2D
#define pglTexSubImage2D glTexSubImage2D

/* Drawing functions */
#define pglDrawArrays glDrawArrays
#define pglDrawElements glDrawElements

/* Texture objects */
#define pglGenTextures glGenTextures
#define pglDeleteTextures glDeleteTextures
#define pglBindTexture glBindTexture

/* Texture mapping */
#define pglCopyTexImage2D glCopyTexImage2D
#define pglCopyTexSubImage2D glCopyTexSubImage2D

/* 1.3 functions for multitexturing */
#define pglActiveTexture glActiveTexture
#else
/* Miscellaneous */
typedef void (R_GL_APIENTRY * PFNglClear) (GLbitfield mask);
extern PFNglClear pglClear;
typedef void (R_GL_APIENTRY * PFNglGetFloatv) (GLenum pname, GLfloat *params);
extern PFNglGetFloatv pglGetFloatv;
typedef void (R_GL_APIENTRY * PFNglGetIntegerv) (GLenum pname, GLint *params);
extern PFNglGetIntegerv pglGetIntegerv;
typedef const GLubyte * (R_GL_APIENTRY * PFNglGetString) (GLenum name);
extern PFNglGetString pglGetString;
typedef void (R_GL_APIENTRY * PFNglClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
extern PFNglClearColor pglClearColor;
typedef void (R_GL_APIENTRY * PFNglColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
extern PFNglColorMask pglColorMask;
typedef void (R_GL_APIENTRY * PFNglAlphaFunc) (GLenum func, GLclampf ref);
extern PFNglAlphaFunc pglAlphaFunc;
typedef void (R_GL_APIENTRY * PFNglBlendFunc) (GLenum sfactor, GLenum dfactor);
extern PFNglBlendFunc pglBlendFunc;
typedef void (R_GL_APIENTRY * PFNglCullFace) (GLenum mode);
extern PFNglCullFace pglCullFace;
typedef void (R_GL_APIENTRY * PFNglPolygonOffset) (GLfloat factor, GLfloat units);
extern PFNglPolygonOffset pglPolygonOffset;
typedef void (R_GL_APIENTRY * PFNglEnable) (GLenum cap);
extern PFNglEnable pglEnable;
typedef void (R_GL_APIENTRY * PFNglDisable) (GLenum cap);
extern PFNglDisable pglDisable;

/* Depth buffer */
typedef void (R_GL_APIENTRY * PFNglDepthFunc) (GLenum func);
extern PFNglDepthFunc pglDepthFunc;
typedef void (R_GL_APIENTRY * PFNglDepthMask) (GLboolean flag);
extern PFNglDepthMask pglDepthMask;

/* Transformation */
typedef void (R_GL_APIENTRY * PFNglViewport) (GLint x, GLint y, GLsizei width, GLsizei height);
extern PFNglViewport pglViewport;

/* Raster functions */
typedef void (R_GL_APIENTRY * PFNglPixelStorei) (GLenum pname, GLint param);
extern PFNglPixelStorei pglPixelStorei;
typedef void (R_GL_APIENTRY * PFNglReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
extern PFNglReadPixels pglReadPixels;

/* Texture mapping */
typedef void (R_GL_APIENTRY * PFNglTexParameteri) (GLenum target, GLenum pname, GLint param);
extern PFNglTexParameteri pglTexParameteri;
typedef void (R_GL_APIENTRY * PFNglTexImage2D) (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern PFNglTexImage2D pglTexImage2D;
typedef void (R_GL_APIENTRY * PFNglTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
extern PFNglTexSubImage2D pglTexSubImage2D;

/* Drawing functions */
typedef void (R_GL_APIENTRY * PFNglDrawArrays) (GLenum mode, GLint first, GLsizei count);
extern PFNglDrawArrays pglDrawArrays;
typedef void (R_GL_APIENTRY * PFNglDrawElements) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
extern PFNglDrawElements pglDrawElements;

/* Texture objects */
typedef void (R_GL_APIENTRY * PFNglGenTextures) (GLsizei n, const GLuint *textures);
extern PFNglGenTextures pglGenTextures;
typedef void (R_GL_APIENTRY * PFNglDeleteTextures) (GLsizei n, const GLuint *textures);
extern PFNglDeleteTextures pglDeleteTextures;
typedef void (R_GL_APIENTRY * PFNglBindTexture) (GLenum target, GLuint texture);
extern PFNglBindTexture pglBindTexture;

/* Texture mapping */
typedef void (R_GL_APIENTRY * PFNglCopyTexImage2D) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
extern PFNglCopyTexImage2D pglCopyTexImage2D;
typedef void (R_GL_APIENTRY * PFNglCopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
extern PFNglCopyTexSubImage2D pglCopyTexSubImage2D;

/* 1.3 functions for multitexturing */
typedef void (R_GL_APIENTRY * PFNglActiveTexture) (GLenum);
extern PFNglActiveTexture pglActiveTexture;
#endif

//
// Multi-texturing
//

#ifndef HAVE_GLES2
	#ifdef STATIC_OPENGL
		#define pglClientActiveTexture glClientActiveTexture
	#else
		typedef void (R_GL_APIENTRY * PFNglClientActiveTexture) (GLenum);
		extern PFNglClientActiveTexture pglClientActiveTexture;
	#endif
#endif

//
// Mipmapping
//

typedef void (R_GL_APIENTRY * PFNglGenerateMipmap) (GLenum target);
extern PFNglGenerateMipmap pglGenerateMipmap;

//
// Depth functions
//
#ifdef STATIC_OPENGL
	#define pglClearDepth glClearDepth
	#define pglDepthFunc glDepthFunc
#else
	typedef void (R_GL_APIENTRY * PFNglClearDepth) (GLclampd depth);
	extern PFNglClearDepth pglClearDepth;
	typedef void (R_GL_APIENTRY * PFNglDepthRange) (GLclampd near_val, GLclampd far_val);
	extern PFNglDepthRange pglDepthRange;
#endif

typedef void (R_GL_APIENTRY * PFNglClearDepthf) (GLclampf depth);
extern PFNglClearDepthf pglClearDepthf;
typedef void (R_GL_APIENTRY * PFNglDepthRangef) (GLclampf near_val, GLclampf far_val);
extern PFNglDepthRangef pglDepthRangef;

//
// Legacy functions
//

#ifndef HAVE_GLES2
#ifdef STATIC_OPENGL
/* Transformation */
#define pglMatrixMode glMatrixMode
#define pglViewport glViewport
#define pglPushMatrix glPushMatrix
#define pglPopMatrix glPopMatrix
#define pglLoadIdentity glLoadIdentity
#define pglMultMatrixf glMultMatrixf
#define pglRotatef glRotatef
#define pglScalef glScalef
#define pglTranslatef glTranslatef

/* Drawing functions */
#define pglVertexPointer glVertexPointer
#define pglNormalPointer glNormalPointer
#define pglTexCoordPointer glTexCoordPointer
#define pglColorPointer glColorPointer
#define pglEnableClientState glEnableClientState
#define pglDisableClientState glDisableClientState

/* Lighting */
#define pglShadeModel glShadeModel
#define pglLightfv glLightfv
#define pglLightModelfv glLightModelfv
#define pglMaterialfv glMaterialfv

/* Texture mapping */
#define pglTexEnvi glTexEnvi

#else // STATIC_OPENGL

typedef void (R_GL_APIENTRY * PFNglMatrixMode) (GLenum mode);
extern PFNglMatrixMode pglMatrixMode;
typedef void (R_GL_APIENTRY * PFNglPushMatrix) (void);
extern PFNglPushMatrix pglPushMatrix;
typedef void (R_GL_APIENTRY * PFNglPopMatrix) (void);
extern PFNglPopMatrix pglPopMatrix;
typedef void (R_GL_APIENTRY * PFNglLoadIdentity) (void);
extern PFNglLoadIdentity pglLoadIdentity;
typedef void (R_GL_APIENTRY * PFNglMultMatrixf) (const GLfloat *m);
extern PFNglMultMatrixf pglMultMatrixf;
typedef void (R_GL_APIENTRY * PFNglRotatef) (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
extern PFNglRotatef pglRotatef;
typedef void (R_GL_APIENTRY * PFNglScalef) (GLfloat x, GLfloat y, GLfloat z);
extern PFNglScalef pglScalef;
typedef void (R_GL_APIENTRY * PFNglTranslatef) (GLfloat x, GLfloat y, GLfloat z);
extern PFNglTranslatef pglTranslatef;

/* Drawing Functions */
typedef void (R_GL_APIENTRY * PFNglVertexPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
extern PFNglVertexPointer pglVertexPointer;
typedef void (R_GL_APIENTRY * PFNglNormalPointer) (GLenum type, GLsizei stride, const GLvoid *pointer);
extern PFNglNormalPointer pglNormalPointer;
typedef void (R_GL_APIENTRY * PFNglTexCoordPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
extern PFNglTexCoordPointer pglTexCoordPointer;
typedef void (R_GL_APIENTRY * PFNglColorPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
extern PFNglColorPointer pglColorPointer;
typedef void (R_GL_APIENTRY * PFNglEnableClientState) (GLenum cap);
extern PFNglEnableClientState pglEnableClientState;
typedef void (R_GL_APIENTRY * PFNglDisableClientState) (GLenum cap);
extern PFNglDisableClientState pglDisableClientState;

/* Lighting */
typedef void (R_GL_APIENTRY * PFNglShadeModel) (GLenum mode);
extern PFNglShadeModel pglShadeModel;
typedef void (R_GL_APIENTRY * PFNglLightfv) (GLenum light, GLenum pname, GLfloat *params);
extern PFNglLightfv pglLightfv;
typedef void (R_GL_APIENTRY * PFNglLightModelfv) (GLenum pname, GLfloat *params);
extern PFNglLightModelfv pglLightModelfv;
typedef void (R_GL_APIENTRY * PFNglMaterialfv) (GLint face, GLenum pname, GLfloat *params);
extern PFNglMaterialfv pglMaterialfv;

/* Texture mapping */
typedef void (R_GL_APIENTRY * PFNglTexEnvi) (GLenum target, GLenum pname, GLint param);
extern PFNglTexEnvi pglTexEnvi;
#endif
#endif // HAVE_GLES2

// Color
#ifdef STATIC_OPENGL
	#define pglColor4f glColor4f
#else
	typedef void (*PFNglColor4f) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
	extern PFNglColor4f pglColor4f;
#endif

#ifdef STATIC_OPENGL
	#define pglColor4ubv glColor4ubv
#else
	typedef void (R_GL_APIENTRY * PFNglColor4ubv) (const GLubyte *v);
	extern PFNglColor4ubv pglColor4ubv;
#endif

/* 1.5 functions for buffers */
typedef void (R_GL_APIENTRY * PFNglGenBuffers) (GLsizei n, GLuint *buffers);
extern PFNglGenBuffers pglGenBuffers;
typedef void (R_GL_APIENTRY * PFNglBindBuffer) (GLenum target, GLuint buffer);
extern PFNglBindBuffer pglBindBuffer;
typedef void (R_GL_APIENTRY * PFNglBufferData) (GLenum target, GLsizei size, const GLvoid *data, GLenum usage);
extern PFNglBufferData pglBufferData;
typedef void (R_GL_APIENTRY * PFNglDeleteBuffers) (GLsizei n, const GLuint *buffers);
extern PFNglDeleteBuffers pglDeleteBuffers;

/* 2.0 functions */
typedef void (R_GL_APIENTRY * PFNglBlendEquation) (GLenum mode);
extern PFNglBlendEquation pglBlendEquation;

/* 3.0 functions for framebuffers and renderbuffers */
typedef void (R_GL_APIENTRY * PFNglGenFramebuffers) (GLsizei n, GLuint *ids);
extern PFNglGenFramebuffers pglGenFramebuffers;
typedef void (R_GL_APIENTRY * PFNglBindFramebuffer) (GLenum target, GLuint framebuffer);
extern PFNglBindFramebuffer pglBindFramebuffer;
typedef void (R_GL_APIENTRY * PFNglDeleteFramebuffers) (GLsizei n, GLuint *ids);
extern PFNglDeleteFramebuffers pglDeleteFramebuffers;
typedef void (R_GL_APIENTRY * PFNglFramebufferTexture2D) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern PFNglFramebufferTexture2D pglFramebufferTexture2D;
typedef GLenum (R_GL_APIENTRY * PFNglCheckFramebufferStatus) (GLenum target);
extern PFNglCheckFramebufferStatus pglCheckFramebufferStatus;
typedef void (R_GL_APIENTRY * PFNglGenRenderbuffers) (GLsizei n, GLuint *renderbuffers);
extern PFNglGenRenderbuffers pglGenRenderbuffers;
typedef void (R_GL_APIENTRY * PFNglBindRenderbuffer) (GLenum target, GLuint renderbuffer);
extern PFNglBindRenderbuffer pglBindRenderbuffer;
typedef void (R_GL_APIENTRY * PFNglDeleteRenderbuffers) (GLsizei n, GLuint *renderbuffers);
extern PFNglDeleteRenderbuffers pglDeleteRenderbuffers;
typedef void (R_GL_APIENTRY * PFNglRenderbufferStorage) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
extern PFNglRenderbufferStorage pglRenderbufferStorage;
typedef void (R_GL_APIENTRY * PFNglFramebufferRenderbuffer) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLenum renderbuffer);
extern PFNglFramebufferRenderbuffer pglFramebufferRenderbuffer;

// ==========================================================================
//                                                                  FUNCTIONS
// ==========================================================================

boolean GLBackend_Init(void);
boolean GLBackend_InitContext(void);
boolean GLBackend_InitShaders(void);
boolean GLBackend_LoadLibrary(void);
boolean GLBackend_LoadFunctions(void);
boolean GLBackend_LoadContextFunctions(void);
boolean GLBackend_LoadCommonFunctions(void);
boolean GLBackend_LoadLegacyFunctions(void);
void   *GLBackend_GetFunction(const char *proc);
INT32   GLBackend_GetShaderType(INT32 type);

#define GETOPENGLFUNC(func) \
	p ## gl ## func = GLBackend_GetFunction("gl" #func); \
	if (!(p ## gl ## func)) \
	{ \
		GL_MSG_Error("Failed to get OpenGL function %s", #func); \
		return false; \
	}

#define GETOPENGLFUNCALT(func1, func2) \
	p ## gl ## func1 = GLBackend_GetFunction("gl" #func1); \
	if (!(p ## gl ## func1)) \
	{ \
		GL_DBG_Printf("Failed to get OpenGL function %s, trying %s instead\n", #func1, #func2); \
		p ## gl ## func2 = GLBackend_GetFunction("gl" #func2); \
		if (!(p ## gl ## func2)) \
		{ \
			GL_MSG_Error("Failed to get OpenGL function %s\n", #func2); \
			return false; \
		} \
	}

#define GETOPENGLFUNCTRY(func) \
	p ## gl ## func = GLBackend_GetFunction("gl" #func); \
	if (!(p ## gl ## func)) \
		GL_DBG_Printf("Failed to get OpenGL function %s\n", #func);

void GLState_SetSurface(INT32 w, INT32 h);
void GLState_SetPalette(RGBA_t *palette);
void GLState_SetFilterMode(INT32 mode);
void GLState_SetClamp(GLenum pname);
void GLState_SetDepthBuffer(void);
void GLState_SetBlend(UINT32 PolyFlags);
void GLState_SetColor(const GLfloat red, const GLfloat green, const GLfloat blue, const GLfloat alpha);
void GLState_SetColorUBV(const GLubyte *v);

void    GLExtension_Init(void);
boolean GLExtension_Available(const char *extension);
boolean GLExtension_LoadFunctions(void);

void GLTexture_Set(HWRTexture_t *pTexInfo);
void GLTexture_Delete(HWRTexture_t *pTexInfo);

void GLTexture_Update(HWRTexture_t *pTexInfo);
void GLTexture_UploadData(HWRTexture_t *pTexInfo, const GLvoid *pTextureBuffer, GLenum format, boolean update);

void GLTexture_WritePalette(HWRTexture_t *pTexInfo, RGBA_t *textureBuffer);
void GLTexture_WriteLuminanceAlpha(HWRTexture_t *pTexInfo, RGBA_t *textureBuffer);
void GLTexture_WriteFadeMaskA(HWRTexture_t *pTexInfo, RGBA_t *textureBuffer);
void GLTexture_WriteFadeMaskR(HWRTexture_t *pTexInfo, RGBA_t *textureBuffer);

void GLTexture_Flush(void);
void GLTexture_FlushScreenTextures(void);
void GLTexture_SetNoTexture(void);

boolean GLTexture_InitMipmapping(void);
boolean GLTexture_CanGenerateMipmaps(HWRTexture_t *pTexInfo);

void GLTexture_GenerateScreenTexture(GLuint *name);
int  GLTexture_GetMemoryUsage(FTextureInfo *head);

void GLFramebuffer_Generate(void);
void GLFramebuffer_Delete(void);

void GLFramebuffer_GenerateAttachments(void);
void GLFramebuffer_DeleteAttachments(void);

void GLFramebuffer_Enable(void);
void GLFramebuffer_Disable(void);

void GLModel_GenerateVBOs(model_t *model);
void GLModel_AllocLerpBuffer(size_t size);
void GLModel_AllocLerpTinyBuffer(size_t size);

// ==========================================================================
//                                                                  CONSTANTS
// ==========================================================================

#define N_PI_DEMI               (M_PIl/2.0f)
#define ASPECT_RATIO            (1.0f)
#define FIELD_OF_VIEW           90.0f

#define FAR_CLIPPING_PLANE      32768.0f // Draw further! Tails 01-21-2001

// shortcut for ((float)1/i)
#define byte2float(x) (x / 255.0f)

#define NULL_VBO_VERTEX ((FSkyVertex*)NULL)
#define sky_vbo_x (GLExtension_vertex_buffer_object ? &NULL_VBO_VERTEX->x : &sky->data[0].x)
#define sky_vbo_u (GLExtension_vertex_buffer_object ? &NULL_VBO_VERTEX->u : &sky->data[0].u)
#define sky_vbo_r (GLExtension_vertex_buffer_object ? &NULL_VBO_VERTEX->r : &sky->data[0].r)

/* 1.2 Parms */
/* GL_CLAMP_TO_EDGE_EXT */
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_TEXTURE_MIN_LOD
#define GL_TEXTURE_MIN_LOD 0x813A
#endif
#ifndef GL_TEXTURE_MAX_LOD
#define GL_TEXTURE_MAX_LOD 0x813B
#endif

/* 1.3 GL_TEXTUREi */
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif

/* 1.5 Parms */
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif

#ifndef GL_FUNC_ADD
#define GL_FUNC_ADD 0x8006
#endif

#ifndef GL_FUNC_SUBTRACT
#define GL_FUNC_SUBTRACT 0x800A
#endif

#ifndef GL_FUNC_REVERSE_SUBTRACT
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#endif

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

// ==========================================================================
//                                                                    STRUCTS
// ==========================================================================

struct GLRGBAFloat
{
	GLfloat red;
	GLfloat green;
	GLfloat blue;
	GLfloat alpha;
};
typedef struct GLRGBAFloat GLRGBAFloat;

struct FExtensionList
{
	const char *name;
	boolean *extension;
};
typedef struct FExtensionList FExtensionList;

// ==========================================================================
//                                                                    GLOBALS
// ==========================================================================

extern const GLubyte *GLVersion;
extern const GLubyte *GLRenderer;
extern const GLubyte *GLExtensions;

extern GLint GLMajorVersion;
extern GLint GLMinorVersion;

extern struct GPURenderingAPI GLInterfaceAPI;

extern RGBA_t GPUTexturePalette[256];
extern GLint  GPUTextureFormat;
extern GLint  GPUScreenWidth, GPUScreenHeight;
extern GLbyte GPUScreenDepth;
extern GLint  GPUMaximumAnisotropy;

// Linked list of all textures.
extern FTextureInfo *TexCacheTail;
extern FTextureInfo *TexCacheHead;

extern GLuint CurrentTexture;
extern GLuint BlankTexture;

extern GLuint ScreenTexture, FinalScreenTexture;
extern GLuint WipeStartTexture, WipeEndTexture;

extern GLuint FramebufferObject, FramebufferTexture, RenderbufferObject;
extern GLboolean FrameBufferEnabled, RenderToFramebuffer;

extern UINT32 CurrentPolyFlags;

extern GLboolean MipmappingEnabled;
extern GLboolean MipmappingSupported;
extern GLboolean ModelLightingEnabled;

extern GLint MipmapMinFilter;
extern GLint MipmapMagFilter;
extern GLint AnisotropicFilter;

extern float NearClippingPlane;

extern float *vertBuffer;
extern float *normBuffer;
extern short *vertTinyBuffer;
extern char *normTinyBuffer;

#ifdef HAVE_GLES2
typedef float fvector3_t[3];
typedef float fvector4_t[4];
typedef fvector4_t fmatrix4_t[4];

extern fmatrix4_t projMatrix;
extern fmatrix4_t viewMatrix;
extern fmatrix4_t modelMatrix;
#endif

// ==========================================================================
//                                                                 EXTENSIONS
// ==========================================================================

extern boolean GLExtension_multitexture;
extern boolean GLExtension_vertex_buffer_object;
extern boolean GLExtension_texture_filter_anisotropic;
extern boolean GLExtension_vertex_program;
extern boolean GLExtension_fragment_program;
extern boolean GLExtension_framebuffer_object;
extern boolean GLExtension_shaders;

#endif // _R_GLCOMMON_H_
