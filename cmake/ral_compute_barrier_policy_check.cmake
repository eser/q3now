# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT required")
endif()

file(READ "${ROOT}/code/renderer/ral/ral_command.h" API)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_translate.c" TRANSLATE)
file(READ "${ROOT}/tests/ral_vulkan_translate_test.c" HOST)
file(READ "${ROOT}/code/renderervk/vk.c" PRODUCT)
file(READ "${ROOT}/tests/ral-effects-dynamic-bind-smoke.sh" SMOKE)

function(require_text body needle why)
	string(FIND "${body}" "${needle}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "RAL compute barrier policy lost ${why}: ${needle}")
	endif()
endfunction()

function(extract_between body begin end out)
	string(FIND "${body}" "${begin}" begin_pos)
	if(begin_pos EQUAL -1)
		message(FATAL_ERROR "RAL compute barrier policy missing span begin: ${begin}")
	endif()
	string(SUBSTRING "${body}" ${begin_pos} -1 tail)
	string(FIND "${tail}" "${end}" end_pos)
	if(end_pos EQUAL -1)
		message(FATAL_ERROR "RAL compute barrier policy missing span end: ${end}")
	endif()
	string(SUBSTRING "${tail}" 0 ${end_pos} span)
	set(${out} "${span}" PARENT_SCOPE)
endfunction()

require_text("${API}" "RAL_BARRIER_COMPUTE_TO_COMPUTE" "portable scope")
foreach(needle IN ITEMS
	"case RAL_BARRIER_COMPUTE_TO_COMPUTE:"
	"out.srcStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;"
	"out.dstStage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;"
	"out.srcAccess = VK_ACCESS_SHADER_WRITE_BIT;"
	"out.dstAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;")
	require_text("${TRANSLATE}" "${needle}" "Vulkan compute-to-compute lowering")
endforeach()
require_text("${HOST}"
	"{ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,"
	"translation mutation tuple")
require_text("${HOST}"
	"VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT }"
	"translation access tuple")

extract_between("${PRODUCT}" "void vk_forwardplus_dispatch( void )"
	"void vk_forwardplus_lit_shutdown( void )" FORWARD)
extract_between("${PRODUCT}" "void vk_tonemap( void )"
	"void vk_begin_bloom_extract_render_pass( void )" HDR)
foreach(needle IN ITEMS
	"RAL_BARRIER_COMPUTE_TO_COMPUTE"
	"RAL_BARRIER_COMPUTE_TO_GRAPHICS")
	require_text("${FORWARD}" "${needle}" "Forward+ semantic barrier")
endforeach()
require_text("${HDR}" "RAL_BARRIER_COMPUTE_TO_TRANSFER"
	"histogram semantic barrier")
foreach(old IN ITEMS
	"VkBufferMemoryBarrier tdb"
	"VkBufferMemoryBarrier rd"
	"VkBufferMemoryBarrier toTransfer")
	if(PRODUCT MATCHES "${old}")
		message(FATAL_ERROR "RAL compute barrier policy: raw product barrier returned: ${old}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"family=(cull|forward-plus|hdr-histogram)"
	"+set r_customwidth 1280 +set r_customheight 720")
	require_text("${SMOKE}" "${needle}" "native 1280x720 receipt authority")
endforeach()

message(STATUS "RAL semantic compute buffer barrier policy: PASS")
