// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	modernhudDrawContext_t drawCtx;
} modernHudElementGridContext;

void* CG_ModernHUDElementGridCreate(const modernhudConfig_t* config)
{
	modernHudElementGridContext* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDDrawMakeContext(&element->config, &element->drawCtx);

	return element;
}

void CG_ModernHUDElementGridRoutine(void* context)
{
	modernHudElementGridContext* element = (modernHudElementGridContext*)context;
	float x = element->config.rect.value[0];
	float y = element->config.rect.value[1];
	float cellWidth = element->config.rect.value[2];
	float cellHeight = element->config.rect.value[3];
	qboolean hasColor2 = element->config.color2.isSet;
	vec4_t color1, color2;

	Vector4Copy(element->config.color.value.rgba, color1);
	if (hasColor2)
	{
		Vector4Copy(element->config.color2.value.rgba, color2);
	}

	float startCol = x;
	while (startCol > 0)
	{
		startCol -= cellWidth;
	}

	float startRow = y;
	while (startRow > 0)
	{
		startRow -= cellHeight;
	}

	modernhudCoord_t coord;
	int index = 0;
	// NOLINTNEXTLINE(bugprone-float-loop-counter,clang-analyzer-security.FloatLoopCounter) — viewport-pixel grid step; drift acceptable for visual output
	for (float col = startCol; col <= (float)cls.glconfig.vidWidth; col += cellWidth, index++)
	{
		coord.named.x = col;
		coord.named.y = 0;
		coord.named.w = 1;
		coord.named.h = (float)cls.glconfig.vidHeight;

		if (hasColor2 && (index & 1))
		{
			CG_ModernHUDFillWithColor(&coord, color2);
		}
		else
		{
			CG_ModernHUDFillWithColor(&coord, color1);
		}
	}

	index = 0;
	// NOLINTNEXTLINE(bugprone-float-loop-counter,clang-analyzer-security.FloatLoopCounter) — viewport-pixel grid step; drift acceptable for visual output
	for (float row = startRow; row <= (float)cls.glconfig.vidHeight; row += cellHeight, index++)
	{
		coord.named.x = 0;
		coord.named.y = row;
		coord.named.w = (float)cls.glconfig.vidWidth;
		coord.named.h = 1;

		if (hasColor2 && (index & 1))
		{
			CG_ModernHUDFillWithColor(&coord, color2);
		}
		else
		{
			CG_ModernHUDFillWithColor(&coord, color1);
		}
	}
}



#endif // FEAT_WIRED_UI
