/* cl_wired_hud_elem_weapon_stats.c -- Per-weapon accuracy stats HUD elements
   Text variants show "hits/shots (acc%)" for a specific weapon.
   Icon variants show the weapon icon.
   "CurrentWeapon" variants track the player's currently held weapon.

   Weapon IDs are numeric indices into the bridge arrays. cgame defines
   the mapping; the client only stores and displays the numeric ID. */
#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI

/* ---- shared element struct ----------------------------------------- */

typedef struct {
	superhudConfig_t config;
	superhudTextContext_t ctx;
	int weaponId;       /* numeric weapon index, or -1 for current weapon */
	qboolean isIcon;    /* qtrue = draw weapon icon, qfalse = draw text stats */
} shudElementWeaponStats_t;

/* ---- create helpers ------------------------------------------------ */

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

/* ---- text Create functions ----------------------------------------- */
/* Weapon IDs: 2=MG, 3=SG, 4=GL, 5=RL, 6=LG, 7=RG, 8=PG             */

void *CG_SHUDElementCreateCurrentWeapon( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, -1 ); }
void *CG_SHUDElementWeaponStatsCreateMG( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, 2 ); }
void *CG_SHUDElementWeaponStatsCreateSG( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, 3 ); }
void *CG_SHUDElementWeaponStatsCreateGL( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, 4 ); }
void *CG_SHUDElementWeaponStatsCreateRL( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, 5 ); }
void *CG_SHUDElementWeaponStatsCreateLG( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, 6 ); }
void *CG_SHUDElementWeaponStatsCreateRG( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, 7 ); }
void *CG_SHUDElementWeaponStatsCreatePG( const superhudConfig_t *config )     { return WeaponStats_CreateText( config, 8 ); }

/* ---- icon Create functions ----------------------------------------- */

void *CG_SHUDElementIconCreateCurrentWeapon( const superhudConfig_t *config ) { return WeaponStats_CreateIcon( config, -1 ); }
void *CG_SHUDElementIconCreateMG( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, 2 ); }
void *CG_SHUDElementIconCreateSG( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, 3 ); }
void *CG_SHUDElementIconCreateGL( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, 4 ); }
void *CG_SHUDElementIconCreateRL( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, 5 ); }
void *CG_SHUDElementIconCreateLG( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, 6 ); }
void *CG_SHUDElementIconCreateRG( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, 7 ); }
void *CG_SHUDElementIconCreatePG( const superhudConfig_t *config )            { return WeaponStats_CreateIcon( config, 8 ); }

/* ---- shared Routine ------------------------------------------------ */

void CG_SHUDElementWeaponStatsRoutine( void *context ) {
	shudElementWeaponStats_t *element = (shudElementWeaponStats_t *)context;
	int wp;
	int hits, shots, acc;
	int maxSlots;

	if ( !element ) return;

	/* resolve weapon ID (-1 = current weapon) */
	wp = element->weaponId;
	if ( wp < 0 ) {
		wp = wiredHud->weapon;
	}

	/* bounds check against bridge array size */
	maxSlots = (int)(sizeof(wiredHud->attackStats) / sizeof(wiredHud->attackStats[0]));
	if ( wp < 0 || wp >= maxSlots ) return;

	hits  = wiredHud->attackStats[wp].hits;
	shots = wiredHud->attackStats[wp].shots;
	acc   = ( shots > 0 ) ? (int)( 100.0f * hits / shots ) : 0;

	if ( element->isIcon ) {
		/* draw weapon icon */
		int iconSlots = (int)(sizeof(wiredHud->weaponIcons) / sizeof(wiredHud->weaponIcons[0]));
		qhandle_t icon;
		if ( wp >= iconSlots ) return;
		icon = cg_weapons[wp].weaponIcon;
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
			Wired_DrawPic( x, y, w, h, 0, 0, 1, 1, icon );
			trap_R_SetColor( NULL );
		}
	} else {
		/* draw text stats */
		if ( shots <= 0 && !SHUD_CHECK_SHOW_EMPTY( element ) ) return;

		if ( element->config.style.isSet && element->config.style.value == 1 ) {
			element->ctx.text = va( "%i%%", acc );
		} else {
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

#endif /* FEAT_WIRED_UI */
