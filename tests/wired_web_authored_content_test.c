// SPDX-License-Identifier: GPL-3.0-or-later

#include "../code/web/web_authored_content.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int main( void ) {
	static const char good[] =
		"WAC1|0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
		"PALETTE|0.1|0.1|0.1|1|0.1|0.1|0.1|0.25|0.2|0.2|0.2|1|0.9|0.9|0.9|1|0.5|0.5|0.5|1|1|0.5|0|1|0.5|0.3|0.1|1|0.8|0.4|0.1|1\n"
		"MENU|6d61696e|5|10.67|41.7|3.9|8|25\n"
		"ITEM|6d656e755f63616d706169676e|43414d504149474e|6e6f7420696e207468697320616c706861|1|63616d706169676e\n"
		"CARD|636172645f70726f66696c65|15|2|50524f46494c45||4c41524f55582e504f53|6e616d65\n"
		"CARD|636172645f6c6173745f6d61746368|24|1|4c415354204d41544348|\n"
		"CARD|636172645f676c6f62616c5f66656564|24|1|474c4f42414c2046454544|\n"
		"CARD|636172645f6172656e61|22|1|4152454e41204f462054484520444159|\n"
		"SERVER|73657276657273|24|2\n"
		"SCOL|534552564552|70\n"
		"SCOL|50494e47|30\n"
		"CONSOLE|636f6e736f6c655f70616e656c|50\n"
		"LOADING|6c6f6164696e675f73637265656e|5.8|51.9|0.3|4|2.5|0.4|6c6f6164696e672e2e2e\n"
		"SOURCE|75692f6d61696e2e777569\n"
		"ROOT|75692f6d61696e2e777569|6d61696e|1\n"
		"L10N|7363656e652f6172656e61312f6772656574|57656c636f6d65\n"
		"HUD|1.94|1.67|4|9.5|9.5|11.5|11.56\n"
		"XHAIR|0|0|1|1|1|1|3|6|1|0|0|0|0|0|1|0.8\n"
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
	CHECK( catalog.palette.accent[0] == 1.0f && catalog.palette.panel[3] == 0.25f );
	CHECK( !strcmp( catalog.items[0].label, "CAMPAIGN" ) );
	CHECK( catalog.items[0].actionKind == WIRED_WEB_AUTHORED_ACTION_OPEN );
	CHECK( !strcmp( catalog.items[0].action, "campaign" ) );
	CHECK( catalog.cardCount == 4 && catalog.cards[0].heightPercent == 15.0f );
	CHECK( !strcmp( catalog.cards[0].lines[1], "LAROUX.POS" ) );
	CHECK( !strcmp( catalog.cards[0].lineBindings[1], "name" ) );
	CHECK( !strcmp( catalog.serverBrowser.menuName, "servers" ) );
	CHECK( catalog.serverBrowser.columnCount == 2 );
	CHECK( !strcmp( catalog.serverBrowser.columns[1].title, "PING" ) );
	CHECK( !strcmp( catalog.console.menuName, "console_panel" )
		&& catalog.console.heightPercent == 50.0f );
	CHECK( !strcmp( catalog.loading.menuName, "loading_screen" )
		&& catalog.loading.leftWidthPercent == 51.9f
		&& !strcmp( catalog.loading.footerText, "loading..." ) );
	CHECK( catalog.rootCount == 1
		&& !strcmp( catalog.roots[0].sourcePath, "ui/main.wui" )
		&& !strcmp( catalog.roots[0].menuName, "main" )
		&& catalog.roots[0].layer == WIRED_WEB_AUTHORED_ROOT_MENU );
	CHECK( catalog.sourceCount == 1
		&& !strcmp( catalog.sourcePaths[0], "ui/main.wui" ) );
	CHECK( catalog.l10nCount == 1 && !strcmp( catalog.l10n[0].value, "Welcome" ) );
	CHECK( catalog.hud.leftInsetPercent == 1.94f );
	CHECK( catalog.crosshairCount == 1 && catalog.crosshairs[0].armLength == 6.0f );
	CHECK( catalog.scene.eyePath.numKnots == 2 && catalog.scene.numEvents == 1 );
	CHECK( !WiredWebAuthored_Decode( "WAC1|bad\nEND\n", 13u, &catalog, error, sizeof( error ) ) );
	CHECK( !WiredWebAuthored_Decode( good, 90u, &catalog, error, sizeof( error ) ) );
	memset( &scene, 0, sizeof( scene ) );
	CHECK( !WiredWebAuthored_LoadScene( &scene, "scripts/scene/arena1.lua" ) );
	puts( "wired Web authored-content decoder contract: PASS" );
	return 0;
}
