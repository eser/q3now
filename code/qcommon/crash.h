/*
===========================================================================
Copyright (C) 2017-2020 Gian 'myT' Schellenbaum (CNQ3 original)
Copyright (C) 2026 Wired engine contributors (C port and adaptation)

This file is part of the Wired engine (derived from Quake III Arena source
code and Challenge Quake 3). It is free software released under the terms
of the GNU General Public License version 2 or (at your option) any later
version. See the file COPYING.txt for details.
===========================================================================
*/
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
void Crash_SaveVMPointer( vmIndex_t vmIndex, vm_t *vm );
void Crash_SaveVMChecksum( vmIndex_t vmIndex, unsigned int crc32 );

// Async-signal-safe stack trace print (POSIX signal handlers).
#if !defined( _WIN32 )
void Crash_PrintVMStackTracesASS( int fd );
#endif

#endif // QCOMMON_CRASH_H
