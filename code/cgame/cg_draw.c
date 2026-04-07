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

/* TA UI text painting functions removed -- now handled by Wired UI text system */

/*
==============
CG_DrawField

Draws large numbers for status bar and powerups
==============
*/
#ifndef MISSIONPACK
static void CG_DrawField (int x, int y, int width, int value) {
	char	num[16], *ptr;
	int		l;
	int		frame;

	if ( width < 1 ) {
		return;
	}

	// draw number string
	if ( width > 5 ) {
		width = 5;
	}

	switch ( width ) {
	case 1:
		value = value > 9 ? 9 : value;
		value = value < 0 ? 0 : value;
		break;
	case 2:
		value = value > 99 ? 99 : value;
		value = value < -9 ? -9 : value;
		break;
	case 3:
		value = value > 999 ? 999 : value;
		value = value < -99 ? -99 : value;
		break;
	case 4:
		value = value > 9999 ? 9999 : value;
		value = value < -999 ? -999 : value;
		break;
	}

	Com_sprintf (num, sizeof(num), "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;
	x += 2 + CHAR_WIDTH*(width - l);

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		CG_DrawPicNorm( x * NORM_HSCALE, y * NORM_VSCALE, CHAR_WIDTH * NORM_HSCALE, CHAR_HEIGHT * NORM_VSCALE, cgs.media.numberShaders[frame] );
		x += CHAR_WIDTH;
		ptr++;
		l--;
	}
}
#endif // MISSIONPACK

/*
================
CG_Draw3DModel

================
*/
void CG_Draw3DModel( float x, float y, float w, float h, qhandle_t model, qhandle_t skin, vec3_t origin, vec3_t angles ) {
	refdef_t		refdef;
	refEntity_t		ent;
	float			nx, ny, nw, nh;

	if ( !cg_draw3dIcons.integer || !cg_drawIcons.integer ) {
		return;
	}

	nx = x * NORM_HSCALE; ny = y * NORM_VSCALE;
	nw = w * NORM_HSCALE; nh = h * NORM_VSCALE;
	CG_AdjustPlacement( &nx, &ny, &nw, &nh );
	x = nx * cgs.glconfig.vidWidth;
	y = ny * cgs.glconfig.vidHeight;
	w = nw * cgs.glconfig.vidWidth;
	h = nh * cgs.glconfig.vidHeight;

	memset( &refdef, 0, sizeof( refdef ) );

	memset( &ent, 0, sizeof( ent ) );
	AnglesToAxis( angles, ent.axis );
	VectorCopy( origin, ent.origin );
	ent.hModel = model;
	ent.customSkin = skin;
	ent.renderfx = RF_NOSHADOW;		// no stencil shadows

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear( refdef.viewaxis );

	refdef.fov_x = 30;
	refdef.fov_y = 30;

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	refdef.time = cg.time;

	trap_R_ClearScene();
	trap_R_AddRefEntityToScene( &ent );
	trap_R_RenderScene( &refdef );
}

/*
================
CG_DrawHead

Used for both the status bar and the scoreboard
================
*/
void CG_DrawHead( float x, float y, float w, float h, int clientNum, vec3_t headAngles ) {
	clipHandle_t	cm;
	clientInfo_t	*ci;
	float			len;
	vec3_t			origin;
	vec3_t			mins, maxs;

	ci = &cgs.clientinfo[ clientNum ];

	if ( cg_draw3dIcons.integer ) {
		cm = ci->headModel;
		if ( !cm ) {
			return;
		}

		// offset the origin y and z to center the head
		trap_R_ModelBounds( cm, mins, maxs );

		origin[2] = -0.5 * ( mins[2] + maxs[2] );
		origin[1] = 0.5 * ( mins[1] + maxs[1] );

		// calculate distance so the head nearly fills the box
		// assume heads are taller than wide
		len = 0.7 * ( maxs[2] - mins[2] );		
		origin[0] = len / 0.268;	// len / tan( fov/2 )

		// allow per-model tweaking
		VectorAdd( origin, ci->headOffset, origin );

		CG_Draw3DModel( x, y, w, h, ci->headModel, ci->headSkin, origin, headAngles );
	} else if ( cg_drawIcons.integer ) {
		CG_DrawPicNorm( x * NORM_HSCALE, y * NORM_VSCALE, w * NORM_HSCALE, h * NORM_VSCALE, ci->modelIcon );
	}

	// if they are deferred, draw a cross out
	if ( ci->deferred ) {
		CG_DrawPicNorm( x * NORM_HSCALE, y * NORM_VSCALE, w * NORM_HSCALE, h * NORM_VSCALE, cgs.media.deferShader );
	}
}

/*
================
CG_DrawFlagModel

Used for both the status bar and the scoreboard
================
*/
void CG_DrawFlagModel( float x, float y, float w, float h, int team, qboolean force2D ) {
	qhandle_t		cm;
	float			len;
	vec3_t			origin, angles;
	vec3_t			mins, maxs;
	qhandle_t		handle;

	if ( !force2D && cg_draw3dIcons.integer ) {

		VectorClear( angles );

		cm = cgs.media.redFlagModel;

		// offset the origin y and z to center the flag
		trap_R_ModelBounds( cm, mins, maxs );

		origin[2] = -0.5 * ( mins[2] + maxs[2] );
		origin[1] = 0.5 * ( mins[1] + maxs[1] );

		// calculate distance so the flag nearly fills the box
		// assume heads are taller than wide
		len = 0.5 * ( maxs[2] - mins[2] );		
		origin[0] = len / 0.268;	// len / tan( fov/2 )

		angles[YAW] = 60 * sin( cg.time / 2000.0 );;

		if( team == TEAM_RED ) {
			handle = cgs.media.redFlagModel;
		} else if( team == TEAM_BLUE ) {
			handle = cgs.media.blueFlagModel;
		} else if( team == TEAM_FREE ) {
			handle = cgs.media.neutralFlagModel;
		} else {
			return;
		}
		CG_Draw3DModel( x, y, w, h, handle, 0, origin, angles );
	} else if ( cg_drawIcons.integer ) {
		gitem_t *item;

		if( team == TEAM_RED ) {
			item = BG_FindItemForPowerup( PW_REDFLAG );
		} else if( team == TEAM_BLUE ) {
			item = BG_FindItemForPowerup( PW_BLUEFLAG );
		} else if( team == TEAM_FREE ) {
			item = BG_FindItemForPowerup( PW_NEUTRALFLAG );
		} else {
			return;
		}
		if (item) {
		  CG_DrawPicNorm( x * NORM_HSCALE, y * NORM_VSCALE, w * NORM_HSCALE, h * NORM_VSCALE, cg_items[ ITEM_INDEX(item) ].icon );
		}
	}
}

/*
================
CG_DrawStatusBarHead

================
*/
#ifndef MISSIONPACK

static void CG_DrawStatusBarHead( float x ) {
	vec3_t		angles;
	float		size, stretch;
	float		frac;

	VectorClear( angles );

	if ( cg.damageTime && cg.time - cg.damageTime < DAMAGE_TIME ) {
		frac = (float)(cg.time - cg.damageTime ) / DAMAGE_TIME;
		size = ICON_SIZE * 1.25 * ( 1.5 - frac * 0.5 );

		stretch = size - ICON_SIZE * 1.25;
		// kick in the direction of damage
		x -= stretch * 0.5 + cg.damageX * stretch * 0.5;

		cg.headStartYaw = 180 + cg.damageX * 45;

		cg.headEndYaw = 180 + 20 * cos( crandom()*M_PI );
		cg.headEndPitch = 5 * cos( crandom()*M_PI );

		cg.headStartTime = cg.time;
		cg.headEndTime = cg.time + 100 + random() * 2000;
	} else {
		if ( cg.time >= cg.headEndTime ) {
			// select a new head angle
			cg.headStartYaw = cg.headEndYaw;
			cg.headStartPitch = cg.headEndPitch;
			cg.headStartTime = cg.headEndTime;
			cg.headEndTime = cg.time + 100 + random() * 2000;

			cg.headEndYaw = 180 + 20 * cos( crandom()*M_PI );
			cg.headEndPitch = 5 * cos( crandom()*M_PI );
		}

		size = ICON_SIZE * 1.25;
	}

	// if the server was frozen for a while we may have a bad head start time
	if ( cg.headStartTime > cg.time ) {
		cg.headStartTime = cg.time;
	}

	frac = ( cg.time - cg.headStartTime ) / (float)( cg.headEndTime - cg.headStartTime );
	frac = frac * frac * ( 3 - 2 * frac );
	angles[YAW] = cg.headStartYaw + ( cg.headEndYaw - cg.headStartYaw ) * frac;
	angles[PITCH] = cg.headStartPitch + ( cg.headEndPitch - cg.headStartPitch ) * frac;

	CG_DrawHead( x, 480 - size, size, size,
				cg.snap->ps.clientNum, angles );
}
#endif // MISSIONPACK

/*
================
CG_DrawStatusBarFlag

================
*/
#ifndef MISSIONPACK
static void CG_DrawStatusBarFlag( float x, int team ) {
	CG_DrawFlagModel( x, 480 - ICON_SIZE, ICON_SIZE, ICON_SIZE, team, qfalse );
}
#endif // MISSIONPACK

/*
================
CG_DrawTeamBackground

================
*/
void CG_DrawTeamBackground( int x, int y, int w, int h, float alpha, int team )
{
	vec4_t		hcolor;

	hcolor[3] = alpha;
	if ( team == TEAM_RED ) {
		hcolor[0] = 1;
		hcolor[1] = 0;
		hcolor[2] = 0;
	} else if ( team == TEAM_BLUE ) {
		hcolor[0] = 0;
		hcolor[1] = 0;
		hcolor[2] = 1;
	} else {
		return;
	}
	trap_R_SetColor( hcolor );
	CG_DrawPicNorm( x * NORM_HSCALE, y * NORM_VSCALE, w * NORM_HSCALE, h * NORM_VSCALE, cgs.media.teamStatusBar );
	trap_R_SetColor( NULL );
}

/*
================
CG_DrawStatusBar

================
*/
#ifndef MISSIONPACK
static void CG_DrawStatusBar( void ) {
	int			color;
	centity_t	*cent;
	playerState_t	*ps;
	int			value;
	vec4_t		hcolor;
	vec3_t		angles;
	vec3_t		origin;

	static float colors[4][4] = { 
//		{ 0.2, 1.0, 0.2, 1.0 } , { 1.0, 0.2, 0.2, 1.0 }, {0.5, 0.5, 0.5, 1} };
		{ 1.0f, 0.69f, 0.0f, 1.0f },    // normal
		{ 1.0f, 0.2f, 0.2f, 1.0f },     // low health
		{ 0.5f, 0.5f, 0.5f, 1.0f },     // weapon firing
		{ 1.0f, 1.0f, 1.0f, 1.0f } };   // health > 100

	if ( cg_drawStatus.integer == 0 ) {
		return;
	}

	// draw the team background
	CG_DrawTeamBackground( 0, 420, 640, 60, 0.33f, cg.snap->ps.persistant[PERS_TEAM] );

	cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;

	VectorClear( angles );

	// draw any 3D icons first, so the changes back to 2D are minimized
	if ( cent->currentState.weapon && cg_weapons[ cent->currentState.weapon ].ammoModel ) {
		origin[0] = 70;
		origin[1] = 0;
		origin[2] = 0;
		angles[YAW] = 90 + 20 * sin( cg.time / 1000.0 );
		CG_Draw3DModel( CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE,
					   cg_weapons[ cent->currentState.weapon ].ammoModel, 0, origin, angles );
	}

	CG_DrawStatusBarHead( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE );

	if( cg.predictedPlayerState.powerups[PW_REDFLAG] ) {
		CG_DrawStatusBarFlag( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE + ICON_SIZE, TEAM_RED );
	} else if( cg.predictedPlayerState.powerups[PW_BLUEFLAG] ) {
		CG_DrawStatusBarFlag( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE + ICON_SIZE, TEAM_BLUE );
	} else if( cg.predictedPlayerState.powerups[PW_NEUTRALFLAG] ) {
		CG_DrawStatusBarFlag( 185 + CHAR_WIDTH*3 + TEXT_ICON_SPACE + ICON_SIZE, TEAM_FREE );
	}

    if (cgs.gametype == GT_KINGOFTHEHILL && ps->powerups[PW_KING]) {
        CG_DrawPicNorm((185 + CHAR_WIDTH * 3 + TEXT_ICON_SPACE + ICON_SIZE) * NORM_HSCALE, (480 - ICON_SIZE) * NORM_VSCALE, ICON_SIZE * NORM_HSCALE, ICON_SIZE * NORM_VSCALE, cgs.media.medalExcellent);
    }

    if (ps->stats[STAT_ARMORCLASS] > ARM_NONE && ps->stats[STAT_ARMOR]) {
        qhandle_t *model;
        
        switch (ps->stats[STAT_ARMORCLASS]) {
        case ARM_HEAVY:
            model = &cgs.media.heavyArmorModel;
            break;
        case ARM_COMBAT:
            model = &cgs.media.combatArmorModel;
            break;
        case ARM_JACKET:
            model = &cgs.media.jacketArmorModel;
            break;
        default:
            model = NULL;
        }

		origin[0] = 90;
		origin[1] = 0;
		origin[2] = -10;
		angles[YAW] = ( cg.time & 2047 ) * 360 / 2048.0;
		// CG_Draw3DModel( 370 + CHAR_WIDTH*3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE,
		//			   cgs.media.armorModel, 0, origin, angles );
        if (model) {
            CG_Draw3DModel(370 + CHAR_WIDTH * 3 + TEXT_ICON_SPACE, 432, ICON_SIZE, ICON_SIZE,
                *model, 0, origin, angles); // CPM
        }
	}
	//
	// ammo
	//
	if ( cent->currentState.weapon ) {
		value = ps->ammo[cent->currentState.weapon];
		if ( value > -1 ) {
			if ( cg.predictedPlayerState.weaponstate == WEAPON_FIRING
				&& cg.predictedPlayerState.weaponTime > 100 ) {
				// draw as dark grey when reloading
				color = 2;	// dark grey
			} else {
				if ( value >= 0 ) {
					color = 0;	// green
				} else {
					color = 1;	// red
				}
			}
			trap_R_SetColor( colors[color] );
			
			CG_DrawField (0, 432, 3, value);
			trap_R_SetColor( NULL );

			// if we didn't draw a 3D icon, draw a 2D icon for ammo
			if ( !cg_draw3dIcons.integer && cg_drawIcons.integer ) {
				qhandle_t	icon;

				icon = cg_weapons[ cg.predictedPlayerState.weapon ].ammoIcon;
				if ( icon ) {
					CG_DrawPicNorm( (CHAR_WIDTH*3 + TEXT_ICON_SPACE) * NORM_HSCALE, 432 * NORM_VSCALE, ICON_SIZE * NORM_HSCALE, ICON_SIZE * NORM_VSCALE, icon );
				}
			}
		}
	}

	//
	// health
	//
	value = ps->stats[STAT_HEALTH];
	if ( value > 100 ) {
		trap_R_SetColor( colors[3] );		// white
	} else if (value > 25) {
		trap_R_SetColor( colors[0] );	// green
	} else if (value > 0) {
		color = (cg.time >> 8) & 1;	// flash
		trap_R_SetColor( colors[color] );
	} else {
		trap_R_SetColor( colors[1] );	// red
	}

	// stretch the health up when taking damage
	CG_DrawField ( 185, 432, 3, value);
	CG_GetColorForAmount( cg.snap->ps.stats[STAT_HEALTH], cg.snap->ps.stats[STAT_ARMOR], hcolor );
	trap_R_SetColor( hcolor );


	//
	// armor
	//
	value = ps->stats[STAT_ARMOR];
    if (ps->stats[STAT_ARMORCLASS] > ARM_NONE && value > 0) {
        qhandle_t *shader;

        switch (ps->stats[STAT_ARMORCLASS]) {
        case ARM_HEAVY:
            shader = &cgs.media.heavyArmorIcon;
            break;
        case ARM_COMBAT:
            shader = &cgs.media.combatArmorIcon;
            break;
        case ARM_JACKET:
            shader = &cgs.media.jacketArmorIcon;
            break;
        default:
            shader = NULL;
        }

		trap_R_SetColor( colors[0] );
		CG_DrawField (370, 432, 3, value);
		trap_R_SetColor( NULL );
		// if we didn't draw a 3D icon, draw a 2D icon for armor
		if ( shader && !cg_draw3dIcons.integer && cg_drawIcons.integer ) {
            CG_DrawPicNorm((370 + CHAR_WIDTH * 3 + TEXT_ICON_SPACE) * NORM_HSCALE, 432 * NORM_VSCALE, ICON_SIZE * NORM_HSCALE, ICON_SIZE * NORM_VSCALE, *shader);
		}

	}
}
#endif

/*
===========================================================================================

  UPPER RIGHT CORNER

===========================================================================================
*/

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
=====================
CG_DrawUpperRight

=====================
*/
static void CG_DrawUpperRight(stereoFrame_t stereoFrame)
{
	float	y;

	y = 0;

	CG_SetScreenPlacement( PLACE_RIGHT, PLACE_TOP );

	if ( cgs.gametype >= GT_TDM && cg_drawTeamOverlay.integer == 1 ) {
		y = CG_DrawTeamOverlay( y, qtrue, qtrue );
	}
	if ( cg_drawSnapshot.integer ) {
		y = CG_DrawSnapshot( y );
	}
	if (cg_drawFPS.integer && (stereoFrame == STEREO_CENTER || stereoFrame == STEREO_RIGHT)) {
		y = CG_DrawFPS( y );
	}
	if ( cg_drawTimer.integer ) {
		y = CG_DrawTimer( y );
	}

	CG_PopScreenPlacement();
}

/*
===========================================================================================

  LOWER RIGHT CORNER

===========================================================================================
*/

/*
=================
CG_DrawScores

Draw the small two score display
=================
*/
#ifndef MISSIONPACK
static float CG_DrawScores( float y ) {
	const char	*s;
	int			s1, s2, score;
	int			x, w;
	int			v;
	vec4_t		color;
	float		y1;
	gitem_t		*item;

	s1 = cgs.scores1;
	s2 = cgs.scores2;

	y -=  BIGCHAR_HEIGHT + 8;

	y1 = y;

	// draw from the right side to left
	if ( cgs.gametype >= GT_TDM ) {
		x = 640;
		color[0] = 0.0f;
		color[1] = 0.0f;
		color[2] = 1.0f;
		color[3] = 0.33f;
		s = va( "%2i", s2 );
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
		x -= w;
		CG_FillRectNorm( x * NORM_HSCALE, (y-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, color );
		if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE ) {
			CG_DrawPicNorm( x * NORM_HSCALE, (y-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, cgs.media.selectShader );
		}
		trap_R_DrawTextNorm( s, (float)(x + 4) * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );

		if ( cgs.gametype == GT_CTF ) {
			// Display flag status
			item = BG_FindItemForPowerup( PW_BLUEFLAG );

			if (item) {
				y1 = y - BIGCHAR_HEIGHT - 8;
				if( cgs.blueflag >= 0 && cgs.blueflag <= 2 ) {
					CG_DrawPicNorm( x * NORM_HSCALE, (y1-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, cgs.media.blueFlagShader[cgs.blueflag] );
				}
			}
		}
		color[0] = 1.0f;
		color[1] = 0.0f;
		color[2] = 0.0f;
		color[3] = 0.33f;
		s = va( "%2i", s1 );
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
		x -= w;
		CG_FillRectNorm( x * NORM_HSCALE, (y-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, color );
		if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED ) {
			CG_DrawPicNorm( x * NORM_HSCALE, (y-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, cgs.media.selectShader );
		}
		trap_R_DrawTextNorm( s, (float)(x + 4) * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );

		if ( cgs.gametype == GT_CTF ) {
			// Display flag status
			item = BG_FindItemForPowerup( PW_REDFLAG );

			if (item) {
				y1 = y - BIGCHAR_HEIGHT - 8;
				if( cgs.redflag >= 0 && cgs.redflag <= 2 ) {
					CG_DrawPicNorm( x * NORM_HSCALE, (y1-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, cgs.media.redFlagShader[cgs.redflag] );
				}
			}
		}

		v = cgs.scorelimit;
		if ( v ) {
			s = va( "%2i", v );
			w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
			x -= w;
			trap_R_DrawTextNorm( s, (float)(x + 4) * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		}

	} else {
		qboolean	spectator;

		x = 640;
		score = cg.snap->ps.persistant[PERS_SCORE];
		spectator = ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR );

		// always show your score in the second box if not in first place
		if ( s1 != score ) {
			s2 = score;
		}
		if ( s2 != SCORE_NOT_PRESENT ) {
			s = va( "%2i", s2 );
			w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
			x -= w;
			if ( !spectator && score == s2 && score != s1 ) {
				color[0] = 1.0f;
				color[1] = 0.0f;
				color[2] = 0.0f;
				color[3] = 0.33f;
				CG_FillRectNorm( x * NORM_HSCALE, (y-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, color );
				CG_DrawPicNorm( x * NORM_HSCALE, (y-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, cgs.media.selectShader );
			} else {
				color[0] = 0.5f;
				color[1] = 0.5f;
				color[2] = 0.5f;
				color[3] = 0.33f;
				CG_FillRectNorm( x * NORM_HSCALE, (y-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, color );
			}
			trap_R_DrawTextNorm( s, (float)(x + 4) * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		}

		// first place
		if ( s1 != SCORE_NOT_PRESENT ) {
			s = va( "%2i", s1 );
			w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
			x -= w;
			if ( !spectator && score == s1 ) {
				color[0] = 0.0f;
				color[1] = 0.0f;
				color[2] = 1.0f;
				color[3] = 0.33f;
				CG_FillRectNorm( x * NORM_HSCALE, (y-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, color );
				CG_DrawPicNorm( x * NORM_HSCALE, (y-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, cgs.media.selectShader );
			} else {
				color[0] = 0.5f;
				color[1] = 0.5f;
				color[2] = 0.5f;
				color[3] = 0.33f;
				CG_FillRectNorm( x * NORM_HSCALE, (y-4) * NORM_VSCALE, w * NORM_HSCALE, (BIGCHAR_HEIGHT+8) * NORM_VSCALE, color );
			}
			trap_R_DrawTextNorm( s, (float)(x + 4) * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		}

		if ( cgs.scorelimit ) {
			s = va( "%2i", cgs.scorelimit );
			w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH + 8;
			x -= w;
			trap_R_DrawTextNorm( s, (float)(x + 4) * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		}

	}

	return y1 - 8;
}
#endif // MISSIONPACK

/*
================
CG_DrawPowerups
================
*/
#ifndef MISSIONPACK
static float CG_DrawPowerups( float y ) {
	int		sorted[MAX_POWERUPS];
	int		sortedTime[MAX_POWERUPS];
	int		i, j, k;
	int		active;
	playerState_t	*ps;
	int		t;
	gitem_t	*item;
	int		x;
	int		color;
	float	size;
	float	f;
	static float colors[2][4] = { 
    { 0.2f, 1.0f, 0.2f, 1.0f } , 
    { 1.0f, 0.2f, 0.2f, 1.0f } 
  };

	ps = &cg.snap->ps;

	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		return y;
	}

	// sort the list by time remaining
	active = 0;
	for ( i = PW_NONE + 1 ; i < PW_NUM_POWERUPS ; i++ ) {
		if ( !ps->powerups[ i ] ) {
			continue;
		}

		// ZOID--don't draw if the power up has unlimited time
		// This is true of the CTF flags
		if ( ps->powerups[ i ] == INT_MAX ) {
			continue;
		}

		t = ps->powerups[ i ] - cg.time;
		if ( t <= 0 ) {
			continue;
		}

		// insert into the list
		for ( j = 0 ; j < active ; j++ ) {
			if ( sortedTime[j] >= t ) {
				for ( k = active - 1 ; k >= j ; k-- ) {
					sorted[k+1] = sorted[k];
					sortedTime[k+1] = sortedTime[k];
				}
				break;
			}
		}
		sorted[j] = i;
		sortedTime[j] = t;
		active++;
	}

	// draw the icons and timers
	x = 640 - ICON_SIZE - CHAR_WIDTH * 2;
	for ( i = 0 ; i < active ; i++ ) {
		item = BG_FindItemForPowerup( sorted[i] );

    if (item) {

		  color = 1;

		  y -= ICON_SIZE;

		  trap_R_SetColor( colors[color] );
		  CG_DrawField( x, y, 2, sortedTime[ i ] / 1000 );

		  t = ps->powerups[ sorted[i] ];
		  if ( t - cg.time >= POWERUP_BLINKS * POWERUP_BLINK_TIME ) {
			  trap_R_SetColor( NULL );
		  } else {
			  vec4_t	modulate;

			  f = (float)( t - cg.time ) / POWERUP_BLINK_TIME;
			  f -= (int)f;
			  modulate[0] = modulate[1] = modulate[2] = modulate[3] = f;
			  trap_R_SetColor( modulate );
		  }

		  if ( cg.powerupActive == sorted[i] && 
			  cg.time - cg.powerupTime < PULSE_TIME ) {
			  f = 1.0 - ( ( (float)cg.time - cg.powerupTime ) / PULSE_TIME );
			  size = ICON_SIZE * ( 1.0 + ( PULSE_SCALE - 1.0 ) * f );
		  } else {
			  size = ICON_SIZE;
		  }

		  CG_DrawPicNorm( (640 - size) * NORM_HSCALE, (y + ICON_SIZE / 2 - size / 2) * NORM_VSCALE,
			  size * NORM_HSCALE, size * NORM_VSCALE, trap_R_RegisterShader( item->icon ) );
    }
	}
	trap_R_SetColor( NULL );

	return y;
}
#endif // MISSIONPACK

/*
=====================
CG_DrawLowerRight

=====================
*/
#ifndef MISSIONPACK
static void CG_DrawLowerRight( void ) {
	float	y;

	y = 480 - ICON_SIZE;

	CG_SetScreenPlacement( PLACE_RIGHT, PLACE_BOTTOM );

	if ( cgs.gametype >= GT_TDM && cg_drawTeamOverlay.integer == 2 ) {
		y = CG_DrawTeamOverlay( y, qtrue, qfalse );
	}

	y = CG_DrawScores( y );

	// speed display above mini scoreboard (2C)
	if ( cg_drawSpeed.integer && cg.snap ) {
		y = CG_DrawSpeed( y );
	}

	CG_DrawPowerups( y );

	CG_PopScreenPlacement();
}
#endif // MISSIONPACK

/*
===================
CG_DrawPickupItem
===================
*/
#ifndef MISSIONPACK
static int CG_DrawPickupItem( int y ) {
	int		value;
	float	*fadeColor;

	if ( cg.snap->ps.stats[STAT_HEALTH] <= 0 ) {
		return y;
	}

	y -= ICON_SIZE;

	value = cg.itemPickup;
	if ( value ) {
		fadeColor = CG_FadeColor( cg.itemPickupTime, 3000 );
		if ( fadeColor ) {
			CG_RegisterItemVisuals( value );
			trap_R_SetColor( fadeColor );
			CG_DrawPicNorm( 8 * NORM_HSCALE, y * NORM_VSCALE, ICON_SIZE * NORM_HSCALE, ICON_SIZE * NORM_VSCALE, cg_items[ value ].icon );
			trap_R_DrawTextNorm( bg_itemlist[ value ].pickup_name, (float)(ICON_SIZE + 16) * NORM_HSCALE, (float)(y + (ICON_SIZE/2 - BIGCHAR_HEIGHT/2)) * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, fadeColor, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
			trap_R_SetColor( NULL );
		}
	}
	
	return y;
}
#endif // MISSIONPACK

/*
=====================
CG_DrawLowerLeft

=====================
*/
#ifndef MISSIONPACK
static void CG_DrawLowerLeft( void ) {
	float	y;

	y = 480 - ICON_SIZE;

	CG_SetScreenPlacement( PLACE_LEFT, PLACE_BOTTOM );

	if ( cgs.gametype >= GT_TDM && cg_drawTeamOverlay.integer == 3 ) {
		y = CG_DrawTeamOverlay( y, qfalse, qfalse );
	}

	CG_DrawPickupItem( y );

	CG_PopScreenPlacement();
}
#endif // MISSIONPACK


//===========================================================================================

/*
=================
CG_DrawTeamInfo
=================
*/
#ifndef MISSIONPACK
static void CG_DrawTeamInfo( void ) {
	int h;
	int i;
	vec4_t		hcolor;
	int		chatHeight;

#define CHATLOC_Y 420 // bottom end
#define CHATLOC_X 0

	if (cg_teamChatHeight.integer < TEAMCHAT_HEIGHT)
		chatHeight = cg_teamChatHeight.integer;
	else
		chatHeight = TEAMCHAT_HEIGHT;
	if (chatHeight <= 0)
		return; // disabled

	if (cgs.teamLastChatPos != cgs.teamChatPos) {
		if (cg.time - cgs.teamChatMsgTimes[cgs.teamLastChatPos % chatHeight] > cg_teamChatTime.integer) {
			cgs.teamLastChatPos++;
		}

		h = (cgs.teamChatPos - cgs.teamLastChatPos) * TINYCHAR_HEIGHT;

		if ( cgs.clientinfo[cg.clientNum].team == TEAM_RED ) {
			hcolor[0] = 1.0f;
			hcolor[1] = 0.0f;
			hcolor[2] = 0.0f;
			hcolor[3] = 0.33f;
		} else if ( cgs.clientinfo[cg.clientNum].team == TEAM_BLUE ) {
			hcolor[0] = 0.0f;
			hcolor[1] = 0.0f;
			hcolor[2] = 1.0f;
			hcolor[3] = 0.33f;
		} else {
			hcolor[0] = 0.0f;
			hcolor[1] = 1.0f;
			hcolor[2] = 0.0f;
			hcolor[3] = 0.33f;
		}

		trap_R_SetColor( hcolor );
		CG_DrawPicNorm( CHATLOC_X * NORM_HSCALE, (CHATLOC_Y - h) * NORM_VSCALE, 640 * NORM_HSCALE, h * NORM_VSCALE, cgs.media.teamStatusBar );
		trap_R_SetColor( NULL );

		hcolor[0] = hcolor[1] = hcolor[2] = 1.0f;
		hcolor[3] = 1.0f;

		for (i = cgs.teamChatPos - 1; i >= cgs.teamLastChatPos; i--) {
			trap_R_DrawTextNorm( cgs.teamChatMsgs[i % chatHeight], (float)(CHATLOC_X + TINYCHAR_WIDTH) * NORM_HSCALE,
				(float)(CHATLOC_Y - (cgs.teamChatPos - i)*TINYCHAR_HEIGHT) * NORM_VSCALE,
				FONT_UI, (float)TINYCHAR_HEIGHT * NORM_VSCALE, hcolor, TEXT_ALIGN_LEFT, 0 );
		}
	}
}
#endif // MISSIONPACK

/*
===================
CG_DrawHoldableItem
===================
*/
#ifndef MISSIONPACK
static void CG_DrawHoldableItem( void ) { 
	int		value;

	value = cg.snap->ps.stats[STAT_HOLDABLE_ITEM];
	if ( value ) {
		CG_RegisterItemVisuals( value );
		CG_DrawPicNorm( (640-ICON_SIZE) * NORM_HSCALE, ((480-ICON_SIZE)/2) * NORM_VSCALE, ICON_SIZE * NORM_HSCALE, ICON_SIZE * NORM_VSCALE, cg_items[ value ].icon );
	}

}
#endif // MISSIONPACK


/*
===================
CG_DrawReward
===================
*/
static void CG_DrawReward( void ) { 
	float	*color;
	int		i, count;
	float	x, y;
	char	buf[32];

	if ( !cg_drawRewards.integer ) {
		return;
	}

	color = CG_FadeColor( cg.rewardTime, REWARD_TIME );
	if ( !color ) {
		if (cg.rewardStack > 0) {
			for(i = 0; i < cg.rewardStack; i++) {
				cg.rewardSound[i] = cg.rewardSound[i+1];
				cg.rewardShader[i] = cg.rewardShader[i+1];
				cg.rewardCount[i] = cg.rewardCount[i+1];
			}
			cg.rewardTime = cg.time;
			cg.rewardStack--;
			color = CG_FadeColor( cg.rewardTime, REWARD_TIME );
			trap_S_StartLocalSound(cg.rewardSound[0], CHAN_ANNOUNCER);
		} else {
			return;
		}
	}

	trap_R_SetColor( color );

	/* old big-reward icon loop removed */

	if ( cg.rewardCount[0] >= 10 ) {
		y = 56;
		x = 320 - ICON_SIZE/2;
		CG_DrawPicNorm( x * NORM_HSCALE, y * NORM_VSCALE, (ICON_SIZE-4) * NORM_HSCALE, (ICON_SIZE-4) * NORM_VSCALE, cg.rewardShader[0] );
		Com_sprintf(buf, sizeof(buf), "%d", cg.rewardCount[0]);
		x = ( 640 - SMALLCHAR_WIDTH * CG_DrawStrlen( buf ) ) / 2;
		trap_R_DrawTextNorm( buf, (float)x * NORM_HSCALE, (float)(y+ICON_SIZE) * NORM_VSCALE, FONT_UI, (float)SMALLCHAR_HEIGHT * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
	}
	else {

		count = cg.rewardCount[0];

		y = 56;
		x = 320 - count * ICON_SIZE/2;
		for ( i = 0 ; i < count ; i++ ) {
			CG_DrawPicNorm( x * NORM_HSCALE, y * NORM_VSCALE, (ICON_SIZE-4) * NORM_HSCALE, (ICON_SIZE-4) * NORM_VSCALE, cg.rewardShader[0] );
			x += ICON_SIZE;
		}
	}
	trap_R_SetColor( NULL );
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

#if FEAT_TA_UI
	x = 640 - 48;
	y = 480 - 144;
#else
	x = 640 - 48;
	y = 480 - 48;
#endif

	CG_DrawPicNorm( x * NORM_HSCALE, y * NORM_VSCALE, 48 * NORM_HSCALE, 48 * NORM_VSCALE, trap_R_RegisterShader("gfx/2d/net.tga" ) );
}


#define	MAX_LAGOMETER_PING	900
#define	MAX_LAGOMETER_RANGE	300

/*
==============
CG_DrawLagometer
==============
*/
static void CG_DrawLagometer( void ) {
	int		a, x, y, i;
	float	v;
	float	ax, ay, aw, ah, mid, range;
	int		color;
	float	vscale;

	if ( !cg_lagometer.integer || cgs.localServer ) {
		CG_DrawDisconnect();
		return;
	}

	//
	// draw the graph
	//
{
	float pixW, pixH;
#if FEAT_TA_UI
	x = (int)(640 - 48);
	y = (int)(480 - 144);
#else
	x = (int)(640 - 48);
	y = (int)(480 - 48);
#endif

	trap_R_SetColor( NULL );
	CG_DrawPicNorm( x * NORM_HSCALE, y * NORM_VSCALE, 48 * NORM_HSCALE, 48 * NORM_VSCALE, cgs.media.lagometerShader );

	ax = (float)x * NORM_HSCALE;
	ay = (float)y * NORM_VSCALE;
	aw = 48 * NORM_HSCALE;
	ah = 48 * NORM_VSCALE;
	CG_AdjustPlacement( &ax, &ay, &aw, &ah );
	// convert to pixels for per-column iteration
	ax *= cgs.glconfig.vidWidth;
	ay *= cgs.glconfig.vidHeight;
	aw *= cgs.glconfig.vidWidth;
	ah *= cgs.glconfig.vidHeight;
	pixW = 1.0f / cgs.glconfig.vidWidth;
	pixH = 1.0f / cgs.glconfig.vidHeight;

	color = -1;
	range = ah / 3;
	mid = ay + range;

	vscale = range / MAX_LAGOMETER_RANGE;

	// draw the frame interpolate / extrapolate graph
	for ( a = 0 ; a < (int)aw ; a++ ) {
		i = ( lagometer.frameCount - 1 - a ) & (LAG_SAMPLES - 1);
		v = lagometer.frameSamples[i];
		v *= vscale;
		if ( v > 0 ) {
			if ( color != 1 ) {
				color = 1;
				trap_R_SetColor( g_color_table[ColorIndex(COLOR_YELLOW)] );
			}
			if ( v > range ) {
				v = range;
			}
			trap_R_DrawStretchPicNorm( (ax + aw - a) * pixW, (mid - v) * pixH, pixW, v * pixH, 0, 0, 0, 0, cgs.media.whiteShader );
		} else if ( v < 0 ) {
			if ( color != 2 ) {
				color = 2;
				trap_R_SetColor( g_color_table[ColorIndex(COLOR_BLUE)] );
			}
			v = -v;
			if ( v > range ) {
				v = range;
			}
			trap_R_DrawStretchPicNorm( (ax + aw - a) * pixW, mid * pixH, pixW, v * pixH, 0, 0, 0, 0, cgs.media.whiteShader );
		}
	}

	// draw the snapshot latency / drop graph
	range = ah / 2;
	vscale = range / MAX_LAGOMETER_PING;

	for ( a = 0 ; a < (int)aw ; a++ ) {
		i = ( lagometer.snapshotCount - 1 - a ) & (LAG_SAMPLES - 1);
		v = lagometer.snapshotSamples[i];
		if ( v > 0 ) {
			if ( lagometer.snapshotFlags[i] & SNAPFLAG_RATE_DELAYED ) {
				if ( color != 5 ) {
					color = 5;
					trap_R_SetColor( g_color_table[ColorIndex(COLOR_YELLOW)] );
				}
			} else {
				if ( color != 3 ) {
					color = 3;
					trap_R_SetColor( g_color_table[ColorIndex(COLOR_GREEN)] );
				}
			}
			v = v * vscale;
			if ( v > range ) {
				v = range;
			}
			trap_R_DrawStretchPicNorm( (ax + aw - a) * pixW, (ay + ah - v) * pixH, pixW, v * pixH, 0, 0, 0, 0, cgs.media.whiteShader );
		} else if ( v < 0 ) {
			if ( color != 4 ) {
				color = 4;
				trap_R_SetColor( g_color_table[ColorIndex(COLOR_RED)] );
			}
			trap_R_DrawStretchPicNorm( (ax + aw - a) * pixW, (ay + ah - range) * pixH, pixW, range * pixH, 0, 0, 0, 0, cgs.media.whiteShader );
		}
	}

	trap_R_SetColor( NULL );
}

	if ( cg_nopredict.integer || cg_synchronousClients.integer ) {
		trap_R_DrawTextNorm( "snc", (float)x * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
	}

	CG_DrawDisconnect();
}



/*
===============================================================================

CENTER PRINTING

===============================================================================
*/


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

		w = (int)(trap_R_MeasureTextNorm( linebuffer, FONT_UI, (float)((int)(cg.centerPrintCharWidth * 1.5)) * NORM_VSCALE ) / NORM_HSCALE);
		x = ( 640 - w ) / 2;
		trap_R_DrawTextNorm( linebuffer, (float)x * NORM_HSCALE, (float)y * NORM_VSCALE, FONT_UI, (float)((int)(cg.centerPrintCharWidth * 1.5)) * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		y += cg.centerPrintCharWidth * 1.5;
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
CG_DrawCrosshair
=================
*/
static void CG_DrawCrosshair(void)
{
	float		w, h;
	qhandle_t	hShader;
	float		f;
	float		x, y;
	int			ca;

	if ( !cg_drawCrosshair.integer ) {
		return;
	}

	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
		return;
	}

#if !FEAT_THIRD_PERSON
	if ( cg.renderingThirdPerson ) {
		return;
	}
#endif

	// set crosshair color
	{
		vec4_t xhairColor;
		if ( cg_crosshairHealth.integer ) {
			int effectiveHealth = BG_GetEffectiveHealth( cg.snap->ps.stats[STAT_HEALTH], cg.snap->ps.stats[STAT_ARMORCLASS], cg.snap->ps.stats[STAT_ARMOR] );
			BG_GetColorForAmount( effectiveHealth, xhairColor );
		} else {
			CG_ParseColor( cg_crosshairColor.string, xhairColor, 1.0f );
		}
		xhairColor[3] = cg_crosshairAlpha.value;
		if ( xhairColor[3] < 0 ) xhairColor[3] = 0;
		if ( xhairColor[3] > 1 ) xhairColor[3] = 1;
		// rocket launcher helix: amber tint during alt-fire cooldown
		if ( cg.snap->ps.weapon == WP_ROCKET_LAUNCHER
			&& bg_weaponlist[WP_ROCKET_LAUNCHER].attackAlt != ATT_NONE
			&& cg.predictedPlayerState.weaponTime > 0
			&& cg.predictedPlayerState.weaponTime > bg_attacklist[ATT_ROCKET_LAUNCHER_PRIMARY].reloadTime ) {
			xhairColor[0] = 1.0f;   // full red
			xhairColor[1] = 0.65f;  // amber
			xhairColor[2] = 0.0f;   // no blue
		}
		trap_R_SetColor( xhairColor );
	}

	w = h = cg_crosshairSize.value;

	// pulse the size of the crosshair when picking up items
	f = cg.time - cg.itemPickupBlendTime;
	if ( f > 0 && f < ITEM_BLOB_TIME ) {
		f /= ITEM_BLOB_TIME;
		w *= ( 1 + f );
		h *= ( 1 + f );
	}

	// crosshair is self-centered via cg.refdef -- offset should scale but not be biased
	{
		float nx, ny, nw, nh;
		float rw = (float)cgs.glconfig.vidWidth;
		float rh = (float)cgs.glconfig.vidHeight;

		nx = (float)cg_crosshairX.integer * NORM_HSCALE;
		ny = (float)cg_crosshairY.integer * NORM_VSCALE;
		nw = w * NORM_HSCALE;
		nh = h * NORM_VSCALE;
		CG_SetScreenPlacement( PLACE_LEFT, PLACE_TOP );
		CG_AdjustPlacement( &nx, &ny, &nw, &nh );
		CG_PopScreenPlacement();

		hShader = cgs.media.crosshairMiscShader;

		trap_R_DrawStretchPicNorm(
			nx + cg.refdef.x / rw + 0.5f * (cg.refdef.width / rw - nw),
			ny + cg.refdef.y / rh + 0.5f * (cg.refdef.height / rh - nh),
			nw, nh, 0, 0, 1, 1, hShader );
	}

	trap_R_SetColor( NULL );
}

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
	centity_t	*cent;

	VectorCopy( cg.refdef.vieworg, start );
	VectorMA( start, 131072, cg.refdef.viewaxis[0], end );

	CG_Trace( &trace, start, vec3_origin, vec3_origin, end, 
		cg.snap->ps.clientNum, CONTENTS_SOLID|CONTENTS_BODY );
	if ( trace.entityNum >= MAX_CLIENTS ) {
		return;
	}

	// if the player is in fog, don't show it
	content = CG_PointContents( trace.endpos, 0 );
	if ( content & CONTENTS_FOG ) {
		return;
	}

	cent = &cg_entities[trace.entityNum];

	// if the player is invisible, don't show it
	if ( CG_IsPlayerInvisible(cent) ) {
		return;
	}

	// update the fade timer
	cg.crosshairClientNum = trace.entityNum;
	cg.crosshairClientTime = cg.time;
}


/*
=====================
CG_DrawCrosshairNames
=====================
*/
static void CG_DrawCrosshairNames( void ) {
	float		*color;
	char		*name;
	float		w;

	if ( !cg_drawCrosshair.integer ) {
		return;
	}
	if ( !cg_drawCrosshairNames.integer ) {
		return;
	}
	if ( cg.renderingThirdPerson ) {
		return;
	}

	// scan the known entities to see if the crosshair is sighted on one
	CG_ScanForCrosshairEntity();

	// draw the name of the player being looked at
	color = CG_FadeColor( cg.crosshairClientTime, 1000 );
	if ( !color ) {
		trap_R_SetColor( NULL );
		return;
	}

	name = cgs.clientinfo[ cg.crosshairClientNum ].name;
	w = CG_DrawStrlen( name ) * BIGCHAR_WIDTH;
	trap_R_DrawTextNorm( name, (float)(320 - w / 2) * NORM_HSCALE, 170.0f * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, CG_ColorFromAlpha( color[3] * 0.5f ), TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
	trap_R_SetColor( NULL );
}


//==============================================================================

/*
=================
CG_DrawSpectator
=================
*/
static void CG_DrawSpectator(void) {
	trap_R_DrawTextNorm( "SPECTATOR", (float)(320 - 9 * 8) * NORM_HSCALE, 440.0f * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );

	if ( cgs.gametype == GT_DUEL ) {
		trap_R_DrawTextNorm( "waiting to play", (float)(320 - 15 * 8) * NORM_HSCALE, 460.0f * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
	}
	else if ( cgs.gametype >= GT_TDM ) {
		trap_R_DrawTextNorm( "press ESC and use the JOIN menu to play", (float)(320 - 39 * 8) * NORM_HSCALE, 460.0f * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
	}
}

/*
=================
CG_DrawVote
=================
*/
static void CG_DrawVote(void) {
	const char	*s;
	int		sec;

	if ( !cgs.voteTime ) {
		return;
	}

	// play a talk beep whenever it is modified
	if ( cgs.voteModified ) {
		cgs.voteModified = qfalse;
		trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
	}

	sec = ( VOTE_TIME - ( cg.time - cgs.voteTime ) ) / 1000;
	if ( sec < 0 ) {
		sec = 0;
	}
	s = va("VOTE(%i):%s yes:%i no:%i", sec, cgs.voteString, cgs.voteYes, cgs.voteNo);
	trap_R_DrawTextNorm( s, 0.0f, 58.0f * NORM_VSCALE, FONT_UI, (float)SMALLCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, 0 );
}

/*
=================
CG_DrawTeamVote
=================
*/
static void CG_DrawTeamVote(void) {
	const char	*s;
	int		sec, cs_offset;

	if ( cgs.clientinfo[cg.clientNum].team == TEAM_RED )
		cs_offset = 0;
	else if ( cgs.clientinfo[cg.clientNum].team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !cgs.teamVoteTime[cs_offset] ) {
		return;
	}

	// play a talk beep whenever it is modified
	if ( cgs.teamVoteModified[cs_offset] ) {
		cgs.teamVoteModified[cs_offset] = qfalse;
		trap_S_StartLocalSound( cgs.media.talkSound, CHAN_LOCAL_SOUND );
	}

	sec = ( VOTE_TIME - ( cg.time - cgs.teamVoteTime[cs_offset] ) ) / 1000;
	if ( sec < 0 ) {
		sec = 0;
	}
	s = va("TEAMVOTE(%i):%s yes:%i no:%i", sec, cgs.teamVoteString[cs_offset],
							cgs.teamVoteYes[cs_offset], cgs.teamVoteNo[cs_offset] );
	trap_R_DrawTextNorm( s, 0.0f, 90.0f * NORM_VSCALE, FONT_UI, (float)SMALLCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, 0 );
}


static qboolean CG_DrawScoreboard( void ) {
#if FEAT_WIRED_UI
	// when Wired UI is active, the client renders the scoreboard overlay
	// cgame just reports whether the scoreboard should be "showing" (suppresses other HUD)
	if ( cg_wiredUI.integer ) {
		if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD
			 || cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
			return qtrue;
		}
		if ( CG_FadeColor( cg.scoreFadeTime, FADE_TIME ) ) {
			return qtrue;
		}
		return qfalse;
	}
#endif
#if FEAT_TA_UI
	static qboolean firstTime = qtrue;

	if (menuScoreboard) {
		menuScoreboard->window.flags &= ~WINDOW_FORCED;
	}
	if (cg_paused.integer) {
		cg.deferredPlayerLoading = 0;
		firstTime = qtrue;
		return qfalse;
	}

	// should never happen in Team Arena
	if (cg_singlePlayer.integer && cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
		cg.deferredPlayerLoading = 0;
		firstTime = qtrue;
		return qfalse;
	}

	// don't draw scoreboard during death while warmup up
	if ( !cg.showScores ) {
		return qfalse;
	}

	if ( cg.showScores || cg.predictedPlayerState.pm_type == PM_DEAD || cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
	} else {
		if ( !CG_FadeColor( cg.scoreFadeTime, FADE_TIME ) ) {
			// next time scoreboard comes up, don't print killer
			cg.deferredPlayerLoading = 0;
			cg.killerName[0] = 0;
			firstTime = qtrue;
			return qfalse;
		}
	}

	if (menuScoreboard == NULL) {
		if ( cgs.gametype >= GT_TDM ) {
			menuScoreboard = Menus_FindByName("teamscore_menu");
		} else {
			menuScoreboard = Menus_FindByName("score_menu");
		}
	}

	if (menuScoreboard) {
		if (firstTime) {
			CG_SetScoreSelection(menuScoreboard);
			firstTime = qfalse;
		}
		Menu_Paint(menuScoreboard, qtrue);
	}

	// load any models that have been deferred
	if ( ++cg.deferredPlayerLoading > 10 ) {
		CG_LoadDeferredPlayers();
	}

	return qtrue;
#else
	if ( cgs.gametype == GT_DUEL )
		return CG_ModernDrawDuelScoreboard();
	if ( cgs.gametype >= GT_TDM )
		return CG_ModernDrawScoretable();

		return CG_ModernDrawFFAScoreboard();
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

/*
=================
CG_DrawFollow
=================
*/
static qboolean CG_DrawFollow( void ) {
	float		x;
	vec4_t		color;
	const char	*name;

	if ( !(cg.snap->ps.pm_flags & PMF_FOLLOW) ) {
		return qfalse;
	}
	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;


	trap_R_DrawTextNorm( "following", (float)(320 - 9 * 8) * NORM_HSCALE, 24.0f * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );

	name = cgs.clientinfo[ cg.snap->ps.clientNum ].name;

	x = 0.5 * ( 640 - GIANT_WIDTH * CG_DrawStrlen( name ) );

	trap_R_DrawTextNorm( name, (float)x * NORM_HSCALE, 40.0f * NORM_VSCALE, FONT_UI, (float)GIANT_HEIGHT * NORM_VSCALE, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW | TEXT_FORCECOLOR );

	return qtrue;
}



/*
=================
CG_DrawAmmoWarning
=================
*/
static void CG_DrawAmmoWarning( void ) {
	const char	*s;
	int			w;

	if ( cg_drawAmmoWarning.integer == 0 ) {
		return;
	}

	if ( !cg.lowAmmoWarning ) {
		return;
	}

	if ( cg.lowAmmoWarning == 2 ) {
		s = "OUT OF AMMO";
	} else {
		s = "LOW AMMO WARNING";
	}
	w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
	trap_R_DrawTextNorm( s, (float)(320 - w / 2) * NORM_HSCALE, 64.0f * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
}


/*
=================
CG_DrawWarmup
=================
*/
static void CG_DrawWarmup( void ) {
	int			w;
	int			sec;
	int			i;
	int			cw;
	clientInfo_t	*ci1, *ci2;
	const char	*s;

	sec = cg.warmup;
	if ( !sec ) {
		return;
	}

	if ( sec < 0 ) {
		s = "Waiting for players";
		w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		trap_R_DrawTextNorm( s, (float)(320 - w / 2) * NORM_HSCALE, 24.0f * NORM_VSCALE, FONT_DISPLAY, (float)BIGCHAR_HEIGHT * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		cg.warmupCount = 0;
		return;
	}

	if (cgs.gametype == GT_DUEL) {
		// find the two active players
		ci1 = NULL;
		ci2 = NULL;
		for ( i = 0 ; i < cgs.maxclients ; i++ ) {
			if ( cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == TEAM_FREE ) {
				if ( !ci1 ) {
					ci1 = &cgs.clientinfo[i];
				} else {
					ci2 = &cgs.clientinfo[i];
				}
			}
		}

		if ( ci1 && ci2 ) {
			s = va( "%s vs %s", ci1->name, ci2->name );
			w = CG_DrawStrlen( s );
			if ( w > 640 / GIANT_WIDTH ) {
				cw = 640 / w;
			} else {
				cw = GIANT_WIDTH;
			}
			trap_R_DrawTextNorm( s, (float)(320 - w * cw/2) * NORM_HSCALE, 20.0f * NORM_VSCALE, FONT_UI, (float)((int)(cw * 1.5f)) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
		}
	} else {
		if (cgs.gametype >= 0 && cgs.gametype < GT_MAX_GAME_TYPE) {
			s = bg_gametypelist[cgs.gametype].name;
		} else {
			s = "";
		}
		w = CG_DrawStrlen( s );
		if ( w > 640 / GIANT_WIDTH ) {
			cw = 640 / w;
		} else {
			cw = GIANT_WIDTH;
		}
		trap_R_DrawTextNorm( s, (float)(320 - w * cw/2) * NORM_HSCALE, 25.0f * NORM_VSCALE, FONT_UI, (float)((int)(cw * 1.1f)) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
	}

	sec = ( sec - cg.time ) / 1000;
	if ( sec < 0 ) {
		cg.warmup = 0;
		sec = 0;
	}
	s = va( "Starts in: %i", sec + 1 );
	if ( sec != cg.warmupCount ) {
		cg.warmupCount = sec;
		switch ( sec ) {
		case 0:
			trap_S_StartLocalSound( cgs.media.count1Sound, CHAN_ANNOUNCER );
			break;
		case 1:
			trap_S_StartLocalSound( cgs.media.count2Sound, CHAN_ANNOUNCER );
			break;
		case 2:
			trap_S_StartLocalSound( cgs.media.count3Sound, CHAN_ANNOUNCER );
			break;
		default:
			break;
		}
	}

	switch ( cg.warmupCount ) {
	case 0:
		cw = 28;
		break;
	case 1:
		cw = 24;
		break;
	case 2:
		cw = 20;
		break;
	default:
		cw = 16;
		break;
	}

	w = CG_DrawStrlen( s );
	trap_R_DrawTextNorm( s, (float)(320 - w * cw/2) * NORM_HSCALE, 70.0f * NORM_VSCALE, FONT_UI, (float)((int)(cw * 1.5)) * NORM_VSCALE, colorWhite, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
}

//==================================================================================
#if FEAT_TA_UI
/* 
=================
CG_DrawTimedMenus
=================
*/
void CG_DrawTimedMenus( void ) {
	if (cg.voiceTime) {
		int t = cg.time - cg.voiceTime;
		if ( t > 2500 ) {
			Menus_CloseByName("voiceMenu");
			trap_Cvar_Set("cl_conXOffset", "0");
			cg.voiceTime = 0;
		}
	}
}
#endif
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
#if FEAT_TA_UI
#if FEAT_TA_UI
	if (cgs.orderPending && cg.time > cgs.orderTime) {
		CG_CheckOrderPending();
	}
#endif
#endif
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

/*
	if (cg.cameraMode) {
		return;
	}
*/
#if FEAT_WIRED_UI
	// Wired UI: always push game state to client
	CG_WiredHudPushState();

	// cg_wiredUI 1: client renders HUD — skip all cgame HUD drawing
	if ( cg_wiredUI.integer ) {
		CG_ScanForCrosshairEntity();  // updates crosshairClientNum for state bridge
		// crosshair + crosshair names drawn by Wired UI elements
		// (cl_wired_hud_elem_crosshair.c, cl_wired_hud_elem_target_name.c)
		// route center print through Wired UI message queue instead of drawing directly
		if ( cg.centerPrintTime && cg.centerPrint[0] ) {
			trap_WiredUI_PushEvent( WIRED_EVENT_CENTERPRINT, cg.centerPrint );
			cg.centerPrintTime = 0;  // consumed — don't push again
		}
		return;
	}
#endif
	// default HUD
	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		CG_DrawSpectator();

		if(stereoFrame == STEREO_CENTER)
			CG_DrawCrosshair();

		CG_DrawCrosshairNames();
	} else {
		// don't draw any status if dead or the scoreboard is being explicitly shown
		if ( !cg.showScores && cg.snap->ps.stats[STAT_HEALTH] > 0 ) {

#if FEAT_TA_UI
#if FEAT_TA_UI
			if ( cg_drawStatus.integer ) {
				Menu_PaintAll();
				CG_DrawTimedMenus();
			}
#endif
#else
			CG_DrawStatusBar();
#endif

			CG_DrawAmmoWarning();

			if(stereoFrame == STEREO_CENTER)
				CG_DrawCrosshair();
			CG_DrawCrosshairNames();
			CG_DrawWeaponSelect();

#ifndef MISSIONPACK
			CG_DrawHoldableItem();
#endif
			CG_DrawReward();
		}
	}

	if ( cgs.gametype >= GT_TDM ) {
#ifndef MISSIONPACK
		CG_DrawTeamInfo();
#endif
	}

	// classic HUD: draw vote, lagometer, upper/lower corners, etc.
	CG_DrawVote();
	CG_DrawTeamVote();

	CG_DrawLagometer();

#if FEAT_TA_UI
#if FEAT_TA_UI
	if (!cg_paused.integer) {
		CG_DrawUpperRight(stereoFrame);
	}
#endif
#else
	CG_DrawUpperRight(stereoFrame);
#endif

#ifndef MISSIONPACK
	CG_DrawLowerRight();
	CG_DrawLowerLeft();
#endif

	if ( !CG_DrawFollow() ) {
		CG_DrawWarmup();
	}

	// don't draw center string if scoreboard is up
	cg.scoreBoardShowing = CG_DrawScoreboard();
	if ( !cg.scoreBoardShowing) {
		CG_DrawCenterString();
	}

#if FEAT_STATS_WINDOW
	/* floating window overlays (stats, votes, bot orders) — drawn last, on top of everything */
	CG_windowDraw();
#endif
}


/*
=====================
CG_DrawActive

Perform all drawing needed to completely fill the screen
=====================
*/
void CG_DrawActive( stereoFrame_t stereoView ) {
	// optionally draw the info screen instead
	if ( !cg.snap ) {
		CG_DrawInformation();
		return;
	}

#if FEAT_WIRED_UI
	if ( !cg_wiredUI.integer )
#endif
	{
		if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR &&
			( cg.snap->ps.pm_flags & PMF_SCOREBOARD ) ) {
			CG_DrawDuelScoreboard();
			return;
		}
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
