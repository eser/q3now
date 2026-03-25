/*
===========================================================================
cl_wired_hud_elem_statusbar_bar.c — Generic bound bar element

Looks up a named binding and renders a fill bar at the binding's percent.
The .hud file specifies: hudElement "statusbar_bar" bind "health" direction R
cgame computes the percentage and color — client just draws the bar.
===========================================================================
*/

#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"
#include "cl_wired_hud.h"

#if FEAT_WIRED_UI

typedef struct {
	superhudConfig_t    config;
	superhudBarContext_t ctx;
} shudElementStatusbarBar_t;

void *CG_SHUDElementStatusbarBarCreate( const superhudConfig_t *config ) {
	shudElementStatusbarBar_t *element;

	SHUD_ELEMENT_INIT( element, config );
	CG_SHUDBarMakeContext( &element->config, &element->ctx, 100 );

	return element;
}

void CG_SHUDElementStatusbarBarRoutine( void *context ) {
	shudElementStatusbarBar_t *element = (shudElementStatusbarBar_t *)context;
	const wiredHudBinding_t *binding;

	if ( !element->config.bind.isSet ) return;

	binding = WiredHud_FindBinding( element->config.bind.value );
	if ( !binding || !binding->visible ) return;

	CG_SHUDFill( &element->config );
	CG_SHUDDrawBorder( &element->config );

	// bar uses its own forecolor from .hud (typically white), NOT the binding color
	// binding only provides the fill percentage
	CG_SHUDBarPrint( &element->config, &element->ctx, binding->percent * 100.0f );
}

void CG_SHUDElementStatusbarBarDestroy( void *context ) {
	if ( context ) Z_Free( context );
}

#endif // FEAT_WIRED_UI
