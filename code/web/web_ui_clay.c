// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
 * Portable Clay consumer for the browser authored-content boundary.
 *
 * Native Wired UI and W0 now share the same vendored layout engine. The
 * browser still consumes an AOT, bounded representation because LuaJIT has no
 * wasm32 backend, but geometry is no longer calculated by an independent
 * rectangle compositor.
 */

#include "web_ui_clay.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#define CLAY_IMPLEMENTATION
#include "clay.h"

#define WIRED_WEB_CLAY_TEXT_BYTES 256

enum {
	WIRED_WEB_CLAY_FONT_DISPLAY = 1,
	WIRED_WEB_CLAY_FONT_UI,
	WIRED_WEB_CLAY_FONT_MONO
};

static void *s_clayMemory;
static uint32_t s_clayMemoryBytes;
static qboolean s_clayReady;
static qboolean s_clayRendered;
static int s_clayErrors;
static Clay_ErrorType s_clayLastError;
static double s_serverLayoutSamples[128];
static int s_serverLayoutSampleCount;
static int s_serverLayoutSampleCursor;

static int WiredWebClay_Font( uint16_t fontId ) {
	switch ( fontId ) {
	case WIRED_WEB_CLAY_FONT_DISPLAY: return FONT_DISPLAY_BOLD;
	case WIRED_WEB_CLAY_FONT_MONO: return FONT_MONO;
	default: return FONT_UI_MEDIUM;
	}
}

static Clay_String WiredWebClay_String( const char *text ) {
	Clay_String value;
	value.isStaticallyAllocated = false;
	value.length = text ? (int32_t)strlen( text ) : 0;
	value.chars = text ? text : "";
	return value;
}

static Clay_Color WiredWebClay_Color( const vec4_t source ) {
	return (Clay_Color){ source[0] * 255.0f, source[1] * 255.0f,
		source[2] * 255.0f, source[3] * 255.0f };
}

static void WiredWebClay_Error( Clay_ErrorData error ) {
	s_clayLastError = error.errorType;
	++s_clayErrors;
}

static double WiredWebClay_NowMillis( void ) {
#ifdef __EMSCRIPTEN__
	return emscripten_get_now();
#else
	return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
#endif
}

static Clay_Dimensions WiredWebClay_Measure( Clay_StringSlice text,
		Clay_TextElementConfig *config, void *userData ) {
	char bounded[WIRED_WEB_CLAY_TEXT_BYTES];
	int length;
	Clay_Dimensions measured = { 0.0f, 0.0f };
	(void)userData;
	if ( !config || !text.chars || text.length <= 0 ) return measured;
	length = text.length;
	if ( length >= (int)sizeof( bounded ) ) length = (int)sizeof( bounded ) - 1;
	memcpy( bounded, text.chars, (size_t)length );
	bounded[length] = '\0';
	measured.width = Text_Measure( bounded, WiredWebClay_Font( config->fontId ),
		(float)config->fontSize );
	measured.height = config->lineHeight ? (float)config->lineHeight
		: (float)config->fontSize * 1.20f;
	return measured;
}

qboolean WiredWebClay_Init( void ) {
	Clay_Arena arena;
	Clay_ErrorHandler errors = { WiredWebClay_Error, NULL };
	Clay_Dimensions dimensions = { 1280.0f, 720.0f };
	if ( s_clayReady ) return qtrue;
	/* W0 carries a bounded AOT root, not the native 87-file live catalog.
	 * Right-size Clay before asking for its arena: the 8192/16384 native
	 * defaults waste several MiB in the browser and can fail after the game
	 * modules/content have occupied the initial wasm heap. */
	Clay_SetMaxElementCount( 512 );
	Clay_SetMaxMeasureTextCacheWordCount( 2048 );
	s_clayMemoryBytes = Clay_MinMemorySize();
	s_clayMemory = malloc( s_clayMemoryBytes );
	if ( !s_clayMemory ) return qfalse;
	arena = Clay_CreateArenaWithCapacityAndMemory( s_clayMemoryBytes, s_clayMemory );
	if ( !Clay_Initialize( arena, dimensions, errors ) ) {
		free( s_clayMemory );
		s_clayMemory = NULL;
		s_clayMemoryBytes = 0;
		return qfalse;
	}
	Clay_SetMeasureTextFunction( WiredWebClay_Measure, NULL );
	s_clayReady = qtrue;
	return qtrue;
}

void WiredWebClay_Shutdown( void ) {
	free( s_clayMemory );
	s_clayMemory = NULL;
	s_clayMemoryBytes = 0;
	s_clayReady = qfalse;
	s_clayRendered = qfalse;
	s_clayErrors = 0;
	s_serverLayoutSampleCount = 0;
	s_serverLayoutSampleCursor = 0;
}

uint32_t WiredWebClay_Receipt( void ) {
	uint32_t receipt = s_clayReady ? 0x1u : 0u;
	if ( s_clayRendered ) receipt |= 0x2u;
	if ( s_clayErrors ) receipt |= ( (uint32_t)s_clayLastError + 1u ) << 8;
	return receipt;
}

uint32_t WiredWebClay_ServerLayoutP99Micros( void ) {
	double sorted[128], value;
	int count = s_serverLayoutSampleCount;
	int i, j, index;
	if ( count < 16 ) return 0u;
	if ( count > 128 ) count = 128;
	memcpy( sorted, s_serverLayoutSamples, (size_t)count * sizeof( sorted[0] ) );
	for ( i = 1; i < count; ++i ) {
		value = sorted[i];
		for ( j = i; j > 0 && sorted[j - 1] > value; --j ) sorted[j] = sorted[j - 1];
		sorted[j] = value;
	}
	index = ( count * 99 + 99 ) / 100 - 1;
	if ( index < 0 ) index = 0;
	return (uint32_t)( sorted[index] * 1000.0 + 0.5 );
}

static void WiredWebClay_Text( const char *text, uint16_t font, uint16_t size,
		Clay_Color color ) {
	CLAY_TEXT( WiredWebClay_String( text ), CLAY_TEXT_CONFIG({
		.textColor = color,
		.fontId = font,
		.fontSize = size,
		.wrapMode = CLAY_TEXT_WRAP_NONE
	}) );
}

static void WiredWebClay_DrawBackdropRect( float x, float y, float width, float height,
		const vec4_t color ) {
	if ( width <= 0.0f || height <= 0.0f || !re.SetColor || !re.DrawMenuBackdrop ) return;
	re.SetColor( color );
	re.DrawMenuBackdrop( x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f );
}

static void WiredWebClay_DrawDemoBackdrop( const wiredWebAuthoredCatalog_t *catalog,
		float width, float height ) {
	vec4_t base, sky, glow, floor, vignette;
	Vector4Copy( catalog->palette.ink, base );
	Vector4Copy( catalog->palette.accentDim, sky );
	Vector4Copy( catalog->palette.accentSoft, glow );
	Vector4Copy( catalog->palette.accentDim, floor );
	vignette[0] = vignette[1] = vignette[2] = 0.0f; vignette[3] = 0.56f;
	base[3] = 1.0f; sky[3] = 0.48f; glow[3] = 0.14f; floor[3] = 0.24f;
	/* main.wui's layered/demo material is compiled to a bounded painter's-order
	 * backdrop. The opaque base intentionally rejects the unfinished world
	 * material as menu chrome; subsequent theme-token layers provide the same
	 * dark/amber depth hierarchy on every RAL backend. */
	WiredWebClay_DrawBackdropRect( 0, 0, width, height, base );
	WiredWebClay_DrawBackdropRect( 0, 0, width, height * 0.58f, sky );
	WiredWebClay_DrawBackdropRect( width * 0.24f, height * 0.08f,
		width * 0.58f, height * 0.74f, glow );
	WiredWebClay_DrawBackdropRect( 0, height * 0.64f, width, height * 0.36f, floor );
	WiredWebClay_DrawBackdropRect( 0, 0, width * 0.07f, height, vignette );
	WiredWebClay_DrawBackdropRect( width * 0.93f, 0, width * 0.07f, height, vignette );
	if ( re.SetColor ) re.SetColor( NULL );
}

static void WiredWebClay_EmitMenu( const wiredWebAuthoredCatalog_t *catalog,
		float width, float height, int focusedItem ) {
	Clay_Color ink = WiredWebClay_Color( catalog->palette.ink );
	const Clay_Color panel = WiredWebClay_Color( catalog->palette.panel );
	const Clay_Color line = WiredWebClay_Color( catalog->palette.line );
	const Clay_Color accent = WiredWebClay_Color( catalog->palette.accent );
	const Clay_Color bone = WiredWebClay_Color( catalog->palette.bone );
	const Clay_Color boneDim = WiredWebClay_Color( catalog->palette.boneDim );
	uint16_t left = (uint16_t)( width * catalog->leftXPercent / 100.0f );
	uint16_t rightInset = (uint16_t)( width * catalog->rightInsetPercent / 100.0f );
	uint16_t top = (uint16_t)( height * catalog->rightYPercent / 100.0f );
	float leftWidth = width * catalog->leftWidthPercent / 100.0f;
	float rightWidth = width * catalog->rightWidthPercent / 100.0f;
	float titleHeight = height * 0.28f;
	float itemHeight = height * 0.075f;
	int i;
	ink.a *= 0.55f; /* main.wui: backdrop dim */

	Clay_BeginLayout();
	CLAY({
		.id = CLAY_ID( "web_main_root" ),
		.layout = {
			.sizing = { CLAY_SIZING_FIXED( width ), CLAY_SIZING_FIXED( height ) },
			.padding = { left, rightInset, top, (uint16_t)( height * 0.026f ) },
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		},
		.backgroundColor = ink
	}) {
		CLAY({
			.id = CLAY_ID( "web_main_left" ),
			.layout = {
				.sizing = { CLAY_SIZING_FIXED( leftWidth ), CLAY_SIZING_GROW( 0 ) },
				.childGap = (uint16_t)( height * 0.008f ),
				.layoutDirection = CLAY_TOP_TO_BOTTOM
			}
		}) {
			CLAY({
				.id = CLAY_ID( "web_main_brand" ),
				.layout = {
					.sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_FIXED( titleHeight ) },
					.padding = CLAY_PADDING_ALL( (uint16_t)( width * 0.008f ) ),
					.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
					.layoutDirection = CLAY_TOP_TO_BOTTOM
				}
			}) {
				WiredWebClay_Text( "QUAKE", WIRED_WEB_CLAY_FONT_DISPLAY,
					(uint16_t)( height * 0.115f ), bone );
				WiredWebClay_Text( "WIRED", WIRED_WEB_CLAY_FONT_DISPLAY,
					(uint16_t)( height * 0.115f ), accent );
			}
			CLAY({ .layout = {
				.sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_FIXED( height * 0.0533f ) }
			} }) {}
			for ( i = 0; i < catalog->itemCount; ++i ) {
				CLAY({
					.id = CLAY_SIDI( WiredWebClay_String( catalog->items[i].id ), i ),
					.layout = {
						.sizing = { CLAY_SIZING_PERCENT( 1.0f ),
							CLAY_SIZING_FIXED( itemHeight ) },
						.padding = { (uint16_t)( width * 0.008f ), 0,
							(uint16_t)( height * 0.008f ), 0 },
						.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
						.layoutDirection = CLAY_LEFT_TO_RIGHT
					},
					.backgroundColor = i == focusedItem
						? (Clay_Color){ accent.r, accent.g, accent.b, accent.a * 0.30f }
						: (Clay_Color){ 0, 0, 0, 0 },
					.border = { .color = line, .width = { .bottom = 1 } }
				}) {
					WiredWebClay_Text( i == focusedItem ? ">" : "",
						WIRED_WEB_CLAY_FONT_DISPLAY, (uint16_t)( height * 0.030f ), bone );
					WiredWebClay_Text( catalog->items[i].label,
						WIRED_WEB_CLAY_FONT_UI, (uint16_t)( height * 0.030f ), bone );
					CLAY({ .layout = { .sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_FIT( 0 ) } } }) {}
					WiredWebClay_Text( catalog->items[i].subtitle,
						WIRED_WEB_CLAY_FONT_MONO, (uint16_t)( height * 0.014f ), boneDim );
				}
			}
		}
		CLAY({ .layout = { .sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_GROW( 0 ) } } }) {}
		CLAY({
			.id = CLAY_ID( "web_main_right" ),
			.layout = {
				.sizing = { CLAY_SIZING_FIXED( rightWidth ), CLAY_SIZING_GROW( 0 ) },
				.childGap = (uint16_t)( height * 0.014f ),
				.layoutDirection = CLAY_TOP_TO_BOTTOM
			}
		}) {
			for ( i = 0; i < catalog->cardCount; ++i ) {
				const wiredWebAuthoredCard_t *card = &catalog->cards[i];
				int line;
				CLAY({
					.id = CLAY_SIDI( WiredWebClay_String( card->id ), i ),
					.layout = {
						.sizing = { CLAY_SIZING_GROW( 0 ),
							CLAY_SIZING_FIXED( height * card->heightPercent / 100.0f ) },
						.padding = { (uint16_t)( rightWidth * 0.075f ),
							(uint16_t)( rightWidth * 0.04f ),
							(uint16_t)( height * 0.018f ),
							(uint16_t)( height * 0.012f ) },
						.childGap = (uint16_t)( height * 0.006f ),
						.layoutDirection = CLAY_TOP_TO_BOTTOM
					},
					.backgroundColor = panel,
					.border = { .color = accent, .width = { .left = 5 } }
				}) {
					for ( line = 0; line < card->lineCount; ++line ) {
						const char *text = card->lines[line];
						if ( card->lineBindings[line][0] ) {
							const char *bound = Cvar_VariableString( card->lineBindings[line] );
							if ( bound && bound[0] ) text = bound;
						}
						WiredWebClay_Text( text,
							line == 0 ? WIRED_WEB_CLAY_FONT_MONO : WIRED_WEB_CLAY_FONT_UI,
							(uint16_t)( height * ( line == 0 ? 0.016f : 0.014f ) ),
							line == 0 ? accent : bone );
					}
				}
			}
		}
	}
}

static void WiredWebClay_EmitServerBrowser( const wiredWebAuthoredCatalog_t *catalog,
		float width, float height, const wiredWebServerRow_t *rows, int rowCount,
		int scrollIndex, int selectedIndex, int *firstVisible, int *visibleCount ) {
	const wiredWebAuthoredServerBrowser_t *browser = &catalog->serverBrowser;
	const Clay_Color ink = WiredWebClay_Color( catalog->palette.ink );
	const Clay_Color panel = WiredWebClay_Color( catalog->palette.panel );
	const Clay_Color line = WiredWebClay_Color( catalog->palette.line );
	const Clay_Color accent = WiredWebClay_Color( catalog->palette.accent );
	const Clay_Color bone = WiredWebClay_Color( catalog->palette.bone );
	const Clay_Color boneDim = WiredWebClay_Color( catalog->palette.boneDim );
	float contentWidth = width * 0.90f;
	float listHeight = height * 0.58f;
	float rowHeight = browser->rowHeight * height / 720.0f;
	int count = rowHeight > 0.0f ? (int)( listHeight / rowHeight ) : 0;
	int row, column;
	if ( count < 1 ) count = 1;
	if ( count > 24 ) count = 24;
	if ( scrollIndex < 0 ) scrollIndex = 0;
	if ( scrollIndex > rowCount - count ) scrollIndex = MAX( 0, rowCount - count );
	*firstVisible = scrollIndex;
	*visibleCount = MIN( count, rowCount - scrollIndex );

	Clay_BeginLayout();
	CLAY({
		.id = CLAY_ID( "web_servers_root" ),
		.layout = {
			.sizing = { CLAY_SIZING_FIXED( width ), CLAY_SIZING_FIXED( height ) },
			.padding = { (uint16_t)( height * 0.04f ), (uint16_t)( width * 0.05f ),
				(uint16_t)( height * 0.035f ), (uint16_t)( width * 0.05f ) },
			.childGap = (uint16_t)( height * 0.012f ),
			.layoutDirection = CLAY_TOP_TO_BOTTOM
		},
		.backgroundColor = ink
	}) {
		CLAY({ .layout = {
			.sizing = { CLAY_SIZING_FIXED( contentWidth ), CLAY_SIZING_FIXED( height * 0.075f ) },
			.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		}, .border = { .color = accent, .width = { .bottom = 2 } } }) {
			WiredWebClay_Text( "SERVER BROWSER", WIRED_WEB_CLAY_FONT_DISPLAY,
				(uint16_t)( height * 0.042f ), accent );
			CLAY({ .layout = { .sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_FIT( 0 ) } } }) {}
			WiredWebClay_Text( "200 SERVERS",
				WIRED_WEB_CLAY_FONT_MONO, (uint16_t)( height * 0.016f ), boneDim );
		}
		CLAY({ .layout = {
			.sizing = { CLAY_SIZING_FIXED( contentWidth ), CLAY_SIZING_FIXED( height * 0.06f ) },
			.padding = CLAY_PADDING_ALL( (uint16_t)( height * 0.012f ) ),
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		}, .backgroundColor = panel }) {
			CLAY({ .layout = { .sizing = { CLAY_SIZING_PERCENT( 0.24f ), CLAY_SIZING_GROW( 0 ) },
				.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER } } }) {
				WiredWebClay_Text( "SOURCE: INTERNET", WIRED_WEB_CLAY_FONT_UI,
					(uint16_t)( height * 0.018f ), bone );
			}
			CLAY({ .layout = { .sizing = { CLAY_SIZING_PERCENT( 0.22f ), CLAY_SIZING_GROW( 0 ) },
				.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER } } }) {
				WiredWebClay_Text( "GAME TYPE: ALL", WIRED_WEB_CLAY_FONT_UI,
					(uint16_t)( height * 0.018f ), bone );
			}
			CLAY({ .layout = { .sizing = { CLAY_SIZING_PERCENT( 0.54f ), CLAY_SIZING_GROW( 0 ) },
				.childAlignment = { CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER } } }) {
				WiredWebClay_Text( "FULL   EMPTY   PING", WIRED_WEB_CLAY_FONT_UI,
					(uint16_t)( height * 0.018f ), boneDim );
			}
		}
		CLAY({ .layout = {
			.sizing = { CLAY_SIZING_FIXED( contentWidth ), CLAY_SIZING_FIXED( height * 0.042f ) },
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		}, .backgroundColor = panel, .border = { .color = line, .width = { .bottom = 1 } } }) {
			for ( column = 0; column < browser->columnCount; ++column ) {
				CLAY({ .layout = { .sizing = {
					CLAY_SIZING_FIXED( contentWidth * browser->columns[column].widthPercent / 100.0f ),
					CLAY_SIZING_GROW( 0 ) }, .padding = { 8, 4, 0, 0 },
					.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER } } }) {
					WiredWebClay_Text( browser->columns[column].title, WIRED_WEB_CLAY_FONT_MONO,
						(uint16_t)( height * 0.014f ), accent );
				}
			}
		}
		CLAY({ .id = CLAY_ID( "web_server_list" ), .layout = {
			.sizing = { CLAY_SIZING_FIXED( contentWidth ), CLAY_SIZING_FIXED( listHeight ) },
			.layoutDirection = CLAY_TOP_TO_BOTTOM
		}, .backgroundColor = panel, .border = { .color = line, .width = { .left = 1, .right = 1, .bottom = 1 } } }) {
			for ( row = 0; row < *visibleCount; ++row ) {
				int index = *firstVisible + row;
				CLAY({ .id = CLAY_SIDI( WiredWebClay_String( "web_server_row" ), index ),
					.layout = { .sizing = { CLAY_SIZING_FIXED( contentWidth ),
						CLAY_SIZING_FIXED( rowHeight ) }, .layoutDirection = CLAY_LEFT_TO_RIGHT },
					.backgroundColor = index == selectedIndex
						? (Clay_Color){ accent.r, accent.g, accent.b, accent.a * 0.28f }
						: ( row & 1 ? panel : (Clay_Color){ 0, 0, 0, 0 } ),
					.border = { .color = line, .width = { .bottom = 1 } } }) {
					for ( column = 0; column < browser->columnCount; ++column ) {
						CLAY({ .layout = { .sizing = {
							CLAY_SIZING_FIXED( contentWidth * browser->columns[column].widthPercent / 100.0f ),
							CLAY_SIZING_GROW( 0 ) }, .padding = { 8, 4, 0, 0 },
							.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER } } }) {
							WiredWebClay_Text( rows[index].fields[column],
								column == 0 ? WIRED_WEB_CLAY_FONT_UI : WIRED_WEB_CLAY_FONT_MONO,
								(uint16_t)( height * 0.015f ), column == 0 ? bone : boneDim );
						}
					}
				}
			}
		}
		CLAY({ .layout = { .sizing = { CLAY_SIZING_FIXED( contentWidth ), CLAY_SIZING_GROW( 0 ) },
			.childAlignment = { CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_LEFT_TO_RIGHT } }) {
			CLAY({ .layout = { .sizing = { CLAY_SIZING_FIXED( width * 0.13f ), CLAY_SIZING_GROW( 0 ) },
				.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER } } }) {
				WiredWebClay_Text( "ESC  CANCEL", WIRED_WEB_CLAY_FONT_MONO,
					(uint16_t)( height * 0.016f ), boneDim );
			}
			CLAY({ .layout = { .sizing = { CLAY_SIZING_FIXED( width * 0.15f ), CLAY_SIZING_GROW( 0 ) },
				.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER } } }) {
				WiredWebClay_Text( "ENTER  CONNECT", WIRED_WEB_CLAY_FONT_MONO,
					(uint16_t)( height * 0.016f ), accent );
			}
		}
	}
}

static float WiredWebClay_Clamp01( float value ) {
	if ( value < 0.0f ) return 0.0f;
	if ( value > 1.0f ) return 1.0f;
	return value;
}

static void WiredWebClay_EmitLoading( const wiredWebAuthoredCatalog_t *catalog,
		float width, float height, const wiredWebLoadingState_t *state ) {
	const wiredWebAuthoredLoading_t *loading = &catalog->loading;
	const Clay_Color ink = WiredWebClay_Color( catalog->palette.ink );
	const Clay_Color panel = WiredWebClay_Color( catalog->palette.panel );
	const Clay_Color line = WiredWebClay_Color( catalog->palette.line );
	const Clay_Color accent = WiredWebClay_Color( catalog->palette.accent );
	const Clay_Color bone = WiredWebClay_Color( catalog->palette.bone );
	const Clay_Color boneDim = WiredWebClay_Color( catalog->palette.boneDim );
	const char *labels[] = { "GEOMETRY", "SHADERS", "AUDIO", "DOWNLOAD" };
	const float values[] = { state->geometry, state->shaders, state->audio, state->download };
	char percentages[4][16], overallPercentage[16];
	float topHeight = height * loading->topBarHeightPercent / 100.0f;
	float bottomHeight = height * loading->bottomHeightPercent / 100.0f;
	float bodyHeight = height - topHeight - bottomHeight;
	float leftWidth = width * loading->leftWidthPercent / 100.0f;
	float dividerWidth = width * loading->dividerWidthPercent / 100.0f;
	float rightWidth = width - leftWidth - dividerWidth;
	int i;
	for ( i = 0; i < 4; ++i )
		snprintf( percentages[i], sizeof( percentages[i] ), "%3d%%",
			(int)( WiredWebClay_Clamp01( values[i] ) * 100.0f + 0.5f ) );
	snprintf( overallPercentage, sizeof( overallPercentage ), "%3d%%",
		(int)( WiredWebClay_Clamp01( state->overall ) * 100.0f + 0.5f ) );

	Clay_BeginLayout();
	CLAY({ .id = CLAY_ID( "web_loading_root" ), .layout = {
		.sizing = { CLAY_SIZING_FIXED( width ), CLAY_SIZING_FIXED( height ) },
		.layoutDirection = CLAY_TOP_TO_BOTTOM }, .backgroundColor = ink }) {
		CLAY({ .id = CLAY_ID( "web_loading_topbar" ), .layout = {
			.sizing = { CLAY_SIZING_FIXED( width ), CLAY_SIZING_FIXED( topHeight ) },
			.padding = { (uint16_t)( width * 0.024f ), (uint16_t)( width * 0.024f ), 0, 0 },
			.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_LEFT_TO_RIGHT }, .backgroundColor = panel,
			.border = { .color = accent, .width = { .bottom = 2 } } }) {
			WiredWebClay_Text( "WIRED / WORLD STREAM", WIRED_WEB_CLAY_FONT_DISPLAY,
				(uint16_t)( height * 0.025f ), accent );
			CLAY({ .layout = { .sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_FIT( 0 ) } } }) {}
			CLAY({ .layout = { .sizing = { CLAY_SIZING_FIXED( width * 0.14f ),
				CLAY_SIZING_GROW( 0 ) }, .childGap = (uint16_t)( width * 0.008f ),
				.childAlignment = { CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER },
				.layoutDirection = CLAY_LEFT_TO_RIGHT } }) {
				WiredWebClay_Text( "OVERALL", WIRED_WEB_CLAY_FONT_MONO,
					(uint16_t)( height * 0.016f ), boneDim );
				WiredWebClay_Text( overallPercentage, WIRED_WEB_CLAY_FONT_MONO,
					(uint16_t)( height * 0.016f ), boneDim );
			}
		}
		CLAY({ .id = CLAY_ID( "web_loading_body" ), .layout = {
			.sizing = { CLAY_SIZING_FIXED( width ), CLAY_SIZING_FIXED( bodyHeight ) },
			.layoutDirection = CLAY_LEFT_TO_RIGHT } }) {
			CLAY({ .id = CLAY_ID( "web_loading_left" ), .layout = {
				.sizing = { CLAY_SIZING_FIXED( leftWidth ), CLAY_SIZING_FIXED( bodyHeight ) },
				.padding = { (uint16_t)( width * 0.035f ), (uint16_t)( width * 0.025f ),
					(uint16_t)( height * 0.055f ), (uint16_t)( height * 0.04f ) },
				.childGap = (uint16_t)( height * 0.018f ), .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
				WiredWebClay_Text( "STREAM TOPOLOGY", WIRED_WEB_CLAY_FONT_MONO,
					(uint16_t)( height * 0.015f ), boneDim );
				CLAY({ .layout = { .sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_GROW( 0 ) },
					.padding = CLAY_PADDING_ALL( (uint16_t)( height * 0.035f ) ),
					.childGap = (uint16_t)( height * 0.022f ), .layoutDirection = CLAY_TOP_TO_BOTTOM },
					.backgroundColor = panel, .border = { .color = line,
						.width = { .left = 1, .right = 1, .top = 1, .bottom = 1 } } }) {
					for ( i = 0; i < 5; ++i ) {
						CLAY({ .layout = { .sizing = { CLAY_SIZING_PERCENT( 0.42f + i * 0.10f ),
							CLAY_SIZING_FIXED( MAX( 2.0f, height * 0.004f ) ) } },
							.backgroundColor = i == 2 ? accent : line }) {}
					}
					WiredWebClay_Text( "BSP / MATERIALS / AUDIO / ENTITIES",
						WIRED_WEB_CLAY_FONT_MONO, (uint16_t)( height * 0.014f ), boneDim );
				}
			}
			CLAY({ .id = CLAY_ID( "web_loading_divider" ), .layout = {
				.sizing = { CLAY_SIZING_FIXED( dividerWidth ), CLAY_SIZING_FIXED( bodyHeight ) } },
				.backgroundColor = accent }) {}
			CLAY({ .id = CLAY_ID( "web_loading_right" ), .layout = {
				.sizing = { CLAY_SIZING_FIXED( rightWidth ), CLAY_SIZING_FIXED( bodyHeight ) },
				.padding = { (uint16_t)( width * 0.035f ), (uint16_t)( width * 0.022f ),
					(uint16_t)( height * 0.048f ), (uint16_t)( height * 0.035f ) },
				.childGap = (uint16_t)( height * 0.016f ), .layoutDirection = CLAY_TOP_TO_BOTTOM } }) {
				WiredWebClay_Text( "LOADING MAP", WIRED_WEB_CLAY_FONT_MONO,
					(uint16_t)( height * 0.014f ), accent );
				WiredWebClay_Text( state->mapName && state->mapName[0] ? state->mapName : "ARENA 17",
					WIRED_WEB_CLAY_FONT_DISPLAY, (uint16_t)( height * 0.064f ), bone );
				WiredWebClay_Text( "REAL-TIME ASSET STREAM / VERIFIED CONTENT",
					WIRED_WEB_CLAY_FONT_UI, (uint16_t)( height * 0.016f ), boneDim );
				CLAY({ .layout = { .sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_FIXED( height * 0.035f ) } } }) {}
				for ( i = 0; i < 4; ++i ) {
					CLAY({ .layout = { .sizing = { CLAY_SIZING_PERCENT( 1.0f ),
						CLAY_SIZING_FIXED( height * 0.052f ) }, .childAlignment = {
						CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER }, .layoutDirection = CLAY_LEFT_TO_RIGHT },
						.border = { .color = line, .width = { .bottom = 1 } } }) {
						WiredWebClay_Text( labels[i], WIRED_WEB_CLAY_FONT_MONO,
							(uint16_t)( height * 0.015f ), boneDim );
						CLAY({ .layout = { .sizing = { CLAY_SIZING_GROW( 0 ), CLAY_SIZING_FIT( 0 ) } } }) {}
						WiredWebClay_Text( percentages[i], WIRED_WEB_CLAY_FONT_MONO,
							(uint16_t)( height * 0.015f ), i == 3 ? accent : bone );
					}
				}
				CLAY({ .layout = { .sizing = { CLAY_SIZING_PERCENT( 1.0f ),
					CLAY_SIZING_FIXED( height * loading->phaseHeightPercent / 100.0f ) } } }) {
					WiredWebClay_Text( state->phase && state->phase[0] ? state->phase : "loading...",
						WIRED_WEB_CLAY_FONT_UI, (uint16_t)( height * 0.016f ), boneDim );
				}
				CLAY({ .layout = { .sizing = { CLAY_SIZING_PERCENT(
					WiredWebClay_Clamp01( state->overall ) ), CLAY_SIZING_FIXED(
						height * loading->overallBarHeightPercent / 100.0f ) } },
					.backgroundColor = accent }) {}
			}
		}
		CLAY({ .id = CLAY_ID( "web_loading_footer" ), .layout = {
			.sizing = { CLAY_SIZING_FIXED( width ), CLAY_SIZING_FIXED( bottomHeight ) },
			.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER } },
			.backgroundColor = panel, .border = { .color = line, .width = { .top = 1 } } }) {
			WiredWebClay_Text( loading->footerText, WIRED_WEB_CLAY_FONT_MONO,
				(uint16_t)( height * 0.014f ), boneDim );
		}
	}
}

static void WiredWebClay_Dispatch( Clay_RenderCommandArray commands ) {
	int i;
	for ( i = 0; i < commands.length; ++i ) {
		Clay_RenderCommand *command = Clay_RenderCommandArray_Get( &commands, i );
		Clay_BoundingBox *box;
		vec4_t color;
		if ( !command ) continue;
		box = &command->boundingBox;
		if ( command->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE ) {
			Clay_Color source = command->renderData.rectangle.backgroundColor;
			color[0] = source.r / 255.0f; color[1] = source.g / 255.0f;
			color[2] = source.b / 255.0f; color[3] = source.a / 255.0f;
			if ( re.SetColor && re.DrawMenuBackdrop ) {
				re.SetColor( color );
				re.DrawMenuBackdrop( box->x, box->y, box->width, box->height,
					0.0f, 0.0f, 1.0f, 1.0f );
			}
		} else if ( command->commandType == CLAY_RENDER_COMMAND_TYPE_BORDER ) {
			Clay_BorderRenderData *border = &command->renderData.border;
			color[0] = border->color.r / 255.0f; color[1] = border->color.g / 255.0f;
			color[2] = border->color.b / 255.0f; color[3] = border->color.a / 255.0f;
			if ( border->width.left > 0 && re.SetColor && re.DrawMenuBackdrop ) {
				re.SetColor( color );
				re.DrawMenuBackdrop( box->x, box->y, border->width.left, box->height,
					0.0f, 0.0f, 1.0f, 1.0f );
			}
			if ( border->width.right > 0 && re.SetColor && re.DrawMenuBackdrop ) {
				re.SetColor( color );
				re.DrawMenuBackdrop( box->x + box->width - border->width.right, box->y,
					border->width.right, box->height, 0.0f, 0.0f, 1.0f, 1.0f );
			}
			if ( border->width.top > 0 && re.SetColor && re.DrawMenuBackdrop ) {
				re.SetColor( color );
				re.DrawMenuBackdrop( box->x, box->y, box->width, border->width.top,
					0.0f, 0.0f, 1.0f, 1.0f );
			}
			if ( border->width.bottom > 0 && re.SetColor && re.DrawMenuBackdrop ) {
				re.SetColor( color );
				re.DrawMenuBackdrop( box->x, box->y + box->height - border->width.bottom,
					box->width, border->width.bottom, 0.0f, 0.0f, 1.0f, 1.0f );
			}
		} else if ( command->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT ) {
			Clay_TextRenderData *text = &command->renderData.text;
			char bounded[WIRED_WEB_CLAY_TEXT_BYTES];
			int length = text->stringContents.length;
			if ( length >= (int)sizeof( bounded ) ) length = (int)sizeof( bounded ) - 1;
			if ( length <= 0 ) continue;
			memcpy( bounded, text->stringContents.chars, (size_t)length );
			bounded[length] = '\0';
			color[0] = text->textColor.r / 255.0f; color[1] = text->textColor.g / 255.0f;
			color[2] = text->textColor.b / 255.0f; color[3] = text->textColor.a / 255.0f;
			Text_Draw( bounded, box->x, box->y, WiredWebClay_Font( text->fontId ),
				(float)text->fontSize, color, TEXT_ALIGN_LEFT, 0 );
		}
	}
	if ( re.SetColor ) re.SetColor( NULL );
}

qboolean WiredWebClay_RenderMainMenu( const wiredWebAuthoredCatalog_t *catalog,
		float width, float height, int focusedItem ) {
	Clay_RenderCommandArray commands;
	if ( !catalog || width <= 0.0f || height <= 0.0f || !WiredWebClay_Init() )
		return qfalse;
	s_clayErrors = 0;
	s_clayLastError = 0;
	Clay_SetLayoutDimensions( (Clay_Dimensions){ width, height } );
	WiredWebClay_EmitMenu( catalog, width, height, focusedItem );
	commands = Clay_EndLayout();
	if ( s_clayErrors != 0 || commands.length <= 0 ) return qfalse;
	WiredWebClay_DrawDemoBackdrop( catalog, width, height );
	WiredWebClay_Dispatch( commands );
	s_clayRendered = qtrue;
	return qtrue;
}

qboolean WiredWebClay_RenderServerBrowser( const wiredWebAuthoredCatalog_t *catalog,
		float width, float height, const wiredWebServerRow_t *rows, int rowCount,
		int scrollIndex, int selectedIndex, float pointerX, float pointerY,
		qboolean pointerDown, qboolean pointerPressed, int *hoveredIndex ) {
	Clay_RenderCommandArray commands;
	double begin, elapsed;
	int firstVisible = 0, visibleCount = 0, row;
	if ( hoveredIndex ) *hoveredIndex = -1;
	if ( !catalog || !rows || rowCount <= 0 || rowCount > WIRED_WEB_SERVER_FIXTURE_MAX
			|| width <= 0.0f || height <= 0.0f || !WiredWebClay_Init() ) return qfalse;
	s_clayErrors = 0; s_clayLastError = 0;
	Clay_SetLayoutDimensions( (Clay_Dimensions){ width, height } );
	Clay_SetPointerState( (Clay_Vector2){ pointerX, pointerY }, pointerDown ? true : false );
	begin = WiredWebClay_NowMillis();
	WiredWebClay_EmitServerBrowser( catalog, width, height, rows, rowCount,
		scrollIndex, selectedIndex, &firstVisible, &visibleCount );
	commands = Clay_EndLayout();
	elapsed = WiredWebClay_NowMillis() - begin;
	s_serverLayoutSamples[s_serverLayoutSampleCursor] = elapsed;
	s_serverLayoutSampleCursor = ( s_serverLayoutSampleCursor + 1 ) % 128;
	if ( s_serverLayoutSampleCount < 128 ) ++s_serverLayoutSampleCount;
	if ( s_clayErrors != 0 || commands.length <= 0 ) return qfalse;
	if ( pointerPressed && hoveredIndex ) {
		for ( row = 0; row < visibleCount; ++row ) {
			int index = firstVisible + row;
			if ( Clay_PointerOver( CLAY_SIDI( WiredWebClay_String( "web_server_row" ), index ) ) ) {
				*hoveredIndex = index; break;
			}
		}
		/* Clay's imperative pointer-over list describes the prior completed tree.
		 * A browser press may arrive on the same rAF boundary as a virtualized
		 * scroll update, so resolve that one bounded edge against the exact
		 * authored list geometry instead of dropping a valid click. */
		if ( *hoveredIndex < 0 ) {
			float listTop = height * ( 0.04f + 0.075f + 0.012f
				+ 0.06f + 0.012f + 0.042f + 0.012f );
			float rowHeight = catalog->serverBrowser.rowHeight * height / 720.0f;
			if ( pointerX >= width * 0.05f && pointerX <= width * 0.95f
					&& pointerY >= listTop && pointerY < listTop + height * 0.58f
					&& rowHeight > 0.0f ) {
				row = (int)( ( pointerY - listTop ) / rowHeight );
				if ( row >= 0 && row < visibleCount ) *hoveredIndex = firstVisible + row;
			}
		}
	}
	WiredWebClay_Dispatch( commands );
	s_clayRendered = qtrue;
	return qtrue;
}

qboolean WiredWebClay_RenderLoading( const wiredWebAuthoredCatalog_t *catalog,
		float width, float height, const wiredWebLoadingState_t *state ) {
	Clay_RenderCommandArray commands;
	if ( !catalog || !state || width <= 0.0f || height <= 0.0f
			|| !catalog->loading.menuName[0] || !WiredWebClay_Init() ) return qfalse;
	s_clayErrors = 0; s_clayLastError = 0;
	Clay_SetLayoutDimensions( (Clay_Dimensions){ width, height } );
	WiredWebClay_EmitLoading( catalog, width, height, state );
	commands = Clay_EndLayout();
	if ( s_clayErrors != 0 || commands.length <= 0 ) return qfalse;
	WiredWebClay_Dispatch( commands );
	s_clayRendered = qtrue;
	return qtrue;
}
