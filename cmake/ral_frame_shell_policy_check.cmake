# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/renderer/ral/ral_frame_shell.h")
set(S "${ROOT}/code/renderer/ral/ral_frame_shell.c")
set(T "${ROOT}/tests/ral_frame_shell_test.c")
foreach(path IN ITEMS "${H}" "${S}" "${T}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing RAL frame-shell contract file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)

foreach(needle IN ITEMS
	"RAL_FRAME_SHELL_READY" "RAL_FRAME_SHELL_RECORDING"
	"RAL_FRAME_SHELL_PRESENTED" "RAL_FRAME_SHELL_CANCELED"
	"RAL_FRAME_SHELL_SHUTDOWN" "Ral_FrameShellBegin"
	"Ral_FrameShellComplete" "Ral_FrameShellCancel"
	"Ral_FrameShellShutdown" "Ral_FrameShellReceiptExact")
	string(FIND "${HEADER}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "RAL frame shell lost portable lifecycle surface: ${needle}")
	endif()
endforeach()
foreach(backend IN ITEMS RAL_BACKEND_VULKAN RAL_BACKEND_METAL RAL_BACKEND_GL43
	RAL_BACKEND_WEBGPU RAL_BACKEND_WEBGL2)
	string(FIND "${SOURCE}" "${backend}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "RAL frame shell lost backend-neutral coverage: ${backend}")
	endif()
endforeach()
foreach(forbidden IN ITEMS "VkDevice" "MTLDevice" "SDL_Window" "WGPUDevice" "GLuint")
	string(FIND "${HEADER}${SOURCE}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "RAL frame shell leaked native backend identity: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"RAL_BACKEND_WEBGPU" "UINT64_MAX" "exact.ownerGeneration++"
	"exact.frameGeneration++" "Ral_FrameShellCancel"
	"RAL_FRAME_SHELL_SHUTDOWN" "memcmp( &ready, &before, sizeof( ready ) ) == 0")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "RAL frame-shell host lost mutation/portable coverage: ${needle}")
	endif()
endforeach()
message(STATUS "RAL frame-shell policy: PASS")
