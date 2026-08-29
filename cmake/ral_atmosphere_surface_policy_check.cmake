# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

function(require_text FILE NEEDLE)
	file(READ "${ROOT}/${FILE}" SOURCE)
	string(FIND "${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "${FILE} misses atmosphere surface contract: ${NEEDLE}")
	endif()
endfunction()

function(require_regex FILE PATTERN)
	file(READ "${ROOT}/${FILE}" SOURCE)
	string(REGEX MATCH "${PATTERN}" MATCH "${SOURCE}")
	if(NOT MATCH)
		message(FATAL_ERROR "${FILE} misses atmosphere surface contract: ${PATTERN}")
	endif()
endfunction()

require_text("code/qcommon/wired/render/primitives.h"
	"WIRED_ATMOSPHERE_SURFACE_EVENT_SCHEMA_VERSION")
require_text("code/qcommon/wired/render/primitives.h"
	"WIRED_ATMOSPHERE_MEDIA_VOLUME_SCHEMA_VERSION")
require_regex("code/render/frontend/render_submission_effects.h"
	"RENDER_SUBMISSION_MAX_SURFACE_CLIMATE_TILES[ \t]+256u")
require_regex("code/render/frontend/render_submission_effects.h"
	"RENDER_SUBMISSION_MAX_ATMOSPHERE_SURFACE_EVENTS[ \t]+256u")
require_regex("code/render/frontend/render_submission_effects.h"
	"RENDER_SUBMISSION_MAX_ATMOSPHERE_MEDIA_VOLUMES[ \t]+64u")
require_text("code/render/frontend/render_submission_effects.c"
	"AtmosphereSurfaceFindOrAllocate")
require_text("code/render/frontend/render_submission_effects.c"
	"1.0f - expf")
require_regex("code/render/ral/core/ral_atmosphere.h"
	"RAL_ATMOSPHERE_MAX_SURFACE_TILES[ \t]+256u")
require_text("code/render/ral/core/ral_atmosphere.c"
	"Ral_AtmosphereBuildSurfaceTable")
require_text("code/render/ral/core/ral_atmosphere.c"
	"receipt.comparisonLimit = 8u;")
require_text("code/render/ral/backends/vulkan/renderer/tr_init.c"
	"RE_SetAtmosphereSurfaceTiles")
require_text("code/render/ral/backends/vulkan/renderer/vk.c"
	"vk.decal.surfaceClimateBuffer[frameIdx]")
require_text("code/render/ral/backends/vulkan/renderer/shaders/decal.frag"
	"SurfaceClimateTile surfaceClimateTiles[256]")
require_text("code/render/ral/backends/vulkan/renderer/shaders/decal.frag"
	"for ( uint step = 0u; step < 8u; ++step )")
require_text("code/render/ral/backends/vulkan/renderer/shaders/decal.frag"
	"localSurfaceClimate = surfaceClimateAt( worldPos.xy );")
require_text("code/render/frontend/render_submission.c"
	"surfaceClimateDigest")
require_text("code/render/frontend/tr_public.h"
	"AddAtmosphereSurfaceEvent")
require_text("code/render/frontend/tr_public.h"
	"AddAtmosphereMediaVolume")
require_text("code/cgame/cg_public.h"
	"CG_R_ADDATMOSPHERESURFACEEVENT")
require_text("code/cgame/cg_public.h"
	"CG_R_ADDATMOSPHEREMEDIAVOLUME")
require_text("code/cgame/cg_syscalls.c"
	"trap_R_AddAtmosphereSurfaceEvent")
require_text("code/client/cl_cgame.c"
	"cl_desc_CG_R_ADDATMOSPHERESURFACEEVENT")
require_text("code/render/ral/backends/vulkan/renderer/tr_init.c"
	"FrontendAddAtmosphereSurfaceEvent")
require_text("code/render/ral/backends/vulkan/renderer/tr_init.c"
	"FrontendAddAtmosphereMediaVolume")
require_text("code/render/ral/backends/opengl/ral_opengl_module.c"
	"AddAtmosphereSurfaceEvent")
require_text("code/render/ral/backends/opengl/ral_opengl_module.c"
	"AddAtmosphereMediaVolume")
require_text("code/render/ral/backends/metal/ral_metal_module.mm"
	"SubmitAtmosphereSurfaceEvent")
require_text("code/render/ral/backends/metal/ral_metal_module.mm"
	"SubmitAtmosphereMediaVolume")
require_text("code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c"
	"AddAtmosphereSurfaceEvent")
require_text("code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c"
	"AddAtmosphereMediaVolume")

foreach(FILE IN ITEMS
		"code/render/frontend/render_submission_effects.c"
		"code/render/frontend/render_submission_effects.h")
	file(READ "${ROOT}/${FILE}" SOURCE)
	foreach(FORBIDDEN IN ITEMS "code/cgame" "cg_local.h" "clientActive_t"
			"VkDevice" "MTLDevice" "WGPUDevice" "GLuint")
		string(FIND "${SOURCE}" "${FORBIDDEN}" POSITION)
		if(NOT POSITION EQUAL -1)
			message(FATAL_ERROR
				"${FILE} leaks application/backend ownership: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()

message(STATUS "RAL atmosphere surface ownership policy: PASS")
