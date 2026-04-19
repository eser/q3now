#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

#define FPS_MAX_FRAMES  4
typedef struct
{
	modernhudConfig_t config;
	float timeAverage;
	int framesNum;
	int timePrev;
	modernhudTextContext_t ctx;
} modernHudElementFPS_t;

void* CG_ModernHUDElementFPSCreate(const modernhudConfig_t* config)
{
	modernHudElementFPS_t* element;

	WHUD_ELEMENT_INIT_TEXT( element, config );

	return element;
}

void CG_ModernHUDElementFPSRoutine(void* context)
{
	modernHudElementFPS_t* element = (modernHudElementFPS_t*)context;
	float     fps_val;
	int     fps_val_int;
	int     t;

	// don't use serverTime, because that will be drifting to
	// correct for internet lag changes, timescales, timedemos, etc
	t = trap_Milliseconds();
	if (element->timePrev == 0)
	{
		// skip first measure result
		element->timePrev = t;
		return;
	}
	element->timeAverage *= element->framesNum;
	element->timeAverage += t - element->timePrev;
	element->timeAverage /= ++element->framesNum;
	element->timePrev = t;

	if (element->framesNum > FPS_MAX_FRAMES)
	{
		element->framesNum = FPS_MAX_FRAMES;
	}

	fps_val = 1000.0f / element->timeAverage;
	fps_val_int = (int)fps_val;
	if (fps_val - (float)fps_val_int > 0.5f)
	{
		++fps_val_int;
	}

	if (element->config.style.isSet && element->config.style.value == 1)
	{
		element->ctx.text = va("%i", fps_val_int);
	}
	else
	{
		element->ctx.text = va("%ifps", fps_val_int);
	}

	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif // FEAT_WIRED_UI
