/*
cl_wired_hud_elem_crosshair.c — Wired UI crosshair element
*/

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct {
	modernhudConfig_t config;
	qhandle_t        shader;
} modernHudElementCrosshair_t;

void *CG_ModernHUDElementCrosshairCreate( const modernhudConfig_t *config ) {
	modernHudElementCrosshair_t *element;
	int i;

	ModernHUD_ELEMENT_INIT( element, config );

	element->shader = re.RegisterShader( "gfx/2d/crosshairMisc" );

	return element;
}

void CG_ModernHUDElementCrosshairRoutine( void *context ) {
	modernHudElementCrosshair_t *element = (modernHudElementCrosshair_t *)context;
	int idx;
	qhandle_t shader;
	float w, h, x, y;

	// cgame already decided visibility — shaderIndex < 0 means hidden
	idx = wiredHud->crosshair.shaderIndex;
	if ( idx < 0 ) return;

	shader = element->shader;
	if ( !shader ) return;

	w = h = wiredHud->crosshair.size;
	if ( w <= 0 ) return;

	// center on screen with offset
	x = wiredHud->crosshair.x;
	y = wiredHud->crosshair.y;

	// draw centered with cgame-computed color (coords are already real pixels)
	re.SetColor( wiredHud->crosshair.color );
	re.DrawStretchPic(
		x + cls.glconfig.vidWidth * 0.5f - w * 0.5f,
		y + cls.glconfig.vidHeight * 0.5f - h * 0.5f,
		w, h, 0, 0, 1, 1, shader );
	re.SetColor( NULL );
}

#endif // FEAT_WIRED_UI
