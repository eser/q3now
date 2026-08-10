// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
cl_wired_hud_elem_subtitle.c -- scene-caption subtitle draw.

Draws the localized scene-caption text (set by WIRED_EVENT_SUBTITLE from the
cgame caption consumer) near the bottom of the screen with a fade in/out.

The subtitle is a HUD element ("hud:subtitle") so it reuses the proven unified
HUD element emit/create/routine path (same as the msgqueue). BUT it is a SCENE-
presentation surface, so the compositor's HUD-suppress gate (cl_wired_clay.c)
EXEMPTS "hud:subtitle" from the sceneHudHidden hide — a cinematic cutscene hides
the HUD but the subtitle stays visible. Its own draw gates on the caption state
(an active, un-faded caption), not on the HUD visibility.

Same element shape as cl_wired_hud_elem_msgqueue.c: a create returning a per-item
context + a routine reading the shared HUD context (ctx->caption).
*/

#include "../../../client.h"
#include "../cl_wired_ui_hud_compat.h"
#include "../cl_wired_ui_hud_private.h"
#include "../cl_wired_ui_hud_state.h"
#include "../cl_wired_text.h"

#if FEAT_WIRED_UI

/* Subtitle display window (no per-caption duration is carried by the scene POD,
 * so a sensible default is used). Total on-screen time = fade-in + hold + fade-out. */
#define SUBTITLE_FADE_IN_MS   250
#define SUBTITLE_HOLD_MS      3500
#define SUBTITLE_FADE_OUT_MS  600
#define SUBTITLE_TOTAL_MS     ( SUBTITLE_FADE_IN_MS + SUBTITLE_HOLD_MS + SUBTITLE_FADE_OUT_MS )

/* Bottom placement: a fraction of the screen height down from the top (subtitle
 * convention — near the bottom, clear of the very edge and the action). */
#define SUBTITLE_BOTTOM_FRAC  0.88f
#define SUBTITLE_CHAR_H       26.0f

typedef struct {
	modernhudConfig_t config;
} modernHudElementSubtitle_t;

void *CG_ModernHUDElementSubtitleCreate( const modernhudConfig_t *config ) {
	modernHudElementSubtitle_t *element;
	ModernHUD_ELEMENT_INIT( element, config );
	return element;
}

void CG_ModernHUDElementSubtitleRoutine( void *context ) {
	modernhudGlobalContext_t *ctx = CG_ModernHUDGetContext();
	int    now, elapsed;
	float  alpha = 1.0f;
	float  px, py;
	vec4_t drawColor;

	(void)context;

	if ( !ctx || !wiredHud || !wiredHud_state_valid )
		return;
	if ( !ctx->caption.subtitle[0] )
		return;   /* no active caption */

	now     = wiredHud->time;
	elapsed = now - ctx->caption.arriveTime;
	if ( elapsed < 0 || elapsed >= SUBTITLE_TOTAL_MS )
		return;   /* faded out — stop drawing */

	/* fade in, hold, fade out */
	if ( elapsed < SUBTITLE_FADE_IN_MS ) {
		alpha = (float)elapsed / (float)SUBTITLE_FADE_IN_MS;
	} else if ( elapsed > SUBTITLE_FADE_IN_MS + SUBTITLE_HOLD_MS ) {
		alpha = 1.0f - (float)( elapsed - ( SUBTITLE_FADE_IN_MS + SUBTITLE_HOLD_MS ) )
		             / (float)SUBTITLE_FADE_OUT_MS;
	}
	if ( alpha <= 0.0f )
		return;
	if ( alpha > 1.0f )
		alpha = 1.0f;

	px = (float)cls.glconfig.vidWidth  * 0.5f;
	py = (float)cls.glconfig.vidHeight * SUBTITLE_BOTTOM_FRAC;

	Vector4Set( drawColor, 1.0f, 1.0f, 1.0f, alpha );

	/* centered, drop-shadow for legibility over any background */
	Text_Draw( ctx->caption.subtitle, px, py, FONT_DISPLAY,
	           SUBTITLE_CHAR_H, drawColor, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
}

#endif // FEAT_WIRED_UI
