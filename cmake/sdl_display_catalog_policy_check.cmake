# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(H "${ROOT}/code/client/cl_display_catalog.h")
set(C "${ROOT}/code/client/cl_display_catalog.c")
set(S "${ROOT}/code/sdl/sdl_display_catalog.c")
set(I "${ROOT}/code/sdl/sdl_input.c")
set(T "${ROOT}/tests/wired_display_catalog_test.c")
set(U "${ROOT}/code/client/wired/ui/cl_wired_populate.c")
set(M "${ROOT}/modfiles/ui/video.wui")
foreach(path IN ITEMS "${H}" "${C}" "${S}" "${I}" "${T}" "${U}" "${M}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "display catalog contract input missing: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${C}" CORE)
file(READ "${S}" SDL_PROVIDER)
file(READ "${I}" SDL_EVENTS)
file(READ "${T}" TEST)
file(READ "${U}" UI_POPULATE)
file(READ "${M}" VIDEO_MENU)

foreach(forbidden IN ITEMS "SDL_" "Vk" "WGPU" "MTL")
	string(FIND "${HEADER}" "${forbidden}" hit)
	if(NOT hit EQUAL -1)
		message(FATAL_ERROR "native-free display catalog header leaked ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"WIRED_DISPLAY_CATALOG_SCHEMA_VERSION 1u"
	"sessionHandle" "persistentKey" "wiredDisplaySelector_t"
	"refreshNumerator" "refreshDenominator"
	"densityNumerator" "densityDenominator"
	"logicalWidth" "presentationWidth" "renderWidth" "uiWidth"
	"wiredDisplayProviderSnapshotFn" "wiredDisplayEffectiveState_t")
	string(FIND "${HEADER}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "display catalog contract lost ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "wiredDisplayResolutionReceipt_t"
	"WiredDisplay_ReconcilerMark" "WiredDisplay_PresentationAction"
	"WiredDisplay_PublishResolutionReceipt")
	string(FIND "${HEADER}${CORE}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "display reconciliation contract lost ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"SDL_GetDisplays" "SDL_GetDesktopDisplayMode" "SDL_GetCurrentDisplayMode"
	"SDL_GetFullscreenDisplayModes" "SDL_GetDisplayContentScale"
	"SDL_GetDisplayForWindow" "SDL_GetPrimaryDisplay"
	"WiredDisplay_CaptureProvider" "WiredDisplay_PublishCatalog" "sdl-soft:")
	string(FIND "${SDL_PROVIDER}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "SDL display provider lost ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"SDL_EVENT_DISPLAY_ADDED" "SDL_EVENT_DISPLAY_REMOVED"
	"SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED"
	"SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED"
	"SDL_EVENT_WINDOW_DISPLAY_CHANGED"
	"SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED"
	"SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED"
	"SDL_EVENT_WINDOW_ICCPROF_CHANGED"
	"SDL_EVENT_WINDOW_HDR_STATE_CHANGED"
	"SDL_EVENT_WINDOW_ENTER_FULLSCREEN"
	"GLimp_DisplayCatalogReconcile();")
	string(FIND "${SDL_EVENTS}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "SDL display event reconciliation lost ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"WiredDisplay_BuildFallbackCatalog"
	"WiredDisplay_CaptureProvider"
	"WiredDisplay_BuildWebCurrentScreen"
	"WiredDisplay_ResolveRequest"
	"WiredDisplay_ExtentDomainsValid"
	"WiredDisplay_FormatModeValue"
	"WiredDisplay_ParseModeValue"
	"WiredDisplay_FormatRefreshValue"
	"WiredDisplay_ParseRefreshValue"
	"WiredDisplay_BuildResolutionReceipt"
	"WiredDisplay_PublishCatalog")
	string(FIND "${CORE}${TEST}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "display core/test lost ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"display_outputs" "display_modes" "display_refresh_rates"
	"WiredDisplay_GetActiveCatalog"
	"cvar \"r_output\"" "cvar \"r_outputMode\""
	"cvar \"r_windowPolicy\"" "cvar \"r_outputRefresh\""
	"populateCallback \"display_outputs\""
	"populateCallback \"display_modes\"")
	string(FIND "${UI_POPULATE}${VIDEO_MENU}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "provider-backed display UI lost ${needle}")
	endif()
endforeach()
string(FIND "${VIDEO_MENU}" "cvarFloatList { \"Native Desktop\"" stale_modes)
if(NOT stale_modes EQUAL -1)
	message(FATAL_ERROR "provider-backed display UI regressed to a static r_mode list")
endif()
message(STATUS "SDL display catalog policy: native-free provider + coalesced topology events present")
