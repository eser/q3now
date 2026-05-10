// Wired UI: ModernHUD drawing helpers migrated from cg_modernhud_util.c
#include "../../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_text.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI

typedef struct
{
	vec4_t bar1_value;
	vec4_t bar1_bottom;
	vec4_t bar2_value;
	vec4_t bar2_bottom;
} drawBarCoords_t;

/* Old CG_ModernHUDConfigPickColor / CG_ModernHUDConfigPickBgColor removed --
   replaced by CG_ModernHUDConfigPickColorGeneric below. */

static void CG_ModernHUDConfigPickColorGeneric(
    const qboolean* mainIsSet,
    const modernhudColor_t* mainValue,
    const qboolean* altIsSet,
    const modernhudColor_t* altValue,
    float* color,
    qboolean alphaOverride)
{
	const modernhudColor_t* in = mainValue;
	const float* target;
	float finalAlpha = 1.0f;

	if (!(*mainIsSet))
	{
		if (alphaOverride)
		{
			Vector4Copy(colorWhite, color);
		}
		else
		{
			VectorCopy(colorWhite, color);
		}
		return;
	}

	switch (in->type)
	{
		case MODERNHUD_COLOR_RGBA:
			target = in->rgba;
			finalAlpha = in->rgba[3];
			break;

		case MODERNHUD_COLOR_T:
			target = wiredHud->isOurTeamBlue ? colorBlue : colorRed;
			break;

		case MODERNHUD_COLOR_E:
			target = wiredHud->isOurTeamBlue ? colorRed : colorBlue;
			break;

		case MODERNHUD_COLOR_I:
		default:
			target = colorWhite;
			break;
	}

	if ((*altIsSet) && in->type != MODERNHUD_COLOR_RGBA)
	{
		finalAlpha = altValue->rgba[3];
	}

	if (alphaOverride)
	{
		color[0] = target[0];
		color[1] = target[1];
		color[2] = target[2];
		color[3] = finalAlpha;
	}
	else
	{
		color[0] = target[0];
		color[1] = target[1];
		color[2] = target[2];
		color[3] = finalAlpha;
	}
}
void CG_ModernHUDConfigPickColor(const modernhudConfig_t* config, float* color, qboolean alphaOverride)
{
	CG_ModernHUDConfigPickColorGeneric(&config->color.isSet, &config->color.value,
	                              &config->color2.isSet, &config->color2.value,
	                              color, alphaOverride);
}

void CG_ModernHUDConfigPickBgColor(const modernhudConfig_t* config, float* color, qboolean alphaOverride)
{
	CG_ModernHUDConfigPickColorGeneric(&config->bgcolor.isSet, &config->bgcolor.value,
	                              &config->bgcolor2.isSet, &config->bgcolor2.value,
	                              color, alphaOverride);
}

void CG_ModernHUDConfigPickBorderColor(const modernhudConfig_t* config, float* color, qboolean alphaOverride)
{
	CG_ModernHUDConfigPickColorGeneric(&config->borderColor.isSet, &config->borderColor.value,
	                              &config->borderColor2.isSet, &config->borderColor2.value,
	                              color, alphaOverride);
}



static void CG_ModernHUDConfigDefaultsCheck(modernhudConfig_t* config)
{
	if (!config->rect.isSet)
	{
		config->rect.value[0] = 0.0f;
		config->rect.value[1] = 0.0f;
		config->rect.value[2] = 320.0f;
		config->rect.value[3] = 12.0f;
		config->rect.isSet = qtrue;
	}

	if (!config->textAlign.isSet)
	{
		config->textAlign.value = MODERNHUD_ALIGNH_LEFT;
		config->textAlign.isSet = qtrue;
	}

	/* if fontsize wasn't set, find max size for rect */
	if (!config->fontsize.isSet)
	{
		config->fontsize.value[1] = config->rect.value[3];
		config->fontsize.value[0] = config->fontsize.value[1] / 1.618f;
		config->fontsize.isSet = qtrue;
	}

	if (!config->textStyle.isSet)
	{
		config->textStyle.value = 0; //drop shadow
		config->textStyle.isSet = qtrue;
	}

	if (!config->color.isSet)
	{
		Vector4Copy(colorWhite, config->color.value.rgba);
		config->color.value.type = MODERNHUD_COLOR_RGBA;
		config->color.isSet = qtrue;
	}
}

static void CG_ModernHUDTextMakeAdjustCoords(const modernhudConfig_t* in, float* out_x, float* out_y)
{
	modernhudAlignH_t h;
	modernhudAlignV_t v;

	if (!in->rect.isSet)
	{
		return;
	}

	if (!in->alignH.isSet)
	{
		h = MODERNHUD_ALIGNH_LEFT;
	}
	else
	{
		h = in->alignH.value;
	}

	if (!in->alignV.isSet)
	{
		v = MODERNHUD_ALIGNV_CENTER;
	}
	else
	{
		v = in->alignV.value;
	}

	switch (h)
	{
		case MODERNHUD_ALIGNH_LEFT:
			//allready x
			*out_x = in->rect.value[0];
			break;
		case MODERNHUD_ALIGNH_CENTER:
			// x + width/2
			*out_x = in->rect.value[0] + in->rect.value[2] / 2.0f;
			break;
		case MODERNHUD_ALIGNH_RIGHT:
			// x + width
			*out_x = in->rect.value[0] + in->rect.value[2];
			break;
	}

	switch (v)
	{
		case MODERNHUD_ALIGNV_TOP:
			*out_y = in->rect.value[1];
			break;
		case MODERNHUD_ALIGNV_CENTER:
			*out_y = in->rect.value[1] + in->rect.value[3] / 2.0f;
			break;
		case MODERNHUD_ALIGNV_BOTTOM:
			*out_y = in->rect.value[1] + in->rect.value[3];
			break;
	}

}

void CG_ModernHUDTextMakeContext(const modernhudConfig_t* in, modernhudTextContext_t* out)
{
	modernhudConfig_t config;
	memset(out, 0, sizeof(*out));
	memcpy(&config, in, sizeof(config));

	CG_ModernHUDConfigDefaultsCheck(&config);

	CG_ModernHUDTextMakeAdjustCoords(in, &out->coord.named.x, &out->coord.named.y);

	switch (config.textAlign.value)
	{
		default:
		case MODERNHUD_ALIGNH_LEFT:
			out->flags |= DS_HLEFT;
			break;
		case MODERNHUD_ALIGNH_CENTER:
			out->flags |= DS_HCENTER;
			break;
		case MODERNHUD_ALIGNH_RIGHT:
			out->flags |= DS_HRIGHT;
			break;
	}

	out->coord.named.w = config.fontsize.value[0];
	out->coord.named.h = config.fontsize.value[1];

	if (config.alignV.isSet)
	{
		switch (config.alignV.value)
		{
			default:
			case MODERNHUD_ALIGNV_TOP:
				out->flags |= DS_VTOP;
				break;
			case MODERNHUD_ALIGNV_CENTER:
				out->flags |= DS_VCENTER;
				break;
			case MODERNHUD_ALIGNV_BOTTOM:
				out->flags |= DS_VBOTTOM;
				break;
		}
	}

	if (!config.monospace.isSet)
	{
		out->flags |= DS_PROPORTIONAL;
	}
	if (config.textStyle.value & 1)
	{
		out->flags |= DS_SHADOW;
	}

	if (!config.shadowColor.isSet)
	{
		Vector4Copy(colorBlack, out->shadowColor);
		if (config.color.isSet)
		{
			out->shadowColor[3] = config.color.value.rgba[3];
		}
	}
	else
	{
		Vector4Copy(config.shadowColor.value, out->shadowColor);
	}

	out->fontId = WiredFont_IdFromName(config.font.isSet ? config.font.value : "defaultSansFont");
	if ( config.fontWeight.isSet ) {
		if ( config.fontWeight.value >= 700 ) {
			if ( out->fontId == FONT_DISPLAY ) out->fontId = FONT_DISPLAY_BOLD;
			else if ( out->fontId == FONT_UI ) out->fontId = FONT_UI_MEDIUM;
		} else if ( config.fontWeight.value >= 500 ) {
			if ( out->fontId == FONT_UI ) out->fontId = FONT_UI_MEDIUM;
		}
	}
	out->width = (float)cls.glconfig.vidWidth;


	CG_ModernHUDConfigPickColor(&config, out->color, qtrue);
	Vector4Copy(out->color, out->color_origin);
	out->letterSpacing = config.letterspacing.isSet ? config.letterspacing.value : 0.0f;
}


void CG_ModernHUDDrawMakeContext(const modernhudConfig_t* in, modernhudDrawContext_t* out)
{
	modernhudConfig_t config;
	memset(out, 0, sizeof(*out));
	memcpy(&config, in, sizeof(config));

	CG_ModernHUDConfigDefaultsCheck(&config);

	out->coord.named.x = config.rect.value[0];
	out->coord.named.y = config.rect.value[1];
	out->coord.named.w = config.rect.value[2];
	out->coord.named.h = config.rect.value[3];

	out->coordPicture.named.x = 0.0f;
	out->coordPicture.named.y = 0.0f;
	out->coordPicture.named.w = 1.0f;
	out->coordPicture.named.h = 1.0f;

	CG_ModernHUDConfigPickColor(&config, out->color, qtrue);
	Vector4Copy(out->color, out->color_origin);
}

void CG_ModernHUDBarMakeContext(const modernhudConfig_t* in, modernhudBarContext_t* out, float max)
{
	float x = 0, y = 0;
	float bar_height, bar_width;
	modernhudConfig_t config;
	memset(out, 0, sizeof(*out));
	memcpy(&config, in, sizeof(config));

	CG_ModernHUDConfigDefaultsCheck(&config);

	if (!config.direction.isSet)
	{
		config.direction.isSet = qtrue;
		config.direction.value = MODERNHUD_DIR_LEFT_TO_RIGHT;
	}

	if (!config.style.isSet) // set default style
	{
		config.style.isSet = qtrue;
		config.style.value = 1;
	}
	else if (config.style.value < 1 || config.style.value > 3) // need to rework
	{
		config.style.value = 1;
	}

	out->direction = config.direction.value;

	x = config.rect.value[0];
	y = config.rect.value[1];

	if (config.doublebar.isSet)
	{
		static const float bar_gap = 4;
		out->two_bars = qtrue;
		if (out->direction == MODERNHUD_DIR_LEFT_TO_RIGHT || out->direction == MODERNHUD_DIR_RIGHT_TO_LEFT)
		{
			if (config.style.value == 1)    // style 1 - default: split into two bars
			{

				bar_height = config.rect.value[3] / 2 - bar_gap / 2; //split horizontal
				bar_width = config.rect.value[2];

				out->bar[0][0] = x;//x
				out->bar[0][1] = y;//y
				out->bar[0][2] = bar_width;//w
				out->bar[0][3] = bar_height;   // height is half of rect and minus half of the gap between two bars
				//
				out->bar[1][0] = x;//x
				out->bar[1][1] = y + bar_height + bar_gap;
				out->bar[1][2] = bar_width;//w
				out->bar[1][3] = bar_height;//h height is same as in first bar
			}
			else if (config.style.value == 2 || config.style.value == 3) // style 2 - same start point for both bars
			{
				// all the same for same bars
				out->bar[1][0] = out->bar[0][0] = x; // x
				out->bar[1][1] = out->bar[0][1] = y; // y
				out->bar[1][2] = out->bar[0][2] = config.rect.value[2]; // w
				out->bar[1][3] = out->bar[0][3] = config.rect.value[3]; // h
			}
			out->max = out->bar[1][2];
			out->koeff = 2 * out->bar[1][2] / max;
		}
		else
		{
			if (config.style.value == 1)    // style 1 - default: split into two bars
			{
				bar_height = config.rect.value[3];
				bar_width = config.rect.value[2] / 2 - bar_gap / 2;

				out->bar[0][0] = x;//x
				out->bar[0][1] = y;//y
				out->bar[0][2] = bar_width;//w
				out->bar[0][3] = bar_height;   //h
				//
				out->bar[1][0] = x + bar_width + bar_gap;
				out->bar[1][1] = y;//y
				out->bar[1][2] = bar_width;//w
				out->bar[1][3] = bar_height;//h
			}
			else if (config.style.value == 2 || config.style.value == 3)   // style 2 - same start point for both bars
			{
				// all the same for same bars
				out->bar[1][0] = out->bar[0][0] = x; // x
				out->bar[1][1] = out->bar[0][1] = y; // y
				out->bar[1][2] = out->bar[0][2] = config.rect.value[2]; // w
				out->bar[1][3] = out->bar[0][3] = config.rect.value[3]; // h
			}
			out->max = out->bar[1][3];
			out->koeff = 2 * out->bar[1][3] / max;
		}
	}
	else
	{
		// single bar
		out->bar[0][0] = x;
		out->bar[0][1] = y;
		out->bar[0][2] = config.rect.value[2];
		out->bar[0][3] = config.rect.value[3];
		if (out->direction == MODERNHUD_DIR_LEFT_TO_RIGHT || out->direction == MODERNHUD_DIR_RIGHT_TO_LEFT)
		{
			out->max = out->bar[0][2]; // max / width
		}
		else
		{
			out->max = out->bar[0][3]; // max / height
		}
		out->koeff = out->max / max;
	}

	CG_ModernHUDConfigPickColor(&config, out->color_top, qtrue);
	if (config.bgcolor.isSet)
	{
		Vector4Copy(config.bgcolor.value.rgba, out->color_back);
	}
	else
	{
		Vector4Set(out->color_back, 0, 0, 0, 0);
	}
}

qboolean CG_ModernHUDIsTimeOut(const modernhudConfig_t* cfg, int startTime)
{
	if (!startTime)
	{
		return qtrue;
	}
	if (cfg->time.isSet)
	{
		if ((cg.time - startTime) < cfg->time.value)
		{
			//fade time is not started;
			return qfalse;
		}
	}
	return qtrue;
}

/*
   *  Затухание
   *  возвращает qfalse если элемент потух
   */
qboolean CG_ModernHUDGetFadeColor(const vec4_t from_color, vec4_t out, const modernhudConfig_t* cfg, int startTime)
{
	int time = 0;

	Vector4Copy(from_color, out);

	if (!CG_ModernHUDIsTimeOut(cfg, startTime))
	{
		return qtrue;
	}

	if (cfg->time.isSet)
	{
		time = cfg->time.value;
	}

	if (cfg->fade.isSet)
	{
		int fadetime;
		float fadedelay = MODERNHUD_DEFAULT_FADEDELAY;

		fadetime = cg.time - startTime - time;

		if (cfg->fadedelay.isSet)
		{
			fadedelay = (float)cfg->fadedelay.value;
		}

		if (fadetime > 0 && fadetime < fadedelay)
		{
			vec4_t tmpfade;
			float k = (float)fadetime / fadedelay;
			Vector4Copy(cfg->fade.value, tmpfade);
			Vector4Subtract(tmpfade, from_color, tmpfade);
			Vector4MA(from_color, k, tmpfade, out);
			return qtrue;
		}

		return qfalse;
	}

	return qfalse;
}

void CG_ModernHUDTextPrint(const modernhudConfig_t* cfg, modernhudTextContext_t* ctx)
{
	if (!ctx->text || !ctx->text[0])
	{
		return;
	}

	CG_ModernHUDConfigPickColor(cfg, ctx->color, qfalse);
	Text_SetLetterSpacing( ctx->letterSpacing );

	Text_Draw( ctx->text,
	           ctx->coord.named.x,
	           ctx->coord.named.y,
	           ctx->fontId,
	           ctx->coord.named.h,
	           ctx->color,
	           WiredFont_ToAlignment( ctx->flags ),
	           WiredFont_ToTextFlags( ctx->flags ) );
	Text_SetLetterSpacing( 0.0f );
}

void CG_ModernHUDTextPrintNew(const modernhudConfig_t* cfg, modernhudTextContext_t* ctx, qboolean colorOverride)
{
	if (!ctx->text || !ctx->text[0])
	{
		return;
	}
	if (colorOverride)
		CG_ModernHUDConfigPickColor(cfg, ctx->color, qfalse);
	Text_SetLetterSpacing( ctx->letterSpacing );

	Text_Draw( ctx->text,
	           ctx->coord.named.x,
	           ctx->coord.named.y,
	           ctx->fontId,
	           ctx->coord.named.h,
	           ctx->color,
	           WiredFont_ToAlignment( ctx->flags ),
	           WiredFont_ToTextFlags( ctx->flags ) );
	Text_SetLetterSpacing( 0.0f );
}

static void CG_ModernHUDBarPreparePrintLTR(const modernhudBarContext_t* ctx, float value, drawBarCoords_t* coords)
{
	if (ctx->two_bars)
	{
		float width;

		Vector4Copy(ctx->bar[0], coords->bar1_value);
		Vector4Copy(ctx->bar[0], coords->bar1_bottom);
		Vector4Copy(ctx->bar[1], coords->bar2_value);
		Vector4Copy(ctx->bar[1], coords->bar2_bottom);
		//bar1
		width = value * ctx->koeff;
		if (width > ctx->max)
		{
			width = ctx->max;
		}
		if (width < 0)
		{
			width = 0;
		}
		coords->bar1_value[2] = width;

		//bar2
		width = value * ctx->koeff;
		width -= ctx->max;

		if (width > ctx->max)
		{
			width = ctx->max;
		}
		if (width < 0)
		{
			width = 0;
		}

		coords->bar2_value[2] = width;
	}
	else
	{
		float width;
		Vector4Copy(ctx->bar[0], coords->bar1_value);
		Vector4Copy(ctx->bar[0], coords->bar1_bottom);
		width = value * ctx->koeff;
		if (width > ctx->max)
		{
			width = ctx->max;
		}
		coords->bar1_value[2] = width;
	}
}

static void CG_ModernHUDBarPreparePrintRTL(const modernhudBarContext_t* ctx, float value, drawBarCoords_t* coords)
{
	//Just make left-to-right and mirror it
	CG_ModernHUDBarPreparePrintLTR(ctx, value, coords);

	coords->bar1_value[0] = coords->bar1_value[0] + ctx->bar[0][2] - coords->bar1_value[2]; //x = x + max_width - width
	coords->bar2_value[0] = coords->bar2_value[0] + ctx->bar[1][2] - coords->bar2_value[2]; //x = x + max_width - width
}

static void CG_ModernHUDBarPreparePrintTTB(const modernhudBarContext_t* ctx, float value, drawBarCoords_t* coords)
{
	if (ctx->two_bars)
	{
		float height;

		Vector4Copy(ctx->bar[0], coords->bar1_value);
		Vector4Copy(ctx->bar[0], coords->bar1_bottom);
		Vector4Copy(ctx->bar[1], coords->bar2_value);
		Vector4Copy(ctx->bar[1], coords->bar2_bottom);
		//bar1
		height = value * ctx->koeff;
		if (height > ctx->max)
		{
			height = ctx->max;
		}
		if (height < 0)
		{
			height = 0;
		}
		coords->bar1_value[3] = height;

		//bar2
		height = value * ctx->koeff;
		height -= ctx->max;

		if (height > ctx->max)
		{
			height = ctx->max;
		}
		if (height < 0)
		{
			height = 0;
		}

		coords->bar2_value[3] = height;
	}
	else
	{
		float height;
		Vector4Copy(ctx->bar[0], coords->bar1_value);
		Vector4Copy(ctx->bar[0], coords->bar1_bottom);
		height = value * ctx->koeff;
		if (height > ctx->max)
		{
			height = ctx->max;
		}
		if (height < 0)
		{
			height = 0;
		}
		coords->bar1_value[2] = height;
	}
}

static void CG_ModernHUDBarPreparePrintBTT(const modernhudBarContext_t* ctx, float value, drawBarCoords_t* coords)
{
	//Just make top to bottom and mirror it
	CG_ModernHUDBarPreparePrintTTB(ctx, value, coords);

	coords->bar1_value[1] += ctx->bar[0][3] - coords->bar1_value[3]; //y = y + max_height - height
	coords->bar2_value[1] += ctx->bar[1][3] - coords->bar2_value[3]; //y = y + max_height - height
}

void CG_ModernHUDBarPrint(const modernhudConfig_t* cfg, modernhudBarContext_t* ctx, float value)
{
	drawBarCoords_t coords;

	memset(&coords, 0, sizeof(coords));

	switch (ctx->direction)
	{
		default:
		case MODERNHUD_DIR_LEFT_TO_RIGHT:
			CG_ModernHUDBarPreparePrintLTR(ctx, value, &coords);
			break;
		case MODERNHUD_DIR_RIGHT_TO_LEFT:
			CG_ModernHUDBarPreparePrintRTL(ctx, value, &coords);
			break;
		case MODERNHUD_DIR_TOP_TO_BOTTOM:
			CG_ModernHUDBarPreparePrintTTB(ctx, value, &coords);
			break;
		case MODERNHUD_DIR_BOTTOM_TO_TOP:
			CG_ModernHUDBarPreparePrintBTT(ctx, value, &coords);
			break;
	}

	trap_R_SetColor(ctx->color_back);
	Wired_DrawPic(coords.bar1_bottom[0], coords.bar1_bottom[1], coords.bar1_bottom[2], coords.bar1_bottom[3],
	                      0, 0, 0, 0,
	                      cgs.media.whiteShader);
	if (ctx->two_bars)
	{
		Wired_DrawPic(coords.bar2_bottom[0], coords.bar2_bottom[1], coords.bar2_bottom[2], coords.bar2_bottom[3],
		                      0, 0, 0, 0,
		                      cgs.media.whiteShader);
	}
	trap_R_SetColor(ctx->color_top);
	Wired_DrawPic(coords.bar1_value[0], coords.bar1_value[1], coords.bar1_value[2], coords.bar1_value[3],
	                      0, 0, 0, 0,
	                      cgs.media.whiteShader);
	if (ctx->two_bars)
	{
		if (cfg->style.value == 2 || cfg->style.value == 3)
		{
			trap_R_SetColor(ctx->color2_top); // 2nd bar color
		}

		Wired_DrawPic(coords.bar2_value[0], coords.bar2_value[1], coords.bar2_value[2], coords.bar2_value[3],
		                      0, 0, 0, 0,
		                      cgs.media.whiteShader);
	}
	trap_R_SetColor(NULL);
}

team_t CG_ModernHUDGetOurActiveTeam(void)
{
	return (team_t)wiredHud->ourActiveTeam;
}

void CG_ModernHUDFillWithColor(const modernhudCoord_t* coord, const float* color)
{
	float x = coord->named.x;
	float y = coord->named.y;
	float w = coord->named.w;
	float h = coord->named.h;
	trap_R_SetColor(color);
	Wired_DrawPic(x, y, w, h, 0, 0, 0, 0, cgs.media.whiteShader);
	trap_R_SetColor(NULL);
}

qboolean CG_ModernHUDFill(const modernhudConfig_t* cfg)
{
	if (!cfg->fill.isSet || !cfg->rect.isSet)
	{
		return qfalse;
	}

	float x = cfg->rect.value[0];
	float y = cfg->rect.value[1];
	float w = cfg->rect.value[2];
	float h = cfg->rect.value[3];
	vec4_t bgColor;

	if (cfg->bgcolor.isSet)
	{
		CG_ModernHUDConfigPickBgColor(cfg, bgColor, qfalse);

		trap_R_SetColor(bgColor);
		Wired_DrawPic(x, y, w, h, 0, 0, 0, 0, cgs.media.whiteShader);
		trap_R_SetColor(NULL);
		return qtrue;
	}

	if (cfg->image.isSet)
	{
		vec4_t color;
		qhandle_t image = trap_R_RegisterShader(cfg->image.value);
		if (!image)
		{
			return qfalse;
		}

		CG_ModernHUDConfigPickColor(cfg, color, qfalse);

		trap_R_SetColor(color);
		Wired_DrawPic(x, y, w, h, 0, 0, 1, 1, image);
		trap_R_SetColor(NULL);
		return qtrue;
	}

	return qfalse;
}



void CG_ModernHUDElementCompileTeamOverlayConfig(int fontWidth, modernHudTeamOverlay_t* configOut)
{
	configOut->powerupOffsetChar = 0;
	configOut->powerupOffsetPix = configOut->powerupOffsetChar * fontWidth;
	configOut->powerupLenChar = 1;
	configOut->powerupLenPix = configOut->powerupLenChar * fontWidth;

	configOut->nameOffsetChar = 1;
	configOut->nameOffsetPix = configOut->nameOffsetChar * fontWidth;
	configOut->nameLenChar = 12;
	configOut->nameLenPix = configOut->nameLenChar * fontWidth;

	configOut->healthAndArmorOffsetChar = 14;
	configOut->healthAndArmorOffsetPix = configOut->healthAndArmorOffsetChar * fontWidth;
	configOut->healthAndArmorLenChar = 7; // 200/200
	configOut->healthAndArmorLenPix = configOut->healthAndArmorLenChar * fontWidth;

	configOut->weaponOffsetChar = 21;
	configOut->weaponOffsetPix = configOut->weaponOffsetChar * fontWidth;
	configOut->weaponLenChar = 1;
	configOut->weaponLenPix = configOut->weaponLenChar * fontWidth;

	configOut->locationOffsetChar = 23;
	configOut->locationOffsetPix = configOut->locationOffsetChar * fontWidth;
	configOut->locationLenChar = cg_MaxlocationWidth.integer;
	configOut->locationLenPix = configOut->locationLenChar * fontWidth;

	configOut->overlayWidthChar = configOut->locationOffsetChar + cg_MaxlocationWidth.integer;
	configOut->overlayWidthPix = configOut->overlayWidthChar * fontWidth;
}

//
// Wired_DrawPic Wrapper
//
// float x       X coord of result image
// float y       Y coord of result image
// float w       Width of result image
// float h       Height of result image
// float s1      X coord in the shader (0.0f...1.0f)
// float t1      Y coord in the shader (0.0f...1.0f)
// float s2      Width of image in the shader (0.0f...1.0f)
// float t2      Height of image in the shader (0.0f...1.0f)
// float *color  Use this color
// qhandle_t shader Shader
//
void CG_ModernHUDDrawStretchPic(modernhudCoord_t coord, const modernhudCoord_t coordPicture, const float* color, qhandle_t shader)
{
	if (!shader) return;

	trap_R_SetColor(color);
	Wired_DrawPic(coord.named.x,
	                      coord.named.y,
	                      coord.named.w,
	                      coord.named.h,
	                      coordPicture.named.x,
	                      coordPicture.named.y,
	                      coordPicture.named.w,
	                      coordPicture.named.h,
	                      shader);
	trap_R_SetColor(NULL);
}

void CG_ModernHUDDrawBorderDirect(const modernhudCoord_t* coord, const vec4_t border, const vec4_t borderColor)
{
	vec4_t tmpCoord;
	if (coord == 0)
	{
		return;
	}

	Vector4Copy(coord->arr, tmpCoord);

	CG_ModernDrawFrame(tmpCoord[0], tmpCoord[1], tmpCoord[2], tmpCoord[3],
	                (float*)border, (float*)borderColor, qtrue);

}

qboolean CG_ModernHUDDrawBorder(const modernhudConfig_t* cfg)
{
	vec4_t coord;
	vec4_t borderColor;

	if (!cfg->border.isSet || !cfg->rect.isSet || !cfg->borderColor.isSet)
	{
		return qfalse;
	}

	Vector4Copy(cfg->rect.value, coord);

	CG_ModernHUDConfigPickBorderColor(cfg, borderColor, qfalse);

	CG_ModernDrawFrame(coord[0], coord[1], coord[2], coord[3],
	                (float*)cfg->border.value, borderColor, qfalse);

	return qtrue;
}

void CG_ModernHUDFillAndFrameForText(modernhudConfig_t* cfg, modernhudTextContext_t* ctx)
{
	qboolean drawBackground = (cfg->bgcolor.isSet && cfg->fill.isSet);
	qboolean drawBorder = (cfg->border.isSet && cfg->borderColor.isSet);

	if (!drawBackground && !drawBorder)
	{
		return;
	}

	if (drawBackground)
	{
		CG_ModernHUDConfigPickBgColor(cfg, ctx->background, qfalse);
	}

	if (drawBorder)
	{
		Vector4Copy(cfg->border.value, ctx->border);
		CG_ModernHUDConfigPickBorderColor(cfg, ctx->borderColor, qfalse);
	}
}

void CG_ModernHUDDrawStretchPicCtx(const modernhudConfig_t* cfg, modernhudDrawContext_t* ctx)
{
	// we have to pick color again, because team could changed
	CG_ModernHUDConfigPickColor(cfg, ctx->color, qfalse);
	CG_ModernHUDDrawStretchPic(ctx->coord, ctx->coordPicture, ctx->color, ctx->image);
}

int CG_ModernHUDGetAmmo(int wpi)
{
	int ammo = cg.snap->ps.ammo[wpi];

	if (ammo < 0) ammo = 0;
	if (ammo > 999) ammo = 999;
	return ammo;
}

#endif // FEAT_WIRED_UI
