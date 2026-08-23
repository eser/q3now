# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026-present Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	message(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
endif()

function(require_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "effect pipeline-layout lost ${why}: ${needle}")
	endif()
endfunction()

function(forbid_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(NOT hit EQUAL -1)
		message(FATAL_ERROR "effect pipeline-layout leaked ${why}: ${needle}")
	endif()
endfunction()

function(require_count body pattern expected why)
	string(REGEX MATCHALL "${pattern}" hits "${body}")
	list(LENGTH hits count)
	if(NOT count EQUAL expected)
		message(FATAL_ERROR "effect pipeline-layout ${why}: got ${count}, want ${expected}")
	endif()
endfunction()

function(slice_between out body begin_marker end_marker)
	string(FIND "${body}" "${begin_marker}" begin)
	string(FIND "${body}" "${end_marker}" end)
	if(begin EQUAL -1 OR end EQUAL -1 OR NOT begin LESS end)
		message(FATAL_ERROR "cannot isolate effect pipeline-layout span: ${begin_marker} -> ${end_marker}")
	endif()
	math(EXPR length "${end} - ${begin}")
	string(SUBSTRING "${body}" ${begin} ${length} slice)
	set(${out} "${slice}" PARENT_SCOPE)
endfunction()

set(public_path "${SOURCE_ROOT}/code/renderer/ral/ral_resource.h")
set(internal_path "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_internal.h")
set(resource_path "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c")
set(pipeline_path "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_pipeline.c")
set(product_path "${SOURCE_ROOT}/code/renderervk/vk.c")
set(boot_path "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c")
set(host_path "${SOURCE_ROOT}/tests/ral_vulkan_dynamic_bind_test.c")
foreach(path IN ITEMS "${public_path}" "${internal_path}" "${resource_path}"
		"${pipeline_path}" "${product_path}" "${boot_path}" "${host_path}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "effect pipeline-layout input missing: ${path}")
	endif()
endforeach()
file(READ "${public_path}" public)
file(READ "${internal_path}" internal)
file(READ "${resource_path}" resource)
file(READ "${pipeline_path}" pipeline)
file(READ "${product_path}" product)
file(READ "${boot_path}" boot)
file(READ "${host_path}" host)

foreach(needle IN ITEMS
		"#define RAL_MAX_PIPELINE_BIND_GROUP_LAYOUTS 8u"
		"const ralBindGroupLayout_t *const *bindGroupLayouts;"
		"uint32_t                    numBindGroupLayouts;"
		"uint32_t                    pushConstantSize;"
		"uint32_t                    pushConstantStages;"
		"ralPipelineLayout_t *Ral_CreatePipelineLayout( ralBackend_t *backend,")
	require_text("${public}" "${needle}" "portable public declaration")
endforeach()
foreach(needle IN ITEMS
		"qboolean          portableShapeKnown;"
		"VkDescriptorSetLayout bindGroupLayouts[ RAL_MAX_PIPELINE_BIND_GROUP_LAYOUTS ];"
		"candidate->ownsHandle = qtrue;"
		"candidate->portableShapeKnown = qtrue;"
		"b->vk.CreatePipelineLayout( b->device, &nativeInfo, NULL,"
		"b->vk.DestroyPipelineLayout( b->device, candidate->vkHandle, NULL );")
	require_text("${internal}${resource}" "${needle}" "owned backend lowering")
endforeach()
foreach(needle IN ITEMS
		"ci->externalLayout->portableShapeKnown"
		"p->bindGroupLayoutsRegistered = qtrue;"
		"p->numSetLayouts = ci->externalLayout->numBindGroupLayouts;"
		"ci->externalLayout->bindGroupLayouts,")
	require_text("${pipeline}" "${needle}" "portable external-layout ABI propagation")
endforeach()

slice_between(helper "${product}"
	"static qboolean vk_create_effect_pipeline_layout("
	"/*\n===============\nPrimitive ribbon")
foreach(needle IN ITEMS
		"layouts[0] = effectLayout;"
		"layouts[1] = vk.ral_bgl_effects_ubo;"
		"candidate = Ral_CreatePipelineLayout( vk_ral_get_backend(), &createInfo );"
		"native = (VkPipelineLayout)Ral_GetPipelineLayoutHandle( candidate );"
		"Ral_DestroyPipelineLayout( candidate );"
		"*outOwner = candidate;"
		"*outNative = native;")
	require_text("${helper}" "${needle}" "candidate-first product publication")
endforeach()
require_count("${product}" "vk_create_effect_pipeline_layout[(]" 5
	"helper definition/call inventory")

set(families ribbon railribbon beam sprite)
foreach(family IN LISTS families)
	if(family STREQUAL "ribbon")
		set(next "void vk_init_railribbon( void )")
	elseif(family STREQUAL "railribbon")
		set(next "void vk_init_beam( void )")
	elseif(family STREQUAL "beam")
		set(next "void vk_init_sprite( void )")
	else()
		set(next "void vk_init_particle( void )")
	endif()
	slice_between(init "${product}" "void vk_init_${family}( void )" "${next}")
	require_count("${init}" "vk_create_effect_pipeline_layout[(]" 1
		"${family} direct RAL pipeline-layout creation")
	forbid_text("${init}" "qvkCreatePipelineLayout" "${family} raw create")
	forbid_text("${init}" "vk_ral_adopt_one_pipeline_layout" "${family} create-then-adopt")

	slice_between(shutdown "${product}" "void vk_shutdown_${family}( void )" "${next}")
	string(FIND "${shutdown}" "Ral_DestroyPipeline(" destroy_pipeline)
	string(FIND "${shutdown}" "Ral_DestroyPipelineLayout(" destroy_pipeline_layout)
	string(FIND "${shutdown}" "Ral_DestroyBindGroupLayout(" destroy_bgl)
	if(destroy_pipeline EQUAL -1 OR destroy_pipeline_layout EQUAL -1 OR destroy_bgl EQUAL -1
			OR NOT destroy_pipeline LESS destroy_pipeline_layout
			OR NOT destroy_pipeline_layout LESS destroy_bgl)
		message(FATAL_ERROR "${family} pipeline -> pipeline-layout -> BGL teardown order drifted")
	endif()
	forbid_text("${shutdown}" "qvkDestroyPipelineLayout" "${family} raw destroy")
endforeach()

slice_between(adopt_sweep "${boot}"
	"void vk_ral_adopt_static_pipeline_layouts( void )"
	"void vk_ral_adopt_one_pipeline_layout(")
slice_between(kill_sweep "${boot}"
	"static void vk_ral_destroy_adopted_pipeline_layouts( void )"
	"ralBindGroup_t *vk_ral_lookup_bindgroup(")
foreach(field IN ITEMS ribbon railRibbon beam sprite)
	forbid_text("${adopt_sweep}" "vk.${field}.ral_pipeline_layout"
		"${field} direct owner in adoption sweep")
	forbid_text("${kill_sweep}" "vk.${field}.ral_pipeline_layout"
		"${field} direct owner in adopted teardown sweep")
endforeach()
slice_between(full_shutdown "${boot}"
	"void vk_ral_textures_shutdown( qboolean destroyWindow )"
	"// ── per-image registration")
string(FIND "${full_shutdown}" "vk_shutdown_ribbon();" effect_shutdown)
string(FIND "${full_shutdown}" "vk_ral_destroy_adopted_bindgroups();" adopted_shutdown)
if(effect_shutdown EQUAL -1 OR adopted_shutdown EQUAL -1
		OR NOT effect_shutdown LESS adopted_shutdown)
	message(FATAL_ERROR "full teardown no longer retires direct effect families first")
endif()
foreach(needle IN ITEMS
		"vk_shutdown_ribbon();"
		"vk_shutdown_railribbon();"
		"vk_shutdown_beam();"
		"vk_shutdown_sprite();")
	require_text("${full_shutdown}" "${needle}" "full teardown family coverage")
endforeach()

string(FIND "${product}" "vk.ral_bgl_effects_ubo = Ral_AdoptBindGroupLayout(" early_shared)
string(FIND "${product}" "vk_init_ribbon();" first_effect_init)
if(early_shared EQUAL -1 OR first_effect_init EQUAL -1
		OR NOT early_shared LESS first_effect_init)
	message(FATAL_ERROR "shared effects UBO layout is not published before effect layout creation")
endif()

foreach(needle IN ITEMS
		"CaptureCreatePipelineLayout"
		"capturedLayoutCount == 2u"
		"capturedLayouts[0] == portableLayouts[0].layout"
		"capturedPushRange.stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT"
		"portablePipeline->bindGroupLayoutsRegistered"
		"portablePipeline->numSetLayouts == 2u"
		"portableLayoutVector[1] = &foreignPortableLayout;"
		"pipelineLayoutInfo.numBindGroupLayouts = RAL_MAX_PIPELINE_BIND_GROUP_LAYOUTS + 1u;"
		"pipelineLayoutInfo.pushConstantSize = 30u;"
		"createPipelineLayoutResult = VK_ERROR_OUT_OF_HOST_MEMORY"
		"capturedDestroyedPipelineLayout == (VkPipelineLayout)(uintptr_t)0x74u")
	require_text("${host}" "${needle}" "host mutation coverage")
endforeach()

message(STATUS "effect pipeline layouts are directly RAL-owned with exact portable ABI")
