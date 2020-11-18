// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime Ita Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  m_videoencoder.h
/// \brief Video creation movie mode.

#ifndef __m_videoencoderr_H__
#define __m_videoencoderr_H__

#include "doomdef.h"
#include "command.h"
#include "screen.h"

boolean VideoEncoder_Start(const char *filename);
boolean VideoEncoder_WriteFrame(void);
boolean VideoEncoder_Stop(void);

boolean VideoEncoder_WriteAudio(INT16 *stream, int len);

extern consvar_t cv_videoencoder_bitrate, cv_videoencoder_gopsize;
extern consvar_t cv_videoencoder_downscale;

#endif
