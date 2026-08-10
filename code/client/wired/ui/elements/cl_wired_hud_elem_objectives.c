// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
cl_wired_hud_elem_objectives.c -- mission-objectives HUD list.

Draws the current mission objectives (set by WIRED_EVENT_OBJECTIVE from the cgame,
already localized via trap_L10n_Get) as a standing top-left list — each line is
the objective's localized text prefixed by a completed/pending indicator, with
completed objectives dimmed.

Unlike the subtitle, this is a STANDARD HUD element: registered as "hud:objectives"
WITHOUT a compositor exemption, so a cutscene that hides the HUD hides the
objectives too (only the subtitle survives HUD-suppression). It draws whenever
there are objectives and the HUD is visible; it costs nothing when the list is
empty. Same element shape as cl_wired_hud_elem_subtitle.c (create + routine
reading the shared HUD context, ctx->objectives).
*/

#include "../../../client.h"
#include "../cl_wired_ui_hud_compat.h"
#include "../cl_wired_ui_hud_private.h"
#include "../cl_wired_ui_hud_state.h"
#include "../cl_wired_text.h"

#if FEAT_WIRED_UI

/* Top-left placement + line metrics (real screen pixels; the routine anchors to
 * the viewport rather than the item rect, like the subtitle). */
#define OBJECTIVES_LEFT_FRAC   0.03f
#define OBJECTIVES_TOP_FRAC    0.20f
#define OBJECTIVES_CHAR_H      18.0f
#define OBJECTIVES_LINE_GAP    6.0f

typedef struct {
	modernhudConfig_t config;
} modernHudElementObjectives_t;

void *CG_ModernHUDElementObjectivesCreate( const modernhudConfig_t *config ) {
	modernHudElementObjectives_t *element;
	ModernHUD_ELEMENT_INIT( element, config );
	return element;
}

void CG_ModernHUDElementObjectivesRoutine( void *context ) {
	modernhudGlobalContext_t *ctx = CG_ModernHUDGetContext();
	float  px, py;
	int    i;

	(void)context;

	if ( !ctx || !wiredHud || !wiredHud_state_valid )
		return;
	if ( ctx->numObjectives <= 0 )
		return;   /* nothing to show */

	px = (float)cls.glconfig.vidWidth  * OBJECTIVES_LEFT_FRAC;
	py = (float)cls.glconfig.vidHeight * OBJECTIVES_TOP_FRAC;

	for ( i = 0; i < ctx->numObjectives && i < ModernHUD_MAX_OBJECTIVES; i++ ) {
		const char *text   = ctx->objectives[i].text;
		qboolean    done   = ctx->objectives[i].completed;
		qboolean    failed = ctx->objectives[i].failed;
		char        line[ModernHUD_MSG_MAX_LEN + 8];
		vec4_t      color;

		if ( !text[0] )
			continue;

		/* failed → dim red + a cross (takes priority — a failed mission beat);
		 * completed → dim green + a check mark; pending → bright + a bullet */
		if ( failed ) {
			Vector4Set( color, 0.85f, 0.35f, 0.35f, 0.75f );
			Com_sprintf( line, sizeof( line ), "[!] %s", text );
		} else if ( done ) {
			Vector4Set( color, 0.45f, 0.85f, 0.45f, 0.75f );
			Com_sprintf( line, sizeof( line ), "[x] %s", text );
		} else {
			Vector4Set( color, 1.0f, 1.0f, 1.0f, 1.0f );
			Com_sprintf( line, sizeof( line ), "[ ] %s", text );
		}

		Text_Draw( line, px, py, FONT_DISPLAY,
		           OBJECTIVES_CHAR_H, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );

		py += OBJECTIVES_CHAR_H + OBJECTIVES_LINE_GAP;
	}
}

#endif // FEAT_WIRED_UI
