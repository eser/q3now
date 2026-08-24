if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(_client "${ROOT}/code/client/cl_cgame.c")
if(NOT EXISTS "${_client}")
	message(FATAL_ERROR "missing client map-load source: ${_client}")
endif()
file(READ "${_client}" _source)

set(_begin_marker "static void CL_CM_LoadMap( const char *mapname ) {")
set(_end_marker "void CL_ShutdownCGame( clientApp_t *app ) {")
string(FIND "${_source}" "${_begin_marker}" _begin)
string(FIND "${_source}" "${_end_marker}" _end)
if(_begin LESS 0 OR _end LESS 0 OR _end LESS_EQUAL _begin)
	message(FATAL_ERROR "CL_CM_LoadMap ownership boundary is missing or malformed")
endif()
math(EXPR _length "${_end} - ${_begin}")
string(SUBSTRING "${_source}" ${_begin} ${_length} _body)

function(require_ordered needle after_position out_position)
	string(FIND "${_body}" "${needle}" _position)
	if(_position LESS 0)
		message(FATAL_ERROR "collision-first map-load contract lost: ${needle}")
	endif()
	if(_position LESS_EQUAL after_position)
		message(FATAL_ERROR "collision-first map-load order regressed at: ${needle}")
	endif()
	set(${out_position} ${_position} PARENT_SCOPE)
endfunction()

require_ordered("CM_LoadMap( mapname, qtrue, &checksum );" -1 _collision)
require_ordered("if ( app->cgameBsp )" ${_collision} _old_ref_release)
require_ordered("if ( !Map_Load( mapname, &bsp, MAP_LOAD_FLAGS_NONE ) )" ${_old_ref_release} _render_map)
require_ordered("app->cgameBsp = bsp;" ${_render_map} _publish)

foreach(_forbidden
		"Sys_CreateThread"
		"MAP_LOAD_FLAGS_RENDER_ONLY")
	string(FIND "${_body}" "${_forbidden}" _position)
	if(NOT _position LESS 0)
		message(FATAL_ERROR
			"collision-first/WebGPU contract forbids '${_forbidden}' in CL_CM_LoadMap")
	endif()
endforeach()

message(STATUS "BSP collision-first source policy: PASS")
