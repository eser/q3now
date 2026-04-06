#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
	qboolean enemy;
} shudElementStatusbarTeamCount;

void* CG_SHUDElementTeamCountCreate(const superhudConfig_t* config, qboolean enemy)
{
	shudElementStatusbarTeamCount* element;

	SHUD_ELEMENT_INIT(element, config);

	//load defaults
	if (!element->config.color.isSet)
	{
		element->config.color.isSet = qtrue;
		element->config.color.value.type = SUPERHUD_COLOR_RGBA;
		Vector4Set(element->config.color.value.rgba, 1, 0.7, 0, 1);
	}

	if (!element->config.text.isSet)
	{
		element->config.text.isSet = qtrue;
		Q_strncpyz(element->config.text.value, "%i", sizeof(element->config.text.value));
	}

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	element->enemy = enemy;

	return element;
}

void* CG_SHUDElementTeamCountOWNCreate(const superhudConfig_t* config)
{
	return CG_SHUDElementTeamCountCreate(config, qfalse);
}

void* CG_SHUDElementTeamCountNMECreate(const superhudConfig_t* config)
{
	return CG_SHUDElementTeamCountCreate(config, qtrue);
}

void CG_SHUDElementTeamCountRoutine(void* context)
{
	shudElementStatusbarTeamCount* element = (shudElementStatusbarTeamCount*)context;
	int count;

	if ( !wiredHud->isTeamGame ) {
		return;
	}

	/* cgame pre-computes own and enemy team counts each frame */
	count = element->enemy ? wiredHud->enemyTeamCount : wiredHud->ownTeamCount;

	if (count >= 0)
	{
		element->ctx.text = va(element->config.text.value, count);
		CG_SHUDTextPrint(&element->config, &element->ctx);
	}

}

void CG_SHUDElementTeamCountDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}

#endif // FEAT_WIRED_UI
