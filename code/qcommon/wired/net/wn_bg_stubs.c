// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
 * wn_bg_stubs.c — stubs for bg_*.c trap calls in the qcommon/server build context
 *
 * bg_misc.c declares and calls trap_Cvar_VariableStringBuffer under #ifdef _DEBUG.
 * In game/cgame DLL builds, g_syscalls.c / cg_syscalls.c provide the real
 * implementation.  In qcommon (dedicated server), no syscall table exists, so we
 * provide a no-op stub to satisfy the linker.
 */

/*
 * In the client binary, cl_wired_hud_compat.c already provides a real
 * trap_Cvar_VariableStringBuffer that routes to Cvar_VariableStringBuffer.
 * We only need the no-op stub in the dedicated-server binary.
 */
#ifdef DEDICATED
/* Called only inside #ifdef _DEBUG in BG_AddPredictableEventToPlayerstate. */
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	(void)var_name;
	if ( buffer && bufsize > 0 )
		buffer[0] = '\0';
}
#endif
