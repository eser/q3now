// cl_wired_hud_elem_player_stats.c — Aggregate player stats (DG, DR, ratio)
// DG = damage given (sum of all weapon damage)
// DR = damage received (approximated from deaths * 100, since exact DR isn't tracked)
// Ratio = DG / DR
#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct {
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
	int mode;  // 0=DG, 1=DR, 2=DG icon, 3=DR icon, 4=ratio
} modernHudElementPlayerStats_t;

static void *PlayerStats_Create( const modernhudConfig_t *config, int mode ) {
	modernHudElementPlayerStats_t *element;
	ModernHUD_ELEMENT_INIT( element, config );
	element->mode = mode;

	if ( mode <= 1 || mode == 4 ) {
		if ( !element->config.text.isSet ) {
			element->config.text.isSet = qtrue;
			Q_strncpyz( element->config.text.value, "%i", sizeof( element->config.text.value ) );
		}
		CG_ModernHUDTextMakeContext( &element->config, &element->ctx );
		CG_ModernHUDFillAndFrameForText( &element->config, &element->ctx );
	}

	return element;
}

void *CG_ModernHUDElementCreatePlayerStatsDG( const modernhudConfig_t *config )          { return PlayerStats_Create( config, 0 ); }
void *CG_ModernHUDElementCreatePlayerStatsDR( const modernhudConfig_t *config )          { return PlayerStats_Create( config, 1 ); }
void *CG_ModernHUDElementCreatePlayerStatsDGIcon( const modernhudConfig_t *config )      { return PlayerStats_Create( config, 2 ); }
void *CG_ModernHUDElementCreatePlayerStatsDRIcon( const modernhudConfig_t *config )      { return PlayerStats_Create( config, 3 ); }
void *CG_ModernHUDElementCreatePlayerStatsDamageRatio( const modernhudConfig_t *config ) { return PlayerStats_Create( config, 4 ); }

void CG_ModernHUDElementPlayerStatsRoutine( void *context ) {
	modernHudElementPlayerStats_t *element = (modernHudElementPlayerStats_t *)context;
	int i, totalDG = 0, totalKills = 0, totalDeaths = 0;
	float ratio;

	if ( !element ) return;

	// aggregate stats across all weapons
	for ( i = ATT_NONE + 1; i < ATT_NUM_ATTACKS; i++ ) {
		totalDG     += wiredHud->attackStats[i].damage;
		totalKills  += wiredHud->attackStats[i].kills;
		totalDeaths += wiredHud->attackStats[i].deaths;
	}

	switch ( element->mode ) {
		case 0: // DG — damage given
			if ( totalDG <= 0 && !ModernHUD_CHECK_SHOW_EMPTY( element ) ) return;
			element->ctx.text = va( element->config.text.value, totalDG );
			CG_ModernHUDTextPrint( &element->config, &element->ctx );
			break;

		case 1: // DR — damage received (deaths × 100 as proxy)
		{
			int dr = totalDeaths * 100;  // rough proxy since exact DR isn't tracked
			if ( dr <= 0 && !ModernHUD_CHECK_SHOW_EMPTY( element ) ) return;
			element->ctx.text = va( element->config.text.value, dr );
			CG_ModernHUDTextPrint( &element->config, &element->ctx );
			break;
		}

		case 2: // DG icon
		case 3: // DR icon
			CG_ModernHUDFill( &element->config );
			break;

		case 4: // damage ratio (DG / DR)
		{
			int dr = totalDeaths * 100;
			if ( dr > 0 ) {
				ratio = (float)totalDG / (float)dr;
			} else {
				ratio = ( totalDG > 0 ) ? 99.9f : 0.0f;
			}
			element->ctx.text = va( "%.1f", ratio );
			CG_ModernHUDTextPrint( &element->config, &element->ctx );
			break;
		}
	}
}

void CG_ModernHUDElementPlayerStatsDestroy( void *context ) {
	if ( context ) {
		Z_Free( context );
	}
}

#endif // FEAT_WIRED_UI
