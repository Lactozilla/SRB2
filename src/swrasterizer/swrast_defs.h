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

#ifndef _SWRAST_DEFS_H_
#define _SWRAST_DEFS_H_

#include "../doomdef.h"
#include "../doomtype.h"
#include "../doomdata.h"
#include "../m_fixed.h"

#define SWRAST_RESOLUTION_X (SWRastState->renderTarget.width)
#define SWRAST_RESOLUTION_Y (SWRastState->renderTarget.height)

#define SWRAST_HALF_RESOLUTION_X (SWRAST_RESOLUTION_X >> 1)
#define SWRAST_HALF_RESOLUTION_Y (SWRAST_RESOLUTION_Y >> 1)

#define SWRAST_MASKDRAWBIT 0x80000000

#ifndef SWRAST_NEAR_CROSS_STRATEGY
	/** Specifies how the library will handle triangles that partially cross the
	near plane. These are problematic and require special handling. Possible
	values:

		0: Strictly cull any triangle crossing the near plane. This will make such triangles disappear.
		This is good for performance or models viewed only from at least small distance.

		1: Forcefully push the vertices crossing near plane in front of it.
		This is a cheap technique that can be good enough for displaying simple environments on slow devices,
		but texturing and geometric artifacts/warps will appear.

		2: Geometrically correct the triangles crossing the near plane.
		This may result in some triangles being subdivided into two and
		is a little more expensive, but the results will be geometrically correct,
		even though barycentric correction is not performed so texturing artifacts will appear.

		3: NOT IMPLEMENTED YET
		Perform both geometrical and barycentric correction of triangle crossing
		the near plane. This is significantly more expensive but results in correct rendering.
	*/

	#define SWRAST_NEAR_CROSS_STRATEGY 0
#endif

#ifndef SWRAST_PERSPECTIVE_CORRECTION
	/** Specifies what type of perspective correction (PC) to use. Remember this
	is an expensive operation! Possible values:

	- 0: No perspective correction. Fastest, inaccurate from most angles.
	- 1: Per-pixel perspective correction, accurate but very expensive.
	- 2: Approximation (computing only at every SWRAST_PC_APPROX_LENGTHth pixel).
	Quake-style approximation is used, which only computes the PC after
	SWRAST_PC_APPROX_LENGTH pixels. This is reasonably accurate and fast. */

	#define SWRAST_PERSPECTIVE_CORRECTION 0
#endif

#ifndef SWRAST_PC_APPROX_LENGTH
	/** For SWRAST_PERSPECTIVE_CORRECTION == 2, this specifies after how many pixels
	PC is recomputed. Should be a power of two to keep up the performance.
	Smaller is nicer but slower. */

	#define SWRAST_PC_APPROX_LENGTH 32
#endif

#if SWRAST_PERSPECTIVE_CORRECTION
#define SWRAST_COMPUTE_DEPTH 1  // PC inevitably computes depth, so enable it
#endif

#ifndef SWRAST_FAST_LERP_QUALITY
	/** Quality (scaling) of SOME (stepped) linear interpolations. 0 will most
	likely be a tiny bit faster, but artifacts can occur for bigger tris, while
	higher values can fix this -- in theory all higher values will have the same
	speed (it is a shift value), but it mustn't be too high to prevent
	overflow. */

	#define SWRAST_FAST_LERP_QUALITY 4
#endif

#ifndef SWRAST_COMPUTE_DEPTH
	/** Whether to compute depth for each pixel (fragment). Some other options
	may turn this on automatically. If you don't need depth information, turning
	this off can save performance. Depth will still be accessible in
	SWRast_Fragment, but will be constant -- equal to center point depth -- over
	the whole triangle. */

	#define SWRAST_COMPUTE_DEPTH 1
#endif

#ifndef SWRAST_Z_BUFFER
	/** What type of z-buffer (depth buffer) to use for visibility determination.
	Possible values:

	- 0: Don't use z-buffer. This saves a lot of memory, but visibility checking
	won't be pixel-accurate and has to mostly be done by other means.

	- 1: Use full fixed-point z-buffer for visibility determination. This is
	the most accurate option (and also a fast one), but requires a big
	amount of memory.

	- 2: Use reduced-size z-buffer (of bytes). This is fast and somewhat
	accurate, but inaccuracies can occur and a considerable amount of memory
	is needed. */

	#define SWRAST_Z_BUFFER 1
#endif

#ifndef SWRAST_REDUCED_Z_BUFFER_GRANULARITY
	/** For SWRAST_Z_BUFFER == 2 this sets the reduced z-buffer granularity. */

	#define SWRAST_REDUCED_Z_BUFFER_GRANULARITY 5
#endif

#ifndef SWRAST_STENCIL_BUFFER
	/** Whether to use stencil buffer for drawing -- with this a pixel that would
	be resterized over an already rasterized pixel (within a frame) will be
	discarded. This is mostly for front-to-back sorted drawing. */

	#define SWRAST_STENCIL_BUFFER 0
#endif

#ifndef SWRAST_NEAR_CLIPPING_PLANE
	/** Distance of the near clipping plane. Points in front or EXACTLY ON this
	plane are considered outside the frustum. This must be >= 0. */

	#define SWRAST_NEAR_CLIPPING_PLANE (FRACUNIT * 2)
#endif

#if SWRAST_NEAR_CLIPPING_PLANE <= 0
#define SWRAST_NEAR_CLIPPING_PLANE 1 // Can't be <= 0.
#endif

#endif
