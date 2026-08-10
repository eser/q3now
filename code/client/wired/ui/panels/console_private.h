// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
 * console_private.h — UI-tier private interface for the console projection
 * (code/client/wired/ui/elements/console.c).
 *
 * TURN 3 V-20: console_t + the compile-time constants moved to the core
 * header code/qcommon/wired/core/console/con_private.h (presentation-agnostic,
 * joins the headless build). This header now carries only the UI-tier render
 * surface: text metrics, the per-element color backing vecs, the UI-consumed
 * cvar pointers, and the chat externs. The elements/console.c projection reads
 * buffer state through the con_public.h accessors — NOT through the `con`
 * macro — so the include below is for the metrics/cvars, not for buffer reads.
 *
 * NOT for inclusion outside the console projection.
 */
#pragma once

/* core data-model + constants (console_t, DEFAULT_CONSOLE_WIDTH, CON_TEXTSIZE,
 * CON_LINEBUF_SIZE, NUM_CON_TIMES, CONSOLE_ARENA_SIZE, the `con` macro). */
#include "../../../../qcommon/wired/core/console/con_private.h"

/* ── Render metrics (written by Con_UpdateTextMetrics, in console.c) ──── */
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

/* ── Cvar pointers (registered in Con_Init, externed here for the UI) ── */
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

/* ── Chat state (defined in cl_keys.c / cl_main.c) ─────────────────── */
extern qboolean chat_team;
extern int      chat_playerNum;

/* ── Internal functions in console.c called by the draw layer ────────── */
void		Con_UpdateColors( void );

/* UI lifecycle: register the presentation-only commands + color parse +
 * close hook (Con_InitProjection), tear them down (Con_ShutdownProjection).
 * Called from the client init/shutdown path right after Con_Init/Con_Shutdown. */
void		Con_InitProjection( void );
void		Con_ShutdownProjection( void );
