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
   "frame":<cls.framecount>, "menu":"<owning menu name>"}

The VR tool clears layoutdump.jsonl before launching the engine, so the
file only ever holds one session's worth of dumps; for a static menu every
frame's lines are identical, so the consumer may read the last block.
*/

#include "../../client.h"
#include "cl_wired_ui.h"

#if FEAT_WIRED_UI

#include <stdio.h>

#define WUI_LAYOUT_DUMP_FILE  "layoutdump.jsonl"

static void WUI_DumpJsonRGBA( FILE *f, const char *key, const vec4_t c ) {
	fprintf( f, "\"%s\":[%.6g,%.6g,%.6g,%.6g]", key,
		(double)c[0], (double)c[1], (double)c[2], (double)c[3] );
}

// JSON string escaping for the small subset of characters that can appear
// in a menu/item name (quotes and backslash; names are otherwise printable
// ASCII per the parser). Writes a bare quoted string.
static void WUI_DumpJsonString( FILE *f, const char *s ) {
	fputc( '"', f );
	for ( ; *s; s++ ) {
		if ( *s == '"' || *s == '\\' ) fputc( '\\', f );
		fputc( *s, f );
	}
	fputc( '"', f );
}

static void WUI_DumpRegionLine( FILE *f, const char *region, const char *kind,
                                const wuiPixelRect_t *r, const vec4_t back,
                                const vec4_t fore, int frame, const char *menuName ) {
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
	fprintf( f, ",\"frame\":%d,\"menu\":", frame );
	WUI_DumpJsonString( f, menuName );
	fputs( "}\n", f );
}

static void WUI_DumpItemTree( FILE *f, const wiredItemDef_t *item,
                              int frame, const char *menuName ) {
	if ( !item ) return;
	if ( item->name[0] ) {
		WUI_DumpRegionLine( f, item->name, "item", &item->resolvedRect,
			item->backcolor, item->forecolor, frame, menuName );
	}
	for ( int i = 0; i < item->childCount; i++ ) {
		WUI_DumpItemTree( f, item->children[i], frame, menuName );
	}
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
		WUI_DumpRegionLine( f, m->name, "menu", &m->resolvedRect,
			m->backcolor, m->forecolor, frame, m->name );
	}
	for ( int i = 0; i < m->itemCount; i++ ) {
		WUI_DumpItemTree( f, m->items[i], frame, m->name );
	}

	fclose( f );
}

#endif // FEAT_WIRED_UI
