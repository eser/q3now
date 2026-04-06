// cl_wired_hud_elem_player_stats.c — Aggregate player stats (DG, DR, ratio)
// DG = damage given (sum of all weapon damage)
// DR = damage received (approximated from deaths * 100, since exact DR isn't tracked)
// Ratio = DG / DR
#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct {
	superhudConfig_t config;
	superhudTextContext_t ctx;
	int mode;  // 0=DG, 1=DR, 2=DG icon, 3=DR icon, 4=ratio
} shudElementPlayerStats_t;

static void *PlayerStats_Create( const superhudConfig_t *config, int mode ) {
	shudElementPlayerStats_t *element;
	SHUD_ELEMENT_INIT( element, config );
	element->mode = mode;

	if ( mode <= 1 || mode == 4 ) {
		if ( !element->config.text.isSet ) {
			element->config.text.isSet = qtrue;
			Q_strncpyz( element->config.text.value, "%i", sizeof( element->config.text.value ) );
		}
		CG_SHUDTextMakeContext( &element->config, &element->ctx );
		CG_SHUDFillAndFrameForText( &element->config, &element->ctx );
	}

	return element;
}

void *CG_SHUDElementCreatePlayerStatsDG( const superhudConfig_t *config )          { return PlayerStats_Create( config, 0 ); }
void *CG_SHUDElementCreatePlayerStatsDR( const superhudConfig_t *config )          { return PlayerStats_Create( config, 1 ); }
void *CG_SHUDElementCreatePlayerStatsDGIcon( const superhudConfig_t *config )      { return PlayerStats_Create( config, 2 ); }
void *CG_SHUDElementCreatePlayerStatsDRIcon( const superhudConfig_t *config )      { return PlayerStats_Create( config, 3 ); }
void *CG_SHUDElementCreatePlayerStatsDamageRatio( const superhudConfig_t *config ) { return PlayerStats_Create( config, 4 ); }

void CG_SHUDElementPlayerStatsRoutine( void *context ) {
	shudElementPlayerStats_t *element = (shudElementPlayerStats_t *)context;
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
			if ( totalDG <= 0 && !SHUD_CHECK_SHOW_EMPTY( element ) ) return;
			element->ctx.text = va( element->config.text.value, totalDG );
			CG_SHUDTextPrint( &element->config, &element->ctx );
			break;

		case 1: // DR — damage received (deaths × 100 as proxy)
		{
			int dr = totalDeaths * 100;  // rough proxy since exact DR isn't tracked
			if ( dr <= 0 && !SHUD_CHECK_SHOW_EMPTY( element ) ) return;
			element->ctx.text = va( element->config.text.value, dr );
			CG_SHUDTextPrint( &element->config, &element->ctx );
			break;
		}

		case 2: // DG icon
		case 3: // DR icon
			CG_SHUDFill( &element->config );
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
			CG_SHUDTextPrint( &element->config, &element->ctx );
			break;
		}
	}
}

void CG_SHUDElementPlayerStatsDestroy( void *context ) {
	if ( context ) {
		Z_Free( context );
	}
}

#endif // FEAT_WIRED_UI
