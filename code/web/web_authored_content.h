// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WIRED_WEB_AUTHORED_CONTENT_H
#define WIRED_WEB_AUTHORED_CONTENT_H

#include "../qcommon/q_shared.h"
#include "../qcommon/wired/scene/wired_scene.h"

#define WIRED_WEB_AUTHORED_SCHEMA_VERSION 10u
#define WIRED_WEB_AUTHORED_MAX_ITEMS 16
#define WIRED_WEB_AUTHORED_MAX_CARDS 4
#define WIRED_WEB_AUTHORED_MAX_CARD_LINES 8
#define WIRED_WEB_AUTHORED_MAX_L10N 128
#define WIRED_WEB_AUTHORED_MAX_CROSSHAIRS 8
#define WIRED_WEB_AUTHORED_MAX_SERVER_COLUMNS 8
#define WIRED_WEB_AUTHORED_MAX_ROOTS 48
#define WIRED_WEB_AUTHORED_MAX_SOURCES 64
#define WIRED_WEB_AUTHORED_RECEIPT_MENU_RENDERED 0x10u

typedef enum {
	WIRED_WEB_AUTHORED_ROOT_MENU = 1,
	WIRED_WEB_AUTHORED_ROOT_POPUP = 2
} wiredWebAuthoredRootLayer_t;

typedef struct {
	char sourcePath[96];
	char menuName[64];
	int layer;
} wiredWebAuthoredRoot_t;

typedef enum {
	WIRED_WEB_AUTHORED_STATUS_EMPTY = 0,
	WIRED_WEB_AUTHORED_STATUS_READY = 1,
	WIRED_WEB_AUTHORED_STATUS_IO_ERROR = 2,
	WIRED_WEB_AUTHORED_STATUS_CONTENT_ERROR = 3
} wiredWebAuthoredStatus_t;

typedef enum {
	WIRED_WEB_AUTHORED_ACTION_NONE = 0,
	WIRED_WEB_AUTHORED_ACTION_OPEN = 1,
	WIRED_WEB_AUTHORED_ACTION_EXEC = 2
} wiredWebAuthoredActionKind_t;

typedef struct {
	char id[64];
	char label[64];
	char subtitle[96];
	int actionKind;
	char action[64];
} wiredWebAuthoredMenuItem_t;

typedef struct {
	char id[64];
	float heightPercent;
	char lines[WIRED_WEB_AUTHORED_MAX_CARD_LINES][96];
	char lineBindings[WIRED_WEB_AUTHORED_MAX_CARD_LINES][64];
	int lineCount;
} wiredWebAuthoredCard_t;

typedef struct {
	char key[96];
	char value[256];
} wiredWebAuthoredL10nEntry_t;

typedef struct {
	float leftInsetPercent;
	float rightInsetPercent;
	float bottomInsetPercent;
	float healthWidthPercent;
	float armorWidthPercent;
	float ammoWidthPercent;
	float panelHeightPercent;
} wiredWebAuthoredHud_t;

typedef struct {
	int weapon;
	int dynamicKind;
	vec4_t color;
	float gap;
	float armLength;
	float armThickness;
	int dotEnabled;
	float dotRadius;
	int ringEnabled;
	float ringRadius;
	float ringThickness;
	float outlineThickness;
	float outlineAlpha;
} wiredWebAuthoredCrosshair_t;

typedef struct {
	vec4_t ink;
	vec4_t panel;
	vec4_t line;
	vec4_t bone;
	vec4_t boneDim;
	vec4_t accent;
	vec4_t accentDim;
	vec4_t accentSoft;
} wiredWebAuthoredPalette_t;

typedef struct {
	char title[32];
	float widthPercent;
} wiredWebAuthoredServerColumn_t;

typedef struct {
	char menuName[64];
	float rowHeight;
	wiredWebAuthoredServerColumn_t columns[WIRED_WEB_AUTHORED_MAX_SERVER_COLUMNS];
	int columnCount;
} wiredWebAuthoredServerBrowser_t;

typedef struct {
	char menuName[64];
	float heightPercent;
} wiredWebAuthoredConsole_t;

typedef struct {
	char menuName[64];
	float topBarHeightPercent;
	float leftWidthPercent;
	float dividerWidthPercent;
	float bottomHeightPercent;
	float phaseHeightPercent;
	float overallBarHeightPercent;
	char footerText[64];
} wiredWebAuthoredLoading_t;

typedef struct {
	uint32_t schemaVersion;
	char sourceDigest[65];
	char menuName[64];
	float leftXPercent, leftYPercent, leftWidthPercent;
	float rightInsetPercent, rightYPercent, rightWidthPercent;
	wiredWebAuthoredMenuItem_t items[WIRED_WEB_AUTHORED_MAX_ITEMS];
	int itemCount;
	wiredWebAuthoredCard_t cards[WIRED_WEB_AUTHORED_MAX_CARDS];
	int cardCount;
	wiredWebAuthoredL10nEntry_t l10n[WIRED_WEB_AUTHORED_MAX_L10N];
	int l10nCount;
	wiredWebAuthoredHud_t hud;
	wiredWebAuthoredCrosshair_t crosshairs[WIRED_WEB_AUTHORED_MAX_CROSSHAIRS];
	int crosshairCount;
	wiredWebAuthoredPalette_t palette;
	wiredWebAuthoredServerBrowser_t serverBrowser;
	wiredWebAuthoredConsole_t console;
	wiredWebAuthoredLoading_t loading;
	char sourcePaths[WIRED_WEB_AUTHORED_MAX_SOURCES][96];
	int sourceCount;
	wiredWebAuthoredRoot_t roots[WIRED_WEB_AUTHORED_MAX_ROOTS];
	int rootCount;
	char sceneName[64];
	wiredScene_t scene;
} wiredWebAuthoredCatalog_t;

int WiredWebAuthored_Decode( const char *bytes, size_t size,
	wiredWebAuthoredCatalog_t *out, char *error, size_t errorSize );
int WiredWebAuthored_EnsureLoaded( void );
void WiredWebAuthored_Shutdown( void );
const wiredWebAuthoredCatalog_t *WiredWebAuthored_Catalog( void );
const char *WiredWebAuthored_Localize( const char *key );
int WiredWebAuthored_LoadScene( wiredScene_t *out, const char *path );
void WiredWebAuthored_MarkMenuRendered( void );
uint32_t WiredWebAuthored_Receipt( void );

#endif
