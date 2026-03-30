// cl_wired_hud_elem_weapon_stats.c — Per-weapon accuracy stats HUD elements
// Text variants show "hits/shots (acc%)" for a specific weapon.
// Icon variants show the weapon icon.
// "CurrentWeapon" variants track the player's currently held weapon.
#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI

// ── shared element struct ────────────────────────────────────────────

typedef struct {
	superhudConfig_t config;
	superhudTextContext_t ctx;
	int weaponId;       // WP_MACHINEGUN..WP_PLASMA_RIFLE, or -1 for current weapon
	qboolean isIcon;    // qtrue = draw weapon icon, qfalse = draw text stats
} shudElementWeaponStats_t;

// ── create helpers ───────────────────────────────────────────────────

static void *WeaponStats_CreateText( const superhudConfig_t *config, int wp ) {
	shudElementWeaponStats_t *element;
	SHUD_ELEMENT_INIT( element, config );
	element->weaponId = wp;
	element->isIcon = qfalse;

	if ( !element->config.text.isSet ) {
		element->config.text.isSet = qtrue;
		Q_strncpyz( element->config.text.value, "%i/%i", sizeof( element->config.text.value ) );
	}

	CG_SHUDTextMakeContext( &element->config, &element->ctx );
	CG_SHUDFillAndFrameForText( &element->config, &element->ctx );
	return element;
}

static void *WeaponStats_CreateIcon( const superhudConfig_t *config, int wp ) {
	shudElementWeaponStats_t *element;
	SHUD_ELEMENT_INIT( element, config );
	element->weaponId = wp;
	element->isIcon = qtrue;
	return element;
}

// ── text Create functions ────────────────────────────────────────────

void *CG_SHUDElementCreateCurrentWeapon( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, -1 ); }
void *CG_SHUDElementWeaponStatsCreateMG( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, WP_MACHINEGUN ); }
void *CG_SHUDElementWeaponStatsCreateSG( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, WP_SHOTGUN ); }
void *CG_SHUDElementWeaponStatsCreateGL( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, WP_GRENADE_LAUNCHER ); }
void *CG_SHUDElementWeaponStatsCreateRL( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, WP_ROCKET_LAUNCHER ); }
void *CG_SHUDElementWeaponStatsCreateLG( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, WP_LIGHTNING_GUN ); }
void *CG_SHUDElementWeaponStatsCreateRG( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, WP_RAILGUN ); }
void *CG_SHUDElementWeaponStatsCreatePG( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, WP_PLASMA_RIFLE ); }

// ── icon Create functions ────────────────────────────────────────────

void *CG_SHUDElementIconCreateCurrentWeapon( const superhudConfig_t *config ) { return WeaponStats_CreateIcon( config, -1 ); }
void *CG_SHUDElementIconCreateMG( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, WP_MACHINEGUN ); }
void *CG_SHUDElementIconCreateSG( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, WP_SHOTGUN ); }
void *CG_SHUDElementIconCreateGL( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, WP_GRENADE_LAUNCHER ); }
void *CG_SHUDElementIconCreateRL( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, WP_ROCKET_LAUNCHER ); }
void *CG_SHUDElementIconCreateLG( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, WP_LIGHTNING_GUN ); }
void *CG_SHUDElementIconCreateRG( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, WP_RAILGUN ); }
void *CG_SHUDElementIconCreatePG( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, WP_PLASMA_RIFLE ); }

// ── shared Routine ───────────────────────────────────────────────────

void CG_SHUDElementWeaponStatsRoutine( void *context ) {
	shudElementWeaponStats_t *element = (shudElementWeaponStats_t *)context;
	int wp;
	int hits, shots, acc;

	if ( !element ) return;

	// resolve weapon ID (-1 = current weapon)
	wp = element->weaponId;
	if ( wp < 0 ) {
		wp = wiredHud->weapon;
	}
	if ( wp < 0 || wp >= WP_NUM_WEAPONS ) return;

	hits  = wiredHud->attackStats[wp].hits;
	shots = wiredHud->attackStats[wp].shots;
	acc   = ( shots > 0 ) ? (int)( 100.0f * hits / shots ) : 0;

	if ( element->isIcon ) {
		// draw weapon icon
		qhandle_t icon = cg_weapons[wp].weaponIcon;
		if ( !icon ) return;

		CG_SHUDFill( &element->config );

		if ( element->config.rect.isSet ) {
			float x = element->config.rect.value[0];
			float y = element->config.rect.value[1];
			float w = element->config.rect.value[2];
			float h = element->config.rect.value[3];

			if ( element->config.color.isSet ) {
				trap_R_SetColor( element->config.color.value.rgba );
			}
			trap_R_DrawStretchPic( x, y, w, h, 0, 0, 1, 1, icon );
			trap_R_SetColor( NULL );
		}
	} else {
		// draw text stats
		if ( shots <= 0 && !SHUD_CHECK_SHOW_EMPTY( element ) ) return;

		// format: "hits/shots" or "hits/shots (acc%)" based on config text
		if ( element->config.style.isSet && element->config.style.value == 1 ) {
			// style 1: accuracy percentage only
			element->ctx.text = va( "%i%%", acc );
		} else {
			// default: hits/shots
			element->ctx.text = va( element->config.text.value, hits, shots );
		}

		CG_SHUDTextPrint( &element->config, &element->ctx );
	}
}

void CG_SHUDElementWeaponStatsDestroy( void *context ) {
	if ( context ) {
		Z_Free( context );
	}
}

#endif // FEAT_WIRED_UI
