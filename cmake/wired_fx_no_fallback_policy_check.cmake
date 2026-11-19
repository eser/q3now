# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT required")
endif()

file(READ "${ROOT}/code/client/cl_wired_fx.c" FX_CLIENT)
file(READ "${ROOT}/code/qcommon/wired/render/effect_profile.h" FX_CONTRACT)
file(READ "${ROOT}/code/render/frontend/tr_public.h" RENDERER_ABI)
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm" METAL_MODULE)
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_present.mm" METAL_PRESENT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_init.c" VULKAN_MODULE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_scene.c" VULKAN_SCENE)
file(READ "${ROOT}/code/render/ral/backends/opengl/ral_opengl_module.c" OPENGL_MODULE)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c" WEBGPU_MODULE)

function(require_text source needle label)
	string(FIND "${${source}}" "${needle}" position)
	if(position EQUAL -1)
		message(FATAL_ERROR "WiredFX no-fallback policy lost ${label}: ${needle}")
	endif()
endfunction()

function(forbid_text source needle label)
	string(FIND "${${source}}" "${needle}" position)
	if(NOT position EQUAL -1)
		message(FATAL_ERROR "WiredFX no-fallback policy found ${label}: ${needle}")
	endif()
endfunction()

require_text(FX_CLIENT
	"return WIRED_FX_DISPATCH_UNSUPPORTED;\n\tcase WIRED_FX_ROTATION_EXTERNAL:"
	"explicit-curve fail-closed dispatch")
require_text(FX_CLIENT
	"case WIRED_FX_ACTION_CONTROLLER_SHAKE:"
	"controller-shake dispatch boundary")
require_text(FX_CLIENT
	"different authored action, so never substitute it here."
	"controller-shake no-substitution rule")
require_text(FX_CONTRACT
	"Unsupported presentation features fail closed and are reported as unsupported;"
	"public strict dispatch contract")
require_text(RENDERER_ABI
	"A non-NULL callback is a promise that the selected backend consumes the"
	"canonical renderer capability promise")
require_text(RENDERER_ABI
	"must not\n\t// substitute a legacy, approximate, CPU-rendered, or backend-specific effect"
	"canonical no-substitution rule")
require_text(RENDERER_ABI
	"fails closed; it never replaces the GPU oracle with a CPU trace"
	"canonical lens no-fallback rule")

# Vulkan and Metal are production WiredFX consumers.  Their public callbacks
string(FIND "${VULKAN_SCENE}" "void RE_RegisterParticleClass(" particle_start)
string(FIND "${VULKAN_SCENE}" "void RE_AddLightToScene(" particle_end)
if(particle_start LESS 0 OR particle_end LESS particle_start)
	message(FATAL_ERROR "Cannot locate Vulkan particle registration boundary")
endif()
math(EXPR particle_length "${particle_end} - ${particle_start}")
string(SUBSTRING "${VULKAN_SCENE}" ${particle_start} ${particle_length} VULKAN_PARTICLE)
require_text(VULKAN_SCENE "classShader = ParticleAuthoredShader( cls->shader );"
	"Vulkan authored particle shader preflight")
require_text(VULKAN_SCENE "animation frame pool exhausted"
	"Vulkan frame pool rejection instead of static substitution")
forbid_text(VULKAN_PARTICLE "resolvedImage = tr.whiteImage;"
	"Vulkan missing class white texture substitution")
forbid_text(VULKAN_PARTICLE "fimg = tr.whiteImage;"
	"Vulkan missing frame white texture substitution")
forbid_text(VULKAN_SCENE "class %d falls back to static shader"
	"Vulkan animation degradation")
require_text(METAL_PRESENT "&texture, &sampler, &textured, &msdf ) || !textured || msdf"
	"Metal rejects unresolved particle textures")
# must reach native lowering, and Metal's module receipt must prove that
# admitted work was actually lowered/drawn rather than merely accepted.
foreach(callback IN ITEMS AddRibbonToScene AddBeamToScene AddSpriteToScene
	EmitParticles AddDecalToScene AddLensSourceToScene GetLensVisibility)
	require_text(VULKAN_MODULE "re.${callback}" "Vulkan WiredFX export")
	require_text(VULKAN_MODULE "= RE_${callback};" "Vulkan native WiredFX binding")
endforeach()
	require_text(OPENGL_MODULE "e->AddSpriteToScene" "OpenGL native sprite capability")
	require_text(OPENGL_MODULE "= SubmitEffectSprite;" "OpenGL native sprite binding")
	require_text(OPENGL_MODULE "RenderSubmission_AddEffectSprite" "OpenGL sprite submission")
	require_text(OPENGL_MODULE "e->AddRibbonToScene" "OpenGL native ribbon capability")
	require_text(OPENGL_MODULE "= SubmitEffectRibbon;" "OpenGL native ribbon binding")
require_text(OPENGL_MODULE "RenderSubmission_AddEffectRibbon" "OpenGL ribbon submission")
	require_text(OPENGL_MODULE "e->AddDecalToScene" "OpenGL native decal capability")
	require_text(OPENGL_MODULE "= SubmitEffectDecal;" "OpenGL native decal binding")
	require_text(OPENGL_MODULE "RenderSubmission_AddEffectDecal" "OpenGL decal submission")
	require_text(WEBGPU_MODULE "e->AddSpriteToScene" "WebGPU native sprite capability")
	require_text(WEBGPU_MODULE "= SubmitEffectSprite;" "WebGPU native sprite binding")
	require_text(WEBGPU_MODULE "RenderSubmission_AddEffectSprite" "WebGPU sprite submission")
	require_text(WEBGPU_MODULE "e->AddRibbonToScene" "WebGPU native ribbon capability")
	require_text(WEBGPU_MODULE "= SubmitEffectRibbon;" "WebGPU native ribbon binding")
	require_text(WEBGPU_MODULE "RenderSubmission_AddEffectRibbon" "WebGPU ribbon submission")
	require_text(WEBGPU_MODULE "e->AddDecalToScene" "WebGPU native decal capability")
	require_text(WEBGPU_MODULE "= SubmitEffectDecal;" "WebGPU native decal binding")
	require_text(WEBGPU_MODULE "RenderSubmission_AddEffectDecal" "WebGPU decal submission")
foreach(binding IN ITEMS
	"exports->AddRibbonToScene = SubmitRibbon"
	"exports->AddBeamToScene = SubmitBeam"
	"exports->AddSpriteToScene = SubmitSprite"
	"exports->EmitParticles = SubmitEmitter"
	"exports->AddDecalToScene = SubmitDecal"
	"exports->AddLensSourceToScene = SubmitLens"
	"exports->GetLensVisibility = LensVisibility")
	require_text(METAL_MODULE "${binding}" "Metal native WiredFX binding")
endforeach()
foreach(receipt IN ITEMS
	"ModuleEffectReceiptValid"
	"loweredEffectSpriteCount"
	"loweredEffectRibbonCount"
	"effectEmitterDispatchCount"
	"effectParticleDrawCount"
	"loweredEffectDecalCount"
	"loweredEffectBeamCount"
	"lensVisibilityDispatchCount")
	require_text(METAL_MODULE "${receipt}" "Metal native WiredFX receipt")
endforeach()
require_text(METAL_PRESENT "RenderEffectParticlePlan_Build"
	"Metal backend-neutral particle plan")
require_text(METAL_PRESENT "return PresentFailure( \"particle-frame-material\" );"
	"Metal authored particle frames fail closed instead of sampling fallback")
require_text(METAL_PRESENT "*outRenderedPool = writePool;"
	"Metal particle write-pool publication")
require_text(METAL_PRESENT "uint32_t renderedPool = effectParticleRenderedPool;"
	"Metal same-frame particle write-pool draw")
require_text(METAL_PRESENT "fillBuffer:present->effectPools[pool]"
	"Metal GPU-ordered timeline-reset clear")
require_text(METAL_PRESENT "if ( effectParticlePoolAdvanced )"
	"Metal publication-gated particle parity commit")
require_text(METAL_PRESENT "present->effectActiveUntil = genericEffectActiveUntil;"
	"Metal publication-gated particle lifetime commit")
require_text(METAL_PRESENT "wiredMetalParticleUploads particleUploads( present );"
	"Metal per-attempt immutable particle staging")
require_text(METAL_PRESENT "particleUploads.wired_publish( split.scene ? split.scene : command );"
	"Metal particle staging guarded by its native scene reader")
forbid_text(METAL_PRESENT "id<MTLBuffer> effectFrame;"
	"Metal singleton in-flight particle frame buffer")
forbid_text(METAL_PRESENT "id<MTLBuffer> particleClasses;"
	"Metal singleton in-flight particle class buffer")
forbid_text(METAL_PRESENT "id<MTLBuffer> particleSpawnRequests;"
	"Metal singleton in-flight particle spawn buffer")
forbid_text(METAL_PRESENT "memset( [present->effectPools[pool] contents]"
	"Metal CPU mutation of live particle pools")

# OpenGL/WebGPU are incomplete consumers today.  Keep their exports explicitly
# NULL until TASK-225.1 lands each native product path; this is strict
# capability truth, not a visual fallback.
require_text(OPENGL_MODULE "e->AddBeamToScene" "OpenGL native beam capability")
if(NOT OPENGL_MODULE MATCHES "e->AddBeamToScene[^\n]*= SubmitEffectBeam;")
	message(FATAL_ERROR "OpenGL native beam capability is not bound")
endif()
require_text(WEBGPU_MODULE "e->AddBeamToScene" "WebGPU native beam capability")
require_text(WEBGPU_MODULE "= SubmitEffectBeam;" "WebGPU native beam binding")
require_text(WEBGPU_MODULE "RenderSubmission_AddEffectBeam" "WebGPU beam submission")
foreach(callback IN ITEMS EmitParticles)
	require_text(OPENGL_MODULE "e->${callback}" "OpenGL explicit WiredFX capability")
	if(NOT OPENGL_MODULE MATCHES "e->${callback}[^\n]*= NULL;")
		message(FATAL_ERROR "OpenGL advertises ${callback} before native lowering")
	endif()
	if(callback STREQUAL "EmitParticles" AND NOT WEBGPU_MODULE MATCHES "e->${callback}[^\n]*= NULL;")
		message(FATAL_ERROR "WebGPU advertises ${callback} before native lowering")
	endif()
endforeach()
require_text(WEBGPU_MODULE "= SubmitLensSource;" "WebGPU native lens submission")
require_text(OPENGL_MODULE "= SubmitLensSource;" "OpenGL native lens submission")
require_text(OPENGL_MODULE "= GetLensVisibility;" "OpenGL native lens readback")
require_text(OPENGL_MODULE "RalOpenGl_ProductResetLensVisibility" "OpenGL scope cancellation")
require_text(WEBGPU_MODULE "= GetLensVisibility;" "WebGPU native lens readback")
require_text(WEBGPU_MODULE "RalWebGpu_BrowserBridgeBeginLensVisibility" "WebGPU native depth oracle")
require_text(WEBGPU_MODULE "RalWebGpu_BrowserBridgeResetLensVisibility" "WebGPU scope cancellation")
require_text(WEBGPU_MODULE "RalWebGpu_ProductProjection" "shared scene/lens projection")
require_text(WEBGPU_MODULE "s_module.lensQueries[i].frame > s_module.lensPublishedFrame" "out-of-order readback publication")

forbid_text(FX_CLIENT "s_fx.degradedActions++;" "runtime semantic degradation")
forbid_text(FX_CLIENT "Controller fallback remains" "controller-to-camera fallback")
forbid_text(FX_CLIENT "falls back to the captured start axis" "external-axis fallback")
forbid_text(FX_CLIENT "Preserve the authored static" "curve-to-static fallback")
forbid_text(RENDERER_ABI "falls back to its own occlusion test" "stale CPU lens fallback contract")

message(STATUS "WiredFX strict cross-backend capability/no-fallback policy passed")
