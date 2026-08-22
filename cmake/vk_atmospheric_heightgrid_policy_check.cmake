# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

IF(NOT DEFINED SOURCE_ROOT)
	MESSAGE(FATAL_ERROR "SOURCE_ROOT is required")
ENDIF()

SET(_cmake_path "${SOURCE_ROOT}/CMakeLists.txt")
SET(_vk_path "${SOURCE_ROOT}/code/renderervk/vk.c")
SET(_vk_h_path "${SOURCE_ROOT}/code/renderervk/vk.h")
SET(_core_path "${SOURCE_ROOT}/code/renderervk/vk_atmospheric_heightgrid.c")
SET(_header_path "${SOURCE_ROOT}/code/renderervk/vk_atmospheric_heightgrid.h")
SET(_test_path "${SOURCE_ROOT}/tests/vk_atmospheric_heightgrid_test.c")
FOREACH(_path IN ITEMS "${_cmake_path}" "${_vk_path}" "${_vk_h_path}"
		"${_core_path}" "${_header_path}" "${_test_path}")
	IF(NOT EXISTS "${_path}")
		MESSAGE(FATAL_ERROR "missing atmospheric heightgrid source: ${_path}")
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
		MESSAGE(FATAL_ERROR "cannot isolate product span: ${begin_marker} -> ${end_marker}")
	ENDIF()
	STRING(LENGTH "${body}" _body_length)
	MATH(EXPR _tail_length "${_body_length} - ${_begin}")
	STRING(SUBSTRING "${body}" ${_begin} ${_tail_length} _tail)
	STRING(FIND "${_tail}" "${end_marker}" _relative_end)
	IF(_relative_end EQUAL -1)
		MESSAGE(FATAL_ERROR "cannot isolate product span: ${begin_marker} -> ${end_marker}")
	ENDIF()
	STRING(SUBSTRING "${_tail}" 0 ${_relative_end} _slice)
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
		MESSAGE(FATAL_ERROR "${label}: expected ${expected} matches of '${regex}', found ${_count}")
	ENDIF()
ENDFUNCTION()

slice_between(_create "${_vk}" "static void vk_atmospheric_create_heightgrid( void )"
	"void vk_atmospheric_write_descriptors( void )")
slice_between(_descriptor "${_vk}" "void vk_atmospheric_write_descriptors( void )"
	"static void vk_init_atmospheric_compute_ral_pipeline( void );")
slice_between(_shutdown "${_vk}" "void vk_shutdown_atmospheric( void )"
	"void RE_SetAtmosphere( const atmosphericDesc_t *desc )")
slice_between(_upload "${_vk}" "void RE_SetAtmosphereHeightgrid( const float *grid, int count )"
	"void RB_RunAtmosphericCompute( void )")

# Product construction seeds the resource before publishing descriptor interop.
require_call_count("${_create}" "VK_AtmosphericHeightgridInit[(]" 1 "heightgrid init")
require_call_count("${_create}" "VK_AtmosphericHeightgridEnsure[(]" 1 "heightgrid ensure")
require_call_count("${_create}" "VK_AtmosphericHeightgridUpload[(]" 1 "heightgrid seed upload")
require_text("${_create}" "memset( zeros, 0, (size_t)VK_ATMOSPHERIC_HEIGHTGRID_BYTES );"
	"deterministic seed")
require_text("${_create}" "ri.Terminate( TERM_UNRECOVERABLE,"
	"construction failure policy")
FOREACH(_needle IN ITEMS "qvk" "VkImage" "VkBuffer" "record_image_layout_transition"
		"Ral_AcquireBegunCommandBuffer" "Ral_SubmitAndDispose")
	forbid_text("${_create}" "${_needle}" "heightgrid construction RAL-only boundary")
ENDFOREACH()

# Vulkan-native identities are derived only where the legacy descriptor is authored.
require_call_count("${_descriptor}" "Ral_GetTextureDefaultViewHandle[(]" 1
	"descriptor texture interop")
require_call_count("${_descriptor}" "Ral_GetSamplerHandle[(]" 1
	"descriptor sampler interop")
require_text("${_descriptor}" "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL"
	"descriptor sampled layout")
require_text("${_descriptor}" "vk_atmospheric_heightgrid.texture"
	"descriptor retained texture")
require_text("${_descriptor}" "vk_atmospheric_heightgrid.sampler"
	"descriptor retained sampler")

# The public update is a single completed RAL transfer transaction.
require_call_count("${_upload}" "VK_AtmosphericHeightgridUpload[(]" 1
	"product heightgrid upload")
FOREACH(_needle IN ITEMS "qvk" "VkBuffer" "VkDeviceMemory" "MapMemory"
		"record_image_layout_transition" "CmdCopyBufferToImage"
		"Ral_AcquireBegunCommandBuffer" "Ral_SubmitAndDispose"
		"Ral_DestroyBuffer" "Ral_DestroyFence")
	forbid_text("${_upload}" "${_needle}" "product upload raw API purge")
ENDFOREACH()

# Teardown destroys the sampler child and texture parent through the owner before
# the atmospheric layouts and descriptor authorities disappear.
require_call_count("${_shutdown}" "VK_AtmosphericHeightgridRelease[(]" 1
	"heightgrid owner release")
require_text("${_shutdown}" "VK_AtmosphericHeightgridRelease( &vk_atmospheric_heightgrid );"
	"heightgrid release identity")
FOREACH(_needle IN ITEMS "heightgrid_image" "heightgrid_memory" "heightgrid_view"
		"heightgrid_sampler")
	forbid_text("${_vk}" "${_needle}" "legacy heightgrid native owner retirement")
	forbid_text("${_vk_h}" "${_needle}" "legacy heightgrid public owner retirement")
ENDFOREACH()

# Backend-neutral owner shape and exact fixed-size texture/sampler contract.
require_text("${_header}" "VK_ATMOSPHERIC_HEIGHTGRID_SIZE 256u" "heightgrid extent")
require_text("${_header}" "ralAllocationReceipt_t allocation;" "allocation authority")
require_text("${_header}" "ralTransferReceipt_t transfer;" "transfer authority")
require_text("${_header}" "uint64_t uploadSerial;" "upload generation")
FOREACH(_needle IN ITEMS
		"textureInfo.type = RAL_TEXTURE_2D;"
		"textureInfo.format = RAL_FORMAT_R32_SFLOAT;"
		"textureInfo.usage = RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_TRANSFER_DST;"
		"textureInfo.memory = RAL_MEMORY_DEVICE_LOCAL;"
		"samplerInfo.minFilter = RAL_FILTER_LINEAR;"
		"samplerInfo.magFilter = RAL_FILTER_LINEAR;"
		"samplerInfo.mipmapMode = RAL_MIPMAP_NEAREST;"
		"samplerInfo.addressU = RAL_ADDRESS_CLAMP_TO_EDGE;"
		"samplerInfo.maxLod = 1.0f;")
	require_text("${_core}" "${_needle}" "heightgrid resource contract")
ENDFOREACH()
FOREACH(_needle IN ITEMS "Vk" "qvk" "ral_vulkan" "vk_ral_get_backend")
	forbid_text("${_core}" "${_needle}" "heightgrid owner backend neutrality")
	forbid_text("${_header}" "${_needle}" "heightgrid receipt backend neutrality")
ENDFOREACH()

# All fallible work precedes owner/output publication; only a completed graphics
# transfer with the exact allocation generation and byte/range facts is accepted.
require_text("${_core}" "count < 0 || (uint32_t)count < VK_ATMOSPHERIC_HEIGHTGRID_TEXELS"
	"bounded upload count")
require_text("${_core}" "owner->uploadSerial >= UINT64_MAX - 1u"
	"upload serial saturation")
require_text("${_core}" "upload.suppressMipGeneration = qtrue;"
	"graphics queue upload selection")
require_call_count("${_core}" "Ral_TextureUploadBegin[(]" 1 "single upload begin")
require_call_count("${_core}" "Ral_WaitFence[(]" 1 "upload completion wait")
require_call_count("${_core}" "Ral_TextureUploadTicketComplete[(]" 1
	"upload completion publication")
require_call_count("${_core}" "Ral_TextureAcquireBatchToGraphics[(]" 1
	"conditional graphics acquire")
require_call_count("${_core}" "Ral_TextureUploadTicketGetReceipt[(]" 1
	"completed transfer receipt")
FOREACH(_needle IN ITEMS
		"transfer->state == RAL_TRANSFER_COMPLETED"
		"transfer->request.direction == RAL_TRANSFER_UPLOAD"
		"transfer->request.resourceKind == RAL_TRANSFER_TEXTURE"
		"transfer->request.resourceIdentity == (uintptr_t)owner->texture"
		"transfer->request.resourceGeneration"
		"transfer->request.byteSize == VK_ATMOSPHERIC_HEIGHTGRID_BYTES"
		"transfer->request.byteBudget == owner->allocation.committedSize"
		"transfer->request.queue == RAL_QUEUE_GRAPHICS"
		"transfer->completionGeneration > 0u")
	require_text("${_core}" "${_needle}" "completed transfer exactness")
ENDFOREACH()
require_text("${_core}" "ownerCandidate = *owner;" "candidate-first owner update")
require_text("${_core}" "*owner = ownerCandidate;\n\t*outReceipt = candidate;"
	"atomic owner and receipt publication")
require_text("${_core}" "if ( owner->sampler ) Ral_DestroySampler( owner->sampler );"
	"sampler child teardown")
require_text("${_core}" "if ( owner->texture ) Ral_DestroyTexture( owner->texture );"
	"texture parent teardown")

# The host pins creation failures, invalid sizes, every upload stage, receipt
# mutation, repeated generations and sampler-before-texture teardown.
FOREACH(_needle IN ITEMS
		"mutation < 3u" "failCreateTexture = mutation == 0u"
		"failCreateSampler = mutation == 1u" "failAllocation = mutation == 2u"
		"VK_AtmosphericHeightgridUpload( &owner, grid, -1, &receipt )"
		"VK_ATMOSPHERIC_HEIGHTGRID_TEXELS - 1"
		"mutation < 9u" "failBegin = mutation == 0u"
		"failComplete = mutation == 1u" "failAcquire = mutation == 2u"
		"failGet = mutation == 3u" "receiptMutation = mutation >= 4u"
		"memcmp( &owner, &before, sizeof( owner ) ) == 0"
		"memcmp( &receipt, &sentinel, sizeof( receipt ) )"
		"receipt.uploadSerial == 1u" "receipt2.uploadSerial == 2u"
		"mutation < 10u" "destroyOrder[0] == 'S'"
		"destroyOrder[1] == 'T'")
	require_text("${_test}" "${_needle}" "heightgrid host mutation coverage")
ENDFOREACH()

# Product authority is intentionally narrow: only the owner TU and vk.c may use
# this transaction API. Other shipping code cannot grow a second lifecycle.
FILE(GLOB_RECURSE _shipping_sources "${SOURCE_ROOT}/code/*.c" "${SOURCE_ROOT}/code/*.h")
FOREACH(_path IN LISTS _shipping_sources)
	IF(_path STREQUAL _core_path OR _path STREQUAL _header_path OR _path STREQUAL _vk_path)
		CONTINUE()
	ENDIF()
	FILE(READ "${_path}" _shipping_body)
	forbid_text("${_shipping_body}" "VK_AtmosphericHeightgrid"
		"heightgrid API escaped its product owner: ${_path}")
ENDFOREACH()

require_text("${_cmake}" "AUX_SOURCE_DIRECTORY(code/renderervk RENDERER_VK_SRCS)"
	"product module discovery")
require_text("${_cmake}" "ADD_EXECUTABLE(vk_atmospheric_heightgrid_test"
	"heightgrid host target")
require_text("${_cmake}" "vk_atmospheric_heightgrid_source_policy_contract"
	"heightgrid source policy target")

MESSAGE(STATUS "atmospheric heightgrid upload and lifecycle are RAL-owned")
