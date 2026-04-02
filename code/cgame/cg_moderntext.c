/*
===========================================================================
cg_moderntext.c -- Modern font system and text rendering

Provides: font loading, text compilation, CG_ModernDrawString,
CG_ModernDrawStringNew, CG_Hex16GetColor.

Our cg_drawtools.c is NOT replaced -- it keeps CG_AdjustFrom640,
CG_DrawStringExt, CG_GetColorForAmount, proportional font, etc.
===========================================================================
*/
#include "cg_local.h"
#include "cg_modern_private.h"

/*
 * ── Hex color parsing ────────────────────────────────────
 */
static qboolean CG_ModernCharHexToInt(char c, int *out)
{
	if (c >= '0' && c <= '9') { *out = c - '0'; return qtrue; }
	if (c >= 'a' && c <= 'f') { *out = c - 'a' + 10; return qtrue; }
	if (c >= 'A' && c <= 'F') { *out = c - 'A' + 10; return qtrue; }
	return qfalse;
}

qboolean CG_Hex16GetColor(const char *str, float *color)
{
	int d1, d2, color_int;
	if (!str) return qfalse;
	if (!CG_ModernCharHexToInt(str[0], &d1)) return qfalse;
	if (!CG_ModernCharHexToInt(str[1], &d2)) return qfalse;
	color_int = d1 * 16 + d2;
	*color = (float)color_int / 255.0f;
	return qtrue;
}

/*
 * ── Text compiler ────────────────────────────────────────
 * Parses a Q3 string with color codes into an array of
 * text_command_t for efficient rendering.
 */

/* Cut time-related symbols like ^f ^F */
void CG_ModernDrawStringPrepare(const char *from, char *to, int size)
{
	int printed = 0;
	int max = size - 1;

	if (!from || !to) return;

	while (*from && printed < max)
	{
		if (from[0] == '^' && from[1] != '^')
		{
			switch (from[1])
			{
				case 'f':
					from += 2;
					if ((cg.time & 0x3ff) >= 512)
					{
						while (*from && !(from[0] == '^' && (from[1] == 'N' || from[1] == 'F')))
							++from;
					}
					break;
				case 'F':
					from += 2;
					if ((cg.time & 0x3ff) < 512)
					{
						while (*from && !(from[0] == '^' && (from[1] == 'N' || from[1] == 'f')))
							++from;
					}
					break;
				default:
					break;
			}
		}
		*to = *from;
		++to;
		++from;
		++printed;
	}
	*to = 0;
}

text_command_t *CG_CompileText(const char *in)
{
	static text_command_t commands[OSP_TEXT_CMD_MAX];
	static char dmem[4096];
	int b;
	char *text;
	int i = 0;
	int len;
	vec4_t back_color;
	qboolean back_color_was_set = qfalse;
	qboolean top_color_was_set = qfalse;
	float rc, gc, bc;
	unsigned int color_index;

	if (!in || *in == 0) return NULL;

	Vector4Copy(colorWhite, back_color);

	len = strlen(in) + 1;
	if (len > (int)sizeof(dmem)) len = sizeof(dmem);
	text = dmem;

	CG_ModernDrawStringPrepare(in, text, len);

	while (*text)
	{
		if (text[0] == '^' && text[1])
		{
			switch (text[1])
			{
				case 'F':
				case 'f':
					text += 2;
					break;
				case 'B':
					if (!top_color_was_set && back_color_was_set)
					{
						commands[i].type = OSP_TEXT_CMD_TEXT_COLOR;
						VectorCopy(back_color, commands[i].value.color);
						++i;
					}
					b = cg.time & 0x7ff;
					if (b > 1024) b = ~b & 0x7ff;
					commands[i].type = OSP_TEXT_CMD_FADE;
					commands[i].value.fade = b / 1463.0f + 0.3f;
					++i;
					text += 2;
					break;
				case 'b':
					if (!top_color_was_set && back_color_was_set)
					{
						commands[i].type = OSP_TEXT_CMD_TEXT_COLOR;
						VectorCopy(back_color, commands[i].value.color);
						++i;
					}
					b = cg.time & 0x7ff;
					if (b > 1024) b = ~b & 0x7ff;
					commands[i].type = OSP_TEXT_CMD_FADE;
					commands[i].value.fade = b / 1024.0f;
					++i;
					text += 2;
					break;
				case 'N':
				case 'n':
					commands[i].type = OSP_TEXT_CMD_FADE;
					commands[i].value.fade = 1.0f;
					++i;
					if (!top_color_was_set && back_color_was_set)
					{
						commands[i].type = OSP_TEXT_CMD_TEXT_COLOR;
						VectorCopy(back_color, commands[i].value.color);
						++i;
					}
					else if (top_color_was_set && !back_color_was_set)
					{
						commands[i].type = OSP_TEXT_CMD_TEXT_COLOR;
						VectorCopy(colorWhite, commands[i].value.color);
						++i;
					}
					text += 2;
					break;
				case '^':
					commands[i].type = OSP_TEXT_CMD_CHAR;
					commands[i].value.character = '^';
					++i;
					text += 1;
					break;
				case 'X':
				case 'x':
					if (!CG_Hex16GetColor(&text[2], &rc)) { text += 2; break; }
					if (!CG_Hex16GetColor(&text[4], &gc)) { text += 2; break; }
					if (!CG_Hex16GetColor(&text[6], &bc)) { text += 2; break; }
					back_color[0] = rc;
					back_color[1] = gc;
					back_color[2] = bc;
					commands[i].type = OSP_TEXT_CMD_SHADOW_COLOR;
					VectorCopy(back_color, commands[i].value.color);
					back_color_was_set = qtrue;
					++i;
					text += 8;
					break;
				default:
					if ((text[1] >= '0') && (text[1] <= '9'))
					{
						color_index = text[1] - 0x30;
						VectorCopy(g_color_table[color_index], commands[i].value.color);
						commands[i].type = OSP_TEXT_CMD_TEXT_COLOR;
						++i;
						top_color_was_set = qtrue;
					}
					text += 2;
					break;
			}
		}
		else
		{
			commands[i].type = OSP_TEXT_CMD_CHAR;
			commands[i].value.character = text[0];
			++i;
			++text;
		}
	}
	commands[i++].type = OSP_TEXT_CMD_STOP;

	return commands;
}

void CG_CompiledTextDestroy(text_command_t *root)
{
	/* static buffer — nothing to free */
}

/*
 * ── Font system ──────────────────────────────────────────
 * Multiple font atlases with per-character metrics loaded from
 * .cfg data files in gfx/2d/.
 */
#define MAX_FONT_SHADERS 4

typedef struct
{
	float tc_prop[4];
	float tc_mono[4];
	float space1;
	float space2;
	float width;
} font_metric_t;

typedef struct
{
	const char     *name;
	font_metric_t   metrics[256];
	qhandle_t       shader[MAX_FONT_SHADERS];
	int             shaderThreshold[MAX_FONT_SHADERS];
	int             shaderCount;
} font_t;

static font_t fonts[] = {
	{"id"}, {"idblock"}, {"sansman"}, {"cpma"}, {"m1rage"},
	{"elite_emoji"}, {"diablo"}, {"eternal"}, {"qlnumbers"},
	{"elite"}, {"elitebigchars"}
};
static int fonts_num = sizeof(fonts) / sizeof(fonts[0]);
static const font_t *font = &fonts[0];
static const font_metric_t *metrics = &fonts[0].metrics[0];

qboolean CG_FontAvailable(int index)
{
	if (index >= 0 && index < fonts_num) return qtrue;
	CG_Printf("Nonexistent font number: ^1%d\n", index);
	return qfalse;
}

void CG_FontSelect(int index)
{
	if (CG_FontAvailable(index))
	{
		if (index < 0 || index >= fonts_num)
		{
			CG_Error("Requested nonexistent font number: %d\n", index);
		}
		font = &fonts[index];
		metrics = &font->metrics[0];
	}
}

int CG_FontIndexFromName(const char *name)
{
	int index;
	for (index = 0; index < fonts_num; ++index)
	{
		if (Q_stricmp(name, fonts[index].name) == 0)
			return index;
	}
	return 0;
}

static qboolean CG_FileExist(const char *file)
{
	fileHandle_t f;
	if (!file || !file[0]) return qfalse;
	trap_FS_FOpenFile(file, &f, FS_READ);
	if (f == FS_INVALID_HANDLE) return qfalse;
	trap_FS_FCloseFile(f);
	return qtrue;
}

static void CG_LoadFont(font_t *fnt, const char *fontName)
{
	char buf[8000];
	fileHandle_t f;
	const char *token;
	const char *text;
	float width, height, r_width, r_height;
	float char_width, char_height;
	char shaderName[MAX_FONT_SHADERS][MAX_QPATH], tmpName[MAX_QPATH];
	int shaderCount;
	int shaderThreshold[MAX_FONT_SHADERS];
	font_metric_t *fm;
	int i, tmp, len, chars;
	float w1, w2, s1, s2, x0, y0;
	qboolean swapped;

	len = trap_FS_FOpenFile(fontName, &f, FS_READ);
	if (f == FS_INVALID_HANDLE)
	{
		CG_Printf(S_COLOR_YELLOW "CG_LoadFont: error opening %s\n", fontName);
		return;
	}

	if (len >= (int)sizeof(buf))
	{
		CG_Printf(S_COLOR_YELLOW "CG_LoadFont: font file too long: %i\n", len);
		len = sizeof(buf) - 1;
	}

	trap_FS_Read(buf, len, f);
	trap_FS_FCloseFile(f);
	buf[len] = '\0';

	shaderCount = 0;
	text = buf;
	COM_BeginParseSession(fontName);

	while (1)
	{
		token = COM_ParseExt(&text, qtrue);
		if (token[0] == '\0')
		{
			Com_Printf(S_COLOR_RED "CG_LoadFont: parse error.\n");
			return;
		}

		if (strcmp(token, "img") == 0)
		{
			if (shaderCount >= MAX_FONT_SHADERS)
			{
				Com_Printf("CG_LoadFont: too many font images, ignoring.\n");
				SkipRestOfLine(&text);
				continue;
			}
			token = COM_ParseExt(&text, qfalse);
			if (!CG_FileExist(token))
			{
				Com_Printf("CG_LoadFont: font image '%s' doesn't exist.\n", token);
				return;
			}
			Q_strncpyz(shaderName[shaderCount], token, sizeof(shaderName[shaderCount]));
			token = COM_ParseExt(&text, qfalse);
			shaderThreshold[shaderCount] = atoi(token);
			shaderCount++;
			SkipRestOfLine(&text);
			continue;
		}

		if (strcmp(token, "fnt") == 0)
		{
			token = COM_ParseExt(&text, qfalse);
			if (token[0] == '\0' || (width = atof(token)) <= 0.0)
			{
				Com_Printf("CG_LoadFont: error reading image width.\n");
				return;
			}
			r_width = 1.0 / width;

			token = COM_ParseExt(&text, qfalse);
			if (token[0] == '\0' || (height = atof(token)) <= 0.0)
			{
				Com_Printf("CG_LoadFont: error reading image height.\n");
				return;
			}
			r_height = 1.0 / height;

			token = COM_ParseExt(&text, qfalse);
			if (token[0] == '\0')
			{
				Com_Printf("CG_LoadFont: error reading char width.\n");
				return;
			}
			char_width = atof(token);

			token = COM_ParseExt(&text, qfalse);
			if (token[0] == '\0')
			{
				Com_Printf("CG_LoadFont: error reading char height.\n");
				return;
			}
			char_height = atof(token);
			break;
		}
	}

	if (shaderCount == 0)
	{
		Com_Printf("CG_LoadFont: no font images specified in %s.\n", fontName);
		return;
	}

	fm = fnt->metrics;
	chars = 0;

	for (;;)
	{
		token = COM_ParseExt(&text, qtrue);
		if (!token[0]) break;

		if (token[0] == '\'' && token[1] && token[2] == '\'')
			i = token[1] & 255;
		else
			i = atoi(token);

		if (i < 0 || i > 255)
		{
			CG_Printf(S_COLOR_RED "CG_LoadFont: bad char index %i.\n", i);
			return;
		}
		fm = fnt->metrics + i;

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { CG_Printf(S_COLOR_RED "CG_LoadFont: error reading x0.\n"); return; }
		x0 = atof(token);

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { CG_Printf(S_COLOR_RED "CG_LoadFont: error reading y0.\n"); return; }
		y0 = atof(token);

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { CG_Printf(S_COLOR_RED "CG_LoadFont: error reading x-offset.\n"); return; }
		w1 = atof(token);

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { CG_Printf(S_COLOR_RED "CG_LoadFont: error reading x-length.\n"); return; }
		w2 = atof(token);

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { CG_Printf(S_COLOR_RED "CG_LoadFont: error reading space1.\n"); return; }
		s1 = atof(token);

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { CG_Printf(S_COLOR_RED "CG_LoadFont: error reading space2.\n"); return; }
		s2 = atof(token);

		fm->tc_mono[0] = x0 * r_width;
		fm->tc_mono[1] = y0 * r_height;
		fm->tc_mono[2] = (x0 + char_width) * r_width;
		fm->tc_mono[3] = (y0 + char_height) * r_height;

		fm->tc_prop[1] = fm->tc_mono[1];
		fm->tc_prop[3] = fm->tc_mono[3];

		fm->width = w2 / char_width;
		fm->space1 = s1 / char_width;
		fm->space2 = (s2 + w2) / char_width;
		fm->tc_prop[0] = fm->tc_mono[0] + (w1 * r_width);
		fm->tc_prop[2] = fm->tc_prop[0] + (w2 * r_width);

		chars++;
		SkipRestOfLine(&text);
	}

	/* sort images by threshold (bubble sort) */
	do
	{
		swapped = qfalse;
		for (i = 1; i < shaderCount; i++)
		{
			if (shaderThreshold[i - 1] > shaderThreshold[i])
			{
				tmp = shaderThreshold[i - 1];
				shaderThreshold[i - 1] = shaderThreshold[i];
				shaderThreshold[i] = tmp;
				strcpy(tmpName, shaderName[i - 1]);
				strcpy(shaderName[i - 1], shaderName[i]);
				strcpy(shaderName[i], tmpName);
				swapped = qtrue;
			}
		}
	} while (swapped);

	shaderThreshold[0] = 0;

	fnt->shaderCount = shaderCount;
	for (i = 0; i < shaderCount; i++)
	{
		fnt->shader[i] = trap_R_RegisterShaderNoMip(shaderName[i]);
		fnt->shaderThreshold[i] = shaderThreshold[i];
	}

	CG_Printf("Font '%s' loaded with %i chars and %i images\n", fontName, chars, shaderCount);
}

void CG_LoadFonts(void)
{
	CG_LoadFont(&fonts[0], "gfx/2d/bigchars.cfg");
	CG_LoadFont(&fonts[1], "gfx/2d/numbers.cfg");
	CG_LoadFont(&fonts[2], "gfx/2d/sansman.cfg");
	CG_LoadFont(&fonts[3], "gfx/2d/sansman.cfg"); /* cpma slot -> use sansman */
	CG_LoadFont(&fonts[4], "gfx/2d/m1rage.cfg");
	CG_LoadFont(&fonts[5], "gfx/2d/elite_emoji.cfg");
	CG_LoadFont(&fonts[6], "gfx/2d/diablo.cfg");
	CG_LoadFont(&fonts[7], "gfx/2d/eternal.cfg");
	CG_LoadFont(&fonts[8], "gfx/2d/qlnumbers.cfg");
	CG_LoadFont(&fonts[9], "gfx/2d/Elite.cfg");
	CG_LoadFont(&fonts[10], "gfx/2d/EliteBigchars.cfg");
}

/*
 * ── String length helpers ────────────────────────────────
 */
static float DrawCompiledStringLength(const text_command_t *cmd, float aw, int proportional)
{
	const font_metric_t *fm;
	float x_end;
	float ax = 0;
	int i;
	const text_command_t *curr;

	if (!cmd) return 0.0f;

	for (i = 0; i < OSP_TEXT_CMD_MAX; ++i)
	{
		curr = &cmd[i];
		if (curr->type == OSP_TEXT_CMD_CHAR)
		{
			fm = &metrics[(unsigned char)curr->value.character];
			if (proportional)
			{
				ax += fm->space1 * aw;
				x_end = ax + fm->space2 * aw;
			}
			else
			{
				x_end = ax + aw;
			}
			if (x_end >= cgs.glconfig.vidWidth) break;
			ax = x_end;
		}
		else if (curr->type == OSP_TEXT_CMD_STOP)
		{
			break;
		}
	}
	return ax;
}

static int CompiledStringSize(const text_command_t *cmd)
{
	int size;
	if (!cmd) return 0;
	for (size = 0; size < OSP_TEXT_CMD_MAX && cmd[size].type != OSP_TEXT_CMD_STOP; ++size);
	return size;
}

static float GetSymbolSize(char sym, qboolean proportional, float charWidth)
{
	const font_metric_t *fm;
	fm = &metrics[(unsigned char)sym];
	if (proportional) return (fm->space1 + fm->space2) * charWidth;
	return charWidth;
}

static float RestrictCompiledString(text_command_t *cmd, float charWidth, qboolean proportional, float toWidth)
{
	const font_metric_t *fm;
	float x_end;
	float ax = 0;
	int i, size;
	text_command_t *curr;
	qboolean restricted = qfalse;

	if (!cmd || toWidth == 0) return 0;

	size = CompiledStringSize(cmd);

	for (i = 0; i < size; ++i)
	{
		curr = &cmd[i];
		if (curr->type == OSP_TEXT_CMD_CHAR)
		{
			fm = &metrics[(unsigned char)curr->value.character];
			if (proportional)
			{
				ax += fm->space1 * charWidth;
				x_end = ax + fm->space2 * charWidth;
			}
			else
			{
				x_end = ax + charWidth;
			}
			if (x_end > toWidth) { restricted = qtrue; break; }
			ax = x_end;
		}
		else if (curr->type == OSP_TEXT_CMD_STOP)
		{
			break;
		}
	}

	if (restricted)
	{
		while (i > 0)
		{
			float dotSize = GetSymbolSize('.', proportional, charWidth);
			float prevSize = GetSymbolSize(cmd[i - 1].value.character, proportional, charWidth);
			if ((ax - prevSize + dotSize) <= toWidth)
			{
				--i;
				ax = ax - prevSize + dotSize;
				break;
			}
			--i;
			ax -= prevSize;
		}
		curr = &cmd[i];
		curr->type = OSP_TEXT_CMD_CHAR;
		curr->value.character = '.';
		if (i + 1 < OSP_TEXT_CMD_MAX)
			cmd[i + 1].type = OSP_TEXT_CMD_STOP;
	}
	return ax;
}

static float RestrictCompiledStringChars(text_command_t *cmd, int maxChars)
{
	int chars = 0;
	int i;
	text_command_t *curr;
	qboolean restricted = qfalse;

	if (!cmd || maxChars <= 0) return 0;

	for (i = 0; i < OSP_TEXT_CMD_MAX; ++i)
	{
		curr = &cmd[i];
		if (curr->type == OSP_TEXT_CMD_CHAR)
		{
			chars++;
			if (chars >= maxChars) { restricted = qtrue; break; }
		}
		else if (curr->type == OSP_TEXT_CMD_STOP)
		{
			break;
		}
	}

	if (restricted)
	{
		curr = &cmd[i];
		if (curr->type == OSP_TEXT_CMD_CHAR)
			curr->value.character = '.';
		if (i + 1 < OSP_TEXT_CMD_MAX)
			cmd[i + 1].type = OSP_TEXT_CMD_STOP;
	}
	return (float)chars;
}

/*
 * ── CG_ModernDrawStringLenPix ──────────────────────────────
 * Returns pixel width of a string when rendered with given params.
 */
int CG_ModernDrawStringLenPix(const char *string, float charWidth, int flags, int toWidth)
{
	float mw;
	int rez;
	text_command_t *text_commands;

	if (!string) return 0;

	text_commands = CG_CompileText(string);
	if (!text_commands) return 0;

	mw = (float)toWidth;
	if (mw > 0)
		CG_AdjustFrom640(NULL, NULL, &mw, NULL);
	RestrictCompiledString(text_commands, charWidth, flags & DS_PROPORTIONAL, mw);
	rez = DrawCompiledStringLength(text_commands, charWidth, flags & DS_PROPORTIONAL);
	CG_CompiledTextDestroy(text_commands);
	return rez;
}

/*
 * ── CG_ModernDrawString ────────────────────────────────────
 * Modern text renderer using compiled text commands
 * and the font metric system.
 *
 * Uses q3now's CG_AdjustFrom640 for widescreen coordinate scaling.
 */
void CG_ModernDrawString(float x, float y, const char *string, const vec4_t setColor,
                       float charWidth, float charHeight, int maxWidth, int flags,
                       vec4_t background)
{
	const font_metric_t *fm;
	const float *tc;
	float ax, ay, aw, aw1, ah;
	float x_end, xx;
	float fade = 1.0f;
	vec4_t color;
	float xx_add, yy_add;
	int i;
	qhandle_t sh;
	int proportional;
	text_command_t *text_commands;
	text_command_t *curr;
	float expectedLength = 0;

	if (!string) return;

	text_commands = CG_CompileText(string);
	if (!text_commands) return;

	CG_AdjustFrom640(&x, &y, &charWidth, &charHeight);

	ax = x;
	ay = y;
	aw = charWidth;
	ah = charHeight;

	proportional = (flags & DS_PROPORTIONAL);

	{
		float mw = (float)maxWidth;
		CG_AdjustFrom640(NULL, NULL, &mw, NULL);
		RestrictCompiledString(text_commands, aw, proportional, mw);
	}

	if (background || (flags & (DS_HCENTER | DS_HRIGHT)))
		expectedLength = DrawCompiledStringLength(text_commands, aw, proportional);

	if (flags & (DS_HCENTER | DS_HRIGHT))
	{
		if (flags & DS_HCENTER)
			ax -= 0.5f * expectedLength;
		else
			ax -= expectedLength;
	}

	if (flags & DS_VCENTER)
		ay -= ah / 2;
	else if (flags & DS_VTOP)
		ay -= ah;

	sh = font->shader[0];
	for (i = 1; i < font->shaderCount; i++)
	{
		if (ah >= font->shaderThreshold[i])
			sh = font->shader[i];
	}

	if (background)
	{
		trap_R_SetColor(background);
		trap_R_DrawStretchPic(ax, ay, expectedLength, ah, 0, 0, 0, 0, cgs.media.whiteShader);
		trap_R_SetColor(colorWhite);
	}

	/* shadow pass */
	if (flags & DS_SHADOW)
	{
		xx = ax;
		yy_add = xx_add = charWidth / 10.0f;

		VectorCopy(colorBlack, color);
		color[3] = fade;
		trap_R_SetColor(color);

		for (i = 0; i < OSP_TEXT_CMD_MAX && text_commands[i].type != OSP_TEXT_CMD_STOP; ++i)
		{
			curr = &text_commands[i];
			switch (curr->type)
			{
				case OSP_TEXT_CMD_CHAR:
					fm = &metrics[(unsigned char)curr->value.character];
					if (proportional)
					{
						tc = fm->tc_prop;
						aw1 = fm->width * aw;
						ax += fm->space1 * aw;
						x_end = ax + fm->space2 * aw;
					}
					else
					{
						tc = fm->tc_mono;
						aw1 = aw;
						x_end = ax + aw;
					}
					if (ax >= cgs.glconfig.vidWidth) break;
					trap_R_DrawStretchPic(ax + xx_add, ay + yy_add, aw1, ah, tc[0], tc[1], tc[2], tc[3], sh);
					ax = x_end;
					break;
				case OSP_TEXT_CMD_SHADOW_COLOR:
					VectorCopy(curr->value.color, color);
					color[3] = fade;
					if (setColor && color[3] > setColor[3]) color[3] = setColor[3];
					trap_R_SetColor(color);
					break;
				case OSP_TEXT_CMD_FADE:
					fade = curr->value.fade;
					color[3] = fade;
					if (setColor && color[3] > setColor[3]) color[3] = setColor[3];
					trap_R_SetColor(color);
					break;
				case OSP_TEXT_CMD_STOP:
				case OSP_TEXT_CMD_TEXT_COLOR:
					break;
			}
		}
		ax = xx;
	}

	/* main text pass */
	Vector4Copy(setColor, color);
	trap_R_SetColor(color);
	fade = 1.0f;

	for (i = 0; i < OSP_TEXT_CMD_MAX && text_commands[i].type != OSP_TEXT_CMD_STOP; ++i)
	{
		curr = &text_commands[i];
		switch (curr->type)
		{
			case OSP_TEXT_CMD_CHAR:
			{
				int index = curr->value.character;
				if (index < 0) index += 256;
				fm = &metrics[(unsigned char)index];
			}
			if (proportional)
			{
				tc = fm->tc_prop;
				aw1 = fm->width * aw;
				ax += fm->space1 * aw;
				x_end = ax + fm->space2 * aw;
			}
			else
			{
				tc = fm->tc_mono;
				aw1 = aw;
				x_end = ax + aw;
			}
			if (ax >= cgs.glconfig.vidWidth) break;
			trap_R_DrawStretchPic(ax, ay, aw1, ah, tc[0], tc[1], tc[2], tc[3], sh);
			ax = x_end;
			break;
			case OSP_TEXT_CMD_TEXT_COLOR:
				VectorCopy(curr->value.color, color);
				color[3] = fade;
				if (setColor && color[3] > setColor[3]) color[3] = setColor[3];
				trap_R_SetColor(color);
				break;
			case OSP_TEXT_CMD_FADE:
				fade = curr->value.fade;
				color[3] = fade;
				if (setColor && color[3] > setColor[3]) color[3] = setColor[3];
				trap_R_SetColor(color);
				break;
			case OSP_TEXT_CMD_SHADOW_COLOR:
			case OSP_TEXT_CMD_STOP:
				break;
		}
	}

	CG_CompiledTextDestroy(text_commands);
	trap_R_SetColor(NULL);
}

/*
 * ── CG_ModernDrawStringNew ─────────────────────────────────
 * Extended version with custom shadow color, border, and
 * DS_MAX_WIDTH_IS_CHARS support.
 * Uses single-pass shadow+text rendering for efficiency.
 */
void CG_ModernDrawStringNew(float x, float y, const char *string, const vec4_t setColor,
                          vec4_t shadowColor,
                          float charWidth, float charHeight, int maxWidth, int flags,
                          vec4_t background, vec4_t border, vec4_t borderColor)
{
	const font_metric_t *fm;
	const float *tc;
	float ax, ay, aw, aw1, ah;
	float x_end, xx;
	float fade;
	vec4_t color;
	float xx_add, yy_add;
	int i, hasBorder, proportional;
	qhandle_t sh;
	text_command_t *text_commands;
	text_command_t *curr;
	float expectedLength;
	int shadowEnabled;
	float shadowFade;
	vec4_t shadowCol;

	if (!string) return;

	text_commands = CG_CompileText(string);
	if (!text_commands) return;

	CG_AdjustFrom640(&x, &y, &charWidth, &charHeight);

	ax = x;
	ay = y;
	aw = charWidth;
	ah = charHeight;

	proportional = (flags & DS_PROPORTIONAL) ? 1 : 0;
	hasBorder = (border != NULL) ? 1 : 0;
	expectedLength = 0.0f;
	xx_add = 0.0f;
	yy_add = 0.0f;

	if (flags & DS_MAX_WIDTH_IS_CHARS)
		RestrictCompiledStringChars(text_commands, maxWidth);
	else
	{
		float mw = (float)maxWidth;
		CG_AdjustFrom640(NULL, NULL, &mw, NULL);
		RestrictCompiledString(text_commands, aw, proportional, mw);
	}

	if (hasBorder || background || (flags & (DS_HCENTER | DS_HRIGHT)))
		expectedLength = DrawCompiledStringLength(text_commands, aw, proportional);

	if (flags & (DS_HCENTER | DS_HRIGHT))
	{
		if (flags & DS_HCENTER)
			ax -= 0.5f * expectedLength;
		else
			ax -= expectedLength;
	}

	if (flags & DS_VCENTER)
		ay -= ah / 2;
	else if (flags & DS_VTOP)
		ay -= ah;

	sh = font->shader[0];
	for (i = 1; i < font->shaderCount; i++)
	{
		if (ah >= font->shaderThreshold[i])
			sh = font->shader[i];
	}

	if (background)
	{
		trap_R_SetColor(background);
		trap_R_DrawStretchPic(ax, ay, expectedLength, ah, 0, 0, 0, 0, cgs.media.whiteShader);
		trap_R_SetColor(colorWhite);
	}

	if (hasBorder)
		CG_ModernDrawFrame(ax, ay, expectedLength, ah, border, borderColor, 0);

	shadowEnabled = (flags & DS_SHADOW) && (shadowColor && shadowColor[3] != 0.0f);
	if (shadowEnabled)
	{
		xx_add = charWidth / 10.0f;
		yy_add = xx_add;
		VectorCopy(shadowColor, shadowCol);
		shadowFade = shadowColor[3];
		shadowCol[3] = shadowFade;
	}
	else
	{
		shadowFade = 1.0f;
		Vector4Copy(colorBlack, shadowCol);
	}

	fade = 1.0f;
	Vector4Copy(setColor, color);
	xx = ax;

	for (i = 0; i < OSP_TEXT_CMD_MAX && text_commands[i].type != OSP_TEXT_CMD_STOP; i++)
	{
		curr = &text_commands[i];

		switch (curr->type)
		{
			case OSP_TEXT_CMD_CHAR:
			{
				int index = curr->value.character;
				if (index < 0) index += 256;
				fm = &metrics[(unsigned char)index];

				if (proportional)
				{
					tc = fm->tc_prop;
					aw1 = fm->width * aw;
				}
				else
				{
					tc = fm->tc_mono;
					aw1 = aw;
				}

				/* shadow */
				if (shadowEnabled)
				{
					trap_R_SetColor(shadowCol);
					trap_R_DrawStretchPic(ax + xx_add, ay + yy_add, aw1, ah, tc[0], tc[1], tc[2], tc[3], sh);
				}

				/* character */
				trap_R_SetColor(color);
				trap_R_DrawStretchPic(ax, ay, aw1, ah, tc[0], tc[1], tc[2], tc[3], sh);

				/* advance position */
				if (proportional)
				{
					ax += fm->space1 * aw;
					x_end = ax + fm->space2 * aw;
				}
				else
				{
					x_end = ax + aw;
				}
				ax = x_end;
			}
			break;

			case OSP_TEXT_CMD_TEXT_COLOR:
				VectorCopy(curr->value.color, color);
				color[3] = fade;
				if (setColor && color[3] > setColor[3]) color[3] = setColor[3];
				shadowCol[3] = color[3];
				break;

			case OSP_TEXT_CMD_SHADOW_COLOR:
				VectorCopy(curr->value.color, shadowCol);
				shadowCol[3] = shadowFade;
				if (setColor && shadowCol[3] > setColor[3]) shadowCol[3] = setColor[3];
				break;

			case OSP_TEXT_CMD_FADE:
				fade = curr->value.fade;
				color[3] = fade;
				shadowCol[3] = fade;
				if (setColor)
				{
					if (color[3] > setColor[3]) color[3] = setColor[3];
					if (shadowCol[3] > setColor[3]) shadowCol[3] = setColor[3];
				}
				break;

			case OSP_TEXT_CMD_STOP:
				break;
		}
	}

	CG_CompiledTextDestroy(text_commands);
	trap_R_SetColor(NULL);
}
