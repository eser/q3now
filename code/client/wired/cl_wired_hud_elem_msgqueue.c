/*
===========================================================================
cl_wired_hud_elem_msgqueue.c -- Unified message queue HUD element

Replaces separate fragmessage + rankmessage elements with a single
priority-ordered queue. Frag messages (HIGH) preempt center prints
(NORMAL) which preempt warmup messages (LOW).

Messages display one at a time with fade-in (300ms) and fade-out (500ms).
===========================================================================
*/

#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"
#include "cl_wired_hud.h"
#include "cl_wired_text.h"

#if FEAT_WIRED_UI

#define MSGQ_FADE_IN_MS   300
#define MSGQ_FADE_OUT_MS  500

typedef struct {
	superhudConfig_t config;
} shudElementMsgQueue_t;

void *CG_SHUDElementMsgQueueCreate( const superhudConfig_t *config ) {
	shudElementMsgQueue_t *element;
	SHUD_ELEMENT_INIT( element, config );
	return element;
}

// find the highest-priority unshown entry in the queue
static int MsgQueue_FindBest( superhudMsgQueue_t *q ) {
	int best = -1;
	superhudMsgPriority_t bestPrio = SHUD_MSG_LOW;
	int i;

	for ( i = 0; i < q->writeIndex; i++ ) {
		int idx = i % SHUD_MSG_QUEUE_SIZE;
		superhudMsgEntry_t *e = &q->entries[idx];
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
static qboolean MsgQueue_ShouldPreempt( superhudMsgQueue_t *q ) {
	int i;
	superhudMsgEntry_t *current;

	if ( q->currentIndex < 0 ) return qfalse;
	current = &q->entries[q->currentIndex % SHUD_MSG_QUEUE_SIZE];

	for ( i = q->currentIndex + 1; i < q->writeIndex; i++ ) {
		int idx = i % SHUD_MSG_QUEUE_SIZE;
		superhudMsgEntry_t *e = &q->entries[idx];
		if ( e->shown ) continue;
		if ( !e->line1[0] ) continue;
		if ( e->priority > current->priority ) return qtrue;
	}
	return qfalse;
}

void CG_SHUDElementMsgQueueRoutine( void *context ) {
	shudElementMsgQueue_t *element = (shudElementMsgQueue_t *)context;
	superhudGlobalContext_t *gctx = CG_SHUDGetContext();
	superhudMsgQueue_t *q = &gctx->msgQueue;
	superhudMsgEntry_t *current;
	int now = wiredHud->time;
	float alpha = 1.0f;

	if ( q->writeIndex == 0 ) return;

	// currently showing something?
	if ( q->showStartTime > 0 && q->currentIndex >= 0 ) {
		current = &q->entries[q->currentIndex % SHUD_MSG_QUEUE_SIZE];
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
	current = &q->entries[q->currentIndex % SHUD_MSG_QUEUE_SIZE];
	if ( !current->line1[0] ) return;
	if ( alpha <= 0.0f ) return;

	// get position from config
	{
		float x, y;
		float charW, charH;
		vec4_t color;

		x = element->config.rect.isSet ? element->config.rect.value[0] : 320.0f;
		y = element->config.rect.isSet ? element->config.rect.value[1] : 95.0f;
		charW = element->config.fontsize.isSet ? element->config.fontsize.value[0] : 10.0f;
		charH = element->config.fontsize.isSet ? element->config.fontsize.value[1] : 14.0f;

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

void CG_SHUDElementMsgQueueDestroy( void *context ) {
	if ( context ) Z_Free( context );
}

#endif // FEAT_WIRED_UI
