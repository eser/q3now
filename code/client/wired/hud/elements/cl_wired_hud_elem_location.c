#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t lastLocation;
	int time;
} modernHudElementLocation_t;

void* CG_ModernHUDElementLocationCreate(const modernhudConfig_t* config)
{
	modernHudElementLocation_t* element;
	modernhudGlobalContext_t* gctx;

	ModernHUD_ELEMENT_INIT(element, config);

	if (!element->config.time.isSet)
	{
		element->config.time.isSet = qtrue;
		element->config.time.value = 2000;
	}

	gctx = CG_ModernHUDGetContext();

	CG_ModernHUDTextMakeContext(&element->config, &element->lastLocation);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->lastLocation);

	return element;
}

void CG_ModernHUDElementLocationRoutine(void* context)
{
	modernHudElementLocation_t* element = (modernHudElementLocation_t*)context;
	clientInfo_t* ci;
	const char* newLocation;

	ci = &cgs.clientinfo[cg.snap->ps.clientNum];

	newLocation = CG_ConfigString(CS_LOCATIONS + ci->location);
	if (!newLocation || *newLocation == 0)
	{
		return;
	}

	if (element->lastLocation.text != newLocation && Q_stricmp(newLocation, "unknown"))
	{
		element->time = cg.time;
	}
	else if (!CG_ModernHUDGetFadeColor(element->lastLocation.color_origin, element->lastLocation.color, &element->config, element->time))
	{
		element->time = 0;
		return;
	}

	element->lastLocation.text = newLocation;

	CG_ModernHUDTextPrint(&element->config, &element->lastLocation);
}


#endif // FEAT_WIRED_UI
