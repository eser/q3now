#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	superhudConfig_t config;
	superhudBarContext_t ctx;
} shudElementStatusbarArmorBar;

void* CG_SHUDElementSBABCreate(const superhudConfig_t* config)
{
	shudElementStatusbarArmorBar* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDBarMakeContext(&element->config, &element->ctx, 200);

	return element;
}

void CG_SHUDElementSBABRoutine(void* context)
{
	shudElementStatusbarArmorBar* element = (shudElementStatusbarArmorBar*)context;
	float ap = cg.snap->ps.stats[STAT_ARMOR];

	CG_SHUDFill(&element->config);
	CG_SHUDDrawBorder(&element->config);

	if (element->config.style.value == 1)
	{
		CG_ColorForHealth(element->ctx.color_top);
	}
	else if (element->config.style.value == 2)
	{
		// Поведение как в style 3, но без isHealthLow
		Vector4Copy(element->config.color.value.rgba, element->ctx.color_top);

		if (!element->config.color2.isSet)
		{
			Vector4Copy(element->ctx.color_top, element->ctx.color2_top);
			element->ctx.color2_top[3] *= 3;
		}
		else
		{
			Vector4Copy(element->config.color2.value.rgba, element->ctx.color2_top);
		}
	}
	else if (element->config.style.value == 3)
	{
		if (qfalse)
		{
			CG_ColorForHealth(element->ctx.color_top);
			element->ctx.color_top[3] = element->config.color.value.rgba[3];

			if (!element->config.color2.isSet)
			{
				Vector4Copy(element->ctx.color_top, element->ctx.color2_top);
				element->ctx.color2_top[3] *= 3;
			}
			else
			{
				CG_ColorForHealth(element->ctx.color2_top);
				element->ctx.color2_top[3] = element->config.color2.value.rgba[3];
			}
		}
		else
		{
			Vector4Copy(element->config.color.value.rgba, element->ctx.color_top);

			if (!element->config.color2.isSet)
			{
				Vector4Copy(element->ctx.color_top, element->ctx.color2_top);
				element->ctx.color2_top[3] *= 3;
			}
			else
			{
				Vector4Copy(element->config.color2.value.rgba, element->ctx.color2_top);
			}
		}
	}

	CG_SHUDBarPrint(&element->config, &element->ctx, ap);
}



void CG_SHUDElementSBABDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}

#endif // FEAT_WIRED_UI
