// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_hud_elem_statusbar_value.c — Generic bound text-value element.
*/

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_hud.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI

typedef struct {
	modernhudConfig_t    config;
	modernhudTextContext_t ctx;
} modernHudElementStatusbarValue_t;

void *CG_ModernHUDElementStatusbarValueCreate( const modernhudConfig_t *config ) {
	modernHudElementStatusbarValue_t *element;

	ModernHUD_ELEMENT_INIT( element, config );

	if ( !element->config.text.isSet ) {
		Q_strncpyz( element->config.text.value, "%s", sizeof( element->config.text.value ) );
	}

	CG_ModernHUDTextMakeContext( &element->config, &element->ctx );
	CG_ModernHUDFillAndFrameForText( &element->config, &element->ctx );
	element->ctx.flags |= DS_FORCE_COLOR;

	return element;
}

void CG_ModernHUDElementStatusbarValueRoutine( void *context ) {
	modernHudElementStatusbarValue_t *element = (modernHudElementStatusbarValue_t *)context;

	if ( !element->config.bind.isSet ) return;

	/* Try Wired Store first, fall back to old binding */
	{
		char storeKey[128];
		wuiStoreEntry_t *storeEntry;
		wuiStoreEntry_t *colorEntry;
		char colorKey[128];

		Com_sprintf( storeKey, sizeof( storeKey ), "player.%s.text", element->config.bind.value );
		storeEntry = WiredStore_Get( storeKey );
		if ( storeEntry && storeEntry->text[0] ) {
			Com_sprintf( colorKey, sizeof( colorKey ), "player.%s.color", element->config.bind.value );
			colorEntry = WiredStore_Get( colorKey );
			if ( colorEntry ) {
				Vector4Copy( colorEntry->color, element->config.color.value.rgba );
			}

			element->ctx.text = storeEntry->text;
			CG_ModernHUDFill( &element->config );
			CG_ModernHUDTextPrint( &element->config, &element->ctx );
			return;
		}
	}

	/* Fall back to old binding system */
	{
		const wiredHudBinding_t *binding;

		binding = WiredHud_FindBinding( element->config.bind.value );
		if ( !binding || !binding->visible ) return;

		element->ctx.text = (char *)binding->text;
		Vector4Copy( binding->color, element->config.color.value.rgba );

		CG_ModernHUDFill( &element->config );
		CG_ModernHUDTextPrint( &element->config, &element->ctx );
	}
}

#endif // FEAT_WIRED_UI
