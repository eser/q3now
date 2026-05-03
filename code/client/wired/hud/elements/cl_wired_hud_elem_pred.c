#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	modernhudDrawContext_t drawCtx;
	modernhudTextContext_t textCtx;
} modernHudElementStatusbarDecorate;

void* CG_ModernHUDElementDecorCreate(const modernhudConfig_t* config)
{
	modernHudElementStatusbarDecorate* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDDrawMakeContext(&element->config, &element->drawCtx);

	if (config->image.isSet)
	{
		element->drawCtx.image = trap_R_RegisterShader(config->image.value);
		if (!element->drawCtx.image)
		{
			Com_Log( SEV_INFO, LOG_CAT_UI, "^2Decorate image %s is not found\n", config->image.value);
		}
	}

	if (config->text.isSet)
	{
		CG_ModernHUDTextMakeContext(&element->config, &element->textCtx);
		element->textCtx.text = config->text.value;
		CG_ModernHUDFillAndFrameForText(&element->config, &element->textCtx);
	}

	return element;
}

void CG_ModernHUDElementDecorRoutine(void* context)
{
	modernHudElementStatusbarDecorate* element = (modernHudElementStatusbarDecorate*)context;

	if (!CG_ModernHUDFill(&element->config))
	{
		if (element->drawCtx.image)
		{
			CG_ModernHUDDrawStretchPicCtx(&element->config, &element->drawCtx);
		}
		if (element->textCtx.text)
		{
			CG_ModernHUDTextPrint(&element->config, &element->textCtx);
		}
	}

	CG_ModernHUDDrawBorder(&element->config);
}

#endif // FEAT_WIRED_UI
