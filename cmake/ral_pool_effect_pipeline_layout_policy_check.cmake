# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026-present Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	message(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
endif()

function(require_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "pool effect pipeline layout lost ${why}: ${needle}")
	endif()
endfunction()

function(forbid_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(NOT hit EQUAL -1)
		message(FATAL_ERROR "pool effect pipeline layout leaked ${why}: ${needle}")
	endif()
endfunction()

function(require_count body pattern expected why)
	string(REGEX MATCHALL "${pattern}" hits "${body}")
	list(LENGTH hits count)
	if(NOT count EQUAL expected)
		message(FATAL_ERROR "pool effect pipeline layout ${why}: got ${count}, want ${expected}")
	endif()
endfunction()

function(slice_between out body begin_marker end_marker)
	string(FIND "${body}" "${begin_marker}" begin)
	string(FIND "${body}" "${end_marker}" end)
	if(begin EQUAL -1 OR end EQUAL -1 OR NOT begin LESS end)
		message(FATAL_ERROR "cannot isolate pool effect pipeline-layout span: ${begin_marker} -> ${end_marker}")
	endif()
	math(EXPR length "${end} - ${begin}")
	string(SUBSTRING "${body}" ${begin} ${length} slice)
	set(${out} "${slice}" PARENT_SCOPE)
endfunction()

set(product_path "${SOURCE_ROOT}/code/renderervk/vk.c")
set(boot_path "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c")
set(host_path "${SOURCE_ROOT}/tests/ral_vulkan_dynamic_bind_test.c")
foreach(path IN ITEMS "${product_path}" "${boot_path}" "${host_path}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "pool effect pipeline-layout input missing: ${path}")
	endif()
endforeach()
file(READ "${product_path}" product)
file(READ "${boot_path}" boot)
file(READ "${host_path}" host)

slice_between(helper "${product}"
	"static qboolean vk_create_single_bind_group_pipeline_layout("
	"/*\n===============\nPrimitive ribbon")
foreach(needle IN ITEMS
		"layouts[0] = bindGroupLayout;"
		"createInfo.numBindGroupLayouts = ARRAY_LEN( layouts );"
		"candidate = Ral_CreatePipelineLayout( vk_ral_get_backend(), &createInfo );"
		"native = (VkPipelineLayout)Ral_GetPipelineLayoutHandle( candidate );"
		"Ral_DestroyPipelineLayout( candidate );"
		"*outOwner = candidate;"
		"*outNative = native;")
	require_text("${helper}" "${needle}" "candidate-first publication")
endforeach()
require_count("${product}" "vk_create_single_bind_group_pipeline_layout[(]" 6
	"helper definition/call inventory")

slice_between(particle_init "${product}"
	"void vk_init_particle( void )" "void vk_init_particle_textures( void )")
slice_between(decal_init "${product}"
	"void vk_init_decal( void )" "void vk_init_decal_textures( void )")
slice_between(atmospheric_init "${product}"
	"void vk_init_atmospheric( void )" "void vk_shutdown_atmospheric( void )")
require_count("${particle_init}" "vk_create_single_bind_group_pipeline_layout[(]" 2
	"particle compute/render direct creation")
require_count("${decal_init}" "vk_create_single_bind_group_pipeline_layout[(]" 1
	"decal render direct creation")
require_count("${atmospheric_init}" "vk_create_single_bind_group_pipeline_layout[(]" 2
	"atmospheric compute/render direct creation")
foreach(span IN ITEMS particle_init decal_init atmospheric_init)
	forbid_text("${${span}}" "qvkCreatePipelineLayout" "${span} raw create")
	forbid_text("${${span}}" "vk_ral_adopt_one_pipeline_layout" "${span} create-then-adopt")
endforeach()

foreach(needle IN ITEMS
		"\"wired-pl-particle-compute\""
		"\"wired-pl-particle-render\""
		"\"wired-pl-decal-render\""
		"\"wired-pl-atmospheric-compute\""
		"\"wired-pl-atmospheric-render\""
		"&vk.particle.ral_compute_pipeline_layout"
		"&vk.particle.ral_render_pipeline_layout"
		"&vk.decal.ral_render_pipeline_layout"
		"&vk.atm.ral_compute_pipeline_layout"
		"&vk.atm.ral_render_pipeline_layout")
	require_text("${product}" "${needle}" "exact owner/mirror publication")
endforeach()

slice_between(particle_shutdown "${product}"
	"void vk_shutdown_particle( void )" "void vk_init_decal( void )")
slice_between(decal_shutdown "${product}"
	"void vk_shutdown_decal( void )" "void vk_atmospheric_write_descriptors( void )")
slice_between(atmospheric_shutdown "${product}"
	"void vk_shutdown_atmospheric( void )" "void RB_RunAtmosphericCompute( void )")
foreach(span IN ITEMS particle_shutdown decal_shutdown atmospheric_shutdown)
	forbid_text("${${span}}" "qvkDestroyPipelineLayout" "${span} raw destroy")
	string(FIND "${${span}}" "Ral_DestroyPipeline(" destroy_pipeline)
	string(FIND "${${span}}" "Ral_DestroyPipelineLayout(" destroy_pipeline_layout)
	string(FIND "${${span}}" "Ral_DestroyBindGroupLayout(" destroy_bgl)
	if(destroy_pipeline EQUAL -1 OR destroy_pipeline_layout EQUAL -1 OR destroy_bgl EQUAL -1
			OR NOT destroy_pipeline LESS destroy_pipeline_layout
			OR NOT destroy_pipeline_layout LESS destroy_bgl)
		message(FATAL_ERROR "${span} pipeline -> pipeline-layout -> BGL teardown order drifted")
	endif()
endforeach()

slice_between(adopt_sweep "${boot}"
	"void vk_ral_adopt_static_pipeline_layouts( void )"
	"void vk_ral_adopt_one_pipeline_layout(")
foreach(field IN ITEMS
		"vk.particle.ral_compute_pipeline_layout"
		"vk.particle.ral_render_pipeline_layout"
		"vk.decal.ral_render_pipeline_layout"
		"vk.atm.ral_compute_pipeline_layout"
		"vk.atm.ral_render_pipeline_layout")
	forbid_text("${adopt_sweep}" "${field}" "direct owner in adoption sweep")
endforeach()
foreach(field IN ITEMS particle decal atm)
	forbid_text("${product}" "vk_ral_adopt_one_pipeline_layout( vk.${field}"
		"${field} create-then-adopt")
endforeach()

slice_between(full_shutdown "${boot}"
	"void vk_ral_textures_shutdown( qboolean destroyWindow )"
	"// ── per-image registration")
string(FIND "${full_shutdown}" "vk_shutdown_particle();" direct_shutdown)
string(FIND "${full_shutdown}" "vk_ral_destroy_adopted_pipeline_layouts();" adopted_shutdown)
if(direct_shutdown EQUAL -1 OR adopted_shutdown EQUAL -1
		OR NOT direct_shutdown LESS adopted_shutdown)
	message(FATAL_ERROR "full teardown no longer retires direct pool effect pipeline layouts first")
endif()

foreach(needle IN ITEMS
		"CaptureCreatePipelineLayout"
		"capturedLayoutCount == 2u"
		"portablePipeline->bindGroupLayoutsRegistered"
		"pipelineLayoutInfo.numBindGroupLayouts = RAL_MAX_PIPELINE_BIND_GROUP_LAYOUTS + 1u;"
		"createPipelineLayoutResult = VK_ERROR_OUT_OF_HOST_MEMORY"
		"capturedDestroyedPipelineLayout == (VkPipelineLayout)(uintptr_t)0x74u")
	require_text("${host}" "${needle}" "backend mutation coverage")
endforeach()

message(STATUS "particle/decal/atmospheric pipeline layouts are directly RAL-owned")
