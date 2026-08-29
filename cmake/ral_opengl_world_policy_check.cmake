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
	"MaterialEmission"
	"emissiveRadiance"
	"Ral_LightingSurfaceBindingBuild"
	"directionalStaticBound"
	"surfaceLightingBindingDigest"
	"gl.BindTextureUnit( 2u"
	"gl.BindTextureUnit( 3u"
	"gl.BindTextureUnit( 4u"
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
	"RenderSubmission_UiPrimitives")
	string(FIND "${_world}" "${_needle}" _position)
	if(_position EQUAL -1)
		message(FATAL_ERROR "OpenGL world lowering is missing exact DSA authority: ${_needle}")
	endif()
endforeach()
if(_world MATCHES "glProgramUniform")
	message(FATAL_ERROR "OpenGL world lowering bypassed the canonical product uniform block")
endif()
if(NOT _world MATCHES "gl.BindTextureUnit\\( 1u, 0u \\)")
	message(FATAL_ERROR "OpenGL world lowering does not unbind legacy lightmap under modern authority")
endif()
if(_world MATCHES "code/renderer/|code/renderer2/|renderervk")
	message(FATAL_ERROR "OpenGL world lowering reached into deprecated renderer ownership")
endif()

file(READ "${_frontend}/render_submission_effects.c" _effects)
if(_effects MATCHES "OpenGL|GL/gl|SDL_opengl|ral_opengl")
	message(FATAL_ERROR "native OpenGL ownership leaked into shared effect retention")
endif()

message(STATUS "OpenGL world/material/effects lowering policy: PASS")
