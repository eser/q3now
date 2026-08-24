// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_OPENGL_INTERNAL_H
#define WIRED_RAL_OPENGL_INTERNAL_H

#include "ral_opengl_core.h"

// Backend-private bridge used by sibling OpenGL adapter units. It keeps the
// native context and GL entry points out of RAL core and render/frontend.
qboolean RalOpenGl_CoreMatchesReceipt( const ralOpenGlCore_t *core,
	const ralOpenGlCoreReceipt_t *receipt );
ralOpenGlProc_t RalOpenGl_CoreResolve( const ralOpenGlCore_t *core,
	const char *name );

#endif
