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

#include <string.h>

const char *WiredToken_Find( const char *name );

static qboolean s_menuVisible = qtrue;
static qboolean s_previewSceneLoaded;
static qboolean s_autoDismissArmed = qtrue;

static void WiredWeb_DrawRect( float x, float y, float w, float h,
		float r, float g, float b, float a ) {
	float color[4] = { r, g, b, a };
	if ( w <= 0.0f || h <= 0.0f || !re.SetColor || !re.DrawMenuBackdrop ) return;
	re.SetColor( color );
	re.DrawMenuBackdrop( x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f );
}

void CL_ShutdownUI( void ) { WiredWebAuthored_Shutdown(); s_menuVisible = qtrue;
	s_previewSceneLoaded = qfalse; s_autoDismissArmed = qtrue; }
qboolean WiredUI_Init( qboolean inGameUI ) { (void)inGameUI; return WiredWebAuthored_EnsureLoaded() ? qtrue : qfalse; }
void WiredUI_Shutdown( void ) { CL_ShutdownUI(); }
void WiredUI_TickFrame( int realtime ) { (void)realtime; }

void WiredUI_RenderFrame( void ) {
	const wiredWebAuthoredCatalog_t *catalog = WiredWebAuthored_Catalog();
	float width, height, leftX, leftY, leftW, rightX, rightY, rightW;
	int i;
	if ( !catalog || !s_menuVisible || !re.DrawMenuBackdrop
			|| cls.glconfig.vidWidth <= 0 || cls.glconfig.vidHeight <= 0 ) return;
	width = (float)cls.glconfig.vidWidth; height = (float)cls.glconfig.vidHeight;
	leftX = width * catalog->leftXPercent / 100.0f;
	leftY = height * catalog->leftYPercent / 100.0f;
	leftW = width * catalog->leftWidthPercent / 100.0f;
	rightW = width * catalog->rightWidthPercent / 100.0f;
	rightX = width - width * catalog->rightInsetPercent / 100.0f - rightW;
	rightY = height * catalog->rightYPercent / 100.0f;
	/* Geometry is compiled from main.wui's authored regions and item cohort.
	 * Default-material rectangles keep the portable W0 renderer independent of
	 * native font handles while still producing the exact menu composition. */
	WiredWeb_DrawRect( 0, 0, width, height, 0.025f, 0.03f, 0.04f, 0.94f );
	WiredWeb_DrawRect( leftX, leftY, leftW, height * 0.13f, 0.82f, 0.69f, 0.25f, 1.0f );
	for ( i = 0; i < catalog->itemCount; ++i ) {
		float y = leftY + height * 0.23f + (float)i * height * 0.054f;
		float intensity = i == 0 ? 0.70f : 0.28f;
		WiredWeb_DrawRect( leftX, y, leftW * ( i == 0 ? 0.96f : 0.82f ),
			height * 0.036f, intensity, intensity * 0.92f, intensity * 0.60f, 1.0f );
	}
	for ( i = 0; i < 4; ++i ) {
		WiredWeb_DrawRect( rightX, rightY + (float)i * height * 0.19f,
			rightW, height * 0.155f, 0.11f, 0.15f, 0.18f, 0.98f );
		WiredWeb_DrawRect( rightX, rightY + (float)i * height * 0.19f,
			rightW * 0.035f, height * 0.155f, 0.78f, 0.62f, 0.20f, 1.0f );
	}
	if ( re.SetColor ) re.SetColor( NULL );
	if ( !s_previewSceneLoaded ) {
		wiredScene_t preview;
		s_previewSceneLoaded = WiredScene_LoadFromFile( &preview,
			"scripts/scene/arena1.lua" ) ? qtrue : qfalse;
		(void)WiredL10n_Get( "scene/arena1/greet" );
	}
	WiredWebAuthored_MarkMenuRendered();
	/* W0 auto-starts arena17. Present the authored main menu once to prove the
	 * portable path, then let active gameplay own the canvas. Explicit menu
	 * activation below disarms this one-shot transition. */
	if ( s_autoDismissArmed && clientActiveApp
			&& clientActiveApp->state == CA_ACTIVE ) {
		s_autoDismissArmed = qfalse; s_menuVisible = qfalse;
	}
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

float Text_Measure( const char *text, int fontId, float size )
{
	(void)fontId;
	return text ? (float)strlen( text ) * size * 0.55f : 0.0f;
}

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

void WiredUI_Activate( void ) { s_autoDismissArmed = qfalse;
	s_menuVisible = WiredWebAuthored_EnsureLoaded() ? qtrue : qfalse; }
void WiredUI_CloseAllMenus( void ) { s_menuVisible = qfalse; }
int WiredUI_GetMenuStackDepth( void ) { return s_menuVisible ? 1 : 0; }
qboolean WiredUI_IsHealthy( void ) { return WiredWebAuthored_EnsureLoaded() ? qtrue : qfalse; }

void WiredUI_KeyEvent( int key, qboolean down )
{
	if ( down && key == K_ESCAPE ) { s_autoDismissArmed = qfalse;
		s_menuVisible = s_menuVisible ? qfalse : qtrue; }
}

void WiredUI_PushMenu( const char *name ) { if ( name && !strcmp( name, "main" ) ) {
	s_autoDismissArmed = qfalse; s_menuVisible = qtrue; } }
void WiredUI_SetActiveMenu( int menu ) { s_autoDismissArmed = qfalse;
	s_menuVisible = menu == UIMENU_NONE ? qfalse : qtrue; }
void WiredUI_SetLoadingMenu( const char *relPath ) { (void)relPath; }
void WiredUI_UnregisterLevelViewportProviders( void ) {}
