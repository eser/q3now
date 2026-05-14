// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_hud_elem_name.c — Player name HUD elements (OWN / NME)
*/
#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

enum modernHudElementNameType_t
{
	ModernHUDENAME_TYPE_OWN,
	ModernHUDENAME_TYPE_NME,
};

typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
	enum modernHudElementNameType_t type;
} modernHudElementName_t;

static void* CG_ModernHUDElementNameCreate(const modernhudConfig_t* config, enum modernHudElementNameType_t type)
{
	modernHudElementName_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	element->type = type;

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void* CG_ModernHUDElementNameOWNCreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementNameCreate(config, ModernHUDENAME_TYPE_OWN);
}

void* CG_ModernHUDElementNameNMECreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementNameCreate(config, ModernHUDENAME_TYPE_NME);
}

static void CG_ModernHUDElementNameGetPairFFA(const char** own, const char** nme)
{
	*own = cgs.clientinfo[cg.snap->ps.clientNum].name;

	int clientNum = cg.predictedPlayerState.persistant[PERS_LAST_ATTACKER];
	if (clientNum < 0 || clientNum >= MAX_CLIENTS)
	{
		return;
	}
	if (!cgs.clientinfo[clientNum].infoValid || clientNum == cg.snap->ps.clientNum)
	{
		int i;
		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (i != cg.snap->ps.clientNum && cgs.clientinfo[i].infoValid &&
			    cgs.clientinfo[i].team != 3 /* spectator */)
			{
				clientNum = i;
				break;
			}
		}
		if (i == MAX_CLIENTS) return;
	}

	const char *info = CG_ConfigString(CS_PLAYERS + clientNum);
	*nme = Info_ValueForKey(info, "n");
}

static void CG_ModernHUDElementNameGetPairDuel(const char** own, const char** nme)
{
	if (cg.snap->ps.persistant[PERS_TEAM] == 3 /* spectator */)
	{
		for (int i = 0, k = 0; i < MAX_CLIENTS; ++i)
		{
			if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team != 3 /* spectator */)
			{
				if (k == 0)
				{
					++k;
					*own = cgs.clientinfo[i].name;
				}
				else
				{
					*nme = cgs.clientinfo[i].name;
				}
			}
		}
	}
	else
	{
		*own = cgs.clientinfo[cg.snap->ps.clientNum].name;

		for (int i = 0; i < MAX_CLIENTS; ++i)
		{
			if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team != 3 /* spectator */)
			{
				*nme = cgs.clientinfo[i].name;
			}
		}
	}
}

static void CG_ModernHUDElementNameGetPairSides(const char** own, const char** nme)
{
	int our_side;

	our_side = cgs.clientinfo[cg.clientNum].team;

	if (our_side == 3 /* spectator */)
	{
		our_side = cgs.clientinfo[cg.snap->ps.clientNum].team;
	}

	switch (our_side)
	{
		case 3: /* spectator */
		case 1: /* red */
			*own = "Red";
			*nme = "Blue";
			break;
		case 2: /* blue */
			*own = "Blue";
			*nme = "Red";
			break;
		default:
			break;
	}
}

static void CG_ModernHUDElementNameGetPair(const char** own, const char** nme)
{
	int gt = cgs.gametype;
	*own = "";
	*nme = "";

	if (wiredHud->isTeamGame)
	{
		CG_ModernHUDElementNameGetPairSides(own, nme);
	}
	else if (wiredHud->isDuel)
	{
		CG_ModernHUDElementNameGetPairDuel(own, nme);
	}
	else
	{
		/* FFA / default */
		CG_ModernHUDElementNameGetPairFFA(own, nme);
	}
}

void CG_ModernHUDElementNameRoutine(void* context)
{
	modernHudElementName_t* element = (modernHudElementName_t*)context;
	const char*  own;
	const char*  nme;

	CG_ModernHUDElementNameGetPair(&own, &nme);

	if (element->type == ModernHUDENAME_TYPE_OWN)
	{
		element->ctx.text = own;
	}
	else
	{
		element->ctx.text = nme;
	}

	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif /* FEAT_WIRED_UI */
