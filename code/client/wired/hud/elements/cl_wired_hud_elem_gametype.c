#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI



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
		if (wiredHud->isDuel)
		{
			const char* player1Name = NULL;
			const char* player2Name = NULL;
			int i;

			for (i = 0; i < cgs.maxclients; ++i)
			{
				if (cgs.clientinfo[i].name[0] && cgs.clientinfo[i].team == 0)
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
			Com_sprintf(str, 512, "%s", wiredHud->gametypeName);
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

#endif // FEAT_WIRED_UI
