// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file r_gleslib.h
/// \brief OpenGL ES API libraries

#if defined(__ANDROID__)
	#if defined(HAVE_GLES2)
		#include <GLES2/gl2.h>
		#include <GLES2/gl2ext.h>
	#elif defined(HAVE_GLES)
		#include <GLES/gl.h>
		#include <GLES/glext.h>
	#endif
#elif defined(HAVE_SDL)
	#define _MATH_DEFINES_DEFINED

	#ifdef _MSC_VER
	#pragma warning(disable : 4214 4244)
	#endif

	#if defined(__ANDROID__)
		#if defined(HAVE_GLES2)
			#include "SDL_opengles2.h"
		#elif defined(HAVE_GLES)
			#include "SDL_opengles.h"
		#endif
	#else
		#include "SDL_opengl.h"
	#endif

	#ifdef _MSC_VER
	#pragma warning(default : 4214 4244)
	#endif
#endif
