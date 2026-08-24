// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WIRED_WEB_AUTHORED_CONTENT_H
#define WIRED_WEB_AUTHORED_CONTENT_H

#include "../qcommon/q_shared.h"
#include "../qcommon/wired/scene/wired_scene.h"

#define WIRED_WEB_AUTHORED_SCHEMA_VERSION 1u
#define WIRED_WEB_AUTHORED_MAX_ITEMS 16
#define WIRED_WEB_AUTHORED_MAX_L10N 128

typedef enum {
	WIRED_WEB_AUTHORED_STATUS_EMPTY = 0,
	WIRED_WEB_AUTHORED_STATUS_READY = 1,
	WIRED_WEB_AUTHORED_STATUS_IO_ERROR = 2,
	WIRED_WEB_AUTHORED_STATUS_CONTENT_ERROR = 3
} wiredWebAuthoredStatus_t;

typedef struct {
	char id[64];
	char label[64];
	char subtitle[96];
} wiredWebAuthoredMenuItem_t;

typedef struct {
	char key[96];
	char value[256];
} wiredWebAuthoredL10nEntry_t;

typedef struct {
	uint32_t schemaVersion;
	char sourceDigest[65];
	char menuName[64];
	float leftXPercent, leftYPercent, leftWidthPercent;
	float rightInsetPercent, rightYPercent, rightWidthPercent;
	wiredWebAuthoredMenuItem_t items[WIRED_WEB_AUTHORED_MAX_ITEMS];
	int itemCount;
	wiredWebAuthoredL10nEntry_t l10n[WIRED_WEB_AUTHORED_MAX_L10N];
	int l10nCount;
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
