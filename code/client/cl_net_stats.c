/* cl_net_stats.c — statistical network overlay (ported from fX3)
 *
 * Three on-screen overlays:
 *   cl_drawPing    — ping mean / max-spike / avg-spike / std-dev over 2 windows
 *   cl_drawSnaps   — SPS mean / drop / delayed / extrapolated over 2 windows
 *   cl_drawPackets — outbound PPS mean / drop / std-dev over 2 windows
 *
 * Each overlay has 6 cvars:  cl_draw<Name>{,FontSize,PosX,PosY,FirstInterval,SecondInterval}
 * Window lengths are in seconds; stats reset every window and display the last computed values.
 */

#include <math.h>
#include <stdlib.h>

#include "client.h"

/* Incremented in cl_input.c every time a packet is sent.
 * SCR_DrawPackets uses it as a guard against a zero-division on first frame. */
int cl_sent;

/* ---- ping cvars ---------------------------------------------------------- */
static cvar_t *cl_drawPing;
static cvar_t *cl_drawPingFontSize;
static cvar_t *cl_drawPingPosX;
static cvar_t *cl_drawPingPosY;
static cvar_t *cl_drawPingFirstInterval;
static cvar_t *cl_drawPingSecondInterval;

/* ---- snap cvars ---------------------------------------------------------- */
static cvar_t *cl_drawSnaps;
static cvar_t *cl_drawSnapsFontSize;
static cvar_t *cl_drawSnapsPosX;
static cvar_t *cl_drawSnapsPosY;
static cvar_t *cl_drawSnapsFirstInterval;
static cvar_t *cl_drawSnapsSecondInterval;

/* ---- packet cvars -------------------------------------------------------- */
static cvar_t *cl_drawPackets;
static cvar_t *cl_drawPacketsFontSize;
static cvar_t *cl_drawPacketsPosX;
static cvar_t *cl_drawPacketsPosY;
static cvar_t *cl_drawPacketsFirstInterval;
static cvar_t *cl_drawPacketsSecondInterval;

void SCR_NetStatsInit( void ) {
	cl_drawPing = Cvar_Get( "cl_drawPing", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_drawPing, "Draw per-interval ping statistics overlay (0=off, 1=on)." );
	cl_drawPingFontSize = Cvar_Get( "cl_drawPingFontSize", "7", CVAR_ARCHIVE );
	cl_drawPingPosX = Cvar_Get( "cl_drawPingPosX", "0", CVAR_ARCHIVE );
	cl_drawPingPosY = Cvar_Get( "cl_drawPingPosY", "14", CVAR_ARCHIVE );
	cl_drawPingFirstInterval = Cvar_Get( "cl_drawPingFirstInterval", "2", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_drawPingFirstInterval, "First collection window length in seconds for ping overlay." );
	cl_drawPingSecondInterval = Cvar_Get( "cl_drawPingSecondInterval", "10", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_drawPingSecondInterval, "Second collection window length in seconds for ping overlay." );

	cl_drawSnaps = Cvar_Get( "cl_drawSnaps", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_drawSnaps, "Draw per-interval snapshot-rate statistics overlay (0=off, 1=on)." );
	cl_drawSnapsFontSize = Cvar_Get( "cl_drawSnapsFontSize", "7", CVAR_ARCHIVE );
	cl_drawSnapsPosX = Cvar_Get( "cl_drawSnapsPosX", "0", CVAR_ARCHIVE );
	cl_drawSnapsPosY = Cvar_Get( "cl_drawSnapsPosY", "7", CVAR_ARCHIVE );
	cl_drawSnapsFirstInterval = Cvar_Get( "cl_drawSnapsFirstInterval", "2", CVAR_ARCHIVE );
	cl_drawSnapsSecondInterval = Cvar_Get( "cl_drawSnapsSecondInterval", "10", CVAR_ARCHIVE );

	cl_drawPackets = Cvar_Get( "cl_drawPackets", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_drawPackets, "Draw per-interval outbound packet-rate statistics overlay (0=off, 1=on)." );
	cl_drawPacketsFontSize = Cvar_Get( "cl_drawPacketsFontSize", "7", CVAR_ARCHIVE );
	cl_drawPacketsPosX = Cvar_Get( "cl_drawPacketsPosX", "16", CVAR_ARCHIVE );
	cl_drawPacketsPosY = Cvar_Get( "cl_drawPacketsPosY", "2", CVAR_ARCHIVE );
	cl_drawPacketsFirstInterval = Cvar_Get( "cl_drawPacketsFirstInterval", "2", CVAR_ARCHIVE );
	cl_drawPacketsSecondInterval = Cvar_Get( "cl_drawPacketsSecondInterval", "10", CVAR_ARCHIVE );
}

/*
================
SCR_DrawPing

Draws 6 lines: current ping, interval header, mean, max-spike, avg-spike, std-dev.
Two side-by-side columns — one per window length.
================
*/
void SCR_DrawPing( void ) {
	if ( !cl_drawPing || !cl_drawPing->integer ) return;
	char string1[64], string2[64], string3[64], string4[64], string5[64], string6[64];
	static int  *pings, *pings2, timeRec, timeRec2, count, count2,
	             acount, acount2, avgPing, avgPing2,
	             flux, flux2, aflux, aflux2, std, std2,
	             alloc_ok, alloc_ok2;
	static unsigned long long int sum, sum2, asum, asum2, stsum, stsum2;
	int timeNew, currPing, interval, interval2, fontsize, posx, posy, posxx, posyy, i;

	timeNew  = Sys_Milliseconds();
	currPing = cl.snap.ping;
	interval  = cl_drawPingFirstInterval->integer;
	interval2 = cl_drawPingSecondInterval->integer;
	fontsize  = cl_drawPingFontSize->integer;
	posx      = cl_drawPingPosX->integer;
	posy      = cl_drawPingPosY->integer;

	if ( interval  <= 0 ) interval  = 1;
	if ( interval2 <= 0 ) interval2 = 1;

	if ( !timeRec )
		timeRec = timeRec2 = timeNew;

	/* --- window 1 --- */
	if ( ( timeNew - timeRec ) / 1000 < interval ) {
		if ( !( count % 128 ) ) {
			int *tmp = realloc( pings, ( count + 128 ) * sizeof( int ) );
			if ( tmp ) { alloc_ok = 1; pings = tmp; }
			else        { alloc_ok = 0; count = 0;  }
		}
		if ( alloc_ok )
			pings[ count++ ] = currPing;
	} else if ( count ) {
		for ( i = 0; i < count; i++ ) sum += pings[ i ];
		avgPing = (int)( sum / count );
		flux = 0;
		for ( i = 0; i < count; i++ ) {
			stsum += (unsigned long long int)pow( (double)( pings[i] - avgPing ), 2 );
			if ( pings[i] > avgPing ) {
				acount++;
				asum += (unsigned long long int)( pings[i] - avgPing );
				if ( pings[i] > flux + avgPing ) flux = pings[i] - avgPing;
			}
		}
		std   = (int)sqrt( (double)( stsum / count ) );
		aflux = acount ? (int)( asum / acount ) : 0;
		timeRec = timeNew;
		sum = count = asum = acount = stsum = 0;
		free( pings );
		pings = NULL;
	}

	/* --- window 2 (identical logic) --- */
	if ( ( timeNew - timeRec2 ) / 1000 < interval2 ) {
		if ( !( count2 % 128 ) ) {
			int *tmp = realloc( pings2, ( count2 + 128 ) * sizeof( int ) );
			if ( tmp ) { alloc_ok2 = 1; pings2 = tmp; }
			else        { alloc_ok2 = 0; count2 = 0;  }
		}
		if ( alloc_ok2 )
			pings2[ count2++ ] = currPing;
	} else if ( count2 ) {
		for ( i = 0; i < count2; i++ ) sum2 += pings2[ i ];
		avgPing2 = (int)( sum2 / count2 );
		flux2 = 0;
		for ( i = 0; i < count2; i++ ) {
			stsum2 += (unsigned long long int)pow( (double)( pings2[i] - avgPing2 ), 2 );
			if ( pings2[i] > avgPing2 ) {
				acount2++;
				asum2 += (unsigned long long int)( pings2[i] - avgPing2 );
				if ( pings2[i] > flux2 + avgPing2 ) flux2 = pings2[i] - avgPing2;
			}
		}
		std2   = (int)sqrt( (double)( stsum2 / count2 ) );
		aflux2 = acount2 ? (int)( asum2 / acount2 ) : 0;
		timeRec2 = timeNew;
		sum2 = count2 = asum2 = acount2 = stsum2 = 0;
		free( pings2 );
		pings2 = NULL;
	}

	Com_sprintf( string1, sizeof( string1 ), "%3ims", currPing );
	Com_sprintf( string2, sizeof( string2 ), "Interval   %3is  %3is", interval, interval2 );
	Com_sprintf( string3, sizeof( string3 ), "Mean      %3ims %3ims", avgPing,  avgPing2 );
	Com_sprintf( string4, sizeof( string4 ), "Max Spike %3ims %3ims", flux,     flux2    );
	Com_sprintf( string5, sizeof( string5 ), "Avg Spike %3ims %3ims", aflux,    aflux2   );
	Com_sprintf( string6, sizeof( string6 ), "Std Dev   %3ims %3ims", std,      std2     );

	posxx = ( posx >= 0 && posx < 21 ) ? posx * 30 : 0;
	posyy = ( posy >= 0 && posy < 24 ) ? posy * 20 : 0;

	SCR_DrawStringExt( posxx, posyy,                  fontsize, string1, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*1, fontsize, string2, g_color_table[4], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*2, fontsize, string3, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*3, fontsize, string4, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*4, fontsize, string5, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*5, fontsize, string6, g_color_table[7], qtrue, qtrue );
}

/*
================
SCR_DrawSnaps

Draws 8 lines: current SPS, interval, mean, max-drop, avg-drop, std-dev,
rate-delayed per second, extrapolated per second.
================
*/
typedef struct {
	int      sps;
	int      fps;
	int      state;
	qboolean extrapolated;
} s_snaps_t;

void SCR_DrawSnaps( void ) {
	if ( !cl_drawSnaps || !cl_drawSnaps->integer ) return;
	char string1[64], string2[64], string3[64], string4[64],
	     string5[64], string6[64], string7[64], string8[64];
	static int   sps, omsg, otime, timeRec, timeRec2,
	             count, count2, avgSps, avgSps2,
	             sdrop, sdrop2, asdrop, asdrop2, dcount, dcount2,
	             stdSps, stdSps2, delayed, delayed2,
	             alloc_ok, alloc_ok2, extrap, extrap2, fps, otimeFps,
	             avgFps, avgFps2;
	static float coef, coef2, delayedF, delayedF2, extrapF, extrapF2;
	static unsigned long long int sumSps, sumSps2, snsum, snsum2, stsum, stsum2, sumFps, sumFps2;
	static s_snaps_t *snapses, *snapses2;
	int cmsg, ctime, interval, interval2, fontsize, posx, posy, posxx, posyy, i;

	cmsg  = cl.snap.messageNum;
	ctime = Sys_Milliseconds();

	if ( !omsg ) { omsg = cmsg; otime = ctime; otimeFps = ctime; }

	if ( ctime - otimeFps )
		{ fps = 1000 / ( ctime - otimeFps ); otimeFps = ctime; }

	if ( ctime - otime && cmsg - omsg )
		{ sps = 1000 * ( cmsg - omsg ) / ( ctime - otime ); omsg = cmsg; otime = ctime; }

	interval  = cl_drawSnapsFirstInterval->integer;
	interval2 = cl_drawSnapsSecondInterval->integer;
	fontsize  = cl_drawSnapsFontSize->integer;
	posx      = cl_drawSnapsPosX->integer;
	posy      = cl_drawSnapsPosY->integer;

	if ( interval  <= 0 ) interval  = 1;
	if ( interval2 <= 0 ) interval2 = 1;

	if ( !timeRec ) timeRec = timeRec2 = ctime;

	/* --- window 1 --- */
	if ( ( ctime - timeRec ) / 1000 < interval ) {
		if ( !( count % 128 ) ) {
			s_snaps_t *tmp = realloc( snapses, ( count + 128 ) * sizeof( s_snaps_t ) );
			if ( tmp ) { alloc_ok = 1; snapses = tmp; }
			else        { alloc_ok = 0; count = 0;    }
		}
		if ( alloc_ok ) {
			snapses[ count ].sps          = sps;
			snapses[ count ].state        = cl.snap.snapFlags;
			snapses[ count ].extrapolated = cl.extrapolatedSnapshot;
			snapses[ count ].fps          = fps;
			count++;
		}
	} else if ( count ) {
		for ( i = 0; i < count; i++ ) { sumSps += snapses[i].sps; sumFps += snapses[i].fps; }
		avgSps = (int)( sumSps / count );
		avgFps = (int)( sumFps / count );
		coef = avgFps ? ( (float)avgSps / (float)avgFps / (float)interval ) : 0;
		sdrop = delayed = extrap = 0;
		for ( i = 0; i < count; i++ ) {
			stsum += (unsigned long long int)pow( (double)( snapses[i].sps - avgSps ), 2 );
			if ( snapses[i].sps < avgSps ) {
				dcount++;
				snsum += (unsigned long long int)( avgSps - snapses[i].sps );
				if ( snapses[i].sps < avgSps - sdrop ) sdrop = avgSps - snapses[i].sps;
			}
			if ( snapses[i].state & SNAPFLAG_RATE_DELAYED ) delayed++;
			if ( snapses[i].extrapolated == qtrue )         extrap++;
		}
		delayedF = (float)delayed * coef;
		extrapF  = (float)extrap  * coef;
		stdSps   = (int)sqrt( (double)( stsum / count ) );
		asdrop   = dcount ? (int)( snsum / dcount ) : 0;
		timeRec  = ctime;
		sumSps = count = stsum = snsum = dcount = sumFps = 0;
		free( snapses );
		snapses = NULL;
	}

	/* --- window 2 --- */
	if ( ( ctime - timeRec2 ) / 1000 < interval2 ) {
		if ( !( count2 % 128 ) ) {
			s_snaps_t *tmp = realloc( snapses2, ( count2 + 128 ) * sizeof( s_snaps_t ) );
			if ( tmp ) { alloc_ok2 = 1; snapses2 = tmp; }
			else        { alloc_ok2 = 0; count2 = 0;    }
		}
		if ( alloc_ok2 ) {
			snapses2[ count2 ].sps          = sps;
			snapses2[ count2 ].state        = cl.snap.snapFlags;
			snapses2[ count2 ].extrapolated = cl.extrapolatedSnapshot;
			snapses2[ count2 ].fps          = fps;
			count2++;
		}
	} else if ( count2 ) {
		for ( i = 0; i < count2; i++ ) { sumSps2 += snapses2[i].sps; sumFps2 += snapses2[i].fps; }
		avgSps2 = (int)( sumSps2 / count2 );
		avgFps2 = (int)( sumFps2 / count2 );
		coef2   = avgFps2 ? ( (float)avgSps2 / (float)avgFps2 / (float)interval2 ) : 0;
		sdrop2 = delayed2 = extrap2 = 0;
		for ( i = 0; i < count2; i++ ) {
			stsum2 += (unsigned long long int)pow( (double)( snapses2[i].sps - avgSps2 ), 2 );
			if ( snapses2[i].sps < avgSps2 ) {
				dcount2++;
				snsum2 += (unsigned long long int)( avgSps2 - snapses2[i].sps );
				if ( snapses2[i].sps < avgSps2 - sdrop2 ) sdrop2 = avgSps2 - snapses2[i].sps;
			}
			if ( snapses2[i].state & SNAPFLAG_RATE_DELAYED ) delayed2++;
			if ( snapses2[i].extrapolated == qtrue )          extrap2++;
		}
		delayedF2 = (float)delayed2 * coef2;
		extrapF2  = (float)extrap2  * coef2;
		stdSps2   = (int)sqrt( (double)( stsum2 / count2 ) );
		asdrop2   = dcount2 ? (int)( snsum2 / dcount2 ) : 0;
		timeRec2  = ctime;
		sumSps2 = count2 = stsum2 = snsum2 = dcount2 = sumFps2 = 0;
		free( snapses2 );
		snapses2 = NULL;
	}

	Com_sprintf( string1, sizeof( string1 ), "%3iSPS",                  sps );
	Com_sprintf( string2, sizeof( string2 ), "Interval   %3is   %3is",  interval,  interval2 );
	Com_sprintf( string3, sizeof( string3 ), "Mean     %3iSPS %3iSPS",  avgSps,    avgSps2   );
	Com_sprintf( string4, sizeof( string4 ), "Max Drop %3iSPS %3iSPS",  sdrop,     sdrop2    );
	Com_sprintf( string5, sizeof( string5 ), "Avg Drop %3iSPS %3iSPS",  asdrop,    asdrop2   );
	Com_sprintf( string6, sizeof( string6 ), "Std Dev  %3iSPS %3iSPS",  stdSps,    stdSps2   );
	Com_sprintf( string7, sizeof( string7 ), "Delayed  %.1fSPS %.1fSPS",delayedF,  delayedF2 );
	Com_sprintf( string8, sizeof( string8 ), "Extrap.  %.1fSPS %.1fSPS",extrapF,   extrapF2  );

	posxx = ( posx >= 0 && posx < 21 ) ? posx * 30 : 0;
	posyy = ( posy >= 0 && posy < 24 ) ? posy * 20 : 0;

	SCR_DrawStringExt( posxx, posyy,                  fontsize, string1, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*1, fontsize, string2, g_color_table[3], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*2, fontsize, string3, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*3, fontsize, string4, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*4, fontsize, string5, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*5, fontsize, string6, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*6, fontsize, string7, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*7, fontsize, string8, g_color_table[7], qtrue, qtrue );
}

/*
================
SCR_DrawPackets

Draws 6 lines: current PPS, interval, mean, max-drop, avg-drop, std-dev.
cl_sent (incremented by cl_input.c) guards against first-frame zero-division.
================
*/
void SCR_DrawPackets( void ) {
	if ( !cl_drawPackets || !cl_drawPackets->integer ) return;
	char string1[64], string2[64], string3[64], string4[64], string5[64], string6[64];
	static int  *packs, *packs2, pps, count, count2, timeRec, timeRec2, lastRecorded,
	             avgPps, avgPps2, mdrop, mdrop2, adrop, adrop2, dcount, dcount2,
	             std, std2, alloc_ok, alloc_ok2;
	static unsigned long long int sum, sum2, stsum, stsum2, dsum, dsum2;
	int posx, posy, posxx, posyy, fontsize, interval, interval2, newtime, i;

	interval  = cl_drawPacketsFirstInterval->integer;
	interval2 = cl_drawPacketsSecondInterval->integer;
	fontsize  = cl_drawPacketsFontSize->integer;
	posx      = cl_drawPacketsPosX->integer;
	posy      = cl_drawPacketsPosY->integer;

	newtime = Sys_Milliseconds();

	if ( !timeRec ) timeRec = timeRec2 = lastRecorded = newtime;
	if ( interval  <= 0 ) interval  = 1;
	if ( interval2 <= 0 ) interval2 = 1;

	if ( cl_sent && ( newtime - lastRecorded ) ) {
		pps = 1000 / ( newtime - lastRecorded );
		lastRecorded = newtime;
	}

	/* --- window 1 --- */
	if ( ( newtime - timeRec ) / 1000 < interval ) {
		if ( !( count % 128 ) ) {
			int *tmp = realloc( packs, ( count + 128 ) * sizeof( int ) );
			if ( tmp ) { alloc_ok = 1; packs = tmp; }
			else        { alloc_ok = 0; count = 0;  }
		}
		if ( alloc_ok )
			packs[ count++ ] = pps;
	} else if ( count ) {
		for ( i = 0; i < count; i++ ) sum += packs[ i ];
		avgPps = (int)( sum / count );
		mdrop  = 0;
		for ( i = 0; i < count; i++ ) {
			stsum += (unsigned long long int)pow( (double)( packs[i] - avgPps ), 2 );
			if ( packs[i] < avgPps ) {
				dcount++;
				dsum += (unsigned long long int)( avgPps - packs[i] );
				if ( packs[i] < avgPps - mdrop ) mdrop = avgPps - packs[i];
			}
		}
		std   = (int)sqrt( (double)( stsum / count ) );
		adrop = dcount ? (int)( dsum / dcount ) : 0;
		timeRec = newtime;
		sum = count = stsum = dsum = dcount = 0;
		free( packs );
		packs = NULL;
	}

	/* --- window 2 --- */
	if ( ( newtime - timeRec2 ) / 1000 < interval2 ) {
		if ( !( count2 % 128 ) ) {
			int *tmp = realloc( packs2, ( count2 + 128 ) * sizeof( int ) );
			if ( tmp ) { alloc_ok2 = 1; packs2 = tmp; }
			else        { alloc_ok2 = 0; count2 = 0;  }
		}
		if ( alloc_ok2 )
			packs2[ count2++ ] = pps;
	} else if ( count2 ) {
		for ( i = 0; i < count2; i++ ) sum2 += packs2[ i ];
		avgPps2 = (int)( sum2 / count2 );
		mdrop2  = 0;
		for ( i = 0; i < count2; i++ ) {
			stsum2 += (unsigned long long int)pow( (double)( packs2[i] - avgPps2 ), 2 );
			if ( packs2[i] < avgPps2 ) {
				dcount2++;
				dsum2 += (unsigned long long int)( avgPps2 - packs2[i] );
				if ( packs2[i] < avgPps2 - mdrop2 ) mdrop2 = avgPps2 - packs2[i];
			}
		}
		std2   = (int)sqrt( (double)( stsum2 / count2 ) );
		adrop2 = dcount2 ? (int)( dsum2 / dcount2 ) : 0;
		timeRec2 = newtime;
		sum2 = count2 = stsum2 = dsum2 = dcount2 = 0;
		free( packs2 );
		packs2 = NULL;
	}

	Com_sprintf( string1, sizeof( string1 ), "%3iPPS",                  pps     );
	Com_sprintf( string2, sizeof( string2 ), "Interval   %3is   %3is",  interval,  interval2 );
	Com_sprintf( string3, sizeof( string3 ), "Mean     %3iPPS %3iPPS",  avgPps,    avgPps2   );
	Com_sprintf( string4, sizeof( string4 ), "Max Drop %3iPPS %3iPPS",  mdrop,     mdrop2    );
	Com_sprintf( string5, sizeof( string5 ), "Avg Drop %3iPPS %3iPPS",  adrop,     adrop2    );
	Com_sprintf( string6, sizeof( string6 ), "Std Dev  %3iPPS %3iPPS",  std,       std2      );

	posxx = ( posx >= 0 && posx < 21 ) ? posx * 30 : 0;
	posyy = ( posy >= 0 && posy < 24 ) ? posy * 20 : 0;

	SCR_DrawStringExt( posxx, posyy,                  fontsize, string1, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*1, fontsize, string2, g_color_table[2], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*2, fontsize, string3, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*3, fontsize, string4, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*4, fontsize, string5, g_color_table[7], qtrue, qtrue );
	SCR_DrawStringExt( posxx, posyy + (fontsize+2)*5, fontsize, string6, g_color_table[7], qtrue, qtrue );
}
