#include "g_local.h"

qboolean G_ValidateLightstylePattern( const char *pattern ) {
	int i;
	if ( !pattern ) return qfalse;
	for ( i = 0; pattern[i]; i++ ) {
		if ( i >= LIGHTSTYLE_PATTERN_MAX ) return qfalse;
		if ( pattern[i] < 'a' || pattern[i] > 'z' ) return qfalse;
	}
	return qtrue;
}

qboolean G_SetLightstyle( int style, const char *pattern ) {
	if ( style < 0 || style >= CS_MAX_LIGHTSTYLES ) return qfalse;
	if ( !G_ValidateLightstylePattern( pattern ) ) return qfalse;
	trap_SetConfigstring( CS_LIGHTSTYLES + style, pattern );
	return qtrue;
}
