# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/render/ral/core/ral_presentation_host.h")
set(S "${ROOT}/code/render/ral/core/ral_presentation_host.c")
set(T "${ROOT}/tests/ral_presentation_host_test.c")
set(P "${ROOT}/code/render/frontend/tr_public.h")
set(E "${ROOT}/code/client/cl_main.c")
set(SDL "${ROOT}/code/sdl/sdl_glimp.c")
set(M "${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm")
set(C "${ROOT}/CMakeLists.txt")
foreach(path IN ITEMS "${H}" "${S}" "${T}" "${P}" "${E}" "${SDL}" "${M}" "${C}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing presentation-host contract file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)
file(READ "${P}" ABI)
file(READ "${E}" ENGINE)
file(READ "${SDL}" SDL_SOURCE)
file(READ "${M}" MODULE)
file(READ "${C}" CMAKE_SOURCE)

foreach(forbidden IN ITEMS "SDL_Window" "SDL_MetalView" "CAMetalLayer" "MTLDevice"
	"VkInstance" "VkSurface" "WGPUCanvas" "wgpu::")
	string(FIND "${HEADER}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "portable presentation-host header leaked native type: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"RAL_PRESENTATION_HOST_SCHEMA_VERSION 1u"
	"Ral_PresentationExtentValid"
	"ralPresentationHostReceipt_t" "ralPresentationSurfaceBorrow_t"
	"ownerGeneration" "surfaceGeneration" "ownerIdentity" "surfaceIdentity"
	"logicalWidth" "logicalHeight" "pixelWidth" "pixelHeight"
	"contentScaleX" "contentScaleY" "visible" "ready"
	"ralPresentationHostOpenFn" "ralPresentationHostRefreshFn"
	"ralPresentationHostBorrowFn" "ralPresentationHostCloseFn"
	"RAL_PRESENTATION_HOST_KEEP_OWNER" "RAL_PRESENTATION_HOST_DESTROY_OWNER")
	string(FIND "${HEADER}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "presentation-host ABI lost field/callback: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "Ral_PresentationExtentValid"
	"width > 0u && height > 0u"
	"Ral_PresentationHostReceiptValid"
	"Ral_PresentationHostReceiptExact" "Ral_PresentationSurfaceBorrowValid"
	"Ral_PresentationSurfaceBorrowExact" "Ral_PresentationHostImportsValid")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "presentation-host validation lost seam: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "Ral_PresentationExtentValid( 1280u, 720u )"
	"Ral_PresentationExtentValid( 640u, 480u )"
	"RAL_BACKEND_WEBGPU" "MUTATE_RECEIPT( ownerGeneration )"
	"MUTATE_RECEIPT( surfaceGeneration )" "MUTATE_RECEIPT( contentScaleX )"
	"MUTATE_RECEIPT( visible )" "MUTATE_BORROW( surfaceIdentity )"
	"borrowExact.surfaceIdentity = borrow.ownerIdentity"
	"imports.borrow = NULL")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "presentation-host host test lost mutation/WebGPU coverage: ${needle}")
	endif()
endforeach()
string(REGEX MATCH "#[ \t]*define[ \t]+REF_API_VERSION[ \t]+29([^0-9]|$)" api29 "${ABI}")
if(NOT api29 OR NOT ABI MATCHES "ralPresentationHostImports_t PresentationHost"
		OR NOT ABI MATCHES "PresentationChanged")
	message(FATAL_ERROR "renderer ABI lost generation-bound presentation API at generation 29")
endif()
foreach(needle IN ITEMS
	"rimp.PresentationHost.schemaVersion = RAL_PRESENTATION_HOST_SCHEMA_VERSION"
	"rimp.PresentationHost.open = RALimp_PresentationOpen"
	"rimp.PresentationHost.refresh = RALimp_PresentationRefresh"
	"rimp.PresentationHost.borrow = RALimp_PresentationBorrow"
	"rimp.PresentationHost.close = RALimp_PresentationClose")
	string(FIND "${ENGINE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "client lost presentation-host import assignment: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "WIRED_WINDOW_API_OPENGL" "WIRED_WINDOW_API_VULKAN"
	"WIRED_WINDOW_API_METAL" "SDL_WINDOW_METAL" "SDL_Metal_CreateView( SDL_window )"
	"receipt.ownerIdentity = (uintptr_t)SDL_window"
	"borrow.surfaceIdentity = (uintptr_t)layer"
	"RALimp_PresentationRefresh" "RALimp_PresentationClose")
	string(FIND "${SDL_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "SDL main-window presentation adapter lost seam: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"WIRED_WINDOW_API_OPENGL );" "WIRED_WINDOW_API_VULKAN );"
	"Ral_PresentationHostImportsValid" "RalMetal_PresentAdoptBorrowedLayer"
	"s_module.imports.PresentationHost.open" "s_module.imports.PresentationHost.refresh"
	"s_module.imports.PresentationHost.borrow" "s_module.imports.PresentationHost.close")
	string(FIND "${SDL_SOURCE}${MODULE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "platform/module presentation adoption lost seam: ${needle}")
	endif()
endforeach()
foreach(forbidden IN ITEMS "WiredMetalSdl_Create" "SDL_CreateWindow" "SDL_Metal_CreateView")
	string(FIND "${MODULE}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal module regained a private platform window: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS "ADD_EXECUTABLE(ral_presentation_host_test"
	"code/render/ral/core/ral_presentation_host.c"
	"LIST(FILTER WINDOW_SYSTEM_SRCS EXCLUDE REGEX"
	"TARGET_LINK_LIBRARIES(\${RENDERER_PREFIX}_metal\${RENDEXT} PRIVATE ral_metal")
	string(FIND "${CMAKE_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "presentation host lost narrow build seam: ${needle}")
	endif()
endforeach()
message(STATUS "RAL presentation-host policy: PASS")
