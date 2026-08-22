# SPDX-License-Identifier: GPL-3.0-or-later
# Exact external-layout push range + advanced-fog migration policy.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/renderer/ral/ral_command.h" RAL_HEADER)
FILE(READ "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_bridge.h" BRIDGE)
FILE(READ "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_internal.h" INTERNAL)
FILE(READ "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c" RESOURCE)
FILE(READ "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_command.c" COMMAND)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.c" VK_SOURCE)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c" RAL_TEXTURES)
FILE(READ "${SOURCE_ROOT}/tests/ral_vulkan_dynamic_bind_test.c" HOST)

FUNCTION(REQUIRE_TEXT BODY NEEDLE MESSAGE_TEXT)
	STRING(FIND "${BODY}" "${NEEDLE}" POS)
	IF(POS EQUAL -1)
		MESSAGE(FATAL_ERROR "${MESSAGE_TEXT}: ${NEEDLE}")
	ENDIF()
ENDFUNCTION()

FUNCTION(REQUIRE_COUNT BODY NEEDLE EXPECTED MESSAGE_TEXT)
	STRING(REGEX REPLACE "([][+.*()^$?\\|])" "\\\\\\1" ESCAPED "${NEEDLE}")
	STRING(REGEX MATCHALL "${ESCAPED}" MATCHES "${BODY}")
	LIST(LENGTH MATCHES ACTUAL)
	IF(NOT ACTUAL EQUAL EXPECTED)
		MESSAGE(FATAL_ERROR "${MESSAGE_TEXT}: ${NEEDLE} count ${ACTUAL}, expected ${EXPECTED}")
	ENDIF()
ENDFUNCTION()

FUNCTION(EXTRACT_SPAN BODY BEGIN END OUT)
	STRING(FIND "${BODY}" "${BEGIN}" BEGIN_POS)
	IF(BEGIN_POS EQUAL -1)
		MESSAGE(FATAL_ERROR "missing span begin: ${BEGIN}")
	ENDIF()
	STRING(SUBSTRING "${BODY}" ${BEGIN_POS} -1 TAIL)
	STRING(FIND "${TAIL}" "${END}" END_POS)
	IF(END_POS EQUAL -1)
		MESSAGE(FATAL_ERROR "missing span end: ${END}")
	ENDIF()
	STRING(SUBSTRING "${TAIL}" 0 ${END_POS} SPAN)
	SET(${OUT} "${SPAN}" PARENT_SCOPE)
ENDFUNCTION()

REQUIRE_TEXT("${RAL_HEADER}" "qboolean Ral_CmdPushConstantsLayoutExact("
	"portable exact explicit-layout push declaration missing")
REQUIRE_TEXT("${RAL_HEADER}" "stageFlags use RAL_STAGE_* on every"
	"explicit layout regained native stage semantics")
REQUIRE_TEXT("${BRIDGE}" "Ral_RegisterExternalPipelineLayoutPushRange("
	"external layout push-range registration missing")
REQUIRE_TEXT("${INTERNAL}" "RAL_VK_MAX_EXTERNAL_PUSH_RANGES 4u"
	"bounded external push-range inventory missing")
FOREACH(NEEDLE IN ITEMS
	"externalPushRangeCount"
	"uint32_t stageFlags; // portable RAL_STAGE_* authority"
	"externalPushRanges[ RAL_VK_MAX_EXTERNAL_PUSH_RANGES ]")
	REQUIRE_TEXT("${INTERNAL}" "${NEEDLE}"
		"external push-range owner field missing")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"stageFlags == 0u || ( stageFlags & ~RAL_STAGE_ALL ) != 0u"
	"( offset & 3u ) != 0u || ( size & 3u ) != 0u"
	"size > limit || offset > limit - size"
	"layout->externalPushRanges[i].stageFlags == stageFlags"
	"layout->externalPushRanges[i].stageFlags & stageFlags"
	"layout->externalPushRangeCount >= RAL_VK_MAX_EXTERNAL_PUSH_RANGES"
	"layout->externalPushRangeCount = i + 1u;")
	REQUIRE_TEXT("${RESOURCE}" "${NEEDLE}"
		"output-atomic external push-range registration seam missing")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"!ralVk_CommandRecordsBufferBindings( cb )"
	"layout->backend != cb->backend"
	"!cb->backend->vk.CmdPushConstants"
	"stageFlags == 0u || ( stageFlags & ~RAL_STAGE_ALL ) != 0u"
	"offset - rangeOffset <= rangeSize - size"
	"stageFlags & RAL_STAGE_FRAGMENT"
	"vkStages |= VK_SHADER_STAGE_FRAGMENT_BIT"
	"CmdPushConstants( cb->cb, layout->vkHandle"
	"return qtrue;")
	REQUIRE_TEXT("${COMMAND}" "${NEEDLE}"
		"exact explicit-layout push validation/lowering seam missing")
ENDFOREACH()

EXTRACT_SPAN("${VK_SOURCE}" "void vk_update_fog_push("
	"#endif // FEAT_FOG_SYSTEM" FOG_PUSH)
REQUIRE_COUNT("${FOG_PUSH}" "Ral_CmdPushConstantsLayoutExact(" 1
	"advanced fog exact push call count drifted")
FOREACH(NEEDLE IN ITEMS
	"vk.cmd->ral_cmd"
	"vk.ral_pipeline_layout, RAL_STAGE_FRAGMENT"
	"WIRED_FOG_PUSH_OFFSET, sizeof( push ), &push"
	"ri.Terminate( TERM_UNRECOVERABLE"
	"vk.stats.push_size += sizeof( push );")
	REQUIRE_TEXT("${FOG_PUSH}" "${NEEDLE}"
		"advanced fog exact push operand/failure seam missing")
ENDFOREACH()
STRING(FIND "${FOG_PUSH}" "vk.stats.push_size += sizeof( push );" STATS_POS)
STRING(FIND "${FOG_PUSH}" "Ral_CmdPushConstantsLayoutExact(" PUSH_POS)
IF(PUSH_POS EQUAL -1 OR STATS_POS EQUAL -1 OR NOT PUSH_POS LESS STATS_POS)
	MESSAGE(FATAL_ERROR "fog stats publish before exact push authority")
ENDIF()

FOREACH(NEEDLE IN ITEMS
	"Ral_RegisterExternalPipelineLayoutPushRange("
	"vk.ral_pipeline_layout, RAL_STAGE_FRAGMENT"
	"WIRED_FOG_PUSH_OFFSET, WIRED_FOG_PUSH_SIZE"
	"Ral_DestroyPipelineLayout( vk.ral_pipeline_layout );"
	"vk.ral_pipeline_layout = NULL;")
	REQUIRE_TEXT("${RAL_TEXTURES}" "${NEEDLE}"
		"main fog push-range adoption/fail-closed seam missing")
ENDFOREACH()

FILE(GLOB_RECURSE RENDERERVK_SOURCES
	"${SOURCE_ROOT}/code/renderervk/*.c"
	"${SOURCE_ROOT}/code/renderervk/*.h")
FOREACH(PATH IN LISTS RENDERERVK_SOURCES)
	FILE(READ "${PATH}" BODY)
	IF(BODY MATCHES "qvkCmdPushConstants|INIT_DEVICE_FUNCTION[(]vkCmdPushConstants[)]")
		FILE(RELATIVE_PATH REL "${SOURCE_ROOT}" "${PATH}")
		MESSAGE(FATAL_ERROR "raw push-constant ownership escaped RAL backend: ${REL}")
	ENDIF()
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"!Ral_CmdPushConstantsLayoutExact( &command, pushLayout"
	"Ral_RegisterExternalPipelineLayoutPushRange( pushLayout"
	"pushLayout->externalPushRangeCount == 1u"
	"capturedPushStages == VK_SHADER_STAGE_FRAGMENT_BIT"
	"otherPushLayout.backend = &otherBackend"
	"command.lifecycle.state = RAL_COMMAND_RECORDING"
	"pushCalls == 0u"
	"pushCalls == 1u")
	REQUIRE_TEXT("${HOST}" "${NEEDLE}"
		"explicit-layout push mutation evidence missing")
ENDFOREACH()

MESSAGE(STATUS "RAL advanced-fog push policy: exact portable range registered; raw renderer ownership absent")
