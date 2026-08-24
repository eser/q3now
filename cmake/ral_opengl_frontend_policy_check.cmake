if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(plan "${ROOT}/code/render/ral/backends/opengl/ral_opengl_frontend.c")
set(frontend "${ROOT}/code/render/frontend")
if(NOT EXISTS "${plan}")
	message(FATAL_ERROR "missing OpenGL frontend-plan owner")
endif()
file(READ "${plan}" text)
foreach(required
	RenderSubmission_ReceiptExact
	RenderSubmission_FrameDigest
	RenderSubmission_WorldSnapshot
	RenderSubmission_ModelSnapshot
	RenderSubmission_EntityCommands
	RenderSubmission_UiPrimitives
	RenderSubmission_MaterialSnapshot)
	if(NOT text MATCHES "${required}")
		message(FATAL_ERROR "OpenGL frontend plan is missing exact join: ${required}")
	endif()
endforeach()
if(text MATCHES "code/renderer2|code/renderer/|gl[A-Z][A-Za-z0-9_]*\\(")
	message(FATAL_ERROR "native/deprecated renderer ownership leaked into pure OpenGL frontend plan")
endif()

file(GLOB frontend_sources "${frontend}/*.c" "${frontend}/*.h")
foreach(source IN LISTS frontend_sources)
	file(READ "${source}" source_text)
	if(source_text MATCHES "#[ \t]*include[ \t]*[<\"](GL|OpenGL|vulkan|Metal)"
			OR source_text MATCHES "\\b(GLuint|GLenum|Vk[A-Z]|id<MTL)[A-Za-z0-9_]*")
		message(FATAL_ERROR "native graphics type/header leaked into shared frontend: ${source}")
	endif()
endforeach()
