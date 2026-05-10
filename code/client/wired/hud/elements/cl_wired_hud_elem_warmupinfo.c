#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	int timePrev;
	modernhudTextContext_t ctx;
} modernHudElementWarmupInfo_t;

void* CG_ModernHUDElementWarmupInfoCreate(const modernhudConfig_t* config)
{
	modernHudElementWarmupInfo_t* element;

	WHUD_ELEMENT_INIT_TEXT( element, config );

	return element;
}

void CG_ModernHUDElementWarmupInfoRoutine(void* context)
{
	modernHudElementWarmupInfo_t* element = (modernHudElementWarmupInfo_t*)context;
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
	// NOLINTNEXTLINE(clang-analyzer-core.StackAddressEscape) — element->ctx.text aliases a local stack buffer; consumed within CG_ModernHUDTextPrint and not retained past this call
	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif // FEAT_WIRED_UI
