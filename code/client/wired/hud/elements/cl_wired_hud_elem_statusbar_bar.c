/*
===========================================================================
cl_wired_hud_elem_statusbar_bar.c — Generic bound bar element

Looks up a named binding and renders a fill bar at the binding's percent.
The .hud file specifies: hudElement "statusbar_bar" bind "health" direction R
cgame computes the percentage and color — client just draws the bar.
===========================================================================
*/

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_hud.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI

typedef struct {
	modernhudConfig_t    config;
	modernhudBarContext_t ctx;
} modernHudElementStatusbarBar_t;

void *CG_ModernHUDElementStatusbarBarCreate( const modernhudConfig_t *config ) {
	modernHudElementStatusbarBar_t *element;

	ModernHUD_ELEMENT_INIT( element, config );
	CG_ModernHUDBarMakeContext( &element->config, &element->ctx, 100 );

	return element;
}

void CG_ModernHUDElementStatusbarBarRoutine( void *context ) {
	modernHudElementStatusbarBar_t *element = (modernHudElementStatusbarBar_t *)context;

	if ( !element->config.bind.isSet ) return;

	/* Try Wired Store first, fall back to old binding */
	{
		char pctKey[128];
		wuiStoreEntry_t *storeEntry;

		Com_sprintf( pctKey, sizeof( pctKey ), "player.%s.percent", element->config.bind.value );
		storeEntry = WiredStore_Get( pctKey );
		if ( storeEntry ) {
			CG_ModernHUDFill( &element->config );
			CG_ModernHUDDrawBorder( &element->config );
			/* bar uses its own forecolor from .hud, store provides fill percentage (0-1) */
			CG_ModernHUDBarPrint( &element->config, &element->ctx, storeEntry->value * 100.0f );
			return;
		}
	}

	/* Fall back to old binding system */
	{
		const wiredHudBinding_t *binding;

		binding = WiredHud_FindBinding( element->config.bind.value );
		if ( !binding || !binding->visible ) return;

		CG_ModernHUDFill( &element->config );
		CG_ModernHUDDrawBorder( &element->config );
		/* bar uses its own forecolor from .hud (typically white), NOT the binding color */
		/* binding only provides the fill percentage */
		CG_ModernHUDBarPrint( &element->config, &element->ctx, binding->percent * 100.0f );
	}
}

#endif // FEAT_WIRED_UI
