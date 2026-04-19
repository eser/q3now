#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementPlayerName;

void* CG_ModernHUDElementPlayerNameCreate(const modernhudConfig_t* config)
{
	modernHudElementPlayerName* element;

	WHUD_ELEMENT_INIT_TEXT( element, config );

	return element;
}

void CG_ModernHUDElementPlayerNameRoutine(void* context)
{
	modernHudElementPlayerName* element = (modernHudElementPlayerName*)context;

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

	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif // FEAT_WIRED_UI
