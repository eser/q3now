// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_hud_elem_msgqueue.c -- Priority-ordered message queue with fade-in/out.
*/

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_hud.h"
#include "cl_wired_text.h"

#if FEAT_WIRED_UI

#define MSGQ_FADE_IN_MS   300
#define MSGQ_FADE_OUT_MS  500

typedef struct {
	modernhudConfig_t config;
} modernHudElementMsgQueue_t;

void *CG_ModernHUDElementMsgQueueCreate( const modernhudConfig_t *config ) {
	modernHudElementMsgQueue_t *element;
	ModernHUD_ELEMENT_INIT( element, config );
	return element;
}

// find the highest-priority unshown entry in the queue
static int MsgQueue_FindBest( modernhudMsgQueue_t *q ) {
	int best = -1;
	modernhudMsgPriority_t bestPrio = ModernHUD_MSG_LOW;

	for ( int i = 0; i < q->writeIndex; i++ ) {
		int idx = i % ModernHUD_MSG_QUEUE_SIZE;
		modernhudMsgEntry_t *e = &q->entries[idx];
		if ( e->shown ) continue;
		if ( !e->line1[0] ) continue;
		if ( best < 0 || e->priority > bestPrio ) {
			best = i;
			bestPrio = e->priority;
		}
	}
	return best;
}

// check if a higher-priority message arrived that should preempt current
static qboolean MsgQueue_ShouldPreempt( modernhudMsgQueue_t *q ) {
	if ( q->currentIndex < 0 ) return qfalse;
	modernhudMsgEntry_t *current = &q->entries[q->currentIndex % ModernHUD_MSG_QUEUE_SIZE];

	for ( int i = q->currentIndex + 1; i < q->writeIndex; i++ ) {
		int idx = i % ModernHUD_MSG_QUEUE_SIZE;
		modernhudMsgEntry_t *e = &q->entries[idx];
		if ( e->shown ) continue;
		if ( !e->line1[0] ) continue;
		if ( e->priority > current->priority ) return qtrue;
	}
	return qfalse;
}

void CG_ModernHUDElementMsgQueueRoutine( void *context ) {
	modernHudElementMsgQueue_t *element = (modernHudElementMsgQueue_t *)context;
	modernhudGlobalContext_t *gctx = CG_ModernHUDGetContext();
	modernhudMsgQueue_t *q = &gctx->msgQueue;
	modernhudMsgEntry_t *current;
	int now = wiredHud->time;
	float alpha = 1.0f;

	if ( q->writeIndex == 0 ) return;

	// currently showing something?
	if ( q->showStartTime > 0 && q->currentIndex >= 0 ) {
		current = &q->entries[q->currentIndex % ModernHUD_MSG_QUEUE_SIZE];
		int elapsed = now - q->showStartTime;
		int displayTime = current->displayTime > 0 ? current->displayTime : 2500;

		// check preemption by higher priority
		if ( MsgQueue_ShouldPreempt( q ) ) {
			current->shown = qtrue;
			q->showStartTime = 0;
		}
		// check expiry
		else if ( elapsed >= displayTime ) {
			current->shown = qtrue;
			q->showStartTime = 0;
		}
		else {
			// fade-in during first MSGQ_FADE_IN_MS
			if ( elapsed < MSGQ_FADE_IN_MS ) {
				alpha = (float)elapsed / (float)MSGQ_FADE_IN_MS;
			}
			// fade-out during last MSGQ_FADE_OUT_MS
			else if ( elapsed > displayTime - MSGQ_FADE_OUT_MS ) {
				alpha = 1.0f - (float)( elapsed - ( displayTime - MSGQ_FADE_OUT_MS ) ) / (float)MSGQ_FADE_OUT_MS;
				if ( alpha < 0 ) alpha = 0;
			}
		}
	}

	// need to pick a new message?
	if ( q->showStartTime == 0 ) {
		int best = MsgQueue_FindBest( q );
		if ( best < 0 ) return;
		q->currentIndex = best;
		q->showStartTime = now;
		alpha = 0.0f;  // starts invisible, fades in
	}

	// render current message
	if ( q->currentIndex < 0 ) return;
	current = &q->entries[q->currentIndex % ModernHUD_MSG_QUEUE_SIZE];
	if ( !current->line1[0] ) return;
	if ( alpha <= 0.0f ) return;

	// renders two lines with independent sizes and alpha — bypasses CG_ModernHUDTextPrint intentionally
	{
		float x = element->config.rect.isSet ? element->config.rect.value[0] : 320.0f;
		float y = element->config.rect.isSet ? element->config.rect.value[1] : 95.0f;
		float charW = element->config.fontsize.isSet ? element->config.fontsize.value[0] : 10.0f;
		float charH = element->config.fontsize.isSet ? element->config.fontsize.value[1] : 14.0f;
		vec4_t color;

		Vector4Set( color, 1, 1, 1, alpha );

		// line 1: primary text (centered, proportional + shadow)
		Text_Draw( current->line1, x, y, FONT_DISPLAY,
			charH, color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );

		// line 2: secondary text (rank placement — smaller, slightly dimmer)
		if ( current->line2[0] ) {
			vec4_t color2;
			float charH2 = charH * 0.85f;

			Vector4Set( color2, 1, 1, 1, alpha * 0.85f );

			Text_Draw( current->line2, x, y + charH + 2, FONT_DISPLAY,
				charH2, color2, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
		}
	}
}

#endif // FEAT_WIRED_UI
