/*
cl_wired_fonts.c -- Wired UI font system (migrated from cg_moderntext.c)
*/
#include "../../client.h"
#include "cl_wired_fonts.h"
#include "cl_wired_msdf.h"
#include "cl_wired_draw.h"

#if FEAT_WIRED_UI

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

	/* Pass 2: score = wrong-side penalty (1000) + abs(diff); pick lowest */
	for ( j = 0; j < family->faceCount; j++ ) {
		const fontFace_t *f = &family->faces[j];
		int diff, score, pref;

		if ( f->style != style ) continue;

		diff  = (int)f->weight - (int)weight;
		pref  = ( (int)weight >= 500 ) ? 1 : -1;
		score = ( ( diff > 0 ? 1 : -1 ) != pref ? 1000 : 0 ) + ( diff < 0 ? -diff : diff );

		if ( !best || score < bestDist ) {
			best     = f;
			bestDist = score;
		}
	}

	return best;
}

const fontFace_t *WiredFont_ResolveByName( const char *name )
{
	int i, j;

	if ( !name || !name[0] ) {
		return NULL;
	}

	for ( i = 0; i < (int)FONT_FAMILY_COUNT; i++ ) {
		const fontFamily_t *family = &s_fontFamilies[i];
		if ( !Q_stricmp( name, family->familyName ) ) {
			return WiredFont_Resolve( family->familyName, FONT_WEIGHT_REGULAR, FONT_STYLE_NORMAL );
		}
		for ( j = 0; j < family->faceCount; j++ ) {
			const fontFace_t *face = &family->faces[j];
			if ( !Q_stricmp( name, face->name ) || !Q_stricmp( name, face->atlasName ) ) {
				return face;
			}
		}
	}

	return NULL;
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
	static const struct { const char *k; int id; } map[] = {
		{ "defaultSerifFont",       FONT_DISPLAY        },
		{ "defaultSerifFontItalic", FONT_DISPLAY_ITALIC },
		{ "defaultSerifFontBold",   FONT_DISPLAY_BOLD   },
		{ "defaultSansFont",        FONT_UI             },
		{ "defaultSansFontMedium",  FONT_UI_MEDIUM      },
		{ "defaultMonoFont",        FONT_MONO           },
		{ "display",                FONT_DISPLAY        },
		{ "displayItalic",          FONT_DISPLAY_ITALIC },
		{ "displayBold",            FONT_DISPLAY_BOLD   },
		{ "ui",                     FONT_UI             },
		{ "uiMedium",               FONT_UI_MEDIUM      },
		{ "mono",                   FONT_MONO           },
		{ "sansman",                FONT_DISPLAY        },
		{ "sansman-regular",        FONT_DISPLAY        },
		{ "sansman-medium",         FONT_DISPLAY        },
		{ "sansman-bold",           FONT_DISPLAY_BOLD   },
		{ "sansman-italic",         FONT_DISPLAY_ITALIC },
		{ "sansman-bold-italic",    FONT_DISPLAY_ITALIC },
		{ "oxanium",                FONT_UI             },
		{ "oxanium-regular",        FONT_UI             },
		{ "oxanium-medium",         FONT_UI_MEDIUM      },
		{ "sharetechmono",          FONT_MONO           },
		{ "sharetechmono-regular",  FONT_MONO           },
		{ NULL, 0 }
	};
	int i;

	if ( !name || !name[0] ) return FONT_DISPLAY;
	for ( i = 0; map[i].k; i++ ) {
		if ( !Q_stricmp( name, map[i].k ) ) return map[i].id;
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


#endif // FEAT_WIRED_UI
