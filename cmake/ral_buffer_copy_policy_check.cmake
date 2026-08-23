# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026-present Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	message(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
endif()

function(require_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "RAL buffer copy lost ${why}: ${needle}")
	endif()
endfunction()

function(forbid_regex body pattern why)
	string(REGEX MATCH "${pattern}" hit "${body}")
	if(hit)
		message(FATAL_ERROR "RAL buffer copy leaked ${why}: ${hit}")
	endif()
endfunction()

function(slice_between out body begin_marker end_marker)
	string(FIND "${body}" "${begin_marker}" begin)
	if(begin EQUAL -1)
		message(FATAL_ERROR "RAL buffer copy span begin missing: ${begin_marker}")
	endif()
	string(SUBSTRING "${body}" ${begin} -1 tail)
	string(FIND "${tail}" "${end_marker}" relative_end)
	if(relative_end EQUAL -1)
		message(FATAL_ERROR "RAL buffer copy span end missing: ${end_marker}")
	endif()
	string(SUBSTRING "${tail}" 0 ${relative_end} span)
	set(${out} "${span}" PARENT_SCOPE)
endfunction()

set(command_header_path "${SOURCE_ROOT}/code/renderer/ral/ral_command.h")
set(command_path "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_command.c")
set(product_path "${SOURCE_ROOT}/code/renderervk/vk.c")
set(host_path "${SOURCE_ROOT}/tests/ral_vulkan_buffer_copy_test.c")
foreach(path IN ITEMS "${command_header_path}" "${command_path}"
		"${product_path}" "${host_path}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "RAL buffer copy input missing: ${path}")
	endif()
endforeach()
file(READ "${command_header_path}" command_header)
file(READ "${command_path}" command)
file(READ "${product_path}" product)
file(READ "${host_path}" host)
slice_between(public_copy "${command_header}"
	"qboolean Ral_CmdCopyBufferExact("
	"void Ral_CmdCopyBufferToTexture")

require_text("${command_header}"
	"qboolean Ral_CmdCopyBufferExact( ralCommandBuffer_t *cb, ralBuffer_t *src,"
	"portable exact API")
forbid_regex("${public_copy}" "(^|[^A-Za-z0-9_])(VkBuffer|VkBufferCopy)([^A-Za-z0-9_]|$)"
	"native type in portable API")

foreach(needle IN ITEMS
	"src->backend != cb->backend || dst->backend != cb->backend"
	"!( src->usage & RAL_BUFFER_TRANSFER_SRC )"
	"!( dst->usage & RAL_BUFFER_TRANSFER_DST )"
	"!ralVk_BufferGpuUseAllowed( src ) || !ralVk_BufferGpuUseAllowed( dst )"
	"cb->state != RAL_VK_CMD_RECORDING"
	"cb->lifecycle.state != RAL_COMMAND_RECORDING"
	"region->size == 0u"
	"region->size > src->size - region->srcOffset"
	"region->size > dst->size - region->dstOffset"
	"src == dst"
	"region->srcOffset < region->dstOffset + region->size"
	"region->dstOffset < region->srcOffset + region->size"
	"cb->backend->vk.CmdCopyBuffer( cb->cb, src->buffer, dst->buffer, 1, &r );"
	"return qtrue;")
	require_text("${command}" "${needle}" "exact validation/emission")
endforeach()
require_text("${command}"
	"(void)Ral_CmdCopyBufferExact( cb, src, dst, region );"
	"compatibility forwarding")

# Product owns one portable staging transaction. It maps and unmaps through a
# generation-bound ticket, records explicit HOST_WRITE/COPY_SOURCE transitions,
# and only then emits exact typed copies. No native registry or raw command
# fallback is permitted.
forbid_regex("${product}" "(^|[^A-Za-z0-9_])qvkCmdCopyBuffer([^A-Za-z0-9_]|$)"
	"raw product command")
forbid_regex("${product}" "PFN_vkCmdCopyBuffer([^A-Za-z0-9_]|$)"
	"raw product PFN")
foreach(needle IN ITEMS
	"static qboolean vk_ral_stage_buffer_copy( ralBuffer_t *destination,"
	"vk_ral_write_upload_buffer( vk.staging_buffer.ral_buffer,"
	"RAL_RESOURCE_USAGE_HOST_WRITE,"
	"RAL_RESOURCE_USAGE_COPY_SOURCE )"
	"recorded = Ral_CmdCopyBufferExact( command,"
	"recorded = vk_ral_stage_buffer_copy( vertCandidate, 0u, vertData,"
	"vk_ral_stage_buffer_copy( idxCandidate, 0u,"
	"vk_ral_stage_buffer_copy( candidate, uploadDone,"
	"vk_ral_stage_buffer_copy( vk.shadowMap.ral_casterBmodelBuf,"
	"vk_ral_stage_buffer_copy( vk.shadowMap.ral_casterBuf,"
	"vk_ral_stage_buffer_copy( vk.shadowMap.ral_casterAtestBuf,")
	require_text("${product}" "${needle}" "portable staging operand seam")
endforeach()
forbid_regex("${product}" "vk_ral_(lookup|register|unregister)_buffer"
	"retired native buffer registry")
string(REGEX MATCHALL "vk_ral_stage_buffer_copy[(]" product_calls "${product}")
list(LENGTH product_calls product_call_count)
if(NOT product_call_count EQUAL 10)
	message(FATAL_ERROR "portable staging buffer-copy inventory changed: ${product_call_count}/10")
endif()

foreach(needle IN ITEMS
	"bad.size = 0u"
	"bad.srcOffset = 200u"
	"bad.dstOffset = 400u"
	"src.usage &= ~RAL_BUFFER_TRANSFER_SRC"
	"dst.usage &= ~RAL_BUFFER_TRANSFER_DST"
	"dst.backend = &otherBackend"
	"src.legacyMapped = qtrue"
	"command.lifecycle.state = RAL_COMMAND_IDLE"
	"Ral_CmdCopyBufferExact( &command, &src, &src, &bad )"
	"bad.srcOffset = 0u; bad.dstOffset = 128u; bad.size = 64u"
	"copyCalls == 1u && capturedSrc == capturedDst")
	require_text("${host}" "${needle}" "mutation host coverage")
endforeach()

message(STATUS "RAL exact renderer buffer-copy policy: PASS")
