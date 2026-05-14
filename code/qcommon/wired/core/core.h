// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
core.h — WiredCore layer umbrella header

Umbrella for wired/core subsystem headers and the Init/Shutdown
orchestration entry points used by Com_Init / Com_Shutdown.
*/
#pragma once

#include "events/event.h"

void WiredCore_Init     ( void );
void WiredCore_Shutdown ( void );
