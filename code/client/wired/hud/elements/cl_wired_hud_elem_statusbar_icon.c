// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_hud_elem_statusbar_icon.c — Generic bound icon element.
*/

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_hud.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI

typedef struct {
	modernhudConfig_t     config;
	modernhudDrawContext_t ctx;
} modernHudElementStatusbarIcon_t;

void *CG_ModernHUDElementStatusbarIconCreate( const modernhudConfig_t *config ) {
	modernHudElementStatusbarIcon_t *element;

	ModernHUD_ELEMENT_INIT( element, config );
	CG_ModernHUDDrawMakeContext( &element->config, &element->ctx );

	return element;
}

void CG_ModernHUDElementStatusbarIconRoutine( void *context ) {
	modernHudElementStatusbarIcon_t *element = (modernHudElementStatusbarIcon_t *)context;

	if ( !element->config.bind.isSet ) return;

	/* Try Wired Store first, fall back to old binding */
	{
		char iconKey[128];
		wuiStoreEntry_t *storeEntry;

		Com_sprintf( iconKey, sizeof( iconKey ), "player.%s.icon", element->config.bind.value );
		storeEntry = WiredStore_Get( iconKey );
		if ( storeEntry && storeEntry->icon ) {
			element->ctx.image = storeEntry->icon;
			CG_ModernHUDDrawStretchPicCtx( &element->config, &element->ctx );
			return;
		}
	}

	/* Fall back to old binding system */
	{
		const wiredHudBinding_t *binding;

		binding = WiredHud_FindBinding( element->config.bind.value );
		if ( !binding || !binding->visible || !binding->icon ) return;

		element->ctx.image = binding->icon;
		CG_ModernHUDDrawStretchPicCtx( &element->config, &element->ctx );
	}
}

#endif // FEAT_WIRED_UI
