// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_ui_hud_state.c — Wired UI HUD: state + receivers + tick.
*
* TRANSITION SHIM
* =================================
* Relocated from code/client/wired/hud/cl_wired_hud.c during the
* WiredUI Render Frame Unification workstream. Function names retain the
* WiredHud_* prefix because:
*   - cl_cgame.c (engine trap handler) still calls WiredHud_ReceiveEvent /
*     WiredHud_ReceiveState by name (a later change reroutes via WiredStore push)
*   - element files at code/client/wired/ui/elements/cl_wired_hud_elem_*.c
*     (relocated 2026-05-31 from hud/elements/) still call the internal
*     helpers below (shim-retire deferred [[wiredui-hud-element-relocation]])
* When the deferred element relocation lands, this file becomes part of the
* hud_state element family proper and the WiredHud_* names retire.
*/

#include "../../client.h"
#include "cl_wired_ui_hud_state.h"
#include "cl_wired_ui.h"
#include "cl_wired_text.h"
#include "cl_wired_ui_hud_private.h"
#include "cl_wired_ui_hud_compat.h"
#include "../store/cl_wired_store.h"

/* Ring depth for the WiredStore-exposed chat history. Caps at
 * `ModernHUD_MAX_CHAT_LINES` (which is also the legacy struct field's
 * sizing); modders consume via `repeat { source "hud.chat" countbind
 * "hud.chat.count" as "row" … }` in default.wui. */
#define WIRED_HUD_CHAT_RING_MAX  ModernHUD_MAX_CHAT_LINES
/* Kill-feed ring depth (mockup shows ~5 recent obituary rows).
 * Exposed as hud.feed.<N>.text + hud.feed.count, mirroring hud.chat.*. */
#define WIRED_HUD_FEED_RING_MAX  5
LOG_DECLARE_CHANNEL( ch_ui, "ui" );

// from cl_wired_hud_compat.c
extern void WiredHud_SyncCompat( void );


// from cl_wired_hud_registry.c
extern qboolean WiredHud_CreateElement( const char *name, const modernhudConfig_t *config );
extern void     WiredHud_DestroyAllElements( void );
/* WiredHud_RenderElements retired — compositor unified-registry
 * dispatch is sole HUD element renderer. */
extern int      WiredHud_GetElementCount( void );
extern void     WiredHud_ShutdownArena( void );  // Arena_Destroy hook

// from cl_wired_parse.c
extern wiredMenuDef_t *WiredUI_GetMenuByIndex( int index );
extern void WiredHud_DrawScorelistWidget( float ox, float oy, float ow, float oh,
	int feederID, const vec4_t textColor );
extern void WiredHud_DrawDuelBoard( float ox, float oy, float ow, float oh );

#if FEAT_WIRED_UI

// ── global HUD state ─────────────────────────────────────────────────

wiredHudState_t  wired_hudStateStorage;
wiredHudState_t *wiredHud = &wired_hudStateStorage;
/* engine-tier fail-fast flag. Mirrors wired_hudStateStorage
 * .valid for the duration of the transition cycle so external callers
 * (cl_scrn.c, cl_wired_clay.c HUD-layer activation predicate) read a stable
 * public symbol while internal helpers retain the .valid member access. */
qboolean         wiredHud_state_valid    = qfalse;
static qboolean  wiredHud_elementsLoaded = qfalse;
static cvar_t   *cl_drawHud             = NULL;

/* Chat-line time-based expiry (2026-06-10): restored.
 *
 * The retired element cl_wired_hud_elem_chat.c expired/faded each line via
 * WHUD_FADE_AND_PRINT against the legacy hud_default.hud chat config
 * (`time 8000  fadedelay 2000  fade 1 1 1 0`): a line shows solid for
 * WIRED_HUD_CHAT_TIME ms, then fades out over WIRED_HUD_CHAT_FADEDELAY ms,
 * then disappears. The relocated path publishes lines into the WiredStore
 * (`hud.chat.*`) which had no per-line lifecycle, so stale lines persisted
 * forever (until shifted out of the ring by new chat).
 *
 * EXPIRY is restored here: each ring slot carries its arrival time
 * (wiredHud->time clock, matching the rest of this file) and WiredHud_
 * ChatExpire() — driven from WiredUI_HudTick — drops fully-expired lines
 * and re-publishes the store every frame. The ring is lifted to file scope
 * so both the receive path (insert) and the tick (expire) share it.
 *
 * FADE is NOT restorable purely here: the compositor's repeat-row Mustache
 * path (cl_wired_clay.c::wui_clay_substitute_row_text) substitutes only the
 * store entry's `.text` into string fields, and default.wui's chat row hard-
 * codes `forecolor 1 1 1 0.7` with no per-row colour/alpha bind. A fade
 * alpha written into the store entry's colour would be ignored. Restoring
 * the alpha ramp needs a companion edit in those (out-of-lane) files. */
#define WIRED_HUD_CHAT_TIME        8000   /* solid display window (ms)   */
#define WIRED_HUD_CHAT_FADEDELAY   2000   /* fade-out window (ms)        */
#define WIRED_HUD_CHAT_LIFETIME    ( WIRED_HUD_CHAT_TIME + WIRED_HUD_CHAT_FADEDELAY )

static char wired_hudChatRing[ WIRED_HUD_CHAT_RING_MAX ][ MAX_SAY_TEXT ];
static int  wired_hudChatTime[ WIRED_HUD_CHAT_RING_MAX ];  /* arrival time (wiredHud->time) */
static int  wired_hudChatCount = 0;

/* Re-publish the live chat ring into the WiredStore `hud.chat.*` prefix.
 * Writes hud.chat.<i>.text for every live row + hud.chat.count. */
static void WiredHud_ChatPublish( void ) {
	wuiStoreEntry_t *e;
	int              i;

	for ( i = 0; i < wired_hudChatCount; i++ ) {
		char key[ 64 ];
		Com_sprintf( key, sizeof( key ), "hud.chat.%d.text", i );
		e = WiredStore_Set( key );
		if ( e ) Q_strncpyz( e->text, wired_hudChatRing[ i ], sizeof( e->text ) );
	}
	/* Clear every stale slot's text so a modder iterating past
	 * hud.chat.count (defensively) reads empty rows, not ghosts of
	 * expired lines. */
	for ( i = wired_hudChatCount; i < WIRED_HUD_CHAT_RING_MAX; i++ ) {
		char key[ 64 ];
		Com_sprintf( key, sizeof( key ), "hud.chat.%d.text", i );
		e = WiredStore_Set( key );
		if ( e ) e->text[ 0 ] = '\0';
	}
	e = WiredStore_Set( "hud.chat.count" );
	if ( e ) {
		e->value = (float) wired_hudChatCount;
		Com_sprintf( e->text, sizeof( e->text ), "%d", wired_hudChatCount );
	}
}

/* Drop fully-expired chat lines (age > WIRED_HUD_CHAT_LIFETIME) and
 * re-publish the store. The ring is ordered newest-first (slot 0), so once
 * a slot is past its lifetime every older slot is too — truncate the tail.
 * Called every frame from WiredUI_HudTick; the publish is idempotent so
 * the caller need not know whether anything was removed. */
static void WiredHud_ChatExpire( void ) {
	int now = wiredHud->time;
	int i;

	/* Find the first expired slot scanning newest→oldest; everything from
	 * there to the tail is at least as old, so it expires too. */
	for ( i = 0; i < wired_hudChatCount; i++ ) {
		if ( ( now - wired_hudChatTime[ i ] ) > WIRED_HUD_CHAT_LIFETIME ) {
			break;
		}
	}
	if ( i < wired_hudChatCount ) {
		wired_hudChatCount = i;
	}
	WiredHud_ChatPublish();
}

// from cl_wired_hud_compat.c
extern modernhudGlobalContext_t* CG_ModernHUDGetContext( void );

// ── event receiver (called from cl_cgame.c trap handler) ─────────────

void WiredHud_ReceiveEvent( int type, const char *data ) {
	modernhudGlobalContext_t *ctx = CG_ModernHUDGetContext();
	if ( !data ) return;

	switch ( type ) {
		case WIRED_EVENT_CHAT:
		case WIRED_EVENT_TEAMCHAT: {
			/* Chat history is exposed through the WiredStore prefix
			 * `hud.chat.*` rather than a private struct field. Newest
			 * message lands at slot 0; older entries shift down; ring
			 * caps at WIRED_HUD_CHAT_RING_MAX. Modder authoring drives
			 * the visible row count via a `repeat { source "hud.chat"
			 * countbind "hud.chat.count" as "row" … }` block in
			 * default.wui.
			 *
			 * the ring + per-slot arrival time live at file scope
			 * (wired_hudChatRing / wired_hudChatTime) so WiredHud_Chat
			 * Expire(), driven from the tick, can drop time-expired lines
			 * (legacy `time 8000 fadedelay 2000`). Insert here, expire +
			 * publish from the tick; publish once on insert too so a line
			 * appears the same frame it arrives. */
			int i;

			for ( i = ( wired_hudChatCount < WIRED_HUD_CHAT_RING_MAX
			              ? wired_hudChatCount
			              : WIRED_HUD_CHAT_RING_MAX - 1 );
			      i > 0; i-- ) {
				Q_strncpyz( wired_hudChatRing[ i ], wired_hudChatRing[ i - 1 ], MAX_SAY_TEXT );
				wired_hudChatTime[ i ] = wired_hudChatTime[ i - 1 ];
			}
			Q_strncpyz( wired_hudChatRing[ 0 ], data, MAX_SAY_TEXT );
			wired_hudChatTime[ 0 ] = wiredHud->time;
			if ( wired_hudChatCount < WIRED_HUD_CHAT_RING_MAX ) wired_hudChatCount++;

			WiredHud_ChatPublish();
			break;
		}
		case WIRED_EVENT_OBITUARY: {
			/* Recent kill feed. Newest obituary at slot 0;
			 * older shift down; capped at WIRED_HUD_FEED_RING_MAX. Exposed
			 * via the hud.feed.* WiredStore prefix, mirroring hud.chat.*. */
			static char     feed_ring[ WIRED_HUD_FEED_RING_MAX ][ MAX_SAY_TEXT ];
			static int      feed_count = 0;
			wuiStoreEntry_t *fe;
			int             i;

			for ( i = ( feed_count < WIRED_HUD_FEED_RING_MAX
			              ? feed_count
			              : WIRED_HUD_FEED_RING_MAX - 1 );
			      i > 0; i-- ) {
				Q_strncpyz( feed_ring[ i ], feed_ring[ i - 1 ], MAX_SAY_TEXT );
			}
			Q_strncpyz( feed_ring[ 0 ], data, MAX_SAY_TEXT );
			if ( feed_count < WIRED_HUD_FEED_RING_MAX ) feed_count++;

			for ( i = 0; i < feed_count; i++ ) {
				char key[ 64 ];
				Com_sprintf( key, sizeof( key ), "hud.feed.%d.text", i );
				fe = WiredStore_Set( key );
				if ( fe ) Q_strncpyz( fe->text, feed_ring[ i ], sizeof( fe->text ) );
			}
			fe = WiredStore_Set( "hud.feed.count" );
			if ( fe ) {
				fe->value = (float) feed_count;
				Com_sprintf( fe->text, sizeof( fe->text ), "%d", feed_count );
			}
			break;
		}
		case WIRED_EVENT_FRAG:
			Q_strncpyz( ctx->fragmessage.message, data, sizeof( ctx->fragmessage.message ) );
			ctx->fragmessage.time = wiredHud->time;
			break;
		case WIRED_EVENT_RANK:
			Q_strncpyz( ctx->rankmessage.message, data, sizeof( ctx->rankmessage.message ) );
			ctx->rankmessage.time = wiredHud->time;
			break;
		case WIRED_EVENT_FRAG_RANK: {
			// combined frag+rank atomic pair — enqueue into message queue
			int idx = ctx->msgQueue.writeIndex % ModernHUD_MSG_QUEUE_SIZE;
			modernhudMsgEntry_t *entry = &ctx->msgQueue.entries[idx];
			const char *sep = strchr( data, '|' );
			if ( sep ) {
				Q_strncpyz( entry->line1, data, MIN( (int)(sep - data) + 1, ModernHUD_MSG_MAX_LEN ) );
				Q_strncpyz( entry->line2, sep + 1, ModernHUD_MSG_MAX_LEN );
			} else {
				Q_strncpyz( entry->line1, data, ModernHUD_MSG_MAX_LEN );
				entry->line2[0] = '\0';
			}
			entry->arriveTime = wiredHud->time;
			entry->displayTime = 2000;
			entry->priority = ModernHUD_MSG_HIGH;
			entry->shown = qfalse;
			ctx->msgQueue.writeIndex++;
			break;
		}
		case WIRED_EVENT_CENTERPRINT: {
			// center print — enqueue with NORMAL priority
			int idx = ctx->msgQueue.writeIndex % ModernHUD_MSG_QUEUE_SIZE;
			modernhudMsgEntry_t *entry = &ctx->msgQueue.entries[idx];
			Q_strncpyz( entry->line1, data, ModernHUD_MSG_MAX_LEN );
			entry->line2[0] = '\0';
			entry->arriveTime = wiredHud->time;
			entry->displayTime = 3000;
			entry->priority = ModernHUD_MSG_NORMAL;
			entry->shown = qfalse;
			ctx->msgQueue.writeIndex++;
			break;
		}
		case WIRED_EVENT_SUBTITLE: {
			// scene-caption subtitle — the localized text renders on the
			// scene:subtitle overlay (not HUD-gated), so it survives a HUD-off
			// cutscene. A new caption replaces the current one; the fade timer
			// restarts from this arrival.
			Q_strncpyz( ctx->caption.subtitle, data, ModernHUD_MSG_MAX_LEN );
			ctx->caption.arriveTime = wiredHud->time;
			break;
		}
		case WIRED_EVENT_OBJECTIVE: {
			// mission objectives — a "text,completed,failed;" list (already
			// localized by the cgame). Parse into the standing objectives slot the
			// hud:objectives element renders. Replaces the whole list each push.
			// The completed+failed flags are the LAST two comma fields; the text is
			// everything before them, so localized text containing commas parses
			// correctly (we scan the flag commas from the end of the record).
			const char *p = data;
			int         n = 0;
			ctx->numObjectives = 0;
			while ( p && *p && n < ModernHUD_MAX_OBJECTIVES ) {
				const char *semi = strchr( p, ';' );
				const char *cFailed;    // last comma  -> failed flag
				const char *cCompleted; // 2nd-last comma -> completed flag
				const char *c;
				int         len;
				if ( !semi )
					break;
				// find the last two commas within this record [p, semi)
				cFailed = cCompleted = NULL;
				for ( c = p; c < semi; c++ ) {
					if ( *c == ',' ) { cCompleted = cFailed; cFailed = c; }
				}
				if ( !cFailed || !cCompleted ) {   // malformed record — skip it
					p = semi + 1;
					continue;
				}
				len = (int)( cCompleted - p );     // text = up to the 2nd-last comma
				if ( len < 0 )
					len = 0;
				if ( len >= ModernHUD_MSG_MAX_LEN )
					len = ModernHUD_MSG_MAX_LEN - 1;
				memcpy( ctx->objectives[n].text, p, len );
				ctx->objectives[n].text[len] = '\0';
				ctx->objectives[n].completed = ( atoi( cCompleted + 1 ) != 0 );
				ctx->objectives[n].failed    = ( atoi( cFailed + 1 ) != 0 );
				n++;
				p = semi + 1;
			}
			ctx->numObjectives = n;
			break;
		}
		case WIRED_EVENT_AWARD: {
			// format: "name|shader_path|count"
			int idx = ctx->awards.writeIndex % ModernHUD_MAX_AWARD_QUEUE;
			modernhudAwardEntry_t *entry = &ctx->awards.entries[idx];
			const char *p = data;
			const char *sep1, *sep2;

			sep1 = strchr( p, '|' );
			if ( !sep1 ) break;
			sep2 = strchr( sep1 + 1, '|' );
			if ( !sep2 ) break;

			Q_strncpyz( entry->name, p, MIN( (int)(sep1 - p) + 1, (int)sizeof( entry->name ) ) );
			Q_strncpyz( entry->shaderPath, sep1 + 1, MIN( (int)(sep2 - sep1), (int)sizeof( entry->shaderPath ) ) );
			entry->count = atoi( sep2 + 1 );
			entry->arriveTime = wiredHud->time;
			ctx->awards.writeIndex++;
			break;
		}
		case WIRED_EVENT_TEMPACC: {
			// format: "weapon|accuracy"
			int wp;
			float acc;
			if ( sscanf( data, "%d|%f", &wp, &acc ) == 2 ) {
				if ( wp >= 0 && wp < (int)(sizeof(ctx->tempAcc.weapon) / sizeof(ctx->tempAcc.weapon[0])) ) {
					ctx->tempAcc.weapon[wp].tempAccuracy = acc;
				}
			}
			break;
		}
	}
}

// ── state receiver (called from cl_cgame.c trap handler) ─────────────

void WiredHud_ReceiveState( wiredHudState_t *state ) {
	if ( !state ) return;
	memcpy( &wired_hudStateStorage, state, sizeof( wiredHudState_t ) );
	wired_hudStateStorage.valid = qtrue;
	wiredHud_state_valid        = qtrue;  /* mirror */
}

// ── HUD draw-enable predicate ─────────────────────────────────────────
/* `cl_drawHud 0` (2026-06-10) must HIDE the HUD, not freeze it.
 * Element rendering moved to the compositor's WUI_LAYER_HUD walk, whose
 * activation predicate (policy/hud.c::hud_policy_isActive) never consulted
 * cl_drawHud — so the HUD kept drawing while WiredUI_HudTick's old early-
 * return only froze the *data*. The gate belongs on the DRAW (the layer
 * activation predicate), not the tick. This accessor exposes the cvar
 * (owned/registered by this file) so the HUD-layer activation predicate
 * can suppress the whole layer when the HUD is toggled off.
 *
 * Companion edit (policy/hud.c, separate owner): hud_policy_isActive()
 * must `&& WiredHud_DrawEnabled()` (or, if it prefers zero linkage, read
 * Cvar_VariableIntegerValue("cl_drawHud")). Until that lands cl_drawHud
 * is inert, but the tick no longer freezes stale data (see HudTick). */
qboolean WiredHud_DrawEnabled( void ) {
	return ( cl_drawHud && !cl_drawHud->integer ) ? qfalse : qtrue;
}

// ── data binding lookup ──────────────────────────────────────────────

const wiredHudBinding_t *WiredHud_FindBinding( const char *name ) {
	if ( !name || !name[0] || !wiredHud->valid ) return NULL;
	for ( int i = 0; i < wiredHud->numBindings && i < WIRED_HUD_MAX_BINDINGS; i++ ) {
		if ( !Q_stricmp( wiredHud->bindings[i].name, name ) )
			return &wiredHud->bindings[i];
	}
	return NULL;
}

// ── init / shutdown ──────────────────────────────────────────────────

void WiredHud_Init( void ) {
	static const cvarDesc_t d = CVAR_BOOL( "cl_drawHud", "1", CVAR_ARCHIVE | CVAR_NODEFAULT,
		"Draw the Wired HUD. 0: off. 1: on (default)." );
	cl_drawHud = Cvar_Register( &d );
	memset( &wired_hudStateStorage, 0, sizeof( wired_hudStateStorage ) );
	wiredHud_elementsLoaded = qfalse;

	/* Point cg.snap at the embedded zero-init _snapData so the HUD element
	 * routines (CG_ModernHUDElement* in elements/) can deref cg.snap->ps.*
	 * before WiredHud_SyncCompat ever runs. SyncCompat updates the data
	 * during CA_ACTIVE; outside that window the routines see a safe zero
	 * playerState_t (clientNum=0 → cgs.clientinfo[0].team=0 → team-side
	 * branches return early) instead of NULL-deref'ing. This matters for
	 * ui_testall pushing hud_default at CA_DISCONNECTED and for the FIRST
	 * CA_ACTIVE frame before SyncCompat fires (compositor's HUD walk
	 * happens before WiredHud_Routine in SCR_DrawScreenField). */
	memset( &wired_cg._snapData, 0, sizeof( wired_cg._snapData ) );
	wired_cg.snap = &wired_cg._snapData;

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredHud: initialized (Phase 3)\n" );
}

void WiredHud_Shutdown( void ) {
	WiredHud_DestroyAllElements();
	/* release the bump arena that backs every HUD
	 * element context. Engine-layer lifetime: process-exit reclaim is
	 * sufficient for correctness; explicit destroy here keeps /meminfo
	 * accurate if this shutdown ever wires up. */
	WiredHud_ShutdownArena();
	memset( &wired_hudStateStorage, 0, sizeof( wired_hudStateStorage ) );
	wiredHud_state_valid    = qfalse;  /* mirror */
	wiredHud_elementsLoaded = qfalse;
}

// ── prototype FPS element ─────────────────────────────────────────────
// Minimal fps counter to prove the state bridge + client rendering pipeline.
// This will be replaced by the full ModernHUD element migration in Step 4.

#define WIREDHUD_FPS_FRAMES  4

static struct {
	float   timeAverage;
	int     framesNum;
	int     timePrev;
} wiredHudFps;

static void WiredHud_DrawFps( int realtime ) {
	float fps_val;
	int fps_int;
	char buf[32];
	float x, y, charSize;
	vec4_t color = { 1.0f, 1.0f, 1.0f, 0.5f };

	// fps calculation (same algorithm as ModernHUD fps element)
	if ( wiredHudFps.timePrev == 0 ) {
		wiredHudFps.timePrev = realtime;
		return;
	}
	wiredHudFps.timeAverage *= wiredHudFps.framesNum;
	wiredHudFps.timeAverage += realtime - wiredHudFps.timePrev;
	wiredHudFps.timeAverage /= ++wiredHudFps.framesNum;
	wiredHudFps.timePrev = realtime;

	if ( wiredHudFps.framesNum > WIREDHUD_FPS_FRAMES ) {
		wiredHudFps.framesNum = WIREDHUD_FPS_FRAMES;
	}

	if ( wiredHudFps.timeAverage <= 0 ) return;

	fps_val = 1000.0f / wiredHudFps.timeAverage;
	fps_int = (int)( fps_val + 0.5f );

	Com_sprintf( buf, sizeof( buf ), "%dfps", fps_int );

	// draw below ModernHUD fps — using proper font system
	charSize = 8.0f;
	x = (float)cls.glconfig.vidWidth - 2.0f;  // right-aligned
	y = 16.0f;

	// green tint to distinguish from ModernHUD's white fps
	color[0] = 0.2f; color[1] = 1.0f; color[2] = 0.4f; color[3] = 0.8f;

	Text_Draw( buf, x, y, FONT_DISPLAY, charSize, color, TEXT_ALIGN_RIGHT, 0 );
}

// ── wiredItemDef_t → modernhudConfig_t conversion ─────────────────────
// Converts Wired UI parsed item properties to ModernHUD config format
// so that elements can be created from .hud file definitions.

/* exposed (was static) so the unified custom-draw
 * bootstrap adapters in cl_wired_hud_registry.c can convert a
 * wiredItemDef_t into the legacy create() callback's expected config. */
void WiredHud_ItemToConfig( const wiredItemDef_t *item, modernhudConfig_t *cfg ) {
	memset( cfg, 0, sizeof( *cfg ) );

	// rect
	if ( item->rect.x != 0 || item->rect.y != 0 || item->rect.w != 0 || item->rect.h != 0 ) {
		cfg->rect.isSet = qtrue;
		cfg->rect.value[0] = item->rect.x;
		cfg->rect.value[1] = item->rect.y;
		cfg->rect.value[2] = item->rect.w;
		cfg->rect.value[3] = item->rect.h;
	}

	// forecolor → color
	if ( item->forecolor[3] > 0 ) {
		cfg->color.isSet = qtrue;
		cfg->color.value.type = MODERNHUD_COLOR_RGBA;
		Vector4Copy( item->forecolor, cfg->color.value.rgba );
	}

	// backcolor → bgcolor
	if ( item->backcolor[3] > 0 ) {
		cfg->bgcolor.isSet = qtrue;
		cfg->bgcolor.value.type = MODERNHUD_COLOR_RGBA;
		Vector4Copy( item->backcolor, cfg->bgcolor.value.rgba );
	}

	// fontsize — prefer explicit fontsize W H over textscale
	if ( item->fontSize[0] > 0 || item->fontSize[1] > 0 ) {
		cfg->fontsize.isSet = qtrue;
		cfg->fontsize.value[0] = item->fontSize[0];
		cfg->fontsize.value[1] = item->fontSize[1];
	} else if ( item->textscale > 0 ) {
		cfg->fontsize.isSet = qtrue;
		cfg->fontsize.value[0] = item->textscale * 16.0f;
		cfg->fontsize.value[1] = item->textscale * 20.0f;
	}

	// textalign → textAlign (sentinel -1 = not set)
	if ( item->textalign >= 0 ) {
		cfg->textAlign.isSet = qtrue;
		cfg->textAlign.value = (modernhudAlignH_t)item->textalign;
	}

	// text
	if ( item->text[0] ) {
		cfg->text.isSet = qtrue;
		Q_strncpyz( cfg->text.value, item->text, sizeof( cfg->text.value ) );
	}

	// image (from "image" keyword or "background" keyword)
	if ( item->image[0] ) {
		cfg->image.isSet = qtrue;
		Q_strncpyz( cfg->image.value, item->image, sizeof( cfg->image.value ) );
	} else if ( item->background[0] ) {
		cfg->image.isSet = qtrue;
		Q_strncpyz( cfg->image.value, item->background, sizeof( cfg->image.value ) );
	}

	// style
	if ( item->style ) {
		cfg->style.isSet = qtrue;
		cfg->style.value = item->style;
	}

	// font name
	if ( item->fontName[0] ) {
		cfg->font.isSet = qtrue;
		Q_strncpyz( cfg->font.value, item->fontName, sizeof( cfg->font.value ) );
	}

	// fontweight (currently regular/medium/bold routing)
	if ( item->fontWeight > 0 ) {
		cfg->fontWeight.isSet = qtrue;
		cfg->fontWeight.value = item->fontWeight;
	}

	// letterspacing
	if ( item->letterSpacing != 0.0f ) {
		cfg->letterspacing.isSet = qtrue;
		cfg->letterspacing.value = item->letterSpacing;
	}

	// direction (bar direction: L2R, R2L, T2B, B2T)
	if ( item->direction >= 0 ) {
		cfg->direction.isSet = qtrue;
		cfg->direction.value = (modernhudDirection_t)item->direction;
	}

	// fill
	if ( item->fillFlag ) {
		cfg->fill.isSet = qtrue;
	}

	// monospace
	if ( item->monospace ) {
		cfg->monospace.isSet = qtrue;
	}

	// color2
	if ( item->color2[3] > 0 ) {
		cfg->color2.isSet = qtrue;
		cfg->color2.value.type = MODERNHUD_COLOR_RGBA;
		Vector4Copy( item->color2, cfg->color2.value.rgba );
	}

	// Per-side border widths + border color → border/borderColor.
	// Mirrors the color2 block; lets hudElements (e.g. weaponlist selected-slot)
	// draw the authored outline. bordersize4 is L,R,T,B (Clay order).
	if ( item->bordersize4[0] > 0 || item->bordersize4[1] > 0
	  || item->bordersize4[2] > 0 || item->bordersize4[3] > 0 ) {
		cfg->border.isSet = qtrue;
		cfg->border.value[0] = item->bordersize4[0];
		cfg->border.value[1] = item->bordersize4[1];
		cfg->border.value[2] = item->bordersize4[2];
		cfg->border.value[3] = item->bordersize4[3];
	}
	if ( item->bordercolor[3] > 0 ) {
		cfg->borderColor.isSet = qtrue;
		cfg->borderColor.value.type = MODERNHUD_COLOR_RGBA;
		Vector4Copy( item->bordercolor, cfg->borderColor.value.rgba );
	}

	// alignV (sentinel -1 = not set)
	if ( item->alignV >= 0 ) {
		cfg->alignV.isSet = qtrue;
		cfg->alignV.value = (modernhudAlignV_t)item->alignV;
	}

	// fade
	if ( item->fadeColor[3] > 0 ) {
		cfg->fade.isSet = qtrue;
		Vector4Copy( item->fadeColor, cfg->fade.value );
	}

	// fadedelay
	if ( item->fadeDelay > 0 ) {
		cfg->fadedelay.isSet = qtrue;
		cfg->fadedelay.value = item->fadeDelay;
	}

	// time
	if ( item->timeMs > 0 ) {
		cfg->time.isSet = qtrue;
		cfg->time.value = item->timeMs;
	}

	// textstyle
	if ( item->textstyle > 0 ) {
		cfg->textStyle.isSet = qtrue;
		cfg->textStyle.value = item->textstyle;
	}

	// bind (data binding name for generic elements)
	if ( item->bind[0] ) {
		cfg->bind.isSet = qtrue;
		Q_strncpyz( cfg->bind.value, item->bind, sizeof( cfg->bind.value ) );
	}
}

// ── load HUD elements from hudOverlay menus ──────────────────────────
// Scans all loaded menus with hudOverlay=1, finds items with hudElement
// set, and creates ModernHUD elements from them.

void WiredHud_LoadFromMenus( void ) {
	int menuCount = WiredUI_GetMenuCount();
	int created = 0;
	int hudOverlayMenus = 0;

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredHud: scanning %d menus for hudOverlay...\n", menuCount );

	// iterate all loaded menus
	for ( int i = 0; i < menuCount; i++ ) {
		wiredMenuDef_t *menu = WiredUI_GetMenuByIndex( i );
		if ( !menu ) continue;

		if ( !menu->hudOverlay ) {
			continue;
		}

		hudOverlayMenus++;
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredHud: found hudOverlay menu '%s' with %d items\n", menu->name, menu->itemCount );

		// this is a HUD overlay menu — create elements from its items
		for ( int j = 0; j < menu->itemCount; j++ ) {
			wiredItemDef_t *item = menu->items[j];
			if ( !item->hudElement[0] ) continue;

			modernhudConfig_t cfg;
			WiredHud_ItemToConfig( item, &cfg );

			if ( WiredHud_CreateElement( item->hudElement, &cfg ) ) {
				created++;
			} else {
				COM_WARN( LOG_CH(ch_ui), "WiredHud: failed to create '%s'\n", item->hudElement );
			}
		}
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredHud: %d hudOverlay menus, %d elements created, %d active\n",
		hudOverlayMenus, created, WiredHud_GetElementCount() );
	wiredHud_elementsLoaded = qtrue;
}

// ── per-frame HUD rendering ─────────────────────────────────────────

void WiredUI_HudTick( int realtime ) {
	if ( !wiredHud->valid ) return;
	/* do NOT early-return on `cl_drawHud 0` here. Gating the tick
	 * froze the HUD's *data* while the compositor's WUI_LAYER_HUD walk
	 * kept drawing it (the walk's activation policy never consulted
	 * cl_drawHud). The draw is now suppressed at the layer-activation
	 * predicate via WiredHud_DrawEnabled(); the tick keeps state fresh so
	 * the HUD is correct the instant the layer reactivates. */

	// lazy element init — deferred from WiredUI_Init to avoid Z_CheckHeap crash
	if ( !wiredHud_elementsLoaded ) {
		int64_t _wld_t0 = Sys_Microseconds();
		WiredHud_LoadFromMenus();
		cl_prof.whud_load += (int)(Sys_Microseconds() - _wld_t0);
		wiredHud_elementsLoaded = qtrue;
	}

	// sync compat structs so element code sees cg.*/cgs.* patterns
	CL_PROF( whud_sync, WiredHud_SyncCompat() );

	/* time-based chat-line expiry. Drops lines older than the legacy
	 * lifetime (8s solid + 2s fade window) and re-publishes hud.chat.* so
	 * old messages disappear as they did with the retired chat element.
	 * (Fade alpha needs an out-of-lane .wui/compositor companion edit; see
	 * the WiredHud_ChatExpire header comment.) */
	WiredHud_ChatExpire();

	// render all active HUD elements through ModernHUD lifecycle
	/* WiredHud_RenderElements call retired. Elements render via
	 * compositor's CUSTOM dispatch through the unified registry.
	 * HUD state still updated via WiredHud_SyncCompat
	 * + element lifecycle in this routine — only the render loop deletes. */

	// scoreboard overlay — select and render gametype-specific scoreboard menu
	{
		qboolean showSb = wiredHud->showScores || wiredHud->intermission || wiredHud->warmup > 0;
		/* scoreboards default visible=0 + tagged layer "hud".
		 * Compositor's HUD-layer multi-panel walk emits them when this
		 * block flips visible=1. Reset all siblings every frame so exactly
		 * one (the gametype-correct one) is visible while showSb is true.
		 * Legacy WiredUI_RenderMenuOverlay retired — compositor
		 * is sole renderer. */
		static const char *s_sbAllNames[] = {
			"ingame_scoreboard_ffa", "ingame_scoreboard_duel",
			"ingame_scoreboard_tdm", "ingame_scoreboard_ctf",
			"end_scoreboard_ffa",    "end_scoreboard_duel",
			"end_scoreboard_tdm",    "end_scoreboard_ctf",
			NULL
		};
		{
			int _ai;
			for ( _ai = 0; s_sbAllNames[ _ai ]; _ai++ ) {
				wiredMenuDef_t *_m = WiredUI_FindMenu( s_sbAllNames[ _ai ] );
				if ( _m ) _m->visible = qfalse;
			}
		}
		if ( showSb ) {
			int64_t _wsb_t0 = Sys_Microseconds();
			static const struct { int gt; const char *suffix; } s_sbSuffix[] = {
				{ GT_DUEL,           "duel" },
				{ GT_TDM,            "tdm"  },
				{ GT_CTF,            "ctf"  },
				{ GT_1FCTF,          "ctf"  },
				{ GT_OBELISK,        "tdm"  },
				{ GT_HARVESTER,      "tdm"  },
				{ -1, NULL }
			};
			const char *prefix = wiredHud->intermission ? "end_scoreboard" : "ingame_scoreboard";
			const char *suffix = "ffa";
			const char *menuName;
			wiredMenuDef_t *sbMenu;
			int _si;
			for ( _si = 0; s_sbSuffix[_si].gt >= 0; _si++ ) {
				if ( s_sbSuffix[_si].gt == wiredHud->gametype ) { suffix = s_sbSuffix[_si].suffix; break; }
			}
			menuName = va( "%s_%s", prefix, suffix );

			sbMenu = WiredUI_FindMenu( menuName );
			if ( !sbMenu ) {
				menuName = va( "%s_ffa", prefix );
				sbMenu = WiredUI_FindMenu( menuName );
			}
			if ( sbMenu ) sbMenu->visible = qtrue;

			if ( sbMenu ) {
				// set cvars for header text
				Cvar_Set( "wired_sb_gametype", wiredHud->gametypeName );

				// compute placement text (FFA modes only)
				if ( wiredHud->gametype < 5 ) {
					int myRank = 0, myScore = 0;
					for ( int i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
						if ( wiredHud->scores[i].team == 3 ) continue; /* spectator */
						myRank++;
						if ( wiredHud->scores[i].client == wiredHud->clientNum ) {
							myScore = wiredHud->scores[i].score;
							break;
						}
					}
					if ( myRank > 0 ) {
						// CG_PlaceString convention: 1st=blue, 2nd=red, 3rd=yellow
						const char *ordinal;
						if ( myRank == 1 )       ordinal = "^4" "1st";
						else if ( myRank == 2 )  ordinal = "^1" "2nd";
						else if ( myRank == 3 )  ordinal = "^3" "3rd";
						else if ( myRank == 11 ) ordinal = "11th";
						else if ( myRank == 12 ) ordinal = "12th";
						else if ( myRank == 13 ) ordinal = "13th";
						else if ( myRank % 10 == 1 ) ordinal = va( "%ist", myRank );
						else if ( myRank % 10 == 2 ) ordinal = va( "%ind", myRank );
						else if ( myRank % 10 == 3 ) ordinal = va( "%ird", myRank );
						else                          ordinal = va( "%ith", myRank );
						Cvar_Set( "wired_sb_place", va( "%s ^7place with %i", ordinal, myScore ) );
					} else {
						Cvar_Set( "wired_sb_place", "" );
					}
				}

				/* scoreboard rendered via compositor (visible=1 above) */
				(void) sbMenu;
			} else {
				vec4_t fallbackColor = { 1, 1, 1, 1 };
				float vw = (float)cls.glconfig.vidWidth;
				float vh = (float)cls.glconfig.vidHeight;

				if ( wiredHud->gametype == GT_DUEL ) {
					WiredHud_DrawDuelBoard( 0.0f, 0.0f, vw, vh );
				} else if ( wiredHud->isTeamGame ) {
					float x = vw * 0.047f;
					float y = vh * 0.071f;
					float w = vw * 0.906f;
					float h = vh * 0.778f;
					WiredHud_DrawScorelistWidget( x, y, w * 0.49f, h, 0x05, fallbackColor );
					WiredHud_DrawScorelistWidget( x + w * 0.51f, y, w * 0.49f, h, 0x06, fallbackColor );
				} else {
					WiredHud_DrawScorelistWidget( vw * 0.109f, vh * 0.05f, vw * 0.781f, vh * 0.85f, 0x0b, fallbackColor );
				}
			}
			cl_prof.whud_score += (int)(Sys_Microseconds() - _wsb_t0);
		}
	}
}

#endif // FEAT_WIRED_UI
