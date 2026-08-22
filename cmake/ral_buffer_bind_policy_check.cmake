# SPDX-License-Identifier: GPL-3.0-or-later
# Exact portable vertex/index binding migration policy.

IF(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
ENDIF()

FILE(READ "${SOURCE_ROOT}/code/renderer/ral/ral_command.h" RAL_HEADER)
FILE(READ "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_command.c" RAL_CORE)
FILE(READ "${SOURCE_ROOT}/code/renderervk/vk.c" VK_SOURCE)
FILE(READ "${SOURCE_ROOT}/tests/ral_vulkan_buffer_bind_test.c" HOST)

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

FOREACH(NEEDLE IN ITEMS
	"qboolean Ral_CmdBindVertexBufferExact("
	"qboolean Ral_CmdBindVertexBuffersExact("
	"qboolean Ral_CmdBindIndexBufferExact(")
	REQUIRE_TEXT("${RAL_HEADER}" "${NEEDLE}"
		"portable exact buffer-binding declaration missing")
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"ralVk_CommandRecordsBufferBindings( cb )"
	"buffer->backend != cb->backend"
	"buffer->usage & RAL_BUFFER_VERTEX"
	"offsets[i] >= buffer->size"
	"( offsets[i] & 3u ) != 0u"
	"buf->usage & RAL_BUFFER_INDEX"
	"offset >= buf->size"
	"elementSize > buf->size - offset"
	"type != RAL_INDEX_UINT16 && type != RAL_INDEX_UINT32"
	"Ral_CmdBindVertexBuffersExact( cb, firstBinding, bindingCount,"
	"Ral_CmdBindIndexBufferExact( cb, buf, offset, type )")
	REQUIRE_TEXT("${RAL_CORE}" "${NEEDLE}"
		"exact buffer-binding validation/compatibility seam missing")
ENDFOREACH()

FOREACH(FORBIDDEN IN ITEMS qvkCmdBindVertexBuffers qvkCmdBindIndexBuffer
	PFN_vkCmdBindVertexBuffers PFN_vkCmdBindIndexBuffer)
	STRING(FIND "${VK_SOURCE}" "${FORBIDDEN}" POS)
	IF(NOT POS EQUAL -1)
		MESSAGE(FATAL_ERROR "raw product buffer binding returned: ${FORBIDDEN}")
	ENDIF()
ENDFOREACH()

FOREACH(NEEDLE IN ITEMS
	"vk_ral_lookup_buffer( nativeBuffers[i] )"
	"Ral_GetBufferHandle( buffers[i] ) != (void *)nativeBuffers[i]"
	"Ral_CmdBindVertexBuffersExact( command, firstBinding, bindingCount,"
	"vk_ral_lookup_buffer( nativeBuffer )"
	"Ral_CmdBindIndexBufferExact( command, buffer, (uint64_t)offset,"
	"Ral_CmdBindPipeline( command, NULL )")
	REQUIRE_TEXT("${VK_SOURCE}" "${NEEDLE}"
		"registry-backed fail-closed product binding seam missing")
ENDFOREACH()
REQUIRE_COUNT("${VK_SOURCE}" "vk_ral_bind_registered_vertex_buffers(" 12
	"shipping vertex bind inventory drifted")
REQUIRE_COUNT("${VK_SOURCE}" "vk_ral_bind_registered_index_buffer(" 9
	"shipping index bind inventory drifted")

FOREACH(NEEDLE IN ITEMS
	"REJECT_VERTEX( vertex0.backend = &otherBackend )"
	"REJECT_VERTEX( vertex0.usage = RAL_BUFFER_INDEX )"
	"REJECT_VERTEX( offsets[0] = vertex0.size )"
	"REJECT_VERTEX( offsets[0] = 2u )"
	"REJECT_VERTEX( vertex0.legacyMapped = qtrue )"
	"REJECT_INDEX( command.lifecycle.state = RAL_COMMAND_IDLE )"
	"Ral_CmdBindVertexBuffersExact( &command, 15u, 2u"
	"REJECT_INDEX( index.backend = &otherBackend )"
	"REJECT_INDEX( index.usage = RAL_BUFFER_VERTEX )"
	"REJECT_INDEX( index.legacyMapped = qtrue )"
	"Ral_CmdBindIndexBufferExact( &command, &index, 2u"
	"Ral_CmdBindIndexBufferExact( &command, &index, index.size"
	"(ralIndexType_t)2"
	"command.lifecycle.state = RAL_COMMAND_RECORDING"
	"Ral_CmdBindVertexBufferExact( &command, 0u, &vertex0, 0u )")
	REQUIRE_TEXT("${HOST}" "${NEEDLE}"
		"buffer-binding mutation coverage missing")
ENDFOREACH()

MESSAGE(STATUS "RAL exact buffer binding policy: portable validation and 19 shipping binds pinned")
