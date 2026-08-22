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

# Product calls only the central registry resolver and never loads or emits
# vkCmdCopyBuffer itself. The nine former sites preserve their exact offsets,
# sizes and native source/destination identities as bridge inputs.
forbid_regex("${product}" "(^|[^A-Za-z0-9_])qvkCmdCopyBuffer([^A-Za-z0-9_]|$)"
	"raw product command")
forbid_regex("${product}" "PFN_vkCmdCopyBuffer([^A-Za-z0-9_]|$)"
	"raw product PFN")
foreach(needle IN ITEMS
	"ralBuffer_t *src = vk_ral_lookup_buffer( srcNative );"
	"ralBuffer_t *dst = vk_ral_lookup_buffer( dstNative );"
	"Ral_GetBufferHandle( src ) != (void *)srcNative"
	"Ral_GetBufferHandle( dst ) != (void *)dstNative"
	"return Ral_CmdCopyBufferExact( command, src, dst, copy );"
	"vk.staging_buffer.handle, *outVertBuf, &copyRegion"
	"vk.staging_buffer.handle, *outIdxBuf, &copyRegion"
	"vk.staging_buffer.handle, vk.vbo.vertex_buffer,"
	"vk.staging_buffer.handle, vk.shadowMap.casterBmodelBuf,"
	"vk.staging_buffer.handle, vk.shadowMap.casterBuf, &region"
	"vk.shadowMap.casterAtestBuf, &aregion"
	"copyRegion.srcOffset = vertSize;"
	"copyRegion.dstOffset = uploadDone;"
	"region.dstOffset = vBytes + uploadDone;"
	"aregion.dstOffset = avBytes + aDone;")
	require_text("${product}" "${needle}" "registered product operand seam")
endforeach()
string(REGEX MATCHALL "if [(] !vk_ral_record_registered_buffer_copy[(]" product_calls "${product}")
list(LENGTH product_calls product_call_count)
if(NOT product_call_count EQUAL 9)
	message(FATAL_ERROR "registered product copy inventory changed: ${product_call_count}/9")
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
