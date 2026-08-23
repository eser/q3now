# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026-present Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	message(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
endif()

function(require_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "effect bind-group layout lost ${why}: ${needle}")
	endif()
endfunction()

function(forbid_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(NOT hit EQUAL -1)
		message(FATAL_ERROR "effect bind-group layout leaked ${why}: ${needle}")
	endif()
endfunction()

function(require_count body pattern expected why)
	string(REGEX MATCHALL "${pattern}" hits "${body}")
	list(LENGTH hits count)
	if(NOT count EQUAL expected)
		message(FATAL_ERROR "effect bind-group layout ${why}: got ${count}, want ${expected}")
	endif()
endfunction()

function(slice_between out body begin_marker end_marker)
	string(FIND "${body}" "${begin_marker}" begin)
	string(FIND "${body}" "${end_marker}" end)
	if(begin EQUAL -1 OR end EQUAL -1 OR NOT begin LESS end)
		message(FATAL_ERROR "cannot isolate effect span: ${begin_marker} -> ${end_marker}")
	endif()
	math(EXPR length "${end} - ${begin}")
	string(SUBSTRING "${body}" ${begin} ${length} slice)
	set(${out} "${slice}" PARENT_SCOPE)
endfunction()

set(product_path "${SOURCE_ROOT}/code/renderervk/vk.c")
set(resource_path "${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c")
set(host_path "${SOURCE_ROOT}/tests/ral_vulkan_bind_group_arena_test.c")
foreach(path IN ITEMS "${product_path}" "${resource_path}" "${host_path}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "effect bind-group layout input missing: ${path}")
	endif()
endforeach()
file(READ "${product_path}" product)
file(READ "${resource_path}" resource)
file(READ "${host_path}" host)

slice_between(helper "${product}"
	"static qboolean vk_create_effect_bind_group_layout("
	"void vk_init_ribbon( void )")
foreach(needle IN ITEMS
		"candidate = Ral_CreateBindGroupLayout( vk_ral_get_backend(), &createInfo );"
		"native = (VkDescriptorSetLayout)Ral_GetBindGroupLayoutHandle( candidate );"
		"Ral_DestroyBindGroupLayout( candidate );"
		"*outOwner = candidate;"
		"*outNative = native;")
	require_text("${helper}" "${needle}" "candidate-first publication")
endforeach()
forbid_text("${helper}" "TERM_UNRECOVERABLE" "fatal compatibility path")
require_count("${product}" "vk_create_effect_bind_group_layout[(]" 12
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
	require_count("${init}" "vk_create_effect_bind_group_layout[(]" 1
		"${family} direct RAL creation")
	require_text("${init}" "SEV_WARN" "${family} nonfatal disable")
	forbid_text("${init}" "qvkCreateDescriptorSetLayout" "${family} raw create")
	forbid_text("${init}" "Ral_AdoptBindGroupLayout" "${family} create-then-adopt")

	slice_between(shutdown "${product}" "void vk_shutdown_${family}( void )" "${next}")
	string(FIND "${shutdown}" "Ral_DestroyPipelineLayout" destroy_pipeline)
	string(FIND "${shutdown}" "Ral_DestroyBindGroupLayout" destroy_layout)
	string(FIND "${shutdown}" ".set_layout = VK_NULL_HANDLE;" clear_mirror)
	if(destroy_pipeline EQUAL -1 OR destroy_layout EQUAL -1 OR clear_mirror EQUAL -1
			OR NOT destroy_pipeline LESS destroy_layout OR NOT destroy_layout LESS clear_mirror)
		message(FATAL_ERROR "${family} child-to-parent shutdown order drifted")
	endif()
	forbid_text("${shutdown}" "qvkDestroyPipelineLayout" "${family} raw pipeline-layout destroy")
	forbid_text("${shutdown}" "qvkDestroyDescriptorSetLayout" "${family} raw destroy")
endforeach()

# Exact entry shapes retain the portable split texture/sampler contract.
foreach(needle IN ITEMS
		"\"wired-ribbon-set-layout\", &vk.ribbon.ral_bgl"
		"\"wired-rail-ribbon-set-layout\", &vk.railRibbon.ral_bgl"
		"\"wired-beam-set-layout\", &vk.beam.ral_bgl"
		"\"wired-sprite-set-layout\", &vk.sprite.ral_bgl"
		"RAL_BIND_TEXTURE_VIEW_2D, qfalse"
		"RAL_BIND_SAMPLER, 1u")
	require_text("${product}" "${needle}" "portable effect layout metadata")
endforeach()

foreach(needle IN ITEMS
		"Ral_DestroyBindGroupLayout( vk.ribbon.ral_bgl );"
		"Ral_DestroyBindGroupLayout( vk.railRibbon.ral_bgl );"
		"Ral_DestroyBindGroupLayout( vk.beam.ral_bgl );"
		"Ral_DestroyBindGroupLayout( vk.sprite.ral_bgl );"
		"&& vk.ribbon.ral_bgl == NULL"
		"&& vk.railRibbon.ral_bgl == NULL"
		"&& vk.beam.ral_bgl == NULL"
		"&& vk.sprite.ral_bgl == NULL")
	require_text("${product}" "${needle}" "owned shutdown/adoption guard")
endforeach()

foreach(needle IN ITEMS
		"L->ownsLayout = qtrue;"
		"b->vk.CreateDescriptorSetLayout( b->device, &lci, NULL, &L->layout )"
		"if ( layout->ownsLayout )"
		"RAL_RES_DESC_SET_LAYOUT")
	require_text("${resource}" "${needle}" "RAL layout ownership")
endforeach()

foreach(needle IN ITEMS
		"CaptureCreateDescriptorSetLayout"
		"capturedLayoutInfo.bindingCount == 4u"
		"VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE"
		"VK_DESCRIPTOR_TYPE_SAMPLER"
		"effectLayout->ownsLayout == qtrue"
		"destroyLayoutCalls == 1u"
		"effectEntries[1].binding = 0u"
		"effectEntries[2].dynamicOffset = qtrue"
		"createLayoutResult = VK_ERROR_OUT_OF_HOST_MEMORY")
	require_text("${host}" "${needle}" "host mutation coverage")
endforeach()

message(STATUS "ribbon/rail-ribbon/beam/sprite layouts are directly RAL-owned")
