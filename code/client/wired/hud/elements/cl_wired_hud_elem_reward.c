// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef enum
{
	ModernHUD_REWARD_ICON,
	ModernHUD_REWARD_COUNT,
} modernHudRewardType_t;

// NOLINTNEXTLINE(bugprone-tagged-union-member-count) — `type` is a reward kind, not a discriminator for the ctx union; ctx variant is implicit per element registration site
typedef struct
{
	modernhudConfig_t config;
	union
	{
		modernhudDrawContext_t d;
		modernhudTextContext_t t;
	} ctx;
	modernHudRewardType_t type;
} modernHudElementStatusbarRewards;

static void* CG_ModernHUDElementRewardCreate(const modernhudConfig_t* config, modernHudRewardType_t type)
{
	modernHudElementStatusbarRewards* element;

	ModernHUD_ELEMENT_INIT(element, config);

	element->type = type;

	if (type == ModernHUD_REWARD_ICON)
	{
		CG_ModernHUDDrawMakeContext(&element->config, &element->ctx.d);
	}
	else
	{
		CG_ModernHUDTextMakeContext(&element->config, &element->ctx.t);
		CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx.t);
	}

	if (!element->config.text.isSet)
	{
		element->config.text.isSet = qtrue;
		Q_strncpyz(element->config.text.value, "%d", sizeof(element->config.text.value));
	}

	return element;
}

void* CG_ModernHUDElementRewardIconCreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementRewardCreate(config, ModernHUD_REWARD_ICON);
}

void* CG_ModernHUDElementRewardCountCreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementRewardCreate(config, ModernHUD_REWARD_COUNT);
}

void CG_ModernHUDElementRewardRoutine(void* context)
{
	modernHudElementStatusbarRewards* element = (modernHudElementStatusbarRewards*)context;

	if (!cg_drawRewards.integer)
	{
		return;
	}

	float* color_origin;
	float* color;

	if (element->type == ModernHUD_REWARD_ICON)
	{
		color_origin = element->ctx.d.color_origin;
		color = element->ctx.d.color;
	}
	else
	{
		color_origin = element->ctx.t.color_origin;
		color = element->ctx.t.color;
	}

	if (!CG_ModernHUDGetFadeColor(color_origin, color, &element->config, cg.rewardTime))
	{
		if (cg.rewardStack > 0)
		{
			for (int i = 0; i < cg.rewardStack; i++)
			{
				cg.rewardSound[i] = cg.rewardSound[i + 1];
				cg.rewardShader[i] = cg.rewardShader[i + 1];
				cg.rewardCount[i] = cg.rewardCount[i + 1];
			}
			cg.rewardTime = cg.time;
			cg.rewardStack--;
			CG_ModernHUDGetFadeColor(color_origin, color, &element->config, cg.rewardTime);

			if (!(cg_drawRewards.integer & DRAW_REWARDS_NOSOUND))
			{
				trap_S_StartLocalSound(cg.rewardSound[0], CHAN_ANNOUNCER);
			}
		}
		else
		{
			return;
		}
	}

	if (element->type == ModernHUD_REWARD_ICON)
	{
		if (!(cg_drawRewards.integer & DRAW_REWARDS_NOICON))
		{
			element->ctx.d.image = cg.rewardShader[0];
			CG_ModernHUDFill(&element->config);
			CG_ModernHUDDrawStretchPicCtx(&element->config, &element->ctx.d);
			CG_ModernHUDDrawBorder(&element->config);
		}
	}
	else
	{
		if (cg.rewardCount[0] && !(cg_drawRewards.integer & DRAW_REWARDS_NOICON))
		{
			element->ctx.t.text = va(element->config.text.value, cg.rewardCount[0]);
			CG_ModernHUDTextPrint(&element->config, &element->ctx.t);
		}
	}
}

#endif // FEAT_WIRED_UI
