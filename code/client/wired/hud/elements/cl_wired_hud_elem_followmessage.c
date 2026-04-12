#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementFollowMessage_t;

void* CG_ModernHUDElementFollowMessageCreate(const modernhudConfig_t* config)
{
	modernHudElementFollowMessage_t* element;

	ModernHUD_ELEMENT_INIT(element, config);


	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_ModernHUDElementFollowMessageRoutine(void* context)
{
	modernHudElementFollowMessage_t* element = (modernHudElementFollowMessage_t*)context;
	const char* str;

	if (!CG_IsFollowing())
	{
		return;
	}

	str = cgs.clientinfo[cg.snap->ps.clientNum].name;
	element->ctx.text = va("Following ^7%s", str);

	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

void CG_ModernHUDElementFollowMessageDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
#endif // FEAT_WIRED_UI
