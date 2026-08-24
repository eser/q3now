// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// elements/viewport.c — WiredUI `type viewport` itemDef render handler.
//
// The compositor walk (cl_wired_clay.c) emits a Clay CUSTOM command with
// "viewport:<id>" in cmd->name when it hits an ITEM_TYPE_VIEWPORT itemDef.
// The CUSTOM dispatch routes "viewport:" prefixed commands here.
//
// Lookup hit, host-side provider  → invoke provider->render( rect_norm,
//               provider->userdata ) (attract-mode bg / demo viewer).
// Lookup hit, VM-routed provider   → enter the cgame via the VM_Call ABI
//               (CL_RenderCGameViewport → VM_Call CG_RENDER_VIEWPORT). The
//               cgame world scene is VM-routed: a raw fn-ptr deref of VM-space
//               code is invalid and silently never reached the cgame body —
//               that was the black-screen defect. The VM_Call lives in
//               cl_cgame.c so this engine file stays free of cgame headers
//               (CLAUDE.md one-way dependency).
// Lookup miss → SEV_WARN (rate-limited per id) + visible red rect placeholder
//               so missing providers are obvious in dev builds without
//               crashing the whole panel.

#include "../../../client.h"
#include "../cl_wired_ui.h"
#include "../cl_wired_viewport.h"
#include "../cl_wired_draw.h"

LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

void WiredUI_RenderViewport( float x, float y, float w, float h,
                              const char *viewportId )
{
	const wuiViewportProvider_t *prov;
	wuiViewportRect_t            rectNorm;
	float                        invW, invH;
	const vec4_t                 redPlaceholder = { 1.0f, 0.0f, 0.0f, 0.5f };

	if ( w <= 0.0f || h <= 0.0f ) return;
	if ( !viewportId || !viewportId[ 0 ] ) {
		COM_WARN( LOG_CH( ch_ui ),
			"WiredUI_RenderViewport: viewport itemDef missing `id`\n" );
		WUI_FillRect( x, y, w, h, redPlaceholder );
		re.SetColor( NULL );
		return;
	}

	prov = WiredUI_FindViewportProvider( viewportId );

	if ( !prov || ( !prov->is_vm_routed && !prov->render ) ) {
		COM_WARN( LOG_CH( ch_ui ),
			"WiredUI_RenderViewport: no provider for id '%s' — drawing placeholder\n",
			viewportId );
		WUI_FillRect( x, y, w, h, redPlaceholder );
		re.SetColor( NULL );
		return;
	}

	/* Provider rect is normalized [0,1] relative to the swapchain. The
	 * compositor passes us pixel coords (real screen space), so convert
	 * back via the cached glconfig dims that already gate the emit. */
	invW = ( cls.glconfig.vidWidth  > 0 ) ? 1.0f / (float) cls.glconfig.vidWidth  : 0.0f;
	invH = ( cls.glconfig.vidHeight > 0 ) ? 1.0f / (float) cls.glconfig.vidHeight : 0.0f;
	rectNorm.x = x * invW;
	rectNorm.y = y * invH;
	rectNorm.w = w * invW;
	rectNorm.h = h * invH;

	if ( prov->is_vm_routed ) {
		/* cgame VM provider: enter the cgame via the VM_Call ABI. A raw
		 * fn-ptr deref of VM-space code is invalid (the black-screen defect).
		 * The VM_Call itself lives in cl_cgame.c (CL_RenderCGameViewport) so
		 * this engine file stays free of cgame headers. Route to the OWNING
		 * app's cgame VM (the owner captured at register time) so each app's
		 * viewport renders its own scene, not the focused app's. */
		CL_PROF( cgr, CL_RenderCGameViewport(
			(void *)WiredUI_FindViewportOwner( viewportId ), prov->vm_key,
			(int)x, (int)y, (int)w, (int)h ) );
	} else if ( prov->render ) {
		/* host-side provider (attract-mode bg / demo viewer). */
		prov->render( &rectNorm, prov->userdata );
	}
}

#endif /* FEAT_WIRED_UI */
