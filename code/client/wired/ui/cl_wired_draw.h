// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_draw.h — Wired UI draw helpers (real screen pixel coordinates)
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
