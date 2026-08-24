if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(_backend "${ROOT}/code/render/ral/backends/opengl")
foreach(_file IN ITEMS ral_opengl_core.h ral_opengl_core.c)
	if(NOT EXISTS "${_backend}/${_file}")
		message(FATAL_ERROR "missing canonical OpenGL adapter file: ${_file}")
	endif()
endforeach()
foreach(_file IN ITEMS ral_opengl_pipeline.h ral_opengl_pipeline.c)
	if(NOT EXISTS "${_backend}/${_file}")
		message(FATAL_ERROR "missing canonical OpenGL pipeline file: ${_file}")
	endif()
endforeach()

file(READ "${_backend}/ral_opengl_core.c" _source)
foreach(_needle IN ITEMS
	"RAL_OPENGL_REQUIRED_MAJOR"
	"RAL_OPENGL_REQUIRED_MINOR"
	"GL_CONTEXT_CORE_PROFILE_BIT_VALUE"
	"glCreateBuffers"
	"glNamedBufferStorage"
	"glCreateTextures"
	"glCreateSamplers"
	"Ral_BackendConformanceBuild")
	string(FIND "${_source}" "${_needle}" _position)
	if(_position EQUAL -1)
		message(FATAL_ERROR "OpenGL core is missing fail-closed authority: ${_needle}")
	endif()
endforeach()

file(GLOB_RECURSE _portable_sources
	"${ROOT}/code/render/frontend/*.[ch]"
	"${ROOT}/code/render/ral/core/*.[ch]")
foreach(_file IN LISTS _portable_sources)
	file(READ "${_file}" _portable)
	if(_portable MATCHES "#[ \t]*include[ \t]*[<\"][^>\"]*(OpenGL/|GL/gl|SDL_opengl)")
		message(FATAL_ERROR "native OpenGL include leaked outside adapter: ${_file}")
	endif()
endforeach()

file(GLOB_RECURSE _ral_sources
	"${ROOT}/code/render/ral/core/*.[ch]"
	"${ROOT}/code/render/ral/backends/opengl/*.[ch]"
	"${ROOT}/tests/ral_*.[cmh]")
foreach(_file IN LISTS _ral_sources)
	file(READ "${_file}" _text)
	if(_text MATCHES "RAL_BACKEND_GL43|RAL_BACKEND_WEBGL2")
		message(FATAL_ERROR "retired GL43/WebGL2 backend identity remains: ${_file}")
	endif()
endforeach()

message(STATUS "OpenGL 4.6 core ownership policy: PASS")

file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/compile_xlate.mjs" _driver)
file(READ "${ROOT}/code/tools/shader_xlate/main.cpp" _translator)
if(_driver MATCHES "glsl430|glsles300" OR _translator MATCHES "glsl430|glsles300")
	message(FATAL_ERROR "retired GLSL 4.30/WebGL2 translation target remains")
endif()
