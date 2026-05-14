// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_hud_elem_awards.c — Animated medal icon row with scale-in/out and centering.
*/

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

#define MAX_VISIBLE_AWARDS  5
#define AWARD_SCALE_MS      300     // scale in/out duration (ms)
#define AWARD_DISPLAY_MS    3000    // visible time before expire
#define AWARD_GAP           8       // pixels between medals

typedef enum {
	AWARD_PHASE_INACTIVE,
	AWARD_PHASE_SCALING_IN,     // 0 → 1 scale
	AWARD_PHASE_VISIBLE,        // fully visible, counting down
	AWARD_PHASE_SCALING_OUT     // 1 → 0 scale
} awardPhase_t;

typedef struct {
	qhandle_t       shader;
	int             phaseStartTime;
	awardPhase_t    phase;
	qboolean        soundPlayed;
} awardNotification_t;

typedef struct {
	modernhudConfig_t    config;
	awardNotification_t visible[MAX_VISIBLE_AWARDS];
	int                 lastReadIndex;
	float               iconW;
	float               iconH;
	float               anchorX;    // center X from config
	float               anchorY;    // top Y from config
} modernHudElementAwards_t;

// ── ease functions ──────────────────────────────────────────────────────

static float easeOutQuad( float t ) {
	return t * ( 2.0f - t );
}

static float easeInQuad( float t ) {
	return t * t;
}

static float clampf( float v, float lo, float hi ) {
	if ( v < lo ) return lo;
	if ( v > hi ) return hi;
	return v;
}

// ── element lifecycle ───────────────────────────────────────────────────

void *CG_ModernHUDElementAwardsCreate( const modernhudConfig_t *config ) {
	modernHudElementAwards_t *element;

	ModernHUD_ELEMENT_INIT( element, config );

	element->anchorX = config->rect.value[0];   // center X
	element->anchorY = config->rect.value[1];   // top Y
	element->iconW   = config->rect.value[2];   // icon width
	element->iconH   = config->rect.value[3];   // icon height
	element->lastReadIndex = 0;

	if ( element->iconW <= 0 ) element->iconW = 48;
	if ( element->iconH <= 0 ) element->iconH = 48;

	memset( element->visible, 0, sizeof( element->visible ) );

	return element;
}

void CG_ModernHUDElementAwardsRoutine( void *context ) {
	modernHudElementAwards_t *element = (modernHudElementAwards_t *)context;
	modernhudGlobalContext_t *gctx = CG_ModernHUDGetContext();
	int now = cg.time;
	int i;

	// ── 1. Consume new awards from queue ──────────────────────────────

	while ( element->lastReadIndex < gctx->awards.writeIndex ) {
		int qIdx = element->lastReadIndex % ModernHUD_MAX_AWARD_QUEUE;
		modernhudAwardEntry_t *entry = &gctx->awards.entries[qIdx];

		// find an inactive slot
		int slot = -1;
		for ( i = 0; i < MAX_VISIBLE_AWARDS; i++ ) {
			if ( element->visible[i].phase == AWARD_PHASE_INACTIVE ) {
				slot = i;
				break;
			}
		}

		if ( slot < 0 ) {
			// all slots full — expire the oldest visible one immediately
			int oldestTime = now + 1;
			int oldestSlot = 0;
			for ( i = 0; i < MAX_VISIBLE_AWARDS; i++ ) {
				if ( element->visible[i].phaseStartTime < oldestTime ) {
					oldestTime = element->visible[i].phaseStartTime;
					oldestSlot = i;
				}
			}
			element->visible[oldestSlot].phase = AWARD_PHASE_INACTIVE;
			slot = oldestSlot;
		}

		// populate the slot — each event becomes its own icon (no merging)
		element->visible[slot].shader = re.RegisterShaderNoMip( entry->shaderPath );
		element->visible[slot].phaseStartTime = now;
		element->visible[slot].phase = AWARD_PHASE_SCALING_IN;
		element->visible[slot].soundPlayed = qfalse;

		element->lastReadIndex++;
	}

	// ── 2. Update phases ──────────────────────────────────────────────

	for ( i = 0; i < MAX_VISIBLE_AWARDS; i++ ) {
		awardNotification_t *n = &element->visible[i];
		int elapsed;

		if ( n->phase == AWARD_PHASE_INACTIVE ) continue;

		elapsed = now - n->phaseStartTime;

		switch ( n->phase ) {
			case AWARD_PHASE_SCALING_IN:
				if ( elapsed >= AWARD_SCALE_MS ) {
					n->phase = AWARD_PHASE_VISIBLE;
					n->phaseStartTime = now;
				}
				break;

			case AWARD_PHASE_VISIBLE:
				if ( elapsed >= AWARD_DISPLAY_MS ) {
					n->phase = AWARD_PHASE_SCALING_OUT;
					n->phaseStartTime = now;
				}
				break;

			case AWARD_PHASE_SCALING_OUT:
				if ( elapsed >= AWARD_SCALE_MS ) {
					n->phase = AWARD_PHASE_INACTIVE;
				}
				break;

			default:
				break;
		}
	}

	// ── 3. Count active and compute positions ─────────────────────────

	int activeCount = 0;
	for ( i = 0; i < MAX_VISIBLE_AWARDS; i++ ) {
		if ( element->visible[i].phase != AWARD_PHASE_INACTIVE ) {
			activeCount++;
		}
	}

	if ( activeCount == 0 ) return;

	float totalWidth = activeCount * element->iconW + ( activeCount - 1 ) * AWARD_GAP;
	float startX = element->anchorX - totalWidth * 0.5f;

	// ── 4. Render ─────────────────────────────────────────────────────

	{
		int slotIdx = 0;
		vec4_t color;

		for ( i = 0; i < MAX_VISIBLE_AWARDS; i++ ) {
			awardNotification_t *n = &element->visible[i];
			float scale, alpha, drawW, drawH, drawX, drawY;
			float slotX;
			int elapsed;

			if ( n->phase == AWARD_PHASE_INACTIVE ) continue;

			elapsed = now - n->phaseStartTime;

			// compute scale and alpha
			switch ( n->phase ) {
				case AWARD_PHASE_SCALING_IN:
					scale = easeOutQuad( clampf( (float)elapsed / AWARD_SCALE_MS, 0.0f, 1.0f ) );
					alpha = scale;
					break;
				case AWARD_PHASE_VISIBLE:
					scale = 1.0f;
					alpha = 1.0f;
					break;
				case AWARD_PHASE_SCALING_OUT:
					scale = 1.0f - easeInQuad( clampf( (float)elapsed / AWARD_SCALE_MS, 0.0f, 1.0f ) );
					alpha = scale;
					break;
				default:
					scale = 0.0f;
					alpha = 0.0f;
					break;
			}

			// play sound on first frame of scale-in
			if ( !n->soundPlayed && n->phase == AWARD_PHASE_SCALING_IN ) {
				// sound is played by the old reward element via pushReward
				// we just mark it so we don't try again
				n->soundPlayed = qtrue;
			}

			// position: centered in the row
			slotX = startX + slotIdx * ( element->iconW + AWARD_GAP );

			// scale from center of slot
			drawW = element->iconW * scale;
			drawH = element->iconH * scale;
			drawX = slotX + ( element->iconW - drawW ) * 0.5f;
			drawY = element->anchorY + ( element->iconH - drawH ) * 0.5f;

			// draw medal icon
			Vector4Set( color, 1.0f, 1.0f, 1.0f, alpha );
			re.SetColor( color );

			if ( n->shader && drawW > 0.1f && drawH > 0.1f ) {
				re.DrawStretchPic( drawX, drawY, drawW, drawH, 0, 0, 1, 1, n->shader );
			}

			slotIdx++;
		}

		re.SetColor( NULL );
	}
}

#endif // FEAT_WIRED_UI
