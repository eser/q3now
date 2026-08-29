// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WIRED_WEB_UI_CLAY_H
#define WIRED_WEB_UI_CLAY_H

#include "web_ui_compat.h"
#include "web_authored_content.h"

#define WIRED_WEB_SERVER_FIXTURE_MAX 200
#define WIRED_WEB_SERVER_FIELD_COUNT 5

typedef struct {
	char fields[WIRED_WEB_SERVER_FIELD_COUNT][64];
} wiredWebServerRow_t;

typedef struct {
	float geometry;
	float shaders;
	float audio;
	float download;
	float overall;
	const char *phase;
	const char *mapName;
} wiredWebLoadingState_t;

qboolean WiredWebClay_Init( void );
void WiredWebClay_Shutdown( void );
qboolean WiredWebClay_RenderMainMenu( const wiredWebAuthoredCatalog_t *catalog,
	float width, float height, int focusedItem );
qboolean WiredWebClay_RenderServerBrowser( const wiredWebAuthoredCatalog_t *catalog,
	float width, float height, const wiredWebServerRow_t *rows, int rowCount,
	int scrollIndex, int selectedIndex, float pointerX, float pointerY,
	qboolean pointerDown, qboolean pointerPressed, int *hoveredIndex );
qboolean WiredWebClay_RenderLoading( const wiredWebAuthoredCatalog_t *catalog,
	float width, float height, const wiredWebLoadingState_t *state );
uint32_t WiredWebClay_ServerLayoutP99Micros( void );
uint32_t WiredWebClay_Receipt( void );

#endif
