// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// qcommon/wired/ui_store_types.h — Wired Store staged-entry and marker-list ABI types.
//
// cgame stages these flat PODs and hands them to the client through the
// trap_WiredStore_* syscalls (code/cgame/cg_public.h); the client's store
// (code/client/wired/store/) reads the same layout. To keep the engine free of
// cgame headers (CLAUDE.md one-way dependency rule) the types live here, like
// ui_viewport_types.h: both sides include this header, neither includes the
// other's. They cross the VM boundary, so a layout change is an ABI change.

#ifndef QCOMMON_WIRED_UI_STORE_TYPES_H
#define QCOMMON_WIRED_UI_STORE_TYPES_H

#include "../q_shared.h"

/* staged entry fields bitmask — tracks which fields were actually set */
#define WUI_STAGED_TEXT    (1 << 0)
#define WUI_STAGED_COLOR   (1 << 1)
#define WUI_STAGED_ICON    (1 << 2)
#define WUI_STAGED_VALUE   (1 << 3)
#define WUI_STAGED_STATE   (1 << 4)

typedef struct {
	char        key[128];       /* store key */
	char        text[256];      /* string value */
	vec4_t      color;          /* RGBA color */
	qhandle_t   icon;           /* shader handle */
	float       value;          /* numeric value */
	char        state[32];      /* semantic state label */
	int         fields;         /* WUI_STAGED_* bitmask — which fields are valid */
} wuiStagedEntry_t;

/* World-anchored-UI marker (WA-2a): one entry in a variable-count marker LIST
 * staged per frame under a named listKey. A flat VM-boundary POD (no pointers;
 * float/vec4/char[] only) — crosses the VM boundary, so changing it is an ABI
 * change (cgame + client rebuilt together). x/y are ABSOLUTE REAL PIXELS (the
 * CG_WorldToScreenPixels convention from WA-1) — the text element draws each
 * marker at its pixel position via Text_Draw. Separate channel from the scalar
 * wuiStagedEntry_t store (markers = arrays, store = deduped scalars). */
typedef struct {
	float       x, y;           /* absolute real-pixel screen position */
	vec4_t      color;          /* RGBA */
	char        text[24];       /* short label (e.g. a damage number) */
} wuiMarker_t;

/* Max markers in one staged list (shared cgame staging scratch + client store);
 * 64 = 32 damage plums + headroom. Overflow drops with a logged warning. */
#define WUI_MAX_MARKERS_PER_LIST    64

#endif
