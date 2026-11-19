# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/render/ral/backends/metal/ral_metal_present.h")
set(S "${ROOT}/code/render/ral/backends/metal/ral_metal_present.mm")
set(T "${ROOT}/tests/ral_metal_present_test.mm")
set(MODULE_TEST "${ROOT}/tests/ral_metal_module_test.mm")
set(MODULE "${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm")
set(MSL "${ROOT}/code/render/ral/backends/metal/shaders/ui_solid.msl")
set(MATERIAL "${ROOT}/code/render/frontend/render_submission_material.h")
set(MODEL "${ROOT}/code/render/frontend/render_submission_model.h")
set(CMAKE_FILE "${ROOT}/CMakeLists.txt")
set(PARTICLE_ABI "${ROOT}/code/render/ral/core/ral_particle_contract.h")
set(VULKAN_PARTICLE_HOST "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.h")
set(PARTICLE_FRAGMENT "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/portable/particle_frag_spv.msl")
set(SHADER_TRANSLATOR "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/compile_xlate.mjs")
foreach(path IN ITEMS "${H}" "${S}" "${T}" "${MODULE_TEST}" "${MODULE}" "${MSL}" "${MATERIAL}" "${MODEL}" "${CMAKE_FILE}" "${PARTICLE_ABI}" "${VULKAN_PARTICLE_HOST}" "${PARTICLE_FRAGMENT}" "${SHADER_TRANSLATOR}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing Metal presentation contract file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)
file(READ "${MODULE_TEST}" MODULE_TEST_SOURCE)
file(READ "${MODULE}" MODULE_SOURCE)
file(READ "${MSL}" SHADER)
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_atmosphere.mm" ATMOSPHERE_SOURCE)
foreach(needle IN ITEMS "atmosphere_froxel_inject_metallib" "atmosphere_froxel_light_metallib"
	"atmosphere_froxel_cloud_metallib" "atmosphere_froxel_integrate_metallib"
	"forwardplus_tile_metallib" "wired_reserve" "memoryBarrierWithScope"
	"RAL_LIGHT_INJECT_ATMOSPHERE")
	string(FIND "${ATMOSPHERE_SOURCE}" "${needle}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR "Metal FULL atmosphere lost canonical kernel/ownership: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "wired_atmosphere_compute" "AtmosphereComputeParams" "atmosphere_hash3")
	string(FIND "${SHADER}" "${needle}" found)
	if(NOT found EQUAL -1)
		message(FATAL_ERROR "parallel approximate Metal atmosphere returned: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "waitUntilCompleted" "newLibraryWithSource" "[command commit]")
	string(FIND "${ATMOSPHERE_SOURCE}" "${needle}" found)
	if(NOT found EQUAL -1)
		message(FATAL_ERROR "Metal atmosphere must not compile/submit/wait independently: ${needle}")
	endif()
endforeach()
file(READ "${MATERIAL}" MATERIAL_HEADER)
file(READ "${MODEL}" MODEL_HEADER)
file(READ "${CMAKE_FILE}" BUILD)
file(READ "${PARTICLE_ABI}" PARTICLE_ABI_SOURCE)
file(READ "${VULKAN_PARTICLE_HOST}" VULKAN_PARTICLE_HOST_SOURCE)
file(READ "${PARTICLE_FRAGMENT}" PARTICLE_FRAGMENT_SOURCE)
file(READ "${SHADER_TRANSLATOR}" SHADER_TRANSLATOR_SOURCE)
foreach(needle IN ITEMS
	"wiredMetalUpload entityVertexUpload( present->entityVertexUploadCache )"
	"wiredMetalUpload entityIndexUpload( present->entityIndexUploadCache )"
	"wiredMetalUpload uiVertexUpload( present->uiVertexUploadCache )"
	"wiredMetalParticleUploads particleUploads( present )"
	"particleUploads.wired_publish( split.scene ? split.scene : command )"
	"for ( auto &cache : present->particleUploadCaches ) cache.wired_reap()"
	"for ( auto &cache : present->particleUploadCaches ) cache.wired_clear()"
	"entityVertexUpload.wired_publish( split.scene ? split.scene : command )"
	"entityIndexUpload.wired_publish( split.scene ? split.scene : command )"
	"uiVertexUpload.wired_publish( command )"
	"present->uiVertexUploadCache.wired_reap()"
	"present->uiVertexUploadCache.wired_clear()")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal upload completion-owner routing lost: ${needle}")
	endif()
endforeach()
string(FIND "${TEST}" "wired_upload_cache_busy_retirement( RalMetal_CoreNativeDevice( core ) )" upload_test)
if(upload_test EQUAL -1)
	message(FATAL_ERROR "Metal busy-cache native retirement test is not executed")
endif()
foreach(needle IN ITEMS "uploads.allocations[input] == 1u"
	"RenderEffectParticlePlan_ReceiptExact( &candidate,")
	string(FIND "${TEST}${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal particle upload reuse lost allocation/receipt coverage: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "RAL_METAL_MAX_DIRECT_LIGHTS_PER_DRAW 4u"
	"RenderSubmission_EffectSnapshots" "PopulateDirectLights"
	"directLightPositionRadius" "directLightColorFalloff"
	"directLightEndLinear" "directLightDrawCount"
	"directLightReferenceCount")
	string(FIND "${SOURCE}${HEADER}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal direct-light lowering lost fact: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"../../../core/ral_particle_contract.h"
	"typedef ralParticleParmGpu_t particleParmGPU_t"
	"typedef ralParticleClassGpu_t particleClassGPU_t"
	"typedef ralParticleGpu_t particleGPU_t"
	"typedef ralParticleSpawnGpu_t particleSpawnGPU_t"
	"typedef ralParticleFrameGpu_t particleFrame_t")
	string(FIND "${VULKAN_PARTICLE_HOST_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Vulkan/Metal canonical RAL particle ABI drifted: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"CAMetalLayer" "MTLPixelFormatBGRA8Unorm" "MTLPixelFormatRGBA16Float"
	"kCGColorSpaceSRGB" "kCGColorSpaceExtendedLinearDisplayP3"
	"wantsExtendedDynamicRangeContent" "maximumDrawableCount"
	"displaySyncEnabled" "nextDrawable" "presentDrawable"
	"lastAcquireGeneration" "lastSubmittedGeneration" "CGColorSpaceGetName"
	"RalMetal_CoreBeginCommand" "RalMetal_CorePublishSubmission"
	"RalMetal_PresentAdoptBorrowedLayer" "present->ownsLayer" "receipt.ownsLayer"
	"newLibraryWithData" "drawPrimitives:MTLPrimitiveTypeTriangle"
	"RenderSubmission_UiPrimitives" "loweredUiPrimitiveCount"
	"RenderSubmission_MaterialSnapshot" "texturedUiPrimitiveCount"
	"uiClampSampler" "uiRepeatSampler" "MTLPixelFormatRGBA8Unorm"
	"MTLPixelFormatRGBA8Unorm_sRGB" "snapshot.srgb"
	"setFragmentTexture" "setFragmentSamplerState" "uiMsdfPipeline"
	"setFragmentTextures:materialTextures" "setFragmentSamplerStates:materialSamplers"
	"setFragmentTextures:entityTextures" "setFragmentSamplerStates:entitySamplers"
	"withRange:NSMakeRange( 0u, 17u )"
	"readbackSceneCommands[readbackSlot] = [split.scene retain]"
	"? firstCommitTime : CACurrentMediaTime()"
	"timelineSampleCount" "timelineUnavailableCount"
	"withRange:NSMakeRange( 0u, 6u + RENDER_MATERIAL_MAX_STAGES )"
	"RenderSubmission_WorldSnapshot" "drawIndexedPrimitives:MTLPrimitiveTypeTriangle"
	"renderWorldBatch_t" "loweredWorldIndexCount" "loweredWorldBatchCount"
	"texturedWorldBatchCount" "lightmappedWorldBatchCount" "patchWorldBatchCount"
	"maskedWorldBatchCount" "blendedWorldBatchCount" "depthWriteWorldBatchCount"
	"worldBlendPipeline" "worldDepthReadState" "setFragmentBytes"
	"BuildWorldAtmosphereParams" "RenderSubmission_AtmosphereSnapshot"
	"ralMetalWorldAtmosphereParams_t"
	"skyCapabilities.distantScene = qtrue"
	"RenderSubmission_DistantWorldSubmissionForActiveView"
	"PrepareWorldSnapshotBuffers" "DistantWorldBatchSelected"
	"distantWorldDepthReset" "MTLLoadActionClear"
	"Distant geometry contributes color behind the gameplay world"
	"RenderSubmission_EntityCommands" "RenderSubmission_ModelSnapshot"
	"BuildEntityVertices" "loweredEntityIndexCount" "modelEntityCount"
	"RenderEffectDecalPool_Advance" "RenderEffectBeamPool_Advance"
	"present->effectPoolIndex = effectCandidateIndex" "loweredEffectBeamCount"
	"const float startAlpha = beam->startColor[3] * fade"
	"batch->primitiveBeam = qtrue"
	"startColor[channel] = channel == 3u ? startAlpha : beam->startColor[channel]"
	"if ( batch->primitiveBeam ) params.hasLightmap |= 32u"
	"surfaceDecal" "batch->surfaceDecal ? 1u"
	"surface decals second, and translucent effects last"
	"haveView ? decalReceipt.activeCount : 0u"
	"haveView ? beamReceipt.activeCount : 0u"
	"legacy content). Vulkan renders that surface through its default"
	"primitiveEntityCount" "temporalEntityCount" "unresolvedEntityCount"
	"blendedCoefficientsQ16" "localSh[4][3]"
	"PlanWeather" "RenderSubmission_ViewSnapshot" "EncodeWeather"
	"weatherDispatchCount" "weatherDrawCount"
	"EnsureSceneColor" "MTLPixelFormatRGBA16Float"
	"EnsureToneMapPipeline" "wired_tonemap_fragment"
	"present->sceneColorCache" "RalMetal_PresentSetToneMap"
	"drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0u"
	"instanceCount:weatherPlan.activeParticleCount"
	"ral_metal_particle_compute_metallib" "ral_metal_particle_vertex_metallib"
	"ral_metal_particle_fragment_metallib" "EnsureParticlePipeline"
	"EncodeRalParticles" "particleIntegratePipeline" "particleSpawnPipeline"
	"particleAlphaPipeline" "particleAdditivePipeline"
	"particleIntegrateArguments" "particleSpawnArguments"
	"sceneDepthCache" "descriptor.usage = MTLTextureUsageShaderRead"
	"depth.usage = MTLTextureUsageRenderTarget"
	"copyFromTexture:worldDepth" "MTLLoadActionLoad"
	"setTexture:present->sceneDepthCache" "useResource:present->sceneDepthCache"
	"uiDepthDisabledState"
	"frame.depthValid = -1.0f"
	"physicalSkyReadable = present->skyResourcesReady"
	"present->skyPhysical[RAL_METAL_SKY_VIEW] != nil"
	"present->skyPhysicalSampler != nil"
	"clamp.supportArgumentBuffers = YES"
	"repeat.supportArgumentBuffers = YES"
	"RAL_PARTICLE_POOL_CAPACITY" "ralParticleClassGpu_t"
	"ralParticleSpawnGpu_t" "ralParticleFrameGpu_t")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal presentation lost native/lifecycle seam: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"ATMOSPHERE_SKY_DISTANT_SCENE"
	"RENDER_DISTANT_WORLD_BASE"
	"presentReceipt.distantWorldCandidateCount == 1u"
	"presentReceipt.distantWorldDrawCount == 1u"
	"presentReceipt.distantWorldIndexCount == 3u"
	"presentReceipt.distantWorldDepthReset == qtrue")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal distant-scene execution lost proof: ${needle}")
	endif()
endforeach()
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/particle.frag" PARTICLE_COMMON_SOURCE)
foreach(needle IN ITEMS "abs( depthValid ) < 0.5"
	"depthValid > 0.0 && sceneDepth <= 0.0"
	"depthValid < 0.0 && sceneDepth >= 1.0"
	"( gl_FragCoord.z - sceneDepth ) * sign( depthValid )")
	string(FIND "${PARTICLE_COMMON_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Common RAL particle depth convention lost fact: ${needle}")
	endif()
endforeach()
string(FIND "${SHADER_TRANSLATOR_SOURCE}" "item.symbol === 'particle_frag_spv'" particle_patch)
if(NOT particle_patch EQUAL -1)
	message(FATAL_ERROR "Particle behavior must not be patched in the backend translator")
endif()
foreach(needle IN ITEMS "0: depth unavailable; +1: reversed-Z; -1: forward-Z"
	"float depthDiff = (gl_FragCoord.z - sceneDepth) * sign(_31.depthValid);"
	"if (abs(_31.depthValid) < 0.5)"
	"sceneDepth >= 1.0")
	string(FIND "${PARTICLE_ABI_SOURCE}${PARTICLE_FRAGMENT_SOURCE}${SHADER_TRANSLATOR_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "RAL particle depth convention lost fact: ${needle}")
	endif()
endforeach()
foreach(forbidden IN ITEMS "EncodeGenericEffects" "effectSlots"
	"RAL_METAL_EFFECT_SLOT_COUNT" "RAL_METAL_EFFECT_PARTICLES_PER_SLOT"
	"setTexture:worldDepth atIndex:1u" "setDepthStencilState:nil"
	"setFragmentSamplerState:present->skyResourcesReady"
	"Metal particle diagnostic")
	string(FIND "${SOURCE}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal must not retain approximate WiredFX fallback: ${forbidden}")
	endif()
endforeach()
string(REGEX MATCHALL "SetDefaultMaterialFactors\\( &params \\)" MATERIAL_IDENTITY_CALLS
	"${SOURCE}")
list(LENGTH MATERIAL_IDENTITY_CALLS MATERIAL_IDENTITY_CALL_COUNT)
if(NOT MATERIAL_IDENTITY_CALL_COUNT EQUAL 3)
	message(FATAL_ERROR
		"Shared Metal entity, world and sky capture material paths must establish identity factors")
endif()
string(REGEX MATCHALL "!DrawEntityBatches\\( encoder" SHARED_ENTITY_CALLS "${SOURCE}")
list(LENGTH SHARED_ENTITY_CALLS SHARED_ENTITY_CALL_COUNT)
if(NOT SHARED_ENTITY_CALL_COUNT EQUAL 3)
	message(FATAL_ERROR "Preview, gameplay and the post-depth translucent pass must all use the shared entity material draw")
endif()
foreach(needle IN ITEMS "RAL_PARTICLE_POOL_CAPACITY 16384u"
	"RAL_PARTICLE_GPU_BYTES 64u" "RAL_PARTICLE_CLASS_GPU_BYTES 736u"
	"RAL_PARTICLE_SPAWN_GPU_BYTES 96u" "ralParticleClassGpu_t"
	"ralParticleGpu_t" "ralParticleSpawnGpu_t" "ralParticleFrameGpu_t")
	string(FIND "${PARTICLE_ABI_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "canonical RAL particle ABI lost fact: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"RenderSubmission_AddEffectRibbon" "AddRibbonToScene = SubmitRibbon"
	"RenderSubmission_AddEffectBeam" "AddBeamToScene = SubmitBeam"
	"RenderSubmission_AddEffectSprite" "AddSpriteToScene = SubmitSprite"
	"RenderSubmission_AddEffectEmitter" "EmitParticles = SubmitEmitter"
	"RenderSubmission_AddEffectDecal" "AddDecalToScene = SubmitDecal"
	"RenderSubmission_AddLensSource" "AddLensSourceToScene = SubmitLens"
	"RalMetal_PresentLensVisibility" "GetLensVisibility = LensVisibility"
	"RenderSubmission_RegisterParticleClass" "RegisterParticleClass = SubmitParticleClass"
	"RenderSubmission_AddLight" "AddLightToScene = SubmitLight"
	"AddLinearLightToScene = SubmitLinearLight")
	string(FIND "${MODULE_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal module lost WiredFX submission seam: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "A VFS/meta remap changes the physical image source"
	"RegisterMaterialImageSourceInternal( kind, name,"
	"identity remains the authored name")
	string(FIND "${MODULE_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal material remap lost authored logical identity: ${needle}")
	endif()
endforeach()
foreach(forbidden IN ITEMS
	"AddRibbonToScene = Noop" "AddBeamToScene = Noop"
	"AddSpriteToScene = Noop" "EmitParticles = Noop"
	"AddDecalToScene = Noop" "RegisterParticleClass = Noop"
	"AddLensSourceToScene = Noop" "GetLensVisibility = Noop"
	"AddLightToScene = Noop" "AddLinearLightToScene = Noop")
	string(FIND "${MODULE_SOURCE}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal module must not silently drop WiredFX primitives: ${forbidden}")
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
	"wired_boost_legacy_lightmap" "boosted /= peak"
	"material.localSh[1].rgb * in.normal.x"
	"material.emissiveRadiance.rgb"
	"material.directLightMeta.x" "material.directLightPositionRadius"
	"material.directLightColorFalloff" "material.directLightEndLinear"
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
foreach(needle IN ITEMS "ral_metal_product_metallib" "ral_metal_particle_metallibs"
	"particle_integrate_comp_spv.msl" "particle_vert_spv.msl"
	"particle_frag_spv.msl" "xcrun -sdk macosx metal" "cmake/embed_binary.cmake")
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
	"loweredEntityIndexCount == 69u" "loweredEntityBatchCount == 7u"
	"modelEntityCount == 1u" "primitiveEntityCount == 2u"
	"temporalEntityCount == 1u" "unresolvedEntityCount == 0u"
	"loweredEffectSpriteCount == 1u" "loweredEffectDecalCount == 1u"
	"loweredEffectRibbonCount == 1u" "loweredEffectBeamCount == 1u"
	"effectEmitterDispatchCount == 1u"
	"effectParticleDrawCount == RAL_PARTICLE_POOL_CAPACITY"
	"persistentReceipt.loweredEntityIndexCount == 12u"
	"persistentReceipt.loweredEffectDecalCount == 1u"
	"MUTATE_PRESENT( loweredEntityIndexCount )")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal presentation host lost entity/temporal coverage: ${needle}")
	endif()
endforeach()
message(STATUS "RAL Metal presentation policy: PASS")
