// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
 * Browser-portable authored Wired UI boundary.
 *
 * The shipped main menu, localization and scene inputs are compiled to a
 * bounded catalog and consumed through the existing high-level subsystem API.
 * Native builds continue to use LuaJIT; this translation unit owns no raw VM
 * compatibility symbols.
 */

#include "web_ui_compat.h"
#include "web_authored_content.h"
#include "web_ui_clay.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

const char *WiredToken_Find( const char *name );

static qboolean s_menuVisible = qtrue;
static qboolean s_previewSceneLoaded;
static qboolean s_textReady;
static int s_menuFocus;
static qboolean s_menuNavigationUsed;
static int s_menuRoot;
static qboolean s_loadingVisible;
static qboolean s_loadingFixture;
static uint32_t s_layerReceipt;
static wiredWebServerRow_t s_serverRows[WIRED_WEB_SERVER_FIXTURE_MAX];
static int s_serverRowCount;
static int s_serverScroll;
static int s_serverSelected = -1;
static float s_pointerX, s_pointerY;
static qboolean s_pointerDown, s_pointerPressed;
static uint32_t s_serverReceipt;
static wiredAssetGlobals_t s_webAssets;
static wiredHudState_t s_hudState;
static qboolean s_hudStateReady;
static uint32_t s_hudReceipt;
static uint32_t s_hudExtent;
static float s_crosshairRecoil;
static qboolean s_crosshairTargetExists;
static int s_crosshairTargetArmorClass;

#define WIRED_WEB_HUD_STATE_READY 0x1u
#define WIRED_WEB_HUD_RENDERED 0x2u
#define WIRED_WEB_CROSSHAIR_RENDERED 0x4u
#define WIRED_WEB_MENU_ROOT_MAIN 0
#define WIRED_WEB_MENU_ROOT_SERVERS 1
#define WIRED_WEB_SERVER_ACTIVE 0x1u
#define WIRED_WEB_SERVER_RENDERED 0x2u
#define WIRED_WEB_SERVER_SCROLLED 0x4u
#define WIRED_WEB_SERVER_CLICKED 0x8u
#define WIRED_WEB_SERVER_FIXTURE_200 0x10u
#define WIRED_WEB_LAYER_LOADING_ACTIVE 0x1u
#define WIRED_WEB_LAYER_LOADING_RENDERED 0x2u
#define WIRED_WEB_LAYER_CONSOLE_ACTIVE 0x4u
#define WIRED_WEB_LAYER_CONSOLE_RENDERED 0x8u
#define WIRED_WEB_LAYER_LOADING_FIXTURE 0x10u

static float WiredWeb_Clamp01( float value );

static void WiredWeb_SetMenuVisible( qboolean visible )
{
	s_menuVisible = visible;
	if ( visible )
		Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );
	else
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
}

static void WiredWeb_DrawSolid( float x, float y, float w, float h,
		const vec4_t color, qhandle_t white ) {
	if ( w <= 0.0f || h <= 0.0f || !re.SetColor ) return;
	re.SetColor( color );
	if ( white && re.DrawStretchPic )
		re.DrawStretchPic( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, white );
	else if ( re.DrawMenuBackdrop )
		re.DrawMenuBackdrop( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f );
}

void WiredWebUi_ReceiveHudState( const wiredHudState_t *state ) {
	if ( !state || !state->valid ) {
		s_hudStateReady = qfalse;
		return;
	}
	s_hudState = *state;
	s_hudStateReady = qtrue;
	s_hudReceipt |= WIRED_WEB_HUD_STATE_READY;
}

void WiredWebUi_ReceiveStoreBatch( const wuiStagedEntry_t *entries, int count ) {
	int i;
	if ( !entries || count <= 0 ) return;
	for ( i = 0; i < count; ++i ) {
		const wuiStagedEntry_t *entry = &entries[i];
		if ( !( entry->fields & WUI_STAGED_VALUE ) ) continue;
		if ( !strcmp( entry->key, "crosshair.recoil" ) )
			s_crosshairRecoil = WiredWeb_Clamp01( entry->value );
		else if ( !strcmp( entry->key, "crosshair.target.exists" ) )
			s_crosshairTargetExists = entry->value != 0.0f ? qtrue : qfalse;
		else if ( !strcmp( entry->key, "crosshair.target.armorClass" ) )
			s_crosshairTargetArmorClass = (int)entry->value;
	}
}

uint32_t WiredWebUi_HudReceipt( void ) { return s_hudReceipt; }
uint32_t WiredWebUi_HudExtent( void ) { return s_hudExtent; }
uint32_t WiredWebUi_LayerReceipt( void ) {
	uint32_t receipt = s_layerReceipt;
	if ( s_loadingVisible ) receipt |= WIRED_WEB_LAYER_LOADING_ACTIVE;
	if ( Key_GetCatcher() & KEYCATCH_CONSOLE ) receipt |= WIRED_WEB_LAYER_CONSOLE_ACTIVE;
	if ( s_loadingFixture ) receipt |= WIRED_WEB_LAYER_LOADING_FIXTURE;
	return receipt;
}
uint32_t WiredWebUi_MenuSemanticReceipt( void ) {
	return (uint32_t)( s_menuFocus + 1 ) | ( s_menuNavigationUsed ? 0x100u : 0u );
}

static void WiredWeb_InitServerFixture( void ) {
	int i;
	if ( s_serverRowCount == WIRED_WEB_SERVER_FIXTURE_MAX ) return;
	for ( i = 0; i < WIRED_WEB_SERVER_FIXTURE_MAX; ++i ) {
		snprintf( s_serverRows[i].fields[0], sizeof( s_serverRows[i].fields[0] ),
			"WIRED TEST SERVER %03d", i + 1 );
		snprintf( s_serverRows[i].fields[1], sizeof( s_serverRows[i].fields[1] ),
			"arena%d", 1 + i % 17 );
		snprintf( s_serverRows[i].fields[2], sizeof( s_serverRows[i].fields[2] ),
			"%d/16", i % 17 );
		snprintf( s_serverRows[i].fields[3], sizeof( s_serverRows[i].fields[3] ),
			"%s", i % 2 ? "TDM" : "FFA" );
		snprintf( s_serverRows[i].fields[4], sizeof( s_serverRows[i].fields[4] ),
			"%d", 12 + i % 89 );
	}
	s_serverRowCount = WIRED_WEB_SERVER_FIXTURE_MAX;
	s_serverReceipt |= WIRED_WEB_SERVER_FIXTURE_200;
}

void WiredWebUi_EnableServerFixture( int count ) {
	if ( count == WIRED_WEB_SERVER_FIXTURE_MAX ) WiredWeb_InitServerFixture();
}

void WiredWebUi_ActivateLayerFixture( int kind ) {
	if ( kind == 1 ) {
		s_loadingVisible = qtrue;
		s_loadingFixture = qtrue;
		s_layerReceipt &= ~WIRED_WEB_LAYER_LOADING_RENDERED;
	} else {
		s_loadingVisible = qfalse;
		s_loadingFixture = qfalse;
	}
}

void WiredWebUi_Pointer( float x, float y, int down ) {
	s_pointerX = x; s_pointerY = y;
	if ( down && !s_pointerDown ) s_pointerPressed = qtrue;
	s_pointerDown = down ? qtrue : qfalse;
}

void WiredWebUi_Wheel( float deltaY ) {
	if ( s_menuRoot != WIRED_WEB_MENU_ROOT_SERVERS || s_serverRowCount <= 0 || deltaY == 0.0f ) return;
	s_serverScroll += deltaY > 0.0f ? 5 : -5;
	if ( s_serverScroll < 0 ) s_serverScroll = 0;
	if ( s_serverScroll >= s_serverRowCount ) s_serverScroll = s_serverRowCount - 1;
	s_serverReceipt |= WIRED_WEB_SERVER_SCROLLED;
}

uint32_t WiredWebUi_ServerReceipt( void ) {
	uint32_t receipt = s_serverReceipt;
	if ( s_menuVisible && s_menuRoot == WIRED_WEB_MENU_ROOT_SERVERS ) receipt |= WIRED_WEB_SERVER_ACTIVE;
	if ( s_serverSelected >= 0 ) receipt |= (uint32_t)( s_serverSelected + 1 ) << 8;
	return receipt;
}

static float WiredWeb_Clamp01( float value ) {
	if ( value < 0.0f ) return 0.0f;
	if ( value > 1.0f ) return 1.0f;
	return value;
}

static void WiredWeb_DrawHudPanel( const char *caption, int value,
		float ratio, float x, float y, float w, float h,
		const vec4_t accent, qboolean alignRight ) {
	const vec4_t background = { 0.025f, 0.03f, 0.04f, 0.76f };
	const vec4_t captionColor = { 0.62f, 0.62f, 0.60f, 1.0f };
	char text[24];
	float edge = MAX( 2.0f, w * 0.018f );
	float inset = w * 0.10f;
	float textX = alignRight ? x + w - inset : x + inset;
	int alignment = alignRight ? TEXT_ALIGN_RIGHT : TEXT_ALIGN_LEFT;

	WiredWeb_DrawSolid( x, y, w, h, background, s_hudState.whiteShader );
	WiredWeb_DrawSolid( alignRight ? x + w - edge : x, y, edge, h,
		accent, s_hudState.whiteShader );
	Text_Draw( caption, textX, y + h * 0.25f, FONT_MONO, h * 0.12f,
		captionColor, alignment, 0 );
	snprintf( text, sizeof( text ), "%d", value );
	Text_Draw( text, textX, y + h * 0.68f, FONT_DISPLAY_BOLD, h * 0.34f,
		accent, alignment, 0 );
	WiredWeb_DrawSolid( alignRight ? x + w - inset - w * 0.58f * WiredWeb_Clamp01( ratio ) : x + inset,
		y + h * 0.80f, w * 0.58f * WiredWeb_Clamp01( ratio ), MAX( 2.0f, h * 0.035f ),
		accent, s_hudState.whiteShader );
}

static const wiredWebAuthoredCrosshair_t *WiredWeb_CrosshairSpec(
		const wiredWebAuthoredCatalog_t *catalog, int weapon ) {
	const wiredWebAuthoredCrosshair_t *fallback = NULL;
	int i;
	for ( i = 0; i < catalog->crosshairCount; ++i ) {
		const wiredWebAuthoredCrosshair_t *spec = &catalog->crosshairs[i];
		if ( spec->weapon == 0 ) fallback = spec;
		if ( spec->weapon == weapon ) return spec;
	}
	return fallback;
}

static void WiredWeb_CrosshairArmorTint( vec4_t color ) {
	if ( !s_crosshairTargetExists || s_crosshairTargetArmorClass <= 0 ) return;
	if ( s_crosshairTargetArmorClass == 1 ) {
		color[0] = 0.4f; color[1] = 1.0f; color[2] = 0.4f;
	} else if ( s_crosshairTargetArmorClass == 2 ) {
		color[0] = 1.0f; color[1] = 1.0f; color[2] = 0.3f;
	} else {
		color[0] = 1.0f; color[1] = 0.3f; color[2] = 0.3f;
	}
}

static void WiredWeb_DrawCrosshair( const wiredWebAuthoredCatalog_t *catalog,
		float width, float height ) {
	const wiredWebAuthoredCrosshair_t *spec;
	vec4_t dark = { 0.0f, 0.0f, 0.0f, 0.0f };
	vec4_t color;
	float scale, cx, cy, gap, length, thickness, outline;
	int arm;

	if ( s_hudState.crosshair.shaderIndex < 0 || !s_hudState.whiteShader ) return;
	spec = WiredWeb_CrosshairSpec( catalog, s_hudState.weapon );
	if ( !spec ) return;
	scale = height / 720.0f;
	if ( s_hudState.crosshair.size > 0.0f ) scale *= s_hudState.crosshair.size / 48.0f;
	cx = width * 0.5f + s_hudState.crosshair.x;
	cy = height * 0.5f + s_hudState.crosshair.y;
	gap = spec->gap;
	if ( spec->dynamicKind == 1 ) {
		gap += MIN( MAX( s_hudState.xyspeed / 320.0f, 0.0f ), 1.0f ) * 8.0f;
		gap += WiredWeb_Clamp01( s_crosshairRecoil ) * 14.0f;
	}
	gap *= scale;
	length = spec->armLength * scale;
	thickness = MAX( 1.0f, spec->armThickness * scale );
	outline = MAX( 0.0f, spec->outlineThickness * scale );
	dark[3] = spec->outlineAlpha;
	Vector4Copy( spec->color, color );
	color[0] *= s_hudState.crosshair.color[0];
	color[1] *= s_hudState.crosshair.color[1];
	color[2] *= s_hudState.crosshair.color[2];
	color[3] *= s_hudState.crosshair.color[3];
	if ( spec->dynamicKind == 1 || spec->dynamicKind == 2 )
		WiredWeb_CrosshairArmorTint( color );

	for ( arm = 0; arm < 4; ++arm ) {
		float x, y, w, h;
		if ( arm == 0 || arm == 2 ) {
			x = cx - thickness * 0.5f;
			y = arm == 0 ? cy - gap - length : cy + gap;
			w = thickness; h = length;
		} else {
			x = arm == 1 ? cx + gap : cx - gap - length;
			y = cy - thickness * 0.5f;
			w = length; h = thickness;
		}
		if ( outline > 0.0f )
			WiredWeb_DrawSolid( x - outline, y - outline, w + outline * 2.0f,
				h + outline * 2.0f, dark, s_hudState.whiteShader );
		WiredWeb_DrawSolid( x, y, w, h, color,
			s_hudState.whiteShader );
	}
	if ( spec->ringEnabled && spec->ringRadius > 0.0f && re.DrawLine ) {
		const int segments = 48;
		float radius = spec->ringRadius * scale;
		float lineWidth = MAX( 1.0f, spec->ringThickness * scale );
		for ( arm = 0; arm < segments; ++arm ) {
			float a0 = 6.28318530718f * (float)arm / (float)segments;
			float a1 = 6.28318530718f * (float)( arm + 1 ) / (float)segments;
			if ( outline > 0.0f ) {
				re.SetColor( dark );
				re.DrawLine( cx + cosf( a0 ) * radius, cy + sinf( a0 ) * radius,
					cx + cosf( a1 ) * radius, cy + sinf( a1 ) * radius,
					lineWidth + outline * 2.0f, s_hudState.whiteShader );
			}
			re.SetColor( color );
			re.DrawLine( cx + cosf( a0 ) * radius, cy + sinf( a0 ) * radius,
				cx + cosf( a1 ) * radius, cy + sinf( a1 ) * radius,
				lineWidth, s_hudState.whiteShader );
		}
	}
	if ( spec->dotEnabled && spec->dotRadius > 0.0f ) {
		float radius = spec->dotRadius * scale;
		if ( outline > 0.0f )
			WiredWeb_DrawSolid( cx - radius - outline, cy - radius - outline,
				( radius + outline ) * 2.0f, ( radius + outline ) * 2.0f,
				dark, s_hudState.whiteShader );
		WiredWeb_DrawSolid( cx - radius, cy - radius, radius * 2.0f,
			radius * 2.0f, color, s_hudState.whiteShader );
	}
	s_hudReceipt |= WIRED_WEB_CROSSHAIR_RENDERED;
}

static void WiredWeb_DrawGameplayHud( const wiredWebAuthoredCatalog_t *catalog,
		float width, float height ) {
	const wiredWebAuthoredHud_t *hud;
	vec4_t health = { 0.92f, 0.12f, 0.18f, 1.0f };
	vec4_t armor = { 0.95f, 0.72f, 0.26f, 1.0f };
	vec4_t ammo = { 0.95f, 0.72f, 0.26f, 1.0f };
	float left, right, bottom, panelH, healthW, armorW, ammoW;
	int ammoValue = 0;

	if ( !catalog || !s_hudStateReady || s_hudState.hud2DHidden
			|| s_hudState.sceneHudHidden ) return;
	hud = &catalog->hud;
	left = width * hud->leftInsetPercent / 100.0f;
	right = width * hud->rightInsetPercent / 100.0f;
	bottom = height * hud->bottomInsetPercent / 100.0f;
	panelH = height * hud->panelHeightPercent / 100.0f;
	healthW = width * hud->healthWidthPercent / 100.0f;
	armorW = width * hud->armorWidthPercent / 100.0f;
	ammoW = width * hud->ammoWidthPercent / 100.0f;
	if ( s_hudState.weapon >= 0 && s_hudState.weapon < MAX_WEAPONS )
		ammoValue = s_hudState.ammo[s_hudState.weapon];

	WiredWeb_DrawHudPanel( "HEALTH", s_hudState.health,
		(float)s_hudState.health / 100.0f, left, height - bottom - panelH * 2.0f,
		healthW, panelH, health, qfalse );
	WiredWeb_DrawHudPanel( "ARMOR", s_hudState.armor,
		(float)s_hudState.armor / 200.0f, left, height - bottom - panelH,
		armorW, panelH, armor, qfalse );
	WiredWeb_DrawHudPanel( "AMMO", ammoValue,
		(float)ammoValue / 200.0f, width - right - ammoW, height - bottom - panelH,
		ammoW, panelH, ammo, qtrue );
	WiredWeb_DrawCrosshair( catalog, width, height );
	if ( re.SetColor ) re.SetColor( NULL );
	s_hudExtent = ( (uint32_t)width << 16 ) | ( (uint32_t)height & 0xffffu );
	s_hudReceipt |= WIRED_WEB_HUD_RENDERED;
}

static void WiredWeb_DrawConsoleOverlay( const wiredWebAuthoredCatalog_t *catalog ) {
	if ( !catalog || !catalog->console.menuName[0]
			|| !( Key_GetCatcher() & KEYCATCH_CONSOLE ) ) return;
	Con_DrawConsole();
	s_layerReceipt |= WIRED_WEB_LAYER_CONSOLE_RENDERED;
}

void CL_ShutdownUI( void ) { WiredWebClay_Shutdown(); WiredWebAuthored_Shutdown(); s_menuVisible = qtrue;
	s_previewSceneLoaded = qfalse; s_textReady = qfalse; s_hudStateReady = qfalse;
	s_menuFocus = 0; s_menuNavigationUsed = qfalse; s_menuRoot = WIRED_WEB_MENU_ROOT_MAIN;
	s_loadingVisible = qfalse; s_loadingFixture = qfalse; s_layerReceipt = 0u;
	s_serverRowCount = 0; s_serverScroll = 0; s_serverSelected = -1;
	s_pointerX = s_pointerY = 0.0f; s_pointerDown = s_pointerPressed = qfalse; s_serverReceipt = 0u;
	s_hudReceipt = 0u; s_hudExtent = 0u; s_crosshairRecoil = 0.0f; s_crosshairTargetExists = qfalse;
	s_crosshairTargetArmorClass = 0; memset( &s_hudState, 0, sizeof( s_hudState ) ); }
qboolean WiredUI_Init( qboolean inGameUI ) {
	qboolean ready;
	(void)inGameUI;
	memset( &s_webAssets, 0, sizeof( s_webAssets ) );
	Text_Init();
	s_textReady = MSDF_GetFontCount() >= 3 ? qtrue : qfalse;
	ready = WiredWebAuthored_EnsureLoaded() && WiredWebClay_Init() ? qtrue : qfalse;
	if ( ready && s_menuVisible ) WiredWeb_SetMenuVisible( qtrue );
	return ready;
}
void WiredUI_Shutdown( void ) { CL_ShutdownUI(); }
void WiredUI_TickFrame( int realtime ) {
	(void)realtime;
	/* The browser adapter owns this menu stack, so its visibility and the
	 * engine input catcher must remain one state across init/reload boundaries. */
	if ( s_menuVisible && !( Key_GetCatcher() & KEYCATCH_UI ) )
		Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );
	else if ( !s_menuVisible && ( Key_GetCatcher() & KEYCATCH_UI ) )
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
}

uint32_t WiredWebUi_TextReceipt( void )
{
	uint32_t flags = 0u;
	if ( MSDF_GetFontCount() >= 3 ) flags |= 0x1u;
	if ( MSDF_GetRenderableFontCount() >= 3 ) flags |= 0x2u;
	if ( Text_Measure( "WIRED", FONT_DISPLAY_BOLD, 32.0f ) > 0.0f ) flags |= 0x4u;
	if ( FS_ReadFile( "fonts/sansman-regular.json", NULL ) > 0 ) flags |= 0x10u;
	return flags;
}

void WiredUI_RenderFrame( void ) {
	const wiredWebAuthoredCatalog_t *catalog = WiredWebAuthored_Catalog();
	float width, height;
	/* W0 calls the browser UI through its explicit presentation adapter; the
	 * native FEAT_WIRED_UI frame hook is compiled out. Reassert the same menu
	 * stack/input-catcher invariant at this boundary. */
	if ( s_menuVisible && !( Key_GetCatcher() & KEYCATCH_UI ) )
		Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );
	if ( !catalog || !re.DrawMenuBackdrop
			|| cls.glconfig.vidWidth <= 0 || cls.glconfig.vidHeight <= 0 ) return;
	/* Com/UI bootstrap can precede the asynchronous browser content mount.
	 * Retry only until the three eager canonical atlases are VFS-resident. */
	if ( !s_textReady && FS_ReadFile( "fonts/sansman-regular.json", NULL ) > 0 ) {
		Text_Init();
		s_textReady = MSDF_GetFontCount() >= 3 ? qtrue : qfalse;
	}
	width = (float)cls.glconfig.vidWidth; height = (float)cls.glconfig.vidHeight;
	if ( s_loadingVisible ) {
		wiredWebLoadingState_t loading;
		memset( &loading, 0, sizeof( loading ) );
		if ( s_loadingFixture ) {
			loading.geometry = 0.92f; loading.shaders = 0.74f;
			loading.audio = 0.61f; loading.download = 1.0f; loading.overall = 0.79f;
			loading.phase = "compiling portable shaders"; loading.mapName = "ARENA 17";
		} else {
			loading.geometry = cl_loadProgress.geometry;
			loading.shaders = cl_loadProgress.shaders;
			loading.audio = cl_loadProgress.audio;
			loading.download = cl_loadProgress.download;
			loading.overall = cl_loadProgress.overall;
			loading.phase = cl_loadProgress.phase;
			loading.mapName = clientActiveApp ? clientActiveApp->cl.mapname : "";
		}
		if ( WiredWebClay_RenderLoading( catalog, width, height, &loading ) )
			s_layerReceipt |= WIRED_WEB_LAYER_LOADING_RENDERED;
		WiredWeb_DrawConsoleOverlay( catalog );
		return;
	}
	if ( !s_menuVisible ) {
		WiredWeb_DrawGameplayHud( catalog, width, height );
		WiredWeb_DrawConsoleOverlay( catalog );
		return;
	}
	if ( s_menuRoot == WIRED_WEB_MENU_ROOT_SERVERS ) {
		int hovered = -1;
		if ( WiredWebClay_RenderServerBrowser( catalog, width, height, s_serverRows,
				s_serverRowCount, s_serverScroll, s_serverSelected, s_pointerX, s_pointerY,
				s_pointerDown, s_pointerPressed, &hovered ) ) {
			s_serverReceipt |= WIRED_WEB_SERVER_RENDERED;
			if ( hovered >= 0 ) {
				s_serverSelected = hovered;
				s_serverReceipt |= WIRED_WEB_SERVER_CLICKED;
			}
			s_pointerPressed = qfalse;
		}
		WiredWeb_DrawConsoleOverlay( catalog );
		return;
	}
	if ( WiredWebClay_RenderMainMenu( catalog, width, height, s_menuFocus ) ) {
		if ( !s_previewSceneLoaded ) {
			wiredScene_t preview;
			s_previewSceneLoaded = WiredScene_LoadFromFile( &preview,
				"scripts/scene/arena1.lua" ) ? qtrue : qfalse;
			(void)WiredL10n_Get( "scene/arena1/greet" );
		}
		WiredWebAuthored_MarkMenuRendered();
		WiredWeb_DrawConsoleOverlay( catalog );
		return;
	}
	/* Clay failure is a retained, fail-closed receipt. Never revive the retired
	 * rectangle-only W0 composition, because that would hide authored-layout
	 * regressions behind a visually plausible second UI authority. */
}

void CL_WiredUI_ShowError( const char *title, const char *message, qboolean retryable )
{
	(void)title;
	(void)message;
	(void)retryable;
}

qboolean CL_WiredUI_ShowJoinPasswordRetry( const char *target, int selectionGeneration )
{
	(void)target;
	(void)selectionGeneration;
	return qfalse;
}

wiredAssetGlobals_t *WiredUI_GetAssetGlobals( void ) { return &s_webAssets; }

qboolean WiredAttract_IsActive( void ) { return qfalse; }
qboolean WiredAttract_IsDemoOverlayActive( void ) { return qfalse; }
qboolean WiredAttract_OnDemoCompleted( void ) { return qfalse; }
qboolean WiredAttract_OnDemoFailed( void ) { return qfalse; }

const char *WiredL10n_Get( const char *key )
{
	return WiredWebAuthored_Localize( key );
}

const char *WiredToken_Find( const char *name )
{
	(void)name;
	return NULL;
}

void WiredUI_Activate( void ) {
	WiredWeb_SetMenuVisible( WiredWebAuthored_EnsureLoaded() ? qtrue : qfalse );
}
void WiredUI_CloseAllMenus( void ) { WiredWeb_SetMenuVisible( qfalse ); }
int WiredUI_GetMenuStackDepth( void ) { return s_menuVisible ? 1 : 0; }
qboolean WiredUI_IsHealthy( void ) { return WiredWebAuthored_EnsureLoaded() ? qtrue : qfalse; }
int WiredUI_GetLastRecoveryFailTime( void ) { return 0; }

void WiredUI_KeyEvent( int key, qboolean down )
{
	const wiredWebAuthoredCatalog_t *catalog;
	if ( !down ) return;
	if ( key == K_ESCAPE ) {
		if ( s_menuVisible && s_menuRoot == WIRED_WEB_MENU_ROOT_SERVERS )
			s_menuRoot = WIRED_WEB_MENU_ROOT_MAIN;
		else WiredWeb_SetMenuVisible( s_menuVisible ? qfalse : qtrue );
		return;
	}
	if ( !s_menuVisible ) return;
	catalog = WiredWebAuthored_Catalog();
	if ( !catalog || catalog->itemCount <= 0 ) return;
	if ( key == K_UPARROW || key == K_KP_UPARROW ) {
		s_menuFocus = ( s_menuFocus + catalog->itemCount - 1 ) % catalog->itemCount;
		s_menuNavigationUsed = qtrue;
	} else if ( key == K_DOWNARROW || key == K_KP_DOWNARROW ) {
		s_menuFocus = ( s_menuFocus + 1 ) % catalog->itemCount;
		s_menuNavigationUsed = qtrue;
	} else if ( key == K_ENTER || key == K_KP_ENTER ) {
		const wiredWebAuthoredMenuItem_t *item = &catalog->items[s_menuFocus];
		if ( item->actionKind == WIRED_WEB_AUTHORED_ACTION_OPEN
				&& !strcmp( item->action, catalog->serverBrowser.menuName ) ) {
			s_menuRoot = WIRED_WEB_MENU_ROOT_SERVERS;
			s_serverReceipt |= WIRED_WEB_SERVER_ACTIVE;
		}
	}
}

void WiredUI_PushMenu( const char *name ) {
	if ( !name ) return;
	if ( !strcmp( name, "main" ) ) s_menuRoot = WIRED_WEB_MENU_ROOT_MAIN;
	else if ( !strcmp( name, "servers" ) ) s_menuRoot = WIRED_WEB_MENU_ROOT_SERVERS;
	else return;
	WiredWeb_SetMenuVisible( qtrue );
}
void WiredUI_SetActiveMenu( int menu ) {
	WiredWeb_SetMenuVisible( menu == UIMENU_NONE ? qfalse : qtrue );
}
void WiredUI_SetLoadingMenu( const char *relPath ) {
	s_loadingVisible = relPath && *relPath ? qtrue : qfalse;
	if ( !s_loadingVisible ) s_loadingFixture = qfalse;
}
void WiredUI_UnregisterLevelViewportProviders( void ) {}
