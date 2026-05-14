// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#if defined(_DEBUG) && defined(_WIN32)
#include <windows.h>
#endif

#include "q_shared.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_system, "system" );

int Com_Split( char *in, char **out, int outsz, int delim )
{
	int c;
	char **o = out, **end = out + outsz;
	if ( delim >= ' ' ) {
		while( (c = (byte)*in) != '\0' && c <= ' ' )
			in++;
	}
	*out = in; out++;
	while( out < end ) {
		while( (c = (byte)*in) != '\0' && c != delim )
			in++;
		*in = '\0';
		if ( !c ) {
			if ( out[-1][0] == '\0' )
				out--;
			break;
		}
		in++;
		if ( delim >= ' ' ) {
			while( (c = (byte)*in) != '\0' && c <= ' ' )
				in++;
		}
		*out = in; out++;
	}
	while( (c = (byte)*in) != '\0' && c != delim )
		in++;
	*in = '\0';
	c = out - o;
	while( out < end ) {
		*out = in; out++;
	}
	return c;
}


static int Hex( char c )
{
	if ( c >= '0' && c <= '9' ) {
		return c - '0';
	}
	if ( c >= 'A' && c <= 'F' ) {
		return 10 + c - 'A';
	}
	if ( c >= 'a' && c <= 'f' ) {
		return 10 + c - 'a';
	}
	return -1;
}


int Com_HexStrToInt( const char *str )
{
	if ( !str )
		return -1;

	if ( str[ 0 ] == '0' && str[ 1 ] == 'x' && str[ 2 ] != '\0' )
	{
		int i, digit, n = 0, len = strlen( str );

		for( i = 2; i < len; i++ )
		{
			n *= 16;

			digit = Hex( str[ i ] );

			if ( digit < 0 )
				return -1;

			n += digit;
		}

		return n;
	}

	return -1;
}


qboolean Com_GetHashColor( const char *str, byte *color )
{
	int hex[6];

	color[0] = color[1] = color[2] = 0;

	if ( *str++ != '#' ) {
		return qfalse;
	}

	int len = (int)strlen( str );
	if ( len <= 0 || len > 6 ) {
		return qfalse;
	}

	for ( int i = 0; i < len; i++ ) {
		hex[i] = Hex( str[i] );
		if ( hex[i] < 0 ) {
			return qfalse;
		}
	}

	switch ( len ) {
		case 3:
			color[0] = hex[0] << 4 | hex[0];
			color[1] = hex[1] << 4 | hex[1];
			color[2] = hex[2] << 4 | hex[2];
			break;
		case 6:
			color[0] = hex[0] << 4 | hex[1];
			color[1] = hex[2] << 4 | hex[3];
			color[2] = hex[4] << 4 | hex[5];
			break;
		default:
			return qfalse;
	}

	return qtrue;
}


int QDECL Com_sprintf( char *dest, int size, const char *fmt, ...)
{
	if ( !dest )
	{
		Com_Terminate( TERM_UNRECOVERABLE, "Com_sprintf: NULL dest" );
#if defined(_DEBUG) && defined(_WIN32)
		DebugBreak();
#endif
		return 0;
	}

	va_list argptr;
	va_start( argptr, fmt );
	int len = vsnprintf( dest, size, fmt, argptr );
	va_end( argptr );

	if ( len < 0 )
	{
		dest[0] = '\0';
		Com_Terminate( TERM_UNRECOVERABLE, "Com_sprintf: encoding error" );
#if defined(_DEBUG) && defined(_WIN32)
		DebugBreak();
#endif
		return 0;
	}

	if ( len >= size )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_system), S_COLOR_YELLOW "Com_sprintf: overflow of %i in %i\n", len, size );
#if defined(_DEBUG) && defined(_WIN32)
		DebugBreak();
#endif
		len = size - 1;
	}

	return len;
}


void Com_TruncateLongString( char *buffer, const char *s )
{
	int length = strlen( s );

	if( length <= TRUNCATE_LENGTH ) {
		Q_strncpyz( buffer, s, TRUNCATE_LENGTH );
		return;
	}

	Q_strncpyz( buffer, s, ( TRUNCATE_LENGTH / 2 ) - 3 );

	qstring_t buf_qs = QS_WrapExisting( buffer, TRUNCATE_LENGTH );
	QS_Append( &buf_qs, " ... " );
	QS_Append( &buf_qs, s + length - ( TRUNCATE_LENGTH / 2 ) + 3 );
}


static const char *Com_StringContains( const char *str1, const char *str2, int len2 ) {
	int len, i, j;

	len = strlen(str1) - len2;
	for (i = 0; i <= len; i++, str1++) {
		for (j = 0; str2[j]; j++) {
			// NOLINTNEXTLINE(clang-analyzer-core.uninitialized.ArraySubscript) — locase[] is initialized at engine startup; analyzer can't see Com_InitLocaseTable
			if (locase[(byte)str1[j]] != locase[(byte)str2[j]]) {
				break;
			}
		}
		if (!str2[j]) {
			return str1;
		}
	}
	return NULL;
}


int Com_Filter( const char *filter, const char *name )
{
	char buf[ MAX_TOKEN_CHARS ];
	const char *ptr;
	int i, found;

	// NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch) — `filter` is a caller-supplied non-NULL string per the API contract
	while(*filter) {
		if (*filter == '*') {
			filter++;
			for (i = 0; *filter; i++) {
				if (*filter == '*' || *filter == '?')
					break;
				buf[i] = *filter;
				filter++;
			}
			buf[i] = '\0';
			if ( i ) {
				ptr = Com_StringContains( name, buf, i );
				if ( !ptr )
					return qfalse;
				name = ptr + i;
			}
		}
		else if (*filter == '?') {
			filter++;
			name++;
		}
		else if (*filter == '[' && *(filter+1) == '[') {
			filter++;
		}
		else if (*filter == '[') {
			filter++;
			found = qfalse;
			while(*filter && !found) {
				if (*filter == ']' && *(filter+1) != ']') break;
				if (*(filter+1) == '-' && *(filter+2) && (*(filter+2) != ']' || *(filter+3) == ']')) {
					if (locase[(byte)*name] >= locase[(byte)*filter] &&
						locase[(byte)*name] <= locase[(byte)*(filter+2)])
							found = qtrue;
					filter += 3;
				}
				else {
					if (locase[(byte)*filter] == locase[(byte)*name])
						found = qtrue;
					filter++;
				}
			}
			if (!found) return qfalse;
			while(*filter) {
				if (*filter == ']' && *(filter+1) != ']') break;
				filter++;
			}
			filter++;
			name++;
		}
		else {
			// NOLINTNEXTLINE(clang-analyzer-core.uninitialized.ArraySubscript) — locase[] is initialized at engine startup; analyzer can't see Com_InitLocaseTable
			if (locase[(byte)*filter] != locase[(byte)*name])
				return qfalse;
			filter++;
			name++;
		}
	}
	return qtrue;
}


qboolean Com_FilterExt( const char *filter, const char *name )
{
	char buf[ MAX_TOKEN_CHARS ];
	const char *ptr;
	int i;

	while ( *filter ) {
		if ( *filter == '*' ) {
			filter++;
			for ( i = 0; *filter != '\0' && i < (int)sizeof(buf)-1; i++ ) {
				if ( *filter == '*' || *filter == '?' )
					break;
				buf[i] = *filter++;
			}
			buf[ i ] = '\0';
			if ( i ) {
				ptr = Com_StringContains( name, buf, i );
				if ( !ptr )
					return qfalse;
				name = ptr + i;
			} else if ( *filter == '\0' ) {
				return qtrue;
			}
		}
		else if ( *filter == '?' ) {
			if ( *name == '\0' )
				return qfalse;
			filter++;
			name++;
		}
		else {
			if ( locase[(byte)*filter] != locase[(byte)*name] )
				return qfalse;
			filter++;
			name++;
		}
	}
	if ( *name ) {
		return qfalse;
	}
	return qtrue;
}


qboolean Com_HasPatterns( const char *str )
{
	int c;

	while ( (c = (byte)*str++) != '\0' )
	{
		if ( c == '*' || c == '?' )
		{
			return qtrue;
		}
	}

	return qfalse;
}


int Com_FilterPath( const char *filter, const char *name )
{
	int i;
	char new_filter[MAX_QPATH];
	char new_name[MAX_QPATH];

	for (i = 0; i < MAX_QPATH-1 && filter[i]; i++) {
		if ( filter[i] == '\\' || filter[i] == ':' ) {
			new_filter[i] = '/';
		}
		else {
			new_filter[i] = filter[i];
		}
	}
	new_filter[i] = '\0';
	for (i = 0; i < MAX_QPATH-1 && name[i]; i++) {
		if ( name[i] == '\\' || name[i] == ':' ) {
			new_name[i] = '/';
		}
		else {
			new_name[i] = name[i];
		}
	}
	new_name[i] = '\0';
	return Com_Filter( new_filter, new_name );
}


const char *Com_SkipCharset( const char *s, const char *sep )
{
	const char *p = s;

	while( p )
	{
		if( strchr( sep, *p ) )
			p++;
		else
			break;
	}

	return p;
}


const char *Com_SkipTokens( const char *s, int numTokens, const char *sep )
{
	int sepCount = 0;
	const char *p = s;

	while( sepCount < numTokens )
	{
		if( strchr( sep, *p++ ) )
		{
			sepCount++;
			while( strchr( sep, *p ) )
				p++;
		}
		else if( *p == '\0' )
			break;
	}

	if( sepCount == numTokens )
		return p;
	return s;
}
