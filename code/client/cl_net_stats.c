// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: fX3 contributors
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* cl_net_stats.c — statistical network overlay cvar registration (ported from fX3)
 *
 * Three on-screen overlays:
 *   cl_drawPing    — ping mean / max-spike / avg-spike / std-dev over 2 windows
 *   cl_drawSnaps   — SPS mean / drop / delayed / extrapolated over 2 windows
 *   cl_drawPackets — outbound PPS mean / drop / std-dev over 2 windows
 *
 * Each overlay has 6 cvars:  cl_draw<Name>{,FontSize,PosX,PosY,FirstInterval,SecondInterval}
 * Window lengths are in seconds; stats reset every window and display the last computed values.
 *
 * Turn 4 V-29 (debug-overlay-migration): the three SCR_Draw* overlay helpers
 * that used to live here (with their cross-frame static history + realloc'd
 * sample buffers) moved to the WUI_LAYER_DEBUG_OVERLAY custom-draw handlers in
 * code/client/wired/ui/elements/debug_overlay.c — where the history lives in
 * arena-owned per-handler context (W-28) instead of file-scope statics. This
 * file now owns ONLY the cvar registration (the handlers read these cvars by
 * name) and the cl_sent packet counter.
 */

#include "client.h"

/* Incremented in cl_input.c every time a packet is sent. The debug_packets
 * handler uses it as a guard against a zero-division on first frame. Externed
 * in client.h. */
int cl_sent;

static const cvarDesc_t netStatsDescs[] = {
	/* ping */
	/* 0  */ CVAR_BOOL( "cl_drawPing",               "0",  CVAR_ARCHIVE, "Draw per-interval ping statistics overlay (0=off, 1=on)." ),
	/* 1  */ CVAR_INT(  "cl_drawPingFontSize",        "7",  CVAR_ARCHIVE, NULL, 0, 0 ),
	/* 2  */ CVAR_INT(  "cl_drawPingPosX",            "0",  CVAR_ARCHIVE, NULL, 0, 0 ),
	/* 3  */ CVAR_INT(  "cl_drawPingPosY",            "14", CVAR_ARCHIVE, NULL, 0, 0 ),
	/* 4  */ CVAR_INT(  "cl_drawPingFirstInterval",   "2",  CVAR_ARCHIVE, "First collection window length in seconds for ping overlay.", 0, 0 ),
	/* 5  */ CVAR_INT(  "cl_drawPingSecondInterval",  "10", CVAR_ARCHIVE, "Second collection window length in seconds for ping overlay.", 0, 0 ),
	/* snaps */
	/* 6  */ CVAR_BOOL( "cl_drawSnaps",               "0",  CVAR_ARCHIVE, "Draw per-interval snapshot-rate statistics overlay (0=off, 1=on)." ),
	/* 7  */ CVAR_INT(  "cl_drawSnapsFontSize",       "7",  CVAR_ARCHIVE, NULL, 0, 0 ),
	/* 8  */ CVAR_INT(  "cl_drawSnapsPosX",           "0",  CVAR_ARCHIVE, NULL, 0, 0 ),
	/* 9  */ CVAR_INT(  "cl_drawSnapsPosY",           "7",  CVAR_ARCHIVE, NULL, 0, 0 ),
	/* 10 */ CVAR_INT(  "cl_drawSnapsFirstInterval",  "2",  CVAR_ARCHIVE, NULL, 0, 0 ),
	/* 11 */ CVAR_INT(  "cl_drawSnapsSecondInterval", "10", CVAR_ARCHIVE, NULL, 0, 0 ),
	/* packets */
	/* 12 */ CVAR_BOOL( "cl_drawPackets",               "0",  CVAR_ARCHIVE, "Draw per-interval outbound packet-rate statistics overlay (0=off, 1=on)." ),
	/* 13 */ CVAR_INT(  "cl_drawPacketsFontSize",       "7",  CVAR_ARCHIVE, NULL, 0, 0 ),
	/* 14 */ CVAR_INT(  "cl_drawPacketsPosX",           "16", CVAR_ARCHIVE, NULL, 0, 0 ),
	/* 15 */ CVAR_INT(  "cl_drawPacketsPosY",           "2",  CVAR_ARCHIVE, NULL, 0, 0 ),
	/* 16 */ CVAR_INT(  "cl_drawPacketsFirstInterval",  "2",  CVAR_ARCHIVE, NULL, 0, 0 ),
	/* 17 */ CVAR_INT(  "cl_drawPacketsSecondInterval", "10", CVAR_ARCHIVE, NULL, 0, 0 ),
};

enum {
	NS_DRAWPING, NS_DRAWPINGFONTSIZE, NS_DRAWPINGPOSX, NS_DRAWPINGPOSY,
	NS_DRAWPINGFIRSTINTERVAL, NS_DRAWPINGSECONDINTERVAL,
	NS_DRAWSNAPS, NS_DRAWSNAPSFONTSIZE, NS_DRAWSNAPSPOSX, NS_DRAWSNAPSPOSY,
	NS_DRAWSNAPSFIRSTINTERVAL, NS_DRAWSNAPSSECONDINTERVAL,
	NS_DRAWPACKETS, NS_DRAWPACKETSFONTSIZE, NS_DRAWPACKETSPOSX, NS_DRAWPACKETSPOSY,
	NS_DRAWPACKETSFIRSTINTERVAL, NS_DRAWPACKETSSECONDINTERVAL,
	NS_CVAR_COUNT
};

_Static_assert( ARRAY_LEN( netStatsDescs ) == NS_CVAR_COUNT, "netStatsDescs/enum mismatch" );
static cvar_t *netStatsHandles[NS_CVAR_COUNT];


void SCR_NetStatsInit( void ) {
	/* Register the 18 net-stat cvars (CVAR_ARCHIVE). The relocated handlers in
	 * elements/debug_overlay.c read them by name via Cvar_VariableValue, so we
	 * no longer cache per-cvar pointers here — only the registration matters. */
	Cvar_RegisterTable( netStatsDescs, ARRAY_LEN( netStatsDescs ), netStatsHandles );
}
