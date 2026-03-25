// cl_wired_hud_elem_tempAcc.c — Temporary weapon accuracy HUD element
// Shows the player's recent accuracy with a weapon, computed from the delta
// of hits/shots between bstats updates. Fades after no new data.
#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct {
	superhudConfig_t config;
	superhudTextContext_t ctx;
	qboolean isIcon;
	float lastAcc;      // last displayed accuracy
	int lastUpdateTime;  // when accuracy was last updated
} shudElementTempAcc_t;

void *CG_SHUDElementTempAccTextCreate( const superhudConfig_t *config ) {
	shudElementTempAcc_t *element;
	SHUD_ELEMENT_INIT( element, config );
	element->isIcon = qfalse;

	if ( !element->config.text.isSet ) {
		element->config.text.isSet = qtrue;
		Q_strncpyz( element->config.text.value, "%i%%", sizeof( element->config.text.value ) );
	}

	CG_SHUDTextMakeContext( &element->config, &element->ctx );
	CG_SHUDFillAndFrameForText( &element->config, &element->ctx );
	return element;
}

void *CG_SHUDElementTempAccIconCreate( const superhudConfig_t *config ) {
	shudElementTempAcc_t *element;
	SHUD_ELEMENT_INIT( element, config );
	element->isIcon = qtrue;
	return element;
}

void CG_SHUDElementTempAccRoutine( void *context ) {
	shudElementTempAcc_t *element = (shudElementTempAcc_t *)context;
	superhudGlobalContext_t *ctx = CG_SHUDGetContext();
	int wp;
	float acc;
	int fadeTime = 3000;  // fade after 3 seconds of no updates

	if ( !element ) return;

	// track the current weapon's temp accuracy
	wp = wiredHud->weapon;
	if ( wp < 0 || wp >= WP_NUM_WEAPONS ) return;

	acc = ctx->tempAcc.weapon[wp].tempAccuracy;

	// detect new accuracy data
	if ( acc != element->lastAcc ) {
		element->lastAcc = acc;
		element->lastUpdateTime = wiredHud->time;
	}

	// fade out after timeout
	if ( element->lastUpdateTime == 0 ) return;
	if ( wiredHud->time - element->lastUpdateTime > fadeTime ) {
		if ( !SHUD_CHECK_SHOW_EMPTY( element ) ) return;
	}

	if ( element->isIcon ) {
		// draw weapon icon for the current weapon
		qhandle_t icon = cg_weapons[wp].weaponIcon;
		if ( !icon ) return;

		CG_SHUDFill( &element->config );

		if ( element->config.rect.isSet ) {
			float x = element->config.rect.value[0];
			float y = element->config.rect.value[1];
			float w = element->config.rect.value[2];
			float h = element->config.rect.value[3];
			vec4_t color = { 1, 1, 1, 1 };

			// fade alpha based on time since last update
			if ( wiredHud->time - element->lastUpdateTime > fadeTime / 2 ) {
				float fadeProgress = (float)( wiredHud->time - element->lastUpdateTime - fadeTime / 2 ) / (float)( fadeTime / 2 );
				if ( fadeProgress > 1.0f ) fadeProgress = 1.0f;
				color[3] = 1.0f - fadeProgress;
			}

			trap_R_SetColor( color );
			trap_R_DrawStretchPic( x, y, w, h, 0, 0, 1, 1, icon );
			trap_R_SetColor( NULL );
		}
	} else {
		// draw accuracy percentage
		int accPct = (int)( acc * 100.0f + 0.5f );
		element->ctx.text = va( element->config.text.value, accPct );
		CG_SHUDTextPrint( &element->config, &element->ctx );
	}
}

void CG_SHUDElementTempAccDestroy( void *context ) {
	if ( context ) {
		Z_Free( context );
	}
}

#endif // FEAT_WIRED_UI
