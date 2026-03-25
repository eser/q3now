#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI




typedef struct
{
	superhudConfig_t config;
	superhudDrawContext_t drawCtx;
	superhudTextContext_t textCtx;
} shudElementStatusbarDecorate;

void* CG_SHUDElementDecorCreate(const superhudConfig_t* config)
{
	shudElementStatusbarDecorate* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDDrawMakeContext(&element->config, &element->drawCtx);

	if (config->image.isSet)
	{
		element->drawCtx.image = trap_R_RegisterShader(config->image.value);
		if (!element->drawCtx.image)
		{
			CG_Printf("^2Decorate image %s is not found\n", config->image.value);
		}
	}

	if (config->text.isSet)
	{
		CG_SHUDTextMakeContext(&element->config, &element->textCtx);
		element->textCtx.text = config->text.value;
		CG_SHUDFillAndFrameForText(&element->config, &element->textCtx);
	}

	return element;
}

void CG_SHUDElementDecorRoutine(void* context)
{
	shudElementStatusbarDecorate* element = (shudElementStatusbarDecorate*)context;

	if (!CG_SHUDFill(&element->config))
	{
		if (element->drawCtx.image)
		{
			CG_SHUDDrawStretchPicCtx(&element->config, &element->drawCtx);
		}
		if (element->textCtx.text)
		{
			CG_SHUDTextPrint(&element->config, &element->textCtx);
		}
	}

	CG_SHUDDrawBorder(&element->config);
}

void CG_SHUDElementDecorDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}

#endif // FEAT_WIRED_UI
