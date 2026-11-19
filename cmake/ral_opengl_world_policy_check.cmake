if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(_backend "${ROOT}/code/render/ral/backends/opengl")
set(_frontend "${ROOT}/code/render/frontend")
foreach(_file IN ITEMS ral_opengl_world.h ral_opengl_world.c)
	if(NOT EXISTS "${_backend}/${_file}")
		message(FATAL_ERROR "missing canonical OpenGL world lowering file: ${_file}")
	endif()
endforeach()
foreach(_file IN ITEMS render_submission_effects.h render_submission_effects.c)
	if(NOT EXISTS "${_frontend}/${_file}")
		message(FATAL_ERROR "missing backend-neutral effect retention file: ${_file}")
	endif()
endforeach()

file(READ "${_backend}/ral_opengl_world.c" _world)
foreach(_needle IN ITEMS
	"glCreateBuffers"
	"glNamedBufferStorage"
	"glNamedBufferSubData"
	"glBindBufferBase"
	"glCreateVertexArrays"
	"glVertexArrayVertexBuffer"
	"glCreateTextures"
	"glTextureStorage2D"
	"glTextureSubImage2D"
	"glBindTextureUnit"
	"glDrawElements"
	"glViewport"
	"glClearColor"
	"glClear"
	"productDrawUniforms_t"
	"UploadProductDraw"
	"ApplyWorldView"
	"ApplyAtmosphere"
	"RenderSubmission_AtmosphereSnapshot"
	"atmosphereColorVisibility"
	"sizeof( productDrawUniforms_t ) == 256u"
	"atmosphereHeightSky"
	"MaterialEmission"
	"emissiveRadiance"
	"Ral_LightingSurfaceBindingBuild"
	"directionalStaticBound"
	"surfaceLightingBindingDigest"
	"BindTextureUnit( 2u"
	"BindTextureUnit( 3u"
	"BindTextureUnit( 4u"
	"gl.BindTextureUnit( 5u, info->physicalSkyTextureName )"
	"Ral_SkySurfacePlan( &info->atmosphere.skyEnvironment"
	"skySurface.physicalBlend"
	"batch->sky"
	"blendedCoefficientsQ16"
	"localSh[4][4]"
	"offsetof( effectVertex_t, normal )"
	"GL_SHADER_STORAGE_BUFFER_VALUE"
	"atmosphereFroxelGrid"
	"info->atmosphereBufferName"
	"GL_DEPTH_TEST_VALUE"
	"RalOpenGl_FrontendPlanReceiptExact"
	"RenderSubmission_ModelSnapshot"
	"RenderSubmission_EntityCommands"
	"RenderSubmission_UiPrimitives"
	"RenderSubmission_DistantWorldSubmissionExact"
	"distant->drawPlan.selections"
	"distantWorldDrawCount"
	"gl.Clear( GL_DEPTH_BUFFER_BIT_VALUE )")
	string(FIND "${_world}" "${_needle}" _position)
	if(_position EQUAL -1)
		message(FATAL_ERROR "OpenGL world lowering is missing exact DSA authority: ${_needle}")
	endif()
endforeach()
if(_world MATCHES "glProgramUniform")
	message(FATAL_ERROR "OpenGL world lowering bypassed the canonical product uniform block")
endif()
if(NOT _world MATCHES "(gl\\.|gl->)BindTextureUnit\\( 1u, 0u \\)")
	message(FATAL_ERROR "OpenGL world lowering does not unbind legacy lightmap under modern authority")
endif()
if(_world MATCHES "code/renderer/|code/renderer2/|renderervk")
	message(FATAL_ERROR "OpenGL world lowering reached into deprecated renderer ownership")
endif()

file(READ "${_frontend}/render_submission_effects.c" _effects)
if(_effects MATCHES "OpenGL|GL/gl|SDL_opengl|ral_opengl")
	message(FATAL_ERROR "native OpenGL ownership leaked into shared effect retention")
endif()

# A backend export is a capability promise.  Until OpenGL lowers the retained
# semantic snapshots into native draws, it must fail closed so WiredFX records
# UNSUPPORTED instead of reporting EXECUTED and silently losing the primitive.
file(READ "${_backend}/ral_opengl_module.c" _module)
foreach(_needle IN ITEMS "e->EmitParticles")
	if(NOT _module MATCHES "${_needle}[^\n]*= NULL;")
		message(FATAL_ERROR "OpenGL advertises an effect primitive without native product lowering: ${_needle}")
	endif()
endforeach()
file(READ "${_backend}/ral_opengl_product.c" _product)
foreach(_needle IN ITEMS
	"= SubmitLensSource;" "= GetLensVisibility;"
	"RenderSubmission_LensSourceSnapshot" "RalOpenGl_WorldProjectLens"
	"gl.CopyTextureSubImage2D" "gl.DispatchCompute"
	"gl.GetNamedBufferSubData" "lensSlots[3]"
	"ClientWaitSync( slot->fence, 0u, 0u )"
	"slot->frame > product->lensPublishedFrame"
	"RalOpenGl_ProductResetLensVisibility"
	"lensPublishedActive")
	string(FIND "${_module}${_product}" "${_needle}" _position)
	if(_position EQUAL -1)
		message(FATAL_ERROR "OpenGL native lens depth/readback ownership lost: ${_needle}")
	endif()
endforeach()
foreach(_needle IN ITEMS
	"RenderSubmission_EffectPrimitiveSnapshots"
	"effectPrimitives.spriteCount"
	"receipt.effectSpriteDrawCount"
	"RenderEffectDecalPool_ReceiptExact"
	"receipt.effectDecalDrawCount"
	"e->AddDecalToScene" "= SubmitEffectDecal;"
	"receipt.effectRibbonDrawCount"
	"RenderEffectBeamPool_ReceiptExact"
	"receipt.effectBeamDrawCount"
	"e->AddBeamToScene" "= SubmitEffectBeam;"
	"PRIM_FLAG_NO_DEPTH_TEST"
	"e->AddSpriteToScene" "= SubmitEffectSprite;"
	"e->AddRibbonToScene" "= SubmitEffectRibbon;")
	string(FIND "${_world}${_module}" "${_needle}" _position)
	if(_position EQUAL -1)
		message(FATAL_ERROR "OpenGL native WiredFX sprite lowering lost: ${_needle}")
	endif()
endforeach()
foreach(_forbidden IN ITEMS
	"static void NoBeam" "static void AddEffectRibbon"
	"static void AddEffectSprite" "static void AddEffectEmitter"
	"static void AddEffectDecal" "static void NoLens"
	"static qboolean NoLensVisibility")
	string(FIND "${_module}" "${_forbidden}" _position)
	if(NOT _position EQUAL -1)
		message(FATAL_ERROR "OpenGL retained a submission-only/no-op WiredFX callback: ${_forbidden}")
	endif()
endforeach()

message(STATUS "OpenGL world/material/effects lowering policy: PASS")
