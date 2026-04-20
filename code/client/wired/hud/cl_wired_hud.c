/*
cl_wired_hud.c — Wired UI HUD: client-side element rendering
*/

#include "../../client.h"
#include "cl_wired_hud.h"
#include "cl_wired_ui.h"
#include "cl_wired_text.h"
#include "cl_wired_hud_private.h"

// from cl_wired_hud_compat.c
extern void WiredHud_SyncCompat( void );


// from cl_wired_hud_registry.c
extern qboolean WiredHud_CreateElement( const char *name, const modernhudConfig_t *config );
extern void     WiredHud_DestroyAllElements( void );
extern void     WiredHud_RenderElements( void );
extern int      WiredHud_GetElementCount( void );

// from cl_wired_parse.c
extern wiredMenuDef_t *WiredUI_GetMenuByIndex( int index );
extern void WiredHud_DrawScorelistWidget( float ox, float oy, float ow, float oh,
	int feederID, const vec4_t textColor );
extern void WiredHud_DrawDuelBoard( float ox, float oy, float ow, float oh );

#if FEAT_WIRED_UI

// ── global HUD state ─────────────────────────────────────────────────

wiredHudState_t  wired_hudStateStorage;
wiredHudState_t *wiredHud = &wired_hudStateStorage;
static qboolean  wiredHud_elementsLoaded = qfalse;
static cvar_t   *cl_drawHud             = NULL;

// from cl_wired_hud_compat.c
extern modernhudGlobalContext_t* CG_ModernHUDGetContext( void );

// ── event receiver (called from cl_cgame.c trap handler) ─────────────

void WiredHud_ReceiveEvent( int type, const char *data ) {
	modernhudGlobalContext_t *ctx = CG_ModernHUDGetContext();
	if ( !data ) return;

	switch ( type ) {
		case WIRED_EVENT_CHAT: {
			int index = ctx->chat.index % ModernHUD_MAX_CHAT_LINES;
			Q_strncpyz( ctx->chat.line[index].message, data, MAX_SAY_TEXT );
			ctx->chat.line[index].time = wiredHud->time;
			ctx->chat.index++;
			break;
		}
		case WIRED_EVENT_TEAMCHAT: {
			int index = ctx->chat.index % ModernHUD_MAX_CHAT_LINES;
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

void WiredHud_Init( void ) {
	cl_drawHud = Cvar_Get( "cl_drawHud", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_drawHud, "Draw the Wired HUD. 0: off. 1: on (default)." );
	memset( &wired_hudStateStorage, 0, sizeof( wired_hudStateStorage ) );
	wiredHud_elementsLoaded = qfalse;
	Com_DPrintf( "WiredHud: initialized (Phase 3)\n" );
}

void WiredHud_Shutdown( void ) {
	WiredHud_DestroyAllElements();
	memset( &wired_hudStateStorage, 0, sizeof( wired_hudStateStorage ) );
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

static void WiredHud_ItemToConfig( const wiredItemDef_t *item, modernhudConfig_t *cfg ) {
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
	int i, j;
	int created = 0;
	int hudOverlayMenus = 0;

	Com_DPrintf( "WiredHud: scanning %d menus for hudOverlay...\n", menuCount );

	// iterate all loaded menus
	for ( i = 0; i < menuCount; i++ ) {
		wiredMenuDef_t *menu = WiredUI_GetMenuByIndex( i );
		if ( !menu ) continue;

		Com_DPrintf( "WiredHud: menu[%d] = '%s' hudOverlay=%d items=%d\n",
			i, menu->name, menu->hudOverlay, menu->itemCount );

		if ( !menu->hudOverlay ) {
			continue;
		}

		hudOverlayMenus++;
		Com_DPrintf( "WiredHud: found hudOverlay menu '%s' with %d items\n", menu->name, menu->itemCount );

		// this is a HUD overlay menu — create elements from its items
		for ( j = 0; j < menu->itemCount; j++ ) {
			wiredItemDef_t *item = menu->items[j];
			if ( !item->hudElement[0] ) continue;

			modernhudConfig_t cfg;
			WiredHud_ItemToConfig( item, &cfg );

			if ( WiredHud_CreateElement( item->hudElement, &cfg ) ) {
				created++;
			} else {
				Com_Printf( S_COLOR_YELLOW "WiredHud: failed to create '%s'\n", item->hudElement );
			}
		}
	}

	Com_DPrintf( "WiredHud: %d hudOverlay menus, %d elements created\n", hudOverlayMenus, created );
	wiredHud_elementsLoaded = qtrue;
}

// ── per-frame HUD rendering ─────────────────────────────────────────

void WiredHud_Routine( int realtime ) {
	if ( !wiredHud->valid ) return;
	if ( cl_drawHud && !cl_drawHud->integer ) return;

	// lazy element init — deferred from WiredUI_Init to avoid Z_CheckHeap crash
	if ( !wiredHud_elementsLoaded ) {
		int64_t _wld_t0 = Sys_Microseconds();
		WiredHud_LoadFromMenus();
		cl_prof.whud_load += (int)(Sys_Microseconds() - _wld_t0);
		wiredHud_elementsLoaded = qtrue;
		Com_DPrintf( "WiredHud: %d elements active\n", WiredHud_GetElementCount() );
	}

	// sync compat structs so element code sees cg.*/cgs.* patterns
	CL_PROF( whud_sync, WiredHud_SyncCompat() );

	// render all active HUD elements through ModernHUD lifecycle
	CL_PROF( whud_render, WiredHud_RenderElements() );

	// scoreboard overlay — select and render gametype-specific scoreboard menu
	{
		qboolean showSb = wiredHud->showScores || wiredHud->intermission || wiredHud->warmup > 0;
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

			if ( sbMenu ) {
				// set cvars for header text
				Cvar_Set( "wired_sb_gametype", wiredHud->gametypeName );

				// compute placement text (FFA modes only)
				if ( wiredHud->gametype < 5 ) {
					int i, myRank = 0, myScore = 0;
					for ( i = 0; i < wiredHud->numScores && i < WIRED_HUD_MAX_SCORES; i++ ) {
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

				WiredUI_RenderMenuOverlay( sbMenu, realtime );
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
