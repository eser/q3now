/*
===========================================================================
cg_utils.c -- A collection of utility functions

===========================================================================
*/
#include "cg_local.h"
#include "cg_modern_private.h"

vmCvar_t cg_MaxlocationWidth;

qboolean CG_ModernIsGameTypeFreeze( void ) {
	return qfalse;
}

qboolean CG_IsFollowing( void ) {
	return ( cg.snap->ps.pm_flags & PMF_FOLLOW ) ? qtrue : qfalse;
}

qboolean CG_IsSpectator( void ) {
	return cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ? qtrue : qfalse;
}

void CG_ModernDrawFrame( float x, float y, float w, float h, const float *border, const float *borderColor, qboolean filled ) {
	if ( !border || !borderColor ) return;
	if ( border[0] > 0 ) CG_FillRectNorm( x * NORM_HSCALE, y * NORM_VSCALE, w * NORM_HSCALE, border[0] * NORM_VSCALE, borderColor );
	if ( border[2] > 0 ) CG_FillRectNorm( x * NORM_HSCALE, (y + h - border[2]) * NORM_VSCALE, w * NORM_HSCALE, border[2] * NORM_VSCALE, borderColor );
	if ( border[3] > 0 ) CG_FillRectNorm( x * NORM_HSCALE, (y + border[0]) * NORM_VSCALE, border[3] * NORM_HSCALE, (h - border[0] - border[2]) * NORM_VSCALE, borderColor );
	if ( border[1] > 0 ) CG_FillRectNorm( (x + w - border[1]) * NORM_HSCALE, (y + border[0]) * NORM_VSCALE, border[1] * NORM_HSCALE, (h - border[0] - border[2]) * NORM_VSCALE, borderColor );
}
