#include "cg_local.h"
#include "cg_superhud_private.h"


typedef struct
{
	superhudConfig_t config;
	int timePrev;
	superhudTextContext_t ctx;
} shudElementGameType_t;

void* CG_SHUDElementGameTypeCreate(const superhudConfig_t* config)
{
	shudElementGameType_t* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

void CG_SHUDElementGameTypeRoutine(void* context)
{
	shudElementGameType_t* element = (shudElementGameType_t*)context;
	char str[512];

	int sec = cg.warmup;

	element->ctx.text = NULL;

	if (sec > 0)
	{
		if (cgs.gametype == GT_TOURNAMENT)
		{
			const char* player1Name = NULL;
			const char* player2Name = NULL;
			int i;

			for (i = 0; i < cgs.maxclients; ++i)
			{
				if (cgs.clientinfo[i].name[0] && cgs.clientinfo[i].team == TEAM_FREE)
				{
					if (!player1Name)
					{
						player1Name = cgs.clientinfo[i].name;
					}
					else
					{
						player2Name = cgs.clientinfo[i].name;
						break;
					}
				}
			}
			if (player1Name && player2Name)
			{
				Com_sprintf(str, 512, "%s^7 vs %s", player1Name, player2Name);
				element->ctx.text = str;
			}
		}
		else
		{
			const char* text;
			int len;
			int width;

			switch ( cgs.gametype ) {
				case GT_FFA:            text = "Free for all"; break;
				case GT_TOURNAMENT:     text = "Tournament"; break;
				case GT_SINGLE_PLAYER:  text = "Single Player"; break;
				case GT_TEAM:           text = "Team Deathmatch"; break;
				case GT_CTF:            text = "Capture the Flag"; break;
				case GT_KINGOFTHEHILL:  text = "King of the Hill"; break;
				default:                text = ""; break;
			}
			Com_sprintf(str, 512, "%s", text);
			element->ctx.text = str;
		}
	}
	if (!element->ctx.text)
	{
		return;
	}

	CG_SHUDTextPrint(&element->config, &element->ctx);
}

void CG_SHUDElementGameTypeDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}

