// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_WEB_MAIN_H
#define WIRED_WEB_MAIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIRED_WEB_CLIENT_SCHEMA_VERSION 1u
#define WIRED_WEB_GAMECL_MODULE_READY 0x1u
#define WIRED_WEB_GAMESV_MODULE_READY 0x2u
#define WIRED_WEB_ARENA17_CONTENT_READY 0x4u
#define WIRED_WEB_ARENA_CLIENT_ACTIVE 0x1u
#define WIRED_WEB_ARENA_MAP_EXACT 0x2u
#define WIRED_WEB_ARENA_PRESENTED 0x4u
#define WIRED_WEB_ARENA_WORLD_SUBMITTED 0x8u
#define WIRED_WEB_ARENA_UI_SUBMITTED 0x10u
#define WIRED_WEB_AUTHORED_READY 0x1u
#define WIRED_WEB_AUTHORED_MENU_READY 0x2u
#define WIRED_WEB_AUTHORED_L10N_READY 0x4u
#define WIRED_WEB_AUTHORED_SCENE_LOADED 0x8u
#define WIRED_WEB_AUTHORED_MENU_RENDERED 0x10u

typedef enum {
	WIRED_WEB_CLIENT_IDLE = 0,
	WIRED_WEB_CLIENT_RUNNING = 1,
	WIRED_WEB_CLIENT_STOPPED = 2,
	WIRED_WEB_CLIENT_FAILED = 3
} wiredWebClientStatus_t;

typedef struct {
	uint32_t schemaVersion;
	uint32_t status;
	uint64_t frameCount;
	double lastFrameTimeMs;
} wiredWebClientReceipt_t;

int WiredWeb_ClientStart( void );
int WiredWeb_ClientFrame( double frameTimeMs );
void WiredWeb_ClientShutdown( void );
int WiredWeb_ClientStatus( void );
int WiredWeb_ClientReceipt( wiredWebClientReceipt_t *outReceipt );
uint32_t WiredWeb_ContentProbe( void );
uint32_t WiredWeb_ArenaReceiptProbe( void );
uint32_t WiredWeb_AuthoredReceiptProbe( void );

#ifdef __cplusplus
}
#endif

#endif
