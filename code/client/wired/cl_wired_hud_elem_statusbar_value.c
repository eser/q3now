/*
===========================================================================
cl_wired_hud_elem_statusbar_value.c — Generic bound text value element

Looks up a named binding from the state bridge and renders its text + color.
The .hud file specifies: hudElement "statusbar_value" bind "health"
cgame pre-computes everything — this is a pure renderer.
===========================================================================
*/

#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"
#include "cl_wired_hud.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI

typedef struct {
	superhudConfig_t    config;
	superhudTextContext_t ctx;
} shudElementStatusbarValue_t;

void *CG_SHUDElementStatusbarValueCreate( const superhudConfig_t *config ) {
	shudElementStatusbarValue_t *element;

	SHUD_ELEMENT_INIT( element, config );

	if ( !element->config.text.isSet ) {
		Q_strncpyz( element->config.text.value, "%s", sizeof( element->config.text.value ) );
	}

	CG_SHUDTextMakeContext( &element->config, &element->ctx );
	CG_SHUDFillAndFrameForText( &element->config, &element->ctx );
	element->ctx.flags |= DS_FORCE_COLOR;

	return element;
}

void CG_SHUDElementStatusbarValueRoutine( void *context ) {
	shudElementStatusbarValue_t *element = (shudElementStatusbarValue_t *)context;

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
			CG_SHUDFill( &element->config );
			CG_SHUDTextPrint( &element->config, &element->ctx );
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

		CG_SHUDFill( &element->config );
		CG_SHUDTextPrint( &element->config, &element->ctx );
	}
}

void CG_SHUDElementStatusbarValueDestroy( void *context ) {
	if ( context ) Z_Free( context );
}

#endif // FEAT_WIRED_UI
