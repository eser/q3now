/*
===========================================================================
cg_colorparse.c -- Hex color parsing utility

Parses color strings in formats:
  "0xRRGGBB"  — hex RGB
  "red"       — named color
  "1"-"12"    — numeric color codes

===========================================================================
*/
#include "cg_local.h"

typedef struct {
	const char	*name;
	const char	*code;
	vec4_t		*color;
} colorEntry_t;

static colorEntry_t colorTable[] = {
	{ "red",		"1",	&colorRed },
	{ "green",		"2",	&colorGreen },
	{ "yellow",		"3",	&colorYellow },
	{ "blue",		"4",	&colorBlue },
	{ "cyan",		"5",	&colorCyan },
	{ "magenta",	"6",	&colorMagenta },
	{ "white",		"7",	&colorWhite },
	{ "orange",		"8",	&colorOrange },
	{ "black",		"9",	&colorBlack },
	{ "ltgrey",		"10",	&colorLtGrey },
	{ "mdgrey",		"11",	&colorMdGrey },
	{ "dkgrey",		"12",	&colorDkGrey },
	{ NULL,			NULL,	NULL }
};

static int hexDigit( char c ) {
	if ( c >= '0' && c <= '9' ) return c - '0';
	if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
	if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
	return -1;
}

static qboolean isHexColor( const char *s ) {
	int i;
	for ( i = 0; i < 6; i++ ) {
		if ( hexDigit( s[i] ) < 0 ) return qfalse;
	}
	return qtrue;
}

/*
==================
CG_ParseColor

Parses a color string into a vec4_t.
Supports: "0xRRGGBB", named colors ("red", "cyan"), numeric codes ("1"-"12").
Alpha is set from the alpha parameter, clamped to [0,1].
==================
*/
void CG_ParseColor( const char *str, float *col, float alpha ) {
	int i;

	col[0] = col[1] = col[2] = 1.0f;
	col[3] = alpha < 0.0f ? 0.0f : ( alpha > 1.0f ? 1.0f : alpha );

	if ( !str || !str[0] ) {
		return;
	}

	// hex: 0xRRGGBB
	if ( str[0] == '0' && ( str[1] == 'x' || str[1] == 'X' ) && isHexColor( str + 2 ) ) {
		const char *s = str + 2;
		col[0] = ( hexDigit( s[0] ) * 16 + hexDigit( s[1] ) ) / 255.0f;
		col[1] = ( hexDigit( s[2] ) * 16 + hexDigit( s[3] ) ) / 255.0f;
		col[2] = ( hexDigit( s[4] ) * 16 + hexDigit( s[5] ) ) / 255.0f;
		return;
	}

	// named or numeric lookup
	for ( i = 0; colorTable[i].name; i++ ) {
		if ( !Q_stricmp( str, colorTable[i].name ) || !Q_stricmp( str, colorTable[i].code ) ) {
			col[0] = (*colorTable[i].color)[0];
			col[1] = (*colorTable[i].color)[1];
			col[2] = (*colorTable[i].color)[2];
			return;
		}
	}
}
