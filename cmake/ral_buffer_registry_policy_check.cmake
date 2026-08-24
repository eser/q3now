# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026-present Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	message(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
endif()

function(require_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "RAL buffer registry lost ${why}: ${needle}")
	endif()
endfunction()

function(forbid_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(NOT hit EQUAL -1)
		message(FATAL_ERROR "RAL buffer registry leaked ${why}: ${needle}")
	endif()
endfunction()

function(slice_between out body begin_marker end_marker)
	string(FIND "${body}" "${begin_marker}" begin)
	if(begin EQUAL -1)
		message(FATAL_ERROR "RAL buffer registry span begin missing: ${begin_marker}")
	endif()
	string(SUBSTRING "${body}" ${begin} -1 tail)
	string(FIND "${tail}" "${end_marker}" relative_end)
	if(relative_end EQUAL -1)
		message(FATAL_ERROR "RAL buffer registry span end missing: ${end_marker}")
	endif()
	string(SUBSTRING "${tail}" 0 ${relative_end} span)
	set(${out} "${span}" PARENT_SCOPE)
endfunction()

set(registry_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.c")
set(header_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.h")
set(resource_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_resource.c")
set(bridge_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/ral_vulkan_bridge.h")
set(host_path "${SOURCE_ROOT}/tests/ral_vulkan_dynamic_bind_test.c")
set(product_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.c")
foreach(path IN ITEMS "${registry_path}" "${header_path}" "${resource_path}"
		"${bridge_path}" "${host_path}" "${product_path}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "RAL buffer registry input missing: ${path}")
	endif()
endforeach()
file(READ "${registry_path}" registry)
file(READ "${header_path}" header)
file(READ "${resource_path}" resource)
file(READ "${bridge_path}" bridge)
file(READ "${host_path}" host)
file(READ "${product_path}" product)

slice_between(overlay "${product}"
	"static void vk_replay_overlay_quads("
	"#define DUAL_BLOOM_ENERGY_SCALE")

# TASK-202.5 retired the renderer's native-buffer registry. Product buffers
# now carry their exact RAL owner next to the native compatibility mirror, and
# command recording consumes that owner directly.
file(GLOB product_sources
	"${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/*.c"
	"${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/*.h")
set(product_all "")
foreach(path IN LISTS product_sources)
	file(READ "${path}" body)
	string(APPEND product_all "\n${body}")
endforeach()
foreach(retired IN ITEMS
	"vk_ral_register_buffer("
	"vk_ral_unregister_buffer("
	"vk_ral_lookup_buffer("
	"vk_ral_adopt_registered_buffer("
	"Ral_AdoptBufferExact(")
	forbid_text("${product_all}" "${retired}" "retired product buffer registry")
endforeach()
foreach(needle IN ITEMS
	"Buffers are direct RAL resources"
	"struct ralBuffer_s *ral_vertex_buffer;"
	"struct ralBuffer_s *ral_entMatBuf;"
	"vk.cmd->ral_vertex_buffer"
	"data->ral_index_buffer")
	require_text("${product_all}" "${needle}" "direct renderer buffer identity")
endforeach()
require_text("${overlay}"
	"ralBuffer_t *vb = vk.cmd->ral_vertex_buffer;"
	"overlay direct RAL-buffer owner")
require_text("${overlay}"
	"Ral_CmdBindVertexBuffer( vk.cmd->ral_cmd, 0, vb, off );"
	"overlay direct RAL-buffer bind")
forbid_text("${overlay}" "Ral_AdoptBuffer" "overlay ad-hoc adoption")
forbid_text("${overlay}" "Ral_DestroyBuffer" "overlay borrowed-wrapper destruction")

# The backend bridge retains portable capabilities but never grants mapping or
# raw-parent destruction authority to an adopted wrapper.
foreach(needle IN ITEMS
	"ralBuffer_t *Ral_AdoptBufferExact("
	"ralBufferUsage_t Ral_GetBufferUsage("
	"ralMemoryType_t Ral_GetBufferMemoryType(")
	require_text("${bridge}" "${needle}" "exact bridge declaration")
endforeach()
foreach(needle IN ITEMS
	"buf->usage           = usage;"
	"buf->memoryType      = memory;"
	"buf->hostVisible     = qfalse;"
	"buf->ownsBuffer      = qfalse;"
	"if ( buf->ownsBuffer )")
	require_text("${resource}" "${needle}" "non-owning metadata lifecycle")
endforeach()

# The host pins positive identity/size/usage/memory facts and malformed exact
# imports. This also proves DestroyBuffer does not need a native destroy hook.
foreach(needle IN ITEMS
	"adoptedBuffer = Ral_AdoptBufferExact( &backend,"
	"Ral_GetBufferHandle( adoptedBuffer ) == (void *)(uintptr_t)0x51u"
	"Ral_GetBufferSize( adoptedBuffer ) == 2048u"
	"Ral_GetBufferUsage( adoptedBuffer )"
	"Ral_GetBufferMemoryType( adoptedBuffer ) == RAL_MEMORY_HOST_COHERENT"
	"adoptedBufferInfo.size = 0u;"
	"adoptedBufferInfo.usage = (ralBufferUsage_t)0;"
	"RAL_MEMORY_LAZY_ALLOC + 1")
	require_text("${host}" "${needle}" "exact adoption host coverage")
endforeach()

# Exact adoption remains backend-only; shipping renderervk must have zero calls.
file(GLOB product_sources "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/*.c")
set(exact_adopt_calls 0)
foreach(path IN LISTS product_sources)
	file(READ "${path}" body)
	string(REGEX MATCHALL "Ral_AdoptBufferExact[(]" hits "${body}")
	list(LENGTH hits count)
	math(EXPR exact_adopt_calls "${exact_adopt_calls} + ${count}")
endforeach()
if(NOT exact_adopt_calls EQUAL 0)
	message(FATAL_ERROR "exact native buffer adoption returned to product sources: ${exact_adopt_calls}")
endif()

message(STATUS "RAL direct buffer ownership / registry retirement policy: PASS")
