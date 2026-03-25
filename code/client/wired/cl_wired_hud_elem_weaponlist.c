#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	superhudConfig_t config;
	superhudConfig_t tmp_config;
	superhudTextContext_t position;
	float x; // main x point of whole weaponlist
	float y; // main y point of whole weaponlist
	float w; // width of one weapon
	float h; // height of one weapon
	float ammoWidth; // width of ammo field
	int ammoMax;
	int weaponNum;
	char ammo[WP_NUM_WEAPONS][8];
	vec4_t border[WP_NUM_WEAPONS];
	vec4_t borderColor[WP_NUM_WEAPONS];
	superhudDrawContext_t back[WP_NUM_WEAPONS];
	superhudDrawContext_t weaponIcon[WP_NUM_WEAPONS];
	superhudTextContext_t ammoCount[WP_NUM_WEAPONS];

} shudElementWeaponList_t;

void* CG_SHUDElementWeaponListCreate(const superhudConfig_t* config)
{
	shudElementWeaponList_t* element;

	SHUD_ELEMENT_INIT(element, config);

	if (!element->config.textAlign.isSet)
	{
		element->config.textAlign.isSet = qtrue;
		element->config.textAlign.value = SUPERHUD_ALIGNH_CENTER;
	}
	memcpy(&element->tmp_config, &element->config, sizeof(element->tmp_config));

	element->x = element->config.rect.value[0];
	element->y = element->config.rect.value[1];
	element->w = element->config.rect.value[2];
	element->h = element->config.rect.value[3];

	CG_SHUDTextMakeContext(&element->tmp_config, &element->ammoCount[0]);

	element->ammoMax = -1; //force update width

	return element;
}

static void CG_SHUDElementWeaponListSetup(shudElementWeaponList_t* element, superhudAlignH_t align)
{
	int wpi, statWeapons;
	int x, y, w, h;
	int total;
	int ammo_max = 0;
	int weapon;
	int ammo;
	float offsetX, offsetY;

	if (!cg.snap)
	{
		return;
	}

	statWeapons = cg.snap->ps.stats[STAT_WEAPONS];
	weapon = cg.snap->ps.weapon;

	//update width
	for (wpi = WP_MACHINEGUN; wpi < WP_NUM_WEAPONS; ++wpi)
	{
		if ((statWeapons & (1 << wpi)) != 0)
		{
			int ammo = CG_SHUDGetAmmo(wpi);
			if (ammo_max < ammo)
			{
				ammo_max = ammo;
			}
		}
	}

	if (ammo_max > element->ammoMax)
	{
		element->ammoMax = ammo_max;
		element->ammoWidth = CG_ModernDrawStringLenPix(va(" %d", ammo_max), element->ammoCount[0].coord.named.w, element->ammoCount[0].flags, 0);
	}


	for (wpi = WP_MACHINEGUN, total = 0; wpi < WP_NUM_WEAPONS; ++wpi)
	{
		if ((statWeapons & (1 << wpi)) != 0)
		{
			++total;
		}
	}

	if (align == SUPERHUD_ALIGNH_CENTER)
	{
		x = element->x - total * (element->w + element->ammoWidth) / 2; //shift to center
		y = element->y;
	}
	else
	{
		x = element->x;
		y = element->y - total * element->h / 2; //shift to center
	}
	w = element->w;
	h = element->h;

	element->weaponNum = 0;

	for (wpi = WP_MACHINEGUN; wpi < WP_NUM_WEAPONS; ++wpi)
	{
		if ((statWeapons & (1 << wpi)) != 0)
		{
			// icon
			element->tmp_config.alignV.value = SUPERHUD_ALIGNV_TOP;
			element->tmp_config.alignV.isSet = qtrue;

			// left and center aligned left
			if (align != SUPERHUD_ALIGNH_RIGHT)
			{
				element->tmp_config.alignH.value = SUPERHUD_ALIGNH_LEFT;
				element->tmp_config.alignH.isSet = qtrue;
			}
			else
			{
				element->tmp_config.alignH.value = SUPERHUD_ALIGNH_RIGHT;
				element->tmp_config.alignH.isSet = qtrue;
			}

			element->tmp_config.rect.value[0] = x;
			element->tmp_config.rect.value[1] = y;
			element->tmp_config.rect.value[2] = w;
			element->tmp_config.rect.value[3] = h;
			CG_SHUDDrawMakeContext(&element->tmp_config, &element->weaponIcon[element->weaponNum]);
			element->weaponIcon[element->weaponNum].image = cg_weapons[wpi].weaponIcon;

			// selection and background
			element->tmp_config.rect.value[0] = x;
			if (align == SUPERHUD_ALIGNH_RIGHT)
			{
				element->tmp_config.rect.value[0] -= element->ammoWidth;
			}
			element->tmp_config.rect.value[1] = y;
			element->tmp_config.rect.value[2] = w + element->ammoWidth;
			element->tmp_config.rect.value[3] = h;
			CG_SHUDDrawMakeContext(&element->tmp_config, &element->back[element->weaponNum]);
			if (wpi != weapon)
			{
				if (element->config.bgcolor.isSet)
				{
					CG_SHUDConfigPickBgColor(&element->tmp_config, element->back[element->weaponNum].color, qfalse);
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

			if (wpi == weapon)
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
					CG_SHUDConfigPickBorderColor(&element->config, element->borderColor[element->weaponNum], qfalse);
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

			// ammo
			if (align != SUPERHUD_ALIGNH_RIGHT)
			{
				element->tmp_config.rect.value[0] = x + w;
				element->tmp_config.textAlign.value = SUPERHUD_ALIGNH_LEFT;
				element->tmp_config.textAlign.isSet = qtrue;
			}
			else
			{
				element->tmp_config.rect.value[0] = x;
				element->tmp_config.textAlign.value = SUPERHUD_ALIGNH_RIGHT;
				element->tmp_config.textAlign.isSet = qtrue;
			}
			element->tmp_config.rect.value[1] = y;
			element->tmp_config.rect.value[2] = w;
			element->tmp_config.rect.value[3] = h;

			element->tmp_config.alignV.value = SUPERHUD_ALIGNV_CENTER;
			element->tmp_config.alignV.isSet = qtrue;

			element->tmp_config.alignH.value = SUPERHUD_ALIGNH_LEFT;
			element->tmp_config.alignH.isSet = qtrue;

			// Offset for text
			offsetX = element->tmp_config.fontsize.value[0] / 8;
			offsetY = element->tmp_config.fontsize.value[1] / 16;


			if (align == SUPERHUD_ALIGNH_RIGHT)
			{
				element->tmp_config.rect.value[0] += (offsetX + offsetX);
			}
			else
			{
				element->tmp_config.rect.value[0] -= offsetX;
				element->tmp_config.rect.value[1] -= offsetY;
			}

			CG_SHUDTextMakeContext(&element->tmp_config, &element->ammoCount[element->weaponNum]);
			element->ammoCount[element->weaponNum].text = &element->ammo[element->weaponNum][0];

			ammo = CG_SHUDGetAmmo(wpi);

			if (align != SUPERHUD_ALIGNH_RIGHT)
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

			if (align == SUPERHUD_ALIGNH_CENTER)
			{
				x += w + element->ammoWidth;
			}
			else
			{
				y += h + 2; /* gap between weapon list items */
			}
			++element->weaponNum;
		}
	}

}

void CG_SHUDElementWeaponListRoutine(void* context)
{
	shudElementWeaponList_t* element = (shudElementWeaponList_t*)context;
	int i;
	vec4_t bgColor;

	CG_SHUDElementWeaponListSetup(element, element->config.textAlign.value);

	//draw background, weapon icon and ammo count
	for (i = 0; i < element->weaponNum; ++i)
	{
		CG_SHUDFillWithColor(&element->back[i].coord, element->back[i].color);
		CG_SHUDDrawStretchPicCtx(&element->config, &element->weaponIcon[i]);
		CG_SHUDTextPrintNew(&element->config, &element->ammoCount[i], qfalse);
		CG_SHUDDrawBorderDirect(&element->back[i].coord, element->border[i], element->borderColor[i]);
	}
}

void CG_SHUDElementWeaponListDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
#endif // FEAT_WIRED_UI
