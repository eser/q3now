/*
===========================================================================
cg_ospcompat.c -- OSP2 compatibility layer for SuperHUD port

Provides stub/adapted implementations of OSP2 functions that
the SuperHUD system depends on but don't exist in q3now.

Note: CG_OSPDrawString, CG_FontSelect, CG_FontIndexFromName, etc.
are now in cg_osptext.c (ported from OSP2-BE cg_drawtools.c).
===========================================================================
*/
#include "cg_local.h"
#include "cg_superhud_private.h"

vmCvar_t cg_MaxlocationWidth;

qboolean CG_OSPIsGameTypeFreeze( void ) {
	return qfalse;
}

qboolean CG_IsFollowing( void ) {
	return ( cg.snap->ps.pm_flags & PMF_FOLLOW ) ? qtrue : qfalse;
}

qboolean CG_IsSpectator( void ) {
	return cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ? qtrue : qfalse;
}

qboolean CG_OSPIsGameTypeTDM( void ) {
	return cgs.gametype == GT_TEAM ? qtrue : qfalse;
}

qboolean CG_OSPIsGameTypeCA( int gametype ) {
	return qfalse;
}

qboolean CG_IsSpectatorOnScreen( void ) {
	return CG_IsSpectator();
}

void CG_BEStatsShowStatsInfo( void ) {
	// stub — stats window not yet ported
}

void CG_OSPDrawFrame( float x, float y, float w, float h, const float *border, const float *borderColor, qboolean filled ) {
	if ( !border || !borderColor ) return;
	if ( border[0] > 0 ) CG_FillRect( x, y, w, border[0], borderColor );
	if ( border[2] > 0 ) CG_FillRect( x, y + h - border[2], w, border[2], borderColor );
	if ( border[3] > 0 ) CG_FillRect( x, y + border[0], border[3], h - border[0] - border[2], borderColor );
	if ( border[1] > 0 ) CG_FillRect( x + w - border[1], y + border[0], border[1], h - border[0] - border[2], borderColor );
}
