/*
	Simple realtime 3D software rasterization renderer. It is fast, focused on
	resource-limited computers, with no dependencies, using only 32bit integer arithmetic.

	author: Miloslav Ciz
	license: CC0 1.0 (public domain) found at https://creativecommons.org/publicdomain/zero/1.0/ + additional waiver of all IP
	version: 0.852d

	--------------------

	This work's goal is to never be encumbered by any exclusive intellectual
	property rights. The work is therefore provided under CC0 1.0 + additional
	WAIVER OF ALL INTELLECTUAL PROPERTY RIGHTS that waives the rest of
	intellectual property rights not already waived by CC0 1.0. The WAIVER OF ALL
	INTELLECTUAL PROPERTY RGHTS is as follows:

	Each contributor to this work agrees that they waive any exclusive rights,
	including but not limited to copyright, patents, trademark, trade dress,
	industrial design, plant varieties and trade secrets, to any and all ideas,
	concepts, processes, discoveries, improvements and inventions conceived,
	discovered, made, designed, researched or developed by the contributor either
	solely or jointly with others, which relate to this work or result from this
	work. Should any waiver of such right be judged legally invalid or
	ineffective under applicable law, the contributor hereby grants to each
	affected person a royalty-free, non transferable, non sublicensable, non
	exclusive, irrevocable and unconditional license to this right.
*/

#include "swrast.h"

#if SWRAST_Z_BUFFER != 0
static inline boolean SpanZTest(
	INT16 x,
	INT16 y,
	fixed_t depth)
{
	UINT32 index = y * SWRAST_RESOLUTION_X + x;

	depth = SWRast_ZBufferFormat(depth);

#if SWRAST_Z_BUFFER == 2
	// For reduced z-buffer we need equality test, because
	// otherwise pixels at the maximum depth (255) would never be
	// drawn over the background (which also has the depth of 255).
	#define cmp <=
#else
	// For normal z-buffer we leave out equality test to not waste
	// time by drawing over already drawn pixls.
	#define cmp <
#endif

	if (depth cmp SWRAST_DEPTH_BUFFER_POINTER[index])
		return true;

#undef cmp

	return false;
}
#endif

#if SWRAST_STENCIL_BUFFER

#define STENCIL_DECL \
	UINT8 val; \
	UINT32 index = y * SWRAST_RESOLUTION_X + x; \
	UINT32 bit = (index & 0x00000007); \
	index = index >> 3; \
	val = SWRAST_STENCIL_BUFFER_POINTER[index]; \

static inline boolean SpanStencilTest(INT16 x, INT16 y)
{
	STENCIL_DECL

	if ((val >> bit) & 0x1)
		return false;

	return true;
}

static inline void SpanStencilWrite(INT16 x, INT16 y)
{
	STENCIL_DECL

	SWRAST_STENCIL_BUFFER_POINTER[index] = val | (0x1 << bit);
}

#undef STENCIL_DECL

#endif

#define SWRAST_COMPUTE_LERP_DEPTH\
	(SWRAST_COMPUTE_DEPTH && (SWRAST_PERSPECTIVE_CORRECTION == 0))

static inline void InitPixelInfo(SWRast_Fragment *p)
{
	p->x = 0;
	p->y = 0;
	p->barycentric[0] = FRACUNIT;
	p->barycentric[1] = 0;
	p->barycentric[2] = 0;
	p->depth = 0;
	p->previousZ = 0;
}

// Serves to accelerate linear interpolation for performance-critical
// code. Functions such as SWRast_Interpolate require division to compute each
// interpolated value, while SWRast_FastLerpState only requires a division for
// the initiation and a shift for retrieving each interpolated value.

// SWRast_FastLerpState stores a value and a step, both scaled (shifted by
// SWRAST_FAST_LERP_QUALITY) to increase precision. The step is being added to the
// value, which achieves the interpolation. This will only be useful for
// interpolations in which we need to get the interpolated value in every step.

// BEWARE! Shifting a negative value is undefined, so handling shifting of
// negative values has to be done cleverly.

typedef struct
{
	INT32 valueScaled;
	INT32 stepScaled;
} SWRast_FastLerpState;

#define GetFastLerpValue(state)\
	(state.valueScaled >> SWRAST_FAST_LERP_QUALITY)

#define StepFastLerp(state)\
	state.valueScaled += state.stepScaled

// This numerator is a number by which we divide values for the reciprocals.
#if SWRAST_PERSPECTIVE_CORRECTION == 1
	#define Z_RECIP_NUMERATOR (16386 * FRACUNIT)
#elif SWRAST_PERSPECTIVE_CORRECTION == 2
	#define Z_RECIP_NUMERATOR (Z_FRACTIONS_PER_UNIT * Z_FRACTIONS_PER_UNIT)

	#define Z_RECIP_UNIT_BITS 9
	#define Z_RECIP_UNIT_SHIFT (FRACBITS - Z_RECIP_UNIT_BITS)

	#define Z_FRACTIONS_PER_UNIT (1 << Z_RECIP_UNIT_BITS)
	#define Z_RECIP_NUMERATOR (Z_FRACTIONS_PER_UNIT * Z_FRACTIONS_PER_UNIT)
#endif

static inline INT32 ZPCInterpolate(INT32 v1, INT32 v2, INT32 t, INT32 tMax)
{
#if SWRAST_PERSPECTIVE_CORRECTION == 2
	return v1 + ((v2 - v1) * t) / tMax;
#else
	return v1 + FixedDiv(FixedMul((v2 - v1), t), tMax);
#endif
}

static inline INT32 ZPCInterpolateByUnit(INT32 v1, INT32 v2, INT32 t)
{
#if SWRAST_PERSPECTIVE_CORRECTION == 2
	return v1 + ((v2 - v1) * t) / Z_FRACTIONS_PER_UNIT;
#else
	return v1 + FixedMul((v2 - v1), t);
#endif
}

static inline INT32 ZPCInterpolateByUnitFrom0(INT32 v2, INT32 t)
{
#if SWRAST_PERSPECTIVE_CORRECTION == 2
	return (v2 * t) / Z_FRACTIONS_PER_UNIT;
#else
	return FixedMul(v2, t);
#endif
}

static inline INT32 ZPCInterpolateFrom0(INT32 v2, INT32 t, INT32 tMax)
{
#if SWRAST_PERSPECTIVE_CORRECTION == 2
	return (v2 * t) / tMax;
#else
	return FixedDiv(FixedMul(v2, t), tMax);
#endif
}

void SWRast_RasterizeTriangle(SWRast_Vec4 point0, SWRast_Vec4 point1, SWRast_Vec4 point2)
{
	SWRast_Fragment p;

	SWRast_Vec4 *tPointSS, *lPointSS, *rPointSS; // points in Screen Space (in fixed_t, normalized by FRACUNIT)

	fixed_t *barycentric0; // bar. coord that gets higher from L to R
	fixed_t *barycentric1; // bar. coord that gets higher from R to L
	fixed_t *barycentric2; // bar. coord that gets higher from bottom up

	INT16 splitY; // Y of the vertically middle point of the triangle
	INT16 endY; // bottom Y of the whole triangle
	INT32 splitOnLeft; // whether splitY is the y coord. of left or right point

	INT16 currentY;

	INT16
		/* triangle side:
		left     right */
		lX,      rX,       // current x position on the screen
		lDx,     rDx,      // dx (end point - start point)
		lDy,     rDy,      // dy (end point - start point)
		lInc,    rInc,     // direction in which to increment (1 or -1)
		lErr,    rErr,     // current error (Bresenham)
		lErrCmp, rErrCmp,  // helper for deciding comparison (> vs >=)
		lErrAdd, rErrAdd,  // error value to add in each Bresenham cycle
		lErrSub, rErrSub;  // error value to subtract when moving in x direction

	SWRast_FastLerpState lSideFLS, rSideFLS;

#if SWRAST_COMPUTE_LERP_DEPTH
	SWRast_FastLerpState lDepthFLS, rDepthFLS;
#endif

#if SWRAST_PERSPECTIVE_CORRECTION != 0
	fixed_t
		tPointRecipZ, lPointRecipZ, rPointRecipZ, // Reciprocals of the depth of each triangle point.
		lRecip0, lRecip1, rRecip0, rRecip1;       // Helper variables for swapping the above after split.
#endif

	InitPixelInfo(&p);

	// sort the vertices:

	#define assignPoints(t,a,b)\
		{\
			tPointSS = &point##t;\
			barycentric2 = &(p.barycentric[t]);\
			if (SWRast_TriangleWinding(point##t.x,point##t.y,point##a.x,point##a.y,\
				point##b.x,point##b.y) >= 0)\
			{\
				lPointSS = &point##a; rPointSS = &point##b;\
				barycentric0 = &(p.barycentric[b]);\
				barycentric1 = &(p.barycentric[a]);\
			}\
			else\
			{\
				lPointSS = &point##b; rPointSS = &point##a;\
				barycentric0 = &(p.barycentric[a]);\
				barycentric1 = &(p.barycentric[b]);\
			}\
		}

	if (point0.y <= point1.y)
	{
		if (point0.y <= point2.y)
			assignPoints(0,1,2)
		else
			assignPoints(2,0,1)
	}
	else
	{
		if (point1.y <= point2.y)
			assignPoints(1,0,2)
		else
			assignPoints(2,0,1)
	}

	#undef assignPoints

	p.triangleSize[0] = rPointSS->x - lPointSS->x;
	p.triangleSize[1] =
		(rPointSS->y > lPointSS->y ? rPointSS->y : lPointSS->y) - tPointSS->y;

	// now draw the triangle line by line:
	if (rPointSS->y <= lPointSS->y)
	{
		splitY = rPointSS->y;
		splitOnLeft = 0;
		endY = lPointSS->y;
	}
	else
	{
		splitY = lPointSS->y;
		splitOnLeft = 1;
		endY = rPointSS->y;
	}

	currentY = tPointSS->y;

	// We'll be using an algorithm similar to Bresenham line algorithm. The
	// specifics of this algorithm are among others:

	// - drawing possibly NON-CONTINUOUS line
	// - NOT tracing the line exactly, but rather rasterizing one the right
	// side of it, according to the pixel CENTERS, INCLUDING the pixel
	// centers

	// The principle is this:

	// - Move vertically by pixels and accumulate the error (abs(dx/dy)).
	// - If the error is greater than one (crossed the next pixel center), keep
	// moving horizontally and subtracting 1 from the error until it is less
	// than 1 again.
	// - To make this INTEGER ONLY, scale the case so that distance between
	// pixels is equal to dy (instead of 1). This way the error becomes
	// dx/dy * dy == dx, and we're comparing the error to (and potentially
	// subtracting) 1 * dy == dy.

#if SWRAST_COMPUTE_LERP_DEPTH
	#define initDepthFLS(s,p1,p2)\
		s##DepthFLS.valueScaled = p1##PointSS->z << SWRAST_FAST_LERP_QUALITY;\
		s##DepthFLS.stepScaled = ((p2##PointSS->z << SWRAST_FAST_LERP_QUALITY) -\
			s##DepthFLS.valueScaled) / (s##Dy != 0 ? s##Dy : 1);
#else
	#define initDepthFLS(s,p1,p2) ;
#endif

	// init side for the algorithm, params:
	//   s - which side (l or r)
	//   p1 - point from (t, l or r)
	//   p2 - point to (t, l or r)
	//   down - whether the side coordinate goes top-down or vice versa

	#define initSide(s,p1,p2,down)\
		s##X = p1##PointSS->x;\
		s##Dx = p2##PointSS->x - p1##PointSS->x;\
		s##Dy = p2##PointSS->y - p1##PointSS->y;\
		initDepthFLS(s,p1,p2)\
		s##SideFLS.stepScaled = (FRACUNIT << SWRAST_FAST_LERP_QUALITY)\
											/ (s##Dy != 0 ? s##Dy : 1);\
		s##SideFLS.valueScaled = 0;\
		if (!down)\
		{\
			s##SideFLS.valueScaled =\
				FRACUNIT << SWRAST_FAST_LERP_QUALITY;\
			s##SideFLS.stepScaled *= -1;\
		}\
		s##Inc = s##Dx >= 0 ? 1 : -1;\
		if (s##Dx < 0)\
			{s##Err = 0;     s##ErrCmp = 0;}\
		else\
			{s##Err = s##Dy; s##ErrCmp = 1;}\
		s##ErrAdd = SWRast_abs(s##Dx);\
		s##ErrSub = s##Dy != 0 ? s##Dy : 1; // don't allow 0, could lead to an infinite subtracting loop

	#define stepSide(s)\
		while (s##Err - s##Dy >= s##ErrCmp)\
		{\
			s##X += s##Inc;\
			s##Err -= s##ErrSub;\
		}\
		s##Err += s##ErrAdd;

	initSide(r,t,r,1)
	initSide(l,t,l,1)

#if SWRAST_PERSPECTIVE_CORRECTION != 0
	// PC is done by linearly interpolating reciprocals from which the corrected
	// values can be computed. See http://www.lysator.liu.se/~mikaelk/doc/perspectivetexture/
	#if SWRAST_PERSPECTIVE_CORRECTION == 1
		tPointRecipZ = FixedDiv(Z_RECIP_NUMERATOR, SWRast_NonZero(tPointSS->z));
		lPointRecipZ = FixedDiv(Z_RECIP_NUMERATOR, SWRast_NonZero(lPointSS->z));
		rPointRecipZ = FixedDiv(Z_RECIP_NUMERATOR, SWRast_NonZero(rPointSS->z));
	#elif SWRAST_PERSPECTIVE_CORRECTION == 2
		tPointRecipZ = Z_RECIP_NUMERATOR / SWRast_NonZero(tPointSS->z >> Z_RECIP_UNIT_SHIFT);
		lPointRecipZ = Z_RECIP_NUMERATOR / SWRast_NonZero(lPointSS->z >> Z_RECIP_UNIT_SHIFT);
		rPointRecipZ = Z_RECIP_NUMERATOR / SWRast_NonZero(rPointSS->z >> Z_RECIP_UNIT_SHIFT);
	#endif

	lRecip0 = tPointRecipZ;
	lRecip1 = lPointRecipZ;
	rRecip0 = tPointRecipZ;
	rRecip1 = rPointRecipZ;

	#define manageSplitPerspective(b0,b1)\
		b1##Recip0 = b0##PointRecipZ;\
		b1##Recip1 = b1##PointRecipZ;\
		b0##Recip0 = b0##PointRecipZ;\
		b0##Recip1 = tPointRecipZ;
#else
	#define manageSplitPerspective(b0,b1) ;
#endif

	// Clip to the screen in y dimension
	// Clipping above the screen (y < 0) can't be easily done here, will be handled inside the loop.
	endY = SWRast_min(endY, SWRastState->viewWindow[SWRAST_VIEW_WINDOW_Y2]);

	// Draw the triangle from top to bottom.
	// The bottom-most row is left out because, following from the rasterization rules (see start of the file),
	// it is to never be rasterized.
	while (currentY < endY)
	{
		if (currentY == splitY) // reached a vertical split of the triangle?
		{
			fixed_t *tmp;

			#define manageSplit(b0,b1,s0,s1)\
				tmp = barycentric##b0;\
				barycentric##b0 = barycentric##b1;\
				barycentric##b1 = tmp;\
				s0##SideFLS.valueScaled = (FRACUNIT\
					 << SWRAST_FAST_LERP_QUALITY) - s0##SideFLS.valueScaled;\
				s0##SideFLS.stepScaled *= -1;\
				manageSplitPerspective(s0,s1)

			if (splitOnLeft)
			{
				initSide(l,l,r,0);
				manageSplit(0,2,r,l)
			}
			else
			{
				initSide(r,r,l,0);
				manageSplit(1,2,l,r)
			}
		}

		stepSide(r)
		stepSide(l)

		// clipping of pixels whose y < 0 (can't be easily done outside the loop because of the Bresenham-like algorithm steps)
		if (currentY >= SWRastState->viewWindow[SWRAST_VIEW_WINDOW_Y1])
		{
			// draw the horizontal line
			INT16 x, rXClipped, lXClipped;

#if SWRAST_PERSPECTIVE_CORRECTION != 0
			INT16 i;
#endif

#if SWRAST_PERSPECTIVE_CORRECTION == 2
			// These interpolate values between row segments (lines of pixels
			// of SWRAST_PC_APPROX_LENGTH length). After each row segment perspective
			// correction is recomputed.

			SWRast_FastLerpState
				depthPC, // interpolates depth between row segments
				b0PC,    // interpolates barycentric0 between row segments
				b1PC;    // interpolates barycentric1 between row segments

			SINT8 rowCount;
#endif

#if SWRAST_Z_BUFFER != 0
			UINT32 zBufferIndex;
#endif

			fixed_t rowLength = SWRast_NonZero(rX - lX - 1); // prevent zero div

	#if SWRAST_PERSPECTIVE_CORRECTION != 0
			fixed_t lOverZ, lRecipZ, rOverZ, rRecipZ, lT, rT;

			lT = GetFastLerpValue(lSideFLS);
			rT = GetFastLerpValue(rSideFLS);

			lOverZ  = ZPCInterpolateByUnitFrom0(lRecip1,lT);
			lRecipZ = ZPCInterpolateByUnit(lRecip0,lRecip1,lT);

			rOverZ  = ZPCInterpolateByUnitFrom0(rRecip1,rT);
			rRecipZ = ZPCInterpolateByUnit(rRecip0,rRecip1,rT);
	#else
			SWRast_FastLerpState b0FLS, b1FLS;

		#if SWRAST_COMPUTE_LERP_DEPTH
			SWRast_FastLerpState depthFLS;

			depthFLS.valueScaled = lDepthFLS.valueScaled;
			depthFLS.stepScaled = (rDepthFLS.valueScaled - lDepthFLS.valueScaled) / rowLength;
		#endif

			b0FLS.valueScaled = 0;
			b1FLS.valueScaled = lSideFLS.valueScaled;

			b0FLS.stepScaled = rSideFLS.valueScaled / rowLength;
			b1FLS.stepScaled = -1 * lSideFLS.valueScaled / rowLength;
	#endif

			p.y = currentY;

			// clip to the screen in x dimension:
			lXClipped = lX;
			rXClipped = SWRast_min(rX,SWRastState->spanPortalClip[1]);

			if (lXClipped < SWRastState->spanPortalClip[0])
			{
				lXClipped = SWRastState->spanPortalClip[0];

#if !SWRAST_PERSPECTIVE_CORRECTION
				b0FLS.valueScaled -= lX * b0FLS.stepScaled;
				b1FLS.valueScaled -= lX * b1FLS.stepScaled;

	#if SWRAST_COMPUTE_LERP_DEPTH
				depthFLS.valueScaled -= lX * depthFLS.stepScaled;
	#endif
#endif
			}

#if SWRAST_PERSPECTIVE_CORRECTION != 0
			i = lXClipped - lX; // helper var to save one subtraction in the inner loop
#endif

#if SWRAST_PERSPECTIVE_CORRECTION == 2
			depthPC.valueScaled = (Z_RECIP_NUMERATOR / SWRast_NonZero(ZPCInterpolate(lRecipZ, rRecipZ, i, rowLength))) << SWRAST_FAST_LERP_QUALITY;

			b0PC.valueScaled = (ZPCInterpolateFrom0(rOverZ, i, rowLength) * depthPC.valueScaled) / (Z_RECIP_NUMERATOR / Z_FRACTIONS_PER_UNIT);
			b1PC.valueScaled = ((lOverZ - ZPCInterpolateFrom0(lOverZ, i, rowLength)) * depthPC.valueScaled) / (Z_RECIP_NUMERATOR / Z_FRACTIONS_PER_UNIT);

			rowCount = SWRAST_PC_APPROX_LENGTH;
#endif

#if SWRAST_Z_BUFFER != 0
			zBufferIndex = p.y * SWRAST_RESOLUTION_X + lXClipped;
#endif

			// draw the row -- inner loop:
			for (x = lXClipped; x < rXClipped; ++x)
			{
				boolean testsPassed = true;

				if (SWRastState->spanBottomClip && SWRastState->spanTopClip)
				{
					INT16 clipY = p.y - SWRastState->viewWindow[SWRAST_VIEW_WINDOW_Y1];

					if (clipY >= SWRastState->spanBottomClip[x] || clipY <= SWRastState->spanTopClip[x])
						testsPassed = false;
				}

#if SWRAST_STENCIL_BUFFER
				if (testsPassed && (SWRast_GetFragmentRead() & SWRAST_READ_STENCIL) && !SpanStencilTest(x,p.y))
					testsPassed = false;
#endif
				p.x = x;

#if SWRAST_COMPUTE_DEPTH
	#if SWRAST_PERSPECTIVE_CORRECTION == 1
				p.depth = FixedDiv(Z_RECIP_NUMERATOR, SWRast_NonZero(SWRast_Interpolate(lRecipZ, rRecipZ, i, rowLength)));
	#elif SWRAST_PERSPECTIVE_CORRECTION == 2
				if (rowCount >= SWRAST_PC_APPROX_LENGTH)
				{
					// init the linear interpolation to the next PC correct value
					INT32 nextI = i + SWRAST_PC_APPROX_LENGTH;

					rowCount = 0;

					if (nextI < rowLength)
					{
						INT32 nextDepthScaled = (Z_RECIP_NUMERATOR / SWRast_NonZero(ZPCInterpolate(lRecipZ, rRecipZ, nextI, rowLength))) << SWRAST_FAST_LERP_QUALITY, nextValue;

						depthPC.stepScaled = (nextDepthScaled - depthPC.valueScaled) / SWRAST_PC_APPROX_LENGTH;

						nextValue = (ZPCInterpolateFrom0(rOverZ, nextI, rowLength) * nextDepthScaled) / (Z_RECIP_NUMERATOR / Z_FRACTIONS_PER_UNIT);
						b0PC.stepScaled = (nextValue - b0PC.valueScaled) / SWRAST_PC_APPROX_LENGTH;

						nextValue = ((lOverZ - ZPCInterpolateFrom0(lOverZ, nextI, rowLength)) * nextDepthScaled) / (Z_RECIP_NUMERATOR / Z_FRACTIONS_PER_UNIT);
						b1PC.stepScaled = (nextValue - b1PC.valueScaled) / SWRAST_PC_APPROX_LENGTH;
					}
					else
					{
						// A special case where we'd be interpolating outside the triangle.
						// It seems like a valid approach at first, but it creates a bug
						// in a case when the rasterized triangle is near screen 0 and can
						// actually never reach the extrapolated screen position. So we
						// have to clamp to the actual end of the triangle here.
						fixed_t maxI = SWRast_NonZero(rowLength - i);
						fixed_t nextValue, nextDepthScaled = (Z_RECIP_NUMERATOR / SWRast_NonZero(rRecipZ)) << SWRAST_FAST_LERP_QUALITY;

						depthPC.stepScaled = (nextDepthScaled - depthPC.valueScaled) / maxI;

						nextValue = (rOverZ * nextDepthScaled) / (Z_RECIP_NUMERATOR / Z_FRACTIONS_PER_UNIT);

						b0PC.stepScaled = (nextValue - b0PC.valueScaled) / maxI;
						b1PC.stepScaled = -1 * b1PC.valueScaled / maxI;
					}
				}

				p.depth = GetFastLerpValue(depthPC)<<Z_RECIP_UNIT_SHIFT;
	#else
				p.depth = GetFastLerpValue(depthFLS);
				StepFastLerp(depthFLS);
	#endif
#else // !SWRAST_COMPUTE_DEPTH
				p.depth = (tPointSS->z + lPointSS->z + rPointSS->z) / 3;
#endif

#if SWRAST_Z_BUFFER != 0
				p.previousZ = SWRAST_DEPTH_BUFFER_POINTER[zBufferIndex];

				if (testsPassed && (SWRast_GetFragmentRead() & SWRAST_READ_DEPTH) && !SpanZTest(p.x,p.y,p.depth))
					testsPassed = false;
#endif

				if (testsPassed)
				{
					UINT8 status;

	#if SWRAST_PERSPECTIVE_CORRECTION == 0
					*barycentric0 = GetFastLerpValue(b0FLS);
					*barycentric1 = GetFastLerpValue(b1FLS);
	#elif SWRAST_PERSPECTIVE_CORRECTION == 1
					*barycentric0 = FixedDiv(FixedMul(SWRast_InterpolateFrom0(rOverZ, i, rowLength), p.depth), Z_RECIP_NUMERATOR);
					*barycentric1 = FixedDiv(FixedMul(lOverZ - SWRast_InterpolateFrom0(lOverZ, i, rowLength), p.depth), Z_RECIP_NUMERATOR);
	#elif SWRAST_PERSPECTIVE_CORRECTION == 2
					*barycentric0 = GetFastLerpValue(b0PC);
					*barycentric1 = GetFastLerpValue(b1PC);
	#endif

					*barycentric2 = FRACUNIT - *barycentric0 - *barycentric1;

					status = SWRastState->fragmentShader(&p);

					if (status)
					{
#if SWRAST_Z_BUFFER != 0
						if ((status & SWRAST_FRAGMENT_SET_DEPTH) && (SWRast_GetFragmentWrite() & SWRAST_WRITE_DEPTH))
							SWRAST_DEPTH_BUFFER_POINTER[zBufferIndex] = p.depth;
#endif

#if SWRAST_STENCIL_BUFFER
						if (SWRast_GetFragmentWrite() & SWRAST_WRITE_STENCIL)
							SpanStencilWrite(x,p.y);
#endif
					}
				} // tests passed

#if SWRAST_Z_BUFFER != 0
				zBufferIndex++;
#endif

	#if SWRAST_PERSPECTIVE_CORRECTION != 0
				i++;
		#if SWRAST_PERSPECTIVE_CORRECTION == 2
				rowCount++;

				StepFastLerp(depthPC);
				StepFastLerp(b0PC);
				StepFastLerp(b1PC);
		#endif
	#else
				StepFastLerp(b0FLS);
				StepFastLerp(b1FLS);
	#endif
			} // inner loop
		} // y clipping

		StepFastLerp(lSideFLS);
		StepFastLerp(rSideFLS);

	#if SWRAST_COMPUTE_LERP_DEPTH
		StepFastLerp(lDepthFLS);
		StepFastLerp(rDepthFLS);
	#endif

		++currentY;
	} // row drawing

	#undef manageSplit
	#undef initPC
	#undef initSide
	#undef stepSide

	#if SWRAST_PERSPECTIVE_CORRECTION != 0
		#undef Z_RECIP_NUMERATOR
	#endif
}

#if SWRAST_PERSPECTIVE_CORRECTION == 2
	#undef Z_FRACTIONS_PER_UNIT
	#undef Z_RECIP_UNIT_SHIFT
	#undef Z_RECIP_UNIT_BITS
#endif
