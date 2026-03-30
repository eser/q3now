// menumacros.h — Wired UI shared macros for DRY .menu definitions
//
// Based on ET:Legacy's menumacros.h pattern. Each macro expands to a
// complete itemDef block. All .menu files should #include this.
//
// Usage:
//   #include "ui/menumacros.h"
//   menuDef {
//     MENU_BACKGROUND("main_menu")
//     BUTTON(270, 200, 100, 24, "Play", 0.5, play_action)
//     LABEL(220, 120, 200, 16, "Subtitle text", 0.4, 1)
//   }

// ── layout constants ──────────────────────────────────────────────────

#define MENU_W          640
#define MENU_H          480
#define BUTTON_H        24
#define LABEL_H         16
#define ITEM_GAP        4
#define MARGIN          16

// ── colors ────────────────────────────────────────────────────────────

#define COLOR_WHITE     1 1 1 1
#define COLOR_GREY      0.7 0.7 0.7 1
#define COLOR_DARK_GREY 0.4 0.4 0.4 1
#define COLOR_AMBER     1 0.75 0 1
#define COLOR_RED       1 0.2 0.2 1
#define COLOR_GREEN     0.2 1 0.2 1
#define COLOR_BG        0.1 0.1 0.15 1
#define COLOR_BG_LIGHT  0.15 0.15 0.2 0.9
#define COLOR_FOCUS     1 0.75 0 0.15

// ── menu background ───────────────────────────────────────────────────

#define MENU_BACKGROUND(MENUNAME) \
	name MENUNAME \
	fullScreen 1 \
	rect 0 0 MENU_W MENU_H \
	style 1 \
	backcolor COLOR_BG \
	focuscolor COLOR_FOCUS

// ── decoration (non-interactive text) ─────────────────────────────────

#define LABEL(X, Y, W, H, TEXT, SCALE, ALIGN) \
	itemDef { \
		type 0 \
		text TEXT \
		rect X Y W H \
		textalign ALIGN \
		textscale SCALE \
		forecolor COLOR_WHITE \
		decoration \
		visible 1 \
	}

#define LABEL_COLOR(X, Y, W, H, TEXT, SCALE, ALIGN, R, G, B, A) \
	itemDef { \
		type 0 \
		text TEXT \
		rect X Y W H \
		textalign ALIGN \
		textscale SCALE \
		forecolor R G B A \
		decoration \
		visible 1 \
	}

// ── button ────────────────────────────────────────────────────────────

#define BUTTON(X, Y, W, H, TEXT, SCALE, ACTION) \
	itemDef { \
		type 1 \
		text TEXT \
		rect X Y W H \
		textalign 1 \
		textscale SCALE \
		forecolor COLOR_AMBER \
		visible 1 \
		action { ACTION } \
	}

#define BUTTON_TOOLTIP(X, Y, W, H, TEXT, SCALE, TOOLTIP, ACTION) \
	itemDef { \
		type 1 \
		text TEXT \
		rect X Y W H \
		textalign 1 \
		textscale SCALE \
		forecolor COLOR_AMBER \
		tooltip TOOLTIP \
		visible 1 \
		action { ACTION } \
	}

// ── named button (for script targeting) ───────────────────────────────

#define BUTTON_NAMED(NAME, X, Y, W, H, TEXT, SCALE, ACTION) \
	itemDef { \
		name NAME \
		type 1 \
		text TEXT \
		rect X Y W H \
		textalign 1 \
		textscale SCALE \
		forecolor COLOR_AMBER \
		visible 1 \
		action { ACTION } \
	}

// ── separator line ────────────────────────────────────────────────────

#define SEPARATOR(X, Y, W) \
	itemDef { \
		type 0 \
		rect X Y W 1 \
		style 1 \
		backcolor 0.3 0.3 0.3 0.5 \
		decoration \
		visible 1 \
	}

// ── cvar-bound items (Phase 2: need parser support) ───────────────────

// YESNO toggle — bind to a cvar 0/1
#define YESNO(X, Y, W, H, LABEL, SCALE, CVAR) \
	itemDef { \
		type 11 \
		text LABEL \
		rect X Y W H \
		textscale SCALE \
		forecolor COLOR_WHITE \
		cvar CVAR \
		visible 1 \
	}

// SLIDER — bind to a cvar with range
#define SLIDER(X, Y, W, H, LABEL, SCALE, CVAR, MIN, MAX, STEP) \
	itemDef { \
		type 10 \
		text LABEL \
		rect X Y W H \
		textscale SCALE \
		forecolor COLOR_WHITE \
		cvar CVAR \
		visible 1 \
	}

// MULTI — multi-choice cvar selector
#define MULTI(X, Y, W, H, LABEL, SCALE, CVAR, LIST) \
	itemDef { \
		type 12 \
		text LABEL \
		rect X Y W H \
		textscale SCALE \
		forecolor COLOR_WHITE \
		cvar CVAR \
		cvarStrList LIST \
		visible 1 \
	}

// BIND — key binding
#define BIND(X, Y, W, H, LABEL, SCALE, COMMAND) \
	itemDef { \
		type 13 \
		text LABEL \
		rect X Y W H \
		textscale SCALE \
		forecolor COLOR_WHITE \
		cvar COMMAND \
		visible 1 \
	}

// EDITFIELD — text input
#define EDITFIELD(X, Y, W, H, LABEL, SCALE, CVAR, MAXCHARS) \
	itemDef { \
		type 4 \
		text LABEL \
		rect X Y W H \
		textscale SCALE \
		forecolor COLOR_WHITE \
		cvar CVAR \
		maxChars MAXCHARS \
		visible 1 \
	}

// ── subwindow (panel header) ──────────────────────────────────────────

#define SUBWINDOW(X, Y, W, H, TEXT) \
	itemDef { \
		type 0 \
		text TEXT \
		rect X Y W 16 \
		textalign 0 \
		textscale 0.35 \
		forecolor COLOR_WHITE \
		style 1 \
		backcolor 0.2 0.2 0.25 0.8 \
		decoration \
		visible 1 \
	}

// ══════════════════════════════════════════════════════════════════════
// TA-STYLED VARIANTS
// Use Team Arena art assets for polished visual appearance.
// These complement the base macros above — use whichever fits.
// ══════════════════════════════════════════════════════════════════════

// ── TA colors ─────────────────────────────────────────────────────────

#define TA_COLOR_FOCUS      1 0.75 0 1
#define TA_COLOR_SHADOW     0.1 0.1 0.1 0.25
#define TA_COLOR_PANEL_BG   0.0 0.0 0.0 0.7
#define TA_COLOR_ITEM_BG    0.2 0.2 0.25 0.5
#define TA_COLOR_BUTTON_FG  1 0.85 0.3 1

// ── TA button (with background image) ─────────────────────────────────

#define TA_BUTTON(X, Y, W, H, TEXT, SCALE, ACTION) \
	itemDef { \
		type 1 \
		text TEXT \
		rect X Y W H \
		textalign 1 \
		textscale SCALE \
		forecolor TA_COLOR_BUTTON_FG \
		style 3 \
		background "ui/assets/button_back" \
		visible 1 \
		action { ACTION } \
	}

// ── TA button with red variant (for destructive actions) ──────────────

#define TA_BUTTON_RED(X, Y, W, H, TEXT, SCALE, ACTION) \
	itemDef { \
		type 1 \
		text TEXT \
		rect X Y W H \
		textalign 1 \
		textscale SCALE \
		forecolor COLOR_WHITE \
		style 3 \
		background "ui/assets/button_red" \
		visible 1 \
		action { ACTION } \
	}

// ── TA panel background (floating panel with TA frame art) ────────────

#define TA_PANEL_BG(X, Y, W, H) \
	itemDef { \
		type 0 \
		rect X Y W H \
		style 1 \
		backcolor TA_COLOR_PANEL_BG \
		decoration \
		visible 1 \
	}

// ── TA gradient section header ────────────────────────────────────────

#define TA_SECTION_HEADER(X, Y, W, TEXT) \
	itemDef { \
		type 0 \
		text TEXT \
		rect X Y W 18 \
		textalign 0 \
		textscale 0.4 \
		forecolor TA_COLOR_BUTTON_FG \
		style 2 \
		backcolor 0.15 0.15 0.2 0.6 \
		decoration \
		visible 1 \
	}

// ── TA separator (gradient bar) ───────────────────────────────────────

#define TA_SEPARATOR(X, Y, W) \
	itemDef { \
		type 0 \
		rect X Y W 2 \
		style 3 \
		background "ui/assets/gradientbar2" \
		backcolor 0.5 0.5 0.5 0.3 \
		decoration \
		visible 1 \
	}

// ── TA menu background (fullscreen with TA shader) ────────────────────

#define TA_MENU_BACKGROUND(MENUNAME) \
	name MENUNAME \
	fullScreen 1 \
	rect 0 0 MENU_W MENU_H \
	style 3 \
	background "ui/assets/menuback_a" \
	focuscolor TA_COLOR_FOCUS

// ── TA menu background (panel — not fullscreen, with TA art) ──────────

#define TA_PANEL_BACKGROUND(MENUNAME, X, Y, W, H) \
	name MENUNAME \
	fullScreen 0 \
	rect X Y W H \
	style 1 \
	backcolor TA_COLOR_PANEL_BG \
	focuscolor TA_COLOR_FOCUS

// ── TA navigation arrows ──────────────────────────────────────────────

#define TA_NAV_LEFT(X, Y, SIZE, ACTION) \
	itemDef { \
		type 1 \
		rect X Y SIZE SIZE \
		style 3 \
		background "ui/assets/backarrow" \
		visible 1 \
		action { ACTION } \
	}

#define TA_NAV_RIGHT(X, Y, SIZE, ACTION) \
	itemDef { \
		type 1 \
		rect X Y SIZE SIZE \
		style 3 \
		background "ui/assets/forwardarrow" \
		visible 1 \
		action { ACTION } \
	}

// ── TA subwindow (panel header with gradient bar) ─────────────────────

#define TA_SUBWINDOW(X, Y, W, H, TEXT) \
	itemDef { \
		type 0 \
		text TEXT \
		rect X Y W 18 \
		textalign 0 \
		textscale 0.35 \
		forecolor TA_COLOR_BUTTON_FG \
		style 2 \
		backcolor 0.15 0.12 0.05 0.6 \
		decoration \
		visible 1 \
	}
