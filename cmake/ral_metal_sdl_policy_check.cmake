# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/sdl/sdl_metal_ral.h")
set(S "${ROOT}/code/sdl/sdl_metal_ral.mm")
set(T "${ROOT}/tests/wired_metal_smoke.mm")
set(P "${ROOT}/code/renderer/ral_metal/ral_metal_present.mm")
set(C "${ROOT}/CMakeLists.txt")
foreach(path IN ITEMS "${H}" "${S}" "${T}" "${P}" "${C}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing SDL Metal RAL contract file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)
file(READ "${P}" PRESENT)
file(READ "${C}" CMAKE_SOURCE)

foreach(forbidden IN ITEMS "CAMetalLayer" "SDL_Window" "SDL_MetalView" "VkInstance" "VkSwapchain")
	string(FIND "${HEADER}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "SDL Metal public adapter leaked native identity: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"SDL_InitSubSystem( SDL_INIT_VIDEO )"
	"SDL_WINDOW_METAL | SDL_WINDOW_HIDDEN"
	"SDL_Metal_CreateView( candidate->window )"
	"SDL_Metal_GetLayer( candidate->view )"
	"SDL_GetWindowSizeInPixels"
	"RalMetal_PresentAdoptBorrowedLayer"
	"SDL_SetWindowSize" "SDL_SyncWindow"
	"RalMetal_PresentReconfigure"
	"RalMetal_PresentAcquire" "RalMetal_PresentClearAndSubmit")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "SDL Metal adapter lost platform/presentation seam: ${needle}")
	endif()
endforeach()
string(FIND "${SOURCE}"
	"RalMetal_PresentDestroy( adapter->present );\n\tSDL_Metal_DestroyView( adapter->view );\n\tSDL_DestroyWindow( adapter->window );"
	destroy_order)
if(destroy_order EQUAL -1)
	message(FATAL_ERROR "SDL Metal teardown lost RAL-before-view-before-window order")
endif()
string(FIND "${PRESENT}" "if ( present->ownsLayer ) [present->layer release];" borrowed_guard)
if(borrowed_guard EQUAL -1)
	message(FATAL_ERROR "borrowed CAMetalLayer can be released by RAL owner")
endif()
foreach(needle IN ITEMS
	"receipt.presentation.ownsLayer == qfalse"
	"adapter == (wiredMetalSdl_t *)(uintptr_t)0x1234u"
	"memcmp( &receipt, &before, sizeof( receipt ) ) == 0"
	"WiredMetalSdl_PresentClear" "203u, 204u" "205u, 206u"
	"WiredMetalSdl_Resize" "80u, 60u, 207u"
	"resized.presentation.layerIdentity == receipt.presentation.layerIdentity"
	"208u, 209u" "RalMetal_PresentReceiptExact")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "wired_metal smoke lost mutation/frame/resize coverage: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"ADD_LIBRARY(ral_metal_sdl STATIC code/sdl/sdl_metal_ral.mm)"
	"TARGET_LINK_LIBRARIES(ral_metal_sdl PUBLIC ral_metal SDL3::SDL3)"
	"ADD_EXECUTABLE(wired_metal_smoke tests/wired_metal_smoke.mm)"
	"TARGET_LINK_LIBRARIES(wired_metal_smoke PRIVATE ral_metal_sdl)")
	string(FIND "${CMAKE_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "wired_metal smoke lost narrow target/link seam: ${needle}")
	endif()
endforeach()
string(FIND "${CMAKE_SOURCE}" "ADD_LIBRARY(ral_metal_sdl STATIC" target_begin)
string(FIND "${CMAKE_SOURCE}" "ADD_EXECUTABLE(ral_metal_core_test" target_end)
if(target_begin EQUAL -1 OR target_end EQUAL -1 OR target_end LESS target_begin)
	message(FATAL_ERROR "cannot isolate SDL Metal target")
endif()
math(EXPR target_len "${target_end}-${target_begin}")
string(SUBSTRING "${CMAKE_SOURCE}" ${target_begin} ${target_len} target_span)
foreach(forbidden IN ITEMS "ral_vulkan" "MoltenVK" "Vulkan.framework")
	string(FIND "${target_span}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "SDL Metal target gained compatibility backend: ${forbidden}")
	endif()
endforeach()
message(STATUS "RAL SDL Metal window policy: PASS")
