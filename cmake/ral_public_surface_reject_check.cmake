# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

IF(NOT DEFINED SOURCE_ROOT OR NOT DEFINED PROBE_ROOT)
	MESSAGE(FATAL_ERROR "SOURCE_ROOT and PROBE_ROOT are required")
ENDIF()

FILE(REMOVE_RECURSE "${PROBE_ROOT}")
FILE(MAKE_DIRECTORY
	"${PROBE_ROOT}/code/renderer/ral"
	"${PROBE_ROOT}/code/renderer/ral_vulkan"
	"${PROBE_ROOT}/code/renderervk"
)
FILE(COPY
	"${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_pipeline.c"
	DESTINATION "${PROBE_ROOT}/code/renderer/ral_vulkan"
)
STRING(CONCAT _retired "Ral_" "AdoptFramebuffer")
FILE(WRITE "${PROBE_ROOT}/code/renderer/ral/reintroduced.h" "void ${_retired}(void);\n")

EXECUTE_PROCESS(
	COMMAND "${CMAKE_COMMAND}"
		-DSOURCE_ROOT=${PROBE_ROOT}
		-P ${SOURCE_ROOT}/cmake/ral_public_surface_check.cmake
	RESULT_VARIABLE _rc
	OUTPUT_VARIABLE _stdout
	ERROR_VARIABLE _stderr
)
FILE(REMOVE_RECURSE "${PROBE_ROOT}")
IF(_rc EQUAL 0)
	MESSAGE(FATAL_ERROR "RAL public-surface contract accepted a reintroduced retired symbol")
ENDIF()
STRING(FIND "${_stdout}${_stderr}" "retired RAL public-surface token" _receipt)
IF(_receipt EQUAL -1)
	MESSAGE(FATAL_ERROR "RAL public-surface rejection lacked the expected receipt: ${_stdout}${_stderr}")
ENDIF()

MESSAGE(STATUS "RAL public surface mutation: retired-symbol reintroduction rejected")

FILE(REMOVE_RECURSE "${PROBE_ROOT}")
FILE(MAKE_DIRECTORY
	"${PROBE_ROOT}/code/renderer/ral"
	"${PROBE_ROOT}/code/renderer/ral_vulkan"
	"${PROBE_ROOT}/code/renderervk"
)
FILE(COPY
	"${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_pipeline.c"
	"${SOURCE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_bridge.h"
	DESTINATION "${PROBE_ROOT}/code/renderer/ral_vulkan"
)
FILE(WRITE "${PROBE_ROOT}/code/renderer/ral_vulkan/ral_vulkan_internal.h" "#include \"ral_vulkan_bridge.h\"\n")
FILE(WRITE "${PROBE_ROOT}/code/renderervk/vk_ral_textures.h" "#include \"../renderer/ral_vulkan/ral_vulkan_bridge.h\"\n")
FILE(WRITE "${PROBE_ROOT}/code/renderer/ral/native-leak.h" "void *Ral_GetBufferHandle(const ralBuffer_t *buffer);\n")

EXECUTE_PROCESS(
	COMMAND "${CMAKE_COMMAND}"
		-DSOURCE_ROOT=${PROBE_ROOT}
		-P ${SOURCE_ROOT}/cmake/ral_public_surface_check.cmake
	RESULT_VARIABLE _bridge_rc
	OUTPUT_VARIABLE _bridge_stdout
	ERROR_VARIABLE _bridge_stderr
)
FILE(REMOVE_RECURSE "${PROBE_ROOT}")
IF(_bridge_rc EQUAL 0)
	MESSAGE(FATAL_ERROR "RAL public-surface contract accepted a backend-native bridge declaration in a portable header")
ENDIF()
STRING(FIND "${_bridge_stdout}${_bridge_stderr}" "leaked into portable" _bridge_receipt)
IF(_bridge_receipt EQUAL -1)
	MESSAGE(FATAL_ERROR "RAL bridge-leak rejection lacked the expected receipt: ${_bridge_stdout}${_bridge_stderr}")
ENDIF()

MESSAGE(STATUS "RAL public surface mutation: backend-native bridge leak rejected")
