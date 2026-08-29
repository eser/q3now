# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(SHADER_ROOT "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders")
file(READ "${SHADER_ROOT}/shaders.manifest.mjs" MANIFEST)
set(CHAIN_TEXT "")
set(ATMOSPHERE_SHADERS
	atmosphere_froxel_inject
	atmosphere_froxel_light
	atmosphere_froxel_cloud
	atmosphere_froxel_integrate
	atmosphere_composite)

foreach(NAME IN LISTS ATMOSPHERE_SHADERS)
	set(SOURCE "${SHADER_ROOT}/${NAME}.comp")
	if(NOT EXISTS "${SOURCE}")
		message(FATAL_ERROR "missing canonical atmosphere shader: ${SOURCE}")
	endif()
	file(READ "${SOURCE}" SOURCE_TEXT)
	string(APPEND CHAIN_TEXT "${SOURCE_TEXT}")
	string(FIND "${MANIFEST}" "source: '${NAME}.comp', output: '${NAME}_comp_spv'" MANIFEST_POS)
	if(MANIFEST_POS EQUAL -1)
		message(FATAL_ERROR "atmosphere shader is absent from canonical manifest: ${NAME}")
	endif()
	foreach(NATIVE "vulkan.h" "VkDescriptor" "VkPipeline" "metal::" "texture_3d" "sampler3D")
		string(FIND "${SOURCE_TEXT}" "${NATIVE}" NATIVE_POS)
		if(NOT NATIVE_POS EQUAL -1)
			message(FATAL_ERROR "atmosphere shader leaked backend-only surface ${NATIVE}: ${NAME}")
		endif()
	endforeach()
	foreach(EXT msl glsl460 wgsl)
		set(PORTABLE "${SHADER_ROOT}/portable/${NAME}_comp_spv.${EXT}")
		if(NOT EXISTS "${PORTABLE}")
			message(FATAL_ERROR "missing portable atmosphere artifact: ${PORTABLE}")
		endif()
	endforeach()
endforeach()

foreach(REQUIREMENT "64u" "32u" "gridHistory.w"
		"sourceRadianceExtinction" "valueNoise" "scene * atmosphere.a + atmosphere.rgb")
	string(FIND "${CHAIN_TEXT}" "${REQUIREMENT}" REQUIREMENT_POS)
	if(REQUIREMENT_POS EQUAL -1)
		message(FATAL_ERROR "missing bounded atmosphere shader requirement: ${REQUIREMENT}")
	endif()
endforeach()

# The shipping weather pool is part of the same atmosphere product. Its pool
# occupancy must scale with authored precipitation/exposure before simulation,
# below-roof spawns must be rejected immediately, and the draw must retain the
# shared scene-depth soft intersection instead of painting through geometry.
file(READ "${SHADER_ROOT}/atmospheric_integrate.comp" WEATHER_COMPUTE)
file(READ "${SHADER_ROOT}/atmospheric.frag" WEATHER_FRAGMENT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" NATIVE_EXECUTOR)
foreach(REQUIREMENT
		"float precipitationCoverage( uint idx )"
		"intensity * clamp( indoorExposure, 0.0, 1.0 )"
		"precipitationCoverage( idx ) <= 0.0"
		"p.pos.z <= groundHeightAt( p.pos.xy )"
		"family == 3u"
		"family == 4u"
		"family == 5u")
	string(FIND "${WEATHER_COMPUTE}" "${REQUIREMENT}" REQUIREMENT_POS)
	if(REQUIREMENT_POS EQUAL -1)
		message(FATAL_ERROR "missing weather coverage/roof requirement: ${REQUIREMENT}")
	endif()
endforeach()
foreach(REQUIREMENT
		"request.capabilities.fullClouds = qtrue"
		"vk_atmosphere_full.cloudPipeline"
		"vk_atmosphere_full.integrateCloudBg"
		"4u + ( plan.cloudsActive ? 1u : 0u )")
	string(FIND "${NATIVE_EXECUTOR}" "${REQUIREMENT}" REQUIREMENT_POS)
	if(REQUIREMENT_POS EQUAL -1)
		message(FATAL_ERROR "missing native cloud executor requirement: ${REQUIREMENT}")
	endif()
endforeach()

# The ordinary material path consumes global surface climate from reserved
# worldLightParams channels. It must remain arithmetic-only and slope-gate snow
# with the existing IBL normal; local tile lookup is a separate bounded leaf.
file(READ "${SHADER_ROOT}/gen_frag.tmpl" MATERIAL_FRAGMENT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_shade.c" MATERIAL_HOST)
foreach(REQUIREMENT
		".yzw = wetness/frost/snow"
		"surfaceTargets[0] + 0.5f * vk.atm.surfaceTargets[3]"
		"vk.atm.surfaceTargets[1]"
		"vk.atm.surfaceTargets[2]")
	string(FIND "${MATERIAL_HOST}" "${REQUIREMENT}" REQUIREMENT_POS)
	if(REQUIREMENT_POS EQUAL -1)
		message(FATAL_ERROR "missing surface-climate UBO publication: ${REQUIREMENT}")
	endif()
endforeach()
foreach(REQUIREMENT
		"if ( lightmap_slot != 0 )"
		"float wetness = clamp( worldLightParams.y"
		"float frost = clamp( worldLightParams.z"
		"smoothstep( 0.35, 0.85, normalize( ibl_N ).z )"
		"float snowCoverage = clamp( worldLightParams.w"
		"orm.g - 0.35 * worldLightParams.y")
	string(FIND "${MATERIAL_FRAGMENT}" "${REQUIREMENT}" REQUIREMENT_POS)
	if(REQUIREMENT_POS EQUAL -1)
		message(FATAL_ERROR "missing surface-climate material response: ${REQUIREMENT}")
	endif()
endforeach()
foreach(REQUIREMENT
		"layout(set = 0, binding = 2) uniform sampler2D sceneDepthTex"
		"float softParticleFade()"
		"fragColor.a * edge * softParticleFade()")
	string(FIND "${WEATHER_FRAGMENT}" "${REQUIREMENT}" REQUIREMENT_POS)
	if(REQUIREMENT_POS EQUAL -1)
		message(FATAL_ERROR "missing weather depth-mask requirement: ${REQUIREMENT}")
	endif()
endforeach()

file(READ "${SHADER_ROOT}/spirv/ral_shader_artifact_catalog.inc" ARTIFACT_CATALOG)
string(REGEX MATCHALL "RAL_SHADER_SPIRV_ARTIFACT\\(" ARTIFACT_ROWS "${ARTIFACT_CATALOG}")
list(LENGTH ARTIFACT_ROWS ARTIFACT_COUNT)
if(NOT ARTIFACT_COUNT EQUAL 299)
	message(FATAL_ERROR "canonical shader corpus must contain exactly 299 modules, got ${ARTIFACT_COUNT}")
endif()

message(STATUS "RAL atmosphere shader policy: PASS")
