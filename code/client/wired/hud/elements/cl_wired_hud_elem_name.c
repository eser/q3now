/* cl_wired_hud_elem_name.c -- Player name HUD elements (OWN / NME)
   Displays player names based on gametype using pre-computed fields
   from the bridge (wiredHud). Numeric team and gametype IDs avoid
   direct use of game enum constants. */
#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

enum shudElementNameType_t
{
	SHUDENAME_TYPE_OWN,
	SHUDENAME_TYPE_NME,
};

typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
	enum shudElementNameType_t type;
} shudElementName_t;

static void* CG_SHUDElementNameCreate(const superhudConfig_t* config, enum shudElementNameType_t type)
{
	shudElementName_t* element;

	SHUD_ELEMENT_INIT(element, config);

	element->type = type;

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void* CG_SHUDElementNameOWNCreate(const superhudConfig_t* config)
{
	return CG_SHUDElementNameCreate(config, SHUDENAME_TYPE_OWN);
}

void* CG_SHUDElementNameNMECreate(const superhudConfig_t* config)
{
	return CG_SHUDElementNameCreate(config, SHUDENAME_TYPE_NME);
}

static void CG_SHUDElementNameGetPairFFA(const char** own, const char** nme)
{
	int clientNum;
	const char* info;
	*own = cgs.clientinfo[cg.snap->ps.clientNum].name;

	clientNum = cg.predictedPlayerState.persistant[PERS_LAST_ATTACKER];
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

	info = CG_ConfigString(CS_PLAYERS + clientNum);
	*nme = Info_ValueForKey(info, "n");
}

static void CG_SHUDElementNameGetPairDuel(const char** own, const char** nme)
{
	int i;
	int k;

	if (cg.snap->ps.persistant[PERS_TEAM] == 3 /* spectator */)
	{
		for (i = 0, k = 0; i < MAX_CLIENTS; ++i)
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

		for (i = 0; i < MAX_CLIENTS; ++i)
		{
			if (cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team != 3 /* spectator */)
			{
				*nme = cgs.clientinfo[i].name;
			}
		}
	}
}

static void CG_SHUDElementNameGetPairSides(const char** own, const char** nme)
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

static void CG_SHUDElementNameGetPair(const char** own, const char** nme)
{
	int gt = cgs.gametype;
	*own = "";
	*nme = "";

	if (wiredHud->isTeamGame)
	{
		CG_SHUDElementNameGetPairSides(own, nme);
	}
	else if (wiredHud->isDuel)
	{
		CG_SHUDElementNameGetPairDuel(own, nme);
	}
	else
	{
		/* FFA / default */
		CG_SHUDElementNameGetPairFFA(own, nme);
	}
}

void CG_SHUDElementNameRoutine(void* context)
{
	shudElementName_t* element = (shudElementName_t*)context;
	const char*  own;
	const char*  nme;

	CG_SHUDElementNameGetPair(&own, &nme);

	if (element->type == SHUDENAME_TYPE_OWN)
	{
		element->ctx.text = own;
	}
	else
	{
		element->ctx.text = nme;
	}

	CG_SHUDTextPrint(&element->config, &element->ctx);
}

void CG_SHUDElementNameDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}
#endif /* FEAT_WIRED_UI */
