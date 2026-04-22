#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementVMW_t;

void* CG_ModernHUDElementVMWCreate(const modernhudConfig_t* config)
{
	modernHudElementVMW_t* element;

	WHUD_ELEMENT_INIT_TEXT( element, config );

	return element;
}

void CG_ModernHUDElementVMWRoutine(void* context)
{
	modernHudElementVMW_t* element = (modernHudElementVMW_t*)context;

	if (cgs.voteTime == 0) return;

	if (cgs.voteModified)
	{
		cgs.voteModified = 0;
		trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
	}

	int time = (30000 - (cg.time - cgs.voteTime)) / 1000;

	if (time < 0)
	{
		time = 0;
	}
	element->ctx.text = va("VOTE(%i):%s yes(F1):%i no(F2):%i", time, cgs.voteString, cgs.voteYes, cgs.voteNo);

	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif // FEAT_WIRED_UI
