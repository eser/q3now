// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// qcommon/wired/ui_viewport_types.h — V-16 shared viewport ABI types.
//
// The engine-side WiredUI viewport-provider registry (code/client/wired/ui/
// cl_wired_viewport.{c,h}) and the cgame VM's CG_REGISTER_VIEWPORT_PROVIDER
// syscall (code/cgame/cg_public.h slot 222) both deref the same struct
// layout when register fires. To keep the engine clean of cgame headers
// (CLAUDE.md one-way dependency rule), the public type lives here in
// qcommon/wired/ — both sides include this header, neither side learns
// the other's internal layout.

#ifndef QCOMMON_WIRED_UI_VIEWPORT_TYPES_H
#define QCOMMON_WIRED_UI_VIEWPORT_TYPES_H

#include "../q_shared.h"

typedef struct wuiViewportRect_s {
	float x, y, w, h;       /* normalized [0,1] (relative to swapchain) */
} wuiViewportRect_t;

typedef enum {
	WUI_VIEWPORT_LIFETIME_PROCESS,
	WUI_VIEWPORT_LIFETIME_LEVEL,
	WUI_VIEWPORT_LIFETIME_FRAME
} wuiViewportLifetime_t;

typedef enum {
	WUI_VIEWPORT_INPUT_MODAL_CGAME,     /* input forwarded to the cgame VM */
	WUI_VIEWPORT_INPUT_PASSIVE_DISPLAY, /* no input dispatch (demo viewer) */
	WUI_VIEWPORT_INPUT_INTERACTIVE_UI   /* UI events route normally (future) */
} wuiViewportInputMode_t;

/* VM-routed viewport keys: the integer the engine passes to the cgame
 * CG_RENDER_VIEWPORT export so cgame dispatches per in-game-UI viewport.
 * APPEND only (ABI). main_scene is the first; future: minimap, mirror, etc. */
typedef enum {
	WUI_VIEWPORT_KEY_MAIN_SCENE = 0,
	WUI_VIEWPORT_KEY_CAMERA     = 1   /* observer view of the same world from a fixed/chase camera */
} wuiViewportKey_t;

typedef struct wuiViewportProvider_s {
	void                   (*render)( const wuiViewportRect_t *rect, void *userdata );
	wuiViewportLifetime_t  lifetime;
	wuiViewportInputMode_t input_mode;
	void                   *userdata;
	qboolean               is_vm_routed;   /* qtrue: ignore `render`; engine issues
	                                           VM_Call(cgvm, CG_RENDER_VIEWPORT, vm_key).
	                                           qfalse: host-side provider, call `render`
	                                           directly (attract-mode bg / demo viewer). */
	int                    vm_key;         /* wuiViewportKey_t when is_vm_routed; else 0 */
} wuiViewportProvider_t;

/* V-20 (2026-05-31): per-frame scene context, pulled by a world-viewport
 * provider's render callback via the CG_GET_SCENE_FRAME_CONTEXT syscall
 * (cg_public.h slot 224). These are the three frame-global inputs the world
 * scene render consumes — previously passed by the engine's direct
 * CL_CGameRendering call (VM_Call CG_DRAW_ACTIVE_FRAME with cl.serverTime /
 * STEREO_CENTER / clc.demoplaying). The provider now pulls them instead, so
 * the engine no longer pushes frame state down through the render contract
 * (charter pull model). Stored as plain int (not stereoFrame_t) to keep this
 * VM-shared header free of renderer-type includes; the callback casts `stereo`
 * back to stereoFrame_t. This struct is VM-shared ABI: APPEND fields only. */
typedef struct wuiSceneFrameCtx_s {
	int serverTime;    /* cl.serverTime — may be paused during play */
	int stereo;        /* stereoFrame_t value (STEREO_CENTER/LEFT/RIGHT) */
	int demoPlayback;  /* clc.demoplaying */
} wuiSceneFrameCtx_t;

#endif /* QCOMMON_WIRED_UI_VIEWPORT_TYPES_H */
