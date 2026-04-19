#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI



typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementTargetStatus_t;

void* CG_ModernHUDElementTargetStatusCreate(const modernhudConfig_t* config)
{
	modernHudElementTargetStatus_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_ModernHUDElementTargetStatusRoutine(void* context)
{
	modernHudElementTargetStatus_t* element = (modernHudElementTargetStatus_t*)context;
	char    s[1024];
	clientInfo_t* ci;

	if (cg_drawCrosshair.integer == 0) return;
	if (cg_drawCrosshairNames.integer == 0) return;
	if (cg.renderingThirdPerson != 0) return;
	if (cg.crosshairClientTime == 0) return;

	if (!CG_ModernHUDGetFadeColor(element->ctx.color_origin, element->ctx.color, &element->config, cg.crosshairClientTime))
	{
		return;
	}

	ci = &cgs.clientinfo[cg.crosshairClientNum];

	{
		wuiStoreEntry_t *e = WiredStore_Get( "crosshair.isTeammate" );
		if ( e && (int)e->value != 0 && cg_crosshairHealth.integer != 0 && !(cg.snap->ps.pm_flags & PMF_FOLLOW) )
		{
			vec4_t hcolor;
			char s[1024];

			wired_GetColorForAmount(ci->health, hcolor);

			Com_sprintf(s, sizeof(s), "[%i/%i]", ci->health, ci->armor);

			element->ctx.text = s;

			VectorCopy(hcolor, element->ctx.color);
			if (element->config.color.isSet)
			{
				element->ctx.color[3] = element->config.color.value.rgba[3];
			}
			CG_ModernHUDTextPrintNew(&element->config, &element->ctx, qfalse);

			element->ctx.text = NULL;
		}
	}

}

#endif // FEAT_WIRED_UI
