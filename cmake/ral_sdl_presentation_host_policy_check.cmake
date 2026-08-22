# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/sdl/sdl_ral_presentation.h")
set(S "${ROOT}/code/sdl/sdl_ral_presentation.mm")
set(T "${ROOT}/tests/ral_metal_module_test.mm")
set(HT "${ROOT}/tests/ral_sdl_presentation_host_test.c")
set(C "${ROOT}/CMakeLists.txt")
foreach(path IN ITEMS "${H}" "${S}" "${T}" "${HT}" "${C}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing SDL presentation-host file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)
file(READ "${HT}" HOST_TEST)
file(READ "${C}" CMAKE_SOURCE)
foreach(forbidden IN ITEMS "SDL_Window" "SDL_MetalView" "CAMetalLayer"
	"MTLDevice" "VkInstance" "WGPUCanvas")
	string(FIND "${HEADER}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "SDL presentation public adapter leaked native type: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS "RAL_BACKEND_WEBGPU"
	"WiredSdlRalPresentationHost_Create( 1280u, 720u"
	"WiredSdlRalPresentationHost_RequestResize( host, 1600u, 900u )"
	"memcmp( &receipt, &beforeReceipt, sizeof( receipt ) ) == 0"
	"WiredSdlRalPresentationHost_RequestResize"
	"resized.surfaceGeneration > receipt.surfaceGeneration"
	"beforeBorrow.surfaceIdentity == borrow.surfaceIdentity"
	"RAL_PRESENTATION_HOST_KEEP_OWNER"
	"RAL_PRESENTATION_HOST_DESTROY_OWNER")
	string(FIND "${HOST_TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "SDL presentation host lost direct mutation coverage: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"!Ral_PresentationExtentValid( (uint32_t)logicalWidth"
	"!Ral_PresentationExtentValid( (uint32_t)pixelWidth"
	"!Ral_PresentationExtentValid( logicalWidth, logicalHeight )"
	"SDL_WINDOW_METAL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN"
	"SDL_Metal_CreateView( host->window )" "SDL_Metal_GetLayer( host->view )"
	"receipt.ownerIdentity = (uintptr_t)host->window"
	"borrow.surfaceIdentity = (uintptr_t)layer"
	"SDL_SetWindowSize" "SDL_SyncWindow"
	"Ral_PresentationHostReceiptExact( currentReceipt"
	"RAL_PRESENTATION_HOST_KEEP_OWNER"
	"RAL_PRESENTATION_HOST_DESTROY_OWNER"
	"SDL_Metal_DestroyView( host->view )"
	"SDL_DestroyWindow( host->window )")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "SDL presentation host lost lifecycle seam: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "RejectOpen" "exports->initFailed == qtrue"
	"WiredSdlRalPresentationHost_RequestResize"
	"receipt.host.logicalWidth == 800u"
	"Shutdown( REF_KEEP_WINDOW )"
	"hostAfterKeep.ownerIdentity == receipt.host.ownerIdentity"
	"Shutdown( REF_UNLOAD_DLL )"
	"!WiredSdlRalPresentationHost_GetReceipt")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "client-shaped SDL host lost fallback/resize/unload coverage: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "ADD_LIBRARY(ral_sdl_presentation_host STATIC"
	"code/sdl/sdl_ral_presentation.mm"
	"TARGET_LINK_LIBRARIES(ral_sdl_presentation_host PUBLIC ral_metal SDL3::SDL3)"
	"LIST(FILTER WINDOW_SYSTEM_SRCS EXCLUDE REGEX")
	string(FIND "${CMAKE_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "SDL presentation host lost narrow build seam: ${needle}")
	endif()
endforeach()
message(STATUS "RAL SDL presentation-host policy: PASS")
