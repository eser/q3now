# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/qcommon/common.c" COMMON)
file(READ "${ROOT}/code/qcommon/wired/core/vfs/files.c" VFS)
file(READ "${ROOT}/code/server/sv_init.c" SERVER_INIT)
file(READ "${ROOT}/code/client/wired/ui/cl_wired_clay.c" WIRED_CLAY)
file(GLOB CLIENT_SOURCES "${ROOT}/code/client/*.c")
set(CLIENT "")
foreach(SOURCE IN LISTS CLIENT_SOURCES)
	file(READ "${SOURCE}" BODY)
	string(APPEND CLIENT "${BODY}")
endforeach()

foreach(NEEDLE IN ITEMS
	"FS_SetPureChecksumFeed( sv.checksumFeed )"
	"operator fs_game changes remain the only full restart authority")
	string(FIND "${SERVER_INIT}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "map transition VFS generation invariant lost: ${NEEDLE}")
	endif()
endforeach()
string(FIND "${SERVER_INIT}" "FS_Restart( sv.checksumFeed )" MAP_FS_RESTART)
if(NOT MAP_FS_RESTART EQUAL -1)
	message(FATAL_ERROR "map transition must not retire App/connection VFS scopes")
endif()
string(FIND "${WIRED_CLAY}" "if ( re.DrawMenuBackdrop )" OPTIONAL_BACKDROP_GUARD)
if(OPTIONAL_BACKDROP_GUARD EQUAL -1)
	message(FATAL_ERROR
		"map-transition loading UI must not call an optional renderer export through NULL")
endif()
file(READ "${ROOT}/code/client/wired/ui/cl_wired_ui.c" UI)

foreach(NEEDLE IN ITEMS
	"static void Com_GameRestart_f( void )"
	"Cvar_Set( \"fs_game\", Cmd_Argv( 1 ) )"
	"Com_GameRestart( 0, qtrue )"
	"Cmd_AddCommand( \"game_restart\", Com_GameRestart_f )")
	string(FIND "${COMMON}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "operator game-restart authority lost: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS
	"gamedirChanged = Q_stricmp( fs_gamedir, requestedGame ) != 0"
	"if ( gamedirChanged )"
	"Com_GameRestart( checksumFeed, clientRestart )")
	string(FIND "${VFS}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "global restart is not gated by an explicit fs_game change: ${NEEDLE}")
	endif()
endforeach()

string(FIND "${CLIENT}" "Com_GameRestart(" CLIENT_RESTART)
if(NOT CLIENT_RESTART EQUAL -1)
	message(FATAL_ERROR "client connection/runtime path regained direct Com_GameRestart authority")
endif()

foreach(NEEDLE IN ITEMS
	"Cbuf_ExecuteText( EXEC_APPEND, \"game_restart\\n\" )")
	string(FIND "${UI}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "explicit operator mod action lost: ${NEEDLE}")
	endif()
endforeach()

message(STATUS "Operator-only global game-restart policy: PASS")
