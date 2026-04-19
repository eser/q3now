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

/* Draw a stretched pic at real screen pixel coordinates (no AdjustFrom640). */
static ID_INLINE void WUI_DrawStretchPic( float x, float y, float w, float h,
                                           float s1, float t1, float s2, float t2,
                                           qhandle_t shader ) {
	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );
}

#endif /* CL_WIRED_DRAW_H */
