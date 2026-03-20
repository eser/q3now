#include "cg_local.h"
#include "cg_superhud_private.h"


typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
} shudElementNGP_t;

void* CG_SHUDElementNGPCreate(const superhudConfig_t* config)
{
	shudElementNGP_t* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_SHUDElementNGPRoutine(void* context)
{
	shudElementNGP_t* element = (shudElementNGP_t*)context;
	int ping;

	if (cg.demoPlayback) return;

	if (qtrue)
	{
		ping = 0;
	}
	else
	{
		ping = cg.snap->ps.ping;
	}

	element->ctx.text = va("%ims", ping);
	CG_SHUDTextPrint(&element->config, &element->ctx);
}

void CG_SHUDElementNGPDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}


