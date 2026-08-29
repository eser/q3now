# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(BACKEND "${ROOT}/code/render/ral/backends/webgpu")
foreach(FILE IN ITEMS ral_webgpu_product.h ral_webgpu_product.c
		ral_webgpu_weather.h ral_webgpu_weather.c)
	file(READ "${BACKEND}/${FILE}" TEXT)
	foreach(FORBIDDEN IN ITEMS "vulkan.h" "VkDevice" "OpenGL" "Metal/" "WebGL2" "WEBGL2" "GLES" "code/renderer" "code/renderer2")
		string(FIND "${TEXT}" "${FORBIDDEN}" POSITION)
		if(NOT POSITION EQUAL -1)
			message(FATAL_ERROR "forbidden backend/deprecated ownership in ${FILE}: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
file(READ "${BACKEND}/ral_webgpu_weather.c" WEATHER_SOURCE)
foreach(NEEDLE IN ITEMS "RAL_WEBGPU_WEATHER_PARTICLE_CAPACITY"
	"Ral_AtmospherePlanWeather" "pools[2]" "computeBindGroups[2]"
	"RAL_WEBGPU_PASS_COMPUTE" "RalWebGpu_CommandRecordComputeDispatch"
	"renderBindGroups" "out->drawIndexCount" "RenderSubmission_ViewSnapshot")
	string(FIND "${WEATHER_SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU weather executor misses native authority: ${NEEDLE}")
	endif()
endforeach()
file(READ "${BACKEND}/ral_webgpu_product.c" SOURCE)
foreach(NEEDLE IN ITEMS "RalWebGpu_FrontendPlanBuild" "RalWebGpu_WriteTexture" "RalWebGpu_WriteBuffer" "RalWebGpu_CommandRecordIndexedDraw" "RalWebGpu_CommandSubmit" "BuildEntityGeometry" "BuildMiscGeometry" "model.positions" "model.normals" "RT_SPRITE" "RT_BEAM" "RenderSubmission_EffectSnapshots" "RenderSubmission_UiPrimitives" "RAL_WEBGPU_DRAW_EFFECT" "RAL_WEBGPU_DRAW_UI" "lightmapMaterial" "world.batchCount" "RalWebGpu_ProductSetWeather" "weatherDrawCount" "activeParticleCount" "EntityLighting" "blendedCoefficientsQ16" "entityLightingData" "firstInstance = i" "localIrradianceDrawCount" "bindGroupCount = 2u" "RAL_WEBGPU_WORLD_MATERIAL_BYTES" "worldMaterialData" "emissionRadianceQ16" "worldMaterialBindGroup")
	string(FIND "${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU product world lowering misses authority: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL WebGPU product world/entity/effect/UI ownership policy: PASS")
