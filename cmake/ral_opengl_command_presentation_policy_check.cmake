if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(backend "${ROOT}/code/render/ral/backends/opengl")
set(platform "${ROOT}/code/sdl/sdl_opengl46_ral.c")
foreach(required
	"${backend}/ral_opengl_command.c"
	"${backend}/ral_opengl_command.h"
	"${platform}")
	if(NOT EXISTS "${required}")
		message(FATAL_ERROR "missing canonical OpenGL 4.6 owner: ${required}")
	endif()
endforeach()

file(GLOB backend_sources "${backend}/*.c" "${backend}/*.h")
foreach(source IN LISTS backend_sources)
	file(READ "${source}" text)
	if(text MATCHES "#[ \t]*include[ \t]*[<\"](GL|OpenGL|SDL)")
		message(FATAL_ERROR "native/platform header leaked into OpenGL backend contract: ${source}")
	endif()
endforeach()

file(READ "${backend}/ral_opengl_command.c" command)
foreach(symbol glMemoryBarrier glFenceSync glClientWaitSync glDeleteSync
	Ral_CommandLifecyclePublishBegin Ral_SubmissionLifecyclePublish
	Ral_CommandLifecycleRecycle)
	if(NOT command MATCHES "${symbol}")
		message(FATAL_ERROR "OpenGL command lifecycle is missing ${symbol}")
	endif()
endforeach()

file(READ "${platform}" presentation)
foreach(token
	"SDL_GL_CONTEXT_MAJOR_VERSION, 4"
	"SDL_GL_CONTEXT_MINOR_VERSION, 6"
	"SDL_GL_CONTEXT_PROFILE_CORE"
	"actualMajor != 4"
	"actualMinor < 6"
	"RalOpenGl_CoreDestroy"
	"SDL_GL_DestroyContext"
	"SDL_DestroyWindow")
	if(NOT presentation MATCHES "${token}")
		message(FATAL_ERROR "OpenGL 4.6 presentation contract is missing: ${token}")
	endif()
endforeach()
if(presentation MATCHES "WebGL|code/renderer2|code/renderer/")
	message(FATAL_ERROR "deprecated/fallback renderer authority leaked into OpenGL 4.6 adapter")
endif()

string(FIND "${presentation}" "RalOpenGl_CoreDestroy" core_destroy)
string(FIND "${presentation}" "SDL_GL_DestroyContext" context_destroy)
string(FIND "${presentation}" "SDL_DestroyWindow" window_destroy)
if(core_destroy LESS 0 OR context_destroy LESS 0 OR window_destroy LESS 0)
	message(FATAL_ERROR "OpenGL teardown symbols are missing")
endif()
if(core_destroy GREATER_EQUAL context_destroy OR context_destroy GREATER_EQUAL window_destroy)
	message(FATAL_ERROR "OpenGL teardown must remain child-before-parent: RAL core, context, window")
endif()
