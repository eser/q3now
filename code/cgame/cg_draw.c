/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// cg_draw.c -- draw all of the graphical elements during
// active (after loading) gameplay

#include "cg_local.h"
#include "cg_modern_private.h"

int drawTeamOverlayModificationCount = -1;

int sortedTeamPlayers[TEAM_MAXOVERLAY];
int	numSortedTeamPlayers;

char systemChat[256];
char teamChat1[256];
char teamChat2[256];

// screen placement state (push/pop API for anchoring HUD elements)
typedef enum {
	PLACE_STRETCH,
	PLACE_CENTER,

	// horizontal only
	PLACE_LEFT,
	PLACE_RIGHT,

	// vertical only
	PLACE_TOP,
	PLACE_BOTTOM
} screenPlacement_e;

static screenPlacement_e cg_horizontalPlacement = PLACE_CENTER;
static screenPlacement_e cg_verticalPlacement = PLACE_CENTER;
static screenPlacement_e cg_lastHorizontalPlacement = PLACE_CENTER;
static screenPlacement_e cg_lastVerticalPlacement = PLACE_CENTER;

void CG_SetScreenPlacement( screenPlacement_e hpos, screenPlacement_e vpos ) {
	cg_lastHorizontalPlacement = cg_horizontalPlacement;
	cg_lastVerticalPlacement = cg_verticalPlacement;
	cg_horizontalPlacement = hpos;
	cg_verticalPlacement = vpos;
}

void CG_PopScreenPlacement( void ) {
	cg_horizontalPlacement = cg_lastHorizontalPlacement;
	cg_verticalPlacement = cg_lastVerticalPlacement;
}

/* TA UI text painting functions removed -- now handled by Wired UI text system */



/*
===========================================================================================

  UPPER RIGHT CORNER

===========================================================================================
*/

/*
=================
CG_DrawStrlen

Returns character count, skiping color escape codes
=================
*/
int CG_DrawStrlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

/*
==================
CG_DrawSpeed
Shows current horizontal (XY) speed in units/second. (2C)
cg_drawSpeed 1 = "Xu/s" label, 2 = number only (centered).
==================
*/
static float CG_DrawSpeed( float y ) {
	const char	*s;
	int			w, speed;
	vec_t		*vel;

	if ( cg.scoreBoardShowing ) {
		return y;
	}

	vel = cg.snap->ps.velocity;
	speed = (int)sqrt( vel[0] * vel[0] + vel[1] * vel[1] );	// 2D, ignore Z

	if ( cg_drawSpeed.integer == 2 ) {
		// center of screen (like RatArena mode 2)
		s = va( "%i", speed );
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		trap_R_DrawTextNorm( s, (float)(320 - w / 2) * NORM_HSCALE, 300.0f * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		return y;
	} else {
		// bottom-right, above mini scoreboard
		y -= SMALLCHAR_HEIGHT + 4;
		s = va( "%iu/s", speed );
		w = CG_DrawStrlen( s ) * SMALLCHAR_WIDTH;
		trap_R_DrawTextNorm( s, (640 - w - 4) * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_UI, (float)SMALLCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, 0 );
		return y;
	}
}

/*
==================
CG_DrawSnapshot
==================
*/
static float CG_DrawSnapshot( float y ) {
	const char	*s;
	int			w;

	s = va( "time:%i snap:%i cmd:%i", cg.snap->serverTime, 
		cg.latestSnapshotNum, cgs.serverCommandSequence );
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

	trap_R_DrawTextNorm( s, (float)(635 - w) * NORM_HSCALE, (y + 2.0f) * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );

	return y + BIGCHAR_HEIGHT + 4;
}

/*
==================
CG_DrawFPS
==================
*/
#define	FPS_FRAMES	4
static float CG_DrawFPS( float y ) {
	const char	*s;
	int			w;
	static int	previousTimes[FPS_FRAMES];
	static int	index;
	int		i, total;
	int		fps;
	static	int	previous;
	int		t, frameTime;

	// don't use serverTime, because that will be drifting to
	// correct for internet lag changes, timescales, timedemos, etc
	t = trap_Milliseconds();
	frameTime = t - previous;
	previous = t;

	previousTimes[index % FPS_FRAMES] = frameTime;
	index++;
	if ( index > FPS_FRAMES ) {
		// average multiple frames together to smooth changes out a bit
		total = 0;
		for ( i = 0 ; i < FPS_FRAMES ; i++ ) {
			total += previousTimes[i];
		}
		if ( !total ) {
			total = 1;
		}
		fps = 1000 * FPS_FRAMES / total;

		s = va( "%ifps", fps );
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

		trap_R_DrawTextNorm( s, (float)(635 - w) * NORM_HSCALE, (y + 2.0f) * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
	}

	return y + BIGCHAR_HEIGHT + 4;
}

/*
=================
CG_DrawTimer
=================
*/
static float CG_DrawTimer( float y ) {
	const char	*s;
	int			w;
	int			mins, seconds, tens;
	int			msec;

	msec = cg.time - cgs.levelStartTime;

	seconds = msec / 1000;
	mins = seconds / 60;
	seconds -= mins * 60;
	tens = seconds / 10;
	seconds -= tens * 10;

	s = va( "%i:%i%i", mins, tens, seconds );
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;

	trap_R_DrawTextNorm( s, (float)(635 - w) * NORM_HSCALE, (y + 2.0f) * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );

	return y + BIGCHAR_HEIGHT + 4;
}


/*
=================
CG_GetColorForAmount
=================
*/
void CG_GetColorForAmount( int health, int armor, vec4_t hcolor ) {
	int		count;
	int		max;

	// calculate the total points of damage that can
	// be sustained at the current health / armor level
	if ( health <= 0 ) {
		VectorClear( hcolor );	// black
		hcolor[3] = 1;
		return;
	}
	count = armor;
	max = health * 0.66 / ( 1.0 - 0.66 );
	if ( max < count ) {
		count = max;
	}
	health += count;

	// set the color based on health
	hcolor[0] = 1.0;
	hcolor[3] = 1.0;
	if ( health >= 100 ) {
		hcolor[2] = 1.0;
	} else if ( health < 66 ) {
		hcolor[2] = 0;
	} else {
		hcolor[2] = ( health - 66 ) / 33.0;
	}

	if ( health > 60 ) {
		hcolor[1] = 1.0;
	} else if ( health < 30 ) {
		hcolor[1] = 0;
	} else {
		hcolor[1] = ( health - 30 ) / 30.0;
	}
}

/*
================
CG_AdjustPlacement

Adjust normalized (0.0-1.0) coordinates for widescreen placement.
Applies aspect-ratio-preserving scale and centering bias.
Output values are still in 0.0-1.0 normalized space (consumed by
trap_R_DrawStretchPicNorm which multiplies by vidWidth/vidHeight).
================
*/
void CG_AdjustPlacement( float *x, float *y, float *w, float *h ) {
	if ( cg_horizontalPlacement == PLACE_STRETCH || cg_stretch.integer ) {
		if ( w ) *w *= cgs.normXScaleStretch;
		if ( x ) *x *= cgs.normXScaleStretch;
	} else {
		if ( w ) *w *= cgs.normXScale;
		if ( x ) {
			*x *= cgs.normXScale;
			if ( cg_horizontalPlacement == PLACE_CENTER ) {
				*x += cgs.normXBias;
			} else if ( cg_horizontalPlacement == PLACE_RIGHT ) {
				*x += cgs.normXBias * 2;
			}
		}
	}

	if ( cg_verticalPlacement == PLACE_STRETCH || cg_stretch.integer ) {
		if ( h ) *h *= cgs.normYScaleStretch;
		if ( y ) *y *= cgs.normYScaleStretch;
	} else {
		if ( h ) *h *= cgs.normYScale;
		if ( y ) {
			*y *= cgs.normYScale;
			if ( cg_verticalPlacement == PLACE_CENTER ) {
				*y += cgs.normYBias;
			} else if ( cg_verticalPlacement == PLACE_BOTTOM ) {
				*y += cgs.normYBias * 2;
			}
		}
	}
}

/*
================
CG_FillRectNorm

Coordinates are normalized (0.0-1.0).
=================
*/
void CG_FillRectNorm( float nx, float ny, float nw, float nh, const float *color ) {
	trap_R_SetColor( color );
	CG_AdjustPlacement( &nx, &ny, &nw, &nh );
	trap_R_DrawStretchPicNorm( nx, ny, nw, nh, 0, 0, 0, 0, cgs.media.whiteShader );
	trap_R_SetColor( NULL );
}

/*
================
CG_DrawPicNorm

Coordinates are normalized (0.0-1.0).
=================
*/
void CG_DrawPicNorm( float nx, float ny, float nw, float nh, qhandle_t hShader ) {
	CG_AdjustPlacement( &nx, &ny, &nw, &nh );
	trap_R_DrawStretchPicNorm( nx, ny, nw, nh, 0, 0, 1, 1, hShader );
}

/*
=================
CG_DrawTeamOverlay
=================
*/

static float CG_DrawTeamOverlay( float y, qboolean right, qboolean upper ) {
	int x, w, h, xx;
	int i, j, len;
	const char *p;
	vec4_t		hcolor;
	int pwidth, lwidth;
	int plyrs;
	char st[16];
	clientInfo_t *ci;
	gitem_t	*item;
	int ret_y, count;

	if ( !cg_drawTeamOverlay.integer ) {
		return y;
	}

	if ( cg.snap->ps.persistant[PERS_TEAM] != TEAM_RED && cg.snap->ps.persistant[PERS_TEAM] != TEAM_BLUE ) {
		return y; // Not on any team
	}

	plyrs = 0;

	// max player name width
	pwidth = 0;
	count = (numSortedTeamPlayers > 8) ? 8 : numSortedTeamPlayers;
	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if ( ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {
			plyrs++;
			len = CG_DrawStrlen(ci->name);
			if (len > pwidth)
				pwidth = len;
		}
	}

	if (!plyrs)
		return y;

	if (pwidth > TEAM_OVERLAY_MAXNAME_WIDTH)
		pwidth = TEAM_OVERLAY_MAXNAME_WIDTH;

	// max location name width
	lwidth = 0;
	for (i = 1; i < MAX_LOCATIONS; i++) {
		p = CG_ConfigString(CS_LOCATIONS + i);
		if (p && *p) {
			len = CG_DrawStrlen(p);
			if (len > lwidth)
				lwidth = len;
		}
	}

	if (lwidth > TEAM_OVERLAY_MAXLOCATION_WIDTH)
		lwidth = TEAM_OVERLAY_MAXLOCATION_WIDTH;

	w = (pwidth + lwidth + 4 + 7) * TINYCHAR_WIDTH;

	if ( right )
		x = 640 - w;
	else
		x = 0;

	h = plyrs * TINYCHAR_HEIGHT;

	if ( upper ) {
		ret_y = y + h;
	} else {
		y -= h;
		ret_y = y;
	}

	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED ) {
		hcolor[0] = 1.0f;
		hcolor[1] = 0.0f;
		hcolor[2] = 0.0f;
		hcolor[3] = 0.33f;
	} else { // if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE )
		hcolor[0] = 0.0f;
		hcolor[1] = 0.0f;
		hcolor[2] = 1.0f;
		hcolor[3] = 0.33f;
	}
	trap_R_SetColor( hcolor );
	CG_DrawPicNorm( x * NORM_HSCALE, y * NORM_VSCALE, w * NORM_HSCALE, h * NORM_VSCALE, cgs.media.teamStatusBar );
	trap_R_SetColor( NULL );

	for (i = 0; i < count; i++) {
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if ( ci->infoValid && ci->team == cg.snap->ps.persistant[PERS_TEAM]) {

			hcolor[0] = hcolor[1] = hcolor[2] = hcolor[3] = 1.0;

			xx = x + TINYCHAR_WIDTH;

			trap_R_DrawTextNorm( ci->name, (float)xx * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_UI, (float)TINYCHAR_HEIGHT * NORM_VSCALE, hcolor, TEXT_ALIGN_LEFT, 0 );

			if (lwidth) {
				p = CG_ConfigString(CS_LOCATIONS + ci->location);
				if (!p || !*p)
					p = "unknown";
//				len = CG_DrawStrlen(p);
//				if (len > lwidth)
//					len = lwidth;

//				xx = x + TINYCHAR_WIDTH * 2 + TINYCHAR_WIDTH * pwidth +
//					((lwidth/2 - len/2) * TINYCHAR_WIDTH);
				xx = x + TINYCHAR_WIDTH * 2 + TINYCHAR_WIDTH * pwidth;
				trap_R_DrawTextNorm( p, (float)xx * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_UI, (float)TINYCHAR_HEIGHT * NORM_VSCALE, hcolor, TEXT_ALIGN_LEFT, 0 );
			}

			CG_GetColorForAmount( ci->health, ci->armor, hcolor );

			Com_sprintf (st, sizeof(st), "%3i %3i", ci->health,	ci->armor);

			xx = x + TINYCHAR_WIDTH * 3 +
				TINYCHAR_WIDTH * pwidth + TINYCHAR_WIDTH * lwidth;

			trap_R_DrawTextNorm( st, (float)xx * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_UI, (float)TINYCHAR_HEIGHT * NORM_VSCALE, hcolor, TEXT_ALIGN_LEFT, 0 );

			// draw weapon icon
			xx += TINYCHAR_WIDTH * 3;

			if ( cg_weapons[ci->curWeapon].weaponIcon ) {
				CG_DrawPicNorm( xx * NORM_HSCALE, y * NORM_VSCALE, TINYCHAR_WIDTH * NORM_HSCALE, TINYCHAR_HEIGHT * NORM_VSCALE,
					cg_weapons[ci->curWeapon].weaponIcon );
			} else {
				CG_DrawPicNorm( xx * NORM_HSCALE, y * NORM_VSCALE, TINYCHAR_WIDTH * NORM_HSCALE, TINYCHAR_HEIGHT * NORM_VSCALE,
					cgs.media.deferShader );
			}

			// Draw powerup icons
			if (right) {
				xx = x;
			} else {
				xx = x + w - TINYCHAR_WIDTH;
			}
			for (j = 0; j <= PW_NUM_POWERUPS; j++) {
				if (ci->powerups & (1 << j)) {

					item = BG_FindItemForPowerup( j );

					if (item) {
						CG_DrawPicNorm( xx * NORM_HSCALE, y * NORM_VSCALE, TINYCHAR_WIDTH * NORM_HSCALE, TINYCHAR_HEIGHT * NORM_VSCALE,
						trap_R_RegisterShader( item->icon ) );
						if (right) {
							xx -= TINYCHAR_WIDTH;
						} else {
							xx += TINYCHAR_WIDTH;
						}
					}
				}
			}

			y += TINYCHAR_HEIGHT;
		}
	}

	return ret_y;
//#endif
}


/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define	LAG_SAMPLES		128


typedef struct {
	int		frameSamples[LAG_SAMPLES];
	int		frameCount;
	int		snapshotFlags[LAG_SAMPLES];
	int		snapshotSamples[LAG_SAMPLES];
	int		snapshotCount;
} lagometer_t;

lagometer_t		lagometer;

/*
==============
CG_AddLagometerFrameInfo

Adds the current interpolate / extrapolate bar for this frame
==============
*/
void CG_AddLagometerFrameInfo( void ) {
	int			offset;

	offset = cg.time - cg.latestSnapshotTime;
	lagometer.frameSamples[ lagometer.frameCount & ( LAG_SAMPLES - 1) ] = offset;
	lagometer.frameCount++;
}

/*
==============
CG_AddLagometerSnapshotInfo

Each time a snapshot is received, log its ping time and
the number of snapshots that were dropped before it.

Pass NULL for a dropped packet.
==============
*/
void CG_AddLagometerSnapshotInfo( snapshot_t *snap ) {
	// dropped packet
	if ( !snap ) {
		lagometer.snapshotSamples[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = -1;
		lagometer.snapshotCount++;
		return;
	}

	// add this snapshot's info
	lagometer.snapshotSamples[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = snap->ping;
	lagometer.snapshotFlags[ lagometer.snapshotCount & ( LAG_SAMPLES - 1) ] = snap->snapFlags;
	lagometer.snapshotCount++;
}

/*
==============
CG_DrawDisconnect

Should we draw something differnet for long lag vs no packets?
==============
*/
static void CG_DrawDisconnect( void ) {
	float		x, y;
	int			cmdNum;
	usercmd_t	cmd;
	const char		*s;
	int			w;

	// draw the phone jack if we are completely past our buffers
	cmdNum = trap_GetCurrentCmdNumber() - CMD_BACKUP + 1;
	trap_GetUserCmd( cmdNum, &cmd );
	if ( cmd.serverTime <= cg.snap->ps.commandTime
		|| cmd.serverTime > cg.time ) {	// special check for map_restart
		return;
	}

	// also add text in center of screen
	s = "Connection Interrupted";
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
	trap_R_DrawTextNorm( s, (float)(320 - w/2) * NORM_HSCALE, 100.0f * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );

	// blink the icon
	if ( ( cg.time >> 9 ) & 1 ) {
		return;
	}

	x = 640 - 48;
	y = 480 - 144;

	CG_DrawPicNorm( x * NORM_HSCALE, y * NORM_VSCALE, 48 * NORM_HSCALE, 48 * NORM_VSCALE, trap_R_RegisterShader("gfx/2d/net.tga" ) );
}


#define	MAX_LAGOMETER_PING	900
#define	MAX_LAGOMETER_RANGE	300





/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

/*
================
CG_FadeColor
================
*/
float *CG_FadeColor( int startMsec, int totalMsec ) {
	static vec4_t		color;
	int			t;

	if ( startMsec == 0 ) {
		return NULL;
	}

	t = cg.time - startMsec;

	if ( t >= totalMsec ) {
		return NULL;
	}

	// fade out
	if ( totalMsec - t < FADE_TIME ) {
		color[3] = ( totalMsec - t ) * 1.0/FADE_TIME;
	} else {
		color[3] = 1.0;
	}
	color[0] = color[1] = color[2] = 1;

	return color;
}

/*
==============
CG_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_CenterPrint( const char *str, int y, int charWidth ) {
	char	*s;

	Q_strncpyz( cg.centerPrint, str, sizeof(cg.centerPrint) );

	cg.centerPrintTime = cg.time;
	cg.centerPrintY = y;
	cg.centerPrintCharWidth = charWidth;

	// count the number of lines for centering
	cg.centerPrintLines = 1;
	s = cg.centerPrint;
	while( *s ) {
		if (*s == '\n')
			cg.centerPrintLines++;
		s++;
	}
}


/*
===================
CG_DrawCenterString
===================
*/
static void CG_DrawCenterString( void ) {
	char	*start;
	int		l;
	int		x, y, w;
	float	*color;

	if ( !cg.centerPrintTime ) {
		return;
	}

	color = CG_FadeColor( cg.centerPrintTime, 1000 * cg_centertime.value );
	if ( !color ) {
		return;
	}

	trap_R_SetColor( color );

	start = cg.centerPrint;

	y = cg.centerPrintY - cg.centerPrintLines * BIGCHAR_HEIGHT / 2;

	while ( 1 ) {
		char linebuffer[1024];

		for ( l = 0; l < 50; l++ ) {
			if ( !start[l] || start[l] == '\n' ) {
				break;
			}
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		w = (int)(trap_R_MeasureTextNorm( linebuffer, FONT_UI, (float)((int)(cg.centerPrintCharWidth * 2.0)) * NORM_VSCALE ) / NORM_HSCALE);
		x = ( 640 - w ) / 2;
		trap_R_DrawTextNorm( linebuffer, (float)x * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_UI, (float)((int)(cg.centerPrintCharWidth * 2.0)) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		y += cg.centerPrintCharWidth * 2.0;
		while ( *start && ( *start != '\n' ) ) {
			start++;
		}
		if ( !*start ) {
			break;
		}
		start++;
	}

	trap_R_SetColor( NULL );
}



/*
================================================================================

CROSSHAIR

================================================================================
*/



/*
=================
CG_DrawCrosshair3D
=================
*/
static void CG_DrawCrosshair3D(void)
{
	float		w;
	qhandle_t	hShader;
	float		f;

	trace_t trace;
	vec3_t endpos;
	float stereoSep, zProj, maxdist, xmax;
	char rendererinfos[128];
	refEntity_t ent;

	if ( !cg_drawCrosshair.integer ) {
		return;
	}

	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
		return;
	}

	if ( cg.renderingThirdPerson ) {
		return;
	}

	w = cg_crosshairSize.value;

	// pulse the size of the crosshair when picking up items
	f = cg.time - cg.itemPickupBlendTime;
	if ( f > 0 && f < ITEM_BLOB_TIME ) {
		f /= ITEM_BLOB_TIME;
		w *= ( 1 + f );
	}

	hShader = cgs.media.crosshairMiscShader;

	// Use a different method rendering the crosshair so players don't see two of them when
	// focusing their eyes at distant objects with high stereo separation
	// We are going to trace to the next shootable object and place the crosshair in front of it.

	// first get all the important renderer information
	trap_Cvar_VariableStringBuffer("r_zProj", rendererinfos, sizeof(rendererinfos));
	zProj = atof(rendererinfos);
	trap_Cvar_VariableStringBuffer("r_stereoSeparation", rendererinfos, sizeof(rendererinfos));
	stereoSep = zProj / atof(rendererinfos);
	
	xmax = zProj * tan(cg.refdef.fov_x * M_PI / 360.0f);
	
	// let the trace run through until a change in stereo separation of the crosshair becomes less than one pixel.
	maxdist = cgs.glconfig.vidWidth * stereoSep * zProj / (2 * xmax);
	VectorMA(cg.refdef.vieworg, maxdist, cg.refdef.viewaxis[0], endpos);
	CG_Trace(&trace, cg.refdef.vieworg, NULL, NULL, endpos, 0, MASK_SHOT);
	
	memset(&ent, 0, sizeof(ent));
	ent.reType = RT_SPRITE;
	ent.renderfx = RF_DEPTHHACK | RF_CROSSHAIR;
	
	VectorCopy(trace.endpos, ent.origin);
	
	// scale the crosshair so it appears the same size for all distances
	ent.radius = w * NORM_HSCALE * xmax * trace.fraction * maxdist / zProj;
	ent.customShader = hShader;

	trap_R_AddRefEntityToScene(&ent);
}



/*
=================
CG_ScanForCrosshairEntity
=================
*/
static void CG_ScanForCrosshairEntity( void ) {
	trace_t		trace;
	vec3_t		start, end;
	int			content;
	qboolean	throughwall = cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR;
	qboolean	lookedThroughWall = qfalse;
	int			clientNum;
	centity_t	*cent;

	VectorCopy( cg.refdef.vieworg, start );
	VectorMA( start, 131072, cg.refdef.viewaxis[0], end );

	CG_Trace( &trace, start, vec3_origin, vec3_origin, end,
		cg.snap->ps.clientNum, CONTENTS_SOLID|CONTENTS_BODY );
	if ( trace.entityNum >= MAX_CLIENTS ) {
		if ( throughwall ) {
			CG_Trace( &trace, start, vec3_origin, vec3_origin, end,
				cg.snap->ps.clientNum, CONTENTS_BODY );

			if ( trace.entityNum >= MAX_CLIENTS ) {
				return;
			}

			lookedThroughWall = qtrue;
		} else {
			return;
		}
	} else {
		clientNum = trace.entityNum;
	}

	if (trace.entityNum >= MAX_CLIENTS) {
		clientNum = cg_entities[trace.entityNum].currentState.clientNum;
		if (clientNum < 0 || clientNum >= MAX_CLIENTS) {
			return;
		}
	} else {
		clientNum = trace.entityNum;
	}

	// if the player is in fog, don't show it
	content = CG_PointContents( trace.endpos, cg.snap->ps.clientNum );
	if ( content & CONTENTS_FOG ) {
		return;
	}

	cent = &cg_entities[trace.entityNum];

	// if the player is invisible, don't show it
	if ( CG_IsPlayerInvisible(cent) ) {
		return;
	}

	if (throughwall && lookedThroughWall) {
		// XXX: technically, this could give an enemy's position away
		// when he obscures the position of a friend
		clientInfo_t	*ci;
		ci = &cgs.clientinfo[clientNum];

		if (!ci->infoValid) {
			return;
		}

		if (ci->team != cg.snap->ps.persistant[PERS_TEAM]) {
			return;
		}
	}

	// update the fade timer
	cg.crosshairClientNum = trace.entityNum;
	cg.crosshairClientTime = cg.time;
}




//==============================================================================








static qboolean CG_DrawScoreboard( void ) {
#if FEAT_WIRED_UI
	// when Wired UI is active, the client renders the scoreboard overlay
	// cgame just reports whether the scoreboard should be "showing" (suppresses other HUD)
	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD
		 || cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
		return qtrue;
	}
	if ( CG_FadeColor( cg.scoreFadeTime, FADE_TIME ) ) {
		return qtrue;
	}
	return qfalse;
#endif
}

#if FEAT_MATCH_SUMMARY
/*
=================
CG_DrawMatchSummary

Intermission overlay: top fragger, best accuracy, most deaths. (8B)
Draws below the scoreboard using existing score_t data.
=================
*/
static void CG_DrawMatchSummary( void ) {
	int		i, bestScore = -9999, bestAcc = -1, mostDeaths = -1;
	int		bestScoreClient = -1, bestAccClient = -1, mostDeathsClient = -1;
	const char *s;
	int		w, y;
	vec4_t	color;

	if ( cg.numScores < 2 ) {
		return;
	}

	// find leaders
	for ( i = 0; i < cg.numScores; i++ ) {
		if ( cg.scores[i].score > bestScore ) {
			bestScore = cg.scores[i].score;
			bestScoreClient = cg.scores[i].client;
		}
		if ( cg.scores[i].accuracy > bestAcc ) {
			bestAcc = cg.scores[i].accuracy;
			bestAccClient = cg.scores[i].client;
		}
	}
	// most deaths from persistant stats — use score as proxy (lowest score often = most deaths)
	// We only have score_t data, so show top fragger + best accuracy

	CG_SetScreenPlacement( PLACE_CENTER, PLACE_CENTER );

	y = 340;
	color[0] = 1.0f; color[1] = 0.8f; color[2] = 0.2f; color[3] = 1.0f;
	trap_R_SetColor( color );

	if ( bestScoreClient >= 0 ) {
		s = va( "Top Fragger: %s ^7(%i)", cgs.clientinfo[bestScoreClient].name, bestScore );
		w = CG_DrawStrlen( s ) * SMALLCHAR_WIDTH;
		trap_R_DrawTextNorm( s, (float)(320 - w / 2) * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_UI, (float)SMALLCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, 0 );
		y += SMALLCHAR_HEIGHT + 2;
	}
	if ( bestAccClient >= 0 && bestAcc > 0 ) {
		s = va( "Best Accuracy: %s ^7(%i%%)", cgs.clientinfo[bestAccClient].name, bestAcc );
		w = CG_DrawStrlen( s ) * SMALLCHAR_WIDTH;
		trap_R_DrawTextNorm( s, (float)(320 - w / 2) * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_UI, (float)SMALLCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, 0 );
	}

	trap_R_SetColor( NULL );
	CG_PopScreenPlacement();
}
#endif

/*
=================
CG_DrawIntermission
=================
*/
static void CG_DrawIntermission( void ) {
//	int key;

    if ( cg_singlePlayer.integer ) {
		CG_DrawCenterString();
		return;
	}

    cg.scoreFadeTime = cg.time;
	cg.scoreBoardShowing = CG_DrawScoreboard();

#if FEAT_MATCH_SUMMARY
	CG_DrawMatchSummary();
#endif
}










//==================================================================================
/*
=================
CG_DrawCampOverlay
Camp detection (11C): gradual dark screen punishment.
Server sends STAT_CAMPER 0-255 as desiredDarkness.
Client smoothly lerps actualDarkness toward desired.
Draws fullscreen dark overlay + persistent "Move!" text.
=================
*/
static void CG_DrawCampOverlay( void ) {
	static float actualDarkness = 0;
	float desiredDarkness, deltaTime;
	int camp;

	camp = cg.snap->ps.stats[STAT_CAMPER];
	desiredDarkness = camp / 255.0f * 0.85f;
	deltaTime = cg.frametime * 0.001f;

	// smooth transition toward desired level
	// darken slowly (0.5/sec = ~1.7s to full dark), recover faster (1.5/sec = ~0.6s to clear)
	if ( actualDarkness < desiredDarkness ) {
		actualDarkness += deltaTime * 0.5f;
		if ( actualDarkness > desiredDarkness ) actualDarkness = desiredDarkness;
	} else if ( actualDarkness > desiredDarkness ) {
		actualDarkness -= deltaTime * 1.5f;
		if ( actualDarkness < 0 ) actualDarkness = 0;
	}

	if ( actualDarkness > 0.01f ) {
		vec4_t color;
		vec4_t textColor;
		int textWidth;
		const char *msg = "Move!";

		// fullscreen dark overlay (physical screen coords for full widescreen coverage)
		color[0] = color[1] = color[2] = 0;
		color[3] = actualDarkness;
		trap_R_SetColor( color );
		trap_R_DrawStretchPicNorm( 0, 0, 1.0f, 1.0f,
			0, 0, 0, 0, cgs.media.whiteShader );
		trap_R_SetColor( NULL );

		// "Move!" text -- always visible while overlay is active
		textColor[0] = 1.0f;
		textColor[1] = 0.3f;
		textColor[2] = 0.3f;
		textColor[3] = actualDarkness / 0.85f;  // text fades with overlay
		textWidth = CG_DrawStrlen( msg ) * BIGCHAR_WIDTH;
		trap_R_DrawTextNorm( msg, (float)(( 640 - textWidth ) / 2) * NORM_HSCALE, 480 * 0.30f * NORM_VSCALE,
			FONT_UI, (float)BIGCHAR_HEIGHT * NORM_VSCALE, textColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
	}
}

/*
=================
CG_Draw2D
=================
*/
static void CG_Draw2D(stereoFrame_t stereoFrame)
{
	// if we are taking a levelshot for the menu, don't draw anything
	if ( cg.levelShot ) {
		return;
	}

	if ( cg_draw2D.integer == 0 ) {
		return;
	}

	if ( cg.snap->ps.pm_type == PM_INTERMISSION ) {
		CG_DrawIntermission();
		return;
	}

	// camp detection overlay (drawn before HUD so UI elements render on top)
	CG_DrawCampOverlay();

	// bot directive text above heads — must be 2D pass (after trap_R_RenderScene)
	CG_Draw2DBotDirectives();

	// score/damage plums — 2D projected text, same timing constraint
	CG_DrawPlumOverlays();

/*
	if (cg.cameraMode) {
		return;
	}
*/
#if FEAT_WIRED_UI
	// Wired UI: always push game state to client
	CG_WiredHudPushState();
	CG_ScanForCrosshairEntity();  // updates crosshairClientNum for state bridge
	// crosshair + crosshair names drawn by Wired UI elements
	// (cl_wired_hud_elem_crosshair.c, cl_wired_hud_elem_target_name.c)
	// route center print through Wired UI message queue instead of drawing directly
	if ( cg.centerPrintTime && cg.centerPrint[0] ) {
		trap_WiredUI_PushEvent( WIRED_EVENT_CENTERPRINT, cg.centerPrint );
		cg.centerPrintTime = 0;  // consumed — don't push again
	}
	return;
#endif

}

/*
=============
CG_TileFillBox

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
static void CG_TileFillBox( int x, int y, int w, int h, qhandle_t hShader ) {
	float	s1, t1, s2, t2;
	float	nx, ny, nw, nh;

	s1 = x/64.0;
	t1 = y/64.0;
	s2 = (x+w)/64.0;
	t2 = (y+h)/64.0;

	/* Convert real pixel coords to normalized 0.0-1.0 space */
	nx = (float)x / cgs.glconfig.vidWidth;
	ny = (float)y / cgs.glconfig.vidHeight;
	nw = (float)w / cgs.glconfig.vidWidth;
	nh = (float)h / cgs.glconfig.vidHeight;
	trap_R_DrawStretchPicNorm( nx, ny, nw, nh, s1, t1, s2, t2, hShader );
}

/*
==============
CG_TileClear

Clear around a sized down screen
==============
*/
void CG_TileClear( void ) {
	int		top, bottom, left, right;
	int		w, h;

	w = cgs.glconfig.vidWidth;
	h = cgs.glconfig.vidHeight;

	if ( cg.refdef.x == 0 && cg.refdef.y == 0 && 
		cg.refdef.width == w && cg.refdef.height == h ) {
		return;		// full screen rendering
	}

	top = cg.refdef.y;
	bottom = top + cg.refdef.height-1;
	left = cg.refdef.x;
	right = left + cg.refdef.width-1;

	// clear above view screen
	CG_TileFillBox( 0, 0, w, top, cgs.media.backTileShader );

	// clear below view screen
	CG_TileFillBox( 0, bottom, w, h - bottom, cgs.media.backTileShader );

	// clear left of view screen
	CG_TileFillBox( 0, top, left, bottom - top + 1, cgs.media.backTileShader );

	// clear right of view screen
	CG_TileFillBox( right, top, w - right, bottom - top + 1, cgs.media.backTileShader );
}


/*
=====================
CG_DrawActive

Perform all drawing needed to completely fill the screen
=====================
*/
void CG_DrawActive( stereoFrame_t stereoView ) {
	if ( !cg.snap ) {
		return;
	}



	// clear around the rendered view if sized down
	CG_TileClear();

	if(stereoView != STEREO_CENTER)
		CG_DrawCrosshair3D();

	// draw 3D view
	trap_R_RenderScene( &cg.refdef );

	// draw status bar and other floating elements
 	CG_Draw2D(stereoView);
}
