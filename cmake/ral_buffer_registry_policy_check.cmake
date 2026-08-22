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

set(registry_path "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c")
set(header_path "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.h")
set(resource_path "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c")
set(bridge_path "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_bridge.h")
set(host_path "${SOURCE_ROOT}/tests/ral_vulkan_dynamic_bind_test.c")
set(product_path "${SOURCE_ROOT}/code/renderervk/vk.c")
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

slice_between(adopt_helper "${registry}"
	"static ralBuffer_t *vk_ral_adopt_registered_buffer("
	"static void vk_ral_flush_pending_buffers( void )")
slice_between(flush "${registry}"
	"static void vk_ral_flush_pending_buffers( void )"
	"static void vk_ral_destroy_all_active_buffers( void )")
slice_between(register "${registry}"
	"void vk_ral_register_buffer("
	"void vk_ral_unregister_buffer(")
slice_between(overlay "${product}"
	"static void vk_replay_overlay_quads("
	"#define DUAL_BLOOM_ENERGY_SCALE")

# Registry authority is the renderer's native buffer itself, never a shadow
# allocation. Both boot-pending and live paths use the same exact helper.
require_text("${adopt_helper}"
	"candidate = Ral_AdoptBufferExact( s_ral_backend, (void *)key, &ci );"
	"exact native adoption")
foreach(needle IN ITEMS
	"Ral_GetBufferHandle( candidate ) != (void *)key"
	"Ral_GetBufferSize( candidate ) != size"
	"Ral_GetBufferUsage( candidate ) != usage"
	"Ral_GetBufferMemoryType( candidate ) != memory")
	require_text("${adopt_helper}" "${needle}" "candidate metadata validation")
endforeach()

# The first shipping consumer now binds the registered renderer bytes directly;
# it may neither create a temporary wrapper nor destroy registry authority.
require_text("${overlay}"
	"ralBuffer_t *vb = vk_ral_lookup_buffer( vk.cmd->vertex_buffer );"
	"overlay registered-buffer lookup")
require_text("${overlay}"
	"Ral_CmdBindVertexBuffer( vk.cmd->ral_cmd, 0, vb, off );"
	"overlay registered-buffer bind")
forbid_text("${overlay}" "Ral_AdoptBuffer" "overlay ad-hoc adoption")
forbid_text("${overlay}" "Ral_DestroyBuffer" "overlay borrowed-wrapper destruction")
forbid_text("${adopt_helper}" "Ral_CreateBuffer" "parallel GPU allocation")
require_text("${flush}"
	"rb = vk_ral_adopt_registered_buffer( p->key, p->size, p->usage,"
	"pending adoption")
forbid_text("${flush}" "Ral_CreateBuffer" "pending parallel GPU allocation")
require_text("${register}"
	"rb = vk_ral_adopt_registered_buffer( key, size, usage, memory,"
	"live adoption")
forbid_text("${register}" "Ral_CreateBuffer" "live parallel GPU allocation")

# One wrapper per native identity; idempotent duplicates retain authority and
# mismatched facts cannot replace the published node.
foreach(needle IN ITEMS
	"if ( active->key != key ) continue;"
	"!vk_ral_registered_buffer_exact( active, key, size, usage, memory )"
	"if ( pending->key != key ) continue;"
	"pending->size != size || pending->usage != usage"
	"|| pending->memory != memory"
	"return;")
	require_text("${register}" "${needle}" "duplicate fail-closed gate")
endforeach()

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

# Exact adoption is a single centralized renderervk bridge. New product owners
# consume lookup wrappers rather than creating scattered native aliases.
file(GLOB product_sources "${SOURCE_ROOT}/code/renderervk/*.c")
set(exact_adopt_calls 0)
foreach(path IN LISTS product_sources)
	file(READ "${path}" body)
	string(REGEX MATCHALL "Ral_AdoptBufferExact[(]" hits "${body}")
	list(LENGTH hits count)
	math(EXPR exact_adopt_calls "${exact_adopt_calls} + ${count}")
endforeach()
if(NOT exact_adopt_calls EQUAL 1)
	message(FATAL_ERROR "exact native buffer adoption inventory changed: ${exact_adopt_calls}/1")
endif()

message(STATUS "RAL exact native buffer registry policy: PASS")
