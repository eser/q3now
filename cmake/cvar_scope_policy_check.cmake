# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/qcommon/q_shared.h" SHARED)
file(READ "${ROOT}/code/qcommon/qcommon.h" PUBLIC)
file(READ "${ROOT}/code/qcommon/wired/core/cvars/cvar.c" CVAR)
file(READ "${ROOT}/code/client/cl_main.c" CLIENT)
file(READ "${ROOT}/tests/smoke-fs-alias.sh" PROCESS_SMOKE)

foreach(NEEDLE IN ITEMS "typedef uint32_t cvarScopeId_t"
	"CVAR_SCOPE_GLOBAL" "ownerScopeId")
	string(FIND "${SHARED}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "cvar scope storage contract lost: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS "Cvar_GetScoped(" "Cvar_VM_RegisterScoped("
	"Cvar_UnsetScope(")
	string(FIND "${PUBLIC}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "public cvar scope API lost: ${NEEDLE}")
	endif()
endforeach()

foreach(NEEDLE IN ITEMS "Cvar_GetInternal" "adoptScope"
	"CVAR_USER_CREATED | CVAR_CMDLINE_CREATED"
	"already owned by scope" "curvar->ownerScopeId == scopeId"
	"Cvar_Unset( curvar )" "Cvar scope verify: PASS"
	"collision=closed" "global_preserved=1" "sharedEngineGlobal")
	string(FIND "${CVAR}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "scoped cvar lifecycle invariant lost: ${NEEDLE}")
	endif()
endforeach()

# Client disconnect must not directly invoke the process-wide cvar reset. The
# remaining legacy fs_game restore is removed by TASK-93 Step 4, after this API
# is available for its replacement teardown.
string(FIND "${CLIENT}" "Cvar_Restart(" CLIENT_RESTART_POS)
if(NOT CLIENT_RESTART_POS EQUAL -1)
	message(FATAL_ERROR "client lifecycle must not call process-global Cvar_Restart")
endif()

foreach(NEEDLE IN ITEMS "+cvar_scope_verify"
	"Cvar scope verify: PASS.*collision=closed.*global_preserved=1")
	string(FIND "${PROCESS_SMOKE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "scoped cvar process coverage lost: ${NEEDLE}")
	endif()
endforeach()

message(STATUS "Cvar scope ownership policy: PASS")
