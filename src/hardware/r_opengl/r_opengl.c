// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1998-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file r_opengl.c
/// \brief OpenGL API for Sonic Robo Blast 2

#if defined (_WIN32)
#define RPC_NO_WINDOWS_H
#include <windows.h>
#endif
#undef GETTEXT
#ifdef __GNUC__
#include <unistd.h>
#endif

#include <stdarg.h>
#include <math.h>
#include "r_opengl.h"
#include "r_vbo.h"

#include "../shaders/gl_shaders.h"

#if defined (HWRENDER) && !defined (NOROPENGL)

static const GLubyte white[4] = { 255, 255, 255, 255 };

// ==========================================================================
//                                                                    GLOBALS
// ==========================================================================

// Hurdler: 04/10/2000: added for the kick ass coronas as Boris wanted;-)
static GLfloat ModelMatrix[16];
static GLfloat ProjectionMatrix[16];
static GLint   SceneViewport[4];

boolean GLBackend_LoadFunctions(void)
{
#ifndef STATIC_OPENGL
	if (!GLBackend_LoadCommonFunctions())
		return false;

	GETOPENGLFUNC(ClearDepth)
	GETOPENGLFUNC(DepthRange)

	GETOPENGLFUNC(Color4ubv)

	GETOPENGLFUNC(VertexPointer)
	GETOPENGLFUNC(NormalPointer)
	GETOPENGLFUNC(TexCoordPointer)
	GETOPENGLFUNC(ColorPointer)
	GETOPENGLFUNC(EnableClientState)
	GETOPENGLFUNC(DisableClientState)

	if (!GLBackend_LoadLegacyFunctions())
		return false;
#endif

	return true;
}

boolean GLBackend_LoadContextFunctions(void)
{
	GLExtension_LoadFunctions();

	if (GLMajorVersion >= 2)
		GETOPENGLFUNCTRY(BlendEquation)

	if (GLTexture_InitMipmapping())
		MipmappingSupported = GL_TRUE;

#ifdef GL_SHADERS
	if (GLExtension_shaders)
		Shader_LoadFunctions();
#endif

	return true;
}

static void GLPerspective(GLfloat fovy, GLfloat aspect)
{
	GLfloat m[4][4] =
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f},
		{ 0.0f, 1.0f, 0.0f, 0.0f},
		{ 0.0f, 0.0f, 1.0f,-1.0f},
		{ 0.0f, 0.0f, 0.0f, 0.0f},
	};
	const GLfloat zNear = NearClippingPlane;
	const GLfloat zFar = FAR_CLIPPING_PLANE;
	const GLfloat radians = (GLfloat)(fovy / 2.0f * M_PIl / 180.0f);
	const GLfloat sine = sin(radians);
	const GLfloat deltaZ = zFar - zNear;
	GLfloat cotangent;

	if ((fabsf((float)deltaZ) < 1.0E-36f) || fpclassify(sine) == FP_ZERO || fpclassify(aspect) == FP_ZERO)
	{
		return;
	}
	cotangent = cosf(radians) / sine;

	m[0][0] = cotangent / aspect;
	m[1][1] = cotangent;
	m[2][2] = -(zFar + zNear) / deltaZ;
	m[3][2] = -2.0f * zNear * zFar / deltaZ;

	pglMultMatrixf(&m[0][0]);
}

static void GLProject(GLfloat objX, GLfloat objY, GLfloat objZ,
                      GLfloat* winX, GLfloat* winY, GLfloat* winZ)
{
	GLfloat in[4], out[4];
	int i;

	for (i=0; i<4; i++)
	{
		out[i] =
			objX * ModelMatrix[0*4+i] +
			objY * ModelMatrix[1*4+i] +
			objZ * ModelMatrix[2*4+i] +
			ModelMatrix[3*4+i];
	}
	for (i=0; i<4; i++)
	{
		in[i] =
			out[0] * ProjectionMatrix[0*4+i] +
			out[1] * ProjectionMatrix[1*4+i] +
			out[2] * ProjectionMatrix[2*4+i] +
			out[3] * ProjectionMatrix[3*4+i];
	}
	if (fpclassify(in[3]) == FP_ZERO) return;
	in[0] /= in[3];
	in[1] /= in[3];
	in[2] /= in[3];
	/* Map x, y and z to range 0-1 */
	in[0] = in[0] * 0.5f + 0.5f;
	in[1] = in[1] * 0.5f + 0.5f;
	in[2] = in[2] * 0.5f + 0.5f;

	/* Map x,y to viewport */
	in[0] = in[0] * SceneViewport[2] + SceneViewport[0];
	in[1] = in[1] * SceneViewport[3] + SceneViewport[1];

	*winX=in[0];
	*winY=in[1];
	*winZ=in[2];
}

// ==========================================================================
//                                                                        API
// ==========================================================================

// -----------------+
// Init             : Initializes the OpenGL interface API
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
#ifdef GL_LIGHT_MODEL_AMBIENT
	GLfloat LightDiffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
#endif

	pglEnable(GL_TEXTURE_2D);	// two-dimensional texturing
	pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	pglEnable(GL_BLEND);		// enable color blending
	pglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	pglEnable(GL_ALPHA_TEST);
	pglAlphaFunc(GL_NOTEQUAL, 0.0f);

	GPU->SetDepthBuffer();

	// Hurdler: not necessary, is it?
	pglShadeModel(GL_SMOOTH);	// iterate vertex colors

	// this sets CurrentPolyFlags to the actual configuration
	CurrentPolyFlags = 0xFFFFFFFF;
	GPU->SetBlend(0);

	CurrentTexture = 0;
	GLTexture_SetNoTexture();

	pglPolygonOffset(-1.0f, -1.0f);

	pglLoadIdentity();
	pglScalef(1.0f, 1.0f, -1.0f);
	pglGetFloatv(GL_MODELVIEW_MATRIX, ModelMatrix); // added for new coronas' code (without depth buffer)

	// Lighting for models
#ifdef GL_LIGHT_MODEL_AMBIENT
	pglLightModelfv(GL_LIGHT_MODEL_AMBIENT, LightDiffuse);
	pglEnable(GL_LIGHT0);
#endif
}

// -----------------+
// SetModelView     : Resets the viewport state
// -----------------+
static void SetModelView(INT32 w, INT32 h)
{
	pglViewport(0, 0, w, h);

	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity();

	pglMatrixMode(GL_MODELVIEW);
	pglLoadIdentity();

	GLPerspective(FIELD_OF_VIEW, ASPECT_RATIO);

	// added for new coronas' code (without depth buffer)
	pglGetIntegerv(GL_VIEWPORT, SceneViewport);
	pglGetFloatv(GL_PROJECTION_MATRIX, ProjectionMatrix);
}

// -----------------+
// ReadRect         : Read a rectangle region of the truecolor framebuffer
//                  : store pixels as 16bit 565 RGB
// Returns          : 16bit 565 RGB pixel array stored in dst_data
// -----------------+
static void ReadRect(INT32 x, INT32 y, INT32 width, INT32 height, INT32 dst_stride, UINT16 * dst_data)
{
	INT32 i;
	if (dst_stride == width*3)
	{
		GLubyte*top = (GLvoid*)dst_data, *bottom = top + dst_stride * (height - 1);
		GLubyte *row = malloc(dst_stride);
		if (!row) return;
		pglPixelStorei(GL_PACK_ALIGNMENT, 1);
		pglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, dst_data);
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
	else
	{
		INT32 j;
		GLubyte *image = malloc(width*height*3*sizeof (*image));
		if (!image) return;
		pglPixelStorei(GL_PACK_ALIGNMENT, 1);
		pglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, image);
		pglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		for (i = height-1; i >= 0; i--)
		{
			for (j = 0; j < width; j++)
			{
				dst_data[(height-1-i)*width+j] =
				(UINT16)(
				                 ((image[(i*width+j)*3]>>3)<<11) |
				                 ((image[(i*width+j)*3+1]>>2)<<5) |
				                 ((image[(i*width+j)*3+2]>>3)));
			}
		}
		free(image);
	}
}

// -----------------+
// GClipRect        : Defines the 2D clipping window
// -----------------+
static void GClipRect(INT32 minx, INT32 miny, INT32 maxx, INT32 maxy, float nearclip)
{
	pglViewport(minx, GPUScreenHeight-maxy, maxx-minx, maxy-miny);
	NearClippingPlane = nearclip;

	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity();
	GLPerspective(FIELD_OF_VIEW, ASPECT_RATIO);
	pglMatrixMode(GL_MODELVIEW);

	// added for new coronas' code (without depth buffer)
	pglGetIntegerv(GL_VIEWPORT, SceneViewport);
	pglGetFloatv(GL_PROJECTION_MATRIX, ProjectionMatrix);
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
static void ClearBuffer(boolean ColorMask, boolean DepthMask, FRGBAFloat *ClearColor)
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
	pglEnableClientState(GL_VERTEX_ARRAY); // We always use this one
	pglEnableClientState(GL_TEXTURE_COORD_ARRAY); // And mostly this one, too
}

// -----------------+
// Draw2DLine       : Render a 2D line
// -----------------+
static void Draw2DLine(F2DCoord *v1, F2DCoord *v2, RGBA_t Color)
{
	GLfloat p[12];
	GLfloat dx, dy;
	GLfloat angle;

	pglDisable(GL_TEXTURE_2D);

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

	pglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	GLState_SetColorUBV((GLubyte*)&Color.s);
	pglVertexPointer(3, GL_FLOAT, 0, p);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	pglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	pglEnable(GL_TEXTURE_2D);
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
	static GLRGBAFloat poly = {0,0,0,0};
	static GLRGBAFloat tint = {0,0,0,0};
	static GLRGBAFloat fade = {0,0,0,0};

	SetBlend(PolyFlags);    //TODO: inline (#pragma..)

	// PolyColor
	if (pSurf)
	{
		// If Modulated, mix the surface colour to the texture
		if (CurrentPolyFlags & PF_Modulated)
		{
			// Poly color
			poly.red    = byte2float(pSurf->PolyColor.s.red);
			poly.green  = byte2float(pSurf->PolyColor.s.green);
			poly.blue   = byte2float(pSurf->PolyColor.s.blue);
			poly.alpha  = byte2float(pSurf->PolyColor.s.alpha);

			GLState_SetColorUBV((GLubyte*)&pSurf->PolyColor.s);
		}

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
	}

	// this test is added for new coronas' code (without depth buffer)
	// I think I should do a separate function for drawing coronas, so it will be a little faster
	if (PolyFlags & PF_Corona) // check to see if we need to draw the corona
	{
		UINT32 i, j;

		//rem: all 8 (or 8.0f) values are hard coded: it can be changed to a higher value
		GLfloat buf[8][8];
		GLfloat cx, cy, cz;
		GLfloat px = 0.0f, py = 0.0f, pz = -1.0f;
		GLfloat scalef = 0.0f;

		GLubyte c[4];

		float alpha;

		cx = (pOutVerts[0].x + pOutVerts[2].x) / 2.0f; // we should change the coronas' ...
		cy = (pOutVerts[0].y + pOutVerts[2].y) / 2.0f; // ... code so its only done once.
		cz = pOutVerts[0].z;

		// I dont know if this is slow or not
		GLProject(cx, cy, cz, &px, &py, &pz);
		//GL_DBG_Printf("Projection: (%f, %f, %f)\n", px, py, pz);

		if ((pz <  0.0l) ||
			(px < -8.0l) ||
			(py < SceneViewport[1]-8.0l) ||
			(px > SceneViewport[2]+8.0l) ||
			(py > SceneViewport[1]+SceneViewport[3]+8.0l))
			return;

		// the damned slow glReadPixels functions :(
		pglReadPixels((INT32)px-4, (INT32)py, 8, 8, GL_DEPTH_COMPONENT, GL_FLOAT, buf);
		//GL_DBG_Printf("DepthBuffer: %f %f\n", buf[0][0], buf[3][3]);

		for (i = 0; i < 8; i++)
			for (j = 0; j < 8; j++)
				scalef += (pz > buf[i][j]+0.00005f) ? 0 : 1;

		// quick test for screen border (not 100% correct, but looks ok)
		if (px < 4) scalef -= (GLfloat)(8*(4-px));
		if (py < SceneViewport[1]+4) scalef -= (GLfloat)(8*(SceneViewport[1]+4-py));
		if (px > SceneViewport[2]-4) scalef -= (GLfloat)(8*(4-(SceneViewport[2]-px)));
		if (py > SceneViewport[1]+SceneViewport[3]-4) scalef -= (GLfloat)(8*(4-(SceneViewport[1]+SceneViewport[3]-py)));

		scalef /= 64;
		//GL_DBG_Printf("Scale factor: %f\n", scalef);

		if (scalef < 0.05f)
			return;

		// GLubyte c[4];
		c[0] = pSurf->PolyColor.s.red;
		c[1] = pSurf->PolyColor.s.green;
		c[2] = pSurf->PolyColor.s.blue;

		alpha = byte2float(pSurf->PolyColor.s.alpha);
		alpha *= scalef; // change the alpha value (it seems better than changing the size of the corona)
		c[3] = (unsigned char)(alpha * 255);
		GLState_SetColorUBV(c);
	}

	Shader_SetUniforms(pSurf, &poly, &tint, &fade);
}

// -----------------+
// DrawPolygon      : Render a polygon, set the texture, set render mode
// -----------------+
static void DrawPolygon(FSurfaceInfo *pSurf, FOutVector *pOutVerts, UINT32 iNumPts, UINT32 PolyFlags)
{
	PreparePolygon(pSurf, pOutVerts, PolyFlags);

	pglVertexPointer(3, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].x);
	pglTexCoordPointer(2, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].s);
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
	PreparePolygon(pSurf, pOutVerts, PolyFlags);

	pglVertexPointer(3, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].x);
	pglTexCoordPointer(2, GL_FLOAT, sizeof(FOutVector), &pOutVerts[0].s);
	pglDrawElements(GL_TRIANGLES, iNumPts, GL_UNSIGNED_INT, IndexArray);

	// the DrawPolygon variant of this has some code about polyflags and wrapping here but havent noticed any problems from omitting it?
}

// -----------------+
// DrawSkyDome      : Renders a sky dome
// -----------------+
static void DrawSkyDome(FSkyDome *sky)
{
	int i, j;

	Shader_SetUniforms(NULL, NULL, NULL, NULL);

	// Build the sky dome! Yes!
	if (sky->rebuild)
	{
		if (GLExtension_vertex_buffer_object)
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
		}

		sky->rebuild = false;
	}

	// bind VBO in order to use
	if (GLExtension_vertex_buffer_object)
		pglBindBuffer(GL_ARRAY_BUFFER, sky->vbo);

	// activate and specify pointers to arrays
	pglVertexPointer(3, GL_FLOAT, sizeof(sky->data[0]), sky_vbo_x);
	pglTexCoordPointer(2, GL_FLOAT, sizeof(sky->data[0]), sky_vbo_u);
	pglColorPointer(4, GL_UNSIGNED_BYTE, sizeof(sky->data[0]), sky_vbo_r);

	// activate color arrays
	pglEnableClientState(GL_COLOR_ARRAY);

	// set transforms
	pglScalef(1.0f, (float)sky->height / 200.0f, 1.0f);
	pglRotatef(270.0f, 0.0f, 1.0f, 0.0f);

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

	pglScalef(1.0f, 1.0f, 1.0f);
	GLState_SetColorUBV(white);

	// bind with 0, so, switch back to normal pointer operation
	if (GLExtension_vertex_buffer_object)
		pglBindBuffer(GL_ARRAY_BUFFER, 0);

	// deactivate color array
	pglDisableClientState(GL_COLOR_ARRAY);
}

static void SetState(INT32 State, INT32 Value)
{
	switch (State)
	{
		case GPU_STATE_SHADERS:
			ShadersAllowed = Value;
			break;

		case GPU_STATE_FRAMEBUFFER:
			FrameBufferEnabled = Value ? GL_TRUE : GL_FALSE;
			break;

		case GPU_STATE_MODEL_LIGHTING:
			ModelLightingEnabled = Value ? GL_TRUE : GL_FALSE;
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
	static GLRGBAFloat poly = {0,0,0,0};
	static GLRGBAFloat tint = {0,0,0,0};
	static GLRGBAFloat fade = {0,0,0,0};

	float pol = 0.0f;
	float scalex, scaley, scalez;

	boolean useTinyFrames;

	boolean useVBO = GLExtension_vertex_buffer_object;

	UINT32 flags;
	int i;

	// Because otherwise, scaling the screen negatively vertically breaks the lighting
	GLfloat LightPos[] = {0.0f, 1.0f, 0.0f, 0.0f};
#ifdef GL_LIGHT_MODEL_AMBIENT
	GLfloat ambient[4];
	GLfloat diffuse[4];
#endif

	// Affect input model scaling
	scale *= 0.5f;
	scalex = scale;
	scaley = scale;
	scalez = scale;

	if (duration != 0 && duration != -1 && tics != -1) // don't interpolate if instantaneous or infinite in length
	{
		UINT32 newtime = (duration - tics); // + 1;

		pol = (newtime)/(float)duration;

		if (pol > 1.0f)
			pol = 1.0f;

		if (pol < 0.0f)
			pol = 0.0f;
	}

	poly.red    = byte2float(Surface->PolyColor.s.red);
	poly.green  = byte2float(Surface->PolyColor.s.green);
	poly.blue   = byte2float(Surface->PolyColor.s.blue);
	poly.alpha  = byte2float(Surface->PolyColor.s.alpha);

#ifdef GL_LIGHT_MODEL_AMBIENT
	if (ModelLightingEnabled)
	{
		if (!ShadersEnabled)
		{
			ambient[0] = poly.red;
			ambient[1] = poly.green;
			ambient[2] = poly.blue;
			ambient[3] = poly.alpha;

			diffuse[0] = poly.red;
			diffuse[1] = poly.green;
			diffuse[2] = poly.blue;
			diffuse[3] = poly.alpha;

			if (ambient[0] > 0.75f)
				ambient[0] = 0.75f;
			if (ambient[1] > 0.75f)
				ambient[1] = 0.75f;
			if (ambient[2] > 0.75f)
				ambient[2] = 0.75f;

			pglLightfv(GL_LIGHT0, GL_POSITION, LightPos);

			pglEnable(GL_LIGHTING);
			pglMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
			pglMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
		}
		pglShadeModel(GL_SMOOTH);
	}
#endif
	else
		GLState_SetColorUBV((GLubyte*)&Surface->PolyColor.s);

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
	Shader_SetUniforms(Surface, &poly, &tint, &fade);

	pglEnable(GL_CULL_FACE);
	pglEnable(GL_NORMALIZE);

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

	pglPushMatrix(); // should be the same as glLoadIdentity
	//Hurdler: now it seems to work
	pglTranslatef(pos->x, pos->z, pos->y);
	if (flipped)
		scaley = -scaley;
	if (hflipped)
		scalez = -scalez;

#ifdef USE_FTRANSFORM_ANGLEZ
	pglRotatef(pos->anglez, 0.0f, 0.0f, -1.0f); // rotate by slope from Kart
#endif
	pglRotatef(pos->angley, 0.0f, -1.0f, 0.0f);
	pglRotatef(pos->anglex, 1.0f, 0.0f, 0.0f);

	if (pos->roll)
	{
		float roll = (1.0f * pos->rollflip);
		pglTranslatef(pos->centerx, pos->centery, 0);
		if (pos->rotaxis == 2) // Z
			pglRotatef(pos->rollangle, 0.0f, 0.0f, roll);
		else if (pos->rotaxis == 1) // Y
			pglRotatef(pos->rollangle, 0.0f, roll, 0.0f);
		else // X
			pglRotatef(pos->rollangle, roll, 0.0f, 0.0f);
		pglTranslatef(-pos->centerx, -pos->centery, 0);
	}

	pglScalef(scalex, scaley, scalez);

	useTinyFrames = model->meshes[0].tinyframes != NULL;

	if (useTinyFrames)
		pglScalef(1 / 64.0f, 1 / 64.0f, 1 / 64.0f);

	// Don't use the VBO if it does not have the correct texture coordinates.
	// (Can happen when model uses a sprite as a texture and the sprite changes)
	// Comparing floats with the != operator here should be okay because they
	// are just copies of glpatches' max_s and max_t values.
	// Instead of the != operator, memcmp is used to avoid a compiler warning.
	if (memcmp(&(model->vbo_max_s), &(model->max_s), sizeof(model->max_s)) != 0 ||
		memcmp(&(model->vbo_max_t), &(model->max_t), sizeof(model->max_t)) != 0)
		useVBO = false;

	pglEnableClientState(GL_NORMAL_ARRAY);

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
					pglVertexPointer(3, GL_SHORT, sizeof(vbotiny_t), BUFFER_OFFSET(0));
					pglNormalPointer(GL_BYTE, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short)*3));
					pglTexCoordPointer(2, GL_FLOAT, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short) * 3 + sizeof(char) * 6));

					pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
					pglBindBuffer(GL_ARRAY_BUFFER, 0);
				}
				else
				{
					pglVertexPointer(3, GL_SHORT, 0, frame->vertices);
					pglNormalPointer(GL_BYTE, 0, frame->normals);
					pglTexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
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

				pglVertexPointer(3, GL_SHORT, 0, vertTinyBuffer);
				pglNormalPointer(GL_BYTE, 0, normTinyBuffer);
				pglTexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
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
					pglVertexPointer(3, GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(0));
					pglNormalPointer(GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 3));
					pglTexCoordPointer(2, GL_FLOAT, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 6));

					pglDrawArrays(GL_TRIANGLES, 0, mesh->numTriangles * 3);
					// No tinyframes, no mesh indices
					pglBindBuffer(GL_ARRAY_BUFFER, 0);
				}
				else
				{
					pglVertexPointer(3, GL_FLOAT, 0, frame->vertices);
					pglNormalPointer(GL_FLOAT, 0, frame->normals);
					pglTexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
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

				pglVertexPointer(3, GL_FLOAT, 0, vertBuffer);
				pglNormalPointer(GL_FLOAT, 0, normBuffer);
				pglTexCoordPointer(2, GL_FLOAT, 0, mesh->uvs);
				pglDrawArrays(GL_TRIANGLES, 0, mesh->numVertices);
			}
		}
	}

	pglDisableClientState(GL_NORMAL_ARRAY);

	pglPopMatrix(); // should be the same as glLoadIdentity
	pglDisable(GL_CULL_FACE);
	pglDisable(GL_NORMALIZE);

#ifdef GL_LIGHT_MODEL_AMBIENT
	if (ModelLightingEnabled)
	{
		if (!ShadersEnabled)
			pglDisable(GL_LIGHTING);
		pglShadeModel(GL_FLAT);
	}
#endif
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

	pglLoadIdentity();

	if (stransform)
	{
		used_fov = stransform->fovxangle;
#ifdef USE_FTRANSFORM_MIRROR
		// mirroring from Kart
		if (stransform->mirror)
			pglScalef(-stransform->scalex, stransform->scaley, -stransform->scalez);
		else
#endif
		if (stransform->flip)
			pglScalef(stransform->scalex, -stransform->scaley, -stransform->scalez);
		else
			pglScalef(stransform->scalex, stransform->scaley, -stransform->scalez);

		if (stransform->roll)
			pglRotatef(stransform->rollangle, 0.0f, 0.0f, 1.0f);
		pglRotatef(stransform->anglex       , 1.0f, 0.0f, 0.0f);
		pglRotatef(stransform->angley+270.0f, 0.0f, 1.0f, 0.0f);
		pglTranslatef(-stransform->x, -stransform->z, -stransform->y);

		special_splitscreen = stransform->splitscreen;
		shearing = stransform->shearing;
	}
	else
	{
		used_fov = FIELD_OF_VIEW;
		pglScalef(1.0f, 1.0f, -1.0f);
	}

	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity();

	// jimita 14042019
	// Simulate Software's y-shearing
	// https://zdoom.org/wiki/Y-shearing
	if (shearing)
	{
		float fdy = stransform->viewaiming * 2;
		if (stransform->flip)
			fdy *= -1.0f;
		pglTranslatef(0.0f, -fdy/BASEVIDHEIGHT, 0.0f);
	}

	if (special_splitscreen)
	{
		used_fov = atan(tan(used_fov*M_PI/360)*0.8)*360/M_PI;
		GLPerspective(used_fov, 2*ASPECT_RATIO);
	}
	else
		GLPerspective(used_fov, ASPECT_RATIO);

	pglGetFloatv(GL_PROJECTION_MATRIX, ProjectionMatrix); // added for new coronas' code (without depth buffer)
	pglMatrixMode(GL_MODELVIEW);

	pglGetFloatv(GL_MODELVIEW_MATRIX, ModelMatrix); // added for new coronas' code (without depth buffer)

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
		-16.0f, -16.0f, 6.0f,
		-16.0f, 16.0f, 6.0f,
		16.0f, 16.0f, 6.0f,
		16.0f, -16.0f, 6.0f
	};

	// Use a power of two texture, dammit
	if (GPUScreenWidth <= 1024)
		texsize = 1024;
	if (GPUScreenWidth <= 512)
		texsize = 512;

	// X/Y stretch fix for all resolutions(!)
	xfix = (float)(texsize)/((float)((GPUScreenWidth)/(float)(GPU_POSTIMGVERTS-1)));
	yfix = (float)(texsize)/((float)((GPUScreenHeight)/(float)(GPU_POSTIMGVERTS-1)));

	pglDisable(GL_DEPTH_TEST);
	pglDisable(GL_BLEND);

	// const float blackBack[16]

	// Draw a black square behind the screen texture,
	// so nothing shows through the edges
	GLState_SetColorUBV(white);

	pglVertexPointer(3, GL_FLOAT, 0, blackBack);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	pglEnableClientState(GL_TEXTURE_COORD_ARRAY);
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

			pglTexCoordPointer(2, GL_FLOAT, 0, stCoords);

			// float vertCoords[12];
			vertCoords[0] = points[x][y][0];
			vertCoords[1] = points[x][y][1];
			vertCoords[2] = 4.4f;
			vertCoords[3] = points[x][y + 1][0];
			vertCoords[4] = points[x][y + 1][1];
			vertCoords[5] = 4.4f;
			vertCoords[6] = points[x + 1][y + 1][0];
			vertCoords[7] = points[x + 1][y + 1][1];
			vertCoords[8] = 4.4f;
			vertCoords[9] = points[x + 1][y][0];
			vertCoords[10] = points[x + 1][y][1];
			vertCoords[11] = 4.4f;

			pglVertexPointer(3, GL_FLOAT, 0, vertCoords);

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

	if (GPUScreenWidth <= 1024)
		texsize = 1024;
	if (GPUScreenWidth <= 512)
		texsize = 512;

	xfix = 1/((float)(texsize)/((float)((GPUScreenWidth))));
	yfix = 1/((float)(texsize)/((float)((GPUScreenHeight))));

	// const float GPU_POSTIMGVERTS[12]

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
	GLState_SetColorUBV(white);

	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	CurrentTexture = ScreenTexture;
}

// Do screen fades!
static void DoScreenWipe(void)
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

	if (!GLExtension_multitexture)
		return;

	// Use a power of two texture, dammit
	if (GPUScreenWidth <= 1024)
		texsize = 1024;
	if (GPUScreenWidth <= 512)
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

	SetBlend(PF_Modulated|PF_NoDepthTest);
	pglEnable(GL_TEXTURE_2D);

	// Draw the original screen
	pglBindTexture(GL_TEXTURE_2D, WipeStartTexture);
	GLState_SetColorUBV(white);
	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	SetBlend(PF_Modulated|PF_Translucent|PF_NoDepthTest);

	// Draw the end screen that fades in
	pglActiveTexture(GL_TEXTURE0);
	pglEnable(GL_TEXTURE_2D);
	pglBindTexture(GL_TEXTURE_2D, WipeEndTexture);
	pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	pglActiveTexture(GL_TEXTURE1);
	pglEnable(GL_TEXTURE_2D);
	pglBindTexture(GL_TEXTURE_2D, fademaskdownloaded);

	pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	// const float defaultST[8]

	pglClientActiveTexture(GL_TEXTURE0);
	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, screenVerts);
	pglClientActiveTexture(GL_TEXTURE1);
	pglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	pglTexCoordPointer(2, GL_FLOAT, 0, defaultST);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	pglDisable(GL_TEXTURE_2D); // disable the texture in the 2nd texture unit
	pglDisableClientState(GL_TEXTURE_COORD_ARRAY);

	pglActiveTexture(GL_TEXTURE0);
	pglClientActiveTexture(GL_TEXTURE0);
	CurrentTexture = WipeEndTexture;
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

static void DrawFinalScreenTexture(INT32 width, INT32 height)
{
	float xfix, yfix;
	float origaspect, newaspect;
	float xoff = 1, yoff = 1; // xoffset and yoffset for the polygon to have black bars around the screen
	FRGBAFloat clearColour;
	INT32 texsize = 2048;

	float off[12];
	float fix[8];

	if (GPUScreenWidth <= 1024)
		texsize = 1024;
	if (GPUScreenWidth <= 512)
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

	GLState_SetColorUBV(white);

	pglTexCoordPointer(2, GL_FLOAT, 0, fix);
	pglVertexPointer(3, GL_FLOAT, 0, off);

	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	CurrentTexture = FinalScreenTexture;
}

static void SetShader(int type)
{
#ifdef GL_SHADERS
	Shader_Set(GLBackend_GetShaderType(type));
#else
	(void)type;
#endif
}

static boolean CompileShaders(void)
{
#ifdef GL_SHADERS
	return Shader_Compile();
#else
	return false;
#endif
}

static void SetShaderInfo(INT32 info, INT32 value)
{
#ifdef GL_SHADERS
	Shader_SetInfo(info, value);
#else
	(void)info;
	(void)value;
#endif
}

static void StoreShader(FShaderProgram *program, UINT32 number, char *shader, size_t size, UINT8 stage)
{
#ifdef GL_SHADERS
	Shader_StoreSource(program, number, shader, size, stage);
#else
	(void)program;
	(void)number;
	(void)shader;
	(void)size;
	(void)stage;
#endif
}

static void StoreCustomShader(FShaderProgram *program, UINT32 number, char *shader, size_t size, UINT8 stage)
{
#ifdef GL_SHADERS
	Shader_StoreCustomSource(program, number, shader, size, stage);
#else
	(void)program;
	(void)number;
	(void)shader;
	(void)size;
	(void)stage;
#endif
}

static void UnSetShader(void)
{
#ifdef GL_SHADERS
	Shader_UnSet();
#endif
}

static void CleanShaders(void)
{
#ifdef GL_SHADERS
	Shader_Clean();
#endif
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
	NULL,
	DrawIntermissionBG,
	DrawFinalScreenTexture,

	PostImgRedraw,

	CompileShaders,
	CleanShaders,
	SetShader,
	UnSetShader,

	SetShaderInfo,
	StoreShader,
	StoreCustomShader,
};

#endif //HWRENDER
