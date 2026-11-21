// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* Engine glue for WiredTasks: the one place the module meets the engine. It owns
 * the com_taskWorkers cvar and the engine-wide scheduler, and injects the zone
 * allocator and the clock. Engine code asks WiredTasks_Get() for the scheduler;
 * a NULL result is valid and means "run inline", every wired_tasks_* entry point
 * accepts it. */

#ifndef WIRED_TASKS_ENGINE_H
#define WIRED_TASKS_ENGINE_H

#include "wired_tasks.h"

#ifdef __cplusplus
extern "C" {
#endif

void           WiredTasks_Register( void );
wired_tasks_t *WiredTasks_Get( void );       /* main thread only; creates on first use */
void           WiredTasks_Shutdown( void );

#ifdef __cplusplus
}
#endif

#endif
