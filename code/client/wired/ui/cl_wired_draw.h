/*
===========================================================================
cl_wired_draw.h — Wired UI draw helpers (real screen pixel coordinates)

SCR_DrawPic / SCR_FillRect assume 640x480 virtual coordinates and call
SCR_AdjustFrom640 internally to scale to the actual resolution.  Wired UI
stores REAL SCREEN PIXELS in item->rect (via WUI_BackfillToScreen), so
calling SCR_* functions causes a double conversion.

These inline helpers call the renderer directly, bypassing AdjustFrom640.
Include this header in any client-side file that draws Wired UI geometry.

Requires: client.h must be included BEFORE this header (provides re, cls).
===========================================================================
*/

#ifndef CL_WIRED_DRAW_H
#define CL_WIRED_DRAW_H

/* Draw a shader at real screen pixel coordinates (no AdjustFrom640). */
static ID_INLINE void WUI_DrawPic( float x, float y, float w, float h, qhandle_t shader ) {
	re.DrawStretchPic( x, y, w, h, 0, 0, 1, 1, shader );
}

/* Fill a rect at real screen pixel coordinates (no AdjustFrom640).
 * Does NOT reset color to white afterwards; caller or B.7 dedup handles state. */
static ID_INLINE void WUI_FillRect( float x, float y, float w, float h, const float *color ) {
	re.SetColor( color );
	re.DrawStretchPic( x, y, w, h, 0, 0, 0, 0, cls.whiteShader );
}

/* Draw a shader with a specific color; resets color to white afterwards. */
static ID_INLINE void WUI_DrawPicC( float x, float y, float w, float h,
                                     const float *color, qhandle_t shader ) {
	re.SetColor( color );
	re.DrawStretchPic( x, y, w, h, 0, 0, 1, 1, shader );
	re.SetColor( NULL );
}

/* Draw a stretched pic at real screen pixel coordinates (no AdjustFrom640). */
static ID_INLINE void WUI_DrawStretchPic( float x, float y, float w, float h,
                                           float s1, float t1, float s2, float t2,
                                           qhandle_t shader ) {
	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );
}

/* Draw horizontal scanlines across rect (x,y,w,h) with the given spacing.
 * Sets color once before the loop; caller must not rely on color state after. */
static ID_INLINE void WUI_DrawScanlines( float x, float y, float w, float h,
                                          const float *color, float spacing ) {
	float sy;
	re.SetColor( color );
	for ( sy = y; sy < y + h; sy += spacing ) {
		re.DrawStretchPic( x, sy, w, 1.0f, 0, 0, 0, 0, cls.whiteShader );
	}
	re.SetColor( NULL );
}

#endif /* CL_WIRED_DRAW_H */
