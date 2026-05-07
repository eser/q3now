// wmenumacros.h -- Wired UI macro system for .wmenu files
//
// Parallel to menumacros.h but designed for the normalized coordinate
// system (0.0-1.0), point-based font sizes, and per-item hover states.
//
// Usage:
//   #include "ui/wmenumacros.h"
//   menuDef {
//     WFULLSCREEN_BACKGROUND
//     WBUTTON(0.35, 0.4, 0.3, WM_BUTTON_H, "Play", WM_FONT_BODY, play)
//     WLABEL(0.2, 0.2, 0.6, WM_LABEL_H, "Welcome", WM_FONT_BODY, 1)
//   }

#ifndef WMENUMACROS_H
#define WMENUMACROS_H

// ── color palette ───────────────────────────────────────────────────
// Intentional, not generic. Amber accent carries Q3 heritage.

#define WCOLOR_TEXT          1 1 1 0.9
#define WCOLOR_TEXT_DIM      0.7 0.7 0.7 0.7
#define WCOLOR_ACCENT        0.85 0.55 0.1 1
#define WCOLOR_ACCENT_HOVER  1 0.75 0.2 1
#define WCOLOR_ACCENT_DIM    0.6 0.4 0.08 0.8
#define WCOLOR_BG            0 0 0 0.8
#define WCOLOR_BG_HIGHLIGHT  0.1 0.1 0.1 0.9
#define WCOLOR_SEPARATOR     0.3 0.3 0.3 0.5
#define WCOLOR_SECTION       0.85 0.55 0.1 0.8
#define WCOLOR_ERROR         1 0.45 0.35 1
#define WCOLOR_ERROR_DIM     0.6 0.25 0.2 0.8

// ── layout constants (normalized) ───────────────────────────────────
// All heights are fractions of screen height (480 = 1.0).

#define WM_BUTTON_H     0.05
#define WM_LABEL_H      0.033
#define WM_ITEM_GAP     0.008
#define WM_SEPARATOR_H  0.002
#define WM_MARGIN        0.025

// ── font sizes (points) ────────────────────────────────────────────

#define WM_FONT_BODY    12
#define WM_FONT_LABEL   11
#define WM_FONT_HEADING 16
#define WM_FONT_SMALL   9
#define WM_FONT_TITLE   22

// ── fullscreen background ──────────────────────────────────────────
// Fills the entire screen with WCOLOR_BG. No parameters.
//
// Example:
//   menuDef {
//     WFULLSCREEN_BACKGROUND
//     ...
//   }

#define WFULLSCREEN_BACKGROUND \
	itemDef { \
		name "bg_full" \
		position viewport \
		ownerdraw "background_full" \
		rect 0 0 100vw 100vh \
		decoration \
		visible 1 \
	}

// ── background scanline grid ──────────────────────────────────────
// Overlay horizontal scanlines via ownerdraw. Place after WFULLSCREEN_BACKGROUND.
// No parameters.
//
// Example:
//   WFULLSCREEN_BACKGROUND
//   WBACKGROUND_GRID

#define WBACKGROUND_GRID \
	itemDef { \
		rect 0 0 1 1 \
		ownerdraw "background_grid" \
		visible 1 \
		decoration \
	}

// ── static text label ──────────────────────────────────────────────
// Decoration only, no interaction.
//
// Parameters:
//   X, Y     - normalized position (0.0-1.0)
//   W, H     - normalized width and height
//   TEXT     - display string (quoted)
//   FONTSIZE - point size (use WM_FONT_BODY, WM_FONT_LABEL, etc.)
//   ALIGN    - 0 = left, 1 = center, 2 = right
//
// Example:
//   WLABEL(0.2, 0.2, 0.6, WM_LABEL_H, "Welcome", WM_FONT_BODY, 1)

#define WLABEL(X, Y, W, H, TEXT, FONTSIZE, ALIGN) \
	itemDef { \
		rect X Y W H \
		type 0 \
		text TEXT \
		textalign ALIGN \
		font "oxanium" FONTSIZE \
		forecolor WCOLOR_TEXT \
		visible 1 \
		decoration \
		grow 0 \
	}

// ── clickable text button ──────────────────────────────────────────
// Center-aligned. Amber foreground with brighter hover state.
//
// Parameters:
//   X, Y     - normalized position (0.0-1.0)
//   W, H     - normalized width and height (use WM_BUTTON_H for standard height)
//   TEXT     - button label (quoted)
//   FONTSIZE - point size (use WM_FONT_BODY, etc.)
//   ACTION   - action block executed on click (e.g., open "menu_name")
//
// Example:
//   WBUTTON(0.35, 0.4, 0.3, WM_BUTTON_H, "Play", WM_FONT_BODY, play)

#define WBUTTON(X, Y, W, H, TEXT, FONTSIZE, ACTION) \
	itemDef { \
		rect X Y W H \
		type 1 \
		text TEXT \
		textalign 1 \
		font "oxanium" FONTSIZE \
		forecolor WCOLOR_ACCENT \
		visible 1 \
		grow 0 \
		mouseEnter { setcolor forecolor WCOLOR_ACCENT_HOVER } \
		mouseExit { setcolor forecolor WCOLOR_ACCENT } \
		action { ACTION } \
	}

// ── horizontal separator line ──────────────────────────────────────
// Thin divider. Height is WM_SEPARATOR_H (0.002).
//
// Parameters:
//   X, Y - normalized position (0.0-1.0)
//   W    - normalized width
//
// Example:
//   WSEPARATOR(0.1, 0.5, 0.8)

#define WSEPARATOR(X, Y, W) \
	itemDef { \
		rect X Y W WM_SEPARATOR_H \
		type 0 \
		style 1 \
		backcolor WCOLOR_SEPARATOR \
		visible 1 \
		decoration \
		grow 0 \
	}

// ── yes/no toggle ──────────────────────────────────────────────────
// Bound to a cvar (0/1). Type 11.
//
// Parameters:
//   X, Y     - normalized position (0.0-1.0)
//   W, H     - normalized width and height
//   LABEL    - display label (quoted)
//   FONTSIZE - point size
//   CVAR     - cvar name to bind (quoted, e.g., "cg_drawFPS")
//
// Example:
//   WYESNO(0.1, 0.3, 0.4, WM_BUTTON_H, "Show FPS:", WM_FONT_BODY, "cg_drawFPS")

#define WYESNO(X, Y, W, H, LABEL, FONTSIZE, CVAR) \
	itemDef { \
		rect X Y W H \
		type 11 \
		text LABEL \
		font "oxanium" FONTSIZE \
		forecolor WCOLOR_TEXT \
		cvar CVAR \
		visible 1 \
		grow 0 \
		mouseEnter { setcolor forecolor WCOLOR_ACCENT_HOVER } \
		mouseExit { setcolor forecolor WCOLOR_TEXT } \
	}

// ── slider control ─────────────────────────────────────────────────
// Bound to a cvar with min/max/step range. Type 10.
//
// Parameters:
//   X, Y     - normalized position (0.0-1.0)
//   W, H     - normalized width and height
//   LABEL    - display label (quoted)
//   FONTSIZE - point size
//   CVAR     - cvar name to bind (quoted)
//   MIN      - minimum value (float)
//   MAX      - maximum value (float)
//   STEP     - step increment (float)
//
// Example:
//   WSLIDER(0.1, 0.4, 0.4, WM_BUTTON_H, "Volume:", WM_FONT_BODY, "s_volume", 0, 1, 0.1)

#define WSLIDER(X, Y, W, H, LABEL, FONTSIZE, CVAR, MIN, MAX, STEP) \
	itemDef { \
		rect X Y W H \
		type 10 \
		text LABEL \
		font "oxanium" FONTSIZE \
		forecolor WCOLOR_TEXT \
		cvar CVAR \
		cvarFloat CVAR MIN MIN MAX \
		visible 1 \
		grow 0 \
		mouseEnter { setcolor forecolor WCOLOR_ACCENT_HOVER } \
		mouseExit { setcolor forecolor WCOLOR_TEXT } \
	}

// ── multi-choice selector ──────────────────────────────────────────
// Dropdown list of cvar values. Type 12.
//
// Parameters:
//   X, Y     - normalized position (0.0-1.0)
//   W, H     - normalized width and height
//   LABEL    - display label (quoted)
//   FONTSIZE - point size
//   CVAR     - cvar name to bind (quoted)
//   LIST     - { "label1" value1 "label2" value2 ... } block
//
// Example:
//   WMULTI(0.1, 0.5, 0.4, WM_BUTTON_H, "Quality:", WM_FONT_BODY, "r_picmip", { "High" 0 "Medium" 1 "Low" 2 })

#define WMULTI(X, Y, W, H, LABEL, FONTSIZE, CVAR, LIST) \
	itemDef { \
		rect X Y W H \
		type 12 \
		text LABEL \
		font "oxanium" FONTSIZE \
		forecolor WCOLOR_TEXT \
		cvar CVAR \
		cvarStrList LIST \
		visible 1 \
		grow 0 \
		mouseEnter { setcolor forecolor WCOLOR_ACCENT_HOVER } \
		mouseExit { setcolor forecolor WCOLOR_TEXT } \
	}

// ── key binding ────────────────────────────────────────────────────
// Binds a key to a command. Type 13.
//
// Parameters:
//   X, Y     - normalized position (0.0-1.0)
//   W, H     - normalized width and height
//   LABEL    - display label (quoted)
//   FONTSIZE - point size
//   COMMAND  - command string to bind (quoted, e.g., "+attack")
//
// Example:
//   WBIND(0.1, 0.3, 0.4, WM_BUTTON_H, "Fire:", WM_FONT_BODY, "+attack")

#define WBIND(X, Y, W, H, LABEL, FONTSIZE, COMMAND) \
	itemDef { \
		rect X Y W H \
		type 13 \
		text LABEL \
		font "oxanium" FONTSIZE \
		forecolor WCOLOR_TEXT \
		cvar COMMAND \
		visible 1 \
		grow 0 \
		mouseEnter { setcolor forecolor WCOLOR_ACCENT_HOVER } \
		mouseExit { setcolor forecolor WCOLOR_TEXT } \
	}

// ── text input field ───────────────────────────────────────────────
// Editable text bound to a cvar. Type 4.
//
// Parameters:
//   X, Y     - normalized position (0.0-1.0)
//   W, H     - normalized width and height
//   LABEL    - display label (quoted)
//   FONTSIZE - point size
//   CVAR     - cvar name to bind (quoted)
//   MAXCHARS - maximum number of input characters (integer)
//
// Example:
//   WEDITFIELD(0.1, 0.3, 0.4, WM_BUTTON_H, "Name:", WM_FONT_BODY, "name", 32)

#define WEDITFIELD(X, Y, W, H, LABEL, FONTSIZE, CVAR, MAXCHARS) \
	itemDef { \
		rect X Y W H \
		type 4 \
		text LABEL \
		font "oxanium" FONTSIZE \
		forecolor WCOLOR_TEXT \
		cvar CVAR \
		maxChars MAXCHARS \
		visible 1 \
		grow 0 \
		mouseEnter { setcolor forecolor WCOLOR_ACCENT_HOVER } \
		mouseExit { setcolor forecolor WCOLOR_TEXT } \
	}

// ── subwindow panel title ──────────────────────────────────────────
// Section container heading. Uses sansman heading font.
// Height is fixed at 0.033 regardless of the H parameter (reserved for future use).
//
// Parameters:
//   X, Y  - normalized position (0.0-1.0)
//   W, H  - normalized width (H is currently unused, fixed at 0.033)
//   TITLE - heading text (quoted)
//
// Example:
//   WSUBWINDOW(0.05, 0.1, 0.4, 0.04, "Settings")

#define WSUBWINDOW(X, Y, W, H, TITLE) \
	itemDef { \
		rect X Y W 0.033 \
		type 0 \
		text TITLE \
		textalign 0 \
		font "sansman" WM_FONT_HEADING \
		forecolor WCOLOR_SECTION \
		visible 1 \
		decoration \
		grow 0 \
	}

// ── section header ─────────────────────────────────────────────────
// Smaller than subwindow title. Dimmed accent color. Height is WM_LABEL_H.
//
// Parameters:
//   X, Y - normalized position (0.0-1.0)
//   W    - normalized width
//   TEXT - header text (quoted)
//
// Example:
//   WSECTION_HEADER(0.1, 0.25, 0.3, "Graphics")

#define WSECTION_HEADER(X, Y, W, TEXT) \
	itemDef { \
		rect X Y W WM_LABEL_H \
		type 0 \
		text TEXT \
		textalign 0 \
		font "sansman" WM_FONT_LABEL \
		forecolor WCOLOR_ACCENT_DIM \
		visible 1 \
		decoration \
		grow 0 \
	}

#endif // WMENUMACROS_H
