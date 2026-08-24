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
	"${ROOT}/code/web/web_ui.c"
	"${ROOT}/code/web/web_ui_compat.h"
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
file(READ "${ROOT}/code/web/web_ui.c" UI)
string(FIND "${UI}" "lua_" LUA_POSITION)
if(NOT LUA_POSITION EQUAL -1)
	message(FATAL_ERROR "browser UI boundary must not emulate raw Lua ABI")
endif()
file(READ "${ROOT}/code/web/web_authored_content.c" AUTHORED)
foreach(NEEDLE IN ITEMS "WiredWebAuthored_Decode" "web/authored-content.wac"
		"WiredWebAuthored_LoadScene" "WiredWebAuthored_MarkMenuRendered")
	string(FIND "${AUTHORED}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser authored-content runtime misses contract: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/code/web/web_main.c" MAIN)
foreach(NEEDLE IN ITEMS "Com_Init( commandLine )" "Com_Frame( qfalse )" "Com_Shutdown()" "RalWebGpu_BrowserModuleBorrow" "WiredWeb_ContentProbe" "gameclwasm32.wasm" "gamesvwasm32.wasm" "maps/arena17.bsp" "1280u" "720u")
	string(FIND "${MAIN}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser client main misses real orchestration: ${NEEDLE}")
	endif()
endforeach()
file(READ "${ROOT}/code/web/wired_web_client_module.mjs" MODULE)
foreach(NEEDLE IN ITEMS "requestAnimationFrame" "_WiredWeb_ClientStart" "_WiredWeb_ClientFrame" "_WiredWeb_ClientShutdown" "statusElement" "unsupported" "lost")
	string(FIND "${MODULE}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "browser client module misses lifecycle authority: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "Wired browser client platform policy: PASS")
