// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// teamvotemessageworld — team-vote counterpart of the call-vote line
// (cl_wired_hud_elem_vmw.c). Same shape, same 30s window (VOTE_TIME in
// g_local.h), same talkSound beep on change; the only differences are the
// TEAMVOTE label and the teamvote yes/no response keys (F5/F6 — F3 was
// already taken by ui_teamorders in default.cfg, so the team vote takes the
// next free pair rather than displacing a working bind).
//
// cgs.teamVote* here is already resolved to the local player's own team by
// the cgame bridge, so this element never has to know the red/blue slot
// mapping — teamVoteTime == 0 simply means "no team vote I can answer".

#include "../../../client.h"
#include "cl_wired_ui_hud_compat.h"
#include "cl_wired_ui_hud_private.h"

#if FEAT_WIRED_UI

typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementTVMW_t;

void* CG_ModernHUDElementTVMWCreate(const modernhudConfig_t* config)
{
	modernHudElementTVMW_t* element;

	WHUD_ELEMENT_INIT_TEXT( element, config );

	return element;
}

void CG_ModernHUDElementTVMWRoutine(void* context)
{
	modernHudElementTVMW_t* element = (modernHudElementTVMW_t*)context;

	if (cgs.teamVoteTime == 0) return;

	if (cgs.teamVoteModified)
	{
		cgs.teamVoteModified = 0;
		trap_S_StartLocalSound(cgs.media.talkSound, CHAN_LOCAL_SOUND);
	}

	int time = (30000 - (cg.time - cgs.teamVoteTime)) / 1000;

	if (time < 0)
	{
		time = 0;
	}
	element->ctx.text = va("TEAMVOTE(%i):%s yes(F5):%i no(F6):%i", time, cgs.teamVoteString, cgs.teamVoteYes, cgs.teamVoteNo);

	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif // FEAT_WIRED_UI
