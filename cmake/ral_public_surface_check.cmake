# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

# This cohort had declarations and Vulkan bodies but no consumer. Keeping it
# public falsely advertised raw-handle/legacy-render-pass portability. Exact
# symbol absence makes retirement fail closed when a stale branch reintroduces
# either the API or its wrapper types/creation fields.
SET(_forbidden
	Ral_CmdBindBindGroups
	Ral_CmdBeginRenderPass
	Ral_CmdEndRenderPass
	Ral_CmdNextSubpass
	Ral_AdoptRenderPass
	Ral_DestroyRenderPass
	Ral_GetRenderPassHandle
	Ral_AdoptFramebuffer
	Ral_DestroyFramebuffer
	Ral_GetFramebufferHandle
	ralRenderPass_t
	ralFramebuffer_t
	externalRenderPass
	externalSubpass
	Ral_AdoptQueryPool
	Ral_GetQueryPoolHandle
	qvkCmdBeginRenderPass
	qvkCmdEndRenderPass
	qvkCmdNextSubpass
	vk.smaa_edge_pipeline
	vk.smaa_blend_pipeline
	vk.smaa_resolve_pipeline
	vk.render_pass.smaa_
	vk.framebuffers.smaa_
)

FILE(GLOB_RECURSE _surface_files
	"${SOURCE_ROOT}/code/render/ral/core/*.c"
	"${SOURCE_ROOT}/code/render/ral/core/*.h"
	"${SOURCE_ROOT}/code/render/ral/backends/vulkan/*.c"
	"${SOURCE_ROOT}/code/render/ral/backends/vulkan/*.h"
	"${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/*.c"
	"${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/*.h"
)
FOREACH(_path IN LISTS _surface_files)
	FILE(READ "${_path}" _body)
	FOREACH(_token IN LISTS _forbidden)
		STRING(FIND "${_body}" "${_token}" _hit)
		IF(NOT _hit EQUAL -1)
			FILE(RELATIVE_PATH _rel "${SOURCE_ROOT}" "${_path}")
			MESSAGE(FATAL_ERROR "retired RAL public-surface token '${_token}' remains in ${_rel}")
		ENDIF()
	ENDFOREACH()
ENDFOREACH()

SET(_pipeline_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_pipeline.c")
FILE(READ "${_pipeline_path}" _pipeline)
FOREACH(_required
	"gpci.pNext = &dynRendering;"
	"gpci.renderPass          = VK_NULL_HANDLE;"
	"gpci.subpass             = 0;"
)
	STRING(FIND "${_pipeline}" "${_required}" _hit)
	IF(_hit EQUAL -1)
		MESSAGE(FATAL_ERROR "RAL dynamic-rendering authority missing: ${_required}")
	ENDIF()
ENDFOREACH()

# Backend-native adoption/getter functions are a Vulkan migration bridge, not
# portable RAL. Their declarations must live in exactly the backend bridge
# header and must never leak back into code/render/ral/core/*.h.
SET(_bridge_symbols
	Ral_GetInstanceHandle
	Ral_GetPhysicalDeviceHandle
	Ral_GetSurfaceHandle
	Ral_GetDeviceHandle
	Ral_GetQueueHandle
	Ral_GetQueueFamily
	Ral_GetEnabledDeviceExtensions
	RalVulkan_FindMemoryType
	RalVulkan_GetImageMemoryRequirements
	RalVulkan_SetObjectName
	Ral_GetCommandBufferHandle
	Ral_AdoptFence
	Ral_GetFenceHandle
	Ral_AdoptSemaphore
	Ral_GetSemaphoreHandle
	Ral_GetSwapchainHandle
	Ral_AdoptBuffer
	Ral_AdoptBufferExact
	Ral_GetBufferHandle
	Ral_GetBufferSize
	Ral_GetBufferUsage
	Ral_GetBufferMemoryType
	Ral_RegisterAdoptedBindGroupDynamicBuffer
	Ral_AdoptTexture
	Ral_AdoptArrayTexture
	Ral_GetTextureImageHandle
	Ral_GetTextureViewHandle
	Ral_SetTextureLayout
	Ral_AdoptSampler
	Ral_GetSamplerHandle
	Ral_AdoptBindGroupLayout
	Ral_GetBindGroupLayoutHandle
	Ral_AdoptPipelineLayout
	Ral_GetPipelineLayoutHandle
	Ral_AdoptBindGroup
	Ral_GetBindGroupHandle
)
SET(_bridge_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_bridge.h")
FILE(READ "${_bridge_path}" _bridge)
FILE(GLOB _public_headers "${SOURCE_ROOT}/code/render/ral/core/*.h")
FOREACH(_symbol IN LISTS _bridge_symbols)
	STRING(FIND "${_bridge}" "${_symbol}(" _bridge_hit)
	IF(_bridge_hit EQUAL -1)
		MESSAGE(FATAL_ERROR "Vulkan migration bridge declaration missing: ${_symbol}")
	ENDIF()
	FOREACH(_header IN LISTS _public_headers)
		FILE(READ "${_header}" _public)
		STRING(FIND "${_public}" "${_symbol}(" _public_hit)
		IF(NOT _public_hit EQUAL -1)
			FILE(RELATIVE_PATH _rel "${SOURCE_ROOT}" "${_header}")
			MESSAGE(FATAL_ERROR "backend-native bridge symbol '${_symbol}' leaked into portable header ${_rel}")
		ENDIF()
	ENDFOREACH()
ENDFOREACH()

FILE(GLOB_RECURSE _all_sources
	"${SOURCE_ROOT}/code/renderer/*.c"
	"${SOURCE_ROOT}/code/renderer/*.h"
	"${SOURCE_ROOT}/code/render/ral/backends/vulkan/*.c"
	"${SOURCE_ROOT}/code/render/ral/backends/vulkan/*.h"
	"${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/*.c"
	"${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/*.h"
)
SET(_bridge_include_owners)
FOREACH(_path IN LISTS _all_sources)
	FILE(READ "${_path}" _body)
	STRING(FIND "${_body}" "ral_vulkan_bridge.h" _include_hit)
	IF(NOT _include_hit EQUAL -1)
		FILE(RELATIVE_PATH _rel "${SOURCE_ROOT}" "${_path}")
		LIST(APPEND _bridge_include_owners "${_rel}")
	ENDIF()
ENDFOREACH()
LIST(SORT _bridge_include_owners)
SET(_expected_bridge_include_owners
	"code/render/ral/backends/vulkan/ral_vulkan_internal.h"
	"code/render/ral/backends/vulkan/renderer/vk.c"
	"code/render/ral/backends/vulkan/renderer/vk_ral_textures.h"
	"code/render/ral/backends/vulkan/renderer/vk_temporal_entmat_runtime.c"
)
LIST(SORT _expected_bridge_include_owners)
IF(NOT _bridge_include_owners STREQUAL _expected_bridge_include_owners)
	MESSAGE(FATAL_ERROR "Vulkan bridge include owner set drifted: ${_bridge_include_owners}")
ENDIF()

MESSAGE(STATUS "RAL public surface contract: callerless raw cohort absent; native migration bridge isolated; graphics pipelines dynamic-rendering only")
