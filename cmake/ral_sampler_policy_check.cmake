# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/renderer/ral/ral_resource.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_sampler.c" CORE)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c" VULKAN)
file(READ "${ROOT}/code/renderervk/vk.h" PRODUCT_HEADER)
file(READ "${ROOT}/code/renderervk/vk.c" PRODUCT)
file(READ "${ROOT}/code/renderervk/vk_ral_textures.h" BINDLESS_HEADER)
file(READ "${ROOT}/code/renderervk/vk_ral_textures.c" BINDLESS)
file(READ "${ROOT}/tests/ral_sampler_policy_test.c" HOST)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_SOURCE)

foreach(REQUIRED IN ITEMS
	"RAL_BORDER_TRANSPARENT_BLACK"
	"RAL_BORDER_OPAQUE_BLACK"
	"RAL_BORDER_OPAQUE_WHITE"
	"ralBorderColor_t borderColor"
	"Ral_SamplerCreateInfoValid")
	string(FIND "${HEADER}${CORE}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "portable sampler contract lost seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"case RAL_BORDER_OPAQUE_BLACK: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;"
	"case RAL_BORDER_OPAQUE_WHITE: return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;"
	"default:                      return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;"
	"if ( !b || !Ral_SamplerCreateInfoValid( ci ) ) return NULL;"
	"sci.maxLod           = ( ci->maxLod > 0.0f ) ? ci->maxLod : VK_LOD_CLAMP_NONE;"
	"sci.borderColor      = ralVk_BorderColor( ci->borderColor );")
	string(FIND "${VULKAN}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan sampler lowering lost seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"struct ralSampler_s *ral_handle[MAX_VK_SAMPLERS]"
	"struct ralSampler_s\t*ral_sampler_repeat"
	"struct ralSampler_s *ral_point_sampler"
	"struct ralSampler_s *ral_linear_sampler")
	string(FIND "${PRODUCT_HEADER}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "renderer sampler ownership inventory lost: ${REQUIRED}")
	endif()
endforeach()

string(REGEX MATCHALL "vk_create_ral_sampler[(]" CREATE_CALLS "${PRODUCT}")
list(LENGTH CREATE_CALLS CREATE_COUNT)
if(NOT CREATE_COUNT EQUAL 8)
	message(FATAL_ERROR "renderer sampler create inventory drifted: expected definition + 7 raw-mirror owner sites, got ${CREATE_COUNT}")
endif()
string(REGEX MATCHALL "Ral_DestroySampler[(]" DESTROY_CALLS "${PRODUCT}")
list(LENGTH DESTROY_CALLS DESTROY_COUNT)
if(NOT DESTROY_COUNT EQUAL 17)
	message(FATAL_ERROR "renderer RAL sampler destroy inventory drifted: expected 17 owned/rollback sites, got ${DESTROY_COUNT}")
endif()

foreach(REQUIRED IN ITEMS
	"candidate = Ral_CreateSampler( vk_ral_get_backend(), info );"
	"Ral_GetSamplerHandle( candidate )"
	"&vk.samplers.ral_handle[samplerIndex]"
	"vk.samplers.ral_handle[i] = NULL"
	"vk.shadowMap.ral_sampler"
	"vk.dlightShadow.ral_sampler"
	"vk.sceneDepth.ral_sampler"
	"vk.blueNoise.ral_sampler"
	"candidateSampler = Ral_CreateSampler( backend, &samplerInfo );")
	string(FIND "${PRODUCT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "renderer sampler owner lifecycle lost seam: ${REQUIRED}")
	endif()
endforeach()

foreach(FORBIDDEN IN ITEMS
	"qvkCreateSampler" "qvkDestroySampler" "VkSamplerCreateInfo")
	string(FIND "${PRODUCT}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "renderer retained raw sampler lifecycle token: ${FORBIDDEN}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"struct ralSampler_s *sampler"
	"identity = Ral_GetSamplerHandle( sampler );"
	"if ( !Ral_BindGroupSetSamplerAt( s_ral_bindless_set, slot, sampler ) )"
	"vk.samplers.ral_handle[smIdx]")
	string(FIND "${BINDLESS_HEADER}${BINDLESS}${PRODUCT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "bindless direct RAL sampler publication lost seam: ${REQUIRED}")
	endif()
endforeach()
string(FIND "${BINDLESS}" "Ral_AdoptSampler" ADOPT_POSITION)
if(NOT ADOPT_POSITION EQUAL -1)
	message(FATAL_ERROR "bindless sampler publication recreated transient adoption")
endif()

foreach(REQUIRED IN ITEMS
	"ci.maxAnisotropy = -1.0f"
	"ci.compareEnable = (qboolean)2"
	"ci.maxLod = 0.5f"
	"ci.borderColor = (ralBorderColor_t)3"
	"ci.borderColor = RAL_BORDER_OPAQUE_BLACK")
	string(FIND "${HOST}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "sampler host mutation coverage lost: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"code/renderer/ral/ral_sampler.c"
	"ral_sampler_policy_test"
	"ral_sampler_source_policy_contract")
	string(FIND "${CMAKE_SOURCE}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "sampler build/policy registration lost: ${REQUIRED}")
	endif()
endforeach()

message(STATUS "RAL sampler ownership policy: portable validation, Vulkan lowering, renderer owners and bindless publication are exact")
