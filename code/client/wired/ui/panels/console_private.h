// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
 * cl_console_private.h — Shared internal interface between cl_console.c
 * and wired/ui/panels/console.c.
 *
 * NOT for inclusion outside those two translation units.
 */
#pragma once

/* ── Compile-time constants ─────────────────────────────────────────── */
#define  DEFAULT_CONSOLE_WIDTH  78
#define  CON_LINEBUF_SIZE       513
#define  NUM_CON_TIMES          17
#define  CON_TEXTSIZE           65536
#define  CONSOLE_ARENA_SIZE     ( sizeof(console_t) + 4096 )

/* ── console_t — the full console state (data model + render state) ── */
typedef struct {
	qboolean	initialized;

	short		text[CON_TEXTSIZE];
	int			current;
	int			x;
	int			display;

	int			linewidth;
	int			totallines;

	float		xadjust;

	float		displayFrac;
	float		finalFrac;

	int			vislines;

	int			times[NUM_CON_TIMES];
	vec4_t		color;

	int			viswidth;
	int			vispage;

	qboolean	newline;

	/* search state */
	qboolean	searchActive;
	char		searchPattern[256];
	int			searchCursor;
	int			searchLine;
	int			searchMatchCount;

	/* mark (selection) state */
	qboolean	markActive;
	int			markStartLine;
	int			markStartCol;
	int			markEndLine;
	int			markEndCol;

} console_t;

/* ── Backing pointer + accessor macro ───────────────────────────────── */
extern console_t *s_con;
#define con (*s_con)

/* ── Render metrics (written by Con_UpdateTextMetrics) ──────────────── */
extern float con_textPointSize;
extern float con_lineAdvance;
extern float con_textCharWidth;
extern float con_textNativeCharW;

/* ── Per-element color backing vecs (written by Con_UpdateColor) ─────── */
extern vec4_t con_bgColor;
extern vec4_t con_borderColor;
extern vec4_t con_textColor;
extern vec4_t con_cvarColor;
extern vec4_t con_cmdColor;
extern vec4_t con_valueColor;

/* ── Cvar pointers (registered in Con_Init) ─────────────────────────── */
extern cvar_t *con_lineheight;
extern cvar_t *con_conspeed;
extern cvar_t *con_autoclear;
extern cvar_t *con_notifytime;
extern cvar_t *con_notifylines;
extern cvar_t *con_scale;
extern cvar_t *con_anim;
extern cvar_t *con_clock;
extern cvar_t *con_fade;
extern cvar_t *con_fps;
extern cvar_t *cl_consoleHeight;
extern cvar_t *cl_consoleType;
extern cvar_t *con_timestamp;
extern cvar_t *con_colBG;
extern cvar_t *con_colBorder;
extern cvar_t *con_colText;
extern cvar_t *con_colCVar;
extern cvar_t *con_colCmd;
extern cvar_t *con_colValue;

/* ── Chat state (defined in cl_cin.c / cl_main.c) ─────────────────── */
extern qboolean chat_team;
extern int      chat_playerNum;

/* ── Internal functions in cl_console.c called by the draw layer ─────── */
void		Con_UpdateColors( void );
qboolean	Con_MarkCellIsSelected( int row, int col );
