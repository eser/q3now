# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/render/ral/backends/metal/ral_metal_present.h")
set(S "${ROOT}/code/render/ral/backends/metal/ral_metal_present.mm")
set(T "${ROOT}/tests/ral_metal_present_test.mm")
set(MODULE_TEST "${ROOT}/tests/ral_metal_module_test.mm")
set(MSL "${ROOT}/code/render/ral/backends/metal/shaders/ui_solid.msl")
set(MATERIAL "${ROOT}/code/render/frontend/render_submission_material.h")
set(MODEL "${ROOT}/code/render/frontend/render_submission_model.h")
set(CMAKE_FILE "${ROOT}/CMakeLists.txt")
foreach(path IN ITEMS "${H}" "${S}" "${T}" "${MODULE_TEST}" "${MSL}" "${MATERIAL}" "${MODEL}" "${CMAKE_FILE}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing Metal presentation contract file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)
file(READ "${MODULE_TEST}" MODULE_TEST_SOURCE)
file(READ "${MSL}" SHADER)
file(READ "${MATERIAL}" MATERIAL_HEADER)
file(READ "${MODEL}" MODEL_HEADER)
file(READ "${CMAKE_FILE}" BUILD)
foreach(needle IN ITEMS
	"CAMetalLayer" "MTLPixelFormatBGRA8Unorm" "MTLPixelFormatRGBA16Float"
	"kCGColorSpaceSRGB" "kCGColorSpaceExtendedLinearDisplayP3"
	"wantsExtendedDynamicRangeContent" "maximumDrawableCount"
	"displaySyncEnabled" "nextDrawable" "presentDrawable"
	"lastAcquireGeneration" "lastPresentGeneration" "CGColorSpaceGetName"
	"RalMetal_CoreBeginCommand" "RalMetal_CorePublishSubmission"
	"RalMetal_PresentAdoptBorrowedLayer" "present->ownsLayer" "receipt.ownsLayer"
	"newLibraryWithData" "drawPrimitives:MTLPrimitiveTypeTriangle"
	"RenderSubmission_UiPrimitives" "loweredUiPrimitiveCount"
	"RenderSubmission_MaterialSnapshot" "texturedUiPrimitiveCount"
	"uiClampSampler" "uiRepeatSampler" "MTLPixelFormatRGBA8Unorm"
	"MTLPixelFormatRGBA8Unorm_sRGB" "snapshot.srgb"
	"setFragmentTexture" "setFragmentSamplerState" "uiMsdfPipeline"
	"RenderSubmission_WorldSnapshot" "drawIndexedPrimitives:MTLPrimitiveTypeTriangle"
	"renderWorldBatch_t" "loweredWorldIndexCount" "loweredWorldBatchCount"
	"texturedWorldBatchCount" "lightmappedWorldBatchCount" "patchWorldBatchCount"
	"maskedWorldBatchCount" "blendedWorldBatchCount" "depthWriteWorldBatchCount"
	"worldBlendPipeline" "worldDepthReadState" "setFragmentBytes"
	"BuildWorldAtmosphereParams" "RenderSubmission_AtmosphereSnapshot"
	"ralMetalWorldAtmosphereParams_t"
	"RenderSubmission_EntityCommands" "RenderSubmission_ModelSnapshot"
	"BuildEntityVertices" "loweredEntityIndexCount" "modelEntityCount"
	"primitiveEntityCount" "temporalEntityCount" "unresolvedEntityCount"
	"blendedCoefficientsQ16" "localSh[4][3]"
	"PlanWeather" "RenderSubmission_ViewSnapshot" "EncodeWeather"
	"weatherDispatchCount" "weatherDrawCount"
	"EnsureSceneColor" "MTLPixelFormatRGBA16Float"
	"EnsureToneMapPipeline" "wired_tonemap_fragment"
	"present->sceneColorCache" "RalMetal_PresentSetToneMap"
	"drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0u"
	"instanceCount:weatherPlan.activeParticleCount")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal presentation lost native/lifecycle seam: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "RENDER_SUBMISSION_MAX_ENTITIES" "renderEntityCommand_t"
	"renderModelSnapshot_t" "renderModelBatch_t")
	string(FIND "${MODEL_HEADER}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "neutral model/entity header lost bounded fact: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "wired_ui_vertex" "wired_ui_fragment"
	"wired_ui_msdf_fragment" "texture2d<float>" "image.sample" "fwidth"
	"wired_world_vertex" "wired_world_fragment" "WorldMaterialParams"
	"WorldAtmosphereParams" "worldPosition" "colorVisibility"
	"3.912023" "heightCloud" "transmittance"
	"baseImage.sample" "lightmapImage.sample" "discard_fragment"
	"material.localSh[1].rgb * in.normal.x"
	"material.emissiveRadiance.rgb"
	"wired_fullscreen_vertex" "wired_tonemap_fragment"
	"wired_tonemap_pbr_neutral" "wired_tonemap_agx"
	"wired_tonemap_lottes" "params.displayVisibility.xyz"
	"wired_weather_compute" "wired_weather_vertex" "wired_weather_fragment"
	"WeatherFrame" "WeatherParticle")
	string(FIND "${SHADER}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal product UI shader lost entry point: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "ATMOSPHERE_QUALITY_FULL"
	"atmosphere.precipitation[1] = 0.5f"
	"atmosphere.precipitation[2] = 0.5f"
	"receipt.presentation.weather.activeParticleCount == 8192u"
	"receipt.presentation.weatherDispatchCount == 1u"
	"receipt.presentation.weatherDrawCount == 1u")
	string(FIND "${MODULE_TEST_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal module lost authored GPU-weather coverage: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "RENDER_SUBMISSION_MAX_MATERIALS"
	"RENDER_SUBMISSION_MAX_MATERIAL_BYTES" "generation" "digest" "rgba8"
	"clampToEdge" "msdf" "srgb" "renderAlphaMode_t" "alphaCutoff"
	"depthWrite")
	string(FIND "${MATERIAL_HEADER}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "neutral material payload lost bounded/native-free fact: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "ral_metal_product_metallib" "xcrun -sdk macosx metal"
	"cmake/embed_binary.cmake")
	string(FIND "${BUILD}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal product lost offline shader build seam: ${needle}")
	endif()
endforeach()
string(FIND "${SOURCE}" "newLibraryWithSource" runtime_compile)
if(NOT runtime_compile EQUAL -1)
	message(FATAL_ERROR "Metal product must not compile shaders at runtime")
endif()
string(FIND "${SOURCE}" "[present->drawable release];\n\tfree( present->captureRgb );\n\tif ( present->ownsLayer ) [present->layer release];" release_order)
if(release_order EQUAL -1)
	message(FATAL_ERROR "Metal presentation lost drawable-before-layer teardown")
endif()
foreach(forbidden IN ITEMS "NSWindow" "SDL_Window" "VkSwapchain" "MoltenVK")
	string(FIND "${HEADER}${SOURCE}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal presentation gained forbidden product/native seam: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"createInfo.desiredWidth = 0u" "unsupportedFormat" "fifo.desiredImageCount = 4u"
	"fifo.unboundedImageCount = 5u" "unlockedPolicy.preferences[4].unboundedImageCount == 4u"
	"selected.requestedImageCount"
	"RAL_PRESENT_MAILBOX" "&exactDrawable, NULL, sdrClear"
	"staleCore.generation++" "exactLayer.presentationGeneration++"
	"!RalMetal_PresentAcquire" "!RalMetal_PresentReconfigure"
	"sdrClear[0] = NAN" "RAL_FORMAT_R16G16B16A16_SFLOAT"
	"layerReceipt.extendedDynamicRange == qtrue" "RalMetal_CorePublishDeviceLoss"
	"layerReceipt.ownsLayer == qtrue" "exactLayer.ownsLayer = qfalse")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal presentation host lost mutation/SDR-HDR coverage: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "RT_SPRITE" "RT_BEAM" "rotation = 45.0f"
	"ATMOSPHERE_QUALITY_ANALYTIC" "atmosphere.mediaDensity = 0.1f"
	"RenderSubmission_SetAtmosphere"
	"RenderSubmission_RegisterInlineModel" "RenderSubmission_AddEntity"
	"loweredEntityIndexCount == 63u" "loweredEntityBatchCount == 6u"
	"modelEntityCount == 1u" "primitiveEntityCount == 2u"
	"temporalEntityCount == 1u" "unresolvedEntityCount == 0u"
	"loweredEffectSpriteCount == 1u" "loweredEffectDecalCount == 1u"
	"loweredEffectRibbonCount == 1u" "effectEmitterDispatchCount == 1u"
	"effectParticleDrawCount == 8u"
	"persistentReceipt.loweredEntityIndexCount == 6u"
	"persistentReceipt.loweredEffectDecalCount == 1u"
	"MUTATE_PRESENT( loweredEntityIndexCount )")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal presentation host lost entity/temporal coverage: ${needle}")
	endif()
endforeach()
message(STATUS "RAL Metal presentation policy: PASS")
