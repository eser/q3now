/*
cl_wired_layout_dump.c — Wired UI: visual-regression layout instrumentation

Writes the rendered pixel rect + authored colours of every *named* menu
item (and the menu itself) to `layoutdump.jsonl` in the process working
directory, once per completed Clay layout, when the `r_layoutDump` cvar is
non-zero. Disabled by default — the only cost when off is one cvar hash
lookup per layout pass.

This is the runtime-ground-truth source for tools/visual_regression. Item
geometry prefers Clay_GetElementData from the preceding completed frame, the
same one-frame-lagged authority used by input and popup anchoring. The
resolvedRect snapshot remains only a first-frame compatibility fallback. `rectSource`
makes that distinction explicit to analyzers.

JSONL schema (one object per line):
  {"region":"<item name>", "kind":"menu"|"item",
   "x":<px>, "y":<px>, "w":<px>, "h":<px>,
   "rgba_authored":[r,g,b,a],   // backcolor — the panel/fill colour
   "rgba_forecolor":[r,g,b,a],  // forecolor — text/foreground colour
   "fontPointSize":<authored base pt, 0 = inherits WUI_DEFAULT_FONT_SIZE>,
   "dpiScale":<physical/logical ratio; font px = fontPointSize*dpiScale>,
   "rootScale":<ui_rootSize; what 1rem is worth before dpiScale>,
   "focused":<0|1; exactly one item per menu may be 1>,
   "frame":<cls.framecount>, "menu":"<owning menu name>"}

The fontPointSize/dpiScale/rootScale/focused fields back the WiredUI computed
checks:
  * DPI (#4): the rendered glyph px size = fontPointSize*dpiScale (the text
    emit path multiplies by the same dpiScale this records), so a consumer
    asserts that relation without a pixel capture.
  * rem (#8): rem-sized lengths are value*rootScale*dpiScale. Recording the
    root next to the ratio lets a consumer check that a change of ui_rootSize
    moves rem-sized text and leaves px-sized text alone — the whole point of
    having a root, and the one thing that cannot be seen from dpiScale.
  * focus (#2): summing "focused" across a menu's items must be <= 1 — the
    invariant that exactly one (or zero) highlight is drawn.

The VR tool clears layoutdump.jsonl before launching the engine, so the
file only ever holds one session's worth of dumps; for a static menu every
frame's lines are identical, so the consumer may read the last block.
*/

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_compositor.h"   /* WiredUI_GetDpiScale + GetRootScale — DPI/rem fields */
#include "../store/cl_wired_store.h"

#if FEAT_WIRED_UI

#include <stdio.h>

#define WUI_LAYOUT_DUMP_FILE  "layoutdump.jsonl"
#define WUI_INSPECTOR_DUMP_FILE "wiredui-inspector.jsonl"

LOG_DECLARE_CHANNEL( ch_ui, "ui" );

static char wui_inspector_menu[64];
static char wui_inspector_item[128];
static qboolean wui_inspector_registered;

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
	char esc[256 * 6 + 1];
	JsonEscapeBody( s, (int)strlen( s ), esc, sizeof( esc ) );
	fputc( '"', f );
	fputs( esc, f );
	fputc( '"', f );
}

static void WUI_InspectorBinding( FILE *f, const char *name, const char *key,
                                  qboolean *first ) {
	const wuiStoreEntry_t *entry;

	if ( !key || !key[0] ) return;
	entry = WiredStore_Get( key );
	if ( !*first ) fputc( ',', f );
	*first = qfalse;
	WUI_DumpJsonString( f, name );
	fputs( ":{\"key\":", f );
	WUI_DumpJsonString( f, key );
	fprintf( f, ",\"present\":%d,\"generation\":%d}",
		entry ? 1 : 0, entry ? entry->generation : -1 );
}

static void WUI_InspectorDumpItem( FILE *f, const wiredMenuDef_t *menu,
                                   const wiredItemDef_t *item,
                                   const char *parentPath, int childIndex,
                                   int depth ) {
	const wiredItemDef_t *focused = WiredUI_GetFocusedItem();
	const wiredItemDef_t *hovered = WiredUI_GetHoveredItem();
	wuiPixelRect_t rect;
	qboolean clayRect;
	qboolean firstBinding = qtrue;
	char name[96];
	char path[256];
	int selected;
	int i;

	if ( !item ) return;
	if ( item->name[0] ) Q_strncpyz( name, item->name, sizeof( name ) );
	else Com_sprintf( name, sizeof( name ), "child%d", childIndex );
	if ( parentPath && parentPath[0] ) {
		Com_sprintf( path, sizeof( path ), "%s/%s", parentPath, name );
	} else {
		Q_strncpyz( path, name, sizeof( path ) );
	}
	selected = ( !wui_inspector_item[0]
		|| !Q_stricmp( wui_inspector_item, "*" )
		|| !Q_stricmp( wui_inspector_item, item->name )
		|| !Q_stricmp( wui_inspector_item, path ) );
	rect = item->resolvedRect;
	clayRect = WiredUI_ClayItemRenderedRect( menu, item, &rect );

	fputs( "{\"schema\":\"wired-ui-inspector/v1\",\"kind\":\"item\",\"menu\":", f );
	WUI_DumpJsonString( f, menu->name );
	fputs( ",\"source\":", f ); WUI_DumpJsonString( f, menu->sourcePath );
	fprintf( f, ",\"layer\":%d,\"depth\":%d,\"path\":", (int)menu->layer, depth );
	WUI_DumpJsonString( f, path );
	fputs( ",\"parent\":", f ); WUI_DumpJsonString( f, parentPath ? parentPath : "" );
	fputs( ",\"name\":", f ); WUI_DumpJsonString( f, item->name );
	fprintf( f, ",\"selected\":%d,\"type\":%d,\"style\":%d,\"position\":%d",
		selected, item->type, item->style, (int)item->position );
	fprintf( f, ",\"visible\":%d,\"focused\":%d,\"hovered\":%d",
		item->visible ? 1 : 0, focused == item ? 1 : 0, hovered == item ? 1 : 0 );
	fprintf( f, ",\"rect\":{\"x\":%.3f,\"y\":%.3f,\"w\":%.3f,\"h\":%.3f,\"source\":",
		rect.x, rect.y, rect.w, rect.h );
	WUI_DumpJsonString( f, clayRect ? "clay" : "compat" );
	fputs( "},\"layout\":{", f );
	fprintf( f, "\"container\":%d,\"direction\":%d,\"align\":%d,\"justify\":%d,\"wrap\":%d,\"scroll\":%d",
		item->isFlexContainer ? 1 : 0, (int)item->flexContainer.direction,
		(int)item->flexContainer.align, (int)item->flexContainer.justify,
		item->flexContainer.wrap ? 1 : 0, item->flexContainer.scroll ? 1 : 0 );
	fprintf( f, ",\"gap\":{\"value\":%.6g,\"unit\":%d},\"grow\":%.6g,\"shrink\":%.6g,\"perspective\":%.6g}",
		(double)item->flexContainer.gap.value, (int)item->flexContainer.gap.unit,
		(double)item->flexChild.grow, (double)item->flexChild.shrink,
		(double)item->perspective );
	fputs( ",\"styleProvenance\":{\"background\":", f );
	WUI_DumpJsonString( f, item->background );
	fprintf( f, ",\"stateColors\":%d,\"activeVariant\":%d,\"animation\":",
		item->stateColors ? 1 : 0,
		( item->hasActiveForecolor || item->hasActiveBackcolor
		  || item->hasActiveBordercolor || item->hasActiveFontSize
		  || item->hasActiveCornerRadius ) ? 1 : 0 );
	WUI_DumpJsonString( f, item->animationName );
	fputs( ",\"curve\":", f ); WUI_DumpJsonString( f, item->animationCurve );
	fputs( "},\"bindings\":{", f );
	WUI_InspectorBinding( f, "text", item->storeBind, &firstBinding );
	WUI_InspectorBinding( f, "color", item->storeBindColor, &firstBinding );
	WUI_InspectorBinding( f, "icon", item->storeBindIcon, &firstBinding );
	WUI_InspectorBinding( f, "value", item->storeBindValue, &firstBinding );
	WUI_InspectorBinding( f, "show", item->showBind, &firstBinding );
	WUI_InspectorBinding( f, "hide", item->hideBind, &firstBinding );
	WUI_InspectorBinding( f, "width", item->storeBindWidth, &firstBinding );
	fputs( "}}\n", f );

	for ( i = 0; i < item->childCount; i++ ) {
		WUI_InspectorDumpItem( f, menu, item->children[i], path, i, depth + 1 );
	}
	if ( item->repeatBlock && item->repeatBlock->templateItem ) {
		WUI_InspectorDumpItem( f, menu, item->repeatBlock->templateItem, path,
			item->childCount, depth + 1 );
	}
	if ( item->ifBlock ) {
		for ( i = 0; i < item->ifBlock->childCount; i++ ) {
			WUI_InspectorDumpItem( f, menu, item->ifBlock->children[i], path,
				item->childCount + i, depth + 1 );
		}
	}
}

static void WUI_InspectorDumpMenu( const wiredMenuDef_t *menu ) {
	FILE *f;
	int i;

	if ( !menu || !wui_inspector_menu[0]
	  || Q_stricmp( menu->name, wui_inspector_menu ) ) return;
	f = fopen( WUI_INSPECTOR_DUMP_FILE, "a" );
	if ( !f ) return;
	fputs( "{\"schema\":\"wired-ui-inspector/v1\",\"kind\":\"menu\",\"menu\":", f );
	WUI_DumpJsonString( f, menu->name );
	fputs( ",\"source\":", f ); WUI_DumpJsonString( f, menu->sourcePath );
	fprintf( f, ",\"layer\":%d,\"itemCount\":%d,\"pathB\":%d,\"frame\":%d}\n",
		(int)menu->layer, menu->itemCount, menu->pathBKind ? 1 : 0, cls.framecount );
	for ( i = 0; i < menu->itemCount; i++ ) {
		WUI_InspectorDumpItem( f, menu, menu->items[i], "", i, 0 );
	}
	fclose( f );
}

static void WUI_InspectorSelect_f( void ) {
	FILE *f;

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "usage: wui_inspect <menu> [item|*]\n" );
		return;
	}
	Q_strncpyz( wui_inspector_menu, Cmd_Argv( 1 ), sizeof( wui_inspector_menu ) );
	Q_strncpyz( wui_inspector_item, Cmd_Argc() > 2 ? Cmd_Argv( 2 ) : "*",
		sizeof( wui_inspector_item ) );
	f = fopen( WUI_INSPECTOR_DUMP_FILE, "w" );
	if ( f ) fclose( f );
	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"WiredUI inspector: menu=%s item=%s protocol=wired-ui-inspector/v1\n",
		wui_inspector_menu, wui_inspector_item );
}

static void WUI_InspectorClear_f( void ) {
	wui_inspector_menu[0] = '\0';
	wui_inspector_item[0] = '\0';
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI inspector: selection cleared\n" );
}

void WUI_InspectorInit( void ) {
	if ( wui_inspector_registered ) return;
	Cmd_AddCommand( "wui_inspect", WUI_InspectorSelect_f );
	Cmd_AddCommand( "wui_inspect_clear", WUI_InspectorClear_f );
	wui_inspector_registered = qtrue;
}

void WUI_InspectorShutdown( void ) {
	if ( !wui_inspector_registered ) return;
	Cmd_RemoveCommand( "wui_inspect" );
	Cmd_RemoveCommand( "wui_inspect_clear" );
	wui_inspector_registered = qfalse;
	wui_inspector_menu[0] = '\0';
	wui_inspector_item[0] = '\0';
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
	fprintf( f, ",\"fontPointSize\":%.6g,\"dpiScale\":%.6g,\"rootScale\":%.6g,\"focused\":%d",
		(double)fontPointSize, (double)WiredUI_GetDpiScale(),
		(double)WiredUI_GetRootScale(), focused );
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
                                const wiredMenuDef_t *menu,
                                int parentFocused, const char *parentName,
                                int childIndex ) {
	const wiredItemDef_t *foc = (const wiredItemDef_t *)WiredUI_GetFocusedItem();
	int focused, focusActive, focusDriven;
	wuiPixelRect_t renderedRect;
	qboolean clayRect;
	char synthName[ 96 ];
	const char *region;
	int i;

	if ( !item ) return;
	renderedRect = item->resolvedRect;
	clayRect = WiredUI_ClayItemRenderedRect( menu, item, &renderedRect );

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
		const wuiPixelRect_t *r = &renderedRect;
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
		fputs( ",\"rectSource\":", f );
		WUI_DumpJsonString( f, clayRect ? "clay" : "compat" );
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
		WUI_DumpItemTreeEx( f, item->children[ i ], frame, menuName, menu,
			focusDriven, item->name[ 0 ] ? item->name : region, i );
	}
}

static void WUI_DumpItemTree( FILE *f, const wiredItemDef_t *item,
                              int frame, const char *menuName,
                              const wiredMenuDef_t *menu ) {
	/* Top-level items have no focused parent (parentFocused=0). */
	WUI_DumpItemTreeEx( f, item, frame, menuName, menu, 0, NULL, 0 );
}

void WUI_DumpLayout( const struct wiredMenuDef_s *menu ) {
	const wiredMenuDef_t *m = (const wiredMenuDef_t *)menu;
	FILE *f;
	int frame;

	if ( !m ) return;
	WUI_InspectorDumpMenu( m );
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
		WUI_DumpItemTree( f, m->items[i], frame, m->name, m );
	}

	fclose( f );
}

#endif // FEAT_WIRED_UI
