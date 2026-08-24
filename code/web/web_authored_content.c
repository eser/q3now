// SPDX-License-Identifier: GPL-3.0-or-later

#include "web_authored_content.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIRED_WEB_AUTHORED_STANDALONE
#include "../qcommon/qcommon.h"
#endif

#define AUTHORED_RUNTIME_READY 0x1u
#define AUTHORED_MENU_READY 0x2u
#define AUTHORED_L10N_READY 0x4u
#define AUTHORED_SCENE_LOADED 0x8u
#define AUTHORED_MENU_RENDERED 0x10u

static wiredWebAuthoredCatalog_t s_catalog;
static wiredWebAuthoredStatus_t s_status;
static uint32_t s_receipt;

static void SetError( char *out, size_t size, const char *text ) {
	if ( out && size ) snprintf( out, size, "%s", text ? text : "authored content error" );
}

static int HexNibble( int c ) {
	if ( c >= '0' && c <= '9' ) return c - '0';
	c = tolower( c );
	return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

static int DecodeHex( const char *hex, char *out, size_t size ) {
	size_t i, count;
	if ( !hex || !out || !size ) return 0;
	count = strlen( hex );
	if ( ( count & 1u ) != 0u || count / 2u >= size ) return 0;
	for ( i = 0; i < count; i += 2 ) {
		int hi = HexNibble( (unsigned char)hex[i] );
		int lo = HexNibble( (unsigned char)hex[i + 1] );
		if ( hi < 0 || lo < 0 ) return 0;
		out[i / 2u] = (char)( ( hi << 4 ) | lo );
	}
	out[count / 2u] = '\0';
	return 1;
}

static char *Field( char **cursor ) {
	char *start, *end;
	if ( !cursor || !*cursor ) return NULL;
	start = *cursor;
	end = strchr( start, '|' );
	if ( end ) { *end = '\0'; *cursor = end + 1; }
	else *cursor = NULL;
	return start;
}

static int FloatField( char **cursor, float *out ) {
	char *field = Field( cursor ), *end = NULL;
	float value;
	if ( !field || !*field || !out ) return 0;
	value = strtof( field, &end );
	if ( !end || *end != '\0' ) return 0;
	*out = value;
	return 1;
}

static int IntField( char **cursor, int *out ) {
	char *field = Field( cursor ), *end = NULL;
	long value;
	if ( !field || !*field || !out ) return 0;
	value = strtol( field, &end, 10 );
	if ( !end || *end != '\0' || value < -2147483647L || value > 2147483647L ) return 0;
	*out = (int)value;
	return 1;
}

static int EventType( const char *verb ) {
	static const char *names[] = { "wait", "target", "fov", "fadeout", "fadein",
		"feather", "stop", "scene", "thirdperson", "hud", "playerfreeze", "caption" };
	int i;
	for ( i = 0; i < (int)( sizeof( names ) / sizeof( names[0] ) ); ++i )
		if ( strcmp( verb, names[i] ) == 0 ) return i;
	return -1;
}

int WiredWebAuthored_Decode( const char *bytes, size_t size,
		wiredWebAuthoredCatalog_t *out, char *error, size_t errorSize ) {
	size_t offset = 0;
	int sawHeader = 0, sawMenu = 0, sawScene = 0, sawEnd = 0;
	int curveType = WCURVE_CATMULLROM, boundary = WCURVE_BT_FREE;
	if ( !bytes || !size || !out || size > 131072u ) {
		SetError( error, errorSize, "invalid authored catalog extent" ); return 0;
	}
	memset( out, 0, sizeof( *out ) );
	while ( offset < size ) {
		char line[1024], *cursor, *kind;
		size_t start = offset, length;
		while ( offset < size && bytes[offset] != '\n' ) ++offset;
		length = offset - start;
		if ( offset < size ) ++offset;
		if ( length && bytes[start + length - 1] == '\r' ) --length;
		if ( !length ) continue;
		if ( length >= sizeof( line ) ) { SetError( error, errorSize, "authored catalog line too long" ); return 0; }
		memcpy( line, bytes + start, length ); line[length] = '\0';
		cursor = line; kind = Field( &cursor );
		if ( !strcmp( kind, "WAC1" ) ) {
			char *digest = Field( &cursor ); size_t i;
			if ( sawHeader || !digest || strlen( digest ) != 64u ) { SetError( error, errorSize, "invalid authored catalog header" ); return 0; }
			for ( i = 0; i < 64u; ++i ) if ( HexNibble( digest[i] ) < 0 ) { SetError( error, errorSize, "invalid authored source digest" ); return 0; }
			memcpy( out->sourceDigest, digest, 65u ); out->schemaVersion = WIRED_WEB_AUTHORED_SCHEMA_VERSION; sawHeader = 1;
		} else if ( !strcmp( kind, "MENU" ) ) {
			char *name = Field( &cursor );
			if ( !sawHeader || sawMenu || !DecodeHex( name, out->menuName, sizeof( out->menuName ) )
				|| !FloatField( &cursor, &out->leftXPercent ) || !FloatField( &cursor, &out->leftYPercent )
				|| !FloatField( &cursor, &out->leftWidthPercent ) || !FloatField( &cursor, &out->rightInsetPercent )
				|| !FloatField( &cursor, &out->rightYPercent ) || !FloatField( &cursor, &out->rightWidthPercent ) ) {
				SetError( error, errorSize, "invalid authored menu record" ); return 0;
			}
			sawMenu = 1;
		} else if ( !strcmp( kind, "ITEM" ) ) {
			wiredWebAuthoredMenuItem_t *item;
			if ( !sawMenu || out->itemCount >= WIRED_WEB_AUTHORED_MAX_ITEMS ) { SetError( error, errorSize, "invalid authored menu item count" ); return 0; }
			item = &out->items[out->itemCount++];
			if ( !DecodeHex( Field( &cursor ), item->id, sizeof( item->id ) )
				|| !DecodeHex( Field( &cursor ), item->label, sizeof( item->label ) )
				|| !DecodeHex( Field( &cursor ), item->subtitle, sizeof( item->subtitle ) ) ) {
				SetError( error, errorSize, "invalid authored menu item" ); return 0;
			}
		} else if ( !strcmp( kind, "L10N" ) ) {
			wiredWebAuthoredL10nEntry_t *entry;
			if ( out->l10nCount >= WIRED_WEB_AUTHORED_MAX_L10N ) { SetError( error, errorSize, "too many authored translations" ); return 0; }
			entry = &out->l10n[out->l10nCount++];
			if ( !DecodeHex( Field( &cursor ), entry->key, sizeof( entry->key ) )
				|| !DecodeHex( Field( &cursor ), entry->value, sizeof( entry->value ) ) ) {
				SetError( error, errorSize, "invalid authored translation" ); return 0;
			}
		} else if ( !strcmp( kind, "SCENE" ) ) {
			char *name = Field( &cursor ); int space;
			if ( sawScene || !DecodeHex( name, out->sceneName, sizeof( out->sceneName ) )
				|| !FloatField( &cursor, &out->scene.totalTimeSec ) || !IntField( &cursor, &space )
				|| !FloatField( &cursor, &out->scene.fovStart ) || !FloatField( &cursor, &out->scene.fovEnd )
				|| !FloatField( &cursor, &out->scene.fovLenSec ) ) {
				SetError( error, errorSize, "invalid authored scene record" ); return 0;
			}
			out->scene.cameraSpace = space; out->scene.hasFov = 1; sawScene = 1;
		} else if ( !strcmp( kind, "CURVE" ) ) {
			if ( !sawScene || !IntField( &cursor, &curveType ) || !IntField( &cursor, &boundary )
				|| curveType < WCURVE_CATMULLROM || curveType > WCURVE_TCB
				|| boundary < WCURVE_BT_FREE || boundary > WCURVE_BT_CLOSED ) {
				SetError( error, errorSize, "invalid authored curve record" ); return 0;
			}
			WiredCurve_Init( &out->scene.eyePath, curveType, boundary );
		} else if ( !strcmp( kind, "EYE" ) ) {
			float t, pos[3], tcb[3] = { 0, 0, 0 };
			if ( !sawScene || !FloatField( &cursor, &t ) || !FloatField( &cursor, &pos[0] )
				|| !FloatField( &cursor, &pos[1] ) || !FloatField( &cursor, &pos[2] )
				|| WiredCurve_AddValue( &out->scene.eyePath, t, pos, tcb[0], tcb[1], tcb[2] ) < 0 ) {
				SetError( error, errorSize, "invalid authored eye knot" ); return 0;
			}
		} else if ( !strcmp( kind, "EVENT" ) ) {
			wiredSceneEvent_t *event; char verb[32]; int type;
			if ( out->scene.numEvents >= WIRED_MAX_SCENE_EVENTS
				|| !DecodeHex( Field( &cursor ), verb, sizeof( verb ) ) ) { SetError( error, errorSize, "invalid authored event" ); return 0; }
			type = EventType( verb ); event = &out->scene.events[out->scene.numEvents];
			if ( type < 0 || !IntField( &cursor, &event->timeMs ) || !FloatField( &cursor, &event->fparam )
				|| !FloatField( &cursor, &event->fparam2 ) || !DecodeHex( Field( &cursor ), event->sparam, sizeof( event->sparam ) ) ) {
				SetError( error, errorSize, "invalid authored event payload" ); return 0;
			}
			event->type = type; ++out->scene.numEvents;
		} else if ( !strcmp( kind, "END" ) ) sawEnd = 1;
		else { SetError( error, errorSize, "unknown authored catalog record" ); return 0; }
	}
	if ( !sawHeader || !sawMenu || out->itemCount < 1 || out->l10nCount < 1
		|| !sawScene || out->scene.eyePath.numKnots < 2 || !sawEnd ) {
		SetError( error, errorSize, "incomplete authored catalog" ); return 0;
	}
	return 1;
}

int WiredWebAuthored_EnsureLoaded( void ) {
#ifdef WIRED_WEB_AUTHORED_STANDALONE
	return s_status == WIRED_WEB_AUTHORED_STATUS_READY;
#else
	void *bytes = NULL; int length; char error[160];
	if ( s_status == WIRED_WEB_AUTHORED_STATUS_READY ) return 1;
	if ( s_status == WIRED_WEB_AUTHORED_STATUS_IO_ERROR || s_status == WIRED_WEB_AUTHORED_STATUS_CONTENT_ERROR ) return 0;
	length = FS_ReadFile( "web/authored-content.wac", &bytes );
	if ( length <= 0 || !bytes ) { s_status = WIRED_WEB_AUTHORED_STATUS_IO_ERROR; return 0; }
	if ( !WiredWebAuthored_Decode( (const char *)bytes, (size_t)length, &s_catalog, error, sizeof( error ) ) ) {
		FS_FreeFile( bytes ); s_status = WIRED_WEB_AUTHORED_STATUS_CONTENT_ERROR;
		fprintf( stderr, "WiredWeb authored-content error: %s\n", error ); return 0;
	}
	FS_FreeFile( bytes ); s_status = WIRED_WEB_AUTHORED_STATUS_READY;
	s_receipt = AUTHORED_RUNTIME_READY | AUTHORED_MENU_READY | AUTHORED_L10N_READY;
	return 1;
#endif
}

void WiredWebAuthored_Shutdown( void ) { memset( &s_catalog, 0, sizeof( s_catalog ) ); s_status = WIRED_WEB_AUTHORED_STATUS_EMPTY; s_receipt = 0u; }
const wiredWebAuthoredCatalog_t *WiredWebAuthored_Catalog( void ) { return WiredWebAuthored_EnsureLoaded() ? &s_catalog : NULL; }
const char *WiredWebAuthored_Localize( const char *key ) {
	int i; if ( !key || !WiredWebAuthored_EnsureLoaded() ) return key ? key : "";
	for ( i = 0; i < s_catalog.l10nCount; ++i ) if ( !strcmp( key, s_catalog.l10n[i].key ) ) return s_catalog.l10n[i].value;
	return key;
}
int WiredWebAuthored_LoadScene( wiredScene_t *out, const char *path ) {
	const char *base, *dot; size_t length;
	if ( !out || !path || !WiredWebAuthored_EnsureLoaded() ) return 0;
	base = strrchr( path, '/' ); base = base ? base + 1 : path; dot = strrchr( base, '.' );
	length = dot ? (size_t)( dot - base ) : strlen( base );
	if ( length != strlen( s_catalog.sceneName ) || strncmp( base, s_catalog.sceneName, length ) ) return 0;
	*out = s_catalog.scene; s_receipt |= AUTHORED_SCENE_LOADED; return 1;
}
void WiredWebAuthored_MarkMenuRendered( void ) { if ( s_status == WIRED_WEB_AUTHORED_STATUS_READY ) s_receipt |= AUTHORED_MENU_RENDERED; }
uint32_t WiredWebAuthored_Receipt( void ) { return s_receipt; }
