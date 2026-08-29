# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT required")
endif()

file(READ "${ROOT}/code/render/ral/core/ral_command.h" API)
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_command.c" CORE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/ral_vulkan_backend.c" BACKEND)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" PRODUCT)
file(READ "${ROOT}/tests/ral_vulkan_clear_storage_test.c" HOST)
file(READ "${ROOT}/tests/vk_atmospheric_pool_test.c" UPLOAD_HOST)
file(READ "${ROOT}/tests/ral-effects-dynamic-bind-smoke.sh" SMOKE)

function(require_text body needle why)
	string(FIND "${body}" "${needle}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "RAL zero-clear policy lost ${why}: ${needle}")
	endif()
endfunction()

function(extract_between body begin end out)
	string(FIND "${body}" "${begin}" begin_pos)
	if(begin_pos EQUAL -1)
		message(FATAL_ERROR "RAL zero-clear policy missing span begin: ${begin}")
	endif()
	string(SUBSTRING "${body}" ${begin_pos} -1 tail)
	string(FIND "${tail}" "${end}" end_pos)
	if(end_pos EQUAL -1)
		message(FATAL_ERROR "RAL zero-clear policy missing span end: ${end}")
	endif()
	string(SUBSTRING "${tail}" 0 ${end_pos} span)
	set(${out} "${span}" PARENT_SCOPE)
endfunction()

require_text("${API}" "Ral_CmdClearStorageBuffer" "public command")
require_text("${API}" "GPUCommandEncoder.clearBuffer" "WebGPU shape")
foreach(needle IN ITEMS
	"size == 0u"
	"( offset & 3u ) != 0u"
	"( size & 3u ) != 0u"
	"RAL_BUFFER_STORAGE"
	"RAL_BUFFER_TRANSFER_DST"
	"VK_PIPELINE_STAGE_ALL_COMMANDS_BIT"
	"VK_PIPELINE_STAGE_TRANSFER_BIT"
	"VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT"
	"VK_ACCESS_TRANSFER_WRITE_BIT"
	"VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT")
	require_text("${CORE}" "${needle}" "core contract")
endforeach()
require_text("${BACKEND}" "LOAD_DEV( CmdFillBuffer,                  vkCmdFillBuffer )" "PFN load")
foreach(needle IN ITEMS
	"fillValue == 0u"
	"barrierCalls == 2u && fillCalls == 1u"
	"command.lifecycle.state = RAL_COMMAND_IDLE; REJECT"
	"buffer.queueTransfer.pending.ready = qtrue"
	"buffer.legacyMapped = qtrue")
	require_text("${HOST}" "${needle}" "host mutation")
endforeach()

extract_between("${PRODUCT}" "void vk_cull_dispatch( void )" "void vk_forwardplus_dispatch( void )" CULL)
extract_between("${PRODUCT}" "void vk_forwardplus_dispatch( void )" "void vk_forwardplus_lit_shutdown( void )" FORWARD)
extract_between("${PRODUCT}" "void vk_tonemap( void )" "void vk_begin_bloom_extract_render_pass( void )" HISTOGRAM)
extract_between("${HISTOGRAM}" "// Zero the bins before accumulating" "// Make the scene's colour writes" HISTOGRAM_CLEAR)
foreach(span IN ITEMS CULL FORWARD HISTOGRAM_CLEAR)
	require_text("${${span}}" "Ral_CmdClearStorageBuffer(" "${span} typed consumer")
	if("${${span}}" MATCHES "qvkCmdFillBuffer[(]" OR "${${span}}" MATCHES "VkBufferMemoryBarrier[ \t]+clr")
		message(FATAL_ERROR "RAL zero-clear policy: raw clear escaped in ${span}")
	endif()
endforeach()
require_text("${CULL}" "vk.ral_cull_visible, 0u, sizeof( uint32_t )" "cull exact range")
require_text("${FORWARD}" "Ral_GetBufferSize( vk.ral_fp_tilelights[ vk.cmd_index ] )" "Forward+ exact range")
require_text("${HISTOGRAM}" "(uint64_t)VK_HDR_HISTOGRAM_BINS * sizeof( uint32_t )" "histogram exact range")

string(REGEX MATCHALL "qvkCmdFillBuffer[(]" raw_fills "${PRODUCT}")
list(LENGTH raw_fills raw_fill_count)
if(NOT raw_fill_count EQUAL 0)
	message(FATAL_ERROR "RAL zero-clear policy expected no product raw fill, got ${raw_fill_count}")
endif()
if(PRODUCT MATCHES "qvkCmdFillBuffer" OR PRODUCT MATCHES "INIT_DEVICE_FUNCTION[(]vkCmdFillBuffer[)]")
	message(FATAL_ERROR "RAL zero-clear policy: product retained raw fill PFN/load state")
endif()
if(PRODUCT MATCHES "exposureAccumulatorSeeded")
	message(FATAL_ERROR "RAL zero-clear policy: command-time exposure seed state returned")
endif()
foreach(needle IN ITEMS
	"const uint32_t seedBits = 0x3F800000u;"
	"Ral_BufferGetAllocationReceipt( candidate, &allocation )"
	"Ral_BufferUploadBegin( candidate, 0u,"
	"Ral_BufferUploadTicketComplete( &ticket )"
	"Ral_BufferAcquireBatchToGraphics( backend, &ticket, 1u )"
	"Ral_BufferUploadTicketGetReceipt( &ticket, &upload )"
	"vk_hdr_exposure_seed_receipts_valid( candidate,"
	"&& vk_hdr_exposure_seed_valid() ) ? qtrue : qfalse")
	require_text("${PRODUCT}" "${needle}" "generation-bound exposure seed")
endforeach()
foreach(needle IN ITEMS
	"bad.uploads[0].transfer.request.byteSize--"
	"bad.uploads[1].graphicsVisibilityGeneration++")
	require_text("${UPLOAD_HOST}" "${needle}" "upload receipt mutation host")
endforeach()

foreach(needle IN ITEMS
	"ral-storage-clear schema=1 map=%s family=%s bytes=%llu visibility=transfer-to-compute"
	"vk_storage_clear_smoke_receipt( 0u, \"cull\", sizeof( uint32_t ) )"
	"vk_storage_clear_smoke_receipt( 1u, \"forward-plus\""
	"vk_storage_clear_smoke_receipt( 2u, \"hdr-histogram\""
	"vk_storage_clear_smoke_receipt( 3u, \"particle-child-budgets\"")
	require_text("${PRODUCT}" "${needle}" "native storage-clear receipt")
endforeach()
foreach(needle IN ITEMS
	"family=(cull|forward-plus|hdr-histogram|particle-child-budgets)"
	"visibility=transfer-to-compute"
	"len(clear_found) not in (6,8)"
	"family=hdr-exposure bytes=4 bits=0x3f800000 transfer=completed graphics-visible=1"
	"len(seed_found)!=2"
	"+set r_customwidth 1280 +set r_customheight 720"
	"+set r_forwardPlus 1 +set r_hdrAutoExposure 1")
	require_text("${SMOKE}" "${needle}" "native storage-clear smoke authority")
endforeach()

message(STATUS "RAL Vulkan/WebGPU zero-clear storage policy: PASS")
