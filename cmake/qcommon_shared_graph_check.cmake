# SPDX-License-Identifier: GPL-2.0-or-later

FOREACH(_required COMPILE_COMMANDS SHARED_MANIFEST VARIANT_MANIFEST SOURCE_ROOT)
	IF(NOT DEFINED ${_required})
		MESSAGE(FATAL_ERROR "${_required} is required")
	ENDIF()
ENDFOREACH()
IF(NOT EXISTS "${COMPILE_COMMANDS}")
	MESSAGE(FATAL_ERROR "compile_commands.json is missing: ${COMPILE_COMMANDS}")
ENDIF()
IF(NOT EXISTS "${SHARED_MANIFEST}" OR NOT EXISTS "${VARIANT_MANIFEST}")
	MESSAGE(FATAL_ERROR "qcommon source manifests are missing")
ENDIF()
IF(NOT IS_DIRECTORY "${SOURCE_ROOT}")
	MESSAGE(FATAL_ERROR "SOURCE_ROOT is not a directory: ${SOURCE_ROOT}")
ENDIF()

FILE(READ "${COMPILE_COMMANDS}" _commands)
FILE(STRINGS "${SHARED_MANIFEST}" _shared_sources)
FILE(STRINGS "${VARIANT_MANIFEST}" _variant_sources)
STRING(JSON _command_count LENGTH "${_commands}")
LIST(LENGTH _shared_sources _shared_expected_count)
LIST(LENGTH _variant_sources _variant_expected_count)
IF(_command_count LESS 1 OR _shared_expected_count LESS 1)
	MESSAGE(FATAL_ERROR "compile graph or shared manifest is empty")
ENDIF()
IF(NOT _variant_expected_count EQUAL 20)
	MESSAGE(FATAL_ERROR "variant allowlist must contain exactly 20 sources, got ${_variant_expected_count}")
ENDIF()

# This is intentionally independent of the CMake router/manifest. A mistaken
# allowlist edit must not make both producer and checker agree with each other.
SET(_expected_variant_relative
	code/qcommon/common.c
	code/qcommon/crash.c
	code/qcommon/history.c
	code/qcommon/msg.c
	code/qcommon/net_ip.c
	code/qcommon/vm.c
	code/qcommon/wired/core/console/con_buffer.c
	code/qcommon/wired/core/cvars/cvar.c
	code/qcommon/wired/core/events/event.c
	code/qcommon/wired/core/logging/log.c
	code/qcommon/wired/core/logging/log_sink_console.c
	code/qcommon/wired/core/shell/cmd.c
	code/qcommon/wired/core/vfs/files.c
	code/qcommon/wired/net/wn_bg_stubs.c
	code/qcommon/wired/net/wn_inmem.c
	code/qcommon/wired/net/wn_main.c
	code/qcommon/wired/net/wn_transport.c
	code/server/sv_client.c
	code/server/sv_init.c
	code/server/sv_main.c
)
SET(_variant_relative)
SET(_variant_absolute)
FOREACH(_source IN LISTS _variant_sources)
	IF(IS_ABSOLUTE "${_source}")
		FILE(RELATIVE_PATH _relative "${SOURCE_ROOT}" "${_source}")
		SET(_absolute "${_source}")
	ELSE()
		SET(_relative "${_source}")
		SET(_absolute "${SOURCE_ROOT}/${_source}")
	ENDIF()
	FILE(TO_CMAKE_PATH "${_relative}" _relative)
	FILE(TO_CMAKE_PATH "${_absolute}" _absolute)
	IF(_relative MATCHES "^\\.\\.(/|$)")
		MESSAGE(FATAL_ERROR "variant manifest source escapes SOURCE_ROOT: ${_source}")
	ENDIF()
	LIST(APPEND _variant_relative "${_relative}")
	LIST(APPEND _variant_absolute "${_absolute}")
ENDFOREACH()
LIST(SORT _expected_variant_relative)
LIST(SORT _variant_relative)
IF(NOT _variant_relative STREQUAL _expected_variant_relative)
	MESSAGE(FATAL_ERROR "variant manifest differs from independent exact 20-source contract")
ENDIF()
SET(_variant_sources "${_variant_absolute}")

SET(_shared_seen)
SET(_client_seen)
SET(_ded_seen)
SET(_client_platform_count 0)
SET(_ded_platform_count 0)
MATH(EXPR _last "${_command_count} - 1")
FOREACH(_index RANGE 0 ${_last})
	STRING(JSON _file GET "${_commands}" ${_index} file)
	STRING(JSON _output GET "${_commands}" ${_index} output)
	STRING(JSON _command GET "${_commands}" ${_index} command)
	LIST(FIND _shared_sources "${_file}" _shared_index)
	LIST(FIND _variant_sources "${_file}" _variant_index)

	IF(_shared_index GREATER -1)
		IF(_output MATCHES "qcommon_engine_shared\\.dir/")
			IF(_command MATCHES "(^| )-DHEADLESS(=| |$)")
				MESSAGE(FATAL_ERROR "HEADLESS leaked into shared engine source: ${_file}")
			ENDIF()
			LIST(APPEND _shared_seen "${_file}")
		ELSEIF(_output MATCHES "qcommon_tool\\.dir/")
			# extract-meta deliberately owns its independent curated compilation.
		ELSEIF(_output MATCHES "qcommon(_ded)?\\.dir/")
			MESSAGE(FATAL_ERROR "shared source leaked into engine variant: ${_file} -> ${_output}")
		ENDIF()
	ELSEIF(_variant_index GREATER -1)
		IF(_output MATCHES "CMakeFiles/qcommon\\.dir/")
			IF(_command MATCHES "(^| )-DHEADLESS(=| |$)")
				MESSAGE(FATAL_ERROR "HEADLESS leaked into client variant: ${_file}")
			ENDIF()
			LIST(APPEND _client_seen "${_file}")
		ELSEIF(_output MATCHES "CMakeFiles/qcommon_ded\\.dir/")
			IF(NOT _command MATCHES "(^| )-DHEADLESS(=| |$)")
				MESSAGE(FATAL_ERROR "dedicated variant lacks HEADLESS: ${_file}")
			ENDIF()
			LIST(APPEND _ded_seen "${_file}")
		ELSEIF(_output MATCHES "qcommon_tool\\.dir/")
			# Tool compilation is outside the engine partition.
		ELSEIF(_output MATCHES "qcommon_engine_shared\\.dir/")
			MESSAGE(FATAL_ERROR "variant source leaked into shared engine target: ${_file} -> ${_output}")
		ENDIF()
	ENDIF()

	IF(_output MATCHES "CMakeFiles/wired-headless[^/]*\\.dir/")
		MATH(EXPR _ded_platform_count "${_ded_platform_count} + 1")
		IF(NOT _command MATCHES "(^| )-DHEADLESS(=| |$)")
			MESSAGE(FATAL_ERROR "HEADLESS PUBLIC propagation missing: ${_file}")
		ENDIF()
	ELSEIF(_output MATCHES "CMakeFiles/wired(\\.[^/]*)?\\.dir/")
		MATH(EXPR _client_platform_count "${_client_platform_count} + 1")
		IF(_command MATCHES "(^| )-DHEADLESS(=| |$)")
			MESSAGE(FATAL_ERROR "HEADLESS leaked into client executable source: ${_file}")
		ENDIF()
	ENDIF()
ENDFOREACH()

LIST(SORT _shared_sources)
LIST(SORT _shared_seen)
LIST(SORT _variant_sources)
LIST(SORT _client_seen)
LIST(SORT _ded_seen)
IF(NOT _shared_seen STREQUAL _shared_sources)
	MESSAGE(FATAL_ERROR "shared compile inventory mismatch")
ENDIF()
IF(NOT _client_seen STREQUAL _variant_sources OR NOT _ded_seen STREQUAL _variant_sources)
	MESSAGE(FATAL_ERROR "variant compile inventory mismatch")
ENDIF()
IF(_client_platform_count LESS 1 OR _ded_platform_count LESS 1)
	MESSAGE(FATAL_ERROR
		"final executable compile authority missing: client=${_client_platform_count} dedicated=${_ded_platform_count}")
ENDIF()

MATH(EXPR _old_engine_edges "(${_shared_expected_count} + ${_variant_expected_count}) * 2")
MATH(EXPR _new_engine_edges "${_shared_expected_count} + (${_variant_expected_count} * 2)")
MATH(EXPR _saved_engine_edges "${_old_engine_edges} - ${_new_engine_edges}")
MESSAGE(STATUS
	"qcommon partition PASS: shared=${_shared_expected_count} variant=${_variant_expected_count}; "
	"engine edges ${_old_engine_edges}->${_new_engine_edges} (saved ${_saved_engine_edges}); tool separate")
