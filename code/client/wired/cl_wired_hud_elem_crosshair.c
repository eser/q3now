/*
===========================================================================
cl_wired_hud_elem_crosshair.c — Wired UI crosshair element

Thin renderer: cgame pre-computes color, size, visibility each frame.
This element just reads the final values from wiredHudState_t and draws.
No game logic — armor classes, health formulas stay in cgame.
===========================================================================
*/

#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI

#define WIRED_NUM_CROSSHAIRS 10

typedef struct {
	superhudConfig_t config;
	qhandle_t        shaders[WIRED_NUM_CROSSHAIRS];
} shudElementCrosshair_t;

void *CG_SHUDElementCrosshairCreate( const superhudConfig_t *config ) {
	shudElementCrosshair_t *element;
	int i;

	SHUD_ELEMENT_INIT( element, config );

	for ( i = 0; i < WIRED_NUM_CROSSHAIRS; i++ ) {
		element->shaders[i] = re.RegisterShader( va( "gfx/2d/crosshair%c", 'a' + i ) );
	}

	return element;
}

void CG_SHUDElementCrosshairRoutine( void *context ) {
	shudElementCrosshair_t *element = (shudElementCrosshair_t *)context;
	int idx;
	qhandle_t shader;
	float w, h, x, y;

	// cgame already decided visibility — shaderIndex < 0 means hidden
	idx = wiredHud->crosshair.shaderIndex;
	if ( idx <= 0 ) return;

	shader = element->shaders[ idx % WIRED_NUM_CROSSHAIRS ];
	if ( !shader ) return;

	w = h = wiredHud->crosshair.size;
	if ( w <= 0 ) return;

	// center on screen with offset
	x = wiredHud->crosshair.x;
	y = wiredHud->crosshair.y;

	// adjust for virtual 640x480 coords
	SCR_AdjustFrom640( &x, &y, &w, &h );

	// draw centered with cgame-computed color
	re.SetColor( wiredHud->crosshair.color );
	re.DrawStretchPic(
		x + cls.glconfig.vidWidth * 0.5f - w * 0.5f,
		y + cls.glconfig.vidHeight * 0.5f - h * 0.5f,
		w, h, 0, 0, 1, 1, shader );
	re.SetColor( NULL );
}

void CG_SHUDElementCrosshairDestroy( void *context ) {
	if ( context ) {
		Z_Free( context );
	}
}

#endif // FEAT_WIRED_UI
