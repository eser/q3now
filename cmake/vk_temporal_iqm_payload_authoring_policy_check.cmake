# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/renderervk/vk_temporal_iqm_payload_authoring.h" ABI)
file(READ "${ROOT}/code/renderervk/vk_temporal_iqm_payload_authoring.c" CORE)
file(READ "${ROOT}/code/renderervk/tr_temporal_iqm_motion.h" SEQUENCE_ABI)
file(READ "${ROOT}/code/renderervk/tr_temporal_iqm_motion.c" SEQUENCE)
file(READ "${ROOT}/tests/vk_temporal_iqm_payload_authoring_test.c" HOST)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)
file(READ "${ROOT}/code/renderervk/vk.c" VK)

function(require_text haystack needle label)
	string(FIND "${haystack}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "temporal IQM authoring policy missing ${label}: ${needle}")
	endif()
endfunction()

foreach(needle IN ITEMS
	"vkBindlessOrdinaryReceipt_t bindless"
	"VK_BindlessPublicationReceiptExact( &f->bindless, &f->bindless )"
	"VK_BindlessPublicationReceiptExact( &a->bindless, &b->bindless )"
	"f->bindless.imageSlot != f->textureSlot"
	"f->bindless.samplerSlot != f->samplerSlot"
	"SequenceValid( const temporalIqmSequence_t *sequence )"
	"sameIndex != sameIdentity"
	"f->vertexBufferBytes % TEMPORAL_IQM_VERTEX_STRIDE"
	"f->indexBufferBytes % sizeof( uint32_t )"
	"* sizeof( uint32_t ) > f->indexBufferBytes"
	"verifier->poisoned = qtrue; return qfalse;")
	require_text("${SEQUENCE_ABI}\n${SEQUENCE}" "${needle}" "sequence/bindless authority")
endforeach()
foreach(field IN ITEMS setIdentity samplerPoolIdentity imageViewIdentity
	samplerIdentity imageOwnerIdentity ordinaryDescriptorIdentity
	imageOwnerGeneration samplerDefinitionDigest setGeneration
	samplerPoolGeneration imagePublicationGeneration imageTransactionGeneration
	samplerPublicationGeneration samplerTransactionGeneration imageSlotGeneration
	samplerSlotGeneration imageBinding samplerBinding ready)
	require_text("${SEQUENCE}" "&f->bindless.${field}" "bindless receipt digest field")
endforeach()
foreach(field IN ITEMS geometryBackend geometryGeneration
	geometryAllocationGeneration)
	require_text("${SEQUENCE}" "&f->${field}" "geometry receipt digest field")
endforeach()
foreach(needle IN ITEMS
	"VK_TemporalIqmPayloadAuthorBegin"
	"VK_TemporalIqmPayloadAuthorWrite"
	"VK_TemporalIqmPayloadAuthorSeal"
	"VK_TemporalIqmPayloadContentRevalidate"
	"R_TemporalIqmSequenceExact( sequence, sequence )"
	"VK_TemporalIqmPayloadReceiptExact( payload, payload )"
	"sequence->authority.commandSlot != payload->commandSlot"
	"recordIndex != author->nextRecordIndex"
	"model->contentDigest != entry->facts.modelContentDigest"
	"R_TemporalIqmBuildGpuRecord"
	"previousValid != entry->facts.previousValid"
	"(size_t)recordIndex * TEMPORAL_IQM_RECORD_SIZE"
	"memcpy( destination, &record, sizeof( record ) )"
	"author->contentDigest != HashRecords"
	"VK_TemporalIqmPayloadGetReceipt( payloadOwner,"
	"receipt->authority.commandSlot, &currentPayload )"
	"VK_TemporalIqmPayloadReceiptExact( &receipt->payload, &currentPayload )"
	"HashRecords( currentPayload.cpuShadowIdentity, receipt->recordCount )")
	require_text("${ABI}\n${CORE}" "${needle}" "write-once Begin/Write/Seal authority")
endforeach()
foreach(forbidden IN ITEMS Ral_Cmd vkCmd vk_ral_lookup_buffer firstInstance
	instanceCount Ral_AdoptBuffer Ral_CreateGraphicsPipeline)
	string(FIND "${CORE}" "${forbidden}" bad)
	if(NOT bad EQUAL -1)
		message(FATAL_ERROR "pure payload authoring gained product command authority: ${forbidden}")
	endif()
endforeach()

# Author Begin/Write/Seal/Cancel remain pure-definition only. The central MAIN
# contract may consume and revalidate the sealed content receipt, but it cannot
# instantiate or author the payload transaction before the Stage4 pre-scan.
file(GLOB_RECURSE PRODUCT_SURFACE "${ROOT}/code/*.c" "${ROOT}/code/*.h")
foreach(source IN LISTS PRODUCT_SURFACE)
	if(source STREQUAL "${ROOT}/code/renderervk/vk_temporal_iqm_payload_authoring.c" OR
			source STREQUAL "${ROOT}/code/renderervk/vk_temporal_iqm_payload_authoring.h" OR
			source STREQUAL "${ROOT}/code/renderervk/vk_temporal_main_activation.c" OR
			source STREQUAL "${ROOT}/code/renderervk/vk_temporal_main_activation.h" OR
			source STREQUAL "${ROOT}/code/renderervk/vk.c")
		continue()
	endif()
	file(READ "${source}" source_text)
	string(FIND "${source_text}" "VK_TemporalIqmPayloadAuthor" escaped)
	string(FIND "${source_text}" "vkTemporalIqmPayloadAuthor_t" owner)
	if(NOT escaped EQUAL -1 OR NOT owner EQUAL -1)
		message(FATAL_ERROR "payload authoring escaped pure definition boundary: ${source}")
	endif()
	foreach(token IN ITEMS VK_TemporalIqmPayloadContent
		vkTemporalIqmPayloadContentReceipt_t)
		string(FIND "${source_text}" "${token}" content_escape)
		if(NOT content_escape EQUAL -1)
			message(FATAL_ERROR "sealed payload content escaped central authority boundary: ${source}")
		endif()
	endforeach()
endforeach()

# The first product checkpoint owns one active-only scan/author transaction in
# vk.c. It intentionally stops before BindIqm/Plan/commands so the existing
# generic Finish remains authoritative until the atomic command slice lands.
string(FIND "${VK}" "qboolean vk_temporal_iqm_prescan_primary_command(" scan_begin)
string(FIND "${VK}" "static qboolean vk_temporal_iqm_revalidate_geometry(" scan_end)
if(scan_begin EQUAL -1 OR scan_end EQUAL -1 OR NOT scan_begin LESS scan_end)
	message(FATAL_ERROR "cannot isolate product IQM pre-scan authoring span")
endif()
math(EXPR scan_len "${scan_end}-${scan_begin}")
string(SUBSTRING "${VK}" ${scan_begin} ${scan_len} PRODUCT_SCAN)
foreach(needle IN ITEMS
	"R_TemporalIqmSequenceBuild( &authority"
	"VK_TemporalIqmPayloadGetReceipt( &vk_temporal_iqm_payload"
	"VK_TemporalIqmPayloadAuthorBegin("
	"VK_TemporalIqmPayloadAuthorWrite("
	"VK_TemporalIqmPayloadAuthorSeal("
	"&backEnd.viewParms.temporalCameraReceipt"
	"backEnd.viewParms.projectionMatrix"
	"vk_temporal_iqm_primary.prepared = qtrue")
	require_text("${PRODUCT_SCAN}" "${needle}" "bounded product pre-scan authoring")
endforeach()
foreach(forbidden IN ITEMS VK_TemporalMainActivationBindIqm
	VK_TemporalMainActivationPlanIqmDraw VK_TemporalMainActivationCommitIqmDraw
	VK_TemporalMainActivationFinishIqm Ral_Cmd vkCmd vkCmdDraw)
	string(FIND "${PRODUCT_SCAN}" "${forbidden}" product_command)
	if(NOT product_command EQUAL -1)
		message(FATAL_ERROR "pre-scan checkpoint gained command/bind authority: ${forbidden}")
	endif()
endforeach()
file(READ "${ROOT}/code/renderervk/vk_temporal_main_activation.c" MAIN)
require_text("${MAIN}" "VK_TemporalIqmPayloadContentReceiptExact" "central content receipt exact join")
require_text("${MAIN}" "VK_TemporalIqmPayloadContentRevalidate" "central owner-aware content revalidation")
foreach(needle IN ITEMS vk_temporal_iqm_payload_authoring_test
	vk_temporal_iqm_payload_authoring.c
	vk_temporal_iqm_payload_authoring_policy_check.cmake)
	require_text("${CMAKE_TEXT}" "${needle}" "CMake acceptance wiring")
endforeach()
foreach(needle IN ITEMS "AuthorWrite(&author,1" "ContentRevalidate(&receipt,&payloadOwner)"
	"VK_TemporalIqmPayloadPrepareAfterFence(&payloadOwner" "imageSlotGeneration")
	require_text("${HOST}" "${needle}" "host mutation gate")
endforeach()

message(STATUS "temporal IQM payload authoring policy PASS")
