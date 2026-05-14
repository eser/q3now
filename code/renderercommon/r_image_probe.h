// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// r_image_probe.h — offline image asset availability probe.
//
// Lives in renderercommon so every renderer DLL gets the same
// implementation. Also linked standalone into offline tools
// (extract-meta) for asset auditing — the implementation only needs
// FS_*, Q_*, Com_*, and COM_StripExtension, which qcommon already
// provides; no renderer-internal types are touched.

#ifndef WIRED_RENDERERCOMMON_R_IMAGE_PROBE_H
#define WIRED_RENDERERCOMMON_R_IMAGE_PROBE_H

#include "../qcommon/q_shared.h"

// Probe whether `name` resolves to any registered image format,
// applying the same path normalization and extension fallback as
// the engine's R_FindImageFile. Does NOT load pixel data.
qboolean R_ImageResolves( const char *name );

#endif /* WIRED_RENDERERCOMMON_R_IMAGE_PROBE_H */
