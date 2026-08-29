# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/core/ral_atmosphere.h" HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_atmosphere.c" SOURCE)
file(READ "${ROOT}/code/render/ral/core/ral_atmosphere_frame_graph.h" GRAPH_HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_atmosphere_frame_graph.c" GRAPH_SOURCE)
file(READ "${ROOT}/tests/ral_atmosphere_test.c" TEST)
file(READ "${ROOT}/code/render/ral/backends/opengl/ral_opengl_product.c" OPENGL)
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm" METAL)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c" WEBGPU)
file(READ "${ROOT}/code/cgame/cg_atmospheric.c" CG_ATMOSPHERE)
file(READ "${ROOT}/code/cgame/wired/cg_wired_particles.c" CG_PARTICLES)
file(READ "${ROOT}/code/game/g_cmds.c" GAME_COMMANDS)
foreach(NEEDLE IN ITEMS "RAL_ATMOSPHERE_MAX_VOLUMES"
		"RAL_ATMOSPHERE_MAX_LIGHTS"
		"RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS"
		"RAL_ATMOSPHERE_PASS_SINGLE_COMPOSITE"
		"RAL_ATMOSPHERE_FALLBACK_BACKEND_UNAVAILABLE"
		"RAL_ATMOSPHERE_FALLBACK_NO_STORAGE_BUFFER"
		"RAL_ATMOSPHERE_WEATHER_ALL"
		"Ral_AtmospherePlanWeather"
		"storageBuffers")
	string(FIND "${HEADER}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "atmosphere RAL header misses: ${NEEDLE}")
	endif()
endforeach()
foreach(CONSTANT_PATTERN IN ITEMS
		"RAL_ATMOSPHERE_MAX_VOLUMES[ \t]+64u"
		"RAL_ATMOSPHERE_MAX_LIGHTS[ \t]+256u"
		"RAL_ATMOSPHERE_MAX_SHADOWED_LIGHTS[ \t]+16u")
	string(REGEX MATCH "${CONSTANT_PATTERN}" CONSTANT_MATCH "${HEADER}")
	if(NOT CONSTANT_MATCH)
		message(FATAL_ERROR "atmosphere RAL header constant drift: ${CONSTANT_PATTERN}")
	endif()
endforeach()
foreach(ANALYTIC_PATH IN ITEMS
		"${ROOT}/code/render/ral/backends/opengl/ral_opengl_product.c"
		"${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm"
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
		"RAL_ATMOSPHERE_EVENT_CAPABILITY_DOWNGRADE")
	string(FIND "${TEST}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "atmosphere fixture coverage misses: ${NEEDLE}")
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
foreach(NEEDLE IN ITEMS "atmosphere_breath" "atmosphere_mist"
		"atmosphere_debris" "CG_ATMOSPHERE_PROFILE_BREATH"
		"ATMOSPHERE_STAGE_CONTINUOUS"
		"trap_R_RegisterAtmosphereEffectProfile")
	string(FIND "${CG_PARTICLES}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "app atmosphere effect graph misses: ${NEEDLE}")
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
endforeach()
message(STATUS "RAL atmosphere plan policy: PASS")
