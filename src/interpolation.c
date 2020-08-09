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
///        Referenced from https://easings.net/

#include "interpolation.h"
#include "tables.h" // FixedAngle
#include "doomdef.h" // M_PIl

#include <math.h> // pow

static fixed_t DoInterp(fixed_t start, fixed_t end, fixed_t t)
{
	return FixedMul((FRACUNIT - t), start) + FixedMul(t, end);
}

static fixed_t DoFixedPow(fixed_t x, fixed_t y)
{
	return FloatToFixed(pow(FixedToFloat(x), FixedToFloat(y)));
}

#define INTERPFUNC(type) fixed_t Interp_ ## type (fixed_t start, fixed_t end, fixed_t t)

//
// Linear
//

INTERPFUNC(Linear)
{
	return DoInterp(start, end, t);
}

//
// Sine
//

INTERPFUNC(EaseInSine)
{
	float x = 1.0f - cos((FixedToFloat(t) * M_PIl) / 2);
	return DoInterp(start, end, FloatToFixed(x));
}

INTERPFUNC(EaseOutSine)
{
	float x = sin((FixedToFloat(t) * M_PIl) / 2);
	return DoInterp(start, end, FloatToFixed(x));
}

INTERPFUNC(EaseInOutSine)
{
	float x = -(cos(M_PIl * FixedToFloat(t)) - 1.0f) / 2;
	return DoInterp(start, end, FloatToFixed(x));
}

//
// Quad
//

INTERPFUNC(EaseInQuad)
{
	return DoInterp(start, end, FixedMul(t, t));
}

INTERPFUNC(EaseOutQuad)
{
	return DoInterp(start, end, FRACUNIT - FixedMul(FRACUNIT - t, FRACUNIT - t));
}

INTERPFUNC(EaseInOutQuad)
{
	fixed_t x = t < (FRACUNIT/2)
	? FixedMul(2*FRACUNIT, FixedMul(t, t))
	: FRACUNIT - DoFixedPow(FixedMul(-2*FRACUNIT, t) + 2*FRACUNIT, 2 * FRACUNIT) / 2;
	return DoInterp(start, end, x);
}

//
// Cubic
//

INTERPFUNC(EaseInCubic)
{
	fixed_t x = FixedMul(t, FixedMul(t, t));
	return DoInterp(start, end, x);
}

INTERPFUNC(EaseOutCubic)
{
	return DoInterp(start, end, FRACUNIT - DoFixedPow(FRACUNIT - t, 3 * FRACUNIT));
}

INTERPFUNC(EaseInOutCubic)
{
	fixed_t x = t < (FRACUNIT/2)
	? FixedMul(4*FRACUNIT, FixedMul(t, FixedMul(t, t)))
	: FRACUNIT - DoFixedPow(FixedMul(-2*FRACUNIT, t) + 2*FRACUNIT, 3 * FRACUNIT) / 2;
	return DoInterp(start, end, x);
}

//
// "Quart"
//

INTERPFUNC(EaseInQuart)
{
	fixed_t x = FixedMul(FixedMul(t, t), FixedMul(t, t));
	return DoInterp(start, end, x);
}

INTERPFUNC(EaseOutQuart)
{
	fixed_t x = FRACUNIT - DoFixedPow(FRACUNIT - t, 4 * FRACUNIT);
	return DoInterp(start, end, x);
}

INTERPFUNC(EaseInOutQuart)
{
	fixed_t x = t < (FRACUNIT/2)
	? FixedMul(8*FRACUNIT, FixedMul(FixedMul(t, t), FixedMul(t, t)))
	: FRACUNIT - DoFixedPow(FixedMul(-2*FRACUNIT, t) + 2*FRACUNIT, 4 * FRACUNIT) / 2;
	return DoInterp(start, end, x);
}

//
// "Quint"
//

INTERPFUNC(EaseInQuint)
{
	fixed_t x = FixedMul(t, FixedMul(FixedMul(t, t), FixedMul(t, t)));
	return DoInterp(start, end, x);
}

INTERPFUNC(EaseOutQuint)
{
	fixed_t x = FRACUNIT - DoFixedPow(FRACUNIT - t, 5 * FRACUNIT);
	return DoInterp(start, end, x);
}

INTERPFUNC(EaseInOutQuint)
{
	fixed_t x = t < (FRACUNIT/2)
	? FixedMul(16*FRACUNIT, FixedMul(t, FixedMul(FixedMul(t, t), FixedMul(t, t))))
	: FRACUNIT - DoFixedPow(FixedMul(-2*FRACUNIT, t) + 2*FRACUNIT, 5 * FRACUNIT) / 2;
	return DoInterp(start, end, x);
}

//
// Exponential
//

INTERPFUNC(EaseInExpo)
{
	fixed_t x = (!t) ? 0 : DoFixedPow(2 * FRACUNIT, FixedMul(10 * FRACUNIT, t) - 10 * FRACUNIT);
	return DoInterp(start, end, x);
}

INTERPFUNC(EaseOutExpo)
{
	fixed_t x = (t >= FRACUNIT) ? FRACUNIT
	: FRACUNIT - DoFixedPow(2 * FRACUNIT, FixedMul(-10 * FRACUNIT, t));
	return DoInterp(start, end, x);
}

INTERPFUNC(EaseInOutExpo)
{
	float f = FixedToFloat(t);
	float x = (!t)
	? 0.0f
	: (t >= FRACUNIT)
	? 1.0f
	: t < (FRACUNIT/2) ? pow(2, 20 * f - 10) / 2
	: (2 - pow(2, -20 * f + 10)) / 2;
	return DoInterp(start, end, FloatToFixed(x));
}

//
// "Back"
//

#define EASEBACKCONST1 1.70158
#define EASEBACKCONST2 1.525

INTERPFUNC(EaseInBack)
{
	const float c1 = EASEBACKCONST1;
	const float c3 = c1 + 1;
	float f = FixedToFloat(t);
	fixed_t x = FloatToFixed(c3 * f * f * f - c1 * f * f);
	return DoInterp(start, end, x);
}

INTERPFUNC(EaseOutBack)
{
	const float c1 = EASEBACKCONST1;
	const float c3 = c1 + 1;
	float f = FixedToFloat(t);
	fixed_t x = FloatToFixed(1 + c3 * pow(f - 1, 3) + c1 * pow(f - 1, 2));
	return DoInterp(start, end, x);
}

INTERPFUNC(EaseInOutBack)
{
	const float c1 = EASEBACKCONST1;
	const float c2 = c1 * EASEBACKCONST2;
	float f = FixedToFloat(t);
	float x = f < 0.5
	? (pow(2 * f, 2) * ((c2 + 1) * 2 * f - c2)) / 2
	: (pow(2 * f - 2, 2) * ((c2 + 1) * (f * 2 - 2) + c2) + 2) / 2;
	return DoInterp(start, end, FloatToFixed(x));
}

#undef INTERPFUNC

// Function list

#define INTERPFUNC(type) Interp_ ## type
#define COMMA ,

interpfunc_t interp_funclist[INTERP_MAX] =
{
	INTERPFUNCLIST(COMMA)
};

// Function names

#undef INTERPFUNC
#define INTERPFUNC(type) #type

const char *interp_funcnames[INTERP_MAX] =
{
	INTERPFUNCLIST(COMMA)
};

#undef COMMA
#undef INTERPFUNC
