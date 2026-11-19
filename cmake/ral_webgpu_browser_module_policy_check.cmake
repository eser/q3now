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
	"${BACKEND}/ral_webgpu_sky.h"
	"${BACKEND}/ral_webgpu_sky.c"
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
file(READ "${BACKEND}/ral_webgpu_browser_host.mjs" HOST_SOURCE)
foreach(NEEDLE IN ITEMS "width: 1280" "height: 720" "noInitialRun: true" "installRalWebGpuBrowserDispatch" "_RalWebGpu_BrowserModuleStart" "_GetRefAPI" "_WiredWebGpu_RendererPoll" "unsupported" "lost" "recreateRalWebGpuBrowserModule")
	string(FIND "${SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU browser module misses required authority: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "depth32float" "colorLoadOp" "depthLoadOp"
		"depthStencilAttachment")
	string(FIND "${HOST_SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU browser host misses explicit depth/pass semantics: ${NEEDLE}")
	endif()
endforeach()
file(READ "${BACKEND}/ral_webgpu_renderer_module.c" RENDERER_SOURCE)
foreach(NEEDLE IN ITEMS "GetRefAPI" "RenderSubmission_Init" "RenderSubmission_LoadWorld" "RalWebGpu_ProductRender" "RalWebGpu_ProductPoll" "RalWebGpu_PresentationPresent" "RalWebGpu_WeatherPlan" "RalWebGpu_WeatherBegin" "RalWebGpu_WeatherPoll" "RalWebGpu_ProductSetWeather" "Cvar_Get( \"r_brightness\", \"1.4\"" "Cvar_CheckRange( s_module.brightness, \"0\", \"32\", CV_FLOAT )" "Ral_DisplayVisibilityPlanBuild" "displayVisibility" "CreateEntityLightingOwner" "RalWebGpu_ProductSetEntityLightingBindGroup" "s_entityVertexWgsl" "@builtin(instance_index)" "@location(4) normal" "lightingIndex * 4u" "CreateWorldMaterialOwner" "RalWebGpu_ProductSetWorldMaterialBindGroup" "s_worldVertexWgsl" "s_worldFragmentWgsl" "authored*emission" "physicalSky" "skySurface" "Ral_SkySurfacePlan( &plan->skyEnvironment" "skySurface.physicalBlend" "RefreshWorldAtmosphereOwner" "RalWebGpu_SkySampleResources" "RalWebGpu_SkyFallbackResources" "PresentationChanged" "DestroyOwners" "WiredWebGpu_RendererSmokeBegin" "WiredWebGpu_RendererSmokePoll")
	string(FIND "${RENDERER_SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU renderer module misses required ownership: ${NEEDLE}")
	endif()
endforeach()
# A non-NULL renderer callback is a native-support promise. Sprite, decal,
# ribbon, beam and the scene-depth lens oracle have native WebGPU products;
# the general emitter family remains fail closed.
foreach(NEEDLE IN ITEMS "e->EmitParticles")
	if(NOT RENDERER_SOURCE MATCHES "${NEEDLE}[^\n]*= NULL;")
		message(FATAL_ERROR "WebGPU advertises an effect primitive without native product lowering: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
    "= SubmitLensSource;" "= GetLensVisibility;"
    "RalWebGpu_BrowserBridgeBeginLensVisibility"
    "RalWebGpu_BrowserBridgePollLensVisibility"
    "RalWebGpu_BrowserBridgeResetLensVisibility"
	"e->AddSpriteToScene" "= SubmitEffectSprite;"
	"RenderSubmission_AddEffectSprite" "e->AddRibbonToScene"
	"= SubmitEffectRibbon;" "RenderSubmission_AddEffectRibbon"
	"e->AddBeamToScene" "= SubmitEffectBeam;"
	"RenderSubmission_AddEffectBeam"
	"e->AddDecalToScene" "= SubmitEffectDecal;"
	"RenderSubmission_AddEffectDecal"
	"WEBGPU_PIPELINE_EFFECT_ADDITIVE_NO_DEPTH"
	"WEBGPU_PIPELINE_EFFECT_ALPHA_DEPTH")
	string(FIND "${RENDERER_SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU native sprite/ribbon lowering misses: ${NEEDLE}")
	endif()
endforeach()
file(READ "${BACKEND}/ral_webgpu_product.c" PRODUCT_SOURCE)
foreach(NEEDLE IN ITEMS
	"RenderSubmission_EffectPrimitiveSnapshots"
	"RAL_WEBGPU_EFFECT_PIPELINE_ADDITIVE_NO_DEPTH"
	"RAL_WEBGPU_EFFECT_PIPELINE_ALPHA_DEPTH"
	"effectSpriteDrawCount" "effectDecalDrawCount"
	"effectRibbonDrawCount" "effectBeamDrawCount"
	"textureBindGroupSlot = 2u")
	string(FIND "${PRODUCT_SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU effect product lowering misses: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS
	"static void NoBeam" "static void AddEffectEmitter"
	"static void AddEffectDecal" "static void NoLens"
	"static qboolean NoLensVisibility")
	string(FIND "${RENDERER_SOURCE}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU retained a submission-only/no-op WiredFX callback: ${FORBIDDEN}")
	endif()
endforeach()
string(FIND "${RENDERER_SOURCE}" "RefreshWorldAtmosphereOwner( qtrue )" FALLBACK_CUTOVER_POSITION)
string(FIND "${RENDERER_SOURCE}" "RalWebGpu_SkyUnloadWorld" SKY_UNLOAD_POSITION)
if(FALLBACK_CUTOVER_POSITION EQUAL -1 OR SKY_UNLOAD_POSITION EQUAL -1
		OR NOT FALLBACK_CUTOVER_POSITION LESS SKY_UNLOAD_POSITION)
	message(FATAL_ERROR "WebGPU world unload must publish the permanent fallback bind group before destroying per-world sky resources")
endif()
file(READ "${BACKEND}/ral_webgpu_sky.c" SKY_SOURCE)
foreach(NEEDLE IN ITEMS "RalWebGpu_SkyFallbackResources" "fallbackReceipt.resourceIdentity" "samplerReceipt.resourceIdentity")
	string(FIND "${SKY_SOURCE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU permanent sky fallback ownership misses: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/tests/ral_webgpu_browser_module_probe.mjs" PROBE_SOURCE)
foreach(NEEDLE IN ITEMS "explicit-texture-usage" "physical-sky-lut-storage"
	"bounded-sky-compute" "sky-submission-completion"
	"weather-ping-pong-pool" "weather-compute-dispatch"
	"weather-instanced-draw")
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
file(READ "${ROOT}/CMakeLists.txt" BUILD_SOURCE)
if(NOT BUILD_SOURCE MATCHES "DEPENDS[ \t\r\n]+ral_webgpu_smoke_assets[ \t\r\n]+ral_webgpu_sky_shaders")
	message(FATAL_ERROR "Browser WASM must regenerate all embedded sky/exposure shaders before emcc")
endif()
message(STATUS "RAL WebGPU browser module bootstrap policy: PASS")
