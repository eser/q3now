#include "../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI



typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
} shudElementSpectators_t;

void* CG_SHUDElementSpectatorsCreate(const superhudConfig_t* config)
{
	shudElementSpectators_t* element;

	SHUD_ELEMENT_INIT(element, config);

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

static qboolean CG_SHUD_SpectatorsBuildString(char* out, int outSize, const superhudConfig_t* config)
{
	int len = 0;
	int clientId;
	qboolean hasSpectators = qfalse;

	if (config->style.isSet && config->style.value & 2)
	{
		Q_strncpyz(out, "", outSize);
	}
	else
	{
		Q_strncpyz(out, "Spectators:", outSize);
	}
	len = strlen(out);

	for (clientId = 0; clientId < MAX_CLIENTS; ++clientId)
	{
		if (cgs.clientinfo[clientId].infoValid && cgs.clientinfo[clientId].team == TEAM_SPECTATOR)
		{
			const char* name = cgs.clientinfo[clientId].name;
			if (!name || !name[0])
			{
				continue;
			}
			if (hasSpectators)
			{
				Q_strncpyz(out + len, ", ", outSize - len);
				len = strlen(out);
				if (len >= outSize)
					break;
			}
			Q_strncpyz(out + len, name, outSize - len);
			len = strlen(out);
			if (len >= outSize)
				break;
			hasSpectators = qtrue;
		}
	}

	if (!hasSpectators)
	{
		out[0] = '\0';
		return qfalse;
	}

	return qtrue;
}

void CG_SHUDElementSpectatorsRoutine(void* context)
{
	shudElementSpectators_t* element = (shudElementSpectators_t*)context;
	static char buffer[MAX_STRING_CHARS];

	if (!CG_SHUD_SpectatorsBuildString(buffer, sizeof(buffer), &element->config))
	{
		if (!SHUD_CHECK_SHOW_EMPTY(element))
		{
			return;
		}
		if (element->config.style.isSet && element->config.style.value & 2)
		{
			Q_strncpyz(buffer, "", sizeof(buffer));
		}
		else
		{
			Q_strncpyz(buffer, "Spectators:", sizeof(buffer));
		}
	}

	element->ctx.text = buffer;
	CG_SHUDTextPrint(&element->config, &element->ctx);
}

void CG_SHUDElementSpectatorsDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}


#endif // FEAT_WIRED_UI
