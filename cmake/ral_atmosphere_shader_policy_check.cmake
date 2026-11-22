# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(SHADER_ROOT "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders")
# Backend allocations and the planner must share one production mip profile.
foreach(ENTRY
		"vulkan/renderer/vk.h|VK_PROBE_RADIANCE_MIPS   WIRED_SKY_SPECULAR_MIP_COUNT"
		"metal/ral_metal_present.mm|RAL_METAL_SKY_RADIANCE_MIPS WIRED_SKY_SPECULAR_MIP_COUNT"
		"opengl/ral_opengl_product.c|RAL_OPENGL_SKY_RADIANCE_MIPS WIRED_SKY_SPECULAR_MIP_COUNT"
		"webgpu/ral_webgpu_sky.c|WEBGPU_SKY_RADIANCE_MIPS WIRED_SKY_SPECULAR_MIP_COUNT"
		"webgpu/ral_webgpu_product.c|textureDesc.mipLevelCount = WIRED_SKY_SPECULAR_MIP_COUNT;")
	string(REPLACE "|" ";" FIELDS "${ENTRY}")
	list(GET FIELDS 0 FILE)
	list(GET FIELDS 1 REQUIRED)
	file(READ "${ROOT}/code/render/ral/backends/${FILE}" TEXT)
	string(FIND "${TEXT}" "${REQUIRED}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "sky production mip profile drift: ${FILE}")
	endif()
endforeach()
foreach(ENTRY
		"vulkan/renderer/shaders/gen_frag.tmpl|textureQueryLevels( radianceCube )"
		"vulkan/renderer/shaders/gen_frag.tmpl|textureQueryLevels( previousRadianceCube )"
		"metal/shaders/ui_solid.msl|currentSkyRadiance.get_num_mip_levels()"
		"metal/shaders/ui_solid.msl|previousSkyRadiance.get_num_mip_levels()"
		"vulkan/renderer/shaders/ral_opengl_product.frag|textureQueryLevels(sampler2DArray(")
	string(REPLACE "|" ";" FIELDS "${ENTRY}")
	list(GET FIELDS 0 FILE)
	list(GET FIELDS 1 REQUIRED)
	file(READ "${ROOT}/code/render/ral/backends/${FILE}" TEXT)
	string(FIND "${TEXT}" "${REQUIRED}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "sky sampling must derive LOD from the actual view: ${FILE}")
	endif()
endforeach()
file(READ "${SHADER_ROOT}/shaders.manifest.mjs" MANIFEST)
set(CHAIN_TEXT "")
set(ATMOSPHERE_SHADERS
	atmosphere_froxel_inject
	atmosphere_froxel_light
	atmosphere_froxel_cloud
	atmosphere_froxel_integrate
	atmosphere_composite
	sky_transmittance
	sky_multiscatter
	sky_view
	sky_surface_composite)

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
		"sourceRadianceExtinction" "valueNoise" "scene * atmosphere.a + atmosphere.rgb"
		"uniform sampler2D skyViewLut" "wiredComposeAuthoredPhysicalSky"
		"authored * scale" "physical * 0.08")
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
file(READ "${SHADER_ROOT}/source_sky.comp" SKY_SOURCE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" NATIVE_EXECUTOR)
foreach(REQUIREMENT
		"authoredRadiance"
		"sunDirectionIntensity"
		"sunRadianceExposure"
		"radiance *= clamp( p.sunRadianceExposure.w, 0.0, 8.0 )"
		"float up = dir.z"
		"smoothstep( 0.0, 0.35, -up )"
		"vec3 authoredTint = authored / authoredEnergy")
	string(FIND "${SKY_SOURCE}" "${REQUIREMENT}" REQUIREMENT_POS)
	if(REQUIREMENT_POS EQUAL -1)
		message(FATAL_ERROR "map-authored sky source misses: ${REQUIREMENT}")
	endif()
endforeach()
foreach(FORBIDDEN "SKY_ZENITH" "SKY_HORIZON" "SUN_DIR" "SUN_COLOR"
		"smoothstep( 0.0, -0.35, up )")
	string(FIND "${SKY_SOURCE}" "${FORBIDDEN}" FORBIDDEN_POS)
	if(NOT FORBIDDEN_POS EQUAL -1)
		message(FATAL_ERROR "map-authored sky source retained fixed analytic authority: ${FORBIDDEN}")
	endif()
endforeach()
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
		"Ral_SkySurfacePlan"
		"vk_atmosphere_full.skyCompositePipeline"
		"sky_surface_composite_comp_spv"
		"vk_sky_physical.readableMask & RAL_SKY_DIRTY_SKY_VIEW"
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
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_shade.c" MATERIAL_HOST)
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
		"orm.g * materialFactors.y"
		"- 0.35 * worldLightParams.y")
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
if(NOT ARTIFACT_COUNT EQUAL 322)
	message(FATAL_ERROR "canonical shader corpus must contain exactly 322 modules, got ${ARTIFACT_COUNT}")
endif()

message(STATUS "RAL atmosphere shader policy: PASS")
