// cg_superhud_element_sbai.c — Status bar armor icon
// Uses STAT_ARMORCLASS to pick the correct armor tier icon
#include "cg_local.h"
#include "cg_superhud_private.h"

typedef struct
{
	superhudConfig_t config;
	superhudDrawContext_t ctx;
	qboolean hasCustomImage;
} shudElementStatusbarArmorIcon;

void* CG_SHUDElementSBAICreate(const superhudConfig_t* config)
{
	shudElementStatusbarArmorIcon* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDDrawMakeContext(&element->config, &element->ctx);

	if (config->image.isSet)
	{
		element->ctx.image = trap_R_RegisterShader(element->config.image.value);
		element->hasCustomImage = qtrue;
	}
	else
	{
		element->hasCustomImage = qfalse;
	}

	return element;
}

void CG_SHUDElementSBAIRoutine(void* context)
{
	shudElementStatusbarArmorIcon* element = (shudElementStatusbarArmorIcon*)context;

	if (cg.snap->ps.stats[STAT_ARMOR] <= 0)
		return;

	if (!element->hasCustomImage)
	{
		switch (cg.snap->ps.stats[STAT_ARMORCLASS])
		{
			case ARM_HEAVY:
				element->ctx.image = cgs.media.heavyArmorIcon;
				break;
			case ARM_COMBAT:
				element->ctx.image = cgs.media.combatArmorIcon;
				break;
			case ARM_JACKET:
				element->ctx.image = cgs.media.jacketArmorIcon;
				break;
			default:
				element->ctx.image = cgs.media.combatArmorIcon;
				break;
		}
	}

	CG_SHUDFill(&element->config);
	CG_SHUDDrawBorder(&element->config);
	CG_SHUDDrawStretchPicCtx(&element->config, &element->ctx);
}

void CG_SHUDElementSBAIDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
