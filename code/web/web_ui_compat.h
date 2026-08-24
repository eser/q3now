// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_WEB_UI_COMPAT_H
#define WIRED_WEB_UI_COMPAT_H

/*
 * Expose the established UI declarations to legacy client translation units
 * while the W0 browser artifact compiles the Lua-owned implementation out.
 * FEAT_WIRED_UI is restored to zero before the translation unit body, so this
 * header does not accidentally enable any implementation block.
 */
#include "../qcommon/q_feats.h"
#undef FEAT_WIRED_UI
#define FEAT_WIRED_UI 1
#include "../client/client.h"
#include "../client/wired/ui/cl_wired_ui.h"
#include "../client/wired/ui/cl_wired_compositor.h"
#include "../client/wired/ui/cl_wired_customdraw.h"
#include "../client/wired/ui/cl_wired_attract.h"
#include "../client/wired/ui/cl_wired_text.h"
#include "../client/wired/ui/cl_wired_viewport.h"
#include "../client/wired/l10n/cl_wired_l10n.h"
#include "../client/wired/store/cl_wired_store.h"
#undef FEAT_WIRED_UI
#define FEAT_WIRED_UI 0

#endif
