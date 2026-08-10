// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// con_private.h — WiredConsole core-tier private state.
//
// Home of console_t (the ring-buffer data model + scroll state) and the
// compile-time geometry constants. Shared between con_buffer.c and the rest
// of the console core. This header is presentation-agnostic: it carries NO
// vec4_t render colors, NO glconfig, NO cvar render metrics — those live in
// the UI projection (code/client/wired/ui/panels/console_private.h).
//
// TURN 3 V-20 EXTRACTION: relocated the core half of console_t here from
// code/client/wired/ui/panels/console_private.h. The UI-only render fields
// (xadjust / vislines / color) moved out to a UI-local struct in
// elements/console.c; the per-line `severities[]` parallel array was added so
// the in-game console colors each logical line by its log severity rather than
// by a text bracket injected upstream.
//
// log_severity_t is provided transitively: q_shared.h (included below) pulls in
// wired/core/logging/log.h which defines the enum. log.h is headless-safe
// (a pure enum + POD structs), so this header joins the dedicated build.

#ifndef WCE_CON_PRIVATE_H
#define WCE_CON_PRIVATE_H

#include "../../../q_shared.h"   /* base types + (transitively) log_severity_t */
#include "con_public.h"          /* conViewMode_t */

/* ── Compile-time constants ─────────────────────────────────────────── */
#define  DEFAULT_CONSOLE_WIDTH  78
#define  CON_LINEBUF_SIZE       513
#define  NUM_CON_TIMES          17
#define  CON_TEXTSIZE           65536
#define  CONSOLE_ARENA_SIZE     ( sizeof(console_t) + 4096 )

/* Worst-case total lines: con.linewidth is clamped to a 40-column minimum
 * (the `width < 40 → 40` clamp in the UI reflow), so totallines never exceeds
 * CON_TEXTSIZE / 40. severities[] is a fixed worst-case array indexed
 * `line % totallines`, exactly mirroring con.text's row layout. */
#define  CON_MAX_TOTALLINES     ( CON_TEXTSIZE / 40 + 1 )

/* ── console_t — core data model + scroll state (presentation-agnostic) ──
 *
 * Render-only fields (xadjust / vislines / color) deliberately do NOT live
 * here; the UI projection keeps them in its own struct. */
typedef struct {
	qboolean	initialized;

	short		text[CON_TEXTSIZE];
	int			current;
	int			x;
	int			display;

	int			linewidth;
	int			totallines;

	float		displayFrac;
	float		finalFrac;

	/* View mode: FULL covers the screen (the attract <-> full-console pair);
	 * HALF is an upper-portion overlay leaving the underlying state (menu /
	 * game / loading) visible below. One console core, two view modes. */
	conViewMode_t	viewMode;

	int			times[NUM_CON_TIMES];

	/* Per-line log severity, parallel to text[] rows. Indexed
	 * `line % totallines` like the ring buffer. SEV_INFO is the default
	 * "no special severity" tag (renders at the console default color). */
	log_severity_t	severities[CON_MAX_TOTALLINES];

	int			viswidth;
	int			vispage;

	qboolean	newline;

	/* search state */
	qboolean	searchActive;
	char		searchPattern[256];
	int			searchCursor;
	int			searchLine;
	int			searchMatchCount;

	/* mark (selection) state */
	qboolean	markActive;
	int			markStartLine;
	int			markStartCol;
	int			markEndLine;
	int			markEndCol;

} console_t;

/* ── Backing pointer + accessor macro ───────────────────────────────── */
extern console_t *s_con;
#define con (*s_con)

#endif /* WCE_CON_PRIVATE_H */
