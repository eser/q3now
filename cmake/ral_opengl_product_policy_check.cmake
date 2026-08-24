if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(_backend "${ROOT}/code/render/ral/backends/opengl")
set(_smoke "${ROOT}/tests/wired_opengl46_smoke.c")
foreach(_file IN ITEMS ral_opengl_product.h ral_opengl_product.c
	ral_opengl_product_shader_catalog.inc shaders/generate_product_shader_catalog.mjs)
	if(NOT EXISTS "${_backend}/${_file}")
		message(FATAL_ERROR "missing canonical OpenGL product runtime: ${_file}")
	endif()
endforeach()
file(READ "${_backend}/ral_opengl_product.c" _product)
file(READ "${_backend}/shaders/generate_product_shader_catalog.mjs" _catalog_generator)
file(READ "${_smoke}" _smoke_source)
foreach(_needle IN ITEMS "glCreateShader" "glCreateProgram"
	"ral_opengl_product_shader_catalog.inc"
	"ralOpenGlProductVertexSource" "ralOpenGlProductFragmentSource"
	"vertexShaderDigestLane0" "fragmentShaderDigestLane1"
	"RalOpenGl_ProductSetOutputExtent" "RalOpenGl_ProductReadbackRgb"
	"RalOpenGl_FrontendPlanBuild" "RalOpenGl_WorldLower"
	"RalOpenGl_ProductFrameReceiptExact")
	string(FIND "${_product}" "${_needle}" _position)
	if(_position EQUAL -1)
		message(FATAL_ERROR "OpenGL product runtime lost exact chain: ${_needle}")
	endif()
endforeach()
foreach(_needle IN ITEMS "ral_opengl_product_vert_spv"
	"ral_opengl_product_frag_spv" "translation_catalog.json"
	"ralShaderArtifactDigest" "#version 460\\n" "--check")
	string(FIND "${_catalog_generator}" "${_needle}" _position)
	if(_position EQUAL -1)
		message(FATAL_ERROR "OpenGL product shader admission lost exact gate: ${_needle}")
	endif()
endforeach()
if(_product MATCHES "#version 460|s_vertexShader|s_fragmentShader|glProgramUniform")
	message(FATAL_ERROR "OpenGL product runtime regained backend-local shader source/uniform ABI")
endif()
foreach(_needle IN ITEMS "1280u" "720u" "arena1" "arena17"
	"loweredWorldBatchCount" "loweredModelEntityCount"
	"loweredUiPrimitiveCount" "loweredPolygonCount" "loweredLightCount")
	string(FIND "${_smoke_source}" "${_needle}" _position)
	if(_position EQUAL -1)
		message(FATAL_ERROR "OpenGL product smoke lost complete-content gate: ${_needle}")
	endif()
endforeach()
if("${_product}${_catalog_generator}${_smoke_source}" MATCHES "code/renderer/|code/renderer2/|WebGL2|glsles")
	message(FATAL_ERROR "OpenGL product runtime reached deprecated/fallback ownership")
endif()
message(STATUS "OpenGL product runtime ownership policy: PASS")
