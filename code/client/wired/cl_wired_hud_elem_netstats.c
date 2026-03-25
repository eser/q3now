#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI


#if FEAT_UNLAGGED

typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
} shudElementNetStats_t;

void* CG_SHUDElementNetStatsCreate(const superhudConfig_t* config)
{
	shudElementNetStats_t* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_SHUDElementNetStatsRoutine(void* context)
{
	shudElementNetStats_t* element = (shudElementNetStats_t*)context;
	int ping;
	int delag;

	if (!cg.snap) {
		return;
	}

	// truePing: the server sends the real (averaged) ping via
	// playerState when g_truePing is enabled; otherwise fall back
	// to the snapshot ping which is quantised to 50 ms buckets.
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

	CG_SHUDTextPrint(&element->config, &element->ctx);
}

void CG_SHUDElementNetStatsDestroy(void* context)
{
	if (context) {
		Z_Free(context);
	}
}

#endif /* FEAT_UNLAGGED */
#endif // FEAT_WIRED_UI
