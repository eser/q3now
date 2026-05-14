// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../../../client.h"
#include "../cl_wired_hud_compat.h"
#include "../cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef enum
{
	ModernHUD_ELEMENT_LOCAL_TIME,
	ModernHUD_ELEMENT_LOCAL_DATE,
} modernHudElementLocalTimeType_t;

typedef struct
{
	modernhudConfig_t config;
	int timePrev;
	char s[MAX_QPATH];
	modernhudTextContext_t ctx;
	modernHudElementLocalTimeType_t type;
} modernHudElementLocalTime_t;

void* CG_ModernHUDElementCreateDateTime(const modernhudConfig_t* config, modernHudElementLocalTimeType_t type)
{
	modernHudElementLocalTime_t* element;

	ModernHUD_ELEMENT_INIT(element, config);

	element->type = type;

	CG_ModernHUDTextMakeContext(&element->config, &element->ctx);
	CG_ModernHUDFillAndFrameForText(&element->config, &element->ctx);
	element->ctx.text = &element->s[0];

	return element;
}

void* CG_ModernHUDElementLocalTimeCreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementCreateDateTime(config, ModernHUD_ELEMENT_LOCAL_TIME);
}

void* CG_ModernHUDElementLocalDateCreate(const modernhudConfig_t* config)
{
	return CG_ModernHUDElementCreateDateTime(config, ModernHUD_ELEMENT_LOCAL_DATE);
}

void CG_ModernHUDElementLocalTimeRoutine(void* context)
{
	modernHudElementLocalTime_t* element = (modernHudElementLocalTime_t*)context;
	qtime_t qtime;

	if (cg.time - element->timePrev > 1000)
	{
		element->timePrev = cg.time;
		trap_RealTime(&qtime);
		if (element->type == ModernHUD_ELEMENT_LOCAL_TIME)
		{
			if (element->config.style.isSet && element->config.style.value == 1)
			{
				int hour = qtime.tm_hour;
				const char* ampm = "AM";

				if (hour == 0)
					hour = 12;
				else if (hour == 12)
					ampm = "PM";
				else if (hour > 12)
				{
					hour -= 12;
					ampm = "PM";
				}

				Com_sprintf(element->s, MAX_QPATH, "%02d:%02d %s", hour, qtime.tm_min, ampm);
			}
			else
			{
				// 24-часовой формат (по умолчанию)
				Com_sprintf(element->s, MAX_QPATH, "%02d:%02d", qtime.tm_hour, qtime.tm_min);
			}
		}
		else if (element->type == ModernHUD_ELEMENT_LOCAL_DATE)
		{
			if (element->config.style.isSet && element->config.style.value == 1)
			{
				Com_sprintf(element->s, MAX_QPATH, "%02d.%02d.%04d",
				            qtime.tm_mon + 1,
				            qtime.tm_mday,
				            qtime.tm_year + 1900
				           );
			}
			else
				Com_sprintf(element->s, MAX_QPATH, "%02d.%02d.%04d",
				            qtime.tm_mday,
				            qtime.tm_mon + 1,
				            qtime.tm_year + 1900
				           );
		}

	}
	CG_ModernHUDTextPrint(&element->config, &element->ctx);
}

#endif // FEAT_WIRED_UI
