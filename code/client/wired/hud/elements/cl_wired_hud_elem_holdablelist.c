// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* cl_wired_hud_elem_holdablelist.c -- Holdable inventory list HUD element.
   Mirrors the weapon list layout (CENTER_RIGHT anchor, T direction, vertical).
   Reads wiredHud->holdableList[]; selected entry is highlighted with color2. */
#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "cl_wired_text.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

#define HLIST_MAX_SLOTS  (int)(sizeof(wiredHud->holdableList) / sizeof(wiredHud->holdableList[0]))

typedef struct {
	modernhudConfig_t      config;
	modernhudConfig_t      tmp_config;
	float                  x;
	float                  y;
	float                  w;
	float                  h;
	int                    count;
	modernhudDrawContext_t back[HLIST_MAX_SLOTS];
	modernhudDrawContext_t icon[HLIST_MAX_SLOTS];
	vec4_t                 backColor[HLIST_MAX_SLOTS];
	modernhudTextContext_t label[HLIST_MAX_SLOTS];
} modernHudElementHoldableList_t;

void* CG_ModernHUDElementHoldableListCreate(const modernhudConfig_t* config)
{
	modernHudElementHoldableList_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	memcpy(&element->tmp_config, &element->config, sizeof(element->tmp_config));

	element->x = element->config.rect.value[0];
	element->y = element->config.rect.value[1];
	element->w = element->config.rect.value[2];
	element->h = element->config.rect.value[3];
	element->count = 0;

	return element;
}

static void HoldableListSetup(modernHudElementHoldableList_t* element)
{
	int count = wiredHud->holdableListCount;
	if (count > HLIST_MAX_SLOTS) count = HLIST_MAX_SLOTS;

	/* grow upward from anchor (T direction) */
	float x = element->x;
	float y = element->y - count * element->h;
	float h = element->h;

	element->count = 0;

	for (int i = 0; i < count; i++) {
		/* background slot */
		element->tmp_config.rect.value[0] = x;
		element->tmp_config.rect.value[1] = y;
		element->tmp_config.rect.value[2] = h;
		element->tmp_config.rect.value[3] = h;
		CG_ModernHUDDrawMakeContext(&element->tmp_config, &element->back[i]);

		if (wiredHud->holdableList[i].selected) {
			if (element->config.color2.isSet)
				Vector4Copy(element->tmp_config.color2.value.rgba, element->backColor[i]);
			else
				Vector4Set(element->backColor[i], 0.4f, 0.2f, 0.6f, 0.5f);
		} else {
			if (element->config.bgcolor.isSet)
				CG_ModernHUDConfigPickBgColor(&element->tmp_config, element->backColor[i], qfalse);
			else
				Vector4Set(element->backColor[i], 0.1f, 0.1f, 0.1f, 0.5f);
		}

		/* icon or text label */
		if (wiredHud->holdableList[i].icon) {
			element->tmp_config.alignV.value = MODERNHUD_ALIGNV_TOP;
			element->tmp_config.alignV.isSet = qtrue;
			element->tmp_config.alignH.value = MODERNHUD_ALIGNH_LEFT;
			element->tmp_config.alignH.isSet = qtrue;
			CG_ModernHUDDrawMakeContext(&element->tmp_config, &element->icon[i]);
			element->icon[i].image = wiredHud->holdableList[i].icon;
		} else {
			/* no icon — render text label (polish item: key icons not yet available) */
			element->icon[i].image = 0;
			CG_ModernHUDTextMakeContext(&element->tmp_config, &element->label[i]);
			element->label[i].text = wiredHud->holdableList[i].label;
		}

		y += h;
		element->count++;
	}
}

void CG_ModernHUDElementHoldableListRoutine(void* context)
{
	modernHudElementHoldableList_t* element = (modernHudElementHoldableList_t*)context;

	HoldableListSetup(element);

	for (int i = 0; i < element->count; i++) {
		CG_ModernHUDFillWithColor(&element->back[i].coord, element->backColor[i]);
		if (element->icon[i].image) {
			CG_ModernHUDDrawStretchPicCtx(&element->config, &element->icon[i]);
		} else {
			CG_ModernHUDTextPrintNew(&element->config, &element->label[i], qfalse);
		}
	}
}

#endif /* FEAT_WIRED_UI */
