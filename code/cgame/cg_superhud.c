#include "cg_local.h"

#include "cg_superhud_private.h"

static superhudElement_t* elementsHead;

static void CG_SHUDRoutenesDestroy(superhudElement_t* shud)
{
	superhudElement_t* tmp;

	while (shud)
	{
		if (shud->element.destroy)
		{
			shud->element.destroy(shud->element.context);
		}
		tmp = shud;
		shud = shud->next;
		Z_Free(tmp);
	}

}

void CG_SHUDRoutenesDestroyAll(void)
{
	CG_SHUDRoutenesDestroy(elementsHead);
	elementsHead = NULL;
}

qboolean CG_SHUDIsLoaded(void)
{
	return elementsHead != NULL;
}

static void CG_SHUDUpdateVisflags(superHUDConfigElement_t* element, const superhudConfig_t* config)
{
	if (element && config && config->visflags.isSet)
	{
		element->visibility = config->visflags.value;
	}
}

qboolean CG_SHUDLoadConfigPrivate(const char* filename)
{
	static char fileContent[32768];
	fileHandle_t fileHandle;
	int rc;
	qboolean filePrepared;
	qboolean eof = qfalse;
	configFileInfo_t finfo = {0, NULL, NULL};
	superhudElement_t* newElementsHead = NULL;
	superhudElement_t* newElementLast = NULL;
	superhudConfig_t* defaultElement = NULL;
	superhudConfigParseElement_t statusElement;
	superhudConfigParseCommand_t statusCommand;
	char superhudFilename[MAX_QPATH];
	int numberOfElements = 0;
	int numberOfElementsCreated = 0;

	/* loading config file into stack/static buffer — NOT pool */
	Com_sprintf(superhudFilename, MAX_QPATH, "hud/%s.cfg", filename);

	rc = trap_FS_FOpenFile(superhudFilename, &fileHandle, FS_READ);
	if (rc < 0 || !fileHandle)
	{
		CG_Printf("^1SuperHUD: config file: %s is not found.\n", superhudFilename);
		return qfalse;
	}
	if (rc == 0)
	{
		CG_Printf("^1SuperHUD: config file: %s is empty.\n", superhudFilename);
		return qfalse;
	}
	if (rc >= (int)sizeof(fileContent))
	{
		CG_Printf("^1SuperHUD: config file: %s is too large (%d bytes).\n", superhudFilename, rc);
		trap_FS_FCloseFile(fileHandle);
		return qfalse;
	}

	trap_FS_Read(fileContent, rc, fileHandle);
	trap_FS_FCloseFile(fileHandle);
	fileContent[rc] = 0;


	filePrepared = CG_SHUDFileInfoInit(&finfo, fileContent);
	if (!filePrepared)
	{
		CG_Printf("^1SuperHUD: config file: could not prepare %s for parsing.\n", superhudFilename);
		return qfalse;
	}

	do
	{
		statusElement = CG_SHUDFileInfoGetElementItem(&finfo);
		switch (statusElement.status)
		{
			case SUPERHUD_CONFIG_OK:
				break;
			case SUPERHUD_CONFIG_UNEXPECTED_CHARACTER:
				CG_Printf("^1SuperHUD: found unexpected character: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
				goto error_exit;
			case SUPERHUD_CONFIG_WRONG_ELEMENT_NAME:
				CG_Printf("^1SuperHUD: found unknown element name: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
				goto error_exit;
			case SUPERHUD_CONFIG_LOST_ELEMENT_BODY:
				CG_Printf("^1SuperHUD: found wrong wrong element body: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
				goto error_exit;
			case SUPERHUD_CONFIG_END_OF_FILE:
				eof = qtrue;
				break;
			case SUPERHUD_CONFIG_END_OF_ELEMENT:
			case SUPERHUD_CONFIG_WRONG_COMMAND_NAME:
				CG_Printf("^1SuperHUD: found command name or end of element: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
				goto error_exit;
		}
		if (statusElement.status == SUPERHUD_CONFIG_OK)
		{
			qboolean eoe = qfalse; //end of element
			qboolean is_default = qfalse;
			// parse element body
			if (!newElementsHead)
			{
				newElementsHead = Z_Malloc(sizeof(*newElementsHead));
				OSP_MEMORY_CHECK(newElementsHead);
			}
			if (!newElementLast)
			{
				newElementLast = newElementsHead;
			}
			else
			{
				newElementLast->next = Z_Malloc(sizeof(*newElementsHead));
				OSP_MEMORY_CHECK(newElementLast->next);
				newElementLast = newElementLast->next;
			}

			is_default = strcmp(statusElement.item->name, "!default") == 0;

			// if this element is not default and we already got default, set it
			if (!is_default && defaultElement)
			{
				memcpy(&newElementLast->config, defaultElement, sizeof(newElementLast->config));
			}

			do
			{
				statusCommand = CG_SHUDFileInfoGetCommandItem(&finfo);
				switch (statusCommand.status)
				{
					case SUPERHUD_CONFIG_OK:
						break;
					case SUPERHUD_CONFIG_UNEXPECTED_CHARACTER:
						CG_Printf("^1SuperHUD: found unexpected character: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
						goto error_exit;
					case SUPERHUD_CONFIG_END_OF_FILE:
						CG_Printf("^1SuperHUD: found unexpected end of file: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
						goto error_exit;
					case SUPERHUD_CONFIG_END_OF_ELEMENT:
						eoe = qtrue;
						break;
					case SUPERHUD_CONFIG_WRONG_ELEMENT_NAME:
					case SUPERHUD_CONFIG_LOST_ELEMENT_BODY:
						CG_Printf("^1SuperHUD: found unexpected result: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
						goto error_exit;
					case SUPERHUD_CONFIG_WRONG_COMMAND_NAME:
						CG_Printf("^3SuperHUD: found unknown command name: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
						goto error_exit;
				}
				if (statusCommand.status == SUPERHUD_CONFIG_OK)
				{
					superhudConfigParseStatus_t parseStatus;
					parseStatus = statusCommand.item->parse(&finfo, &newElementLast->config);
					switch (parseStatus)
					{
						case SUPERHUD_CONFIG_OK:
							break;
						case SUPERHUD_CONFIG_UNEXPECTED_CHARACTER:
							CG_Printf("^1SuperHUD: found unexpected character: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
							goto error_exit;
						case SUPERHUD_CONFIG_END_OF_FILE:
							CG_Printf("^1SuperHUD: found unexpected end of file: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
							goto error_exit;
						case SUPERHUD_CONFIG_END_OF_ELEMENT:
							CG_Printf("^1SuperHUD: found unexpected end of element: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
							goto error_exit;
						case SUPERHUD_CONFIG_WRONG_ELEMENT_NAME:
						case SUPERHUD_CONFIG_WRONG_COMMAND_NAME:
						case SUPERHUD_CONFIG_LOST_ELEMENT_BODY:
							CG_Printf("^1SuperHUD: found unexpected result: file %s, line %d, pos %d.\n", superhudFilename, finfo.last_line->line_number, finfo.pos);
							goto error_exit;
							break;
					}
				}
			}
			while (!eoe);
			++numberOfElements;
			memcpy(&newElementLast->element, statusElement.item, sizeof(newElementLast->element));

			//set default
			if (is_default)
			{
				defaultElement = &newElementLast->config;
			}
		}
	}
	while (!eof);

	if (numberOfElements == 0)
	{
		CG_Printf("^3SuperHUD: ignored empty config file\n");
		goto error_exit;
	}
	else
	{

		/* creating HUD routines */
		newElementLast = newElementsHead;

		while (newElementLast)
		{
			if (newElementLast->element.create)
			{
				newElementLast->element.context = newElementLast->element.create(&newElementLast->config);

				CG_SHUDUpdateVisflags(&newElementLast->element, &newElementLast->config);

				if (newElementLast->element.context)
				{
					++numberOfElementsCreated;
				}
			}
			newElementLast = newElementLast->next;
		}
	}

	/* sorting HUD routines... */
	{
		superhudElement_t* prev;
		superhudElement_t* current;
		superhudElement_t* next;
		int swapped;

		do
		{
			swapped = 0;
			prev = NULL;
			current = newElementsHead;
			next = newElementsHead->next;

			while (current && next)
			{
				if (next->element.order < current->element.order)
				{
					//swap current and next
					// prev->current->next->after next
					if (prev)
					{
						prev->next = next;
					}
					else
					{
						newElementsHead = next;
					}

					current->next = next->next;
					next->next = current;
					++swapped;
				}

				prev = current;
				current = next;
				next = current->next;
			}
		}
		while (swapped);


	}



	CG_SHUDRoutenesDestroy(elementsHead);
	elementsHead = newElementsHead;

	CG_Printf("^3SuperHUD: loaded config file: %s: %d drawable element%s.\n", superhudFilename, numberOfElementsCreated, numberOfElementsCreated > 1 ? "s" : "");

	CG_SHUDFileInfoTeardown(&finfo);
	return qtrue;

error_exit:
	CG_SHUDRoutenesDestroy(newElementsHead);
	CG_SHUDFileInfoTeardown(&finfo);
	return qfalse;
}

/*
 * Load SuperHUD config
 */
void CG_SHUDLoadConfig(void)
{
	if (!CG_SHUDLoadConfigPrivate(cg_hudFile.string))
	{
		CG_Printf("^1SuperHUD: couldn't load file %s, going to load default.cfg\n", cg_hudFile.string);
		if (!CG_SHUDLoadConfigPrivate("default"))
		{
			CG_Printf("^1SuperHUD: couldn't load default config\n");
		}
	}
}

void CG_SHUDRoutine(void)
{
	superhudElement_t* last = elementsHead;
	int vflags;
	qboolean skip;
	qboolean is_dead, is_intermission, is_team_game, is_spectator, is_scores;
	qboolean is_gt_ffa, is_gt_tourney, is_gt_tdm, is_gt_ctf, is_gt_ft, is_gt_ca;

	if ( !cg.snap || !last ) {
		return;
	}

	/* SuperHUD uses 640x480 virtual coords that stretch to fill the screen.
	   Without this, widescreen correction squishes text horizontally. */
	CG_SetScreenPlacement( PLACE_STRETCH, PLACE_STRETCH );

	is_dead = cg.predictedPlayerState.pm_type == PM_DEAD;
	is_intermission = cg.predictedPlayerState.pm_type == PM_INTERMISSION;
	is_team_game = cgs.gametype >= GT_TEAM;
	is_spectator = CG_IsSpectator();
	is_scores = cg.showScores;
	is_gt_ffa = cgs.gametype == GT_FFA;
	is_gt_tourney = cgs.gametype == GT_TOURNAMENT;
	is_gt_tdm = cgs.gametype == GT_TOURNAMENT == GT_TEAM;
	is_gt_ctf = cgs.gametype == GT_CTF;
	is_gt_ft = CG_ModernIsGameTypeFreeze();
	is_gt_ca = qfalse; // cgs.gametype == GT_CLANARENA ?

	while (last)
	{
		// check visibility
		vflags = last->config.visiblity.isSet ? last->config.visiblity.value : last->element.visibility;

		skip = (!(vflags & SE_IM) && is_intermission) ||
		       ((vflags & SE_TEAM_ONLY) && (!is_team_game)) ||
		       (!(vflags & SE_DEAD) && is_dead) ||
		       (!(vflags & SE_SPECT) && is_spectator) ||
		       ((vflags & SE_SCORES_HIDE) && is_scores) ||
		       ((vflags & SE_DEMO_HIDE) && cg.demoPlayback) ||
		       ((vflags & SE_GT_FFA) && is_gt_ffa) ||
		       ((vflags & SE_GT_TDM) && is_gt_tdm) ||
		       ((vflags & SE_GT_CTF) && is_gt_ctf) ||
		       ((vflags & SE_GT_TOURNEY) && is_gt_tourney) ||
		       ((vflags & SE_GT_FREEZETAG) && is_gt_ft) ||
		       ((vflags & SE_GT_CLANARENA) && is_gt_ca)
		       ;

		if (!skip && last->element.routine)
		{
			last->element.routine(last->element.context);
		}
		last = last->next;
	}

	CG_PopScreenPlacement();
}

void CG_SHUDEventFrag(const char* message)
{
	superhudGlobalContext_t* ctx = CG_SHUDGetContext();
	Q_strncpyz(ctx->fragmessage.message, message, sizeof(ctx->fragmessage.message));
	ctx->fragmessage.time = cg.time;

	if (cgs.gametype < GT_TEAM)
	{
		ctx->rankmessage.time = cg.time;
		Com_sprintf(ctx->rankmessage.message, sizeof(ctx->rankmessage.message), "%s place with %i", CG_PlaceString(cg.snap->ps.persistant[PERS_RANK] + 1), cg.snap->ps.persistant[PERS_SCORE]);
	}
	else
	{
		ctx->rankmessage.time = 0;
		ctx->rankmessage.message[0] = 0;
	}

#if FEAT_WIRED_UI
	// push combined frag+rank as atomic pair to client-side message queue
	if ( ctx->rankmessage.time ) {
		trap_WiredUI_PushEvent( WIRED_EVENT_FRAG_RANK, va( "%s|%s", message, ctx->rankmessage.message ) );
	} else {
		trap_WiredUI_PushEvent( WIRED_EVENT_FRAG_RANK, message );
	}
#endif
}

void CG_SHUDEventChat(const char* message)
{
	superhudGlobalContext_t* ctx = CG_SHUDGetContext();
	int index;

	index = ctx->chat.index % SHUD_MAX_CHAT_LINES;
	Q_strncpyz(ctx->chat.line[index].message, message, MAX_SAY_TEXT);
	ctx->chat.line[index].time = cg.time;
	++ctx->chat.index;

#if FEAT_WIRED_UI
	trap_WiredUI_PushEvent( WIRED_EVENT_CHAT, message );
#endif
}

void CG_SHUDEventTeamChat(const char* message)
{
	int len = 0;
	char* loc_start;
	char* loc_end;
	char* p;
	int size = 1024;
	int index;
	superhudGlobalContext_t* ctx = CG_SHUDGetContext();

	index = ctx->chat.index % SHUD_MAX_CHAT_LINES;

	p = ctx->chat.line[index].message;
	(*(p) = 0);
	while (*message)
	{
		if (Q_IsColorString(message))
		{
			*p++ = *message++;
			*p++ = *message++;
			continue;
		}
		*p++ = *message++;
		len++;
	}

	*p = 0;

	ctx->chat.line[index].time = cg.time;
	++ctx->chat.index;
}

void CG_SHUDEventObituaries(int attacker, int target, int mod, qboolean unfrozen)
{
	int i;
	superhudGlobalContext_t* ctx = CG_SHUDGetContext();
	superhudObituariesEntry_t* entry = &ctx->obituaries.line[ctx->obituaries.index % SHUD_MAX_OBITUARIES_LINES];

	memset(entry, 0, sizeof(*entry));

	entry->attacker = attacker;
	entry->target = target;
	entry->mod = mod;
	entry->unfrozen = unfrozen;
	entry->time = cg.time;
	entry->attackerTeam = cgs.clientinfo[attacker].team;
	entry->targetTeam = cgs.clientinfo[target].team;
	entry->runtime.isInitialized = qfalse;

	++ctx->obituaries.index;
}

void CG_SHUDEventTempAccuracy(int weapon, float accuracy)
{
	superhudGlobalContext_t* ctx = CG_SHUDGetContext();
	superhudTempAccEntry_t* entry = &ctx->tempAcc.weapon[weapon];


	memset(entry, 0, sizeof(*entry));

	entry->tempAccuracy = accuracy;

}
