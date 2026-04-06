#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI



typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
} shudElementTargetName_t;

void* CG_SHUDElementTargetNameCreate(const superhudConfig_t* config)
{
	shudElementTargetName_t* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_SHUDElementTargetNameRoutine(void* context)
{
	shudElementTargetName_t* element = (shudElementTargetName_t*)context;
	char    s[1024];
	clientInfo_t* ci;
	int clientNum;

	if ( wiredHud->crosshair.shaderIndex < 0 ) return;  // crosshair hidden
	if ( wiredHud->renderingThirdPerson ) return;
	if ( wiredHud->crosshairClientTime == 0 ) return;

	if ( !CG_SHUDGetFadeColor( element->ctx.color_origin, element->ctx.color,
			&element->config, wiredHud->crosshairClientTime ) )
	{
		return;
	}

	clientNum = wiredHud->crosshairClientNum;
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) return;

	ci = &wired_cgs.clientinfo[clientNum];
	if ( !ci->infoValid ) return;

	// team-only mode: hide enemy names in team games (pre-computed by cgame)
	{
		wuiStoreEntry_t *e = WiredStore_Get( "crosshair.showName" );
		if ( e && (int)e->value == 0 ) return;
	}

	Com_sprintf( s, sizeof(s), "%s", ci->name );
	element->ctx.text = s;
	CG_SHUDTextPrint( &element->config, &element->ctx );
	element->ctx.text = NULL;
}

void CG_SHUDElementTargetNameDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}

#endif // FEAT_WIRED_UI
