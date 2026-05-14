// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
core.c — WiredCore layer orchestration

Single entry point for wired/core/* sub-component Init/Shutdown.
Called from Com_Init / Com_Shutdown in common.c.
*/
#include "q_shared.h"
#include "qcommon.h"
#include "core.h"

void WiredCore_Init( void ) {
    WiredCoreEvents_Init();   // first — subsystems may subscribe during their own init
    WiredScript_Init();
}

void WiredCore_Shutdown( void ) {
    WiredScript_Shutdown();
    WiredCoreEvents_Shutdown(); // last — bus stays live until all core components are down
}
