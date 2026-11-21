// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* The Vulkan module's implementation of the services declared in render/renderer/tr_backend.h. */

#include "vk_local.h"
#include "../../../../renderer/tr_backend.h"
#include "vk_ral_textures.h"

qboolean R_Backend_DepthClampSupported( void )
{
	return vk_depth_clamp_supported();
}

qboolean R_Backend_ClearAttachmentSupported( void )
{
	return vk.clearAttachment ? qtrue : qfalse;
}

void R_Backend_TemporalHistoryShutdown( void )
{
	vk_temporal_history_store_shutdown();
}

struct ralBackend_s *R_Backend_Ral( void )
{
	return vk_ral_get_backend();
}

void R_Backend_CullCaptureWorld( const void *viewParms, const void *world, int viewCount )
{
	vk_cull_capture_world( viewParms, world, viewCount );
}

qboolean R_Backend_CullHostDeriveVisible( const cplane_t frustum[4], const vec3_t viewOrigin,
										  const byte *reached, byte *visibleOut, int n )
{
	return vk_cull_host_derive_visible( frustum, viewOrigin, reached, visibleOut, n );
}
