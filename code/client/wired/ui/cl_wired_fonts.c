/*
===========================================================================
cl_wired_fonts.c -- Wired UI font system (migrated from cg_moderntext.c)

Provides: font loading, text compilation, WiredFont_DrawString.
Runs in the CLIENT -- uses re.* directly instead of trap_R_*.
===========================================================================
*/
#include "../../client.h"
#include "cl_wired_fonts.h"
#include "cl_wired_msdf.h"
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

int WiredFont_IdFromName( const char *name )
{
	if ( !name || !name[0] ) {
		return FONT_DISPLAY;
	}

	/* Native MSDF family/face names */
	if ( !Q_stricmp( name, "sansman" ) ) return FONT_DISPLAY;
	if ( !Q_stricmp( name, "sansman-regular" ) ) return FONT_DISPLAY;
	if ( !Q_stricmp( name, "sansman-medium" ) ) return FONT_DISPLAY;
	if ( !Q_stricmp( name, "sansman-bold" ) ) return FONT_DISPLAY_BOLD;
	if ( !Q_stricmp( name, "sansman-italic" ) ) return FONT_DISPLAY_ITALIC;
	if ( !Q_stricmp( name, "sansman-bold-italic" ) ) return FONT_DISPLAY_ITALIC;

	if ( !Q_stricmp( name, "oxanium" ) ) return FONT_UI;
	if ( !Q_stricmp( name, "oxanium-regular" ) ) return FONT_UI;
	if ( !Q_stricmp( name, "oxanium-medium" ) ) return FONT_UI_MEDIUM;

	if ( !Q_stricmp( name, "sharetechmono" ) ) return FONT_MONO;
	if ( !Q_stricmp( name, "sharetechmono-regular" ) ) return FONT_MONO;
	if ( !Q_stricmp( name, "console" ) ) return FONT_MONO;

	/* Legacy bitmap names preserved for SuperHUD compatibility */
	if ( !Q_stricmp( name, "id" ) ) return FONT_MONO;
	if ( !Q_stricmp( name, "idblock" ) ) return FONT_MONO;
	if ( !Q_stricmp( name, "cpma" ) ) return FONT_UI_MEDIUM;
	if ( !Q_stricmp( name, "m1rage" ) ) return FONT_UI;
	if ( !Q_stricmp( name, "elite_emoji" ) ) return FONT_UI;
	if ( !Q_stricmp( name, "diablo" ) ) return FONT_UI_MEDIUM;
	if ( !Q_stricmp( name, "eternal" ) ) return FONT_UI_MEDIUM;
	if ( !Q_stricmp( name, "qlnumbers" ) ) return FONT_DISPLAY;
	if ( !Q_stricmp( name, "elite" ) ) return FONT_UI;
	if ( !Q_stricmp( name, "elitebigchars" ) ) return FONT_DISPLAY;

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


#endif // FEAT_WIRED_UI
