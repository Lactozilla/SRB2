// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file hw_md2.h
/// \brief MD2 Model Handling
///	Inspired from md2.h by Mete Ciragan (mete@swissquake.ch)

#ifndef _HW_MD2_H_
#define _HW_MD2_H_

#include "hw_glob.h"
#include "../r_model.h"

#if defined(_MSC_VER)
#pragma pack()
#endif

boolean HWR_DrawModel(modelinfo_t *md2, gl_vissprite_t *spr);

#define PLAYERMODELPREFIX "PLAYER"

#endif // _HW_MD2_H_
