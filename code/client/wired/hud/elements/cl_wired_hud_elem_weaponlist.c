#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "cl_wired_text.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

/* maximum weapon slots for local arrays — matches wiredHudState_t capacity */
#define WLIST_MAX_SLOTS  (int)(sizeof(wiredHud->weaponList) / sizeof(wiredHud->weaponList[0]))

typedef struct
{
	modernhudConfig_t config;
	modernhudConfig_t tmp_config;
	modernhudTextContext_t position;
	float x;
	float y;
	float w;
	float h;
	float ammoWidth;
	int ammoMax;
	int weaponNum;
	char ammo[16][8];                     /* generic buffer (>= WLIST_MAX_SLOTS) */
	vec4_t border[16];
	vec4_t borderColor[16];
	modernhudDrawContext_t back[16];
	modernhudDrawContext_t weaponIcon[16];
	modernhudTextContext_t ammoCount[16];

} modernHudElementWeaponList_t;

void* CG_ModernHUDElementWeaponListCreate(const modernhudConfig_t* config)
{
	modernHudElementWeaponList_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	if (!element->config.textAlign.isSet)
	{
		element->config.textAlign.isSet = qtrue;
		element->config.textAlign.value = MODERNHUD_ALIGNH_CENTER;
	}
	memcpy(&element->tmp_config, &element->config, sizeof(element->tmp_config));

	element->x = element->config.rect.value[0];
	element->y = element->config.rect.value[1];
	element->w = element->config.rect.value[2];
	element->h = element->config.rect.value[3];

	CG_ModernHUDTextMakeContext(&element->tmp_config, &element->ammoCount[0]);

	element->ammoMax = -1;

	return element;
}

static void CG_ModernHUDElementWeaponListSetup(modernHudElementWeaponList_t* element, modernhudAlignH_t align)
{
	int wpi;
	int x, y, h;
	int total;
	int ammo_max = 0;
	float offsetX;
	int count;

	count = wiredHud->weaponListCount;
	if (count > WLIST_MAX_SLOTS) count = WLIST_MAX_SLOTS;
	if (count > 16) count = 16;

	/* update max ammo width */
	for (wpi = 0; wpi < count; ++wpi)
	{
		int ammoVal = wiredHud->weaponList[wpi].ammo;
		if (ammo_max < ammoVal)
		{
			ammo_max = ammoVal;
		}
	}

	if (ammo_max > element->ammoMax)
	{
		element->ammoMax = ammo_max;
		element->ammoWidth = (int)Text_Measure(va(" %d", ammo_max), element->ammoCount[0].fontId, element->ammoCount[0].coord.named.h);
	}

	total = count;

	if (align == MODERNHUD_ALIGNH_CENTER)
	{
		x = element->x - total * (element->h + element->ammoWidth) / 2;
		y = element->y;
	}
	else
	{
		x = element->x;
		y = element->y - total * element->h / 2;
	}
	h = element->h;

	element->weaponNum = 0;

	for (wpi = 0; wpi < count; ++wpi)
	{
		int ammo;

		/* icon */
		element->tmp_config.alignV.value = MODERNHUD_ALIGNV_TOP;
		element->tmp_config.alignV.isSet = qtrue;

		if (align != MODERNHUD_ALIGNH_RIGHT)
		{
			element->tmp_config.alignH.value = MODERNHUD_ALIGNH_LEFT;
			element->tmp_config.alignH.isSet = qtrue;
		}
		else
		{
			element->tmp_config.alignH.value = MODERNHUD_ALIGNH_RIGHT;
			element->tmp_config.alignH.isSet = qtrue;
		}

		element->tmp_config.rect.value[0] = x;
		element->tmp_config.rect.value[1] = y;
		element->tmp_config.rect.value[2] = h;
		element->tmp_config.rect.value[3] = h;
		CG_ModernHUDDrawMakeContext(&element->tmp_config, &element->weaponIcon[element->weaponNum]);
		element->weaponIcon[element->weaponNum].image = wiredHud->weaponList[wpi].icon;

		/* selection and background */
		element->tmp_config.rect.value[0] = x;
		if (align == MODERNHUD_ALIGNH_RIGHT)
		{
			element->tmp_config.rect.value[0] -= element->ammoWidth;
		}
		element->tmp_config.rect.value[1] = y;
		element->tmp_config.rect.value[2] = h + element->ammoWidth;
		element->tmp_config.rect.value[3] = h;
		CG_ModernHUDDrawMakeContext(&element->tmp_config, &element->back[element->weaponNum]);
		if (!wiredHud->weaponList[wpi].selected)
		{
			if (element->config.bgcolor.isSet)
			{
				CG_ModernHUDConfigPickBgColor(&element->tmp_config, element->back[element->weaponNum].color, qfalse);
			}
			else
			{
				memset(element->back[element->weaponNum].color, 0, sizeof(element->back[element->weaponNum].color));
			}
		}
		else
		{
			if (element->config.color2.isSet)
			{
				Vector4Copy(element->tmp_config.color2.value.rgba, element->back[element->weaponNum].color);
			}
			else
			{
				memset(element->back[element->weaponNum].color, 0, sizeof(element->back[element->weaponNum].color));
			}
		}

		if (wiredHud->weaponList[wpi].selected)
		{
			if (element->config.border.isSet)
			{
				Vector4Copy(element->config.border.value, element->border[element->weaponNum]);
			}
			else
			{
				Vector4Set(element->border[element->weaponNum], 0, 0, 0, 0);
			}

			if (element->config.borderColor.isSet)
			{
				CG_ModernHUDConfigPickBorderColor(&element->config, element->borderColor[element->weaponNum], qfalse);
			}
			else
			{
				Vector4Set(element->borderColor[element->weaponNum], 1, 1, 1, 0);
			}
		}
		else
		{
			Vector4Set(element->border[element->weaponNum], 0, 0, 0, 0);
			Vector4Set(element->borderColor[element->weaponNum], 0, 0, 0, 0);
		}

		/* ammo */
		if (align != MODERNHUD_ALIGNH_RIGHT)
		{
			element->tmp_config.rect.value[0] = x + h;
			element->tmp_config.textAlign.value = MODERNHUD_ALIGNH_LEFT;
			element->tmp_config.textAlign.isSet = qtrue;
		}
		else
		{
			element->tmp_config.rect.value[0] = x;
			element->tmp_config.textAlign.value = MODERNHUD_ALIGNH_RIGHT;
			element->tmp_config.textAlign.isSet = qtrue;
		}
		/* position text top so it is visually centered in the slot:
		   MSDF y = em-top (glyph drawn downward), so offset = (slot_h - font_h) / 2 */
		element->tmp_config.rect.value[1] = y + (h - element->tmp_config.fontsize.value[1]) * 0.5f;
		element->tmp_config.rect.value[2] = element->ammoWidth;
		element->tmp_config.rect.value[3] = h;

		element->tmp_config.alignV.value = MODERNHUD_ALIGNV_TOP;
		element->tmp_config.alignV.isSet = qtrue;

		element->tmp_config.alignH.value = MODERNHUD_ALIGNH_LEFT;
		element->tmp_config.alignH.isSet = qtrue;

		offsetX = element->tmp_config.fontsize.value[0] / 8;

		if (align == MODERNHUD_ALIGNH_RIGHT)
		{
			element->tmp_config.rect.value[0] += (offsetX + offsetX);
		}
		else
		{
			element->tmp_config.rect.value[0] -= offsetX;
		}

		CG_ModernHUDTextMakeContext(&element->tmp_config, &element->ammoCount[element->weaponNum]);
		element->ammoCount[element->weaponNum].text = &element->ammo[element->weaponNum][0];

		ammo = wiredHud->weaponList[wpi].ammo;

		if (align != MODERNHUD_ALIGNH_RIGHT)
		{
			Com_sprintf(&element->ammo[element->weaponNum][0], 8, " %i", ammo);
		}
		else
		{
			Com_sprintf(&element->ammo[element->weaponNum][0], 8, "%i ", ammo);
		}

		if (ammo == 0)
		{
			vec4_t tmpColor;
			Vector4Copy(colorRed, tmpColor);
			tmpColor[3] = element->tmp_config.color.value.rgba[3];
			Vector4Copy(tmpColor, element->ammoCount[element->weaponNum].color);
		}
		else
		{
			Vector4Copy(element->tmp_config.color.value.rgba, element->ammoCount[element->weaponNum].color);
		}

		if (align == MODERNHUD_ALIGNH_CENTER)
		{
			x += h + element->ammoWidth;
		}
		else
		{
			y += h + 2;
		}
		++element->weaponNum;
	}
}

void CG_ModernHUDElementWeaponListRoutine(void* context)
{
	modernHudElementWeaponList_t* element = (modernHudElementWeaponList_t*)context;
	int i;

	CG_ModernHUDElementWeaponListSetup(element, element->config.textAlign.value);

	for (i = 0; i < element->weaponNum; ++i)
	{
		CG_ModernHUDFillWithColor(&element->back[i].coord, element->back[i].color);
		CG_ModernHUDDrawStretchPicCtx(&element->config, &element->weaponIcon[i]);
		CG_ModernHUDTextPrintNew(&element->config, &element->ammoCount[i], qfalse);
		CG_ModernHUDDrawBorderDirect(&element->back[i].coord, element->border[i], element->borderColor[i]);
	}
}

#endif /* FEAT_WIRED_UI */
