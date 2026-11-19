// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_WEBGPU_SMOKE_FIXTURE_H
#define WIRED_WEBGPU_SMOKE_FIXTURE_H

#include "tr_public.h"

#define WIRED_WEBGPU_SMOKE_MAP "maps/webgpu-renderer-smoke.bsp"
#define WIRED_WEBGPU_SMOKE_CHECKSUM 0x11821
#define WIRED_WEBGPU_SMOKE_TEXTURE "textures/webgpu-smoke.tga"

/* Test-only file imports shared by the offline fixture cooker and WASM probe.
 * No fabricated resolve results, extension probing, runtime cooking or assets
 * from the user's game installation. Root must remain alive while in use. */
void WiredWebGpuSmokeFixture_Init( refimport_t *imports, const char *root );
unsigned WiredWebGpuSmokeFixture_CookedReads( void );

#endif
