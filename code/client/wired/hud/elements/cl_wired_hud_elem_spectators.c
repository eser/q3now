#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"
#include "cl_wired_store.h"

#if FEAT_WIRED_UI



typedef struct
{
	modernhudConfig_t config;
	modernhudTextContext_t ctx;
} modernHudElementSpectators_t;

void* CG_ModernHUDElementSpectatorsCreate(const modernhudConfig_t* config)
{
	modernHudElementSpectators_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);

	return element;
}

static qboolean CG_ModernHUD_SpectatorsBuildString(char* out, int outSize, const modernhudConfig_t* config)
{
	wuiStoreEntry_t *e;
	int len = 0;

	e = WiredStore_Get( "game.spectators.list" );
	if ( !e || !e->text[0] ) {
		out[0] = '\0';
		return qfalse;
	}

	if (config->style.isSet && config->style.value & 2)
	{
		Q_strncpyz(out, "", outSize);
	}
	else
	{
		Q_strncpyz(out, "Spectators:", outSize);
	}
	len = strlen(out);

	Q_strncpyz(out + len, e->text, outSize - len);

	return qtrue;
}

void CG_ModernHUDElementSpectatorsRoutine(void* context)
{
	modernHudElementSpectators_t* element = (modernHudElementSpectators_t*)context;
	static char buffer[MAX_STRING_CHARS];

	if (!CG_ModernHUD_SpectatorsBuildString(buffer, sizeof(buffer), &element->config))
	{
		if (!ModernHUD_CHECK_SHOW_EMPTY(element))
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
	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

void CG_ModernHUDElementSpectatorsDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}


#endif // FEAT_WIRED_UI
