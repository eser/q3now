# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

FOREACH(_required ARCHIVE NM_TOOL)
	IF(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
		MESSAGE(FATAL_ERROR "${_required} is required")
	ENDIF()
ENDFOREACH()
IF(NOT EXISTS "${ARCHIVE}")
	MESSAGE(FATAL_ERROR "RAL archive does not exist: ${ARCHIVE}")
ENDIF()

EXECUTE_PROCESS(
	COMMAND "${NM_TOOL}" -u "${ARCHIVE}"
	RESULT_VARIABLE _nm_rc
	OUTPUT_VARIABLE _undefined
	ERROR_VARIABLE _nm_stderr
)
IF(NOT _nm_rc EQUAL 0)
	MESSAGE(FATAL_ERROR "nm -u failed (${_nm_rc}): ${_nm_stderr}")
ENDIF()

IF(_undefined MATCHES "(^|[\\r\\n \\t])_?ri([\\r\\n \\t]|$)"
	OR _undefined MATCHES "(^|[\\r\\n \\t])_?Q_str[^\\r\\n \\t]*"
	OR _undefined MATCHES "(^|[\\r\\n \\t])_?Com_sprintf([\\r\\n \\t]|$)")
	MESSAGE(FATAL_ERROR
		"RAL archive leaked renderer/qcommon host symbols:\n${_undefined}")
ENDIF()

MESSAGE(STATUS "RAL host symbol policy PASS: no ri/Q_str*/Com_sprintf imports")
