# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(BACKEND "${ROOT}/code/render/ral/backends/webgpu")
set(FILES
	"${BACKEND}/ral_webgpu_browser_module.mjs"
	"${BACKEND}/ral_webgpu_browser_emscripten.h"
	"${BACKEND}/ral_webgpu_browser_emscripten.c"
	"${BACKEND}/ral_webgpu_renderer_module.h"
	"${BACKEND}/ral_webgpu_renderer_module.c"
	"${BACKEND}/ral_webgpu_weather.h"
	"${BACKEND}/ral_webgpu_weather.c"
	"${ROOT}/tests/ral_webgpu_browser_module_probe.mjs"
	"${ROOT}/tests/ral_webgpu_browser_module_test.mjs")
foreach(FILE IN LISTS FILES)
	if(NOT EXISTS "${FILE}")
		message(FATAL_ERROR "missing WebGPU browser module contract file: ${FILE}")
	endif()
	file(READ "${FILE}" TEXT)
	foreach(FORBIDDEN IN ITEMS "WebGL2" "webgl2" "getContext(\"webgl" "SDL_" "code/renderer" "code/renderer2")
		string(FIND "${TEXT}" "${FORBIDDEN}" POSITION)
		if(NOT POSITION EQUAL -1)
			message(FATAL_ERROR "forbidden fallback/deprecated ownership in ${FILE}: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
file(READ "${BACKEND}/ral_webgpu_browser_module.mjs" SOURCE)
foreach(NEEDLE IN ITEMS "width: 1280" "height: 720" "noInitialRun: true" "installRalWebGpuBrowserDispatch" "_RalWebGpu_BrowserModuleStart" "_GetRefAPI" "_WiredWebGpu_RendererPoll" "unsupported" "lost" "recreateRalWebGpuBrowserModule")
	string(FIND "${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU browser module misses required authority: ${NEEDLE}")
	endif()
endforeach()
file(READ "${BACKEND}/ral_webgpu_renderer_module.c" RENDERER_SOURCE)
foreach(NEEDLE IN ITEMS "GetRefAPI" "RenderSubmission_Init" "RenderSubmission_LoadWorld" "RalWebGpu_ProductRender" "RalWebGpu_ProductPoll" "RalWebGpu_PresentationPresent" "RalWebGpu_WeatherPlan" "RalWebGpu_WeatherBegin" "RalWebGpu_WeatherPoll" "RalWebGpu_ProductSetWeather" "Cvar_Get( \"r_brightness\", \"1\"" "Cvar_CheckRange( s_module.brightness, \"0\", \"32\", CV_FLOAT )" "Ral_DisplayVisibilityPlanBuild" "displayVisibility" "CreateEntityLightingOwner" "RalWebGpu_ProductSetEntityLightingBindGroup" "s_entityVertexWgsl" "@builtin(instance_index)" "@location(4) normal" "lightingIndex * 4u" "CreateWorldMaterialOwner" "RalWebGpu_ProductSetWorldMaterialBindGroup" "s_worldVertexWgsl" "s_worldFragmentWgsl" "color.rgb * emission" "PresentationChanged" "DestroyOwners" "WiredWebGpu_RendererSmokeBegin" "WiredWebGpu_RendererSmokePoll")
	string(FIND "${RENDERER_SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU renderer module misses required ownership: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/tests/ral_webgpu_browser_module_probe.mjs" PROBE_SOURCE)
foreach(NEEDLE IN ITEMS "weather-ping-pong-pool"
	"weather-compute-dispatch" "weather-instanced-draw")
	string(FIND "${PROBE_SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU browser smoke lost weather proof: ${NEEDLE}")
	endif()
endforeach()
file(READ "${BACKEND}/ral_webgpu_browser_emscripten.c" C_SOURCE)
foreach(NEEDLE IN ITEMS "RalWebGpu_BrowserModuleStart" "RalWebGpu_RuntimeBegin" "RalWebGpu_BrowserModulePoll" "RalWebGpu_PresentationConfigure" "RalWebGpu_BrowserModuleStop")
	string(FIND "${C_SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU browser C module misses required authority: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL WebGPU browser module bootstrap policy: PASS")
