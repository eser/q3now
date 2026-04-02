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
	if ( border[0] > 0 ) CG_FillRect( x, y, w, border[0], borderColor );
	if ( border[2] > 0 ) CG_FillRect( x, y + h - border[2], w, border[2], borderColor );
	if ( border[3] > 0 ) CG_FillRect( x, y + border[0], border[3], h - border[0] - border[2], borderColor );
	if ( border[1] > 0 ) CG_FillRect( x + w - border[1], y + border[0], border[1], h - border[0] - border[2], borderColor );
}
