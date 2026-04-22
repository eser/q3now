/*
cl_wired_background.c -- Shared 3-layer background: base fill, radial glows, grid lines.
*/

#include "../../client.h"
#include "cl_wired_draw.h"
#include "cl_wired_background.h"

#if FEAT_WIRED_UI

static qhandle_t wui_radialGlow = 0;

void WUI_BackgroundInit( void ) {
	wui_radialGlow = re.RegisterShaderNoMip( "gfx/ui/glow_radial" );
}

void WUI_DrawBackground( float x, float y, float w, float h ) {
	float vpH = (float)cls.glconfig.vidHeight;

	/* Layer 1: dark base fill */
	{
		vec4_t bg = { 0.031f, 0.047f, 0.063f, 1.0f };
		re.SetColor( bg );
		re.DrawStretchPic( x, y, w, h, 0, 0, 0, 0, cls.whiteShader );
		re.SetColor( NULL );
	}

	/* Layer 2: radial glows */
	if ( wui_radialGlow ) {
		vec4_t leftColor  = { 0.102f, 0.227f, 0.431f, 0.6f  };
		vec4_t rightColor = { 0.431f, 0.102f, 0.102f, 0.18f };

		re.SetColor( leftColor );
		re.DrawStretchPic( x - w*0.1f, y + h*0.1f, w*0.7f, h*0.8f, 0, 0, 1, 1, wui_radialGlow );
		re.SetColor( rightColor );
		re.DrawStretchPic( x + w*0.55f, y + h*0.1f, w*0.6f, h*0.8f, 0, 0, 1, 1, wui_radialGlow );
		re.SetColor( NULL );
	}

	/* Layer 3: horizontal grid lines */
	{
		vec4_t gridColor = { 0.118f, 0.227f, 0.290f, 0.15f };
		WUI_DrawScanlines( x, y, w, h, gridColor, vpH * 0.04f );
	}
}

#endif // FEAT_WIRED_UI
