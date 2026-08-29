# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(FILES
	"${ROOT}/code/web/web_main.c"
	"${ROOT}/code/web/web_sys.c"
	"${ROOT}/code/web/web_presentation.c"
	"${ROOT}/code/web/web_transport.c"
	"${ROOT}/code/web/web_scripting.c"
	"${ROOT}/code/web/web_ui_native.c"
	"${ROOT}/code/web/web_ui_compat.h"
	"${ROOT}/code/client/wired/ui/cl_wired_parse.c"
	"${ROOT}/code/client/wired/ui/cl_wired_clay.c"
	"${ROOT}/code/web/web_authored_content.c"
	"${ROOT}/code/web/web_authored_content.h"
	"${ROOT}/tools/web-authored-compiler.mjs"
	"${ROOT}/code/web/wired_web_content.mjs"
	"${ROOT}/cmake/wired_web_content_manifest.cmake"
	"${ROOT}/code/web/wired_web_client_module.mjs"
	"${ROOT}/tests/wired_web_client_lifecycle_test.c"
	"${ROOT}/tests/wired_web_client_module_test.mjs"
	"${ROOT}/tests/wired_web_content_test.mjs"
	"${ROOT}/tests/wired_web_client_probe.html"
	"${ROOT}/tests/wired_web_client_probe.mjs")
foreach(FILE IN LISTS FILES)
	if(NOT EXISTS "${FILE}")
		message(FATAL_ERROR "missing browser-client boundary file: ${FILE}")
	endif()
	file(READ "${FILE}" TEXT)
	foreach(FORBIDDEN IN ITEMS "WebGL2" "webgl2" "SDL_" "code/renderer/" "code/renderer2/")
		string(FIND "${TEXT}" "${FORBIDDEN}" POSITION)
		if(NOT POSITION EQUAL -1)
			message(FATAL_ERROR "forbidden browser fallback/platform ownership in ${FILE}: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
file(READ "${ROOT}/code/web/wired_web_content.mjs" CONTENT)
foreach(NEEDLE IN ITEMS "manifest-download" "manifest-verified" "asset-download"
		"asset-verified" "asset-cache-invalid" "mounting" "wiredWebContentReceipt"
		"/base/gameclwasm32.wasm" "/base/gamesvwasm32.wasm")
	string(FIND "${CONTENT}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser content bootstrap misses production contract: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "file://" "/Users/" "fs_homepath")
	string(FIND "${CONTENT}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "browser content bootstrap exposes native filesystem ownership: ${FORBIDDEN}")
	endif()
endforeach()
file(READ "${ROOT}/code/web/web_ui_native.c" UI)
string(FIND "${UI}" "lua_" LUA_POSITION)
if(NOT LUA_POSITION EQUAL -1)
	message(FATAL_ERROR "browser UI boundary must not emulate raw Lua ABI")
endif()
foreach(NEEDLE IN ITEMS "WiredWebUi_ReceiveHudState" "WiredWebUi_ReceiveStoreBatch"
		"WiredWebUi_HudExtent" "WiredCrosshair_Eval" "WiredUI_MouseEvent"
		"WiredUI_KeyEvent" "WiredUI_SetLoadingMenu" "WiredUI_GetMenuStackTop"
		"WiredWebUi_RootCount" "WiredWebUi_ActivateRoot" "WiredWebUi_RootReceipt")
	string(FIND "${UI}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser native UI bridge misses contract: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/code/client/wired/ui/cl_wired_parse.c" UI_PARSE)
foreach(NEEDLE IN ITEMS "WIRED_WEB_UI_NATIVE" "WiredWebAuthored_Catalog"
		"catalog->sourceCount" "catalog->sourcePaths" "WiredUI_LoadMenuFile")
	string(FIND "${UI_PARSE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser native parser misses AOT manifest seam: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/code/client/wired/ui/cl_wired_clay.c" UI_CLAY)
foreach(NEEDLE IN ITEMS "CLAY_IMPLEMENTATION" "Clay_BeginLayout"
		"WiredUI_CompositorEmitFrame" "WiredUI_ClayFrame")
	string(FIND "${UI_CLAY}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser native Clay UI misses canonical semantic: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/code/web/web_scripting.c" SCRIPTING)
foreach(NEEDLE IN ITEMS "WiredWebAuthored_Localize" "WiredCrosshair_Eval"
		"catalog->crosshairs" "WiredStore_Get")
	string(FIND "${SCRIPTING}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser AOT scripting seam misses canonical adapter: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/CMakeLists.txt" BUILD)
foreach(NEEDLE IN ITEMS "code/web/web_ui_native.c" "-DFEAT_WIRED_UI=1"
		"-DWIRED_WEB_UI_NATIVE=1" "_WiredWeb_UiRootCount"
		"_WiredWeb_UiActivateRoot" "_WiredWeb_UiRootReceiptProbe"
		"_WiredWeb_UiServerRowCenterProbe")
	string(FIND "${BUILD}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser build misses canonical WiredUI ownership: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/cmake/wired_web_content_manifest.cmake" CONTENT_MANIFEST)
foreach(NEEDLE IN ITEMS "UI_DIR" "GLOB_RECURSE" "/base/ui/")
	string(FIND "${CONTENT_MANIFEST}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser content manifest misses authored UI asset contract: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "requiredUiFiles" "ui-")
	string(FIND "${CONTENT}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser content bootstrap misses authored UI mount contract: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/Makefile" MAKEFILE)
string(FIND "${MAKEFILE}" "-DUI_DIR=" UI_DIR_POSITION)
if(UI_DIR_POSITION EQUAL -1)
	message(FATAL_ERROR "browser content build misses authored UI directory input")
endif()
file(READ "${ROOT}/code/web/web_authored_content.c" AUTHORED)
foreach(NEEDLE IN ITEMS "WiredWebAuthored_Decode" "web/authored-content.wac"
		"WiredWebAuthored_LoadScene" "WiredWebAuthored_MarkMenuRendered"
		"WIRED_WEB_AUTHORED_MAX_ROOTS" "out->rootCount"
		"WIRED_WEB_AUTHORED_MAX_SOURCES" "out->sourceCount")
	string(FIND "${AUTHORED}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser authored-content runtime misses contract: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/tools/web-authored-compiler.mjs" AUTHORED_COMPILER)
foreach(NEEDLE IN ITEMS "--ui-dir" "--menu-manifest" "invalid authored source count"
		"duplicate authored source path" "`SOURCE|" "invalid MENU/POPUP root count"
		"duplicate authored MENU/POPUP root name" "`ROOT|")
	string(FIND "${AUTHORED_COMPILER}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser authored compiler misses bounded root inventory: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/code/web/web_main.c" MAIN)
foreach(NEEDLE IN ITEMS "Com_Init( commandLine )" "Com_Frame( qfalse )" "Com_Shutdown()"
		"RalWebGpu_BrowserModuleBorrow" "WiredWeb_PublishPresentationResolution"
		"WiredDisplay_PublishResolutionReceipt" "WiredWeb_ClientPresentationChanged"
		"WiredWeb_ContentProbe" "gameclwasm32.wasm" "gamesvwasm32.wasm"
		"maps/arena17.bsp" "1280u" "720u")
	string(FIND "${MAIN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser client main misses real orchestration: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/code/web/wired_web_client_module.mjs" MODULE)
foreach(NEEDLE IN ITEMS "_WiredWeb_InputBrowserKey" "_WiredWeb_InputPointer" "_WiredWeb_InputWheel"
		"getBoundingClientRect" "pointerdown" "wheel")
	string(FIND "${MODULE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser client module misses UI input bridge: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "requestAnimationFrame" "_WiredWeb_ClientStart" "_WiredWeb_ClientFrame" "_WiredWeb_ClientShutdown" "statusElement" "unsupported" "lost")
	string(FIND "${MODULE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser client module misses lifecycle authority: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "Wired browser client platform policy: PASS")
