
#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "cl_wired_text.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	modernhudDrawContext_t ctxPowerup;
	modernhudTextContext_t ctxName;
	modernhudTextContext_t ctxHealthArmor;
	modernhudDrawContext_t ctxWeapon;
	modernhudTextContext_t ctxLocation;
	int index;
} modernHudElementTeam_t;


void* CG_ModernHUDElementTeamCreate(const modernhudConfig_t* config, int line)
{
	modernHudTeamOverlay_t teamOverlay;

	modernhudConfig_t lcfg;

	modernHudElementTeam_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	element->index = line;

	//force aligin settings
	element->config.textAlign.isSet = qtrue;
	element->config.textAlign.value = MODERNHUD_ALIGNH_LEFT;
	element->config.alignH.isSet = qtrue;
	element->config.alignH.value = MODERNHUD_ALIGNH_LEFT;
	element->config.alignV.isSet = qtrue;
	element->config.alignV.value = MODERNHUD_ALIGNV_CENTER;

	CG_ModernHUDElementCompileTeamOverlayConfig(config->fontsize.value[0], &teamOverlay);

	memcpy(&lcfg, &element->config, sizeof(element->config));

	// setup powerup
	lcfg.rect.value[0] = config->rect.value[0] + teamOverlay.powerupOffsetPix;
	lcfg.rect.value[2] = teamOverlay.powerupLenPix;
	CG_ModernHUDDrawMakeContext(&lcfg, &element->ctxPowerup);

	// setup name
	lcfg.rect.value[0] = config->rect.value[0] + teamOverlay.nameOffsetPix;
	lcfg.rect.value[2] = teamOverlay.nameLenPix;
	CG_ModernHUDTextMakeContext(&lcfg, &element->ctxName);
	element->ctxName.width = teamOverlay.nameLenPix;

	// setup health and armor
	lcfg.rect.value[0] = config->rect.value[0] + teamOverlay.healthAndArmorOffsetPix;
	lcfg.rect.value[2] = teamOverlay.healthAndArmorLenPix;
	CG_ModernHUDTextMakeContext(&lcfg, &element->ctxHealthArmor);
	element->ctxHealthArmor.width = teamOverlay.healthAndArmorLenPix;
	element->ctxHealthArmor.flags |= DS_FORCE_COLOR;

	// setup weapon
	lcfg.rect.value[0] = config->rect.value[0] + teamOverlay.weaponOffsetPix;
	lcfg.rect.value[2] = teamOverlay.weaponLenPix;
	CG_ModernHUDDrawMakeContext(&lcfg, &element->ctxWeapon);

	// setup location
	lcfg.rect.value[0] = config->rect.value[0] + teamOverlay.locationOffsetPix;
	lcfg.rect.value[2] = teamOverlay.locationLenPix;
	CG_ModernHUDTextMakeContext(&lcfg, &element->ctxLocation);
	element->ctxLocation.width = teamOverlay.locationLenPix;

	// setup width of element
	element->config.rect.value[2] = teamOverlay.overlayWidthPix;

	return element;
}

void CG_ModernHUDElementTeamRoutine(void* context)
{
	modernHudElementTeam_t* element = (modernHudElementTeam_t*)context;
	clientInfo_t* ci;
	int cnt = 0;
	int index;
	int ourTeam = cg.snap->ps.persistant[PERS_TEAM];

	for (index = 0; index < numSortedTeamPlayers; ++index)
	{
		ci = &cgs.clientinfo[sortedTeamPlayers[index]];
		if (ci->infoValid)
		{
			if (ci->team != ourTeam)
			{
				continue;
			}
			if (++cnt == element->index)
			{
				break;
			}
		}
	}

	if (cnt != element->index)
	{
		//no elements
		return;
	}
	CG_ModernHUDFill(&element->config);

	//get player
	ci = &cgs.clientinfo[sortedTeamPlayers[index]];

	// draw name
	element->ctxName.text = ci->name;
	CG_ModernHUDTextPrint(&element->config, &element->ctxName);

	// draw health and armor
	wired_GetColorForAmount(ci->health, element->ctxHealthArmor.color);
	element->ctxHealthArmor.text = va("%3i/%i", ci->health, ci->armor);

	Text_Draw( element->ctxHealthArmor.text,
	           element->ctxHealthArmor.coord.named.x,
	           element->ctxHealthArmor.coord.named.y,
	           element->ctxHealthArmor.fontId,
	           element->ctxHealthArmor.coord.named.h,
	           element->ctxHealthArmor.color,
	           WiredFont_ToAlignment( element->ctxHealthArmor.flags ),
	           WiredFont_ToTextFlags( element->ctxHealthArmor.flags ) );

	// draw weapon
	element->ctxWeapon.image = cg_weapons[ci->curWeapon].ammoIcon ?  cg_weapons[ci->curWeapon].ammoIcon : cgs.media.deferShader;
	CG_ModernHUDDrawStretchPicCtx(&element->config, &element->ctxWeapon);

	// draw powerup
	{
		int k = 0;
		gitem_t* gi;

		do
		{
			if (qfalse && ci->health <= 0)
			{
				element->ctxPowerup.image = cgs.media.noammoShader;
				CG_ModernHUDDrawStretchPicCtx(&element->config, &element->ctxPowerup);
				break;
			}
			else if (ci->powerups & (1 << k))
			{
				gi = BG_FindItemForPowerup(k);
				if (gi)
				{
					element->ctxPowerup.image = trap_R_RegisterShader(gi->icon);
					CG_ModernHUDDrawStretchPicCtx(&element->config, &element->ctxPowerup);
				}
			}
		}
		while (++k < 16);
	}

	// draw location
	{
		element->ctxLocation.text = CG_ConfigString(CS_LOCATIONS + ci->location);
		if (!element->ctxLocation.text || *element->ctxLocation.text == 0)
		{
			element->ctxLocation.text = "unknown";
		}

		CG_ModernHUDTextPrint(&element->config, &element->ctxLocation);
	}
}

#endif // FEAT_WIRED_UI
