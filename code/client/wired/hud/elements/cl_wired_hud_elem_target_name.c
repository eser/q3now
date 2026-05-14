// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementTargetName_t;

void* CG_ModernHUDElementTargetNameCreate(const modernhudConfig_t* config)
{
	modernHudElementTargetName_t* element;

	WHUD_ELEMENT_INIT_TEXT( element, config );

	return element;
}

void CG_ModernHUDElementTargetNameRoutine(void* context)
{
	modernHudElementTargetName_t* element = (modernHudElementTargetName_t*)context;
	char    s[1024];

	if ( wiredHud->crosshair.shaderIndex < 0 ) return;  // crosshair hidden
	if ( wiredHud->renderingThirdPerson ) return;
	if ( wiredHud->crosshairClientTime == 0 ) return;

	if ( !CG_ModernHUDGetFadeColor( element->ctx.color_origin, element->ctx.color,
			&element->config, wiredHud->crosshairClientTime ) )
	{
		return;
	}

	int clientNum = wiredHud->crosshairClientNum;
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) return;

	clientInfo_t* ci = &wired_cgs.clientinfo[clientNum];
	if ( !ci->infoValid ) return;

	// team-only mode: hide enemy names in team games (pre-computed by cgame)
	{
		wuiStoreEntry_t *e = WiredStore_Get( "crosshair.showName" );
		if ( e && (int)e->value == 0 ) return;
	}

	Com_sprintf( s, sizeof(s), "%s", ci->name );
	element->ctx.text = s;
	CG_ModernHUDTextPrint( &element->config, &element->ctx );
	element->ctx.text = NULL;
}

#endif // FEAT_WIRED_UI
