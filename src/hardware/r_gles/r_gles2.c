// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1998-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file r_gles2.c
/// \brief OpenGL ES 2.0 API for Sonic Robo Blast 2

#include <stdarg.h>
#include <math.h>

#include "r_gles.h"
#include "../r_opengl/r_vbo.h"

#include "../shaders/gl_shaders.h"

#include "lzml.h"

#if defined (HWRENDER) && !defined (NOROPENGL)

static GLRGBAFloat white = {1.0f, 1.0f, 1.0f, 1.0f};
static GLRGBAFloat black = {0.0f, 0.0f, 0.0f, 1.0f};

// ==========================================================================
//                                                                  CONSTANTS
// ==========================================================================

#define Deg2Rad(x) ((x) * ((float)M_PIl / 180.0f))

// ==========================================================================
//                                                                    GLOBALS
// ==========================================================================

fmatrix4_t projMatrix;
fmatrix4_t viewMatrix;
fmatrix4_t modelMatrix;

/* Drawing Functions */
typedef void (R_GL_APIENTRY * PFNglEnableVertexAttribArray) (GLuint index);
static PFNglEnableVertexAttribArray pglEnableVertexAttribArray;
typedef void (R_GL_APIENTRY * PFNglDisableVertexAttribArray) (GLuint index);
static PFNglDisableVertexAttribArray pglDisableVertexAttribArray;
typedef void (R_GL_APIENTRY * PFNglVertexAttribPointer) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void * pointer);
static PFNglVertexAttribPointer pglVertexAttribPointer;

boolean GLBackend_LoadFunctions(void)
{
	if (!GLBackend_LoadCommonFunctions())
		return false;

#if defined(__ANDROID__)
	GLBackend_LoadContextFunctions();
	GLBackend_InitShaders();
#endif

	return true;
}

boolean GLBackend_LoadContextFunctions(void)
{
	GETOPENGLFUNC(ActiveTexture)

	GETOPENGLFUNC(GenBuffers)
	GETOPENGLFUNC(BindBuffer)
	GETOPENGLFUNC(BufferData)
	GETOPENGLFUNC(DeleteBuffers)

	GETOPENGLFUNC(EnableVertexAttribArray)
	GETOPENGLFUNC(DisableVertexAttribArray)
	GETOPENGLFUNC(VertexAttribPointer)

	GETOPENGLFUNC(BlendEquation)

	GETOPENGLFUNCALT(ClearDepthf, ClearDepth)
	GETOPENGLFUNCALT(DepthRangef, DepthRange)

	if (GLTexture_InitMipmapping())
		MipmappingSupported = GL_TRUE;

	return true;
}

static void GLPerspective(GLfloat fovy, GLfloat aspect)
{
	fmatrix4_t perspectiveMatrix;
	lzml_matrix4_perspective(perspectiveMatrix, Deg2Rad((float)fovy), (float)aspect, NearClippingPlane, FAR_CLIPPING_PLANE);
	lzml_matrix4_multiply(projMatrix, perspectiveMatrix);
}

// ==========================================================================
//                                                                        API
// ==========================================================================

// -----------------+
// Init             : Initializes the OpenGL ES interface API
// -----------------+
static boolean Init(void)
{
	return GLBackend_Init();
}

// -----------------+
// SetInitialStates : Set permanent states
// -----------------+
static void SetInitialStates(void)
{
	pglEnable(GL_BLEND);
	pglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	pglEnable(GL_DEPTH_TEST);    // check the depth buffer
	pglDepthMask(GL_TRUE);       // enable writing to depth buffer
	GPU->SetDepthBuffer();

	pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	pglAlphaFunc(GL_NOTEQUAL, 0.0f);

	// this sets CurrentPolyFlags to the actual configuration
	CurrentPolyFlags = 0xFFFFFFFF;
	GPU->SetBlend(0);

	if (ShaderState.current)
	{
		pglEnableVertexAttribArray(Shader_AttribLoc(LOC_POSITION));
		pglEnableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD));
	}

	CurrentTexture = 0;
	GLTexture_SetNoTexture();
}

// -----------------+
// SetModelView     : Resets the viewport
// -----------------+
static void SetModelView(INT32 w, INT32 h)
{
	// The screen textures need to be flushed if the width or height change so that they be remade for the correct size
	if (GPUScreenWidth != w || GPUScreenHeight != h)
		GPU->FlushScreenTextures();

	GPUScreenWidth = (GLint)w;
	GPUScreenHeight = (GLint)h;

	pglViewport(0, 0, w, h);

	lzml_matrix4_identity(projMatrix);
	lzml_matrix4_identity(viewMatrix);
	lzml_matrix4_identity(modelMatrix);

	Shader_SetTransform();
}

// -----------------+
// ReadRect         : Read a rectangle region of the truecolor framebuffer
//                  : store pixels as RGBA8888
// Returns          : RGBA8888 pixel array stored in dst_data
// -----------------+
static void ReadRect(INT32 x, INT32 y, INT32 width, INT32 height, INT32 dst_stride, UINT16 * dst_data)
{
	INT32 i;
	GLubyte*top = (GLvoid*)dst_data, *bottom = top + dst_stride * (height - 1);
	GLubyte *row = malloc(dst_stride);
	if (!row) return;
	pglPixelStorei(GL_PACK_ALIGNMENT, 1);
	pglReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dst_data);
	pglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	for(i = 0; i < height/2; i++)
	{
		memcpy(row, top, dst_stride);
		memcpy(top, bottom, dst_stride);
		memcpy(bottom, row, dst_stride);
		top += dst_stride;
		bottom -= dst_stride;
	}
	free(row);
}

// -----------------+
// GClipRect        : Defines the 2D clipping window
// -----------------+
static void GClipRect(INT32 minx, INT32 miny, INT32 maxx, INT32 maxy, float nearclip)
{
	pglViewport(minx, GPUScreenHeight-maxy, maxx-minx, maxy-miny);
	NearClippingPlane = nearclip;

	lzml_matrix4_identity(projMatrix);
	lzml_matrix4_identity(viewMatrix);
	lzml_matrix4_identity(modelMatrix);

	Shader_SetTransform();
}

// -----------------+
// SetBlend         : Set blend mode
// -----------------+
static void SetBlend(UINT32 PolyFlags)
{
	GLState_SetBlend(PolyFlags);
}

// -----------------+
// SetPalette       : Changes the current texture palette
// -----------------+
static void SetPalette(RGBA_t *palette)
{
	GLState_SetPalette(palette);
}

// -----------------+
// SetDepthBuffer   : Set depth buffer state
// -----------------+
static void SetDepthBuffer(void)
{
	GLState_SetDepthBuffer();
}

// -----------------+
// ClearBuffer      : Clear the color/alpha/depth buffer(s)
// -----------------+
static void ClearBuffer(boolean ColorMask, boolean DepthMask, FRGBAFloat * ClearColor)
{
	GLbitfield ClearMask = 0;

	if (ColorMask)
	{
		if (ClearColor)
			pglClearColor(ClearColor->red, ClearColor->green, ClearColor->blue, ClearColor->alpha);
		ClearMask |= GL_COLOR_BUFFER_BIT;
	}

	if (DepthMask)
	{
		SetDepthBuffer();
		ClearMask |= GL_DEPTH_BUFFER_BIT;
	}

	SetBlend(DepthMask ? PF_Occlude | CurrentPolyFlags : CurrentPolyFlags&~PF_Occlude);
	pglClear(ClearMask);

	if (ShaderState.current)
	{
		pglEnableVertexAttribArray(Shader_AttribLoc(LOC_POSITION));
		pglEnableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD));
	}
}

// -----------------+
// Draw2DLine       : Render a 2D line
// -----------------+
static void Draw2DLine(F2DCoord *v1, F2DCoord *v2, RGBA_t Color)
{
	GLfloat p[12];
	GLfloat dx, dy;
	GLfloat angle;

	GLRGBAFloat fcolor = {byte2float(Color.s.red), byte2float(Color.s.green), byte2float(Color.s.blue), byte2float(Color.s.alpha)};

	if (ShaderState.current == NULL)
		return;

	GLTexture_SetNoTexture();

	// This is the preferred, 'modern' way of rendering lines -- creating a polygon.
	if (fabsf(v2->x - v1->x) > FLT_EPSILON)
		angle = (float)atan((v2->y-v1->y)/(v2->x-v1->x));
	else
		angle = (float)N_PI_DEMI;
	dx = (float)sin(angle) / (float)GPUScreenWidth;
	dy = (float)cos(angle) / (float)GPUScreenHeight;

	p[0] = v1->x - dx;  p[1] = -(v1->y + dy); p[2] = 1;
	p[3] = v2->x - dx;  p[4] = -(v2->y + dy); p[5] = 1;
	p[6] = v2->x + dx;  p[7] = -(v2->y - dy); p[8] = 1;
	p[9] = v1->x + dx;  p[10] = -(v1->y - dy); p[11] = 1;

	Shader_SetUniforms(NULL, &fcolor, NULL, NULL);

	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, p);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// -----------------+
// UpdateTexture    : Updates the texture data.
// -----------------+
static void UpdateTexture(HWRTexture_t *pTexInfo)
{
	GLTexture_Update(pTexInfo);
}

// -----------------+
// SetTexture       : Uploads the texture, and sets it as the current one.
// -----------------+
static void SetTexture(HWRTexture_t *pTexInfo)
{
	GLTexture_Set(pTexInfo);
}

// -----------------+
// DeleteTexture    : Deletes a texture from the GPU, and frees its data.
// -----------------+
static void DeleteTexture(HWRTexture_t *pTexInfo)
{
	if (!pTexInfo)
		return;
	else if (pTexInfo->downloaded)
		pglDeleteTextures(1, (GLuint *)&pTexInfo->downloaded);

	GLTexture_Delete(pTexInfo);
	pTexInfo->downloaded = 0;
}

// -----------------+
// ClearTextureCache: Flush OpenGL textures from memory
// -----------------+
static void ClearTextureCache(void)
{
	GLTexture_Flush();
}

// code that is common between DrawPolygon and DrawIndexedTriangles
// the corona thing is there too, i have no idea if that stuff works with DrawIndexedTriangles and batching
static void PreparePolygon(FSurfaceInfo *pSurf, FOutVector *pOutVerts, UINT32 PolyFlags)
{
	GLRGBAFloat poly = {1.0f, 1.0f, 1.0f, 1.0f};
	GLRGBAFloat tint = {1.0f, 1.0f, 1.0f, 1.0f};
	GLRGBAFloat fade = {1.0f, 1.0f, 1.0f, 1.0f};

	GLRGBAFloat *c_poly = NULL, *c_tint = NULL, *c_fade = NULL;

	if (ShaderState.current)
	{
		pglEnableVertexAttribArray(Shader_AttribLoc(LOC_POSITION));
		pglEnableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD));
	}

	(void)pOutVerts;
	if (PolyFlags & PF_Corona)
		PolyFlags &= ~(PF_NoDepthTest|PF_Corona);

	SetBlend(PolyFlags);    //TODO: inline (#pragma..)

	// If Modulated, mix the surface colour to the texture
	if (pSurf && (CurrentPolyFlags & PF_Modulated))
	{
		// Poly color
		poly.red   = byte2float(pSurf->PolyColor.s.red);
		poly.green = byte2float(pSurf->PolyColor.s.green);
		poly.blue  = byte2float(pSurf->PolyColor.s.blue);
		poly.alpha = byte2float(pSurf->PolyColor.s.alpha);

		// Tint color
		tint.red   = byte2float(pSurf->TintColor.s.red);
		tint.green = byte2float(pSurf->TintColor.s.green);
		tint.blue  = byte2float(pSurf->TintColor.s.blue);
		tint.alpha = byte2float(pSurf->TintColor.s.alpha);

		// Fade color
		fade.red   = byte2float(pSurf->FadeColor.s.red);
		fade.green = byte2float(pSurf->FadeColor.s.green);
		fade.blue  = byte2float(pSurf->FadeColor.s.blue);
		fade.alpha = byte2float(pSurf->FadeColor.s.alpha);

		c_poly = &poly;
		c_tint = &tint;
		c_fade = &fade;
	}
	else
		c_poly = &white;

	Shader_SetUniforms(pSurf, c_poly, c_tint, c_fade);
}

// -----------------+
// DrawPolygon      : Render a polygon, set the texture, set render mode
// -----------------+
static void DrawPolygon(FSurfaceInfo *pSurf, FOutVector *pOutVerts, UINT32 iNumPts, UINT32 PolyFlags)
{
	if (ShaderState.current == NULL)
		return;

	PreparePolygon(pSurf, pOutVerts, PolyFlags);

	pglBindBuffer(GL_ARRAY_BUFFER, 0);

	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].x);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].s);

	pglDrawArrays(GL_TRIANGLE_FAN, 0, iNumPts);

	if (PolyFlags & PF_RemoveYWrap)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	if (PolyFlags & PF_ForceWrapX)
		GLState_SetClamp(GL_TEXTURE_WRAP_S);

	if (PolyFlags & PF_ForceWrapY)
		GLState_SetClamp(GL_TEXTURE_WRAP_T);
}

// ---------------------+
// DrawIndexedTriangles : Renders indexed triangles
// ---------------------+
static void DrawIndexedTriangles(FSurfaceInfo *pSurf, FOutVector *pOutVerts, UINT32 iNumPts, UINT32 PolyFlags, UINT32 *IndexArray)
{
	if (ShaderState.current == NULL)
		return;

	PreparePolygon(pSurf, pOutVerts, PolyFlags);

	pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].x);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].s);

	pglDrawElements(GL_TRIANGLES, iNumPts, GL_UNSIGNED_INT, IndexArray);

	// the DrawPolygon variant of this has some code about polyflags and wrapping here but havent noticed any problems from omitting it?
}

// -----------------+
// DrawSkyDome      : Renders a sky dome
// -----------------+
static void DrawSkyDome(FSkyDome *sky)
{
	int i, j;

	fvector3_t scale;
	scale[0] = scale[2] = 1.0f;

	Shader_SetUniforms(NULL, NULL, NULL, NULL);

	// Build the sky dome! Yes!
	if (sky->rebuild)
	{
		// delete VBO when already exists
		if (sky->vbo)
			pglDeleteBuffers(1, &sky->vbo);

		// generate a new VBO and get the associated ID
		pglGenBuffers(1, &sky->vbo);

		// bind VBO in order to use
		pglBindBuffer(GL_ARRAY_BUFFER, sky->vbo);

		// upload data to VBO
		pglBufferData(GL_ARRAY_BUFFER, sky->vertex_count * sizeof(sky->data[0]), sky->data, GL_STATIC_DRAW);

		sky->rebuild = false;
	}

	if (ShaderState.current == NULL)
	{
		pglBindBuffer(GL_ARRAY_BUFFER, 0);
		return;
	}

	// bind VBO in order to use
	pglBindBuffer(GL_ARRAY_BUFFER, sky->vbo);

	// activate and specify pointers to arrays
	pglEnableVertexAttribArray(Shader_AttribLoc(LOC_COLORS));
	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, sizeof(sky->data[0]), sky_vbo_x);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, sizeof(sky->data[0]), sky_vbo_u);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_COLORS), 4, GL_FLOAT, GL_FALSE, sizeof(sky->data[0]), sky_vbo_r);

	// set transforms
	scale[1] = (float)sky->height / 200.0f;
	lzml_matrix4_scale(viewMatrix, scale);
	lzml_matrix4_rotate_y(viewMatrix, Deg2Rad(270.0f));

	Shader_SetTransform();

	for (j = 0; j < 2; j++)
	{
		for (i = 0; i < sky->loopcount; i++)
		{
			FSkyLoopDef *loop = &sky->loops[i];
			unsigned int mode = 0;

			if (j == 0 ? loop->use_texture : !loop->use_texture)
				continue;

			switch (loop->mode)
			{
				case GPU_SKYLOOP_FAN:
					mode = GL_TRIANGLE_FAN;
					break;
				case GPU_SKYLOOP_STRIP:
					mode = GL_TRIANGLE_STRIP;
					break;
				default:
					continue;
			}

			pglDrawArrays(mode, loop->vertexindex, loop->vertexcount);
		}
	}

	pglDisableVertexAttribArray(Shader_AttribLoc(LOC_COLORS));

	// bind with 0, so, switch back to normal pointer operation
	pglBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void SetState(INT32 State, INT32 Value)
{
	switch (State)
	{
		case GPU_STATE_MODEL_LIGHTING:
			ModelLightingEnabled = Value ? GL_TRUE : GL_FALSE;
			break;

		case GPU_STATE_SHADERS:
			ShadersAllowed = Value ? GL_TRUE : GL_FALSE;
			break;

		case GPU_STATE_TEXTUREFILTERMODE:
			if (MipmappingSupported)
			{
				GLState_SetFilterMode(Value);
				GLTexture_Flush(); //??? if we want to change filter mode by texture, remove this
			}
			else
			{
				MipmappingEnabled = GL_FALSE;
				MipmapMinFilter = GL_LINEAR;
			}
			break;

		case GPU_STATE_TEXTUREANISOTROPICMODE:
			if (GLExtension_texture_filter_anisotropic && GPUMaximumAnisotropy)
			{
				AnisotropicFilter = min(Value, GPUMaximumAnisotropy);
				GLTexture_Flush(); //??? if we want to change filter mode by texture, remove this
			}
			break;

		default:
			break;
	}
}

static void CreateModelVBOs(model_t *model)
{
	GLModel_GenerateVBOs(model);
}

#define BUFFER_OFFSET(i) ((void*)(i))

static void DrawModelEx(model_t *model, INT32 frameIndex, INT32 duration, INT32 tics, INT32 nextFrameIndex, FTransform *pos, float scale, UINT8 flipped, UINT8 hflipped, FSurfaceInfo *Surface)
{
	static GLRGBAFloat poly = {1.0f, 1.0f, 1.0f, 1.0f};
	static GLRGBAFloat tint = {1.0f, 1.0f, 1.0f, 1.0f};
	static GLRGBAFloat fade = {1.0f, 1.0f, 1.0f, 1.0f};

	float pol = 0.0f;

	boolean useTinyFrames;

	boolean useVBO = true;

	fvector3_t v_scale;
	fvector3_t translate;

	UINT32 flags;
	int i;

	if (ShaderState.current == NULL)
		return;

	// Affect input model scaling
	scale *= 0.5f;
	v_scale[0] = v_scale[1] = v_scale[2] = scale;

	if (duration != 0 && duration != -1 && tics != -1) // don't interpolate if instantaneous or infinite in length
	{
		UINT32 newtime = (duration - tics); // + 1;

		pol = (newtime)/(float)duration;

		if (pol > 1.0f)
			pol = 1.0f;

		if (pol < 0.0f)
			pol = 0.0f;
	}

	poly.red   = byte2float(Surface->PolyColor.s.red);
	poly.green = byte2float(Surface->PolyColor.s.green);
	poly.blue  = byte2float(Surface->PolyColor.s.blue);
	poly.alpha = byte2float(Surface->PolyColor.s.alpha);

	tint.red   = byte2float(Surface->TintColor.s.red);
	tint.green = byte2float(Surface->TintColor.s.green);
	tint.blue  = byte2float(Surface->TintColor.s.blue);
	tint.alpha = byte2float(Surface->TintColor.s.alpha);

	fade.red   = byte2float(Surface->FadeColor.s.red);
	fade.green = byte2float(Surface->FadeColor.s.green);
	fade.blue  = byte2float(Surface->FadeColor.s.blue);
	fade.alpha = byte2float(Surface->FadeColor.s.alpha);

	flags = (Surface->PolyFlags | PF_Modulated);
	if (Surface->PolyFlags & (PF_Additive|PF_AdditiveSource|PF_Subtractive|PF_ReverseSubtract|PF_Multiplicative))
		flags |= PF_Occlude;
	else if (Surface->PolyColor.s.alpha == 0xFF)
		flags |= (PF_Occlude | PF_Masked);
	SetBlend(flags);

	pglEnableVertexAttribArray(Shader_AttribLoc(LOC_NORMAL));

	Shader_SetUniforms(Surface, &poly, &tint, &fade);

	pglEnable(GL_CULL_FACE);

#ifdef USE_FTRANSFORM_MIRROR
	// flipped is if the object is vertically flipped
	// hflipped is if the object is horizontally flipped
	// pos->flip is if the screen is flipped vertically
	// pos->mirror is if the screen is flipped horizontally
	// XOR all the flips together to figure out what culling to use!
	{
		boolean reversecull = (flipped ^ hflipped ^ pos->flip ^ pos->mirror);
		if (reversecull)
			pglCullFace(GL_FRONT);
		else
			pglCullFace(GL_BACK);
	}
#else
	// pos->flip is if the screen is flipped too
	if (flipped ^ hflipped ^ pos->flip) // If one or three of these are active, but not two, invert the model's culling
	{
		pglCullFace(GL_FRONT);
	}
	else
	{
		pglCullFace(GL_BACK);
	}
#endif

	lzml_matrix4_identity(modelMatrix);

	translate[0] = pos->x;
	translate[1] = pos->z;
	translate[2] = pos->y;
	lzml_matrix4_translate(modelMatrix, translate);

	if (flipped)
		v_scale[1] = -v_scale[1];
	if (hflipped)
		v_scale[2] = -v_scale[2];

	if (pos->roll)
	{
		float roll = (1.0f * pos->rollflip);
		fvector3_t rotate;

		translate[0] = pos->centerx;
		translate[1] = pos->centery;
		translate[2] = 0.0f;
		lzml_matrix4_translate(modelMatrix, translate);

		rotate[0] = rotate[1] = rotate[2] = 0.0f;

		if (pos->rotaxis == 2) // Z
			rotate[2] = roll;
		else if (pos->rotaxis == 1) // Y
			rotate[1] = roll;
		else // X
			rotate[0] = roll;

		lzml_matrix4_rotate_by_vector(modelMatrix, rotate, Deg2Rad(pos->rollangle));

		translate[0] = -translate[0];
		translate[1] = -translate[1];
		lzml_matrix4_translate(modelMatrix, translate);
	}

#ifdef USE_FTRANSFORM_ANGLEZ
	lzml_matrix4_rotate_z(modelMatrix, -Deg2Rad(pos->anglez)); // rotate by slope from Kart
#endif
	lzml_matrix4_rotate_y(modelMatrix, -Deg2Rad(pos->angley));
	lzml_matrix4_rotate_x(modelMatrix, Deg2Rad(pos->anglex));

	lzml_matrix4_scale(modelMatrix, v_scale);

	useTinyFrames = (model->meshes[0].tinyframes != NULL);
	if (useTinyFrames)
	{
		v_scale[0] = v_scale[1] = v_scale[2] = (1 / 64.0f);
		lzml_matrix4_scale(modelMatrix, v_scale);
	}

	// Don't use the VBO if it does not have the correct texture coordinates.
	// (Can happen when model uses a sprite as a texture and the sprite changes)
	// Comparing floats with the != operator here should be okay because they
	// are just copies of glpatches' max_s and max_t values.
	// Instead of the != operator, memcmp is used to avoid a compiler warning.
	if (memcmp(&(model->vbo_max_s), &(model->max_s), sizeof(model->max_s)) != 0 ||
		memcmp(&(model->vbo_max_t), &(model->max_t), sizeof(model->max_t)) != 0)
		useVBO = false;

	Shader_SetTransform();

	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (useTinyFrames)
		{
			tinyframe_t *frame = &mesh->tinyframes[frameIndex % mesh->numFrames];
			tinyframe_t *nextframe = NULL;

			if (nextFrameIndex != -1)
				nextframe = &mesh->tinyframes[nextFrameIndex % mesh->numFrames];

			if (!nextframe || fpclassify(pol) == FP_ZERO)
			{
				if (useVBO)
				{
					pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);

					pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_SHORT, GL_FALSE, sizeof(vbotiny_t), BUFFER_OFFSET(0));
					pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short) * 3 + sizeof(char) * 6));
					pglVertexAttribPointer(Shader_AttribLoc(LOC_NORMAL), 3, GL_BYTE, GL_FALSE, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short)*3));

					pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
					pglBindBuffer(GL_ARRAY_BUFFER, 0);
				}
				else
				{
					pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_SHORT, GL_FALSE, 0, frame->vertices);
					pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, frame->normals);
					pglVertexAttribPointer(Shader_AttribLoc(LOC_NORMAL), 3, GL_BYTE, GL_FALSE, 0, mesh->uvs);

					pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
				}
			}
			else
			{
				short *vertPtr;
				char *normPtr;
				int j = 0;

				// Dangit, I soooo want to do this in a GLSL shader...
				GLModel_AllocLerpTinyBuffer(mesh->numVertices * sizeof(short) * 3);
				vertPtr = vertTinyBuffer;
				normPtr = normTinyBuffer;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = (short)(frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j])));
					*normPtr++ = (char)(frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j])));
				}

				pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_SHORT, GL_FALSE, 0, vertTinyBuffer);
				pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, mesh->uvs);
				pglVertexAttribPointer(Shader_AttribLoc(LOC_NORMAL), 3, GL_BYTE, GL_FALSE, 0, normTinyBuffer);

				pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
			}
		}
		else
		{
			mdlframe_t *frame = &mesh->frames[frameIndex % mesh->numFrames];
			mdlframe_t *nextframe = NULL;

			if (nextFrameIndex != -1)
				nextframe = &mesh->frames[nextFrameIndex % mesh->numFrames];

			if (!nextframe || fpclassify(pol) == FP_ZERO)
			{
				if (useVBO)
				{
					// Zoom! Take advantage of just shoving the entire arrays to the GPU.
					pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);

					pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, sizeof(vbo64_t), BUFFER_OFFSET(0));
					pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 6));
					pglVertexAttribPointer(Shader_AttribLoc(LOC_NORMAL), 3, GL_FLOAT, GL_FALSE, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 3));

					pglDrawArrays(GL_TRIANGLES, 0, mesh->numTriangles * 3);

					// No tinyframes, no mesh indices
					pglBindBuffer(GL_ARRAY_BUFFER, 0);
				}
				else
				{
					pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, frame->vertices);
					pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, frame->normals);
					pglVertexAttribPointer(Shader_AttribLoc(LOC_NORMAL), 3, GL_FLOAT, GL_FALSE, 0, mesh->uvs);

					pglDrawArrays(GL_TRIANGLES, 0, mesh->numTriangles * 3);
				}
			}
			else
			{
				float *vertPtr;
				float *normPtr;
				int j = 0;

				// Dangit, I soooo want to do this in a GLSL shader...
				GLModel_AllocLerpBuffer(mesh->numVertices * sizeof(float) * 3);
				vertPtr = vertBuffer;
				normPtr = normBuffer;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j]));
					*normPtr++ = frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j]));
				}

				pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, vertBuffer);
				pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, mesh->uvs);
				pglVertexAttribPointer(Shader_AttribLoc(LOC_NORMAL), 3, GL_FLOAT, GL_FALSE, 0, normBuffer);

				pglDrawArrays(GL_TRIANGLES, 0, mesh->numVertices);
			}
		}
	}

	lzml_matrix4_identity(modelMatrix);
	Shader_SetTransform();

	pglDisableVertexAttribArray(Shader_AttribLoc(LOC_NORMAL));

	pglDisable(GL_CULL_FACE);
}

// -----------------+
// DrawModel        : Renders a model
// -----------------+
static void DrawModel(model_t *model, INT32 frameIndex, INT32 duration, INT32 tics, INT32 nextFrameIndex, FTransform *pos, float scale, UINT8 flipped, UINT8 hflipped, FSurfaceInfo *Surface)
{
	DrawModelEx(model, frameIndex, duration, tics, nextFrameIndex, pos, scale, flipped, hflipped, Surface);
}

// -----------------+
// SetTransform     :
// -----------------+
static void SetTransform(FTransform *stransform)
{
	static boolean special_splitscreen;
	boolean shearing = false;
	float used_fov;

	fvector3_t scale;

	lzml_matrix4_identity(viewMatrix);
	lzml_matrix4_identity(modelMatrix);

	if (stransform)
	{
		used_fov = stransform->fovxangle;

#ifdef USE_FTRANSFORM_MIRROR
		// mirroring from Kart
		if (stransform->mirror)
		{
			scale[0] = -stransform->scalex;
			scale[1] = -stransform->scaley;
			scale[2] = -stransform->scalez;
		}
		else
#endif
		if (stransform->flip)
		{
			scale[0] = stransform->scalex;
			scale[1] = -stransform->scaley;
			scale[2] = -stransform->scalez;
		}
		else
		{
			scale[0] = stransform->scalex;
			scale[1] = stransform->scaley;
			scale[2] = -stransform->scalez;
		}

		lzml_matrix4_scale(viewMatrix, scale);

		if (stransform->roll)
			lzml_matrix4_rotate_z(viewMatrix, Deg2Rad(stransform->rollangle));
		lzml_matrix4_rotate_x(viewMatrix, Deg2Rad(stransform->anglex));
		lzml_matrix4_rotate_y(viewMatrix, Deg2Rad(stransform->angley + 270.0f));

		lzml_matrix4_translate_x(viewMatrix, -stransform->x);
		lzml_matrix4_translate_y(viewMatrix, -stransform->z);
		lzml_matrix4_translate_z(viewMatrix, -stransform->y);

		special_splitscreen = stransform->splitscreen;
		shearing = stransform->shearing;
	}
	else
		used_fov = FIELD_OF_VIEW;

	lzml_matrix4_identity(projMatrix);

	if (stransform)
	{
		// jimita 14042019
		// Simulate Software's y-shearing
		// https://zdoom.org/wiki/Y-shearing
		if (shearing)
		{
			float fdy = stransform->viewaiming * 2;
			if (stransform->flip)
				fdy *= -1.0f;
			lzml_matrix4_translate_y(projMatrix, (-fdy / BASEVIDHEIGHT));
		}

		if (special_splitscreen)
		{
			used_fov = atan(tan(used_fov*M_PI/360)*0.8)*360/M_PI;
			GLPerspective(used_fov, 2*ASPECT_RATIO);
		}
		else
			GLPerspective(used_fov, ASPECT_RATIO);
	}

	Shader_SetTransform();
}

static INT32 GetTextureUsed(void)
{
	return GLTexture_GetMemoryUsage(TexCacheHead);
}

static void PostImgRedraw(float points[GPU_POSTIMGVERTS][GPU_POSTIMGVERTS][2])
{
	INT32 x, y;
	float float_x, float_y, float_nextx, float_nexty;
	float xfix, yfix;
	INT32 texsize = 2048;

	const float blackBack[16] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	if (ShaderState.current == NULL)
		return;

	// Use a power of two texture, dammit
	if(GPUScreenWidth <= 1024)
		texsize = 1024;
	if(GPUScreenWidth <= 512)
		texsize = 512;

	// X/Y stretch fix for all resolutions(!)
	xfix = (float)(texsize)/((float)((GPUScreenWidth)/(float)(GPU_POSTIMGVERTS-1)));
	yfix = (float)(texsize)/((float)((GPUScreenHeight)/(float)(GPU_POSTIMGVERTS-1)));

	pglDisable(GL_DEPTH_TEST);
	pglDisable(GL_BLEND);

	pglDisableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD));

	// Draw a black square behind the screen texture,
	// so nothing shows through the edges
	Shader_SetUniforms(NULL, &black, NULL, NULL);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, blackBack);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	pglEnableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD));
	Shader_SetUniforms(NULL, &white, NULL, NULL);

	for(x=0;x<GPU_POSTIMGVERTS-1;x++)
	{
		for(y=0;y<GPU_POSTIMGVERTS-1;y++)
		{
			float stCoords[8];
			float vertCoords[12];

			// Used for texture coordinates
			// Annoying magic numbers to scale the square texture to
			// a non-square screen..
			float_x = (float)(x/(xfix));
			float_y = (float)(y/(yfix));
			float_nextx = (float)(x+1)/(xfix);
			float_nexty = (float)(y+1)/(yfix);

			// float stCoords[8];
			stCoords[0] = float_x;
			stCoords[1] = float_y;
			stCoords[2] = float_x;
			stCoords[3] = float_nexty;
			stCoords[4] = float_nextx;
			stCoords[5] = float_nexty;
			stCoords[6] = float_nextx;
			stCoords[7] = float_y;

			pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, stCoords);

			// float vertCoords[12];
			vertCoords[0] = points[x][y][0] / 4.5f;
			vertCoords[1] = points[x][y][1] / 4.5f;
			vertCoords[2] = 1.0f;
			vertCoords[3] = points[x][y + 1][0] / 4.5f;
			vertCoords[4] = points[x][y + 1][1] / 4.5f;
			vertCoords[5] = 1.0f;
			vertCoords[6] = points[x + 1][y + 1][0] / 4.5f;
			vertCoords[7] = points[x + 1][y + 1][1] / 4.5f;
			vertCoords[8] = 1.0f;
			vertCoords[9] = points[x + 1][y][0] / 4.5f;
			vertCoords[10] = points[x + 1][y][1] / 4.5f;
			vertCoords[11] = 1.0f;

			pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, vertCoords);

			pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}
	}

	pglEnable(GL_DEPTH_TEST);
	pglEnable(GL_BLEND);
}

static void FlushScreenTextures(void)
{
	GLTexture_FlushScreenTextures();
}

// Create screen to fade from
static void StartScreenWipe(void)
{
	GLTexture_GenerateScreenTexture(&WipeStartTexture);
}

// Create screen to fade to
static void EndScreenWipe(void)
{
	GLTexture_GenerateScreenTexture(&WipeEndTexture);
}

// Draw the last scene under the intermission
static void DrawIntermissionBG(void)
{
	float xfix, yfix;
	INT32 texsize = 2048;

	const float screenVerts[12] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	float fix[8];

	if (ShaderState.current == NULL)
		return;

	if(GPUScreenWidth <= 1024)
		texsize = 1024;
	if(GPUScreenWidth <= 512)
		texsize = 512;

	xfix = 1/((float)(texsize)/((float)((GPUScreenWidth))));
	yfix = 1/((float)(texsize)/((float)((GPUScreenHeight))));

	// const float screenVerts[12]

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	pglBindTexture(GL_TEXTURE_2D, ScreenTexture);
	Shader_SetUniforms(NULL, &white, NULL, NULL);

	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, screenVerts);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, fix);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	CurrentTexture = ScreenTexture;
}

// Do screen fades!
static void DoWipe(boolean tinted, boolean isfadingin, boolean istowhite)
{
	INT32 texsize = 2048;
	float xfix, yfix;

	INT32 fademaskdownloaded = CurrentTexture; // the fade mask that has been set

	const float screenVerts[12] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	float fix[8];

	const float defaultST[8] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};

	if (ShaderState.current == NULL)
		return;

	// Use a power of two texture, dammit
	if(GPUScreenWidth <= 1024)
		texsize = 1024;
	if(GPUScreenWidth <= 512)
		texsize = 512;

	xfix = 1/((float)(texsize)/((float)((GPUScreenWidth))));
	yfix = 1/((float)(texsize)/((float)((GPUScreenHeight))));

	// const float screenVerts[12]

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	SetBlend(PF_Modulated|PF_Translucent|PF_NoDepthTest);

	Shader_Set(tinted ? SHADER_FADEMASK_ADDITIVEANDSUBTRACTIVE : SHADER_FADEMASK);
	Shader_SetProgram();

	pglDisableVertexAttribArray(Shader_AttribLoc(LOC_COLORS));
	pglEnableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD1));

	Shader_SetSampler(uniform_startscreen, 0);
	Shader_SetSampler(uniform_endscreen, 1);
	Shader_SetSampler(uniform_fademask, 2);

	if (tinted)
	{
		Shader_SetIntegerUniform(uniform_isfadingin, isfadingin);
		Shader_SetIntegerUniform(uniform_istowhite, istowhite);
	}

	Shader_SetUniforms(NULL, &white, NULL, NULL);
	Shader_SetTransform();

	pglActiveTexture(GL_TEXTURE0 + 0);
	pglBindTexture(GL_TEXTURE_2D, WipeStartTexture);
	pglActiveTexture(GL_TEXTURE0 + 1);
	pglBindTexture(GL_TEXTURE_2D, WipeEndTexture);
	pglActiveTexture(GL_TEXTURE0 + 2);
	pglBindTexture(GL_TEXTURE_2D, fademaskdownloaded);

	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, screenVerts);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD0), 2, GL_FLOAT, GL_FALSE, 0, fix);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD1), 2, GL_FLOAT, GL_FALSE, 0, defaultST);

	pglActiveTexture(GL_TEXTURE0);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	pglDisableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD1));

	Shader_UnSet();
	CurrentTexture = WipeEndTexture;
}

static void DoScreenWipe(void)
{
	DoWipe(false, false, false);
}

static void DoTintedWipe(boolean isfadingin, boolean istowhite)
{
	DoWipe(true, isfadingin, istowhite);
}

// Create a texture from the screen.
static void MakeScreenTexture(void)
{
	GLTexture_GenerateScreenTexture(&ScreenTexture);
}

static void MakeScreenFinalTexture(void)
{
	GLTexture_GenerateScreenTexture(&FinalScreenTexture);
}

static void DrawScreenFinalTexture(int width, int height)
{
	float xfix, yfix;
	float origaspect, newaspect;
	float xoff = 1, yoff = 1; // xoffset and yoffset for the polygon to have black bars around the screen
	FRGBAFloat clearColour;
	INT32 texsize = 2048;

	float off[12];
	float fix[8];

	if (ShaderState.current == NULL)
		return;

	if(GPUScreenWidth <= 1024)
		texsize = 1024;
	if(GPUScreenWidth <= 512)
		texsize = 512;

	xfix = 1/((float)(texsize)/((float)((GPUScreenWidth))));
	yfix = 1/((float)(texsize)/((float)((GPUScreenHeight))));

	origaspect = (float)GPUScreenWidth / GPUScreenHeight;
	newaspect = (float)width / height;
	if (origaspect < newaspect)
	{
		xoff = origaspect / newaspect;
		yoff = 1;
	}
	else if (origaspect > newaspect)
	{
		xoff = 1;
		yoff = newaspect / origaspect;
	}

	// float off[12];
	off[0] = -xoff;
	off[1] = -yoff;
	off[2] = 1.0f;
	off[3] = -xoff;
	off[4] = yoff;
	off[5] = 1.0f;
	off[6] = xoff;
	off[7] = yoff;
	off[8] = 1.0f;
	off[9] = xoff;
	off[10] = -yoff;
	off[11] = 1.0f;

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglViewport(0, 0, width, height);

	clearColour.red = clearColour.green = clearColour.blue = 0;
	clearColour.alpha = 1;
	ClearBuffer(true, false, &clearColour);
	pglBindTexture(GL_TEXTURE_2D, FinalScreenTexture);

	Shader_SetUniforms(NULL, &white, NULL, NULL);

	pglBindBuffer(GL_ARRAY_BUFFER, 0);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, off);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, fix);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	CurrentTexture = FinalScreenTexture;
}

static void SetShader(int type)
{
	Shader_Set(GLBackend_GetShaderType(type));
}

static boolean CompileShaders(void)
{
	return Shader_Compile();
}

static void SetShaderInfo(INT32 info, INT32 value)
{
	Shader_SetInfo(info, value);
}

static void LoadCustomShader(int number, char *shader, size_t size, boolean fragment)
{
	Shader_LoadCustom(number, shader, size, fragment);
}

static void UnSetShader(void)
{
	Shader_UnSet();
}

static void CleanShaders(void)
{
	Shader_Clean();
}

struct GPURenderingAPI GLInterfaceAPI = {
	Init,

	SetInitialStates,
	SetModelView,
	SetState,
	SetTransform,
	SetBlend,
	SetPalette,
	SetDepthBuffer,

	DrawPolygon,
	DrawIndexedTriangles,
	Draw2DLine,
	DrawModel,
	DrawSkyDome,

	SetTexture,
	UpdateTexture,
	DeleteTexture,

	ClearTextureCache,
	GetTextureUsed,

	CreateModelVBOs,

	ReadRect,
	GClipRect,
	ClearBuffer,

	MakeScreenTexture,
	MakeScreenFinalTexture,
	FlushScreenTextures,

	StartScreenWipe,
	EndScreenWipe,
	DoScreenWipe,
	DoTintedWipe,
	DrawIntermissionBG,
	DrawScreenFinalTexture,

	PostImgRedraw,

	CompileShaders,
	CleanShaders,
	SetShader,
	UnSetShader,

	SetShaderInfo,
	LoadCustomShader,
};

#endif //HWRENDER
