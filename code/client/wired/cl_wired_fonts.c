/*
===========================================================================
cl_wired_fonts.c -- Wired UI font system (migrated from cg_moderntext.c)

Provides: font loading, text compilation, WiredFont_DrawString.
Runs in the CLIENT -- uses re.* directly instead of trap_R_*.
===========================================================================
*/
#include "../client.h"
#include "cl_wired_fonts.h"
#include "cl_wired_msdf.h"
#include "cl_wired_ui.h"
#include "cl_wired_draw.h"

#if FEAT_WIRED_UI

/* ── MSDF font integration ─────────────────────────────────────────── */

static msdfFont_t *msdf_sansman = NULL;
static msdfFont_t *msdf_sansman_italic = NULL;
static msdfFont_t *msdf_oxanium = NULL;
static msdfFont_t *msdf_oxanium_medium = NULL;
static msdfFont_t *msdf_console = NULL;

/* ── Font family registration table ───────────────────────────────── */

static fontFamily_t s_fontFamilies[] = {
	// ── Enter Sansman ──────────────────────────────────────────────
	// Each weight is a separate atlas from a FontForge-transformed TTF.
	// Source: entsans.ttf (Bold), entsani.ttf (Bold Italic)
	// Regular/Medium synthesized via changeWeight() at build time.
	{
		"sansman", {
			{ "sansman-regular",     "sansman-regular",     FONT_WEIGHT_REGULAR, FONT_STYLE_NORMAL,  NULL },
			{ "sansman-medium",      "sansman-medium",      FONT_WEIGHT_MEDIUM,  FONT_STYLE_NORMAL,  NULL },
			{ "sansman-bold",        "sansman-bold",         FONT_WEIGHT_BOLD,    FONT_STYLE_NORMAL,  NULL },
			{ "sansman-italic",      "sansman-italic",      FONT_WEIGHT_REGULAR, FONT_STYLE_ITALIC,  NULL },
			{ "sansman-bold-italic", "sansman-bold-italic", FONT_WEIGHT_BOLD,    FONT_STYLE_ITALIC,  NULL },
		}, 5
	},
	// ── Oxanium ────────────────────────────────────────────────────
	// Native Regular + Medium TTFs. No transform needed.
	{
		"oxanium", {
			{ "oxanium-regular",  "oxanium",        FONT_WEIGHT_REGULAR,  FONT_STYLE_NORMAL, NULL },
			{ "oxanium-medium",   "oxanium-medium", FONT_WEIGHT_MEDIUM,   FONT_STYLE_NORMAL, NULL },
		}, 2
	},
	// ── Share Tech Mono ────────────────────────────────────────────
	// Single weight, no transform.
	{
		"sharetechmono", {
			{ "sharetechmono-regular", "sharetechmono", FONT_WEIGHT_REGULAR, FONT_STYLE_NORMAL, NULL },
		}, 1
	},
};

#define FONT_FAMILY_COUNT (sizeof(s_fontFamilies) / sizeof(s_fontFamilies[0]))

void WiredFonts_InitMSDF( void ) {
	int i, j;

	/* ── Family-based loading: iterate families and faces ─────────── */
	for ( i = 0; i < (int)FONT_FAMILY_COUNT; i++ ) {
		fontFamily_t *fam = &s_fontFamilies[i];
		for ( j = 0; j < fam->faceCount; j++ ) {
			fontFace_t *face = &fam->faces[j];
			msdfFont_t *atlas = NULL;
			int k, m;

			/* Check if this atlas is already loaded by a previous face */
			for ( k = 0; k < i + 1; k++ ) {
				fontFamily_t *prev = &s_fontFamilies[k];
				int limit = ( k == i ) ? j : prev->faceCount;
				for ( m = 0; m < limit; m++ ) {
					if ( !Q_stricmp( prev->faces[m].atlasName, face->atlasName ) &&
					     prev->faces[m].atlas != NULL ) {
						atlas = prev->faces[m].atlas;
						break;
					}
				}
				if ( atlas ) break;
			}

			if ( !atlas ) {
				atlas = MSDF_LoadFont( face->atlasName );
			}

			face->atlas = atlas;
		}
	}

	/* ── Legacy static pointers (for WiredFonts_GetMSDF compat) ──── */
	msdf_sansman        = s_fontFamilies[0].faces[2].atlas;  /* sansman-bold */
	msdf_sansman_italic = s_fontFamilies[0].faces[3].atlas;  /* sansman-italic */
	msdf_oxanium        = s_fontFamilies[1].faces[0].atlas;  /* oxanium-regular */
	msdf_oxanium_medium = s_fontFamilies[1].faces[1].atlas;  /* oxanium-medium */
	msdf_console        = s_fontFamilies[2].faces[0].atlas;  /* sharetechmono */
}

msdfFont_t *WiredFonts_GetMSDF( const char *fontName ) {
	/* Primary MSDF fonts */
	if ( !Q_stricmp( fontName, "sansman" ) )         return msdf_sansman;
	if ( !Q_stricmp( fontName, "sansman-italic" ) )   return msdf_sansman_italic;
	if ( !Q_stricmp( fontName, "oxanium" ) )          return msdf_oxanium;
	if ( !Q_stricmp( fontName, "oxanium-medium" ) )   return msdf_oxanium_medium;
	if ( !Q_stricmp( fontName, "console" ) )          return msdf_console;
	if ( !Q_stricmp( fontName, "sharetechmono" ) )    return msdf_console;

	/* Legacy bitmap names -> MSDF equivalents */
	if ( !Q_stricmp( fontName, "id" ) )               return msdf_console;
	if ( !Q_stricmp( fontName, "idblock" ) )          return msdf_console;
	if ( !Q_stricmp( fontName, "cpma" ) )             return msdf_oxanium_medium;
	if ( !Q_stricmp( fontName, "m1rage" ) )           return msdf_oxanium;
	if ( !Q_stricmp( fontName, "elite" ) )            return msdf_oxanium;
	if ( !Q_stricmp( fontName, "elitebigchars" ) )    return msdf_sansman;
	if ( !Q_stricmp( fontName, "qlnumbers" ) )        return msdf_sansman;
	if ( !Q_stricmp( fontName, "eternal" ) )          return msdf_oxanium_medium;
	if ( !Q_stricmp( fontName, "diablo" ) )           return msdf_oxanium_medium;
	if ( !Q_stricmp( fontName, "elite_emoji" ) )      return msdf_oxanium;

	/* Unknown font -- use oxanium as safe default */
	Com_Printf( S_COLOR_YELLOW "WiredFonts_GetMSDF: unknown font '%s', using oxanium\n", fontName );
	return msdf_oxanium;
}

/*
 * WiredFont_Resolve — CSS-like font face resolution.
 *
 * 1. Find the family by name (case-insensitive).
 * 2. Filter faces by style (normal / italic).
 * 3. Exact weight match first.
 * 4. If no exact match: weight >= 500 prefers nearest heavier,
 *    weight < 500 prefers nearest lighter. Falls back to the
 *    nearest in the opposite direction.
 * 5. Returns NULL if family not found.
 */
const fontFace_t *WiredFont_Resolve(
	const char   *familyName,
	fontWeight_t  weight,
	fontStyle_t   style
) {
	int i, j;
	const fontFamily_t *family = NULL;
	const fontFace_t   *best = NULL;
	int                 bestDist = 99999;

	/* Find the family */
	for ( i = 0; i < (int)FONT_FAMILY_COUNT; i++ ) {
		if ( !Q_stricmp( familyName, s_fontFamilies[i].familyName ) ) {
			family = &s_fontFamilies[i];
			break;
		}
	}

	if ( !family ) {
		return NULL;
	}

	/* Pass 1: exact weight match among faces with matching style */
	for ( j = 0; j < family->faceCount; j++ ) {
		const fontFace_t *f = &family->faces[j];
		if ( f->style != style ) {
			continue;
		}
		if ( f->weight == weight ) {
			return f;
		}
	}

	/* Pass 2: nearest weight with CSS-like preference direction */
	for ( j = 0; j < family->faceCount; j++ ) {
		const fontFace_t *f = &family->faces[j];
		int diff, dist;

		if ( f->style != style ) {
			continue;
		}

		diff = (int)f->weight - (int)weight;
		dist = diff < 0 ? -diff : diff;

		if ( !best ) {
			best = f;
			bestDist = dist;
			continue;
		}

		if ( (int)weight >= 500 ) {
			/* Prefer heavier (positive diff), then nearest */
			if ( diff >= 0 && ( (int)best->weight - (int)weight ) < 0 ) {
				/* f is heavier, best is lighter — take f */
				best = f;
				bestDist = dist;
			} else if ( diff >= 0 && ( (int)best->weight - (int)weight ) >= 0 ) {
				/* Both heavier — pick nearest */
				if ( dist < bestDist ) {
					best = f;
					bestDist = dist;
				}
			} else if ( diff < 0 && ( (int)best->weight - (int)weight ) < 0 ) {
				/* Both lighter — pick nearest */
				if ( dist < bestDist ) {
					best = f;
					bestDist = dist;
				}
			}
			/* diff < 0 and best is heavier: keep best */
		} else {
			/* Prefer lighter (negative diff), then nearest */
			if ( diff <= 0 && ( (int)best->weight - (int)weight ) > 0 ) {
				/* f is lighter, best is heavier — take f */
				best = f;
				bestDist = dist;
			} else if ( diff <= 0 && ( (int)best->weight - (int)weight ) <= 0 ) {
				/* Both lighter — pick nearest */
				if ( dist < bestDist ) {
					best = f;
					bestDist = dist;
				}
			} else if ( diff > 0 && ( (int)best->weight - (int)weight ) > 0 ) {
				/* Both heavier — pick nearest */
				if ( dist < bestDist ) {
					best = f;
					bestDist = dist;
				}
			}
			/* diff > 0 and best is lighter: keep best */
		}
	}

	return best;
}

// ── type definitions from superhud (needed by font system) ───────────
#define OSP_TEXT_CMD_MAX 2048

typedef enum {
	OSP_TEXT_CMD_CHAR = 0,
	OSP_TEXT_CMD_STOP,
	OSP_TEXT_CMD_FADE,
	OSP_TEXT_CMD_TEXT_COLOR,
	OSP_TEXT_CMD_SHADOW_COLOR,
} text_command_type_t;

typedef struct {
	text_command_type_t type;
	union {
		char character;
		float fade;
		vec4_t color;
	} value;
} text_command_t;

// draw style flags
#define DS_HLEFT        0x0000
#define DS_HCENTER      0x0001
#define DS_HRIGHT       0x0002
#define DS_VTOP         0x0000
#define DS_VCENTER      0x0004
#define DS_VBOTTOM      0x0008
#define DS_PROPORTIONAL 0x0010
#define DS_SHADOW       0x0020
#define DS_EMOJI        0x0040
#define DS_FORCE_COLOR  0x0080
#define DS_MAX_WIDTH_IS_CHARS 0x0100

// border frame drawing
void WiredFont_DrawFrame( float x, float y, float w, float h, const float *border, const float *borderColor, qboolean filled ) {
	if ( !border || !borderColor ) return;
	if ( border[0] > 0 ) WUI_FillRect( x, y, w, border[0], borderColor );
	if ( border[2] > 0 ) WUI_FillRect( x, y + h - border[2], w, border[2], borderColor );
	if ( border[3] > 0 ) WUI_FillRect( x, y + border[0], border[3], h - border[0] - border[2], borderColor );
	if ( border[1] > 0 ) WUI_FillRect( x + w - border[1], y + border[0], border[1], h - border[0] - border[2], borderColor );
}

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

/* Old bitmap text compiler removed -- dead code after MSDF migration. */


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
/* font/metrics globals removed -- were used by old bitmap text system (dead) */



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

/*
 * Map a bitmap font index (0-10) to a FONT_* id for Text_Draw().
 * The mapping is based on which MSDF font each bitmap slot maps to.
 */
int WiredFont_ToFontId( int fontIndex )
{
	if ( fontIndex < 0 || fontIndex >= fonts_num )
		return FONT_UI;

	{
		const char *name = fonts[fontIndex].name;
		msdfFont_t *msdf = WiredFonts_GetMSDF( name );

		if ( msdf == msdf_sansman )         return FONT_DISPLAY;
		if ( msdf == msdf_sansman_italic )   return FONT_DISPLAY_ITALIC;
		if ( msdf == msdf_oxanium )          return FONT_UI;
		if ( msdf == msdf_oxanium_medium )   return FONT_UI_MEDIUM;
		if ( msdf == msdf_console )          return FONT_MONO;
	}

	return FONT_UI;
}

/*
 * Convert DS_* flags to TEXT_ALIGN_* alignment value.
 */
int WiredFont_ToAlignment( int dsFlags )
{
	if ( dsFlags & DS_HCENTER ) return TEXT_ALIGN_CENTER;
	if ( dsFlags & DS_HRIGHT )  return TEXT_ALIGN_RIGHT;
	return TEXT_ALIGN_LEFT;
}

/*
 * Convert DS_* flags to Text_Draw flags (TEXT_DROPSHADOW, etc.).
 */
int WiredFont_ToTextFlags( int dsFlags )
{
	int f = 0;
	if ( dsFlags & DS_SHADOW ) f |= TEXT_DROPSHADOW;
	return f;
}

static qboolean CG_FileExist(const char *file)
{
	fileHandle_t f;
	if (!file || !file[0]) return qfalse;
	FS_FOpenFileRead( file, &f, qfalse );
	if (f == FS_INVALID_HANDLE) return qfalse;
	FS_FCloseFile(f);
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

	len = FS_FOpenFileRead( fontName, &f, qfalse );
	if (f == FS_INVALID_HANDLE)
	{
		Com_Printf(S_COLOR_YELLOW "CG_LoadFont: error opening %s\n", fontName);
		return;
	}

	if (len >= (int)sizeof(buf))
	{
		Com_Printf(S_COLOR_YELLOW "CG_LoadFont: font file too long: %i\n", len);
		len = sizeof(buf) - 1;
	}

	FS_Read(buf, len, f);
	FS_FCloseFile(f);
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
			Com_Printf(S_COLOR_RED "CG_LoadFont: bad char index %i.\n", i);
			return;
		}
		fm = fnt->metrics + i;

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { Com_Printf(S_COLOR_RED "CG_LoadFont: error reading x0.\n"); return; }
		x0 = atof(token);

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { Com_Printf(S_COLOR_RED "CG_LoadFont: error reading y0.\n"); return; }
		y0 = atof(token);

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { Com_Printf(S_COLOR_RED "CG_LoadFont: error reading x-offset.\n"); return; }
		w1 = atof(token);

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { Com_Printf(S_COLOR_RED "CG_LoadFont: error reading x-length.\n"); return; }
		w2 = atof(token);

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { Com_Printf(S_COLOR_RED "CG_LoadFont: error reading space1.\n"); return; }
		s1 = atof(token);

		token = COM_ParseExt(&text, qfalse);
		if (!token[0]) { Com_Printf(S_COLOR_RED "CG_LoadFont: error reading space2.\n"); return; }
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
		fnt->shader[i] = re.RegisterShaderNoMip(shaderName[i]);
		fnt->shaderThreshold[i] = shaderThreshold[i];
	}

	Com_Printf("Font '%s' loaded with %i chars and %i images\n", fontName, chars, shaderCount);
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


// ══════════════════════════════════════════════════════════════════════
// TA Font System — fontInfo_t-based rendering for v6/TA menu compat
// ══════════════════════════════════════════════════════════════════════

static fontInfo_t taFonts[TA_FONT_COUNT];
static qboolean   taFontsLoaded = qfalse;

void WiredUI_LoadTAFonts( void ) {
	wiredAssetGlobals_t *ag = WiredUI_GetAssetGlobals();

	re.RegisterFont( ag->font,      ag->fontSize,      &taFonts[TA_FONT_NORMAL] );
	re.RegisterFont( ag->smallFont,  ag->smallFontSize,  &taFonts[TA_FONT_SMALL] );
	re.RegisterFont( ag->bigFont,    ag->bigFontSize,    &taFonts[TA_FONT_BIG] );

	taFontsLoaded = qtrue;
	Com_DPrintf( "WiredUI: TA fonts loaded (normal=%s/%d, small=%s/%d, big=%s/%d)\n",
		ag->font, ag->fontSize, ag->smallFont, ag->smallFontSize, ag->bigFont, ag->bigFontSize );
}

fontInfo_t *WiredUI_GetTAFont( taFontSize_t size ) {
	if ( !taFontsLoaded ) {
		WiredUI_LoadTAFonts();
	}
	if ( size < 0 || size >= TA_FONT_COUNT ) size = TA_FONT_NORMAL;
	return &taFonts[size];
}

/*
============
WiredUI_DrawText_TA

Draws text using TA fontInfo_t glyphs.
Scale: 1.0 = font's native point size. 0.5 = half size, etc.
Style: 0 = normal, 3 = shadowed (ITEM_TEXTSTYLE_SHADOWED), 6 = more shadow
Limit: max chars to draw (0 = unlimited)
============
*/
void WiredUI_DrawText_TA( float x, float y, float scale, const vec4_t color,
                          const char *text, int limit, int style, fontInfo_t *font ) {
	int len, count;
	float ax, ay;
	glyphInfo_t *glyph;
	float useScale;

	if ( !text || !text[0] ) return;
	if ( !font ) font = WiredUI_GetTAFont( TA_FONT_NORMAL );

	useScale = scale * font->glyphScale;
	len = Q_PrintStrlen( text );
	if ( limit > 0 && len > limit ) len = limit;

	ax = x;
	ay = y;
	re.SetColor( color );

	count = 0;
	while ( *text && ( limit <= 0 || count < limit ) ) {
		if ( Q_IsColorString( text ) ) {
			// handle color codes
			vec4_t newColor;
			Com_Memcpy( newColor, g_color_table[ColorIndex(*(text+1))], sizeof( newColor ) );
			newColor[3] = color[3];
			re.SetColor( newColor );
			text += 2;
			continue;
		}

		glyph = &font->glyphs[(unsigned char)*text];

		// shadow pass
		if ( style == 3 || style == 6 ) {
			float shadowOffset = ( style == 6 ) ? 2.0f : 1.0f;
			vec4_t shadowColor = { 0, 0, 0, color[3] };
			re.SetColor( shadowColor );
			WUI_DrawPic( ax + shadowOffset, ay + shadowOffset,
				glyph->imageWidth * useScale,
				glyph->imageHeight * useScale,
				glyph->glyph );
			re.SetColor( color );
		}

		WUI_DrawPic( ax, ay,
			glyph->imageWidth * useScale,
			glyph->imageHeight * useScale,
			glyph->glyph );

		ax += ( glyph->xSkip * useScale );
		text++;
		count++;
	}

	re.SetColor( NULL );
}

/*
============
WiredUI_TextWidth_TA

Measures text width in pixels using TA fontInfo_t glyphs.
============
*/
float WiredUI_TextWidth_TA( const char *text, float scale, int limit, fontInfo_t *font ) {
	float width = 0;
	float useScale;
	int count = 0;

	if ( !text || !text[0] ) return 0;
	if ( !font ) font = WiredUI_GetTAFont( TA_FONT_NORMAL );

	useScale = scale * font->glyphScale;

	while ( *text && ( limit <= 0 || count < limit ) ) {
		if ( Q_IsColorString( text ) ) {
			text += 2;
			continue;
		}
		width += font->glyphs[(unsigned char)*text].xSkip * useScale;
		text++;
		count++;
	}

	return width;
}

/*
============
WiredUI_TextHeight_TA

Returns the height of the tallest glyph in the text.
============
*/
float WiredUI_TextHeight_TA( const char *text, float scale, int limit, fontInfo_t *font ) {
	float maxHeight = 0;
	float useScale;
	int count = 0;

	if ( !text || !text[0] ) return 0;
	if ( !font ) font = WiredUI_GetTAFont( TA_FONT_NORMAL );

	useScale = scale * font->glyphScale;

	while ( *text && ( limit <= 0 || count < limit ) ) {
		if ( Q_IsColorString( text ) ) {
			text += 2;
			continue;
		}
		float h = font->glyphs[(unsigned char)*text].imageHeight * useScale;
		if ( h > maxHeight ) maxHeight = h;
		text++;
		count++;
	}

	return maxHeight;
}

#endif // FEAT_WIRED_UI
