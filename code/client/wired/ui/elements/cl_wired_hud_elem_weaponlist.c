// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../../../client.h"
#include "cl_wired_ui_hud_compat.h"
#include "cl_wired_text.h"
#include "cl_wired_ui_hud_private.h"

#if FEAT_WIRED_UI

/* maximum weapon slots for local arrays — matches wiredHudState_t capacity */
#define WLIST_MAX_SLOTS  (int)(sizeof(wiredHud->weaponList) / sizeof(wiredHud->weaponList[0]))

/* HUD redesign (Eser): horizontal gap between square carousel slots (px). */
#define WLIST_SLOT_GAP   8

/* HUD redesign (Eser): vertical gap between a slot's icon bottom and its ammo
   number, so the digits are not flush against the icon (vanilla-Q3 spacing). */
#define WLIST_AMMO_GAP   4

/* HUD redesign (Eser): fraction of the slot height to inset the weapon ICON quad
   inside its slot. The weapon icons (weapons/<w>/icon.png) are alpha-correct RGBA —
   a transparent background with an opaque silhouette — so they blend cleanly and need
   no black-margin compensation. This inset is purely stylistic: it centres the icon
   with breathing room around it (the classic vanilla-Q3 carousel look) while the
   slot/selection-border rects stay FULL size, so the selection frame still hugs the
   slot edges. 0.16 → ~16% margin total. Set to 0 to fill the slot edge-to-edge. */
#define WLIST_ICON_INSET 0.16f

typedef struct
{
	modernhudConfig_t config;
	modernhudConfig_t tmp_config;
	modernhudTextContext_t position;
	float x;
	float y;
	float w;
	float h;
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
	/* Rectless WiredUI leaves receive a true Clay bounding box and mark alignH
	 * in the adapter.  The legacy format encoded the horizontal anchor directly
	 * in rect.x, so preserve that path when alignH is absent; otherwise derive
	 * the carousel anchor from the resolved box. */
	if ( element->config.alignH.isSet ) {
		if ( element->config.alignH.value == MODERNHUD_ALIGNH_CENTER )
			element->x += element->w * 0.5f;
		else if ( element->config.alignH.value == MODERNHUD_ALIGNH_RIGHT )
			element->x += element->w;
	}

	CG_ModernHUDTextMakeContext(&element->tmp_config, &element->ammoCount[0]);

	return element;
}

static void CG_ModernHUDElementWeaponListSetup(modernHudElementWeaponList_t* element, modernhudAlignH_t align)
{
	int x, y, h;

	int count = wiredHud->weaponListCount;
	if (count > WLIST_MAX_SLOTS) count = WLIST_MAX_SLOTS;
	if (count > 16) count = 16;

	int total = count;

	/* HUD redesign (Eser): square slots with a small gap, NOT (icon+ammoWidth)
	   wide. The ammo number now sits BELOW the icon (see the ammo block), so each
	   slot is just the icon square + a fixed gap — this stops the overlapping /
	   interlocked-grid look the old (h+ammoWidth) packing produced. */

	if (align == MODERNHUD_ALIGNH_CENTER)
	{
		x = element->x - total * (element->h + WLIST_SLOT_GAP) / 2;
		y = element->y;
	}
	else
	{
		x = element->x;
		y = element->y - total * element->h / 2;
	}
	h = element->h;

	element->weaponNum = 0;

	for (int wpi = 0; wpi < count; ++wpi)
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

		/* HUD redesign (Eser): inset the icon quad inside the slot so the opaque
		   weapon-icon texture (a dark square) floats with a margin instead of filling
		   the slot edge-to-edge (vanilla-Q3 look). The slot/back/border below stay
		   FULL size — only this icon quad shrinks + re-centres. */
		{
			int iconInset = (int)( h * WLIST_ICON_INSET + 0.5f );
			element->tmp_config.rect.value[0] = x + iconInset;
			element->tmp_config.rect.value[1] = y + iconInset;
			element->tmp_config.rect.value[2] = h - 2 * iconInset;
			element->tmp_config.rect.value[3] = h - 2 * iconInset;
		}
		CG_ModernHUDDrawMakeContext(&element->tmp_config, &element->weaponIcon[element->weaponNum]);
		element->weaponIcon[element->weaponNum].image = wiredHud->weaponList[wpi].icon;

		/* selection and background — SQUARE slot now (ammo sits below the icon,
		   not to its right), so the background is just the icon square. */
		element->tmp_config.rect.value[0] = x;
		element->tmp_config.rect.value[1] = y;
		element->tmp_config.rect.value[2] = h;
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

		/* ammo — HUD redesign (Eser): drawn BELOW the icon (was to its right and
		   tiny/unreadable), centred under the square slot. The ammo count is sized
		   PROPORTIONALLY to the slot (≈0.42·h) so it scales with resolution/aspect
		   and stays legible — the authored fontsize (~12px) was far too small under a
		   ~50px slot. A drop shadow (textStyle 1) keeps the light digits readable over
		   the bright/cluttered world floor behind the carousel. */
		{
			float ammoFont = h * 0.42f;
			element->tmp_config.fontsize.value[1] = ammoFont;
			element->tmp_config.fontsize.value[0] = ammoFont / 1.618f;
			element->tmp_config.fontsize.isSet = qtrue;
		}
		element->tmp_config.textStyle.value = 1;   /* drop shadow */
		element->tmp_config.textStyle.isSet = qtrue;
		element->tmp_config.rect.value[0] = x;
		element->tmp_config.rect.value[1] = y + h + WLIST_AMMO_GAP;  /* small gap below the icon */
		element->tmp_config.rect.value[2] = h;                /* slot-wide */
		element->tmp_config.rect.value[3] = element->tmp_config.fontsize.value[1];
		element->tmp_config.textAlign.value = MODERNHUD_ALIGNH_CENTER;
		element->tmp_config.textAlign.isSet = qtrue;
		element->tmp_config.alignV.value = MODERNHUD_ALIGNV_TOP;
		element->tmp_config.alignV.isSet = qtrue;
		element->tmp_config.alignH.value = MODERNHUD_ALIGNH_CENTER;
		element->tmp_config.alignH.isSet = qtrue;

		CG_ModernHUDTextMakeContext(&element->tmp_config, &element->ammoCount[element->weaponNum]);
		element->ammoCount[element->weaponNum].text = &element->ammo[element->weaponNum][0];

		ammo = wiredHud->weaponList[wpi].ammo;

		/* HUD redesign (Eser): infinite-ammo weapons (ammo < 0, the gauntlet's
		   -1 sentinel) show the ∞ glyph in the carousel slot instead of the raw
		   "-1". ∞ = UTF-8 U+221E (\xE2\x88\x9E), present in the oxanium atlas. */
		/* centred under the icon — no leading/trailing pad. ∞ for infinite. */
		if (ammo < 0)
			Com_sprintf(&element->ammo[element->weaponNum][0], 8, "\xE2\x88\x9E");
		else
			Com_sprintf(&element->ammo[element->weaponNum][0], 8, "%i", ammo);

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
			x += h + WLIST_SLOT_GAP;   /* square slot + fixed gap (ammo is below, not beside) */
		}
		else
		{
			y += h + 2;
		}
		++element->weaponNum;
	}
}

/* HUD redesign (Eser): the carousel shows only briefly after a weapon switch,
   then fades out (classic Q3) — it does NOT sit on screen all match. Window:
   fully visible for WLIST_SHOW_TIME, then a WLIST_FADE_TIME alpha ramp to 0. */
#define WLIST_SHOW_TIME  1250
#define WLIST_FADE_TIME   400

void CG_ModernHUDElementWeaponListRoutine(void* context)
{
	modernHudElementWeaponList_t* element = (modernHudElementWeaponList_t*)context;

	/* show/fade gate: time since the last weapon switch.
	   weaponSelectTime <= 0 means "never switched" (zero-init / fresh spawn) —
	   draw nothing. This restores vanilla CG_FadeColor's `if (startMsec==0)
	   return NULL` guard that the plain `dt<0` clamp alone dropped, and keeps the
	   gate correct when cg.time is small/pinned (deterministic capture) where a
	   zero weaponSelectTime would otherwise read dt < SHOW+FADE and draw. */
	if ( wiredHud->weaponSelectTime <= 0 )
	{
		return;
	}
	int dt = wiredHud->time - wiredHud->weaponSelectTime;
	if ( dt < 0 ) dt = 0;
	if ( dt > WLIST_SHOW_TIME + WLIST_FADE_TIME )
	{
		return;  /* fully faded out — draw nothing during normal play */
	}
	float gateAlpha = 1.0f;
	if ( dt > WLIST_SHOW_TIME )
	{
		gateAlpha = 1.0f - (float)( dt - WLIST_SHOW_TIME ) / (float)WLIST_FADE_TIME;
	}

	CG_ModernHUDElementWeaponListSetup(element, element->config.textAlign.value);

	for (int i = 0; i < element->weaponNum; ++i)
	{
		/* apply the gate fade to every layer of the slot */
		element->back[i].color[3]        *= gateAlpha;
		element->weaponIcon[i].color[3]  *= gateAlpha;
		element->ammoCount[i].color[3]   *= gateAlpha;
		element->borderColor[i][3]       *= gateAlpha;

		/* Draw the slot back-fill ONLY when it has visible alpha. An idle slot with no
		   authored backcolor is memset to rgba(0,0,0,0); CG_ModernHUDFillWithColor does
		   not early-out on alpha 0, so it would stamp a solid BLACK square behind the
		   icon (the whiteShader 2D fill is not suppressed at alpha 0). Gating on
		   color[3] > 0 keeps idle slots box-free AND lets the selected slot's fill fade
		   out cleanly — once gateAlpha drives its alpha toward 0 the fill simply stops. */
		if ( element->back[i].color[3] > 0.0f )
			CG_ModernHUDFillWithColor(&element->back[i].coord, element->back[i].color);
		/* Draw the icon with its already-gate-faded color directly. The Ctx wrapper
		   re-picks color from config (for team-tinting), which would overwrite the
		   fade alpha applied above and leave the icon at full opacity — so the whole
		   carousel appeared to vanish abruptly instead of fading. Weapon icons are
		   not team-coloured, so the direct draw is correct and honours the ramp. */
		CG_ModernHUDDrawStretchPic(element->weaponIcon[i].coord,
		                           element->weaponIcon[i].coordPicture,
		                           element->weaponIcon[i].color,
		                           element->weaponIcon[i].image);
		CG_ModernHUDTextPrintNew(&element->config, &element->ammoCount[i], qfalse);
		CG_ModernHUDDrawBorderDirect(&element->back[i].coord, element->border[i], element->borderColor[i]);
	}
}

#endif /* FEAT_WIRED_UI */
