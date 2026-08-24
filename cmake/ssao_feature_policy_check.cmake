# Exact source contract for the current GTAO compile-time policy.

IF(NOT DEFINED SOURCE_ROOT)
	MESSAGE(FATAL_ERROR "SOURCE_ROOT is required")
ENDIF()

FILE(READ "${SOURCE_ROOT}/CMakeLists.txt" _cmake)
FILE(READ "${SOURCE_ROOT}/code/qcommon/q_feats.h" _features)

SET(_required_cmake
	"OPTION(USE_SSAO \"Compile the GTAO/SSAO renderer path (current product default ON; OFF is an explicit compiled-out permutation)\" ON)"
	"IF(USE_SSAO)"
	"ADD_COMPILE_DEFINITIONS(FEAT_SSAO=1)"
	"ELSE()"
	"ADD_COMPILE_DEFINITIONS(FEAT_SSAO=0)"
)
FOREACH(_needle IN LISTS _required_cmake)
	STRING(FIND "${_cmake}" "${_needle}" _at)
	IF(_at EQUAL -1)
		MESSAGE(FATAL_ERROR "missing SSAO build-policy token: ${_needle}")
	ENDIF()
ENDFOREACH()

SET(_feature_block "#ifndef FEAT_SSAO\n#define FEAT_SSAO                         1   // ground-truth ambient occlusion (GTAO); CMake explicitly owns ON/OFF.\n#endif")
STRING(FIND "${_features}" "${_feature_block}" _feature_at)
IF(_feature_at EQUAL -1)
	MESSAGE(FATAL_ERROR "q_feats.h FEAT_SSAO default is not override-safe")
ENDIF()

# A second unguarded definition would make the OFF permutation dishonest.
STRING(REGEX MATCHALL "#define[ \t]+FEAT_SSAO[ \t]+[01]" _defines "${_features}")
LIST(LENGTH _defines _define_count)
IF(NOT _define_count EQUAL 1)
	MESSAGE(FATAL_ERROR "expected exactly one guarded FEAT_SSAO definition; found ${_define_count}")
ENDIF()

# Configured builds additionally prove that the renderer translation unit sees
# the selected policy value.  This catches a cached option or directory-scope
# definition silently diverging from the source-level contract above.
IF(DEFINED COMPILE_COMMANDS OR DEFINED EXPECTED_SSAO)
	IF(NOT DEFINED COMPILE_COMMANDS OR NOT DEFINED EXPECTED_SSAO)
		MESSAGE(FATAL_ERROR "COMPILE_COMMANDS and EXPECTED_SSAO must be provided together")
	ENDIF()
	IF(NOT EXPECTED_SSAO MATCHES "^[01]$")
		MESSAGE(FATAL_ERROR "EXPECTED_SSAO must be 0 or 1")
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
			STRING(FIND "${_command}" "FEAT_SSAO=${EXPECTED_SSAO}" _expected_at)
			MATH(EXPR _wrong_value "1 - ${EXPECTED_SSAO}")
			STRING(FIND "${_command}" "FEAT_SSAO=${_wrong_value}" _wrong_at)
			IF(_expected_at EQUAL -1 OR NOT _wrong_at EQUAL -1)
				MESSAGE(FATAL_ERROR
					"vk.c compile policy mismatch: expected FEAT_SSAO=${EXPECTED_SSAO}, command=${_command}")
			ENDIF()
		ENDIF()
	ENDFOREACH()
	IF(_vk_count EQUAL 0)
		MESSAGE(FATAL_ERROR "compile command database has no code/render/ral/backends/vulkan/renderer/vk.c entry")
	ENDIF()
ENDIF()

MESSAGE(STATUS "SSAO feature policy contract: default ON, explicit FEAT_SSAO=1/0 branches, override-safe header, renderer commands=${_vk_count}")
