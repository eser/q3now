/*
===========================================================================
cl_wired_hud_elem_statusbar_icon.c — Generic bound icon element

Looks up a named binding and renders its icon shader handle.
The .hud file specifies: hudElement "statusbar_icon" bind "armor"
cgame selects the right icon (e.g., heavy/combat/jacket armor) — client just draws.
===========================================================================
*/

#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"
#include "cl_wired_hud.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI

typedef struct {
	superhudConfig_t     config;
	superhudDrawContext_t ctx;
} shudElementStatusbarIcon_t;

void *CG_SHUDElementStatusbarIconCreate( const superhudConfig_t *config ) {
	shudElementStatusbarIcon_t *element;

	SHUD_ELEMENT_INIT( element, config );
	CG_SHUDDrawMakeContext( &element->config, &element->ctx );

	return element;
}

void CG_SHUDElementStatusbarIconRoutine( void *context ) {
	shudElementStatusbarIcon_t *element = (shudElementStatusbarIcon_t *)context;

	if ( !element->config.bind.isSet ) return;

	/* Try Wired Store first, fall back to old binding */
	{
		char iconKey[128];
		wuiStoreEntry_t *storeEntry;

		Com_sprintf( iconKey, sizeof( iconKey ), "player.%s.icon", element->config.bind.value );
		storeEntry = WiredStore_Get( iconKey );
		if ( storeEntry && storeEntry->icon ) {
			element->ctx.image = storeEntry->icon;
			CG_SHUDDrawStretchPicCtx( &element->config, &element->ctx );
			return;
		}
	}

	/* Fall back to old binding system */
	{
		const wiredHudBinding_t *binding;

		binding = WiredHud_FindBinding( element->config.bind.value );
		if ( !binding || !binding->visible || !binding->icon ) return;

		element->ctx.image = binding->icon;
		CG_SHUDDrawStretchPicCtx( &element->config, &element->ctx );
	}
}

void CG_SHUDElementStatusbarIconDestroy( void *context ) {
	if ( context ) Z_Free( context );
}

#endif // FEAT_WIRED_UI
