#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI



typedef struct
{
	superhudConfig_t config;
	int timePrev;
	superhudTextContext_t ctx;
} shudElementWarmupInfo_t;

void* CG_SHUDElementWarmupInfoCreate(const superhudConfig_t* config)
{
	shudElementWarmupInfo_t* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_SHUDElementWarmupInfoRoutine(void* context)
{
	shudElementWarmupInfo_t* element = (shudElementWarmupInfo_t*)context;
	char str[256];

	int sec = cg.warmup;

	element->ctx.text = NULL;

	if (sec < 0)
	{
		{
			wuiStoreEntry_t *e = WiredStore_Get( "game.warmup.message" );
			element->ctx.text = ( e && e->text[0] ) ? e->text : "^BWaiting for Players";
		}
	}
	else if (sec > 0)
	{
		if (cg.showScores == 0)
		{
			sec = (sec - cg.time) / 1000;
			if (sec < 0)
			{
				cg.warmup = 0;
				sec = 0;
			}
			Com_sprintf(str, 256, "Starts in: %i", sec + 1);
			element->ctx.text = str;
		}
	}
	if (!element->ctx.text)
	{
		return;
	}
	CG_SHUDTextPrint(&element->config, &element->ctx);
}

void CG_SHUDElementWarmupInfoDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}

#endif // FEAT_WIRED_UI
