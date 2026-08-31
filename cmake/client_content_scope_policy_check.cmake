# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/client/client.h" CLIENT_H)
file(READ "${ROOT}/code/client/cl_main.c" CLIENT_MAIN)
file(READ "${ROOT}/code/client/cl_parse.c" CLIENT_PARSE)
file(READ "${ROOT}/code/client/cl_cgame.c" CLIENT_CGAME)
file(READ "${ROOT}/code/client/wired/ui/cl_wired_ui.c" WIRED_UI)
file(READ "${ROOT}/code/qcommon/qcommon.h" QCOMMON_H)
file(READ "${ROOT}/code/qcommon/wired/core/logging/log.c" LOG_C)
file(READ "${ROOT}/tests/wiredui-server-browser-check.sh" BROWSER_TEST)

foreach(NEEDLE IN ITEMS "contentScopeId" "contentScopeMounted"
	"contentGameDir[MAX_QPATH]" "CL_ConfigureContentScope("
	"CL_RefreshContentScope(" "CL_ReconnectTarget("
	"pendingErrorTitle[64]" "pendingErrorMessage[512]"
	"pendingErrorRetryable")
	string(FIND "${CLIENT_H}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "client content receipt surface lost: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS "CL_DeferLoadingErrorPopup("
	"app->state <= CA_DISCONNECTED || app->state >= CA_ACTIVE"
	"CL_QueueErrorPopup( app, \"Loading Failed\""
	"CL_WiredUI_ShowError( pendingTitle[0] ? pendingTitle : \"Error\"")
	string(FIND "${CLIENT_MAIN}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "deferred loading error popup invariant lost: ${NEEDLE}")
	endif()
endforeach()

string(FIND "${LOG_C}"
	"CL_DeferLoadingErrorPopup( CL_FrameApp(), com_errorMessage )" POS)
if(POS EQUAL -1)
	message(FATAL_ERROR "TERM_CLIENT_DROP no longer preserves loading errors across UI teardown")
endif()

foreach(NEEDLE IN ITEMS "CL_AllocateContentScopeId"
	"FS_GetInstallResourcePath()" "FS_GetHomePath()"
	"gameDir[0] ? gameDir : BASEGAME"
	"FS_MountLayer( installRoot" "FS_MountLayer( homeRoot"
	"CL_UnmountContentLayers" "FS_UnmountScope( app->contentScopeId"
	"Cvar_UnsetScope( (cvarScopeId_t)app->contentScopeId )"
	"CL_ReleaseContentScope( app )" "CL_RefreshContentScope( clientActiveApp )"
	"content-scope released" "return clientActiveApp->servername")
	string(FIND "${CLIENT_MAIN}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "client content lifecycle invariant lost: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS "CL_ConfigureContentScope( app, gameDir )"
	"Cvar_GetScoped( (cvarScopeId_t)app->contentScopeId")
	string(FIND "${CLIENT_PARSE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "systeminfo content scope routing lost: ${NEEDLE}")
	endif()
endforeach()

string(FIND "${CLIENT_PARSE}" "Cvar_Set( \"fs_game\"" GLOBAL_FS_GAME_WRITE)
if(NOT GLOBAL_FS_GAME_WRITE EQUAL -1)
	message(FATAL_ERROR "server connection path must not write global fs_game")
endif()

foreach(NEEDLE IN ITEMS "Cvar_VM_RegisterScoped("
	"app->contentScopeId != FS_MOUNT_SCOPE_GLOBAL")
	string(FIND "${CLIENT_CGAME}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "cgame cvar registration is not connection-scoped: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS "reconnectTarget = CL_ReconnectTarget()")
	string(FIND "${WIRED_UI}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "WiredUI reconnect target is not connection-owned")
	endif()
endforeach()

set(LEGACY "${CLIENT_H}${CLIENT_MAIN}${CLIENT_PARSE}${CLIENT_CGAME}${WIRED_UI}${QCOMMON_H}${BROWSER_TEST}")
foreach(BANNED IN ITEMS "cl_oldGame" "CL_RestoreOldGame" "CL_ResetOldGame"
	"cl_reconnectArgs")
	string(FIND "${LEGACY}" "${BANNED}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "legacy client mod-switch bookkeeping returned: ${BANNED}")
	endif()
endforeach()

message(STATUS "Client content scope lifecycle policy: PASS")
