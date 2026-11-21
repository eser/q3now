# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/core/ral_atmosphere.h" HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_atmosphere.c" SOURCE)
file(READ "${ROOT}/code/render/ral/core/ral_sky_radiance.h" SKY_RADIANCE_HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_sky_radiance.c" SKY_RADIANCE_SOURCE)
file(READ "${ROOT}/code/render/frontend/render_submission_effects.c" FRONTEND)
file(READ "${ROOT}/code/qcommon/wired/render/primitives.h" PRIMITIVES)
file(READ "${ROOT}/code/render/ral/core/ral_atmosphere_frame_graph.h" GRAPH_HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_atmosphere_frame_graph.c" GRAPH_SOURCE)
file(READ "${ROOT}/tests/ral_atmosphere_test.c" TEST)
file(READ "${ROOT}/code/render/ral/backends/opengl/ral_opengl_product.c" OPENGL)
file(READ "${ROOT}/code/render/ral/backends/opengl/ral_opengl_world.c" OPENGL_WORLD)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/ral_opengl_product.frag" OPENGL_WORLD_SHADER)
file(READ "${ROOT}/code/render/ral/backends/opengl/ral_opengl_module.c" OPENGL_MODULE)
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm" METAL)
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_present.mm" METAL_PRESENT)
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_atmosphere.mm" METAL_ATMOSPHERE)
file(READ "${ROOT}/code/render/frontend/render_atmosphere_gpu.h" ATMOSPHERE_GPU)
file(READ "${ROOT}/code/render/ral/backends/metal/shaders/ui_solid.msl" METAL_SHADERS)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c" WEBGPU)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_product.c" WEBGPU_PRODUCT)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_sky.c" WEBGPU_SKY)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_atmosphere.c" WEBGPU_ATMOSPHERE)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_weather.c" WEBGPU_WEATHER)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_command.c" WEBGPU_COMMAND)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_browser_bridge.c" WEBGPU_BRIDGE)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_browser_host.mjs" WEBGPU_HOST)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_browser_dispatch.mjs" WEBGPU_DISPATCH)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" VULKAN)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_init.c" VULKAN_FRONTEND)
file(READ "${ROOT}/code/render/renderer/tr_main.c" VULKAN_SCENE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_cmds.c" VULKAN_COMMANDS)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_backend.c" VULKAN_BACKEND)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.c" VULKAN_RESOURCES)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_shade.c" VULKAN_MATERIAL_HOST)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/gen_frag.tmpl" VULKAN_MATERIAL_SHADER)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/sky_surface_composite.comp" VULKAN_SKY_COMPOSITE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/lens_occlusion.comp" VULKAN_LENS_SHADER)
file(READ "${ROOT}/code/cgame/cg_atmospheric.c" CG_ATMOSPHERE)
file(READ "${ROOT}/code/cgame/cg_servercmds.c" CG_SERVER_COMMANDS)
file(READ "${ROOT}/code/cgame/cg_lensflare.c" CG_LENS_FLARE)
file(READ "${ROOT}/code/cgame/wired/cg_wired_particles.c" CG_PARTICLES)
file(READ "${ROOT}/code/game/g_cmds.c" GAME_COMMANDS)
file(READ "${ROOT}/code/game/g_main.c" GAME_MAIN)
file(READ "${ROOT}/code/game/entities/g_spawn.c" GAME_SPAWN)
file(READ "${ROOT}/code/client/cl_cgame.c" CLIENT_CGAME)
file(READ "${ROOT}/code/client/cl_main.c" CLIENT_MAIN)
file(READ "${ROOT}/code/render/frontend/tr_public.h" RENDER_PUBLIC)
foreach(NEEDLE IN ITEMS "RAL_ATMOSPHERE_MAX_VOLUMES"
		"RAL_ATMOSPHERE_MAX_LIGHTS"
		"RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS"
		"RAL_ATMOSPHERE_PASS_SINGLE_COMPOSITE"
		"RAL_ATMOSPHERE_FALLBACK_BACKEND_UNAVAILABLE"
		"RAL_ATMOSPHERE_FALLBACK_NO_STORAGE_BUFFER"
		"RAL_ATMOSPHERE_WEATHER_ALL"
		"Ral_AtmospherePlanWeather"
		"Ral_AtmosphereSkyEventMask"
		"RAL_SKY_TIER_BUDGET_SCHEMA_VERSION"
		"RAL_SKY_TIER_MAX_RADIANCE_SLICES_PER_FRAME"
		"Ral_SkyTierBudget"
		"RAL_SKY_ENVIRONMENT_RUNTIME_SCHEMA_VERSION"
		"Ral_SkyEnvironmentRuntimeAdvance"
		"Ral_SkyEnvironmentRuntimeSchedule"
		"sunRadiance[3]" "ambientSh[27]" "exposure"
		"storageBuffers")
	string(FIND "${HEADER}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "atmosphere RAL header misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
		"WIRED_ENGINE_RES_BIND_PREV_IRRADIANCE"
		"WIRED_ENGINE_RES_BIND_PREV_RADIANCE"
		"skyRadianceReceipt.previousProductValid"
		"probeIrradianceTargetReadable"
		"probeRadianceTargetReadable")
	string(FIND "${VULKAN_RESOURCES}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan previous sky-product binding misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
		"tess.shader->materialFactorsReady"
		"case TYPE_MULTI_TEXTURE_MUL2_IDENTITY: idef.shader_type = TYPE_MULTI_TEXTURE_MUL2_IDENTITY_IBL; break;"
		"case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR: idef.shader_type = TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_IBL; break;"
		"skyRadianceConsumer.previousToCurrentBlend"
		"idef.ibl_enabled = pStage->bundle[3].image[0] != NULL ? 1 : 2")
	string(FIND "${VULKAN_MATERIAL_HOST}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan cooked material-factor publication misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
		"previousRadianceCube"
		"materialBaseColor"
		"materialFactors"
		"mix( previousPrefiltered, currentPrefiltered,"
		"base.rgb += specularIBL;")
	string(FIND "${VULKAN_MATERIAL_SHADER}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan surface sky-radiance consumption misses: ${NEEDLE}")
	endif()
endforeach()
# Baked lightmaps/local SH own diffuse illumination. Sky is an additional
# specular input, not a second diffuse irradiance term (exact-once lighting).
foreach(FORBIDDEN IN ITEMS "uniform samplerCube irradianceCube"
		"uniform samplerCube previousIrradianceCube" "diffuseIBL + specularIBL")
	string(FIND "${VULKAN_MATERIAL_SHADER}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan sky reintroduced duplicate diffuse: ${FORBIDDEN}")
	endif()
endforeach()
foreach(NATIVE_SKY_SOURCE IN ITEMS "${OPENGL}" "${METAL_PRESENT}"
		"${WEBGPU_PRODUCT}")
	foreach(NEEDLE IN ITEMS "EnsureSkyRadianceResources"
			"skyResourceWorldLifecycleGeneration"
			"skyResourcesReady")
		string(FIND "${NATIVE_SKY_SOURCE}" "${NEEDLE}" POSITION)
		if(POSITION EQUAL -1)
			message(FATAL_ERROR "native sky resource owner misses: ${NEEDLE}")
		endif()
	endforeach()
endforeach()
foreach(SKY_CONSUMER_SOURCE IN ITEMS "${OPENGL}" "${METAL_PRESENT}"
		"${WEBGPU_SKY}" "${VULKAN}")
	string(FIND "${SKY_CONSUMER_SOURCE}" "Ral_SkyRadianceConsumerPlan"
		SKY_CONSUMER_PLAN)
	if(SKY_CONSUMER_PLAN EQUAL -1)
		message(FATAL_ERROR
			"native sky backend bypasses the shared radiance publication plan")
	endif()
endforeach()
# Residency is not frame admission. All adapters must apply the shared budget;
# Vulkan must also withdraw at descriptor acceptance, before world draws (its
# sky compute/publication pass runs only at the later scene-to-UI boundary).
foreach(ADAPTER IN ITEMS VULKAN METAL_PRESENT OPENGL WEBGPU)
	string(FIND "${${ADAPTER}}" "Ral_SkyRadianceConsumerAdmit" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "${ADAPTER} bypasses sky frame admission")
	endif()
endforeach()
string(FIND "${VULKAN}" "void RE_SetAtmosphere(" VK_ATM_START)
string(FIND "${VULKAN}" "vk.atm.state = normalized;" VK_ATM_PUBLISH)
math(EXPR VK_ATM_LENGTH "${VK_ATM_PUBLISH} - ${VK_ATM_START}")
string(SUBSTRING "${VULKAN}" ${VK_ATM_START} ${VK_ATM_LENGTH} VK_ATM_ACCEPT)
string(FIND "${VK_ATM_ACCEPT}" "Ral_SkyRadianceConsumerAdmit" VK_ATM_ADMIT)
if(VK_ATM_ADMIT EQUAL -1)
	message(FATAL_ERROR "Vulkan sky consumer withdrawal happens after world drawing")
endif()
string(FIND "${VULKAN}" "qboolean vk_atmosphere_full_execute( void )" VK_EXEC_START)
string(FIND "${VULKAN}" "static qboolean vk_atmosphere_full_prepare(" VK_PREPARE_START)
string(FIND "${VULKAN}" "static qboolean vk_atmosphere_full_composite(" VK_COMPOSITE_START)
if(VK_EXEC_START EQUAL -1 OR VK_PREPARE_START EQUAL -1 OR VK_COMPOSITE_START EQUAL -1)
	message(FATAL_ERROR "Vulkan atmosphere execution entry point missing")
endif()
math(EXPR VK_PREPARE_LENGTH "${VK_COMPOSITE_START} - ${VK_PREPARE_START}")
string(SUBSTRING "${VULKAN}" ${VK_PREPARE_START} ${VK_PREPARE_LENGTH} VK_EXEC_SOURCE)
# A producer may prepare composite parameters, but must not overwrite scene
# colour or publish temporal ping-pong before its consumer succeeded.
foreach(FORBIDDEN IN ITEMS "vk_atmosphere_full_copy_to_scene("
		"vk_atmosphere_full_transition_begin("
		"vk_atmosphere_full.historyRead ="
		"vk_atmosphere_full.historyValid = qtrue")
	string(FIND "${VK_EXEC_SOURCE}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan atmosphere producer also composites/publishes: ${FORBIDDEN}")
	endif()
endforeach()
# Grid distances must not drift between a backend's producer and consumer or
# inherit surface clip planes. This supplements the numeric/GPU range tests.
foreach(ADAPTER IN ITEMS OPENGL_WORLD METAL_ATMOSPHERE ATMOSPHERE_GPU)
	string(FIND "${${ADAPTER}}" "wired_atmosphere_grid_far(" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "${ADAPTER} bypasses the RAL atmosphere grid range")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RenderAtmosphereGpu_Build( frontend, plan"
		"ralOpenGlProductAtmosphereTileSource" "sizeof( frame.lights )"
		"*nativeDispatchCount != plan->computeDispatchCount+1u")
	string(FIND "${OPENGL}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "OpenGL bypasses canonical atmosphere packet/tile: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "nearZ = WIRED_ATMOSPHERE_GRID_NEAR;"
		"farZ = wired_atmosphere_grid_far( vk.atm.state.visibility );")
	string(FIND "${VK_EXEC_SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan atmosphere grid inherits scene clipping: ${NEEDLE}")
	endif()
endforeach()
string(FIND "${VK_EXEC_SOURCE}" "request.requestedTier = ( vk.atm.state.flags & ATMOSPHERE_FLAG_ENABLED )" VK_TIER)
if(VK_TIER EQUAL -1)
	message(FATAL_ERROR "Vulkan sky plan ignores the accepted atmosphere tier")
endif()
# The radiance cohort is a lighting/IBL product and does not encode ordered Q3
# sky stages.  Every adapter must preserve those rasterized pixels and apply the
# shared physical LUT as a bounded composition rather than replacing the sky
# with the reduced radiance cube.
foreach(NEEDLE IN ITEMS "wiredComposeAuthoredPhysicalSky"
		"Radiance cubes are the shared IBL/lighting product")
	string(FIND "${VULKAN_SKY_COMPOSITE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan visible sky loses authored stage authority: ${NEEDLE}")
	endif()
endforeach()
string(FIND "${METAL_PRESENT}" "outParams->skyControl[1] = 0.0f"
	METAL_AUTHORED_GATE)
string(FIND "${WEBGPU}" "uniform.radianceControl[3] = 0.0f"
	WEBGPU_AUTHORED_GATE)
string(FIND "${OPENGL_WORLD}" "emissiveRadiance[3] = -1.0f"
	OPENGL_AUTHORED_GATE)
foreach(GATE_POSITION IN ITEMS METAL_AUTHORED_GATE WEBGPU_AUTHORED_GATE
		OPENGL_AUTHORED_GATE)
	if(${GATE_POSITION} EQUAL -1)
		message(FATAL_ERROR
			"visible sky adapter replaces ordered authored stages: ${GATE_POSITION}")
	endif()
endforeach()
# Surface IBL is one shared sky-product contract, not a Vulkan-only feature.
# Every portable backend must publish the cooked material cohort, bind both
# current and previous diffuse/specular products, and use the receipt blend.
foreach(SURFACE_IBL_SOURCE IN ITEMS
		"${OPENGL_WORLD}${OPENGL_WORLD_SHADER}"
		"${METAL_PRESENT}${METAL_SHADERS}"
		"${WEBGPU_PRODUCT}${WEBGPU}")
	foreach(NEEDLE IN ITEMS "materialFactors"
			"currentSkyIrradiance" "previousSkyIrradiance"
			"currentSkyRadiance" "previousSkyRadiance"
			"previousToCurrentBlend")
		string(FIND "${SURFACE_IBL_SOURCE}" "${NEEDLE}" POSITION)
		if(POSITION EQUAL -1)
			message(FATAL_ERROR
				"native surface IBL bypasses the shared material/radiance cohort: ${NEEDLE}")
		endif()
	endforeach()
endforeach()
# Fog/cloud/world-lighting must consume the same versioned sky receipt as the
# visible background and IBL.  Reading the parallel legacy ambient/sun/shadow
# fields here permits one-frame disagreement during an environment revision.
foreach(ENVIRONMENT_CONSUMER_SOURCE IN ITEMS
		"${OPENGL}${OPENGL_WORLD}"
		"${METAL_PRESENT}"
		"${WEBGPU}${WEBGPU_ATMOSPHERE}${WEBGPU_WEATHER}")
	foreach(NEEDLE IN ITEMS "skyEnvironment.ambientSh"
			"skyEnvironment.sunRadiance"
			"skyEnvironment.cloudSunOcclusion")
		string(FIND "${ENVIRONMENT_CONSUMER_SOURCE}" "${NEEDLE}" POSITION)
		if(POSITION EQUAL -1)
			message(FATAL_ERROR
				"native environment consumer bypasses the shared sky receipt: ${NEEDLE}")
		endif()
	endforeach()
endforeach()
# Once a normalized skyEnvironment exists, native weather/froxel consumers may
# not read the legacy import mirror.  It can diverge during a source revision.
foreach(LEGACY_ENVIRONMENT_READ IN ITEMS
		"vk.atm.state.sunDirection" "vk.atm.state.moonDirection"
		"vk.atm.state.ambientColor" "vk.atm.state.cloudShadow"
		"snapshot.state.sunDirection" "snapshot.state.moonDirection"
		"snapshot.state.ambientColor" "snapshot.state.cloudShadow"
		"vk.atm.state.sunIntensity" "vk.atm.state.moonIntensity"
		"snapshot.state.sunIntensity" "snapshot.state.moonIntensity"
		"s.state.sunDirection" "s.state.moonDirection"
		"s.state.ambientColor" "s.state.cloudShadow"
		"s.state.sunIntensity" "s.state.moonIntensity")
	string(FIND "${VULKAN}${OPENGL}${METAL_PRESENT}${WEBGPU_ATMOSPHERE}${WEBGPU_WEATHER}"
		"${LEGACY_ENVIRONMENT_READ}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR
			"native atmosphere consumer reads legacy sky-lighting mirror: ${LEGACY_ENVIRONMENT_READ}")
	endif()
endforeach()
# Vulkan sun rays are an optics consumer too.  Selecting the variant without
# the native oracle, or treating a missing/cold receipt as fully visible,
# silently reintroduces the draw-through-geometry fallback.
foreach(NEEDLE IN ITEMS
		"r_lens->integer && vk.sceneDepth.active"
		"&& tr.sunHasSource && vk.ral_lens_pipeline && vk.lensSourcesPtr"
		"float sunVisibility = 0.0f"
		"vk.atm.state.skyEnvironment.sunDirection"
		"vk.atm.state.skyEnvironment.sunRadiance"
		"r_sunRayIntensity->value * skySunEnergy * edgeFade"
		"native scene-depth + sky-optics receipt")
	string(FIND "${VULKAN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan sun-ray optics path is not fail-closed: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS
		"float sunVisibility = 1.0f"
		"oracle is off/cold → god-rays behave exactly as before"
		"1.0 when the oracle is off → unchanged")
	string(FIND "${VULKAN}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan sun-ray path retains an optics fallback: ${FORBIDDEN}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "skyTextures[RAL_OPENGL_SKY_TEXTURE_COUNT]"
		"skyShBuffers[RAL_OPENGL_SKY_SH_BUFFER_COUNT]")
	string(FIND "${OPENGL}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "OpenGL native sky double-buffer cohort misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NATIVE_SKY_SOURCE IN ITEMS "${METAL_PRESENT}" "${WEBGPU_PRODUCT}")
	foreach(NEEDLE IN ITEMS "skyIrradiance[2]" "skyRadiance[2]" "skySh[2]")
		string(FIND "${NATIVE_SKY_SOURCE}" "${NEEDLE}" POSITION)
		if(POSITION EQUAL -1)
			message(FATAL_ERROR "native sky double-buffer cohort misses: ${NEEDLE}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS
		"skyPhysical[RAL_METAL_SKY_PHYSICAL_TEXTURE_COUNT]"
		"CollectSkyWorkCompletion"
		"EncodeSkyPhysical"
		"skyCapabilities.physicalLuts = present->skyResourcesReady"
		"present->skyWorkCommand ? 0u : 1u")
	string(FIND "${METAL_PRESENT}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Metal physical-sky execution misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "wired_sky_transmittance_compute"
		"wired_sky_multiscatter_compute" "wired_sky_view_compute")
	string(FIND "${METAL_SHADERS}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Metal physical-sky shader misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
		"Ral_SkySurfacePlan"
		"physicalSkyReadable"
		"present->skyPhysical[RAL_METAL_SKY_VIEW]"
		"physicalSkyView [[texture(14)]]"
		"atmosphere.skyControl.x > 0.0")
	string(FIND "${METAL_PRESENT}${METAL_SHADERS}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Metal visible physical-sky consumer misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RalWebGpu_SkyEnsureWorld" "RalWebGpu_SkyBegin"
		"RalWebGpu_SkyPoll" "texture_storage_2d<rgba16float"
		"RAL_WEBGPU_TEXTURE_STORAGE" "completedSkyWorkMask"
		"RalWebGpu_SkySampleResources" "physicalSky" "skySurface"
		"Ral_SkySurfacePlan")
	string(FIND "${WEBGPU_SKY}${WEBGPU}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU physical-sky execution misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NATIVE_SKY_MODULE IN ITEMS "${OPENGL_MODULE}" "${METAL}" "${WEBGPU}")
	string(FIND "${NATIVE_SKY_MODULE}" "UnloadSkyRadianceResources" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "native sky world-unload teardown is not wired")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "Ral_SkyRadianceReceiptValid"
		"RAL_SKY_RADIANCE_CONSUMER_RECEIPT_SCHEMA_VERSION"
		"ralSkyRadianceConsumerReceipt_t"
		"Ral_SkyRadianceConsumerPlan"
		"Ral_SkyRadianceConsumerReceiptExact")
	string(FIND "${SKY_RADIANCE_HEADER}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "sky radiance contract misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION"
		"atmosphereSkyEnvironment_t" "sunDirection" "moonDirection"
		"ambientSh[27]" "distantSceneIdentity" "distantSceneOrigin"
		"distantSceneScale" "distantSceneLodBias")
	string(FIND "${PRIMITIVES}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "versioned sky/environment authority misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RenderSubmission_SkyEnvironmentDigest"
		"RenderSubmission_BuildSkyEnvironmentRuntimeRequest"
		"RenderSubmission_NormalizeAtmosphereState"
		"request.distantSceneIdentity = sky->distantSceneIdentity"
		"request.distantSceneOrigin, sky->distantSceneOrigin"
		"request.distantSceneScale = sky->distantSceneScale"
		"request.distantSceneLodBias = sky->distantSceneLodBias"
		"request.sunDirection, sky->sunDirection"
		"request.sunRadiance, sky->sunRadiance"
		"request.ambientSh, sky->ambientSh"
		"request.exposure = sky->exposure")
	string(FIND "${FRONTEND}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "sky/environment frontend bridge misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
		"plan.skyEnvironment.sunDirection"
		"plan.skyEnvironment.moonDirection"
		"plan.skyEnvironment.ambientSh"
		"plan.skyEnvironment.cloudSunOcclusion"
		"vkSkySourcePush_t"
		"vk_sky_source_push"
		"sizeof( vkSkySourcePush_t )"
		"sky->ambientSh"
		"sky->sunDirection"
		"sky->sunRadiance"
		"sky->exposure"
		"Ral_SkySurfacePlan"
		"vk_atmosphere_full.skyCompositePipeline"
		"sky_surface_composite_comp_spv")
	string(FIND "${VULKAN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan shared sky consumer misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "c_skyPendingWork" "c_skyInFlightWork"
		"c_skyAdmittedWork" "c_skyCompletedWork" "c_skyCancelledWork")
	string(FIND "${VULKAN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan bounded sky telemetry misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "plan.skyEnvironment.opticsVisibility"
		"vk.lensOpticsVisibility"
		"normalized.skyEnvironment.opticsVisibility"
		"shadow[(size_t)i * 8u + 4u] = vk.lensOpticsVisibility")
	string(FIND "${VULKAN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan lens oracle bypasses shared sky optics: ${NEEDLE}")
	endif()
endforeach()
string(FIND "${VULKAN_LENS_SHADER}"
	"clamp( lensSources[idx].colorVis.x, 0.0, 1.0 )" VULKAN_OPTICS_SHADER)
if(VULKAN_OPTICS_SHADER EQUAL -1)
	message(FATAL_ERROR "Vulkan lens shader does not apply shared sky optics visibility")
endif()
string(FIND "${VULKAN}" "Ral_AtmosphereSkyEventMask" SKY_EVENT_MAPPER)
if(SKY_EVENT_MAPPER EQUAL -1)
	message(FATAL_ERROR "Vulkan sky lifecycle must use the shared atmosphere invalidation mapper")
endif()
# Frame-local receipts cannot be reused merely because scene time stopped.
# Real arena7 pinned-time A/B reproduced perpetual cancellation with this gate;
# the runtime unit test covers event -> 16 held frames for every backend.
string(FIND "${VULKAN}"
	"vk.atm.runtimeState.timelineSeconds != vk.atm.timelineSeconds" SKY_STALE_TIME_GATE)
if(NOT SKY_STALE_TIME_GATE EQUAL -1)
	message(FATAL_ERROR "Vulkan must advance its atmosphere receipt on held-time render frames")
endif()
foreach(NEEDLE IN ITEMS
		"cinematicEnabled = qtrue"
		"cinematicSpecified = qtrue"
		"if ( explicitFxSource && !cinematicSpecified )"
		"if ( !e->cinematicEnabled )"
		"trap_WiredFx_EmitEvent( &event )")
	string(FIND "${CG_LENS_FLARE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "cinematic lens flare ownership policy misses: ${NEEDLE}")
	endif()
endforeach()
# There are two distinct opt-in points: qualified map lights default to the
# cinematic WiredFX recipe, and an explicit fx_lensflare classname preserves
# that default unless the entity overrides lensflare.  A single substring
# check let the first policy silently regress while the second still passed.
string(REGEX MATCHALL "cinematicEnabled = qtrue" CINEMATIC_DEFAULTS
	"${CG_LENS_FLARE}")
list(LENGTH CINEMATIC_DEFAULTS CINEMATIC_DEFAULT_COUNT)
if(NOT CINEMATIC_DEFAULT_COUNT EQUAL 2)
	message(FATAL_ERROR
		"cinematic lens flare policy requires both map-light and explicit-source defaults; found ${CINEMATIC_DEFAULT_COUNT}")
endif()
foreach(FORBIDDEN IN ITEMS "CINEMATIC_LENS_FLARE_FX"
		"Compile-time fallback for builds that omit cinematic WiredFX"
		"CG_EmitFlareLayer( &ent, &lf.mapLayers[li], scale, NULL )")
	string(FIND "${CG_LENS_FLARE}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "cinematic map flare must not retain a non-WiredFX fallback: ${FORBIDDEN}")
	endif()
endforeach()
foreach(SKY_PATH IN ITEMS
		"${ROOT}/code/render/ral/backends/opengl/ral_opengl_product.c"
		"${ROOT}/code/render/ral/backends/metal/ral_metal_present.mm"
		"${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c"
		"${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c")
	file(READ "${SKY_PATH}" SKY_SOURCE)
	foreach(NEEDLE IN ITEMS "RenderSubmission_BuildSkyEnvironmentRuntimeRequest"
			"Ral_SkyEnvironmentRuntimeAdvance" "request.skyEnvironment = skyPlan"
			"Ral_SkyEnvironmentRuntimeSchedule")
		string(FIND "${SKY_SOURCE}" "${NEEDLE}" POSITION)
		if(POSITION EQUAL -1)
			message(FATAL_ERROR "production sky runtime ${SKY_PATH} misses: ${NEEDLE}")
		endif()
	endforeach()
endforeach()
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm"
	METAL_MODULE_SKY_SOURCE)
string(FIND "${METAL_MODULE_SKY_SOURCE}"
	"published.atmosphere = published.presentation.atmosphere"
	METAL_PRESENT_AUTHORITY)
if(METAL_PRESENT_AUTHORITY EQUAL -1)
	message(FATAL_ERROR
		"Metal module must publish the presentation-owned atmosphere receipt")
endif()
foreach(FORBIDDEN IN ITEMS "RenderSubmission_BuildSkyEnvironmentRuntimeRequest"
		"Ral_SkyEnvironmentRuntimeAdvance" "Ral_SkyEnvironmentRuntimeSchedule")
	string(FIND "${METAL_MODULE_SKY_SOURCE}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR
			"Metal module duplicates presentation-owned sky runtime: ${FORBIDDEN}")
	endif()
endforeach()
foreach(MAP_LIFECYCLE_PATH IN ITEMS
		"${ROOT}/code/render/ral/backends/opengl/ral_opengl_product.c"
		"${ROOT}/code/render/ral/backends/metal/ral_metal_present.mm"
		"${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c")
	file(READ "${MAP_LIFECYCLE_PATH}" MAP_LIFECYCLE_SOURCE)
	foreach(NEEDLE IN ITEMS "RenderSubmission_WorldLifecycleGeneration"
			"RAL_SKY_EVENT_MAP_TRANSITION"
			"skyWorldLifecycleGeneration")
		string(FIND "${MAP_LIFECYCLE_SOURCE}" "${NEEDLE}" POSITION)
		if(POSITION EQUAL -1)
			message(FATAL_ERROR "production sky map lifecycle ${MAP_LIFECYCLE_PATH} misses: ${NEEDLE}")
		endif()
	endforeach()
endforeach()
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" VULKAN_SKY_LIFECYCLE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_init.c" VULKAN_FRONTEND_LIFECYCLE)
foreach(NEEDLE IN ITEMS "R_FrontendWorldLifecycleGeneration"
		"RAL_ATMOSPHERE_EVENT_MAP_TRANSITION"
		"skyWorldLifecycleGeneration"
		"skyPlanFrameGeneration == UINT64_MAX"
		"request.frameGeneration = ++vk.atm.skyPlanFrameGeneration"
		"request.frameGeneration,")
	string(FIND "${VULKAN_SKY_LIFECYCLE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan production sky map lifecycle misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RenderSubmission_WorldLifecycleGeneration"
		"vk.probeCurrentProductValid = qfalse")
	string(FIND "${VULKAN_FRONTEND_LIFECYCLE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan frontend sky ownership boundary misses: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_shade.c" VULKAN_SHADE)
foreach(NEEDLE IN ITEMS "vk.probeCurrentProductValid"
		"TYPE_MULTI_TEXTURE_MUL2_IBL")
	string(FIND "${VULKAN_SHADE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan stale sky-radiance draw gate misses: ${NEEDLE}")
	endif()
endforeach()
foreach(CONSTANT_PATTERN IN ITEMS
		"RAL_ATMOSPHERE_MAX_VOLUMES[ \t]+64u"
		"RAL_ATMOSPHERE_MAX_LIGHTS[ \t]+256u"
		"RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS[ \t]+16u"
		"RAL_SKY_TIER_MAX_RADIANCE_SLICES_PER_FRAME[ \t]+4u")
	string(REGEX MATCH "${CONSTANT_PATTERN}" CONSTANT_MATCH "${HEADER}")
	if(NOT CONSTANT_MATCH)
		message(FATAL_ERROR "atmosphere RAL header constant drift: ${CONSTANT_PATTERN}")
	endif()
endforeach()
foreach(ANALYTIC_PATH IN ITEMS
		"${ROOT}/code/render/ral/backends/opengl/ral_opengl_product.c"
		"${ROOT}/code/render/ral/backends/metal/ral_metal_present.mm"
		"${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c")
	file(READ "${ANALYTIC_PATH}" ANALYTIC_SOURCE)
	foreach(NEEDLE IN ITEMS "RenderSubmission_AtmosphereSnapshot"
		"RenderSubmission_AtmosphereMediaSnapshot"
		"request.capabilities.analyticComposite = qtrue"
		"request.capabilities.fullClouds"
		"Ral_AtmospherePlan( &request")
		string(FIND "${ANALYTIC_SOURCE}" "${NEEDLE}" POSITION)
		if(POSITION EQUAL -1)
			message(FATAL_ERROR "analytic atmosphere executor ${ANALYTIC_PATH} misses: ${NEEDLE}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS "maxFroxelCount" "droppedVolumeCount"
		"temporalRejectCount" "compositeCount = 1u"
		"Temporal reconstruction is fused into froxel integration"
		"Ral_AtmosphereRuntimeAdvance" "RAL_ATMOSPHERE_INVALIDATE_DEVICE")
	string(FIND "${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "atmosphere RAL planner misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "SkyDirtyForMode" "pendingDirtyMask"
		"inFlightWorkMask" "targetRadianceRevision"
		"receipt.cancelledWorkMask" "next.previousRadianceRevision"
		"SkyDistantSceneValid" "receipt.distantSceneIdentity"
		"plan.distantSceneIdentity" "Ral_SkyTierBudget"
		"tierBudget.allowedWorkMask" "effectiveWorkBudget = MinU32")
	string(FIND "${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "sky/environment runtime lifecycle misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "activeParticleCount"
		"admittedCoverage" "heightgridActive" "depthIntersectionActive")
	string(FIND "${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "atmosphere weather planner misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_BACKEND_VULKAN" "RAL_BACKEND_COUNT"
		"receipt.froxelCount == 230400u" "receipt.zeroWork == qtrue"
		"RAL_ATMOSPHERE_EVENT_DEVICE_RECREATED"
		"RAL_ATMOSPHERE_EVENT_CAPABILITY_DOWNGRADE"
		"RAL_ATMOSPHERE_EVENT_SUN_CHANGE"
		"RAL_ATMOSPHERE_EVENT_WEATHER_TRANSITION"
		"RAL_ATMOSPHERE_INVALIDATE_SUN"
		"RAL_ATMOSPHERE_INVALIDATE_WEATHER")
	string(FIND "${TEST}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "atmosphere fixture coverage misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "Ral_SkyEnvironmentRuntimeInit"
		"runtime.inFlightWorkMask" "runtime.targetReady"
		"runtime.currentRadianceRevision == 11u"
		"Ral_SkyTierBudgetReceiptExact"
		"budgets[RAL_ATMOSPHERE_TIER_OFF].allowedWorkMask == 0u"
		"budgets[RAL_ATMOSPHERE_TIER_WEATHER].maxWorkItemsPerFrame == 1u")
	string(FIND "${TEST}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "sky runtime fixture coverage misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "weatherReceipt.familyMask == RAL_ATMOSPHERE_WEATHER_ALL"
		"weatherReceipt.activeParticleCount == 6144u"
		"weather.indoorExposure = 0.0f")
	string(FIND "${TEST}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "weather fixture matrix misses: ${NEEDLE}")
	endif()
endforeach()
string(REGEX MATCH "weather\\.heightgridAvailable[ \t]*=[ \t]*qfalse"
	HEIGHTGRID_FALLBACK_FIXTURE "${TEST}")
if(NOT HEIGHTGRID_FALLBACK_FIXTURE)
	message(FATAL_ERROR "weather fixture matrix misses disabled heightgrid fallback")
endif()
foreach(NEEDLE IN ITEMS "RAL_ATMOSPHERE_RESOURCE_SCENE_HDR"
		"RAL_ATMOSPHERE_RESOURCE_HISTORY_PREVIOUS"
		"RAL_ATMOSPHERE_RESOURCE_COMPOSED_HDR"
		"RAL_ATMOSPHERE_PASS_ID_MEDIA_INJECT"
		"RAL_ATMOSPHERE_PASS_ID_CLOUD_INJECT"
		"RAL_ATMOSPHERE_RESOURCE_CLOUD_MEDIA"
		"RAL_ATMOSPHERE_PASS_ID_COMPOSITE")
	string(FIND "${GRAPH_HEADER}${GRAPH_SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "atmosphere frame graph misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "ATM_SLEET" "ATM_HAIL" "ATM_DUST_ASH"
		"ATM_COLD" "ATM_FOG" "ATM_STORM"
		"CG_AtmosphericSkyMode" "CG_AtmosphericBuildSkyEnvironment"
		"ATMOSPHERE_SKY_AUTHORED" "ATMOSPHERE_SKY_PHYSICAL"
		"ATMOSPHERE_SKY_HYBRID" "WIRED_SKY_ENVIRONMENT_SCHEMA_VERSION"
		"sky->flags |= ATMOSPHERE_SKY_AUTHORED_AVAILABLE"
		"sky->sourceRevision = atm.sourceRevision"
		"atm.sourceRevision += 1u"
		"atm.skyMode >= ATMOSPHERE_SKY_PHYSICAL"
		"? ATMOSPHERE_QUALITY_FULL : ATMOSPHERE_QUALITY_ANALYTIC"
		"desc->timelineSeconds" "(float)cg.time * 0.001f"
		"trap_R_SetAtmosphere( &desc );"
		"!atm.heightgridEmitted"
		"ATMOSPHERE_EMITTER_BREATH"
		"ATMOSPHERE_EMITTER_GROUND_MIST"
		"ATMOSPHERE_EMITTER_DEBRIS")
	string(FIND "${CG_ATMOSPHERE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "app atmosphere producer misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "g_envSkyMode" "CVAR_SERVERINFO")
	string(FIND "${GAME_MAIN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "map-authored sky server authority misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "g_envDistantSkyMap" "g_envDistantSkyOrigin"
		"g_envDistantSkyScale" "g_envDistantSkyLodBias")
	string(FIND "${GAME_MAIN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "distant-sky server authority misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "G_SpawnString( \"skyMode\", \"authored\", &s )"
		"trap_Cvar_Set( \"g_envSkyMode\", s )")
	string(FIND "${GAME_SPAWN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "worldspawn sky authoring bridge misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "G_SpawnString( \"distantSky\", \"\", &s )"
		"G_SpawnString( \"distantSkyOrigin\", \"0 0 0\", &s )"
		"G_SpawnString( \"distantSkyScale\", \"16\", &s )"
		"G_SpawnString( \"distantSkyLodBias\", \"0\", &s )"
		"trap_Cvar_Set( \"g_envDistantSkyMap\", s )"
		"trap_Cvar_Set( \"g_envDistantSkyOrigin\", s )"
		"trap_Cvar_Set( \"g_envDistantSkyScale\", s )"
		"trap_Cvar_Set( \"g_envDistantSkyLodBias\", s )")
	string(FIND "${GAME_SPAWN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "worldspawn distant-sky bridge misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RENDER_DISTANT_WORLD_BASE"
		"MAX_RENDER_WORLDS           ( MAX_RENDER_APP_WORLDS * 2 )"
		"WorldContentIdentity)( int worldIndex, uint64_t *outIdentity )")
	string(FIND "${RENDER_PUBLIC}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "distant-sky public renderer contract misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "CL_DistantWorldIndex" "CL_UnloadDistantWorld"
		"CL_LoadDistantWorld" "MAP_LOAD_FLAG_RENDER_ONLY"
		"re.WorldContentIdentity( worldIndex, &identity )"
		"resolved.skyEnvironment.flags |= ATMOSPHERE_SKY_DISTANT_SCENE"
		"resolved.skyEnvironment.distantSceneIdentity = identity")
	string(FIND "${CLIENT_CGAME}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "distant-sky client lifecycle/identity bridge misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "skyCapabilities.distantScene = qtrue")
	string(FIND "${VULKAN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan distant-sky capability misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "R_FrontendDistantWorldSubmission"
		"RenderSubmission_DistantWorldSubmissionForActiveView"
		"RAL_SKY_DISTANT_MAX_VISIBLE_DRAWS")
	string(FIND "${VULKAN_FRONTEND}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan distant-sky frontend lowering misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "R_QueueDistantWorldPasses"
		"main authored sky -> distant non-sky -> main foreground/effects/UI"
		"surface->shader->isSky" "RENDER_DISTANT_WORLD_BASE"
		"R_CommandBufferCanReserve"
		"Distant lowering is optional presentation work"
		"queued = R_AddDrawSurfCmd")
	string(FIND "${VULKAN_SCENE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan distant-sky scene ordering misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "R_AddDrawSurfPassCmd" "cmd->worldIndex = worldIndex"
		"cmd->clearDepth = clearDepth" "cmd->finalizeView = finalizeView")
	string(FIND "${VULKAN_COMMANDS}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan distant-sky command receipt misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "R_SetWorldSlot( cmd->worldIndex )"
		"RB_BeginDrawingView( cmd->clearDepth, cmd->resetSkyVisibility )"
		"if ( !cmd->finalizeView )")
	string(FIND "${VULKAN_BACKEND}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan distant-sky native pass execution misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "appIndex -= RENDER_DISTANT_WORLD_BASE"
		"Level_Alloc( &clientApps[appIndex].memory"
		"Arena_Used( clientApps[appIndex].memory.levelArena )")
	string(FIND "${CLIENT_MAIN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "distant-sky app arena ownership misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "Info_ValueForKey( info, \"g_envSkyMode\" )"
		"CG_AtmosphericInit()")
	string(FIND "${CG_SERVER_COMMANDS}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "client sky authoring bridge misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "atmosphere_breath" "atmosphere_mist"
		"atmosphere_debris" "CG_ATMOSPHERE_PROFILE_BREATH"
		"ATMOSPHERE_STAGE_CONTINUOUS"
		"trap_R_RegisterAtmosphereEffectProfile")
	string(FIND "${CG_PARTICLES}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "app atmosphere effect graph misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
		"Unsupported or not-yet-completed: no legacy/approximate visibility path."
		"!trap_R_GetLensVisibility( ld.id, &vis ) || vis <= 0.0f"
		"there is no CPU trace or visual fallback")
	string(FIND "${CG_LENS_FLARE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "lens visibility no-fallback policy misses: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS
		"CG_Trace( &tr, cg.refdef.vieworg"
		"falls back to CG_Trace"
		"draw as today (pre-P5b")
	string(FIND "${CG_LENS_FLARE}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "lens visibility retained a legacy visual fallback: ${FORBIDDEN}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "weather <rain|snow|sleet|hail|dust|ash|cold|fog|storm|clean>"
		"!Q_stricmp( arg2, \"storm\" )" "!Q_stricmp( arg2, \"fog\" )"
		"!Q_stricmp( arg2, \"cold\" )")
	string(FIND "${GAME_COMMANDS}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "server atmosphere authoring misses: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "VkDevice" "MTLDevice" "WGPUDevice" "GLuint"
		"cg_local.h" "code/renderer" "code/renderer2")
	string(FIND "${SOURCE}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "atmosphere RAL core leaks ownership: ${FORBIDDEN}")
	endif()
	string(FIND "${GRAPH_SOURCE}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "atmosphere frame graph leaks ownership: ${FORBIDDEN}")
	endif()
	string(FIND "${FRONTEND}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "atmosphere frontend leaks ownership: ${FORBIDDEN}")
	endif()
endforeach()

# GPU workload telemetry must be measured by each native API and normalized by
# the shared receipt. CPU wall-clock or queue-completion latency is not GPU time.
foreach(NEEDLE IN ITEMS "RAL_SKY_GPU_TIMING_UNAVAILABLE"
		"RAL_SKY_GPU_TIMING_PENDING" "RAL_SKY_GPU_TIMING_VALID"
		"RAL_SKY_GPU_TIMING_FAILED" "physicalGpuDurationNanoseconds"
		"radianceGpuDurationNanoseconds")
	string(FIND "${HEADER}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "shared sky GPU telemetry misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "wired-qp-sky-gpu-ts" "Ral_CmdWriteTimestamp"
		"Ral_GetQueryResults" "vk_sky_gpu_ts_copy_timing")
	string(FIND "${VULKAN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan sky GPU timing misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "GL_TIME_ELAPSED_VALUE" "BeginQuery"
		"GetQueryObjectui64v" "skyTimingDurationNanoseconds")
	string(FIND "${OPENGL}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "OpenGL sky GPU timing misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "GPUStartTime" "GPUEndTime"
		"skyPhysicalGpuDurationNanoseconds"
		"skyRadianceGpuDurationNanoseconds")
	string(FIND "${METAL_PRESENT}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Metal sky GPU timing misses: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "timestamp-query" "createQuerySet"
		"timestampWrites" "resolveQuerySet" "copyBufferToBuffer"
		"timing.readback.mapAsync" "gpuDurationNanoseconds")
	string(FIND "${WEBGPU_HOST}${WEBGPU_DISPATCH}${WEBGPU_BRIDGE}${WEBGPU_COMMAND}${WEBGPU_SKY}${WEBGPU}"
		"${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU sky GPU timing misses: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "performance.now" "Date.now")
	string(FIND "${WEBGPU_HOST}${WEBGPU_COMMAND}${WEBGPU_SKY}${WEBGPU}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU sky telemetry uses CPU wall time: ${FORBIDDEN}")
	endif()
endforeach()
# Every adapter must upload the same cloud-aware source payload, not maintain
# its own exposure/sun packing formula.
foreach(ADAPTER IN ITEMS VULKAN METAL_PRESENT OPENGL WEBGPU_SKY)
	string(FIND "${${ADAPTER}}" "Ral_SkyRadianceSourceParamsBuild" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "${ADAPTER} bypasses common sky radiance source parameters")
	endif()
endforeach()
string(FIND "${SKY_RADIANCE_SOURCE}" "1.0f - cloudCoverage * sky->cloudSunOcclusion" POSITION)
if(POSITION EQUAL -1)
	message(FATAL_ERROR "sky source ignores shared cloud/sun transmission")
endif()
message(STATUS "RAL atmosphere plan policy: PASS")
