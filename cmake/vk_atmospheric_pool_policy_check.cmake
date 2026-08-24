# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

IF(NOT DEFINED SOURCE_ROOT)
	MESSAGE(FATAL_ERROR "SOURCE_ROOT is required")
ENDIF()

SET(_cmake_path "${SOURCE_ROOT}/CMakeLists.txt")
SET(_vk_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.c")
SET(_vk_h_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.h")
SET(_core_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_atmospheric_pool.c")
SET(_header_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_atmospheric_pool.h")
SET(_test_path "${SOURCE_ROOT}/tests/vk_atmospheric_pool_test.c")
FOREACH(_path IN ITEMS "${_cmake_path}" "${_vk_path}" "${_vk_h_path}"
		"${_core_path}" "${_header_path}" "${_test_path}")
	IF(NOT EXISTS "${_path}")
		MESSAGE(FATAL_ERROR "missing atmospheric pool source: ${_path}")
	ENDIF()
ENDFOREACH()

FILE(READ "${_cmake_path}" _cmake)
FILE(READ "${_vk_path}" _vk)
FILE(READ "${_vk_h_path}" _vk_h)
FILE(READ "${_core_path}" _core)
FILE(READ "${_header_path}" _header)
FILE(READ "${_test_path}" _test)

FUNCTION(slice_between out body begin_marker end_marker)
	STRING(FIND "${body}" "${begin_marker}" _begin)
	IF(_begin EQUAL -1)
		MESSAGE(FATAL_ERROR "cannot isolate ${begin_marker} -> ${end_marker}")
	ENDIF()
	STRING(LENGTH "${body}" _body_length)
	MATH(EXPR _tail_length "${_body_length} - ${_begin}")
	STRING(SUBSTRING "${body}" ${_begin} ${_tail_length} _tail)
	STRING(FIND "${_tail}" "${end_marker}" _end)
	IF(_end EQUAL -1)
		MESSAGE(FATAL_ERROR "cannot isolate ${begin_marker} -> ${end_marker}")
	ENDIF()
	STRING(SUBSTRING "${_tail}" 0 ${_end} _slice)
	SET(${out} "${_slice}" PARENT_SCOPE)
ENDFUNCTION()

FUNCTION(require_text body needle label)
	STRING(FIND "${body}" "${needle}" _hit)
	IF(_hit EQUAL -1)
		MESSAGE(FATAL_ERROR "${label}: missing '${needle}'")
	ENDIF()
ENDFUNCTION()

FUNCTION(forbid_text body needle label)
	STRING(FIND "${body}" "${needle}" _hit)
	IF(NOT _hit EQUAL -1)
		MESSAGE(FATAL_ERROR "${label}: forbidden '${needle}'")
	ENDIF()
ENDFUNCTION()

FUNCTION(require_call_count body regex expected label)
	STRING(REGEX MATCHALL "${regex}" _hits "${body}")
	LIST(LENGTH _hits _count)
	IF(NOT _count EQUAL expected)
		MESSAGE(FATAL_ERROR "${label}: expected ${expected}, found ${_count}")
	ENDIF()
ENDFUNCTION()

slice_between(_pool_init "${_vk}" "// ── Ping-pong pool buffers ─"
	"// ── Per-frame uniform buffer")
slice_between(_descriptor "${_vk}" "void vk_atmospheric_write_descriptors( void )"
	"static void vk_init_atmospheric_compute_ral_pipeline( void );")
slice_between(_shutdown "${_vk}" "void vk_shutdown_atmospheric( void )"
	"void RE_SetAtmosphere( const atmosphericDesc_t *desc )")
slice_between(_atm_struct "${_vk_h}" "// Dedicated compute + draw pipeline, separate from the particle path."
	"#if FEAT_IQM")

# Backend-neutral owner: exact device-local pair, completed zero uploads and
# candidate-first publication. No native handle or API may enter this TU.
FOREACH(_needle IN ITEMS "Vk" "qvk" "ral_vulkan" "vk_ral")
	forbid_text("${_core}" "${_needle}" "pool owner native-free core")
	forbid_text("${_header}" "${_needle}" "pool owner native-free header")
ENDFOREACH()
FOREACH(_needle IN ITEMS
		"VK_ATMOSPHERIC_POOL_BUFFER_COUNT 2u"
		"ralAllocationReceipt_t allocations[VK_ATMOSPHERIC_POOL_BUFFER_COUNT]"
		"ralBufferUploadReceipt_t uploads[VK_ATMOSPHERIC_POOL_BUFFER_COUNT]"
		"uint64_t poolGeneration;" "uint64_t byteSize;")
	require_text("${_header}" "${_needle}" "pool receipt shape")
ENDFOREACH()
FOREACH(_needle IN ITEMS
		"createInfo.usage = RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST;"
		"createInfo.memory = RAL_MEMORY_DEVICE_LOCAL;"
		"Ral_BufferGetAllocationReceipt( candidate.buffers[i]"
		"tickets[i] = Ral_BufferUploadBegin( candidate.buffers[i], 0u,"
		"Ral_BufferUploadTicketComplete( &tickets[i] )"
		"Ral_BufferAcquireBatchToGraphics( backend, tickets,"
		"Ral_BufferUploadTicketGetReceipt( &tickets[i],"
		"request->resourceIdentity == (uintptr_t)buffer"
		"request->resourceGeneration == allocation->allocationGeneration"
		"allocation->requestedSize == byteSize"
		"request->byteOffset == 0u && request->byteSize == byteSize"
		"request->byteBudget == allocation->committedSize"
		"upload->graphicsVisibilityGeneration"
		"candidate.poolGeneration++;" "candidate.ready = qtrue;"
		"*owner = candidate;")
	require_text("${_core}" "${_needle}" "pool owner transaction")
ENDFOREACH()
require_text("${_core}" "zeroSeed = (unsigned char *)calloc( 1u, (size_t)byteSize );"
	"deterministic zero seed")
require_text("${_core}" "owner->poolGeneration >= UINT64_MAX - 1u"
	"generation saturation")
require_text("${_core}" "candidate.buffers[i] == candidate.buffers[0]"
	"buffer alias rejection")
require_text("${_core}" "DestroyCandidate( &candidate, tickets );"
	"failure cleanup")

# Shipping initialization uses only the pool owner transaction. Per-frame UBO
# ownership is governed independently by vk_atmospheric_frame_policy_check.
require_call_count("${_pool_init}" "VK_AtmosphericPoolInit[(]" 1 "pool init")
require_call_count("${_pool_init}" "VK_AtmosphericPoolEnsure[(]" 1 "pool ensure")
FOREACH(_needle IN ITEMS "qvk" "VkBuffer" "VkDeviceMemory" "MapMemory"
		"vk_ral_register_buffer" "memset( vk.atm.pool")
	forbid_text("${_pool_init}" "${_needle}" "raw atmospheric pool owner purge")
ENDFOREACH()

# Direct RAL groups borrow only the exact generation-bound pool receipt; no raw
# descriptor allocation/update/adoption authority remains in this product seam.
require_call_count("${_descriptor}" "VK_AtmosphericPoolGetReceipt[(]" 1
	"descriptor pool receipt")
require_call_count("${_descriptor}" "VK_AtmosphericPoolReceiptExact[(]" 1
	"descriptor pool exactness")
require_text("${_descriptor}" "poolReceipt.byteSize != poolBytes"
	"descriptor receipt byte-size join")
FOREACH(_needle IN ITEMS
		"bufferIdentities[2] = poolReceipt.buffers[0];"
		"bufferIdentities[3] = poolReceipt.buffers[1];"
		"Ral_GetBufferSize( poolReceipt.buffers[i] ) != poolBytes"
		"values[1].buffer = poolReceipt.buffers[i];"
		"values[2].buffer = poolReceipt.buffers[writePool];"
		"values[1].buffer = poolReceipt.buffers[renderPool];"
		"createInfo.arena = vk.ral_descriptor_arena;"
		"createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;"
		"computeCandidates[i] = Ral_CreateBindGroup( backend, &createInfo );"
		"renderCandidates[i] = Ral_CreateBindGroup( backend, &createInfo );")
	require_text("${_descriptor}" "${_needle}" "direct atmospheric pool group")
ENDFOREACH()
FOREACH(_needle IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets
		Ral_AdoptBindGroup VkDescriptorBufferInfo VkWriteDescriptorSet)
	forbid_text("${_descriptor}" "${_needle}"
		"atmospheric pool retained raw descriptor authority")
ENDFOREACH()
FOREACH(_needle IN ITEMS "pool_buffer" "pool_memory" "pool_ptr")
	forbid_text("${_atm_struct}" "${_needle}" "legacy atmospheric pool fields")
ENDFOREACH()

# Descriptor children are retired before their two RAL buffer parents.
require_call_count("${_shutdown}" "VK_AtmosphericPoolRelease[(]" 1
	"pool owner release")
STRING(FIND "${_shutdown}" "Ral_DestroyBindGroup( vk.atm.ral_render_descriptor[i] )" _child)
STRING(FIND "${_shutdown}" "VK_AtmosphericPoolRelease( &vk_atmospheric_pool );" _parent)
IF(_child EQUAL -1 OR _parent EQUAL -1 OR NOT _child LESS _parent)
	MESSAGE(FATAL_ERROR "atmospheric descriptor child must precede pool parent")
ENDIF()
FOREACH(_needle IN ITEMS "vk.atm.pool_buffer" "vk.atm.pool_memory"
		"vk.atm.pool_ptr" "qvkDestroyBuffer" "qvkFreeMemory")
	IF(_needle MATCHES "^qvk")
		# Raw frame-UBO teardown is allowed; it is outside this leaf.
	ELSE()
		forbid_text("${_shutdown}" "${_needle}" "raw pool teardown purge")
	ENDIF()
ENDFOREACH()

# Host covers every creation/upload stage, alias, both queue shapes, exact
# receipt mutation, persistent generation ceiling and reverse buffer teardown.
FOREACH(_needle IN ITEMS
		"phase <= 13u" "failPhase = phase" "receiptMutation = 1u"
		"receiptMutation = 2u" "aliasSecond = 1u" "requireAcquire = 1u"
		"memcmp( &owner, &before, sizeof( owner ) )"
		"mutation < 10u" "bad.allocations[0].requestedSize++"
		"bad.uploads[1].graphicsVisibilityGeneration++"
		"receipt2.poolGeneration == 2u"
		"owner.poolGeneration = UINT64_MAX - 1u"
		"destroyOrder[0] == '1'" "destroyOrder[1] == '0'")
	require_text("${_test}" "${_needle}" "pool host mutation coverage")
ENDFOREACH()

# Only the pool owner and vk.c may consume the transaction API.
FILE(GLOB_RECURSE _shipping_sources "${SOURCE_ROOT}/code/*.c" "${SOURCE_ROOT}/code/*.h")
FOREACH(_path IN LISTS _shipping_sources)
	IF(_path STREQUAL _core_path OR _path STREQUAL _header_path OR _path STREQUAL _vk_path)
		CONTINUE()
	ENDIF()
	FILE(READ "${_path}" _shipping_body)
	forbid_text("${_shipping_body}" "VK_AtmosphericPool"
		"atmospheric pool API escaped owner: ${_path}")
ENDFOREACH()

require_text("${_cmake}" "ADD_EXECUTABLE(vk_atmospheric_pool_test"
	"pool host target")
require_text("${_cmake}" "vk_atmospheric_pool_source_policy_contract"
	"pool policy target")
MESSAGE(STATUS "atmospheric ping-pong pools are RAL-owned and ticket-seeded")
