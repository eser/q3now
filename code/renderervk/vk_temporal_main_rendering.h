// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_VK_TEMPORAL_MAIN_RENDERING_H
#define WIRED_VK_TEMPORAL_MAIN_RENDERING_H

#include "../renderer/ral/ral_command.h"

qboolean VK_TemporalMainRenderingBuildInitial(
	ralTexture_t *scene, ralTexture_t *depth, uint32_t width, uint32_t height,
	qboolean hasStencil, qboolean storeDepthStencil,
	ralRenderingInfo_t *outInfo );
qboolean VK_TemporalMainRenderingBuildExact3(
	ralTexture_t *scene, ralTexture_t *velocity, ralTexture_t *validity,
	ralTexture_t *depth, uint32_t width, uint32_t height,
	qboolean hasStencil, qboolean clearAuxiliary,
	ralRenderingInfo_t *outInfo );
qboolean VK_TemporalMainRenderingBuildResume(
	ralTexture_t *scene, ralTexture_t *depth, uint32_t width, uint32_t height,
	qboolean hasStencil, ralRenderingInfo_t *outInfo );
qboolean VK_TemporalMainRenderingBuildAttachmentBarrier(
	ralMemoryBarrier_t *outMemory, ralPipelineBarrierInfo_t *outBarrier );

#endif
