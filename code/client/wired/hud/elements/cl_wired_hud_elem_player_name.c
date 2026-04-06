#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
} shudElementPlayerName;

void* CG_SHUDElementPlayerNameCreate(const superhudConfig_t* config)
{
	shudElementPlayerName* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDTextMakeContext(&element->config, &element->ctx);

	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_SHUDElementPlayerNameRoutine(void* context)
{
	shudElementPlayerName* element = (shudElementPlayerName*)context;

	char textBuffer[MAX_QPATH];

	int clientNum = cg.snap->ps.clientNum;

	if (clientNum >= 0 && clientNum < MAX_CLIENTS && cgs.clientinfo[clientNum].infoValid)
	{
		Q_strncpyz(textBuffer, cgs.clientinfo[clientNum].name, sizeof(textBuffer));
	}
	else
	{
		Q_strncpyz(textBuffer, "---", sizeof(textBuffer));
	}

	element->ctx.text = textBuffer;

	CG_SHUDTextPrint(&element->config, &element->ctx);
}


void CG_SHUDElementPlayerNameDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
#endif // FEAT_WIRED_UI
