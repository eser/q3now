/*
===========================================================================
cl_wired_hud.c — Wired UI HUD: client-side element rendering

Phase 3: SuperHUD elements run in the client. Game state is pushed by
cgame each frame via trap_WiredUI_PushHudState(). Elements read from
the wiredHud global instead of cgame globals.
===========================================================================
*/

#include "../client.h"
#include "cl_wired_hud.h"
#include "cl_wired_ui.h"
#include "cl_wired_fonts.h"
#include "cl_wired_hud_private.h"

// from cl_wired_hud_compat.c
extern void WiredHud_SyncCompat( void );


// from cl_wired_hud_registry.c
extern qboolean WiredHud_CreateElement( const char *name, const superhudConfig_t *config );
extern void     WiredHud_DestroyAllElements( void );
extern void     WiredHud_RenderElements( void );
extern int      WiredHud_GetElementCount( void );

// from cl_wired_parse.c
extern wiredMenuDef_t *WiredUI_GetMenuByIndex( int index );

#if FEAT_WIRED_UI

// ── global HUD state ─────────────────────────────────────────────────

wiredHudState_t  wired_hudStateStorage;
wiredHudState_t *wiredHud = &wired_hudStateStorage;

// from cl_wired_hud_compat.c
extern superhudGlobalContext_t* CG_SHUDGetContext( void );

// ── event receiver (called from cl_cgame.c trap handler) ─────────────

void WiredHud_ReceiveEvent( int type, const char *data ) {
	superhudGlobalContext_t *ctx = CG_SHUDGetContext();
	if ( !data ) return;

	switch ( type ) {
		case WIRED_EVENT_CHAT: {
			int index = ctx->chat.index % SHUD_MAX_CHAT_LINES;
			Q_strncpyz( ctx->chat.line[index].message, data, MAX_SAY_TEXT );
			ctx->chat.line[index].time = wiredHud->time;
			ctx->chat.index++;
			break;
		}
		case WIRED_EVENT_TEAMCHAT: {
			int index = ctx->chat.index % SHUD_MAX_CHAT_LINES;
			Q_strncpyz( ctx->chat.line[index].message, data, MAX_SAY_TEXT );
			ctx->chat.line[index].time = wiredHud->time;
			ctx->chat.index++;
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
			int idx = ctx->msgQueue.writeIndex % SHUD_MSG_QUEUE_SIZE;
			superhudMsgEntry_t *entry = &ctx->msgQueue.entries[idx];
			const char *sep = strchr( data, '|' );
			if ( sep ) {
				Q_strncpyz( entry->line1, data, MIN( (int)(sep - data) + 1, SHUD_MSG_MAX_LEN ) );
				Q_strncpyz( entry->line2, sep + 1, SHUD_MSG_MAX_LEN );
			} else {
				Q_strncpyz( entry->line1, data, SHUD_MSG_MAX_LEN );
				entry->line2[0] = '\0';
			}
			entry->arriveTime = wiredHud->time;
			entry->displayTime = 2000;
			entry->priority = SHUD_MSG_HIGH;
			entry->shown = qfalse;
			ctx->msgQueue.writeIndex++;
			break;
		}
		case WIRED_EVENT_CENTERPRINT: {
			// center print — enqueue with NORMAL priority
			int idx = ctx->msgQueue.writeIndex % SHUD_MSG_QUEUE_SIZE;
			superhudMsgEntry_t *entry = &ctx->msgQueue.entries[idx];
			Q_strncpyz( entry->line1, data, SHUD_MSG_MAX_LEN );
			entry->line2[0] = '\0';
			entry->arriveTime = wiredHud->time;
			entry->displayTime = 3000;
			entry->priority = SHUD_MSG_NORMAL;
			entry->shown = qfalse;
			ctx->msgQueue.writeIndex++;
			break;
		}
		case WIRED_EVENT_AWARD: {
			// format: "name|shader_path|count"
			int idx = ctx->awards.writeIndex % SHUD_MAX_AWARD_QUEUE;
			superhudAwardEntry_t *entry = &ctx->awards.entries[idx];
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
	}
}

// ── state receiver (called from cl_cgame.c trap handler) ─────────────

void WiredHud_ReceiveState( wiredHudState_t *state ) {
	if ( !state ) return;
	Com_Memcpy( &wired_hudStateStorage, state, sizeof( wiredHudState_t ) );
	wired_hudStateStorage.valid = qtrue;
}

// ── data binding lookup ──────────────────────────────────────────────

const wiredHudBinding_t *WiredHud_FindBinding( const char *name ) {
	int i;
	if ( !name || !name[0] || !wiredHud->valid ) return NULL;
	for ( i = 0; i < wiredHud->numBindings && i < WIRED_HUD_MAX_BINDINGS; i++ ) {
		if ( !Q_stricmp( wiredHud->bindings[i].name, name ) )
			return &wiredHud->bindings[i];
	}
	return NULL;
}

// ── init / shutdown ──────────────────────────────────────────────────

static qboolean wiredHud_fontsLoaded = qfalse;

void WiredHud_Init( void ) {
	Com_Memset( &wired_hudStateStorage, 0, sizeof( wired_hudStateStorage ) );
	wiredHud_fontsLoaded = qfalse;
	Com_Printf( "WiredHud: initialized (Phase 3)\n" );
}

void WiredHud_Shutdown( void ) {
	Com_Memset( &wired_hudStateStorage, 0, sizeof( wired_hudStateStorage ) );
}

// ── prototype FPS element ─────────────────────────────────────────────
// Minimal fps counter to prove the state bridge + client rendering pipeline.
// This will be replaced by the full SuperHUD element migration in Step 4.

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

	// fps calculation (same algorithm as SuperHUD fps element)
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

	// draw below SuperHUD fps — using proper font system
	charSize = 8.0f;
	x = 640.0f - 2.0f;  // right-aligned
	y = 16.0f;

	// green tint to distinguish from SuperHUD's white fps
	color[0] = 0.2f; color[1] = 1.0f; color[2] = 0.4f; color[3] = 0.8f;

	CG_FontSelect( CG_FontIndexFromName( "sansman" ) );
	CG_ModernDrawString( x, y, buf, color, charSize, charSize * 1.5f, 0,
		DS_HRIGHT | DS_PROPORTIONAL, NULL );
}

// ── wiredItemDef_t → superhudConfig_t conversion ─────────────────────
// Converts Wired UI parsed item properties to SuperHUD config format
// so that elements can be created from .hud file definitions.

static void WiredHud_ItemToConfig( const wiredItemDef_t *item, superhudConfig_t *cfg ) {
	Com_Memset( cfg, 0, sizeof( *cfg ) );

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
		cfg->color.value.type = SUPERHUD_COLOR_RGBA;
		Vector4Copy( item->forecolor, cfg->color.value.rgba );
	}

	// backcolor → bgcolor
	if ( item->backcolor[3] > 0 ) {
		cfg->bgcolor.isSet = qtrue;
		cfg->bgcolor.value.type = SUPERHUD_COLOR_RGBA;
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
		cfg->textAlign.value = (superhudAlignH_t)item->textalign;
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

	// direction (bar direction: L2R, R2L, T2B, B2T)
	if ( item->direction >= 0 ) {
		cfg->direction.isSet = qtrue;
		cfg->direction.value = (superhudDirection_t)item->direction;
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
		cfg->color2.value.type = SUPERHUD_COLOR_RGBA;
		Vector4Copy( item->color2, cfg->color2.value.rgba );
	}

	// alignV (sentinel -1 = not set)
	if ( item->alignV >= 0 ) {
		cfg->alignV.isSet = qtrue;
		cfg->alignV.value = (superhudAlignV_t)item->alignV;
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
// set, and creates SuperHUD elements from them.

static qboolean wiredHud_elementsLoaded = qfalse;

void WiredHud_LoadFromMenus( void ) {
	int menuCount = WiredUI_GetMenuCount();
	int i, j;
	int created = 0;
	int hudOverlayMenus = 0;

	Com_Printf( "WiredHud: scanning %d menus for hudOverlay...\n", menuCount );

	// iterate all loaded menus
	for ( i = 0; i < menuCount; i++ ) {
		wiredMenuDef_t *menu = WiredUI_GetMenuByIndex( i );
		if ( !menu ) continue;

		Com_Printf( "WiredHud: menu[%d] = '%s' hudOverlay=%d items=%d\n",
			i, menu->name, menu->hudOverlay, menu->itemCount );

		if ( !menu->hudOverlay ) {
			continue;
		}

		hudOverlayMenus++;
		Com_Printf( "WiredHud: found hudOverlay menu '%s' with %d items\n", menu->name, menu->itemCount );

		// this is a HUD overlay menu — create elements from its items
		for ( j = 0; j < menu->itemCount; j++ ) {
			wiredItemDef_t *item = menu->items[j];
			if ( !item->hudElement[0] ) continue;

			superhudConfig_t cfg;
			WiredHud_ItemToConfig( item, &cfg );

			if ( WiredHud_CreateElement( item->hudElement, &cfg ) ) {
				created++;
			} else {
				Com_Printf( S_COLOR_YELLOW "WiredHud: failed to create '%s'\n", item->hudElement );
			}
		}
	}

	Com_Printf( "WiredHud: %d hudOverlay menus, %d elements created\n", hudOverlayMenus, created );
}

// ── per-frame HUD rendering ─────────────────────────────────────────

void WiredHud_Routine( int realtime ) {
	if ( !wiredHud->valid ) return;

	// lazy init — deferred from WiredUI_Init to avoid Z_CheckHeap crash
	if ( !wiredHud_fontsLoaded ) {
		CG_LoadFonts();
		wiredHud_fontsLoaded = qtrue;

		// load HUD elements from any hudOverlay menus
		WiredHud_LoadFromMenus();

		Com_Printf( "WiredHud: fonts loaded, %d elements active\n", WiredHud_GetElementCount() );
	}

	// sync compat structs so element code sees cg.*/cgs.* patterns
	WiredHud_SyncCompat();

	// render all active HUD elements through SuperHUD lifecycle
	WiredHud_RenderElements();

	// scoreboard overlay — select and render gametype-specific scoreboard menu
	{
		qboolean showSb = wiredHud->showScores || wiredHud->intermission;
		if ( showSb ) {
			const char *prefix = wiredHud->intermission ? "end_scoreboard" : "ingame_scoreboard";
			const char *menuName;
			wiredMenuDef_t *sbMenu;

			// select scoreboard menu by gametype
			switch ( wiredHud->gametype ) {
				case 5:  menuName = va( "%s_tdm", prefix ); break;  // GT_TEAM
				case 6:  menuName = va( "%s_ctf", prefix ); break;  // GT_CTF
				case 7:  menuName = va( "%s_ctf", prefix ); break;  // GT_1FCTF
				default: menuName = va( "%s_ffa", prefix ); break;  // FFA, Tournament, etc.
			}

			sbMenu = WiredUI_FindMenu( menuName );
			if ( sbMenu ) {
				// set cvars for header text
				{
					const char *gt;
					switch ( wiredHud->gametype ) {
						case 0:  gt = "Free For All"; break;
						case 1:  gt = "Tournament"; break;
						case 3:  gt = "King of the Hill"; break;
						case 4:  gt = "Last Man Standing"; break;
						case 5:  gt = "Team Deathmatch"; break;
						case 6:  gt = "Capture the Flag"; break;
						case 7:  gt = "One Flag CTF"; break;
						case 8:  gt = "Overload"; break;
						case 9:  gt = "Harvester"; break;
						default: gt = "Free For All"; break;
					}
					Cvar_Set( "wired_sb_gametype", gt );
				}

				// compute placement text (FFA modes only)
				if ( wiredHud->gametype < 5 ) {
					int i, myRank = 0, myScore = 0;
					for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
						if ( wiredHud->scores[i].team == TEAM_SPECTATOR ) continue;
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

				WiredUI_RenderMenuOverlay( sbMenu, realtime );
			}
		}
	}
}

#endif // FEAT_WIRED_UI
