/*
===========================================================================
cl_wired_ui.c — Wired UI: unified menu/HUD system implementation

Phase 1 stub: establishes the integration points and symbol/element
registration framework. Actual menu parsing will be extracted from
code/ui/ui_shared.c in subsequent phases.
===========================================================================
*/

#include "client.h"
#include "cl_wired_ui.h"
#include "../ui/menudef.h"

#if FEAT_WIRED_UI

// ── symbol registry ───────────────────────────────────────────────────

#define WIRED_MAX_SYMBOLS  256

typedef struct {
	char                    name[64];
	wiredSymbolCallback_t   callback;
	void                   *userData;
	qboolean                active;
} wiredSymbol_t;

static wiredSymbol_t  wired_symbols[WIRED_MAX_SYMBOLS];
static int            wired_numSymbols = 0;

// ── element registry ──────────────────────────────────────────────────

#define WIRED_MAX_ELEMENTS  256

typedef struct {
	char                     name[64];
	wiredElementCreate_t     create;
	wiredElementRoutine_t    routine;
	wiredElementDestroy_t    destroy;
	qboolean                 active;
} wiredElement_t;

static wiredElement_t  wired_elements[WIRED_MAX_ELEMENTS];
static int             wired_numElements = 0;

// ── feeder registry ───────────────────────────────────────────────────

typedef struct {
	int                       feederID;
	wiredFeederCount_t        count;
	wiredFeederItemText_t     itemText;
	wiredFeederSelection_t    selection;
	qboolean                  active;
} wiredFeeder_t;

static wiredFeeder_t  wired_feeders[WIRED_MAX_FEEDERS];
static int            wired_numFeeders = 0;

void WiredUI_RegisterFeeder( int feederID, wiredFeederCount_t count,
                              wiredFeederItemText_t itemText,
                              wiredFeederSelection_t selection ) {
	int i;
	// update existing
	for ( i = 0; i < wired_numFeeders; i++ ) {
		if ( wired_feeders[i].active && wired_feeders[i].feederID == feederID ) {
			wired_feeders[i].count = count;
			wired_feeders[i].itemText = itemText;
			wired_feeders[i].selection = selection;
			return;
		}
	}
	if ( wired_numFeeders >= WIRED_MAX_FEEDERS ) return;
	wired_feeders[wired_numFeeders].feederID = feederID;
	wired_feeders[wired_numFeeders].count = count;
	wired_feeders[wired_numFeeders].itemText = itemText;
	wired_feeders[wired_numFeeders].selection = selection;
	wired_feeders[wired_numFeeders].active = qtrue;
	wired_numFeeders++;
}

int WiredUI_FeederCount( int feederID ) {
	int i;
	for ( i = 0; i < wired_numFeeders; i++ ) {
		if ( wired_feeders[i].active && wired_feeders[i].feederID == feederID && wired_feeders[i].count ) {
			return wired_feeders[i].count( feederID );
		}
	}
	return 0;
}

const char *WiredUI_FeederItemText( int feederID, int index, int column ) {
	int i;
	for ( i = 0; i < wired_numFeeders; i++ ) {
		if ( wired_feeders[i].active && wired_feeders[i].feederID == feederID && wired_feeders[i].itemText ) {
			return wired_feeders[i].itemText( feederID, index, column );
		}
	}
	return "";
}

void WiredUI_FeederSelection( int feederID, int index ) {
	int i;
	for ( i = 0; i < wired_numFeeders; i++ ) {
		if ( wired_feeders[i].active && wired_feeders[i].feederID == feederID && wired_feeders[i].selection ) {
			wired_feeders[i].selection( feederID, index );
			return;
		}
	}
}

// ── state ─────────────────────────────────────────────────────────────

static qboolean  wired_initialized = qfalse;
static int       wired_activeMenu = UIMENU_NONE;

// ── menu stack ────────────────────────────────────────────────────────
// Supports open/close navigation between screens (e.g., Main → Options → Video).
// Each entry is a menu name. ESC or "close" pops the stack.
// The bottom of the stack is always the root menu (main or ingame).

static char      wired_menuStack[WIRED_MENU_STACK_DEPTH][64];
static int       wired_menuStackDepth = 0;

// ── cursor ────────────────────────────────────────────────────────────

// ── key binding capture state ─────────────────────────────────────────
static qboolean  wired_waitingForKey = qfalse;
static wiredItemDef_t *wired_bindItem = NULL;

// ── text field editing state ──────────────────────────────────────────
static qboolean       wired_editingField = qfalse;
static wiredItemDef_t *wired_editItem = NULL;
static int            wired_editCursorPos = 0;
static int            wired_editPaintOffset = 0;

static float     wired_cursorX = 320.0f;
static float     wired_cursorY = 240.0f;
static int       wired_focusItem = -1;     // index of focused item
static qboolean  wired_focusFromMouse = qfalse;  // qtrue if focus came from mouse hover

// ── double-click detection ───────────────────────────────────────────
#define WIRED_DOUBLECLICK_TIME  300   // ms
static int       wired_lastClickTime = 0;
static int       wired_lastClickRow = -1;
static float     wired_lastClickFeeder = 0;

// ── symbol registration ───────────────────────────────────────────────

void WiredUI_RegisterSymbol( const char *name, wiredSymbolCallback_t callback, void *userData ) {
	int i;

	if ( !name || !name[0] || !callback ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterSymbol: invalid args\n" );
		return;
	}

	// check for existing symbol (update in place)
	for ( i = 0; i < wired_numSymbols; i++ ) {
		if ( wired_symbols[i].active && !Q_stricmp( wired_symbols[i].name, name ) ) {
			wired_symbols[i].callback = callback;
			wired_symbols[i].userData = userData;
			return;
		}
	}

	if ( wired_numSymbols >= WIRED_MAX_SYMBOLS ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterSymbol: too many symbols (max %d)\n", WIRED_MAX_SYMBOLS );
		return;
	}

	Q_strncpyz( wired_symbols[wired_numSymbols].name, name, sizeof( wired_symbols[0].name ) );
	wired_symbols[wired_numSymbols].callback = callback;
	wired_symbols[wired_numSymbols].userData = userData;
	wired_symbols[wired_numSymbols].active = qtrue;
	wired_numSymbols++;
}

void WiredUI_UnregisterSymbol( const char *name ) {
	int i;
	for ( i = 0; i < wired_numSymbols; i++ ) {
		if ( wired_symbols[i].active && !Q_stricmp( wired_symbols[i].name, name ) ) {
			wired_symbols[i].active = qfalse;
			return;
		}
	}
}

const char *WiredUI_ResolveSymbol( const char *name ) {
	int i;
	for ( i = 0; i < wired_numSymbols; i++ ) {
		if ( wired_symbols[i].active && !Q_stricmp( wired_symbols[i].name, name ) ) {
			return wired_symbols[i].callback( wired_symbols[i].userData );
		}
	}
	return "???";
}

// ── element registration ──────────────────────────────────────────────

void WiredUI_RegisterElement( const char *name,
                               wiredElementCreate_t create,
                               wiredElementRoutine_t routine,
                               wiredElementDestroy_t destroy ) {
	int i;

	if ( !name || !name[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterElement: invalid name\n" );
		return;
	}

	// check for existing element (update in place)
	for ( i = 0; i < wired_numElements; i++ ) {
		if ( wired_elements[i].active && !Q_stricmp( wired_elements[i].name, name ) ) {
			wired_elements[i].create = create;
			wired_elements[i].routine = routine;
			wired_elements[i].destroy = destroy;
			return;
		}
	}

	if ( wired_numElements >= WIRED_MAX_ELEMENTS ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterElement: too many elements (max %d)\n", WIRED_MAX_ELEMENTS );
		return;
	}

	Q_strncpyz( wired_elements[wired_numElements].name, name, sizeof( wired_elements[0].name ) );
	wired_elements[wired_numElements].create = create;
	wired_elements[wired_numElements].routine = routine;
	wired_elements[wired_numElements].destroy = destroy;
	wired_elements[wired_numElements].active = qtrue;
	wired_numElements++;
}

// ── batch registration stubs ──────────────────────────────────────────
// These will be filled in Phase 3 when SuperHUD elements are wrapped.

void WiredUI_RegisterCoreSymbols( void ) {
	Com_Printf( "WiredUI: core symbols registered (stub — Phase 3)\n" );
}

void WiredUI_RegisterCoreElements( void ) {
	Com_Printf( "WiredUI: core elements registered (stub — Phase 3)\n" );
}

// ── public API ────────────────────────────────────────────────────────

// ── delayed screenshot ────────────────────────────────────────────────
// +set wired_screenshotDelay N triggers screenshotJPEG after N seconds.
// Useful for automated testing: make run-game DEV=1 +set wired_screenshotDelay 5

static cvar_t *wired_screenshotDelay = NULL;
static int     wired_screenshotTime = 0;
static qboolean wired_screenshotTaken = qfalse;

void WiredUI_Init( qboolean inGameUI ) {
	Com_Printf( "------- WiredUI_Init -------\n" );

	Com_Memset( wired_symbols, 0, sizeof( wired_symbols ) );
	Com_Memset( wired_elements, 0, sizeof( wired_elements ) );
	wired_numSymbols = 0;
	wired_numElements = 0;

	// load menu files from manifest
	WiredUI_ClearMenus();
	WiredUI_LoadMenus( "ui/menus.txt" );

	// register feeder data sources
	WiredUI_RegisterCoreFeeders();

	// TODO Phase 3: register core symbols and elements from cgame
	// TODO Phase 4: register /hud_reload and /menu_reload commands

	wired_activeMenu = UIMENU_NONE;
	wired_initialized = qtrue;

	// delayed screenshot support
	wired_screenshotDelay = Cvar_Get( "wired_screenshotDelay", "0", 0 );
	wired_screenshotTime = cls.realtime;
	wired_screenshotTaken = qfalse;

	Com_Printf( "WiredUI: initialized (%d menus loaded)\n", WiredUI_GetMenuCount() );
}

void WiredUI_Shutdown( void ) {
	if ( !wired_initialized ) {
		return;
	}

	// TODO Phase 3: destroy all HUD element contexts

	Com_Memset( wired_symbols, 0, sizeof( wired_symbols ) );
	Com_Memset( wired_elements, 0, sizeof( wired_elements ) );
	wired_numSymbols = 0;
	wired_numElements = 0;
	wired_activeMenu = UIMENU_NONE;
	wired_initialized = qfalse;

	Com_Printf( "WiredUI: shutdown\n" );
}

void WiredUI_Refresh( int realtime ) {
	wiredMenuDef_t *menu;
	int i;

	if ( !wired_initialized ) {
		return;
	}

	// delayed screenshot — fire once after N seconds
	if ( wired_screenshotDelay && wired_screenshotDelay->integer > 0 && !wired_screenshotTaken ) {
		if ( realtime - wired_screenshotTime >= wired_screenshotDelay->integer * 1000 ) {
			Cbuf_ExecuteText( EXEC_APPEND, "screenshotJPEG\n" );
			wired_screenshotTaken = qtrue;
			Com_Printf( "WiredUI: delayed screenshot taken\n" );
		}
	}

	// find the active menu (top of stack, or root)
	menu = WiredUI_GetActiveMenu();

	if ( !menu ) {
		return;
	}

	// auto-refresh server pings when server browser is visible
	{
		wiredMenuDef_t *serverMenu = WiredUI_FindMenu( "servers" );
		if ( menu == serverMenu ) {
			static int lastPingUpdate = 0;
			if ( realtime - lastPingUpdate > 1000 ) {  // every second
				int uiSource = Cvar_VariableIntegerValue( "ui_netSource" );
				int engineSource;
				extern qboolean CL_UpdateVisiblePings_f( int source );

				// map UI source values to engine AS_* constants
				if ( uiSource == 0 )       engineSource = AS_LOCAL;
				else if ( uiSource == 6 )  engineSource = AS_FAVORITES;
				else                       engineSource = AS_GLOBAL;

				CL_UpdateVisiblePings_f( engineSource );
				lastPingUpdate = realtime;
			}
		}
	}

	// TODO: animated cloud background — deferred (tcmod scroll not animating in 2D path)
	// See TODOS.md "Wired UI: animated cloud menu background"

	// menu origin offset — non-fullscreen menus position items relative to rect
	float menuX = menu->fullscreen ? 0 : menu->rect.x;
	float menuY = menu->fullscreen ? 0 : menu->rect.y;
	float menuW = menu->fullscreen ? SCREEN_WIDTH : menu->rect.w;
	float menuH = menu->fullscreen ? SCREEN_HEIGHT : menu->rect.h;
	float scrollY = menu->scrollOffset;  // scroll offset applied to all items
	float clipTop = menuY;               // visible area top
	float clipBottom = menuY + menuH;    // visible area bottom

	// render background — SCR_FillRect takes 640x480 virtual coords
	{
		vec4_t bgColor;
		if ( menu->style == WINDOW_STYLE_FILLED && ( menu->backcolor[3] > 0.0f ) ) {
			Vector4Copy( menu->backcolor, bgColor );
		} else {
			bgColor[0] = 0.1f; bgColor[1] = 0.1f; bgColor[2] = 0.15f; bgColor[3] = 1.0f;
		}
		if ( menu->fullscreen ) {
			SCR_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, bgColor );
		} else {
			SCR_FillRect( menu->rect.x, menu->rect.y, menu->rect.w, menu->rect.h, bgColor );
		}
	}

	// render items — coordinates are relative to menu origin for non-fullscreen
	for ( i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *item = menu->items[i];

		if ( !item->visible ) {
			continue;
		}

		// cvarTest + showCvar/hideCvar conditional visibility
		if ( item->cvarTest[0] ) {
			char testBuf[256];
			Cvar_VariableStringBuffer( item->cvarTest, testBuf, sizeof( testBuf ) );

			if ( item->showCvar[0] ) {
				// showCvar: only visible if cvarTest value is in the list
				if ( !strstr( item->showCvar, testBuf ) ) continue;
			}
			if ( item->hideCvar[0] ) {
				// hideCvar: hidden if cvarTest value is in the list
				if ( strstr( item->hideCvar, testBuf ) ) continue;
			}
		}

		// compute absolute item position (menu origin + scroll offset)
		float itemX = menuX + item->rect.x;
		float itemY = menuY + item->rect.y - scrollY;

		// clip items outside visible area
		if ( itemY + item->rect.h < clipTop || itemY > clipBottom ) {
			continue;
		}

		// draw item background if styled
		if ( item->style == WINDOW_STYLE_FILLED && item->backcolor[3] > 0.0f ) {
			SCR_FillRect( itemX, itemY, item->rect.w, item->rect.h, item->backcolor );
		}

		// draw LISTBOX items — feeder-driven scrollable list
		if ( item->type == ITEM_TYPE_LISTBOX && item->feeder != 0 ) {
			int feederID = (int)item->feeder;
			int totalRows = WiredUI_FeederCount( feederID );
			float rowH = item->elementheight > 0 ? item->elementheight : 16.0f;
			int visibleRows = (int)( item->rect.h / rowH );
			int row, col;
			float charSize = item->textscale >= 0.7f ? 16.0f : 8.0f;
			vec4_t selColor = { 0.3f, 0.3f, 0.5f, 0.6f };
			vec4_t rowColor;
			float scrollBarW = 4.0f;
			float contentW = item->rect.w;

			// reserve space for scrollbar when content overflows
			if ( totalRows > visibleRows ) {
				contentW -= scrollBarW + 2.0f;
			}

			// draw list background
			if ( item->backcolor[3] > 0 ) {
				SCR_FillRect( itemX, itemY, item->rect.w, item->rect.h, item->backcolor );
			}

			// draw rows
			for ( row = 0; row < visibleRows && ( item->listScrollOffset + row ) < totalRows; row++ ) {
				int dataRow = item->listScrollOffset + row;
				float rowY = itemY + row * rowH;

				// highlight selected row
				if ( dataRow == item->listSelectedRow ) {
					SCR_FillRect( itemX, rowY, contentW, rowH, selColor );
				}

				// draw columns — clip text to column width
				Vector4Copy( item->forecolor, rowColor );
				float colX = itemX + 4;
				for ( col = 0; col < ( item->columns > 0 ? item->columns : 1 ); col++ ) {
					const char *text = WiredUI_FeederItemText( feederID, dataRow, col );
					float colW = ( col < item->columns && item->columnWidths[col] > 0 )
						? item->columnWidths[col] : contentW;
					if ( text && text[0] ) {
						// truncate to fit column width
						int maxChars = (int)( ( colW - 4 ) / charSize );
						if ( maxChars < 1 ) maxChars = 1;
						if ( (int)strlen( text ) > maxChars ) {
							char clipped[128];
							Q_strncpyz( clipped, text, sizeof( clipped ) );
							if ( maxChars < (int)sizeof( clipped ) ) {
								clipped[maxChars] = '\0';
							}
							SCR_DrawStringExt( (int)colX, (int)( rowY + 2 ), charSize,
								clipped, rowColor, qfalse, qfalse );
						} else {
							SCR_DrawStringExt( (int)colX, (int)( rowY + 2 ), charSize,
								text, rowColor, qfalse, qfalse );
						}
					}
					colX += colW;
				}
			}

			// listbox scrollbar — macOS-style with fade
			if ( totalRows > visibleRows ) {
				float trackX = itemX + item->rect.w - scrollBarW - 1.0f;
				float trackY = itemY + 1.0f;
				float trackH = item->rect.h - 2.0f;
				float visibleFrac = (float)visibleRows / (float)totalRows;
				float thumbH = trackH * visibleFrac;
				float maxScroll = (float)( totalRows - visibleRows );
				float thumbY;
				float alpha = 0.0f;

				if ( thumbH < 16.0f ) thumbH = 16.0f;
				thumbY = trackY;
				if ( maxScroll > 0 ) {
					thumbY += ( trackH - thumbH ) * ( (float)item->listScrollOffset / maxScroll );
				}

				// fade out after 1.5s of inactivity (same as menu scrollbar)
				if ( item->listScrollFadeTime > 0 ) {
					int elapsed = realtime - item->listScrollFadeTime;
					if ( elapsed < 1500 ) {
						alpha = 1.0f;
					} else {
						alpha = 1.0f - (float)( elapsed - 1500 ) / 500.0f;
						if ( alpha < 0 ) alpha = 0;
					}
				}

				if ( alpha > 0 ) {
					vec4_t trackColor = { 0.3f, 0.3f, 0.3f, 0.15f * alpha };
					vec4_t thumbColor = { 0.7f, 0.7f, 0.7f, 0.5f * alpha };
					SCR_FillRect( trackX, trackY, scrollBarW, trackH, trackColor );
					SCR_FillRect( trackX, thumbY, scrollBarW, thumbH, thumbColor );
				}
			}

			continue; // skip normal text rendering for listbox
		}

		// draw cvar-bound item value (right side of label)
		if ( item->cvar[0] && item->type != ITEM_TYPE_TEXT && item->type != ITEM_TYPE_BUTTON ) {
			char cvarBuf[256];
			const char *valueText = "";
			float labelX = itemX + item->textalignx;
			float labelY = itemY + item->textaligny;
			float charSize = item->textscale >= 0.7f ? 16.0f : 8.0f;
			float valueX;

			// draw the label on the left
			if ( item->text[0] ) {
				SCR_DrawStringExt( (int)labelX, (int)labelY, charSize, item->text,
					item->forecolor, qfalse, qfalse );
			}

			// compute value text based on item type
			Cvar_VariableStringBuffer( item->cvar, cvarBuf, sizeof( cvarBuf ) );

			switch ( item->type ) {
				case ITEM_TYPE_YESNO:
					valueText = atof( cvarBuf ) != 0 ? "Yes" : "No";
					break;

				case ITEM_TYPE_MULTI:
					if ( item->multiData ) {
						int j;
						for ( j = 0; j < item->multiData->count; j++ ) {
							if ( item->multiData->isStringList ) {
								if ( !Q_stricmp( cvarBuf, item->multiData->strValues[j] ) ) {
									valueText = item->multiData->labels[j];
									break;
								}
							} else {
								if ( item->multiData->floatValues[j] == atof( cvarBuf ) ) {
									valueText = item->multiData->labels[j];
									break;
								}
							}
						}
					}
					break;

				case ITEM_TYPE_SLIDER:
					{
						float val = atof( cvarBuf );
						float range = item->sliderData.maxVal - item->sliderData.minVal;
						float frac = ( range > 0 ) ? ( val - item->sliderData.minVal ) / range : 0;
						float barX = itemX + item->rect.w * 0.5f;
						float barW = item->rect.w * 0.45f;
						float barY = itemY + item->rect.h * 0.4f;
						float barH = 4.0f;
						vec4_t barBg = { 0.3f, 0.3f, 0.3f, 0.6f };
						vec4_t barFg = { 1.0f, 0.75f, 0.0f, 1.0f };

						if ( frac < 0 ) frac = 0;
						if ( frac > 1 ) frac = 1;

						// draw slider track
						SCR_FillRect( barX, barY, barW, barH, barBg );
						// draw slider fill
						SCR_FillRect( barX, barY, barW * frac, barH, barFg );
						// draw value as text
						valueText = va( "%.1f", val );
					}
					break;

				case ITEM_TYPE_BIND:
					{
						int k;
						if ( wired_waitingForKey && wired_bindItem == item ) {
							valueText = "Press a key...";
						} else {
							// find which key is bound to this command
							valueText = "---";
							for ( k = 0; k < MAX_KEYS; k++ ) {
								const char *b = Key_GetBinding( k );
								if ( b && !Q_stricmp( b, item->cvar ) ) {
									valueText = Key_KeynumToString( k );
									break;
								}
							}
						}
					}
					break;

				case ITEM_TYPE_EDITFIELD:
				case ITEM_TYPE_NUMERICFIELD:
					if ( wired_editingField && wired_editItem == item ) {
						// show value with blinking cursor
						static char editBuf[512];
						int curPos = wired_editCursorPos;
						qboolean showCursor = ( (int)( cls.realtime / 250 ) & 1 );

						if ( curPos > (int)strlen( cvarBuf ) ) curPos = strlen( cvarBuf );
						Q_strncpyz( editBuf, cvarBuf, curPos + 1 );
						if ( showCursor ) Q_strcat( editBuf, sizeof(editBuf), "_" );
						else Q_strcat( editBuf, sizeof(editBuf), " " );
						Q_strcat( editBuf, sizeof(editBuf), &cvarBuf[curPos] );
						valueText = editBuf;
					} else {
						valueText = cvarBuf;
					}
					break;

				default:
					valueText = cvarBuf;
					break;
			}

			// draw value text on the right side
			if ( valueText[0] ) {
				float textLen = strlen( item->text[0] ? item->text : "" ) * charSize;
				valueX = itemX + textLen + 16;
				if ( valueX < itemX + item->rect.w * 0.5f ) {
					valueX = itemX + item->rect.w * 0.5f;
				}
				SCR_DrawStringExt( (int)valueX, (int)labelY, charSize, valueText,
					item->forecolor, qfalse, qfalse );
			}
		}
		// draw text-only items (no cvar)
		else if ( item->text[0] ) {
			float x = itemX + item->textalignx;
			float y = itemY + item->textaligny;
			float charSize = item->textscale >= 0.7f ? 16.0f : 8.0f;

			// adjust for text alignment within rect
			if ( item->textalign == ITEM_ALIGN_CENTER && item->rect.w > 0 ) {
				float textWidth = strlen( item->text ) * charSize;
				x = itemX + ( item->rect.w - textWidth ) * 0.5f;
			} else if ( item->textalign == ITEM_ALIGN_RIGHT && item->rect.w > 0 ) {
				float textWidth = strlen( item->text ) * charSize;
				x = itemX + item->rect.w - textWidth;
			}

			SCR_DrawStringExt( (int)x, (int)y, charSize, item->text,
				item->forecolor, qfalse, qfalse );
		}
	}

	// draw focus highlight on hovered/selected item
	if ( wired_focusItem >= 0 && wired_focusItem < menu->itemCount ) {
		wiredItemDef_t *focus = menu->items[wired_focusItem];
		if ( focus->visible && !focus->decoration ) {
			float fx = menuX + focus->rect.x;
			float fy = menuY + focus->rect.y - scrollY;
			// don't draw focus highlight outside clip area
			if ( fy + focus->rect.h < clipTop || fy > clipBottom ) goto skip_focus;
			SCR_FillRect( fx, fy, focus->rect.w, focus->rect.h, menu->focuscolor );
			// redraw the focused item's text on top of highlight
			if ( focus->text[0] ) {
				float x = fx + focus->textalignx;
				float y = fy + focus->textaligny;
				float charSize = focus->textscale >= 0.7f ? 16.0f : 8.0f;
				if ( focus->textalign == ITEM_ALIGN_CENTER && focus->rect.w > 0 ) {
					float textWidth = strlen( focus->text ) * charSize;
					x = fx + ( focus->rect.w - textWidth ) * 0.5f;
				}
				SCR_DrawStringExt( (int)x, (int)y, charSize, focus->text,
					focus->forecolor, qfalse, qfalse );
			}
		}
	}
skip_focus:

	// draw tooltip for mouse-hovered item only (ET:Legacy + QL)
	// keyboard focus does NOT show tooltips — they anchor to the cursor
	if ( wired_focusFromMouse && wired_focusItem >= 0 && wired_focusItem < menu->itemCount ) {
		wiredItemDef_t *focus = menu->items[wired_focusItem];
		if ( focus->tooltip[0] ) {
			float tx = wired_cursorX + 16;
			float ty = wired_cursorY + 16;
			float tw = strlen( focus->tooltip ) * 8.0f + 8;
			float th = 16.0f;
			vec4_t tipBg = { 0.0f, 0.0f, 0.0f, 0.85f };
			vec4_t tipFg = { 1.0f, 1.0f, 1.0f, 1.0f };

			// keep tooltip on screen
			if ( tx + tw > SCREEN_WIDTH ) tx = SCREEN_WIDTH - tw;
			if ( ty + th > SCREEN_HEIGHT ) ty = wired_cursorY - th - 4;

			SCR_FillRect( tx, ty, tw, th, tipBg );
			SCR_DrawStringExt( (int)(tx + 4), (int)(ty + 4), 8.0f,
				focus->tooltip, tipFg, qfalse, qfalse );
		}
	}

	// draw cursor (32x32 centered on cursor position, same as q3_ui and v6)
	if ( Key_GetCatcher() & KEYCATCH_UI ) {
		vec4_t cursorColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		// draw a simple crosshair cursor using filled rects
		// TODO Phase 2: use a cursor shader image like ui_assets/cursor
		re.SetColor( cursorColor );
		SCR_FillRect( wired_cursorX - 1, wired_cursorY - 8, 2, 16, cursorColor );
		SCR_FillRect( wired_cursorX - 8, wired_cursorY - 1, 16, 2, cursorColor );
		re.SetColor( NULL );
	}

	// macOS-style scrollbar — thin, semi-transparent, fades out
	{
		float maxScroll = menu->contentHeight - menuH;
		if ( maxScroll > 0 ) {
			float scrollBarWidth = 4.0f;
			float scrollBarPadding = 2.0f;
			float trackX = menuX + menuW - scrollBarWidth - scrollBarPadding;
			float trackY = menuY + scrollBarPadding;
			float trackH = menuH - scrollBarPadding * 2;

			// thumb size proportional to visible/content ratio
			float visibleFrac = menuH / menu->contentHeight;
			float thumbH = trackH * visibleFrac;
			if ( thumbH < 20.0f ) thumbH = 20.0f;
			float thumbY = trackY + ( trackH - thumbH ) * ( scrollY / maxScroll );

			// fade out after 1.5 seconds of inactivity
			float alpha = 1.0f;
			if ( menu->scrollBarFadeTime > 0 ) {
				int elapsed = realtime - menu->scrollBarFadeTime;
				if ( elapsed > 1500 ) {
					alpha = 1.0f - ( elapsed - 1500 ) / 500.0f;
					if ( alpha < 0 ) alpha = 0;
				}
			} else {
				alpha = 0; // never scrolled — don't show
			}

			if ( alpha > 0 ) {
				vec4_t trackColor = { 0.3f, 0.3f, 0.3f, 0.15f * alpha };
				vec4_t thumbColor = { 0.7f, 0.7f, 0.7f, 0.5f * alpha };

				SCR_FillRect( trackX, trackY, scrollBarWidth, trackH, trackColor );
				SCR_FillRect( trackX, thumbY, scrollBarWidth, thumbH, thumbColor );
			}
		}
	}

	// TODO Phase 2: full rendering with borders, models, etc.
}

// ── script command system ─────────────────────────────────────────────
//
// Based on q3now's ui_script.c command table pattern. Same handler
// signature (name + numArgs + args) but adapted for Wired UI structs.
//
// Key difference from v6: unknown commands pass to the engine console
// via Cbuf_ExecuteText instead of being silently dropped. This means
// any console command works as a script action without hardcoding.

#define WIRED_MAX_SCRIPT_ARGS  8

// forward declarations
static wiredItemDef_t *WiredUI_FindItemByName( wiredMenuDef_t *menu, const char *name );

typedef void (*wiredScriptHandler_t)( wiredMenuDef_t *menu, wiredItemDef_t *item,
                                       int numArgs, const char **args );

typedef struct {
	const char            *name;
	wiredScriptHandler_t   handler;
} wiredScriptCommand_t;

// ── script handlers ───────────────────────────────────────────────────

static void WiredScript_Show( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) target->visible = qtrue;
}

static void WiredScript_Hide( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) target->visible = qfalse;
}

static void WiredScript_Open( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs < 1 ) return;
	WiredUI_PushMenu( args[0] );
}

static void WiredScript_Close( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	WiredUI_PopMenu();
}

static void WiredScript_SetCvar( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 2 ) {
		Cvar_Set( args[0], args[1] );
	}
}

static void WiredScript_Exec( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		Cbuf_ExecuteText( EXEC_APPEND, va( "%s\n", args[0] ) );
	}
}

static void WiredScript_Play( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		sfxHandle_t sfx = S_RegisterSound( args[0], qfalse );
		if ( sfx ) S_StartLocalSound( sfx, CHAN_LOCAL_SOUND );
	}
}

static void WiredScript_PlayLooped( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		S_StartBackgroundTrack( args[0], args[0] );
	}
}

static void WiredScript_StopMusic( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	S_StopBackgroundTrack();
}

static void WiredScript_FadeIn( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) {
		target->visible = qtrue;
		// TODO Phase 4: animate alpha ramp
	}
}

static void WiredScript_FadeOut( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) {
		// TODO Phase 4: animate alpha ramp, then set visible = qfalse
	}
}

static void WiredScript_SetFocus( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int i;
	if ( numArgs < 1 ) return;
	for ( i = 0; i < menu->itemCount; i++ ) {
		if ( !Q_stricmp( menu->items[i]->name, args[0] ) ) {
			wired_focusItem = i;
			break;
		}
	}
}

// ── game action handlers ──────────────────────────────────────────────
// These read cvars set by feeder selection callbacks and execute real game actions.

static void WiredScript_StartServer( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char mapName[MAX_QPATH];

	Cvar_VariableStringBuffer( "ui_selectedMap", mapName, sizeof( mapName ) );
	if ( !mapName[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no map selected\n" );
		return;
	}

	// apply server cvars from menu selections
	// note: fraglimit, timelimit, sv_maxclients, sv_pure, sv_allowDownload
	// are already set directly by cvar-bound menu items
	Cvar_Set( "g_gametype", Cvar_VariableString( "ui_netGameType" ) );
	Cvar_Set( "dedicated", "0" );  // listen server from menu

	WiredUI_CloseAllMenus();

	// wait allows dedicated cvar to take effect before map load
	Cbuf_ExecuteText( EXEC_APPEND, va( "wait ; wait ; map %s\n", mapName ) );
}

static void WiredScript_JoinServer( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char addr[256];

	Cvar_VariableStringBuffer( "ui_selectedServerAddr", addr, sizeof( addr ) );
	if ( !addr[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no server selected\n" );
		return;
	}

	WiredUI_CloseAllMenus();
	Cbuf_ExecuteText( EXEC_APPEND, va( "connect %s\n", addr ) );
}

static void WiredScript_RunDemo( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char demoName[MAX_QPATH];

	Cvar_VariableStringBuffer( "ui_selectedDemo", demoName, sizeof( demoName ) );
	if ( !demoName[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no demo selected\n" );
		return;
	}

	WiredUI_CloseAllMenus();
	Cbuf_ExecuteText( EXEC_APPEND, va( "demo %s\n", demoName ) );
}

static void WiredScript_RunMod( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char modName[MAX_QPATH];

	Cvar_VariableStringBuffer( "ui_selectedMod", modName, sizeof( modName ) );
	if ( !modName[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no mod selected\n" );
		return;
	}

	WiredUI_CloseAllMenus();

	// "baseq3" means return to base game — clear fs_game
	if ( Q_stricmp( modName, "baseq3" ) == 0 ) {
		Cvar_Set( "fs_game", "" );
	} else {
		Cvar_Set( "fs_game", modName );
	}
	Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n" );
}

static void WiredScript_RefreshServers( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int source = Cvar_VariableIntegerValue( "ui_netSource" );

	// always query local servers too — they appear instantly
	Cbuf_ExecuteText( EXEC_APPEND, "localservers\n" );

	if ( source > 0 && source < 6 ) {
		// query internet master servers
		Cbuf_ExecuteText( EXEC_APPEND, va( "globalservers %d %d\n", source - 1, DEFAULT_PROTOCOL_VERSION ) );
	}

}

// command table — matches q3now's ui_script.c set, plus Wired UI additions
static const wiredScriptCommand_t wiredScriptCommands[] = {
	{ "show",         WiredScript_Show },
	{ "hide",         WiredScript_Hide },
	{ "open",         WiredScript_Open },
	{ "close",        WiredScript_Close },
	{ "setcvar",      WiredScript_SetCvar },
	{ "exec",         WiredScript_Exec },
	{ "play",         WiredScript_Play },
	{ "playlooped",   WiredScript_PlayLooped },
	{ "stopmusic",    WiredScript_StopMusic },
	{ "fadein",       WiredScript_FadeIn },
	{ "fadeout",      WiredScript_FadeOut },
	{ "setfocus",     WiredScript_SetFocus },
	// ── game action commands (uiScript equivalents) ──────────────────
	// These use cvars set by feeder selection callbacks.
	{ "startserver",  WiredScript_StartServer },
	{ "joinserver",   WiredScript_JoinServer },
	{ "rundemo",      WiredScript_RunDemo },
	{ "runmod",       WiredScript_RunMod },
	{ "refreshservers", WiredScript_RefreshServers },

	// TODO: setcolor, setitemcolor, transition (from q3now ui_script.c)
	{ NULL, NULL }
};

// ── script runner ─────────────────────────────────────────────────────
// Based on q3now's UI_RunScript (ui_script.c:282). Same tokenization:
// semicolon-separated commands, quoted string args, up to 8 args each.
// KEY DIFFERENCE: unknown commands pass to engine console instead of
// being silently dropped.

static void WiredUI_RunScript( wiredMenuDef_t *menu, wiredItemDef_t *item, const char *script ) {
	char        token[MAX_STRING_CHARS];
	const char *args[WIRED_MAX_SCRIPT_ARGS];
	static char argBuf[WIRED_MAX_SCRIPT_ARGS][256];
	int         numArgs, i;
	const char *p;
	qboolean    handled;

	if ( !script || !script[0] ) return;

	p = script;

	while ( *p ) {
		// skip whitespace and semicolons
		while ( *p == ' ' || *p == '\t' || *p == ';' ) p++;
		if ( !*p ) break;

		// read command name
		i = 0;
		while ( *p && *p != ' ' && *p != '\t' && *p != ';' && i < (int)sizeof(token) - 1 ) {
			token[i++] = *p++;
		}
		token[i] = '\0';
		if ( !token[0] ) break;

		// read arguments (up to 8, stop at semicolon)
		numArgs = 0;
		while ( numArgs < WIRED_MAX_SCRIPT_ARGS ) {
			while ( *p == ' ' || *p == '\t' ) p++;
			if ( !*p || *p == ';' ) break;

			i = 0;
			if ( *p == '"' ) {
				p++;
				while ( *p && *p != '"' && i < 255 ) argBuf[numArgs][i++] = *p++;
				if ( *p == '"' ) p++;
			} else {
				while ( *p && *p != ' ' && *p != '\t' && *p != ';' && i < 255 ) argBuf[numArgs][i++] = *p++;
			}
			argBuf[numArgs][i] = '\0';
			args[numArgs] = argBuf[numArgs];
			numArgs++;
		}

		// dispatch to command table
		handled = qfalse;
		for ( i = 0; wiredScriptCommands[i].name; i++ ) {
			if ( !Q_stricmp( token, wiredScriptCommands[i].name ) ) {
				wiredScriptCommands[i].handler( menu, item, numArgs, args );
				handled = qtrue;
				break;
			}
		}

		// Wired UI design decision: unknown commands pass to engine console.
		// This means any cvar or console command works as a script action
		// without needing to be hardcoded in the command table.
		if ( !handled ) {
			char cmdBuf[1024];
			Com_sprintf( cmdBuf, sizeof(cmdBuf), "%s", token );
			for ( i = 0; i < numArgs; i++ ) {
				Q_strcat( cmdBuf, sizeof(cmdBuf), va( " \"%s\"", args[i] ) );
			}
			Q_strcat( cmdBuf, sizeof(cmdBuf), "\n" );
			Cbuf_ExecuteText( EXEC_APPEND, cmdBuf );
		}
	}
}

// ── menu stack ────────────────────────────────────────────────────────

void WiredUI_PushMenu( const char *name ) {
	if ( !name || !name[0] ) return;

	// check if menu exists
	if ( !WiredUI_FindMenu( name ) ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: cannot open menu '%s' — not found (loaded %d menus)\n", name, WiredUI_GetMenuCount() );
		return;
	}

	if ( wired_menuStackDepth >= WIRED_MENU_STACK_DEPTH ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: menu stack overflow (max %d)\n", WIRED_MENU_STACK_DEPTH );
		return;
	}

	Q_strncpyz( wired_menuStack[wired_menuStackDepth], name, sizeof( wired_menuStack[0] ) );
	wired_menuStackDepth++;
	wired_focusItem = -1;
	wired_focusFromMouse = qfalse;

	Com_DPrintf( "WiredUI: push menu '%s' (depth %d)\n", name, wired_menuStackDepth );

	// run onOpen script if present
	{
		wiredMenuDef_t *opened = WiredUI_FindMenu( name );
		if ( opened && opened->onOpen[0] ) {
			WiredUI_RunScript( opened, NULL, opened->onOpen );
		}
	}
}

void WiredUI_PopMenu( void ) {
	if ( wired_menuStackDepth <= 0 ) {
		// nothing to pop — close UI entirely
		wired_activeMenu = UIMENU_NONE;
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		Cvar_Set( "cl_paused", "0" );
		return;
	}

	wired_menuStackDepth--;
	wired_focusItem = -1;
	wired_focusFromMouse = qfalse;

	if ( wired_menuStackDepth <= 0 ) {
		// stack empty — return to root menu behavior
		if ( wired_activeMenu == UIMENU_INGAME ) {
			// close in-game menu entirely
			wired_activeMenu = UIMENU_NONE;
			Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
			Cvar_Set( "cl_paused", "0" );
		}
		// for UIMENU_MAIN, the root stays visible (can't close the main menu)
	}

	Com_DPrintf( "WiredUI: pop menu (depth %d)\n", wired_menuStackDepth );
}

void WiredUI_CloseAllMenus( void ) {
	wired_menuStackDepth = 0;
	wired_focusItem = -1;
	wired_activeMenu = UIMENU_NONE;
	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
	Cvar_Set( "cl_paused", "0" );
}

// ── helper functions ──────────────────────────────────────────────────

wiredMenuDef_t *WiredUI_GetActiveMenu( void ) {
	// if there's a menu on the stack, show that
	if ( wired_menuStackDepth > 0 ) {
		return WiredUI_FindMenu( wired_menuStack[wired_menuStackDepth - 1] );
	}
	// otherwise show the root menu based on wired_activeMenu
	if ( wired_activeMenu == UIMENU_MAIN )   return WiredUI_FindMenu( "main" );
	if ( wired_activeMenu == UIMENU_INGAME ) return WiredUI_FindMenu( "ingame" );
	return NULL;
}

static wiredItemDef_t *WiredUI_FindItemByName( wiredMenuDef_t *menu, const char *name ) {
	int i;
	if ( !menu || !name ) return NULL;
	for ( i = 0; i < menu->itemCount; i++ ) {
		if ( !Q_stricmp( menu->items[i]->name, name ) ) {
			return menu->items[i];
		}
	}
	return NULL;
}

static qboolean WiredUI_PointInRect( float px, float py, wiredRect_t *r ) {
	return ( px >= r->x && px < r->x + r->w &&
	         py >= r->y && py < r->y + r->h );
}

static int WiredUI_FindItemAtCursor( wiredMenuDef_t *menu, float cx, float cy ) {
	int i;
	float ox = menu->fullscreen ? 0 : menu->rect.x;
	float oy = menu->fullscreen ? 0 : menu->rect.y;
	float sy = menu->scrollOffset;

	// iterate back-to-front so topmost item wins
	for ( i = menu->itemCount - 1; i >= 0; i-- ) {
		wiredItemDef_t *item = menu->items[i];
		wiredRect_t absRect;
		if ( !item->visible || item->decoration ) continue;
		absRect.x = ox + item->rect.x;
		absRect.y = oy + item->rect.y - sy;
		absRect.w = item->rect.w;
		absRect.h = item->rect.h;

		// skip items scrolled out of view
		float clipTop = menu->fullscreen ? 0 : menu->rect.y;
		float clipBottom = clipTop + ( menu->fullscreen ? SCREEN_HEIGHT : menu->rect.h );
		if ( absRect.y + absRect.h < clipTop || absRect.y > clipBottom ) continue;

		if ( WiredUI_PointInRect( cx, cy, &absRect ) ) return i;
	}
	return -1;
}

// ── key event ─────────────────────────────────────────────────────────
// Combines: v6 Menu_HandleKey structure, q3now q3_ui key dispatch,
// and ET:Legacy per-item events (onEsc, onEnter, onTab, execKey).

void WiredUI_KeyEvent( int key, qboolean down ) {
	wiredMenuDef_t *menu;
	wiredItemDef_t *focusedItem = NULL;
	int i;

	if ( !wired_initialized ) return;

	// text field editing mode — intercepts all keys while editing
	if ( wired_editingField && wired_editItem && down ) {
		char buff[1024];
		int len;

		Cvar_VariableStringBuffer( wired_editItem->cvar, buff, sizeof( buff ) );
		len = strlen( buff );

		if ( key & K_CHAR_FLAG ) {
			// character input
			int ch = key & ~K_CHAR_FLAG;

			if ( ch == 'h' - 'a' + 1 ) {
				// ctrl-h = backspace
				if ( wired_editCursorPos > 0 ) {
					memmove( &buff[wired_editCursorPos - 1], &buff[wired_editCursorPos], len + 1 - wired_editCursorPos );
					wired_editCursorPos--;
				}
				Cvar_Set( wired_editItem->cvar, buff );
			} else if ( ch >= 32 ) {
				// printable character — insert at cursor
				int maxC = wired_editItem->maxChars > 0 ? wired_editItem->maxChars : 255;
				if ( wired_editItem->type == ITEM_TYPE_NUMERICFIELD && ( ch < '0' || ch > '9' ) && ch != '.' && ch != '-' ) {
					// reject non-numeric
				} else if ( len < maxC ) {
					memmove( &buff[wired_editCursorPos + 1], &buff[wired_editCursorPos], len + 1 - wired_editCursorPos );
					buff[wired_editCursorPos] = ch;
					wired_editCursorPos++;
					Cvar_Set( wired_editItem->cvar, buff );
				}
			}
			return;
		}

		// non-character keys
		switch ( key ) {
			case K_ESCAPE:
				wired_editingField = qfalse;
				wired_editItem = NULL;
				return;

			case K_ENTER:
			case K_KP_ENTER:
				wired_editingField = qfalse;
				wired_editItem = NULL;
				return;

			case K_TAB:
				wired_editingField = qfalse;
				wired_editItem = NULL;
				// fall through to normal key handling for focus change
				break;

			case K_BACKSPACE:
				if ( wired_editCursorPos > 0 ) {
					memmove( &buff[wired_editCursorPos - 1], &buff[wired_editCursorPos], len + 1 - wired_editCursorPos );
					wired_editCursorPos--;
					Cvar_Set( wired_editItem->cvar, buff );
				}
				return;

			case K_DEL:
			case K_KP_DEL:
				if ( wired_editCursorPos < len ) {
					memmove( &buff[wired_editCursorPos], &buff[wired_editCursorPos + 1], len - wired_editCursorPos );
					Cvar_Set( wired_editItem->cvar, buff );
				}
				return;

			case K_LEFTARROW:
			case K_KP_LEFTARROW:
				if ( wired_editCursorPos > 0 ) wired_editCursorPos--;
				return;

			case K_RIGHTARROW:
			case K_KP_RIGHTARROW:
				if ( wired_editCursorPos < len ) wired_editCursorPos++;
				return;

			case K_HOME:
			case K_KP_HOME:
				wired_editCursorPos = 0;
				return;

			case K_END:
			case K_KP_END:
				wired_editCursorPos = len;
				return;

			default:
				return; // eat all other keys while editing
		}
	}

	// key binding capture mode — waiting for user to press a key
	if ( wired_waitingForKey && down ) {
		if ( key == K_ESCAPE ) {
			// cancel binding
			wired_waitingForKey = qfalse;
			wired_bindItem = NULL;
		} else if ( wired_bindItem && wired_bindItem->cvar[0] ) {
			// bind the key to the command
			Key_SetBinding( key, wired_bindItem->cvar );
			wired_waitingForKey = qfalse;
			wired_bindItem = NULL;
		}
		return;
	}

	menu = WiredUI_GetActiveMenu();
	if ( !menu ) return;

	if ( wired_focusItem >= 0 && wired_focusItem < menu->itemCount ) {
		focusedItem = menu->items[wired_focusItem];
	}

	// only process key-down for most actions
	if ( !down ) return;

	// ET:Legacy execKey: check ALL items for key-specific bindings
	for ( i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *item = menu->items[i];
		if ( item->execKeyCode && item->execKeyCode == key && item->execKeyAction[0] ) {
			WiredUI_RunScript( menu, item, item->execKeyAction );
			return;
		}
	}

	// ET:Legacy per-item events: onEsc, onEnter, onTab
	if ( focusedItem ) {
		if ( key == K_ESCAPE && focusedItem->onEsc[0] ) {
			WiredUI_RunScript( menu, focusedItem, focusedItem->onEsc );
			return;
		}
		if ( ( key == K_ENTER || key == K_KP_ENTER ) && focusedItem->onEnter[0] ) {
			WiredUI_RunScript( menu, focusedItem, focusedItem->onEnter );
			return;
		}
		if ( key == K_TAB && focusedItem->onTab[0] ) {
			WiredUI_RunScript( menu, focusedItem, focusedItem->onTab );
			return;
		}
	}

	// default key handling
	switch ( key ) {
		case K_ESCAPE:
			if ( menu->onESC[0] ) {
				WiredUI_RunScript( menu, NULL, menu->onESC );
			} else {
				// no onESC handler — pop the menu stack
				WiredUI_PopMenu();
			}
			break;

		case K_MOUSE1:
		case K_MOUSE2:
		case K_ENTER:
		case K_KP_ENTER:
			if ( !focusedItem ) {
				break;
			}

			// EDITFIELD items: click to start editing
			if ( ( focusedItem->type == ITEM_TYPE_EDITFIELD || focusedItem->type == ITEM_TYPE_NUMERICFIELD )
			     && focusedItem->cvar[0] ) {
				char buf[256];
				wired_editingField = qtrue;
				wired_editItem = focusedItem;
				Cvar_VariableStringBuffer( focusedItem->cvar, buf, sizeof( buf ) );
				wired_editCursorPos = strlen( buf );
				wired_editPaintOffset = 0;
				break;
			}

			// LISTBOX items: click to select row
			if ( focusedItem->type == ITEM_TYPE_LISTBOX && focusedItem->feeder != 0 ) {
				float rowH = focusedItem->elementheight > 0 ? focusedItem->elementheight : 16.0f;
				float menuOY = menu->fullscreen ? 0 : menu->rect.y;
				float scrollOff = menu->scrollOffset;
				float listAbsY = menuOY + focusedItem->rect.y - scrollOff;
				int clickedRow = (int)( ( wired_cursorY - listAbsY ) / rowH ) + focusedItem->listScrollOffset;
				int total = WiredUI_FeederCount( (int)focusedItem->feeder );

				if ( clickedRow >= 0 && clickedRow < total ) {
					focusedItem->listSelectedRow = clickedRow;
					WiredUI_FeederSelection( (int)focusedItem->feeder, clickedRow );

					// double-click detection: same row + same feeder within threshold
					if ( focusedItem->doubleClick[0] &&
					     clickedRow == wired_lastClickRow &&
					     focusedItem->feeder == wired_lastClickFeeder &&
					     ( cls.realtime - wired_lastClickTime ) < WIRED_DOUBLECLICK_TIME ) {
						WiredUI_RunScript( menu, focusedItem, focusedItem->doubleClick );
						wired_lastClickTime = 0;  // consumed
					} else {
						wired_lastClickTime = cls.realtime;
						wired_lastClickRow = clickedRow;
						wired_lastClickFeeder = focusedItem->feeder;
					}
				}
				if ( focusedItem->action[0] && ( key == K_MOUSE1 || key == K_ENTER || key == K_KP_ENTER ) ) {
					WiredUI_RunScript( menu, focusedItem, focusedItem->action );
				}
				break;
			}

			// BIND items: start key capture mode
			if ( focusedItem->type == ITEM_TYPE_BIND && focusedItem->cvar[0] ) {
				wired_waitingForKey = qtrue;
				wired_bindItem = focusedItem;
				break;
			}

			// cvar-bound items: handle interaction based on type
			if ( focusedItem->cvar[0] ) {
				char cvarBuf[256];
				Cvar_VariableStringBuffer( focusedItem->cvar, cvarBuf, sizeof( cvarBuf ) );

				switch ( focusedItem->type ) {
					case ITEM_TYPE_YESNO:
						// toggle 0 <-> 1
						Cvar_Set( focusedItem->cvar, atof( cvarBuf ) != 0 ? "0" : "1" );
						break;

					case ITEM_TYPE_MULTI:
						if ( focusedItem->multiData ) {
							int cur = -1, next, j;
							int dir = ( key == K_MOUSE2 ) ? -1 : 1;
							// find current value index
							for ( j = 0; j < focusedItem->multiData->count; j++ ) {
								if ( focusedItem->multiData->isStringList ) {
									if ( !Q_stricmp( cvarBuf, focusedItem->multiData->strValues[j] ) ) {
										cur = j; break;
									}
								} else {
									if ( focusedItem->multiData->floatValues[j] == atof( cvarBuf ) ) {
										cur = j; break;
									}
								}
							}
							// cycle to next
							next = ( cur + dir + focusedItem->multiData->count ) % focusedItem->multiData->count;
							if ( focusedItem->multiData->isStringList ) {
								Cvar_Set( focusedItem->cvar, focusedItem->multiData->strValues[next] );
							} else {
								Cvar_Set( focusedItem->cvar,
									va( "%g", focusedItem->multiData->floatValues[next] ) );
							}
						}
						break;

					case ITEM_TYPE_SLIDER:
						{
							float val = atof( cvarBuf );
							float step = ( focusedItem->sliderData.maxVal - focusedItem->sliderData.minVal ) / 20.0f;
							if ( step < 0.01f ) step = 0.01f;
							if ( key == K_MOUSE2 ) step = -step;
							val += step;
							if ( val < focusedItem->sliderData.minVal ) val = focusedItem->sliderData.minVal;
							if ( val > focusedItem->sliderData.maxVal ) val = focusedItem->sliderData.maxVal;
							Cvar_Set( focusedItem->cvar, va( "%g", val ) );
						}
						break;

					default:
						break;
				}
			}

			// always run action script if present
			if ( focusedItem->action[0] ) {
				WiredUI_RunScript( menu, focusedItem, focusedItem->action );
			}
			break;

		case K_MWHEELUP:
			if ( focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX ) {
				// scroll listbox
				if ( focusedItem->listScrollOffset > 0 ) focusedItem->listScrollOffset -= 3;
				if ( focusedItem->listScrollOffset < 0 ) focusedItem->listScrollOffset = 0;
				focusedItem->listScrollFadeTime = cls.realtime;
			} else {
				// scroll the menu itself (Wired UI: HTML DIV-style scroll)
				float maxScroll = menu->contentHeight - menu->rect.h;
				if ( maxScroll > 0 ) {
					menu->scrollOffset -= 24.0f;
					if ( menu->scrollOffset < 0 ) menu->scrollOffset = 0;
					menu->scrollBarFadeTime = cls.realtime;
				}
			}
			break;

		case K_MWHEELDOWN:
			if ( focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX && focusedItem->feeder != 0 ) {
				// scroll listbox
				int total = WiredUI_FeederCount( (int)focusedItem->feeder );
				float rowH = focusedItem->elementheight > 0 ? focusedItem->elementheight : 16.0f;
				int visibleRows = (int)( focusedItem->rect.h / rowH );
				focusedItem->listScrollOffset += 3;
				if ( focusedItem->listScrollOffset > total - visibleRows )
					focusedItem->listScrollOffset = total - visibleRows;
				if ( focusedItem->listScrollOffset < 0 ) focusedItem->listScrollOffset = 0;
				focusedItem->listScrollFadeTime = cls.realtime;
			} else {
				// scroll the menu itself
				float maxScroll = menu->contentHeight - menu->rect.h;
				if ( maxScroll > 0 ) {
					menu->scrollOffset += 24.0f;
					if ( menu->scrollOffset > maxScroll ) menu->scrollOffset = maxScroll;
					menu->scrollBarFadeTime = cls.realtime;
				}
			}
			break;

		case K_LEFTARROW:
		case K_KP_LEFTARROW:
		case K_RIGHTARROW:
		case K_KP_RIGHTARROW:
			// left/right adjusts sliders and cycles multi items
			if ( focusedItem && focusedItem->cvar[0] ) {
				char cvarBuf[256];
				int dir = ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) ? -1 : 1;
				Cvar_VariableStringBuffer( focusedItem->cvar, cvarBuf, sizeof( cvarBuf ) );

				if ( focusedItem->type == ITEM_TYPE_SLIDER ) {
					float val = atof( cvarBuf );
					float step = ( focusedItem->sliderData.maxVal - focusedItem->sliderData.minVal ) / 20.0f;
					if ( step < 0.01f ) step = 0.01f;
					val += step * dir;
					if ( val < focusedItem->sliderData.minVal ) val = focusedItem->sliderData.minVal;
					if ( val > focusedItem->sliderData.maxVal ) val = focusedItem->sliderData.maxVal;
					Cvar_Set( focusedItem->cvar, va( "%g", val ) );
				}
				else if ( focusedItem->type == ITEM_TYPE_MULTI && focusedItem->multiData ) {
					int cur = -1, next, j;
					for ( j = 0; j < focusedItem->multiData->count; j++ ) {
						if ( focusedItem->multiData->isStringList ) {
							if ( !Q_stricmp( cvarBuf, focusedItem->multiData->strValues[j] ) ) { cur = j; break; }
						} else {
							if ( focusedItem->multiData->floatValues[j] == atof( cvarBuf ) ) { cur = j; break; }
						}
					}
					next = ( cur + dir + focusedItem->multiData->count ) % focusedItem->multiData->count;
					if ( focusedItem->multiData->isStringList ) {
						Cvar_Set( focusedItem->cvar, focusedItem->multiData->strValues[next] );
					} else {
						Cvar_Set( focusedItem->cvar, va( "%g", focusedItem->multiData->floatValues[next] ) );
					}
				}
				else if ( focusedItem->type == ITEM_TYPE_YESNO ) {
					Cvar_Set( focusedItem->cvar, atof( cvarBuf ) != 0 ? "0" : "1" );
				}
			}
			break;

		case K_UPARROW:
		case K_KP_UPARROW:
			{
				int start = ( wired_focusItem > 0 ) ? wired_focusItem - 1 : menu->itemCount - 1;
				for ( i = 0; i < menu->itemCount; i++ ) {
					int idx = ( start - i + menu->itemCount ) % menu->itemCount;
					if ( menu->items[idx]->visible && !menu->items[idx]->decoration ) {
						wired_focusItem = idx;
						wired_focusFromMouse = qfalse;
						break;
					}
				}
			}
			break;

		case K_TAB:
		case K_DOWNARROW:
		case K_KP_DOWNARROW:
			{
				int start = wired_focusItem + 1;
				for ( i = 0; i < menu->itemCount; i++ ) {
					int idx = ( start + i ) % menu->itemCount;
					if ( menu->items[idx]->visible && !menu->items[idx]->decoration ) {
						wired_focusItem = idx;
						wired_focusFromMouse = qfalse;
						break;
					}
				}
			}
			break;
	}
}

// ── mouse event ───────────────────────────────────────────────────────
// Accumulates deltas into virtual 640x480 cursor, updates focus item,
// fires mouseEnter/mouseExit scripts (ET:Legacy per-item events).

void WiredUI_MouseEvent( int dx, int dy ) {
	wiredMenuDef_t *menu;
	int oldFocus, newFocus;

	if ( !wired_initialized ) return;

	// accumulate deltas into virtual 640x480 cursor position
	wired_cursorX += dx;
	if ( wired_cursorX < 0 ) wired_cursorX = 0;
	else if ( wired_cursorX > SCREEN_WIDTH ) wired_cursorX = SCREEN_WIDTH;

	wired_cursorY += dy;
	if ( wired_cursorY < 0 ) wired_cursorY = 0;
	else if ( wired_cursorY > SCREEN_HEIGHT ) wired_cursorY = SCREEN_HEIGHT;

	menu = WiredUI_GetActiveMenu();
	if ( !menu ) return;

	oldFocus = wired_focusItem;
	newFocus = WiredUI_FindItemAtCursor( menu, wired_cursorX, wired_cursorY );

	// any mouse movement reactivates mouse-based focus
	wired_focusFromMouse = ( newFocus >= 0 );

	// fire mouseExit on old item, mouseEnter on new item
	if ( newFocus != oldFocus ) {
		if ( oldFocus >= 0 && oldFocus < menu->itemCount ) {
			wiredItemDef_t *old = menu->items[oldFocus];
			if ( old->mouseExit[0] ) {
				WiredUI_RunScript( menu, old, old->mouseExit );
			}
			if ( old->leaveFocus[0] ) {
				WiredUI_RunScript( menu, old, old->leaveFocus );
			}
		}
		if ( newFocus >= 0 && newFocus < menu->itemCount ) {
			wiredItemDef_t *cur = menu->items[newFocus];
			if ( cur->mouseEnter[0] ) {
				WiredUI_RunScript( menu, cur, cur->mouseEnter );
			}
			if ( cur->onFocus[0] ) {
				WiredUI_RunScript( menu, cur, cur->onFocus );
			}
		}
		wired_focusItem = newFocus;
	}
}

void WiredUI_SetActiveMenu( int menu ) {
	if ( !wired_initialized ) {
		return;
	}

	wired_activeMenu = menu;

	if ( menu != UIMENU_NONE ) {
		// activate UI key catcher so the engine routes input and draw calls to us
		Key_SetCatcher( KEYCATCH_UI );
		Com_DPrintf( "WiredUI: SetActiveMenu %d\n", menu );
	} else {
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
	}
}

qboolean WiredUI_IsFullscreen( void ) {
	if ( !wired_initialized ) {
		return qfalse;
	}

	// main menu is always "fullscreen" for the engine — prevents 3D scene rendering
	// at CA_DISCONNECTED. Wired UI draws its own background (clouds) in Refresh.
	return ( wired_activeMenu == UIMENU_MAIN );
}

qboolean WiredUI_ConsoleCommand( int realtime ) {
	if ( !wired_initialized ) {
		return qfalse;
	}

	// TODO Phase 2: handle UI console commands
	return qfalse;
}

void WiredUI_DrawConnectScreen( qboolean overlay ) {
	if ( !wired_initialized ) {
		return;
	}

	// TODO Phase 2: render connection screen from .menu file
}

void WiredUI_ReloadHud( void ) {
	Com_Printf( "WiredUI: HUD reload (stub — Phase 4)\n" );
	// TODO Phase 4: reparse .hud files, atomic swap
}

void WiredUI_ReloadMenus( void ) {
	Com_Printf( "WiredUI: menu reload (stub — Phase 4)\n" );
	// TODO Phase 4: reparse .menu files
}

#endif // FEAT_WIRED_UI
