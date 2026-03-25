#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
} shudElementStatusbarAmmoCount;

void* CG_SHUDElementSBAmCCreate(const superhudConfig_t* config)
{
	shudElementStatusbarAmmoCount* element;

	SHUD_ELEMENT_INIT(element, config);

	//load defaults
	if (!element->config.color.isSet)
	{
		element->config.color.isSet = qtrue;
		element->config.color.value.type = SUPERHUD_COLOR_RGBA;
		Vector4Set(element->config.color.value.rgba, 1, 0.7, 0, 1);
	}

	if (!element->config.text.isSet)
	{
		element->config.text.isSet = qtrue;
		Q_strncpyz(element->config.text.value, "%i", sizeof(element->config.text.value));
	}

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_SHUDElementSBAmCRoutine(void* context)
{
	shudElementStatusbarAmmoCount* element = (shudElementStatusbarAmmoCount*)context;
	int wp;
	int  isFiring;
	int ammo;
	vec4_t finalColor;
	if (!cg.snap)
	{
		return;
	}

	wp = cg.snap->ps.weapon;

	isFiring = (cg.predictedPlayerState.weaponstate == WEAPON_FIRING) ? 1 : 0;

	if (wp == WP_NONE || wp == WP_GAUNTLET) return;

	ammo = CG_SHUDGetAmmo(wp);

	if (ammo == 0)
	{
		Vector4Copy(colorRed, finalColor);
		finalColor[3] = element->config.color.value.rgba[3];
	}
	else
	{
		if (isFiring && !element->config.style.isSet)
		{
			if (element->config.color2.isSet)
			{
				Vector4Copy(element->config.color2.value.rgba, finalColor);
			}
			else
			{
				Vector4Copy(colorDkGrey, finalColor);
			}
		}
		else
		{
			Vector4Copy(element->config.color.value.rgba, finalColor);
		}
	}

	Vector4Copy(finalColor, element->ctx.color);

	element->ctx.text = va(element->config.text.value, ammo > 0 ? ammo : 0);

	CG_SHUDTextPrintNew(&element->config, &element->ctx, qfalse);
}

void CG_SHUDElementSBAmCDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
#endif // FEAT_WIRED_UI
