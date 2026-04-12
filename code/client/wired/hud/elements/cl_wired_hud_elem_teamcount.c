#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
	qboolean enemy;
} modernHudElementStatusbarTeamCount;

void* CG_ModernHUDElementTeamCountCreate(const modernhudConfig_t* config, qboolean enemy)
{
	modernHudElementStatusbarTeamCount* element;

	ModernHUD_ELEMENT_INIT(element, config);

	//load defaults
	if (!element->config.color.isSet)
	{
		element->config.color.isSet = qtrue;
		element->config.color.value.type = MODERNHUD_COLOR_RGBA;
		Vector4Set(element->config.color.value.rgba, 1, 0.7, 0, 1);
	}

	if (!element->config.text.isSet)
	{
		element->config.text.isSet = qtrue;
		Q_strncpyz(element->config.text.value, "%i", sizeof(element->config.text.value));
	}

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	element->enemy = enemy;

	return element;
}

void* CG_ModernHUDElementTeamCountOWNCreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementTeamCountCreate(config, qfalse);
}

void* CG_ModernHUDElementTeamCountNMECreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementTeamCountCreate(config, qtrue);
}

void CG_ModernHUDElementTeamCountRoutine(void* context)
{
	modernHudElementStatusbarTeamCount* element = (modernHudElementStatusbarTeamCount*)context;
	int count;

	if ( !wiredHud->isTeamGame ) {
		return;
	}

	/* cgame pre-computes own and enemy team counts each frame */
	count = element->enemy ? wiredHud->enemyTeamCount : wiredHud->ownTeamCount;

	if (count >= 0)
	{
		element->ctx.text = va(element->config.text.value, count);
		CG_ModernHUDTextPrint(&element->config, &element->ctx);
	}

}

void CG_ModernHUDElementTeamCountDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}

#endif // FEAT_WIRED_UI
