// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_hud_elem_botdirectives.c — Bot coaching directive list from WiredStore.
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

	WHUD_ELEMENT_INIT_TEXT( element, config );

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
