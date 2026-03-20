#include "cg_local.h"
#include "cg_superhud_private.h"


typedef struct
{
	superhudConfig_t config;
	superhudTextContext_t ctx;
	qboolean enemy;
} shudElementStatusbarTeamCount;

void* CG_SHUDElementTeamCountCreate(const superhudConfig_t* config, qboolean enemy)
{
	shudElementStatusbarTeamCount* element;

	SHUD_ELEMENT_INIT(element, config);

	//load defaults
	if (!element->config.color.isSet)
	{
		element->config.color.isSet = qtrue;
		element->config.color.value.type = SUPERHUD_COLOR_RGBA;
		Vector4Set(element->config.color.value.rgba, 1, 0.7, 0, 1);
	}

	if (!element->config.text.isSet)
	{
		element->config.text.isSet = qtrue;
		Q_strncpyz(element->config.text.value, "%i", sizeof(element->config.text.value));
	}

	CG_SHUDTextMakeContext(&element->config, &element->ctx);
	CG_SHUDFillAndFrameForText(&element->config, &element->ctx);

	element->enemy = enemy;

	return element;
}

void* CG_SHUDElementTeamCountOWNCreate(const superhudConfig_t* config)
{
	return CG_SHUDElementTeamCountCreate(config, qfalse);
}

void* CG_SHUDElementTeamCountNMECreate(const superhudConfig_t* config)
{
	return CG_SHUDElementTeamCountCreate(config, qtrue);
}

void CG_SHUDElementTeamCountRoutine(void* context)
{
	shudElementStatusbarTeamCount* element = (shudElementStatusbarTeamCount*)context;
	int count;
	team_t team;
	team_t countTeam;
	int i;

	if ( cgs.gametype < GT_TEAM ) {
		return;
	}

	team = CG_SHUDGetOurActiveTeam();

	if (team == TEAM_RED)
	{
		countTeam = element->enemy ? TEAM_BLUE : TEAM_RED;
	}
	else if (team == TEAM_BLUE)
	{
		countTeam = element->enemy ? TEAM_RED : TEAM_BLUE;
	}
	else
	{
		return;
	}

	// count alive players on the target team
	count = 0;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( cgs.clientinfo[i].infoValid && cgs.clientinfo[i].team == countTeam ) {
			count++;
		}
	}

	if (count >= 0)
	{
		element->ctx.text = va(element->config.text.value, count);
		CG_SHUDTextPrint(&element->config, &element->ctx);
	}

}

void CG_SHUDElementTeamCountDestroy(void* context)
{
	if (context)
	{
		Z_Free(context);
	}
}

