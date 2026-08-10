/*
cl_wired_layout_dump.c — Wired UI: visual-regression layout instrumentation

Writes the resolved pixel rect + authored colours of every *named* menu
item (and the menu itself) to `layoutdump.jsonl` in the process working
directory, once per WUI_LayoutMenu pass, when the `r_layoutDump` cvar is
non-zero. Disabled by default — the only cost when off is one cvar hash
lookup per layout pass.

This is the runtime-ground-truth source for tools/visual_regression: the
layout engine resolves rects per-element during the layout walk (there is
no single "layout pass complete" callback), so the dump is invoked at the
tail of WUI_LayoutMenu after every item's resolvedRect has been filled.

JSONL schema (one object per line):
  {"region":"<item name>", "kind":"menu"|"item",
   "x":<px>, "y":<px>, "w":<px>, "h":<px>,
   "rgba_authored":[r,g,b,a],   // backcolor — the panel/fill colour
   "rgba_forecolor":[r,g,b,a],  // forecolor — text/foreground colour
   "fontPointSize":<authored base pt, 0 = inherits WUI_DEFAULT_FONT_SIZE>,
   "dpiScale":<physical/logical ratio; font px = fontPointSize*dpiScale>,
   "focused":<0|1; exactly one item per menu may be 1>,
   "frame":<cls.framecount>, "menu":"<owning menu name>"}

The fontPointSize/dpiScale/focused fields back the WiredUI computed checks:
  * DPI (#4): the rendered glyph px size = fontPointSize*dpiScale (the text
    emit path multiplies by the same dpiScale this records), so a consumer
    asserts that relation without a pixel capture.
  * focus (#2): summing "focused" across a menu's items must be <= 1 — the
    invariant that exactly one (or zero) highlight is drawn.

The VR tool clears layoutdump.jsonl before launching the engine, so the
file only ever holds one session's worth of dumps; for a static menu every
frame's lines are identical, so the consumer may read the last block.
*/

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_compositor.h"   /* WiredUI_GetDpiScale — DPI verification field */

#if FEAT_WIRED_UI

#include <stdio.h>

#define WUI_LAYOUT_DUMP_FILE  "layoutdump.jsonl"

static void WUI_DumpJsonRGBA( FILE *f, const char *key, const vec4_t c ) {
	fprintf( f, "\"%s\":[%.6g,%.6g,%.6g,%.6g]", key,
		(double)c[0], (double)c[1], (double)c[2], (double)c[3] );
}

// Writes a bare quoted, fully JSON-escaped string. Reuses the logging core's
// JsonEscapeBody (declared via q_shared.h -> log.h, linked from qcommon's
// log_sink_file.c) so the escaping matches the rest of the JSONL we emit:
// it covers quotes, backslash, control chars (\n \r \t and < 0x20 -> \uXXXX)
// — the hand-rolled version only escaped " and \, leaving control chars raw
// and producing invalid JSON. Names dumped here come from fixed char[64]
// fields, so a 6x-expansion stack buffer is always enough.
// Caveat: JsonEscapeBody also strips Quake "^N" colour codes; item/menu names
// are parsed identifiers and do not carry colour codes, so this is inert here.
static void WUI_DumpJsonString( FILE *f, const char *s ) {
	char esc[64 * 6 + 1];
	JsonEscapeBody( s, (int)strlen( s ), esc, sizeof( esc ) );
	fputc( '"', f );
	fputs( esc, f );
	fputc( '"', f );
}

static void WUI_DumpRegionLine( FILE *f, const char *region, const char *kind,
                                const wuiPixelRect_t *r, const vec4_t back,
                                const vec4_t fore, float fontPointSize,
                                int focused, const char *activeCvar,
                                int hasActiveBackcolor, int frame,
                                const char *menuName ) {
	fputc( '{', f );
	fputs( "\"region\":", f );
	WUI_DumpJsonString( f, region );
	fprintf( f, ",\"kind\":\"%s\"", kind );
	fprintf( f, ",\"x\":%.3f,\"y\":%.3f,\"w\":%.3f,\"h\":%.3f",
		r->x, r->y, r->w, r->h );
	fputc( ',', f );
	WUI_DumpJsonRGBA( f, "rgba_authored", back );
	fputc( ',', f );
	WUI_DumpJsonRGBA( f, "rgba_forecolor", fore );
	fprintf( f, ",\"fontPointSize\":%.6g,\"dpiScale\":%.6g,\"focused\":%d",
		(double)fontPointSize, (double)WiredUI_GetDpiScale(), focused );
	/* Physical backing width: lets the DPI gate derive its expected ratio from
	 * the window the engine ACTUALLY got. On HiDPI the launch width is the
	 * LOGICAL size (a 1280 request backs at 2560 on a 2x display), so a gate
	 * that assumes physical == requested computes the wrong expectation —
	 * the engine's ratio was right, the harness's assumption wasn't. */
	fprintf( f, ",\"vidWidthPx\":%d", cls.glconfig.vidWidth );
	/* Selection-state verification fields: activeCvar = the `active <cvar>`
	 * binding (empty after the menu rows dropped ui_currentMenuItem; the
	 * settings tabs keep ui_settingsSection); hasActiveBackcolor = whether a
	 * backcolor.active variant exists (the removed cyan second-highlight). A
	 * menu row with both empty/0 proves the hover-active system is gone and
	 * focus is its only state. */
	fputc( ',', f );
	fputs( "\"activeCvar\":", f );
	WUI_DumpJsonString( f, activeCvar ? activeCvar : "" );
	fprintf( f, ",\"hasActiveBackcolor\":%d", hasActiveBackcolor );
	fprintf( f, ",\"frame\":%d,\"menu\":", frame );
	WUI_DumpJsonString( f, menuName );
	fputs( "}\n", f );
}

/* focusActive mirrors the focus-derived isActive predicate in
 * cl_wired_clay.c (item == focused, or item is a direct child of the focused
 * container) — INCLUDING the strict gate: it applies only to items with no
 * cvar-`active` binding (activeCvar empty), so the settings tabs (which DO bind
 * a cvar) never get focus-as-active. The focused row's text children get this
 * → forecolor.active, so a consumer can verify the text-emphasis-follows-focus
 * re-homing even though the children are unnamed. parentFocused is qtrue when
 * this item's container is the focused item AND that container is itself a
 * focus-driven (non-cvar) row (threaded by the caller). */
static void WUI_DumpItemTreeEx( FILE *f, const wiredItemDef_t *item,
                                int frame, const char *menuName,
                                int parentFocused, const char *parentName,
                                int childIndex ) {
	const wiredItemDef_t *foc = (const wiredItemDef_t *)WiredUI_GetFocusedItem();
	int focused, focusActive, focusDriven;
	char synthName[ 96 ];
	const char *region;
	int i;

	if ( !item ) return;

	focused = ( foc == item ) ? 1 : 0;
	/* === cl_wired_clay.c isActive-by-focus, with the activeCvar gate === */
	focusActive = ( item->activeCvar[ 0 ] == '\0' )
	            ? ( focused || parentFocused )
	            : 0;
	/* a container is focus-driven (lights its children) only when it is the
	 * focused item AND uses no cvar-active mechanism */
	focusDriven = ( focused && item->activeCvar[ 0 ] == '\0' ) ? 1 : 0;

	if ( item->name[ 0 ] ) {
		region = item->name;
	} else {
		/* Synthetic name so unnamed leaf children (the ">" chevron + LABEL +
		 * SUB inside a menu row) still appear, letting the check confirm their
		 * focusActive flips with the parent's focus. */
		Com_sprintf( synthName, sizeof( synthName ), "%s/child%d",
			parentName && parentName[ 0 ] ? parentName : "(anon)", childIndex );
		region = synthName;
	}

	{
		/* Full item line — same columns as WUI_DumpRegionLine (so the existing
		 * #2/#4/#N checks keep working) plus focusActive + hasActiveForecolor
		 * for the text-emphasis-follows-focus verification. */
		const wuiPixelRect_t *r = &item->resolvedRect;
		fputc( '{', f );
		fputs( "\"region\":", f );
		WUI_DumpJsonString( f, region );
		fputs( ",\"kind\":\"item\"", f );
		fprintf( f, ",\"x\":%.3f,\"y\":%.3f,\"w\":%.3f,\"h\":%.3f",
			r->x, r->y, r->w, r->h );
		fputc( ',', f );
		WUI_DumpJsonRGBA( f, "rgba_authored", item->backcolor );
		fputc( ',', f );
		WUI_DumpJsonRGBA( f, "rgba_forecolor", item->forecolor );
		fprintf( f, ",\"fontPointSize\":%.6g,\"dpiScale\":%.6g",
			(double)item->fontPointSize, (double)WiredUI_GetDpiScale() );
		fprintf( f, ",\"focused\":%d,\"focusActive\":%d", focused, focusActive );
		fputc( ',', f );
		fputs( "\"activeCvar\":", f );
		WUI_DumpJsonString( f, item->activeCvar );
		fprintf( f, ",\"hasActiveBackcolor\":%d", item->hasActiveBackcolor ? 1 : 0 );
		fprintf( f, ",\"hasActiveForecolor\":%d", item->hasActiveForecolor ? 1 : 0 );
		fprintf( f, ",\"frame\":%d,\"menu\":", frame );
		WUI_DumpJsonString( f, menuName );
		fputs( "}\n", f );
	}

	for ( i = 0; i < item->childCount; i++ ) {
		WUI_DumpItemTreeEx( f, item->children[ i ], frame, menuName,
			focusDriven, item->name[ 0 ] ? item->name : region, i );
	}
}

static void WUI_DumpItemTree( FILE *f, const wiredItemDef_t *item,
                              int frame, const char *menuName ) {
	/* Top-level items have no focused parent (parentFocused=0). */
	WUI_DumpItemTreeEx( f, item, frame, menuName, 0, NULL, 0 );
}

void WUI_DumpLayout( const struct wiredMenuDef_s *menu ) {
	const wiredMenuDef_t *m = (const wiredMenuDef_t *)menu;
	FILE *f;
	int frame;

	if ( !m ) return;
	if ( Cvar_VariableIntegerValue( "r_layoutDump" ) <= 0 ) return;

	f = fopen( WUI_LAYOUT_DUMP_FILE, "a" );
	if ( !f ) return;

	frame = cls.framecount;

	if ( m->name[0] ) {
		/* The menu container is not a focusable item (focused=0) and carries
		 * no own font size (0); it still records dpiScale via WUI_DumpRegionLine
		 * so a consumer can read the ratio from the menu line alone. */
		WUI_DumpRegionLine( f, m->name, "menu", &m->resolvedRect,
			m->backcolor, m->forecolor, 0.0f, 0, "", 0, frame, m->name );
	}
	for ( int i = 0; i < m->itemCount; i++ ) {
		WUI_DumpItemTree( f, m->items[i], frame, m->name );
	}

	fclose( f );
}

#endif // FEAT_WIRED_UI
