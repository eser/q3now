#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI


#if FEAT_UNLAGGED

typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementNetStats_t;

void* CG_ModernHUDElementNetStatsCreate(const modernhudConfig_t* config)
{
	modernHudElementNetStats_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_ModernHUDElementNetStatsRoutine(void* context)
{
	modernHudElementNetStats_t* element = (modernHudElementNetStats_t*)context;
	int ping;
	int delag;

	if (!cg.snap) {
		return;
	}

	// truePing: the server sends the real (averaged) ping via
	// playerState.
	ping = cg.snap->ping;

	// effective delag window: difference between level time and
	// the command time the server would rewind to.  Approximated
	// on the client as the reported ping.
	delag = ping;

	if (element->config.style.isSet && element->config.style.value == 1) {
		// compact: just the number
		element->ctx.text = va("%i", ping);
	} else {
		// default: labelled
		element->ctx.text = va("%ims delag:%ims", ping, delag);
	}

	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

void CG_ModernHUDElementNetStatsDestroy(void* context)
{
	if (context) {
		Z_Free(context);
	}
}

#endif /* FEAT_UNLAGGED */
#endif // FEAT_WIRED_UI
