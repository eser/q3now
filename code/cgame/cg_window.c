/*
===========================================================================
cg_window.c — general-purpose floating panel system

Provides overlay windows with fade-in/out effects and auto-close timers.
Each window is identified by a unique ID (WID_*). Multiple windows can
be active simultaneously.

Consumers:
  WID_STATS    — per-weapon accuracy overlay (+stats/-stats)
  WID_VOTE     — vote notification (auto-close 5s)
  WID_BOTORDER — bot order confirmation (auto-close 2s)

Architecture:
  ┌──────────────────────────────────────────────┐
  │  CG_windowHandler (static, up to 6 windows)  │
  │                                              │
  │  CG_windowAlloc(id) → find/create window     │
  │  CG_windowFree(id)  → start fadeout          │
  │  CG_windowDraw()    → render all active       │
  │                       (called from CG_Draw2D) │
  │                                              │
  │  State machine per window:                   │
  │    OFF → FADEIN → ON → FADEOUT → OFF         │
  │           200ms        200ms                 │
  │                  (autoCloseTime)             │
  └──────────────────────────────────────────────┘

Inspired by Nemesis cg_window.c, simplified for q3now.
===========================================================================
*/

#include "cg_local.h"

#if FEAT_STATS_WINDOW

#define FADE_TIME       200     /* ms for fade in/out */
#define WIN_PADDING     6       /* pixels of padding inside window */
#define WIN_LINE_HEIGHT 12      /* pixels per text line */
#define WIN_BG_ALPHA    0.75f

cg_windowHandler_t cg_windowHandler;

/*
=================
CG_windowInit
=================
*/
void CG_windowInit( void )
{
	memset( &cg_windowHandler, 0, sizeof( cg_windowHandler ) );
}


/*
=================
CG_windowAlloc

Find an existing window with the given ID, or allocate a free slot.
If no free slot, overwrite the oldest window.
=================
*/
cg_window_t *CG_windowAlloc( int id )
{
	int i;
	int oldest;
	int oldestTime;
	cg_window_t *w;

	/* reuse existing window with same ID */
	for ( i = 0; i < MAX_WINDOWS; i++ ) {
		if ( cg_windowHandler.windows[i].id == id && cg_windowHandler.windows[i].state != WSTATE_OFF ) {
			w = &cg_windowHandler.windows[i];
			w->lineCount = 0;
			w->state = WSTATE_FADEIN;
			w->stateStartTime = cg.time;
			w->alpha = 0.0f;
			return w;
		}
	}

	/* find a free slot */
	for ( i = 0; i < MAX_WINDOWS; i++ ) {
		if ( cg_windowHandler.windows[i].state == WSTATE_OFF ) {
			w = &cg_windowHandler.windows[i];
			memset( w, 0, sizeof( *w ) );
			w->id = id;
			w->state = WSTATE_FADEIN;
			w->stateStartTime = cg.time;
			return w;
		}
	}

	/* no free slot — overwrite oldest */
	oldest = 0;
	oldestTime = cg_windowHandler.windows[0].stateStartTime;
	for ( i = 1; i < MAX_WINDOWS; i++ ) {
		if ( cg_windowHandler.windows[i].stateStartTime < oldestTime ) {
			oldest = i;
			oldestTime = cg_windowHandler.windows[i].stateStartTime;
		}
	}
	w = &cg_windowHandler.windows[oldest];
	memset( w, 0, sizeof( *w ) );
	w->id = id;
	w->state = WSTATE_FADEIN;
	w->stateStartTime = cg.time;
	return w;
}


/*
=================
CG_windowFree

Start fadeout for the window with the given ID.
=================
*/
void CG_windowFree( int id )
{
	int i;

	for ( i = 0; i < MAX_WINDOWS; i++ ) {
		if ( cg_windowHandler.windows[i].id == id && cg_windowHandler.windows[i].state != WSTATE_OFF ) {
			cg_windowHandler.windows[i].state = WSTATE_FADEOUT;
			cg_windowHandler.windows[i].stateStartTime = cg.time;
		}
	}
}


/*
=================
CG_windowDraw

Render all active windows. Called from CG_Draw2D().
Handles state transitions: FADEIN→ON→FADEOUT→OFF.
=================
*/
void CG_windowDraw( void )
{
	int i, line;
	cg_window_t *w;
	float elapsed;
	float frac;
	vec4_t bgColor;
	vec4_t lineColor;

	for ( i = 0; i < MAX_WINDOWS; i++ ) {
		w = &cg_windowHandler.windows[i];
		if ( w->state == WSTATE_OFF )
			continue;

		elapsed = (float)( cg.time - w->stateStartTime );

		/* update state machine */
		switch ( w->state ) {
		case WSTATE_FADEIN:
			frac = elapsed / FADE_TIME;
			if ( frac >= 1.0f ) {
				frac = 1.0f;
				w->state = WSTATE_ON;
				w->stateStartTime = cg.time;
			}
			w->alpha = frac;
			break;

		case WSTATE_ON:
			w->alpha = 1.0f;
			/* auto-close timer */
			if ( w->autoCloseTime > 0 && elapsed >= (float)w->autoCloseTime ) {
				w->state = WSTATE_FADEOUT;
				w->stateStartTime = cg.time;
			}
			break;

		case WSTATE_FADEOUT:
			frac = 1.0f - ( elapsed / FADE_TIME );
			if ( frac <= 0.0f ) {
				w->state = WSTATE_OFF;
				continue;
			}
			w->alpha = frac;
			break;

		default:
			continue;
		}

		/* auto-size width from line content */
		if ( w->w <= 0 ) {
			w->w = 280;
		}
		if ( w->h <= 0 ) {
			w->h = w->lineCount * WIN_LINE_HEIGHT + WIN_PADDING * 2;
		}

		/* draw background */
		Vector4Set( bgColor, 0.0f, 0.0f, 0.0f, WIN_BG_ALPHA * w->alpha );
		CG_FillRect( w->x, w->y, w->w, w->h, bgColor );

		/* draw border */
		Vector4Set( bgColor, 0.5f, 0.5f, 0.5f, 0.3f * w->alpha );
		CG_DrawRect( w->x, w->y, w->w, w->h, 1, bgColor );

		/* draw lines */
		for ( line = 0; line < w->lineCount && line < MAX_WINDOW_LINES; line++ ) {
			Vector4Copy( w->lineColor[line], lineColor );
			lineColor[3] *= w->alpha;
			trap_R_DrawText( w->lineText[line],
				(float)( w->x + WIN_PADDING ),
				(float)( w->y + WIN_PADDING + line * WIN_LINE_HEIGHT ),
				FONT_UI, (float)SMALLCHAR_HEIGHT, lineColor, TEXT_ALIGN_LEFT, 0 );
		}
	}
}


/*
=================
CG_windowAddLine

Append a text line to a window. Returns qfalse if window is full.
=================
*/
static qboolean CG_windowAddLine( cg_window_t *w, const vec4_t color, const char *text )
{
	if ( !w || w->lineCount >= MAX_WINDOW_LINES )
		return qfalse;

	Q_strncpyz( w->lineText[w->lineCount], text, MAX_WINDOW_LINE_LEN );
	Vector4Copy( color, w->lineColor[w->lineCount] );
	w->lineCount++;

	/* recalc height */
	w->h = w->lineCount * WIN_LINE_HEIGHT + WIN_PADDING * 2;
	return qtrue;
}


/* ──────────────────────────────────────────────────────────────
   Consumer 1: Stats overlay (+stats / -stats)
   ────────────────────────────────────────────────────────────── */

void CG_statsWindow( void )
{
	cg_window_t *w;
	int att;
	int acc;
	qboolean hasData;
	char buf[MAX_WINDOW_LINE_LEN];
	vec4_t colorGreen  = { 0.2f, 1.0f, 0.2f, 1.0f };
	vec4_t colorYellow = { 1.0f, 1.0f, 0.2f, 1.0f };
	vec4_t colorRed    = { 1.0f, 0.2f, 0.2f, 1.0f };
	vec4_t colorHeader = { 1.0f, 0.8f, 0.0f, 1.0f };
	vec4_t colorWhite  = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t colorGray   = { 0.6f, 0.6f, 0.6f, 1.0f };
	vec4_t *accColor;
	cgAttackStat_t *s;

	w = CG_windowAlloc( WID_STATS );
	w->x = 8;
	w->y = 100;
	w->w = 320;
	w->autoCloseTime = 0;  /* manual close via -stats */

	/* header */
	CG_windowAddLine( w, colorHeader, "Weapon       Shots Hits  Acc  Kills  Dmg" );

	hasData = qfalse;
	for ( att = ATT_NONE + 1; att < ATT_NUM_ATTACKS; att++ ) {
		s = &cgs.attackStats[cg.clientNum][att];
		if ( s->shots == 0 && s->kills == 0 )
			continue;

		hasData = qtrue;
		acc = s->shots > 0 ? ( 100 * s->hits / s->shots ) : 0;

		/* choose color based on accuracy */
		if ( acc >= 50 )
			accColor = &colorGreen;
		else if ( acc >= 25 )
			accColor = &colorYellow;
		else
			accColor = &colorRed;

		Com_sprintf( buf, sizeof( buf ), "%-12s %4d %4d  %3d%%  %4d %5d",
			( att < ATT_NUM_ATTACKS ) ? bg_attacklist[att].name : "???",
			s->shots, s->hits, acc, s->kills, s->damage );

		CG_windowAddLine( w, *accColor, buf );
	}

	if ( !hasData ) {
		CG_windowAddLine( w, colorGray, "No weapon data yet" );
	}
}


void CG_StatsDown_f( void )
{
	CG_statsWindow();
}


void CG_StatsUp_f( void )
{
	CG_windowFree( WID_STATS );
}


/* ──────────────────────────────────────────────────────────────
   Consumer 2: Vote notification
   ────────────────────────────────────────────────────────────── */

void CG_voteNotification( const char *msg )
{
	cg_window_t *w;
	vec4_t colorVote = { 1.0f, 1.0f, 0.5f, 1.0f };

	if ( !msg || !msg[0] )
		return;

	w = CG_windowAlloc( WID_VOTE );
	w->x = 320 - 140;   /* centered horizontally */
	w->y = 60;
	w->w = 280;
	w->autoCloseTime = 5000;

	CG_windowAddLine( w, colorVote, msg );
}


/* ──────────────────────────────────────────────────────────────
   Consumer 3: Bot order confirmation
   ────────────────────────────────────────────────────────────── */

void CG_botOrderConfirmation( const char *bot, const char *order )
{
	cg_window_t *w;
	char buf[MAX_WINDOW_LINE_LEN];
	vec4_t colorOrder = { 0.5f, 1.0f, 0.5f, 1.0f };

	if ( !bot || !order )
		return;

	w = CG_windowAlloc( WID_BOTORDER );
	w->x = 320 - 120;
	w->y = 400;
	w->w = 240;
	w->autoCloseTime = 2000;

	Com_sprintf( buf, sizeof( buf ), "Order sent: %s, %s", bot, order );
	CG_windowAddLine( w, colorOrder, buf );
}


#endif /* FEAT_STATS_WINDOW */
