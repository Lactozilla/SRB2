// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  interpolation.c
/// \brief Interpolation functions

#include "doomtype.h"
#include "m_fixed.h"

typedef enum
{
	INTERP_LINEAR = 0,

	INTERP_EASEINSINE,
	INTERP_EASEOUTSINE,
	INTERP_EASEINOUTSINE,

	INTERP_EASEINQUAD,
	INTERP_EASEOUTQUAD,
	INTERP_EASEINOUTQUAD,

	INTERP_EASEINCUBIC,
	INTERP_EASEOUTCUBIC,
	INTERP_EASEINOUTCUBIC,

	INTERP_EASEINQUART,
	INTERP_EASEOUTQUART,
	INTERP_EASEINOUTQUART,

	INTERP_EASEINQUINT,
	INTERP_EASEOUTQUINT,
	INTERP_EASEINOUTQUINT,

	INTERP_EASEINEXPO,
	INTERP_EASEOUTEXPO,
	INTERP_EASEINOUTEXPO,

	INTERP_EASEINBACK,
	INTERP_EASEOUTBACK,
	INTERP_EASEINOUTBACK,

	INTERP_MAX,
} interpolation_t;

typedef fixed_t (*interpfunc_t)(fixed_t, fixed_t, fixed_t);

extern interpfunc_t interp_funclist[INTERP_MAX];
extern const char *interp_funcnames[INTERP_MAX];

#define INTERPFUNCLIST(sep) \
	INTERPFUNC(Linear) sep \
 \
	INTERPFUNC(EaseInSine) sep \
	INTERPFUNC(EaseOutSine) sep \
	INTERPFUNC(EaseInOutSine) sep \
 \
	INTERPFUNC(EaseInQuad) sep \
	INTERPFUNC(EaseOutQuad) sep \
	INTERPFUNC(EaseInOutQuad) sep \
 \
	INTERPFUNC(EaseInCubic) sep \
	INTERPFUNC(EaseOutCubic) sep \
	INTERPFUNC(EaseInOutCubic) sep \
 \
	INTERPFUNC(EaseInQuart) sep \
	INTERPFUNC(EaseOutQuart) sep \
	INTERPFUNC(EaseInOutQuart) sep \
 \
	INTERPFUNC(EaseInQuint) sep \
	INTERPFUNC(EaseOutQuint) sep \
	INTERPFUNC(EaseInOutQuint) sep \
 \
	INTERPFUNC(EaseInExpo) sep \
	INTERPFUNC(EaseOutExpo) sep \
	INTERPFUNC(EaseInOutExpo) sep \
 \
	INTERPFUNC(EaseInBack) sep \
	INTERPFUNC(EaseOutBack) sep \
	INTERPFUNC(EaseInOutBack) sep

#define INTERPFUNC(type) fixed_t Interp_ ## type (fixed_t start, fixed_t end, fixed_t t);

INTERPFUNCLIST()

#undef INTERPFUNC
