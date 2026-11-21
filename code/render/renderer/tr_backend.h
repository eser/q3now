// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_TR_BACKEND_H
#define WIRED_TR_BACKEND_H

/*
 * Services the neutral renderer (render/renderer/tr_*.c) needs from whichever backend
 * module it is linked into. The neutral code never names a backend type or global; it
 * calls these, and each backend module defines them (the Vulkan module in
 * ral/backends/vulkan/renderer/vk_backend_hooks.c). The binding is at link time, so a call
 * costs what any direct call costs.
 *
 * A backend that wants to compile the neutral renderer implements this whole list.
 */

#include "../../qcommon/q_shared.h"

struct ralBackend_s;

/* Native rasterizer depth clamp available (RAL caps)? qfalse selects the projection fallback. */
qboolean R_Backend_DepthClampSupported( void );

/* Can the backend clear an attachment inside a pass (fast sky clear)? */
qboolean R_Backend_ClearAttachmentSupported( void );

/* Drop the backend's temporal history store (camera cut, shutdown, resize). */
void R_Backend_TemporalHistoryShutdown( void );

/* The RAL backend this module renders through. */
struct ralBackend_s *R_Backend_Ral( void );

/* World visibility from the backend's culling data. DeriveVisible fills visibleOut from the
 * reached set; qfalse means the backend has no data and the caller walks the BSP itself. */
void     R_Backend_CullCaptureWorld( const void *viewParms, const void *world, int viewCount );
qboolean R_Backend_CullHostDeriveVisible( const cplane_t frustum[4], const vec3_t viewOrigin,
										  const byte *reached, byte *visibleOut, int n );

#endif
