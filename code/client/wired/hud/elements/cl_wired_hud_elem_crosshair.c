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

	ModernHUD_ELEMENT_INIT( element, config );

	element->shader = re.RegisterShader( "gfx/2d/crosshairDefault" );

	return element;
}

void CG_ModernHUDElementCrosshairRoutine( void *context ) {
	modernHudElementCrosshair_t *element = (modernHudElementCrosshair_t *)context;

	// cgame already decided visibility — shaderIndex < 0 means hidden
	int idx = wiredHud->crosshair.shaderIndex;
	if ( idx < 0 ) return;

	qhandle_t shader = element->shader;
	if ( !shader ) return;

	float w = wiredHud->crosshair.size;
	float h = w;
	if ( w <= 0 ) return;

	// center on screen with offset
	float x = wiredHud->crosshair.x;
	float y = wiredHud->crosshair.y;

	// draw centered with cgame-computed color (coords are already real pixels)
	re.SetColor( wiredHud->crosshair.color );
	re.DrawStretchPic(
		x + cls.glconfig.vidWidth * 0.5f - w * 0.5f,
		y + cls.glconfig.vidHeight * 0.5f - h * 0.5f,
		w, h, 0, 0, 1, 1, shader );
	re.SetColor( NULL );
}

#endif // FEAT_WIRED_UI
