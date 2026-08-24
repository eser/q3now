# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026-present Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	message(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
endif()

function(require_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "pool effect bind-group layout lost ${why}: ${needle}")
	endif()
endfunction()

function(forbid_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(NOT hit EQUAL -1)
		message(FATAL_ERROR "pool effect bind-group layout leaked ${why}: ${needle}")
	endif()
endfunction()

function(require_count body pattern expected why)
	string(REGEX MATCHALL "${pattern}" hits "${body}")
	list(LENGTH hits count)
	if(NOT count EQUAL expected)
		message(FATAL_ERROR "pool effect bind-group layout ${why}: got ${count}, want ${expected}")
	endif()
endfunction()

function(slice_between out body begin_marker end_marker)
	string(FIND "${body}" "${begin_marker}" begin)
	string(FIND "${body}" "${end_marker}" end)
	if(begin EQUAL -1 OR end EQUAL -1 OR NOT begin LESS end)
		message(FATAL_ERROR "cannot isolate pool effect span: ${begin_marker} -> ${end_marker}")
	endif()
	math(EXPR length "${end} - ${begin}")
	string(SUBSTRING "${body}" ${begin} ${length} slice)
	set(${out} "${slice}" PARENT_SCOPE)
endfunction()

set(product_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.c")
set(boot_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.c")
set(host_path "${SOURCE_ROOT}/tests/ral_vulkan_bind_group_arena_test.c")
foreach(path IN ITEMS "${product_path}" "${boot_path}" "${host_path}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "pool effect bind-group layout input missing: ${path}")
	endif()
endforeach()
file(READ "${product_path}" product)
file(READ "${boot_path}" boot)
file(READ "${host_path}" host)

slice_between(particle_init "${product}"
	"void vk_init_particle( void )" "void vk_init_particle_textures( void )")
slice_between(decal_init "${product}"
	"void vk_init_decal( void )" "void vk_init_decal_textures( void )")
slice_between(atmospheric_init "${product}"
	"void vk_init_atmospheric( void )" "void vk_shutdown_atmospheric( void )")
require_count("${particle_init}" "vk_create_effect_bind_group_layout[(]" 2
	"particle compute/render direct creation")
require_count("${decal_init}" "vk_create_effect_bind_group_layout[(]" 1
	"decal render direct creation")
require_count("${atmospheric_init}" "vk_create_effect_bind_group_layout[(]" 2
	"atmospheric compute/render direct creation")
foreach(span IN ITEMS particle_init decal_init atmospheric_init)
	forbid_text("${${span}}" "qvkCreateDescriptorSetLayout" "${span} raw create")
	forbid_text("${${span}}" "Ral_AdoptBindGroupLayout" "${span} create-then-adopt")
endforeach()

foreach(needle IN ITEMS
		"\"wired-particle-compute-set-layout\""
		"\"wired-particle-render-set-layout\""
		"\"wired-decal-render-set-layout\""
		"\"wired-atmospheric-compute-set-layout\""
		"\"wired-atmospheric-render-set-layout\""
		"&vk.particle.ral_bgl_compute"
		"&vk.particle.ral_bgl_render"
		"&vk.decal.ral_bgl_render"
		"&vk.atm.ral_bgl_compute"
		"&vk.atm.ral_bgl_render")
	require_text("${product}" "${needle}" "exact owner/mirror publication")
endforeach()

slice_between(particle_shutdown "${product}"
	"void vk_shutdown_particle( void )" "void vk_init_decal( void )")
slice_between(decal_shutdown "${product}"
	"void vk_shutdown_decal( void )" "void vk_atmospheric_write_descriptors( void )")
slice_between(atmospheric_shutdown "${product}"
	"void vk_shutdown_atmospheric( void )" "void RB_RunAtmosphericCompute( void )")
foreach(span IN ITEMS particle_shutdown decal_shutdown atmospheric_shutdown)
	forbid_text("${${span}}" "qvkDestroyDescriptorSetLayout" "${span} raw destroy")
	string(FIND "${${span}}" "Ral_DestroyPipelineLayout(" destroy_pipeline_layout)
	string(FIND "${${span}}" "Ral_DestroyBindGroupLayout(" destroy_bgl)
	if(destroy_pipeline_layout EQUAL -1 OR destroy_bgl EQUAL -1
			OR NOT destroy_pipeline_layout LESS destroy_bgl)
		message(FATAL_ERROR "${span} no longer retires pipeline-layout before BGL")
	endif()
endforeach()

slice_between(full_shutdown "${boot}"
	"void vk_ral_textures_shutdown( qboolean destroyWindow )"
	"// ── per-image registration")
string(FIND "${full_shutdown}" "vk_shutdown_particle();" direct_shutdown)
string(FIND "${full_shutdown}" "vk_ral_destroy_adopted_bindgroups();" adopted_shutdown)
if(direct_shutdown EQUAL -1 OR adopted_shutdown EQUAL -1
		OR NOT direct_shutdown LESS adopted_shutdown)
	message(FATAL_ERROR "full teardown no longer retires pool effect families first")
endif()
foreach(needle IN ITEMS
		"vk_shutdown_particle();" "vk_shutdown_decal();"
		"vk_shutdown_atmospheric();")
	require_text("${full_shutdown}" "${needle}" "full teardown family coverage")
endforeach()
foreach(field IN ITEMS
		"vk.particle.ral_bgl_compute" "vk.particle.ral_bgl_render"
		"vk.decal.ral_bgl_render" "vk.atm.ral_bgl_compute"
		"vk.atm.ral_bgl_render")
	forbid_text("${boot}" "KILL_BGL( ${field} )" "direct owner in central sweep")
endforeach()

foreach(needle IN ITEMS
		"CaptureCreateDescriptorSetLayout"
		"effectLayout->ownsLayout == qtrue"
		"destroyLayoutCalls == 1u"
		"effectEntries[1].binding = 0u"
		"effectEntries[2].dynamicOffset = qtrue"
		"createLayoutResult = VK_ERROR_OUT_OF_HOST_MEMORY")
	require_text("${host}" "${needle}" "backend mutation coverage")
endforeach()

message(STATUS "particle/decal/atmospheric bind-group layouts are directly RAL-owned")
