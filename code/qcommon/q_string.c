// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "q_shared.h"

#ifndef NDEBUG
#define QS_WARN_TRUNC( needed, avail ) \
    fprintf( stderr, "qstring: truncated (needed %d, available %d)\n", (needed), (avail) )
#else
#define QS_WARN_TRUNC( needed, avail ) ((void)0)
#endif

qstring_t QS_Wrap( char *buffer, int capacity ) {
	qstring_t qs;
	qs.data     = buffer;
	qs.capacity = capacity;
	qs.len      = 0;
	if ( capacity > 0 ) {
		buffer[0] = '\0';
	}
	return qs;
}

qstring_t QS_WrapFrom( char *buffer, int capacity, const char *initial ) {
	qstring_t qs = QS_Wrap( buffer, capacity );
	QS_Set( &qs, initial );
	return qs;
}

qstring_t QS_WrapExisting( char *buffer, int capacity ) {
	qstring_t qs;
	qs.data     = buffer;
	qs.capacity = capacity;
	qs.len      = (int)strlen( buffer );
	if ( qs.len >= capacity ) {
		qs.len          = capacity - 1;
		buffer[qs.len]  = '\0';
	}
	return qs;
}

void QS_Clear( qstring_t *qs ) {
	qs->len     = 0;
	qs->data[0] = '\0';
}

void QS_Truncate( qstring_t *qs, int maxLen ) {
	if ( maxLen < qs->len ) {
		qs->len          = maxLen;
		qs->data[maxLen] = '\0';
	}
}

void QS_Set( qstring_t *qs, const char *src ) {
	int srcLen = (int)strlen( src );
	int space  = qs->capacity - 1;
	if ( srcLen > space ) {
		QS_WARN_TRUNC( srcLen, space );
		srcLen = space;
	}
	memcpy( qs->data, src, srcLen );
	qs->data[srcLen] = '\0';
	qs->len          = srcLen;
}

void QS_Setf( qstring_t *qs, const char *fmt, ... ) {
	va_list argptr;
	va_start( argptr, fmt );
	int written = vsnprintf( qs->data, qs->capacity, fmt, argptr );
	va_end( argptr );

	if ( written < 0 ) {
		qs->data[0] = '\0';
		qs->len     = 0;
	} else if ( written >= qs->capacity ) {
		QS_WARN_TRUNC( written, qs->capacity - 1 );
		qs->len = qs->capacity - 1;
	} else {
		qs->len = written;
	}
}

void QS_Append( qstring_t *qs, const char *src ) {
	int srcLen = (int)strlen( src );
	int space  = qs->capacity - qs->len - 1;

	if ( srcLen > space ) {
		QS_WARN_TRUNC( srcLen, space );
		srcLen = space;
	}

	if ( srcLen > 0 ) {
		memcpy( qs->data + qs->len, src, srcLen );
		qs->len         += srcLen;
		qs->data[qs->len] = '\0';
	}
}

void QS_Appendf( qstring_t *qs, const char *fmt, ... ) {
	if ( qs->len >= qs->capacity - 1 ) return;

	int remaining = qs->capacity - qs->len;
	va_list argptr;
	va_start( argptr, fmt );
	int written = vsnprintf( qs->data + qs->len, remaining, fmt, argptr );
	va_end( argptr );

	if ( written > 0 && written < remaining ) {
		qs->len += written;
	} else if ( written >= remaining ) {
		QS_WARN_TRUNC( written, remaining - 1 );
		qs->len = qs->capacity - 1;
	}
}

void QS_AppendChar( qstring_t *qs, char c ) {
	if ( qs->len >= qs->capacity - 1 ) return;
	qs->data[qs->len]     = c;
	qs->data[qs->len + 1] = '\0';
	qs->len++;
}

void QS_AppendN( qstring_t *qs, const char *src, int n ) {
	int space = qs->capacity - qs->len - 1;
	if ( n > space ) {
		QS_WARN_TRUNC( n, space );
		n = space;
	}
	if ( n > 0 ) {
		memcpy( qs->data + qs->len, src, n );
		qs->len          += n;
		qs->data[qs->len]  = '\0';
	}
}

qboolean QS_Equal( const qstring_t *a, const qstring_t *b ) {
	if ( a->len != b->len ) return qfalse;
	return (qboolean)( strcmp( a->data, b->data ) == 0 );
}

qboolean QS_EqualI( const qstring_t *a, const qstring_t *b ) {
	if ( a->len != b->len ) return qfalse;
	return (qboolean)( Q_stricmp( a->data, b->data ) == 0 );
}

qboolean QS_EqualStr( const qstring_t *qs, const char *str ) {
	return (qboolean)( strcmp( qs->data, str ) == 0 );
}

qboolean QS_EqualStrI( const qstring_t *qs, const char *str ) {
	return (qboolean)( Q_stricmp( qs->data, str ) == 0 );
}

void QS_CopyTo( const qstring_t *qs, char *dest, int destsize ) {
	Q_strncpyz( dest, qs->data, destsize );
}
