# Exact source contract for the current AO compile-time policy.

IF(NOT DEFINED SOURCE_ROOT)
	MESSAGE(FATAL_ERROR "SOURCE_ROOT is required")
ENDIF()

FILE(READ "${SOURCE_ROOT}/CMakeLists.txt" _cmake)
FILE(READ "${SOURCE_ROOT}/code/qcommon/q_feats.h" _features)

SET(_required_cmake
	"OPTION(USE_AO \"Compile the XeAO renderer path (current product default ON; OFF is an explicit compiled-out permutation)\" ON)"
	"IF(USE_AO)"
	"ADD_COMPILE_DEFINITIONS(FEAT_AO=1)"
	"ELSE()"
	"ADD_COMPILE_DEFINITIONS(FEAT_AO=0)"
)
FOREACH(_needle IN LISTS _required_cmake)
	STRING(FIND "${_cmake}" "${_needle}" _at)
	IF(_at EQUAL -1)
		MESSAGE(FATAL_ERROR "missing AO build-policy token: ${_needle}")
	ENDIF()
ENDFOREACH()

SET(_feature_block "#ifndef FEAT_AO\n#define FEAT_AO                           1   // ambient occlusion (XeGTAO); CMake explicitly owns ON/OFF.\n#endif")
STRING(FIND "${_features}" "${_feature_block}" _feature_at)
IF(_feature_at EQUAL -1)
	MESSAGE(FATAL_ERROR "q_feats.h FEAT_AO default is not override-safe")
ENDIF()

# A second unguarded definition would make the OFF permutation dishonest.
STRING(REGEX MATCHALL "#define[ \t]+FEAT_AO[ \t]+[01]" _defines "${_features}")
LIST(LENGTH _defines _define_count)
IF(NOT _define_count EQUAL 1)
	MESSAGE(FATAL_ERROR "expected exactly one guarded FEAT_AO definition; found ${_define_count}")
ENDIF()

# Configured builds additionally prove that the renderer translation unit sees
# the selected policy value.  This catches a cached option or directory-scope
# definition silently diverging from the source-level contract above.
IF(DEFINED COMPILE_COMMANDS OR DEFINED EXPECTED_AO)
	IF(NOT DEFINED COMPILE_COMMANDS OR NOT DEFINED EXPECTED_AO)
		MESSAGE(FATAL_ERROR "COMPILE_COMMANDS and EXPECTED_AO must be provided together")
	ENDIF()
	IF(NOT EXPECTED_AO MATCHES "^[01]$")
		MESSAGE(FATAL_ERROR "EXPECTED_AO must be 0 or 1")
	ENDIF()
	IF(NOT EXISTS "${COMPILE_COMMANDS}")
		MESSAGE(FATAL_ERROR "compile command database is missing: ${COMPILE_COMMANDS}")
	ENDIF()

	FILE(READ "${COMPILE_COMMANDS}" _commands)
	STRING(JSON _command_count LENGTH "${_commands}")
	SET(_vk_count 0)
	MATH(EXPR _command_last "${_command_count} - 1")
	FOREACH(_index RANGE 0 ${_command_last})
		STRING(JSON _file GET "${_commands}" ${_index} file)
		FILE(TO_CMAKE_PATH "${_file}" _file)
		IF(_file MATCHES "/code/render/ral/backends/vulkan/renderer/vk\\.c$")
			MATH(EXPR _vk_count "${_vk_count} + 1")
			STRING(JSON _command GET "${_commands}" ${_index} command)
			STRING(FIND "${_command}" "FEAT_AO=${EXPECTED_AO}" _expected_at)
			MATH(EXPR _wrong_value "1 - ${EXPECTED_AO}")
			STRING(FIND "${_command}" "FEAT_AO=${_wrong_value}" _wrong_at)
			IF(_expected_at EQUAL -1 OR NOT _wrong_at EQUAL -1)
				MESSAGE(FATAL_ERROR
					"vk.c compile policy mismatch: expected FEAT_AO=${EXPECTED_AO}, command=${_command}")
			ENDIF()
		ENDIF()
	ENDFOREACH()
	IF(_vk_count EQUAL 0)
		MESSAGE(FATAL_ERROR "compile command database has no code/render/ral/backends/vulkan/renderer/vk.c entry")
	ENDIF()
ENDIF()

MESSAGE(STATUS "AO feature policy contract: default ON, explicit FEAT_AO=1/0 branches, override-safe header, renderer commands=${_vk_count}")
