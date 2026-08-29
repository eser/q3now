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
	int sawHeader = 0, sawPalette = 0, sawMenu = 0, sawServer = 0;
	int sawConsole = 0, sawLoading = 0, sawHud = 0, sawScene = 0, sawEnd = 0;
	int decodedServerColumns = 0;
	int curveType = WCURVE_CATMULLROM, boundary = WCURVE_BT_FREE;
	if ( !bytes || !size || !out || size > 131072u ) {
		SetError( error, errorSize, "invalid authored catalog extent" ); return 0;
	}
	memset( out, 0, sizeof( *out ) );
	while ( offset < size ) {
		char line[4096], *cursor, *kind;
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
		} else if ( !strcmp( kind, "PALETTE" ) ) {
			vec4_t *colors[] = { &out->palette.ink, &out->palette.panel,
				&out->palette.line, &out->palette.bone, &out->palette.boneDim,
				&out->palette.accent, &out->palette.accentDim,
				&out->palette.accentSoft };
			int i, channel;
			if ( !sawHeader || sawPalette ) {
				SetError( error, errorSize, "invalid authored palette record" ); return 0;
			}
			for ( i = 0; i < 8; ++i ) {
				for ( channel = 0; channel < 4; ++channel ) {
					if ( !FloatField( &cursor, &( *colors[i] )[channel] )
							|| ( *colors[i] )[channel] < 0.0f || ( *colors[i] )[channel] > 1.0f ) {
						SetError( error, errorSize, "invalid authored palette color" ); return 0;
					}
				}
			}
			sawPalette = 1;
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
				|| !DecodeHex( Field( &cursor ), item->subtitle, sizeof( item->subtitle ) )
				|| !IntField( &cursor, &item->actionKind )
				|| item->actionKind < WIRED_WEB_AUTHORED_ACTION_OPEN
				|| item->actionKind > WIRED_WEB_AUTHORED_ACTION_EXEC
				|| !DecodeHex( Field( &cursor ), item->action, sizeof( item->action ) )
				|| !item->action[0] ) {
				SetError( error, errorSize, "invalid authored menu item" ); return 0;
			}
		} else if ( !strcmp( kind, "CARD" ) ) {
			wiredWebAuthoredCard_t *card;
			int declaredLines, i;
			if ( !sawMenu || out->cardCount >= WIRED_WEB_AUTHORED_MAX_CARDS ) {
				SetError( error, errorSize, "invalid authored card count" ); return 0;
			}
			card = &out->cards[out->cardCount++];
			if ( !DecodeHex( Field( &cursor ), card->id, sizeof( card->id ) )
					|| !FloatField( &cursor, &card->heightPercent )
					|| !IntField( &cursor, &declaredLines ) || card->heightPercent <= 0.0f
					|| declaredLines < 1 || declaredLines > WIRED_WEB_AUTHORED_MAX_CARD_LINES ) {
				SetError( error, errorSize, "invalid authored card" ); return 0;
			}
			for ( i = 0; i < declaredLines; ++i ) {
				if ( !DecodeHex( Field( &cursor ), card->lines[i], sizeof( card->lines[i] ) )
						|| !DecodeHex( Field( &cursor ), card->lineBindings[i],
							sizeof( card->lineBindings[i] ) ) ) {
					SetError( error, errorSize, "invalid authored card line" ); return 0;
				}
			}
			card->lineCount = declaredLines;
		} else if ( !strcmp( kind, "SERVER" ) ) {
			wiredWebAuthoredServerBrowser_t *server = &out->serverBrowser;
			int declaredColumns;
			if ( sawServer || !DecodeHex( Field( &cursor ), server->menuName,
					sizeof( server->menuName ) )
					|| !FloatField( &cursor, &server->rowHeight )
					|| !IntField( &cursor, &declaredColumns )
					|| server->rowHeight <= 0.0f || declaredColumns < 1
					|| declaredColumns > WIRED_WEB_AUTHORED_MAX_SERVER_COLUMNS ) {
				SetError( error, errorSize, "invalid authored server browser" ); return 0;
			}
			server->columnCount = declaredColumns;
			sawServer = 1;
		} else if ( !strcmp( kind, "SCOL" ) ) {
			wiredWebAuthoredServerBrowser_t *server = &out->serverBrowser;
			wiredWebAuthoredServerColumn_t *column;
			if ( !sawServer || decodedServerColumns >= server->columnCount ) {
				SetError( error, errorSize, "invalid authored server column count" ); return 0;
			}
			column = &server->columns[decodedServerColumns++];
			if ( !DecodeHex( Field( &cursor ), column->title, sizeof( column->title ) )
					|| !FloatField( &cursor, &column->widthPercent )
					|| column->widthPercent <= 0.0f || column->widthPercent > 100.0f ) {
				SetError( error, errorSize, "invalid authored server column" ); return 0;
			}
		} else if ( !strcmp( kind, "CONSOLE" ) ) {
			wiredWebAuthoredConsole_t *console = &out->console;
			if ( sawConsole || !DecodeHex( Field( &cursor ), console->menuName,
					sizeof( console->menuName ) )
					|| !FloatField( &cursor, &console->heightPercent )
					|| console->heightPercent <= 0.0f || console->heightPercent > 100.0f ) {
				SetError( error, errorSize, "invalid authored console panel" ); return 0;
			}
			sawConsole = 1;
		} else if ( !strcmp( kind, "LOADING" ) ) {
			wiredWebAuthoredLoading_t *loading = &out->loading;
			if ( sawLoading || !DecodeHex( Field( &cursor ), loading->menuName,
					sizeof( loading->menuName ) )
					|| !FloatField( &cursor, &loading->topBarHeightPercent )
					|| !FloatField( &cursor, &loading->leftWidthPercent )
					|| !FloatField( &cursor, &loading->dividerWidthPercent )
					|| !FloatField( &cursor, &loading->bottomHeightPercent )
					|| !FloatField( &cursor, &loading->phaseHeightPercent )
					|| !FloatField( &cursor, &loading->overallBarHeightPercent )
					|| !DecodeHex( Field( &cursor ), loading->footerText,
						sizeof( loading->footerText ) )
					|| loading->topBarHeightPercent <= 0.0f
					|| loading->leftWidthPercent <= 0.0f || loading->leftWidthPercent >= 100.0f
					|| loading->dividerWidthPercent <= 0.0f
					|| loading->bottomHeightPercent <= 0.0f
					|| loading->phaseHeightPercent <= 0.0f
					|| loading->overallBarHeightPercent <= 0.0f
					|| !loading->footerText[0] ) {
				SetError( error, errorSize, "invalid authored loading screen" ); return 0;
			}
			sawLoading = 1;
		} else if ( !strcmp( kind, "SOURCE" ) ) {
			char *sourcePath;
			int i;
			if ( out->sourceCount >= WIRED_WEB_AUTHORED_MAX_SOURCES ) {
				SetError( error, errorSize, "too many authored source paths" ); return 0;
			}
			sourcePath = out->sourcePaths[out->sourceCount];
			if ( !DecodeHex( Field( &cursor ), sourcePath, sizeof( out->sourcePaths[0] ) )
					|| !sourcePath[0] ) {
				SetError( error, errorSize, "invalid authored source path" ); return 0;
			}
			for ( i = 0; i < out->sourceCount; ++i ) {
				if ( !strcmp( out->sourcePaths[i], sourcePath ) ) {
					SetError( error, errorSize, "duplicate authored source path" ); return 0;
				}
			}
			++out->sourceCount;
		} else if ( !strcmp( kind, "ROOT" ) ) {
			wiredWebAuthoredRoot_t *root;
			int i;
			if ( out->rootCount >= WIRED_WEB_AUTHORED_MAX_ROOTS ) {
				SetError( error, errorSize, "too many authored MENU/POPUP roots" ); return 0;
			}
			root = &out->roots[out->rootCount];
			if ( !DecodeHex( Field( &cursor ), root->sourcePath, sizeof( root->sourcePath ) )
					|| !DecodeHex( Field( &cursor ), root->menuName, sizeof( root->menuName ) )
					|| !IntField( &cursor, &root->layer )
					|| !root->sourcePath[0] || !root->menuName[0]
					|| ( root->layer != WIRED_WEB_AUTHORED_ROOT_MENU
						&& root->layer != WIRED_WEB_AUTHORED_ROOT_POPUP ) ) {
				SetError( error, errorSize, "invalid authored MENU/POPUP root" ); return 0;
			}
			for ( i = 0; i < out->rootCount; ++i ) {
				if ( !strcmp( out->roots[i].menuName, root->menuName ) ) {
					SetError( error, errorSize, "duplicate authored MENU/POPUP root" ); return 0;
				}
			}
			++out->rootCount;
		} else if ( !strcmp( kind, "L10N" ) ) {
			wiredWebAuthoredL10nEntry_t *entry;
			if ( out->l10nCount >= WIRED_WEB_AUTHORED_MAX_L10N ) { SetError( error, errorSize, "too many authored translations" ); return 0; }
			entry = &out->l10n[out->l10nCount++];
			if ( !DecodeHex( Field( &cursor ), entry->key, sizeof( entry->key ) )
				|| !DecodeHex( Field( &cursor ), entry->value, sizeof( entry->value ) ) ) {
				SetError( error, errorSize, "invalid authored translation" ); return 0;
			}
		} else if ( !strcmp( kind, "HUD" ) ) {
			wiredWebAuthoredHud_t *hud = &out->hud;
			if ( sawHud || !FloatField( &cursor, &hud->leftInsetPercent )
				|| !FloatField( &cursor, &hud->rightInsetPercent )
				|| !FloatField( &cursor, &hud->bottomInsetPercent )
				|| !FloatField( &cursor, &hud->healthWidthPercent )
				|| !FloatField( &cursor, &hud->armorWidthPercent )
				|| !FloatField( &cursor, &hud->ammoWidthPercent )
				|| !FloatField( &cursor, &hud->panelHeightPercent ) ) {
				SetError( error, errorSize, "invalid authored HUD record" ); return 0;
			}
			sawHud = 1;
		} else if ( !strcmp( kind, "XHAIR" ) ) {
			wiredWebAuthoredCrosshair_t *crosshair;
			if ( out->crosshairCount >= WIRED_WEB_AUTHORED_MAX_CROSSHAIRS ) {
				SetError( error, errorSize, "too many authored crosshairs" ); return 0;
			}
			crosshair = &out->crosshairs[out->crosshairCount++];
			if ( !IntField( &cursor, &crosshair->weapon )
				|| !IntField( &cursor, &crosshair->dynamicKind )
				|| !FloatField( &cursor, &crosshair->color[0] )
				|| !FloatField( &cursor, &crosshair->color[1] )
				|| !FloatField( &cursor, &crosshair->color[2] )
				|| !FloatField( &cursor, &crosshair->color[3] )
				|| !FloatField( &cursor, &crosshair->gap )
				|| !FloatField( &cursor, &crosshair->armLength )
				|| !FloatField( &cursor, &crosshair->armThickness )
				|| !IntField( &cursor, &crosshair->dotEnabled )
				|| !FloatField( &cursor, &crosshair->dotRadius )
				|| !IntField( &cursor, &crosshair->ringEnabled )
				|| !FloatField( &cursor, &crosshair->ringRadius )
				|| !FloatField( &cursor, &crosshair->ringThickness )
				|| !FloatField( &cursor, &crosshair->outlineThickness )
				|| !FloatField( &cursor, &crosshair->outlineAlpha ) ) {
				SetError( error, errorSize, "invalid authored crosshair record" ); return 0;
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
	if ( !sawHeader || !sawPalette || !sawMenu || !sawServer || !sawConsole || !sawLoading
		|| decodedServerColumns != out->serverBrowser.columnCount
		|| !sawHud || out->crosshairCount < 1
		|| out->itemCount < 1 || out->cardCount != WIRED_WEB_AUTHORED_MAX_CARDS
		|| out->sourceCount < 1 || out->rootCount < 1 || out->l10nCount < 1
		|| !sawScene || out->scene.eyePath.numKnots < 2 || !sawEnd ) {
		SetError( error, errorSize, "incomplete authored catalog" ); return 0;
	}
	{
		int rootIndex, sourceIndex;
		for ( rootIndex = 0; rootIndex < out->rootCount; ++rootIndex ) {
			for ( sourceIndex = 0; sourceIndex < out->sourceCount; ++sourceIndex ) {
				if ( !strcmp( out->roots[rootIndex].sourcePath,
						out->sourcePaths[sourceIndex] ) ) break;
			}
			if ( sourceIndex == out->sourceCount ) {
				SetError( error, errorSize, "authored root source is absent from manifest" ); return 0;
			}
		}
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
