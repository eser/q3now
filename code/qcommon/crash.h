// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2017-2020 Gian 'myT' Schellenbaum (CNQ3 original)
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// Structured crash report interface (JSON + platform-native dumps).

#ifndef QCOMMON_CRASH_H
#define QCOMMON_CRASH_H

#include "q_shared.h"
#include "qcommon.h"

// Public API
void Crash_Init( void );
void Crash_WriteReport( const char *reason, const char *address, const char *module );
void Crash_InstallHandlers( void );

// Per-VM state tracking (called by vm.c when VMs load / unload).
// cgameInstance: per-app cgame slot (in-process-queue L7); 0 for game / primary cgame.
void Crash_SaveVMPointer( vmIndex_t vmIndex, int cgameInstance, vm_t *vm );
void Crash_SaveVMChecksum( vmIndex_t vmIndex, int cgameInstance, unsigned int crc32 );

// Async-signal-safe stack trace print (POSIX signal handlers).
#if !defined( _WIN32 )
void Crash_PrintVMStackTracesASS( int fd );
#endif

// sentry-native out-of-process crash capture (crash_sentry.c).
// Install returns qfalse when it declines — missing handler binary, unwritable
// home, or FEAT_SENTRY_CRASH=0 — and the caller must then fall back to the
// per-platform handler. Always linked; a stub when the feature is off, so
// callers need no #if of their own.
qboolean Crash_SentryInstall( void );
void     Crash_SentryShutdown( void );

#endif // QCOMMON_CRASH_H
