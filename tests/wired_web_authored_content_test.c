// SPDX-License-Identifier: GPL-3.0-or-later

#include "../code/web/web_authored_content.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int main( void ) {
	static const char good[] =
		"WAC1|0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
		"MENU|6d61696e|5|10.67|41.7|3.9|8|25\n"
		"ITEM|6d656e755f63616d706169676e|43414d504149474e|6e6f7420696e207468697320616c706861\n"
		"L10N|7363656e652f6172656e61312f6772656574|57656c636f6d65\n"
		"SCENE|6172656e6131|8|0|90|70|4\n"
		"CURVE|0|1\n"
		"EYE|0|700|1200|80\n"
		"EYE|8|1350|1600|120\n"
		"EVENT|73746f70|8000|0|0|\nEND\n";
	wiredWebAuthoredCatalog_t catalog;
	wiredScene_t scene;
	char error[160];
	CHECK( WiredWebAuthored_Decode( good, sizeof( good ) - 1u, &catalog, error, sizeof( error ) ) );
	CHECK( catalog.schemaVersion == WIRED_WEB_AUTHORED_SCHEMA_VERSION );
	CHECK( !strcmp( catalog.menuName, "main" ) && catalog.itemCount == 1 );
	CHECK( !strcmp( catalog.items[0].label, "CAMPAIGN" ) );
	CHECK( catalog.l10nCount == 1 && !strcmp( catalog.l10n[0].value, "Welcome" ) );
	CHECK( catalog.scene.eyePath.numKnots == 2 && catalog.scene.numEvents == 1 );
	CHECK( !WiredWebAuthored_Decode( "WAC1|bad\nEND\n", 13u, &catalog, error, sizeof( error ) ) );
	CHECK( !WiredWebAuthored_Decode( good, 90u, &catalog, error, sizeof( error ) ) );
	memset( &scene, 0, sizeof( scene ) );
	CHECK( !WiredWebAuthored_LoadScene( &scene, "scripts/scene/arena1.lua" ) );
	puts( "wired Web authored-content decoder contract: PASS" );
	return 0;
}
