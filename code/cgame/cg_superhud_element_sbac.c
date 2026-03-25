#include "cg_local.h"
#include "cg_superhud_private.h"


typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
} shudElementStatusbarArmorCount;

void* CG_SHUDElementSBACCreate(const superhudConfig_t* config)
{
	shudElementStatusbarArmorCount* element;

	SHUD_ELEMENT_INIT(element, config);

	//load defaults
	// if (!element->config.color.isSet)
	// {
	//  element->config.color.isSet = qtrue;
	//  element->config.color.value.type = SUPERHUD_COLOR_RGBA;
	//  Vector4Set(element->config.color.value.rgba, 1, 0.7, 0, 1);
	// }

	if (!element->config.text.isSet)
	{
		element->config.text.isSet = qtrue;
		Q_strncpyz(element->config.text.value, "%i", sizeof(element->config.text.value));
	}

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);
	element->ctx.flags |= DS_FORCE_COLOR;

	return element;
}

void CG_SHUDElementSBACRoutine(void* context)
{
	shudElementStatusbarArmorCount* element = (shudElementStatusbarArmorCount*)context;
	int ap = cg.snap->ps.stats[STAT_ARMOR];

	if (ap <= 0 && !SHUD_CHECK_SHOW_EMPTY(element))
		return;

	element->ctx.text = va(element->config.text.value, ap > 0 ? ap : 0);

	if (ap <= 0 && element->config.color.isSet)
		Vector4Copy(element->config.color.value.rgba, element->ctx.color);
	else
	CG_GetColorForAmount( cg.snap->ps.stats[STAT_HEALTH], cg.snap->ps.stats[STAT_ARMOR], element->ctx.color );
	CG_SHUDTextPrintNew(&element->config, &element->ctx, qfalse);
}

void CG_SHUDElementSBACDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
