# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_iqm_payload.c" CORE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_iqm_payload.h" ABI)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_temporal_iqm_motion.h" IQM_ABI)

function(require_text haystack needle label)
	string(FIND "${haystack}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "temporal IQM payload policy missing ${label}: ${needle}")
	endif()
endfunction()

foreach(needle IN ITEMS
	"TEMPORAL_IQM_MAX_RECORDS 256u"
	"TEMPORAL_IQM_RECORD_SIZE 12480u"
	"TEMPORAL_IQM_SLOT_BYTES")
	require_text("${IQM_ABI}" "${needle}" "fixed 256-record ABI")
endforeach()
foreach(needle IN ITEMS
	"maxStorageBufferRange < (uint64_t)TEMPORAL_IQM_SLOT_BYTES"
	"bci.size = TEMPORAL_IQM_SLOT_BYTES"
	"bci.usage = RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST"
	"bci.memory = RAL_MEMORY_DEVICE_LOCAL"
	"slot.cpuShadow = calloc( 1, TEMPORAL_IQM_SLOT_BYTES )"
	"free( slot->cpuShadow )"
	"value.bufferOffset = 0"
	"value.bufferRange = TEMPORAL_IQM_SLOT_BYTES"
	"RAL_BIND_STORAGE_BUFFER, 1u, RAL_STAGE_VERTEX"
	"readySlotMask"
	"slotFenceCompleted != qtrue"
	"nextPrepareGeneration"
	"commandSlot != owner->preparedSlot"
	"!ReceiptValid( a ) || !ReceiptValid( b )"
	"layoutLeaseCount"
	"VK_TemporalIqmPayloadAcquireLayoutLease"
	"VK_TemporalIqmPayloadReleaseLayoutLease")
	require_text("${CORE}" "${needle}" "owner/lifecycle invariant")
endforeach()
foreach(forbidden IN ITEMS "Ral_MapBuffer(" "Ral_UnmapBuffer(" "legacyMapped")
	string(FIND "${CORE}${ABI}" "${forbidden}" forbidden_pos)
	if(NOT forbidden_pos EQUAL -1)
		message(FATAL_ERROR "temporal IQM payload regained persistent map surface: ${forbidden}")
	endif()
endforeach()
foreach(forbidden IN ITEMS VK_WHOLE_SIZE Ral_Cmd Ral_CreateGraphicsPipeline
	Ral_CreateShaderModule VkPipeline vkCmdBind vkCmdDraw)
	string(FIND "${CORE}" "${forbidden}" forbidden_pos)
	if(NOT forbidden_pos EQUAL -1)
		message(FATAL_ERROR "definition-only payload gained command/factory surface: ${forbidden}")
	endif()
endforeach()

# Stage 4 adds a pure receipt-bound authoring transaction. Product
# materialization still waits for the exact IQM pre-scan/activation authority.
file(GLOB_RECURSE PRODUCT_C "${ROOT}/code/*.c")
foreach(source IN LISTS PRODUCT_C)
	if(source STREQUAL "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_iqm_payload.c")
		continue()
	endif()
	file(READ "${source}" source_text)
	if(source STREQUAL "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_iqm_exact3_factory.c")
		foreach(needle IN ITEMS
			"VK_TemporalIqmPayloadAcquireLayoutLease"
			"VK_TemporalIqmPayloadReleaseLayoutLease")
			string(REGEX MATCHALL "${needle}\\(" lease_calls "${source_text}")
			list(LENGTH lease_calls lease_count)
			if(NOT lease_count EQUAL 1)
				message(FATAL_ERROR "exact3 factory must hold one exact payload lease call: ${needle}")
			endif()
		endforeach()
		foreach(forbidden IN ITEMS Init NeedsIdle PrepareAfterFence GetReceipt ReceiptExact HasLive ReleaseAfterIdle)
			string(FIND "${source_text}" "VK_TemporalIqmPayload${forbidden}" escaped_pos)
			if(NOT escaped_pos EQUAL -1)
				message(FATAL_ERROR "exact3 factory escaped payload lease-only surface: ${forbidden}")
			endif()
		endforeach()
		continue()
	endif()
	if(source STREQUAL "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_iqm_payload_authoring.c")
		require_text("${source_text}" "VK_TemporalIqmPayloadReceiptExact" "authoring receipt join")
		string(REGEX MATCHALL "VK_TemporalIqmPayloadGetReceipt\\(" get_calls "${source_text}")
		list(LENGTH get_calls get_count)
		if(NOT get_count EQUAL 1)
			message(FATAL_ERROR "authoring must have exactly one owner-aware GetReceipt call")
		endif()
		require_text("${source_text}"
			"VK_TemporalIqmPayloadGetReceipt( payloadOwner,"
			"ContentRevalidate current owner query")
		require_text("${source_text}"
			"receipt->authority.commandSlot, &currentPayload"
			"ContentRevalidate exact command slot query")
		require_text("${source_text}"
			"VK_TemporalIqmPayloadReceiptExact( &receipt->payload, &currentPayload )"
			"ContentRevalidate current receipt exact join")
		foreach(forbidden IN ITEMS Init NeedsIdle PrepareAfterFence
			AcquireLayoutLease ReleaseLayoutLease HasLive ReleaseAfterIdle)
			string(FIND "${source_text}" "VK_TemporalIqmPayload${forbidden}" escaped_pos)
			if(NOT escaped_pos EQUAL -1)
				message(FATAL_ERROR "authoring escaped payload receipt-only surface: ${forbidden}")
			endif()
		endforeach()
		continue()
	endif()
	if(source STREQUAL "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_main_activation.c")
		foreach(forbidden IN ITEMS Init NeedsIdle PrepareAfterFence GetReceipt
			ReceiptExact AcquireLayoutLease ReleaseLayoutLease HasLive ReleaseAfterIdle)
			string(FIND "${source_text}" "VK_TemporalIqmPayload${forbidden}" escaped_pos)
			if(NOT escaped_pos EQUAL -1)
				message(FATAL_ERROR "central activation escaped sealed content-only surface: ${forbidden}")
			endif()
		endforeach()
		require_text("${source_text}" "VK_TemporalIqmPayloadContentReceiptExact"
			"central activation exact sealed content receipt")
		require_text("${source_text}" "VK_TemporalIqmPayloadContentRevalidate"
			"central activation owner-aware content revalidation")
		continue()
	endif()
	if(source STREQUAL "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c")
		string(REGEX MATCHALL
			"vkTemporalIqmPayloadOwner_t[ 	]+vk_temporal_iqm_payload"
			product_owners "${source_text}")
		list(LENGTH product_owners product_owner_count)
		if(NOT product_owner_count EQUAL 1)
			message(FATAL_ERROR "payload product owner inventory drifted")
		endif()
		foreach(api_count IN ITEMS "Init;1" "NeedsIdle;1" "PrepareAfterFence;1"
			"GetReceipt;3" "HasLive;3" "ReleaseAfterIdle;2")
			list(GET api_count 0 api)
			list(GET api_count 1 expected)
			string(REGEX MATCHALL "VK_TemporalIqmPayload${api}[(]" calls "${source_text}")
			list(LENGTH calls actual)
			if(NOT actual EQUAL expected)
				message(FATAL_ERROR "payload product resource API drifted ${api}: ${actual}")
			endif()
		endforeach()
		require_text("${source_text}"
			"VK_TemporalIqmPayloadGetReceipt( &vk_temporal_iqm_payload,\n\t\t\t\tauthority.commandSlot, &payload )"
			"active pre-scan current-slot payload receipt")
		foreach(forbidden IN ITEMS AcquireLayoutLease ReleaseLayoutLease)
			string(FIND "${source_text}" "VK_TemporalIqmPayload${forbidden}(" escaped_pos)
			if(NOT escaped_pos EQUAL -1)
				message(FATAL_ERROR "payload product escaped owner-only resource surface: ${forbidden}")
			endif()
		endforeach()
		continue()
	endif()
	string(FIND "${source_text}" "VK_TemporalIqmPayload" caller_pos)
	if(NOT caller_pos EQUAL -1)
		message(FATAL_ERROR "payload API escaped definition-only owner: ${source}")
	endif()
	string(FIND "${source_text}" "vkTemporalIqmPayloadOwner_t" owner_pos)
	if(NOT owner_pos EQUAL -1)
		message(FATAL_ERROR "payload owner escaped definition-only TU: ${source}")
	endif()
endforeach()

message(STATUS "temporal IQM payload definition-only policy PASS")
