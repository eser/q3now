/*
===========================================================================
cl_wired_background.c -- Shared 3-layer background: base fill, radial glows, grid lines.

Used by both Wired UI menus (via UI_BACKGROUND_FULL ownerdraw) and the
loading screen. All coordinates are real screen pixels. Uses re.SetColor
+ re.DrawStretchPic directly.
===========================================================================
*/

#include "../client.h"
#include "cl_wired_draw.h"

#if FEAT_WIRED_UI

// Draw the standard 3-layer background: base fill, radial glows, grid lines.
// All coordinates are real screen pixels. Uses re.SetColor + re.DrawStretchPic directly.
void WUI_DrawBackground(float x, float y, float w, float h) {
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;

	Com_Printf(">>> WUI_DrawBackground called: %.0f %.0f %.0f %.0f\n", x, y, w, h);

	// Layer 1: dark base fill
	{
		vec4_t bg = { 0.031f, 0.047f, 0.063f, 1.0f };
		re.SetColor(bg);
		re.DrawStretchPic(x, y, w, h, 0, 0, 0, 0, cls.whiteShader);
		re.SetColor(NULL);
	}

	// Layer 2: radial glows (draw directly with re.DrawStretchPic, no shader needed)
	// Left glow -- dark cyan
	{
		vec4_t glowColor = { 0.102f, 0.227f, 0.431f, 0.6f };
		qhandle_t glowShader = re.RegisterShaderNoMip("gfx/ui/radial_glow");
		if (glowShader) {
			re.SetColor(glowColor);
			re.DrawStretchPic(x - w*0.1f, y + h*0.1f, w*0.7f, h*0.8f, 0, 0, 1, 1, glowShader);
			re.SetColor(NULL);
		}
	}
	// Right glow -- dark red
	{
		vec4_t glowColor = { 0.431f, 0.102f, 0.102f, 0.18f };
		qhandle_t glowShader = re.RegisterShaderNoMip("gfx/ui/radial_glow");
		if (glowShader) {
			re.SetColor(glowColor);
			re.DrawStretchPic(x + w*0.55f, y + h*0.1f, w*0.6f, h*0.8f, 0, 0, 1, 1, glowShader);
			re.SetColor(NULL);
		}
	}

	// Layer 3: horizontal grid lines
	{
		vec4_t gridColor = { 0.118f, 0.227f, 0.290f, 0.15f };
		float spacing = vpH * 0.04f; // ~4% of viewport height
		float lineH = 1.0f;
		float sy;
		re.SetColor(gridColor);
		for (sy = y; sy < y + h; sy += spacing) {
			re.DrawStretchPic(x, sy, w, lineH, 0, 0, 0, 0, cls.whiteShader);
		}
		re.SetColor(NULL);
	}
}

#endif // FEAT_WIRED_UI
