#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementDisconnect_t;

void* CG_ModernHUDElementDisconnectCreate(const modernhudConfig_t* config)
{
	modernHudElementDisconnect_t* element;

	WHUD_ELEMENT_INIT_TEXT( element, config );

	return element;
}

void CG_ModernHUDElementDisconnectRoutine(void* context)
{
	modernHudElementDisconnect_t* element = (modernHudElementDisconnect_t*)context;

	if ( !wiredHud->connectionInterrupted ) {
		return;
	}

	element->ctx.text = "Connection Interrupted";
	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif // FEAT_WIRED_UI
