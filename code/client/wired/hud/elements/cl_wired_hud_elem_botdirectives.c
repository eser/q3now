/*
===========================================================================
cl_wired_hud_elem_botdirectives.c — Wired UI HUD: bot coaching directive list

Renders a text panel listing every active bot and their current coaching
directive.  Data is pushed from cgame via WiredStore key
"game.bots.directives" each frame.

Example output:
  Sarge   ^3> Heavy Armor
  Keel    ^1X Visor
  Sorlag  ^5# Defend
===========================================================================
*/

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t		config;
	modernhudTextContext_t	ctx;
} modernHudElementBotDirectives_t;

void* CG_ModernHUDElementBotDirectivesCreate(const modernhudConfig_t* config)
{
	modernHudElementBotDirectives_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_ModernHUDElementBotDirectivesRoutine(void* context)
{
	modernHudElementBotDirectives_t* element = (modernHudElementBotDirectives_t*)context;
	static char buffer[MAX_STRING_CHARS];
	wuiStoreEntry_t *e;

	e = WiredStore_Get( "game.bots.directives" );
	if ( !e || !e->text[0] ) {
		if ( !ModernHUD_CHECK_SHOW_EMPTY(element) ) {
			return;
		}
		Q_strncpyz( buffer, "", sizeof(buffer) );
	} else {
		Q_strncpyz( buffer, e->text, sizeof(buffer) );
	}

	element->ctx.text = buffer;
	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif /* FEAT_WIRED_UI */
