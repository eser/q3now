// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// con_tty.c — WiredConsole core TTY glue.
//
// Turn 1 (V-01) scope: thin / placeholder. The existing TTY paths
// (Sys_Printf, stdin read loop, Windows console attach) live in
// code/sys/sys_win32.c + code/sys/sys_unix.c + code/win32/win_syscon.c.
// Those are already presentation-agnostic and continue to be used
// directly by the engine (Com_Printf → Sys_Print → platform TTY).
//
// This file exists so the qcommon/wired/core/console/ subdir has the
// expected three-file shape (con_buffer.c state, con_tty.c platform
// glue, con_public.h API) ahead of Turn 4 V-27 headless-mode validation,
// where the TTY-only path must be reachable without any UI symbols
// linked. If headless validation surfaces a missing glue layer that
// the existing code/sys/* paths don't cover, the wrapper functions
// land here.
//
// No symbols defined yet — Turn 4 V-27 fills the body if needed.

#include "../../../q_shared.h"
#include "con_public.h"

/* Intentionally empty in Turn 1.
 * Turn 4 V-27 (headless mode validation) will add wrappers here if the
 * existing code/sys/* TTY paths are not directly callable from WCE-tier
 * code without UI-tier dependencies. */
