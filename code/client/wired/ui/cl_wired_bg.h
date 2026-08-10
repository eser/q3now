// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_bg.h — Wired UI 6-layered background compositor extension

Author-driven background composition: each container can request a
subset of six layers (base/grid/scanlines/glow_rays/noise/vignette)
via the modder-facing `background "layered" effects "<flags>"` keyword.
Compositor emit walks the active layer bitmask in painter's order and
issues primitive draws into the surrounding Clay tree. Each layer is a
fixed-shape pattern; modders cannot author new layer types — they
opt-in to subsets of the canonical six.

Performance budget: aggregate per-frame ≤0.5ms on Tier 1 GPU. Most
layers reduce to a handful of CLAY rectangles (grid lines, scanlines,
corner glows, vignette gradient quads). Noise + scanlines pre-bake on
first call; subsequent frames just re-issue the cached CLAY blocks.
*/

#ifndef CL_WIRED_BG_H
#define CL_WIRED_BG_H

#include "../../../qcommon/q_feats.h"

#if FEAT_WIRED_UI

#include "../../../qcommon/q_shared.h"

// ── Layer bitmask ───────────────────────────────────────────────────
#define WUI_BG_LAYER_BASE              0x001
#define WUI_BG_LAYER_GRID              0x002
#define WUI_BG_LAYER_SCANLINES         0x004
#define WUI_BG_LAYER_GLOW_RAYS         0x008
#define WUI_BG_LAYER_NOISE             0x010
#define WUI_BG_LAYER_VIGNETTE          0x020

/* v2 design DemoBackdrop layers — composable additive painter's order
 * after the legacy six. Per-layer renderers consume the 15.2-LANDED v2
 * palette token values baked into the file-static colour table; the
 * dynamic mode/accent swap path stays as a future extension (current
 * cl_wired_palette overlay rewrites `_tokens.wui` for the parser but
 * doesn't notify already-loaded engine modules). */
#define WUI_BG_LAYER_SKY               0x040   /* SKY_GRADIENT_RADIAL */
#define WUI_BG_LAYER_ARENA_SILHOUETTE  0x080   /* 2D arena geometry */
#define WUI_BG_LAYER_EMBERS            0x100   /* 14 dot particles, sin-oscillating */
#define WUI_BG_LAYER_FOG               0x200   /* linear top-to-mid fog */
#define WUI_BG_LAYER_PLASMA            0x400   /* GLOW_PLASMA (PlasmaBG primitive) */

/* Modifiers — NOT layers; bitwise-OR'd alongside layer flags to
 * customise per-layer behaviour. PARALLAX shifts SKY/ARENA/EMBERS in
 * X by a time-driven offset; ANIMATED gates the qwplasma-style cycle
 * on PLASMA. */
#define WUI_BG_MODIFIER_PARALLAX       0x1000
#define WUI_BG_MODIFIER_ANIMATED       0x2000

#define WUI_BG_LAYER_ALL        ( WUI_BG_LAYER_BASE      \
                                | WUI_BG_LAYER_GRID      \
                                | WUI_BG_LAYER_SCANLINES \
                                | WUI_BG_LAYER_GLOW_RAYS \
                                | WUI_BG_LAYER_NOISE     \
                                | WUI_BG_LAYER_VIGNETTE )

#define WUI_BG_DEMO_BACKDROP    ( WUI_BG_LAYER_SKY              \
                                | WUI_BG_LAYER_ARENA_SILHOUETTE \
                                | WUI_BG_LAYER_EMBERS           \
                                | WUI_BG_LAYER_FOG              \
                                | WUI_BG_LAYER_VIGNETTE         \
                                | WUI_BG_LAYER_NOISE            \
                                | WUI_BG_MODIFIER_PARALLAX )

// ── Push-time background intent ─────────────────────────────────────
/* How a menu's background is emitted is decided by HOW the menu was
 * pushed (its intent), not baked into the .wui. WiredUI_PushMenu records
 * one of these per stack entry; the compositor resolves it against the
 * item's authored bgLayerFlags at emit time. INHERIT == 0 so any
 * zero-initialized / unspecified path reproduces today's behavior. */
typedef enum {
	WUI_BG_INTENT_INHERIT = 0,  /* use the menu's .wui-authored bgLayerFlags (default) */
	WUI_BG_INTENT_SCENE,        /* full scene (Phase 1: == authored flags; Phase 2: parallax) */
	WUI_BG_INTENT_DIM,          /* dark alpha overlay only — over live gameplay */
	WUI_BG_INTENT_NONE          /* emit no background at all */
} wuiBgIntent_t;

// ── Emit API ────────────────────────────────────────────────────────

/* Emit one layered background pass on top of an already-opened CLAY
 * parent block. `xy_wh_pixels` is the viewport-pixel rect the background
 * covers (typically the menu's resolvedRect); `flags` selects layers
 * from the WUI_BG_LAYER_* bitmask. Sequential painter's order: base →
 * grid → scanlines → glow_rays → noise → vignette. */
void WUI_DrawBackgroundLayered( float x, float y, float w, float h, int flags );

/* DIM intent overlay: a single dark alpha wash covering (x,y,w,h),
 * emitted INSTEAD of the layered background so live gameplay behind the
 * menu shows through. Matches the in-game scrim alpha (~0.55). */
void WUI_DrawBackgroundDim( float x, float y, float w, float h );

/* SCENE intent: the full depth-parallax DemoBackdrop scene. Distant layers
 * (sky) barely move; near layers (embers/plasma) lead. Responds to the mouse
 * (smoothed) and to menu transitions (one-shot nudge). Emitted for a menu
 * pushed WUI_BG_INTENT_SCENE regardless of its authored effects. */
void WUI_DrawBackgroundScene( float x, float y, float w, float h );

/* R1 SCENE-as-shader seam. WUI_DrawBackgroundScene emits a single backmost
 * custom command (WUI_EmitSceneBackdrop, defined in cl_wired_clay.c where the
 * scratch arena lives); its dispatch reads WUI_SceneBackdropParams (defined in
 * cl_wired_bg.c) for the continuous time / smoothed cursor / transition it
 * hands to re.DrawMenuBackdrop. Split this way so the parallax-state smoothing
 * stays in cl_wired_bg.c and the Clay emit stays in cl_wired_clay.c. */
/* Backmost z-index for every WiredUI background (rect DIM/INHERIT layers AND
 * the R1 procedural SCENE pass) — a negative index pins them strictly behind the
 * zIndex-0 content root, so the background is always the backmost layer
 * regardless of emit order. Shared by cl_wired_bg.c (rect emits) and
 * cl_wired_clay.c (the SCENE backdrop custom command). */
#ifndef WUI_BG_SCENE_ZINDEX
#define WUI_BG_SCENE_ZINDEX  (-10)
#endif

void WUI_EmitSceneBackdrop( float x, float y, float w, float h );
void WUI_SceneBackdropParams( float *outTime, float *outMouseX,
                              float *outMouseY, float *outTransition );

/* Kick the one-shot menu-transition parallax nudge (call on a SCENE menu
 * push/pop). Safe to call when no scene is showing (just arms an anim). */
void WiredUI_NotifyBgTransition( void );

/* Parse a comma-or-plus separated flag string from the modder-facing
 * keyword (`"base+grid+scanlines+..."` or just `"full"`/`"minimal"`).
 * Returns the OR'd bitmask. Unknown flag names log SEV_WARN once. */
int  WUI_BackgroundParseFlags( const char *spec );

#endif /* FEAT_WIRED_UI */
#endif /* CL_WIRED_BG_H */
