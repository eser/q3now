// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "cvar_value.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

static unsigned char wired_cvar_value_lower_ascii( unsigned char ch ) {
	if ( ch >= 'A' && ch <= 'Z' ) {
		return (unsigned char)( ch + ( 'a' - 'A' ) );
	}
	return ch;
}

static int wired_cvar_value_equal_ascii( const char *left, const char *right ) {
	if ( !left || !right ) {
		return 0;
	}
	while ( *left && *right ) {
		if ( wired_cvar_value_lower_ascii( (unsigned char)*left )
		  != wired_cvar_value_lower_ascii( (unsigned char)*right ) ) {
			return 0;
		}
		left++;
		right++;
	}
	return *left == *right;
}

wiredCvarValueStatus_t wired_cvar_value_bool( const char *text, wiredCvarValue_t *out ) {
	wiredCvarValue_t parsed;

	if ( !text || !out ) {
		return WIRED_CVAR_VALUE_INVALID;
	}
	if ( wired_cvar_value_equal_ascii( text, "1" )
	  || wired_cvar_value_equal_ascii( text, "true" )
	  || wired_cvar_value_equal_ascii( text, "yes" )
	  || wired_cvar_value_equal_ascii( text, "on" ) ) {
		parsed.integer = 1;
		parsed.number = 1.0f;
		parsed.normalized = "1";
		*out = parsed;
		return WIRED_CVAR_VALUE_OK;
	}
	if ( wired_cvar_value_equal_ascii( text, "0" )
	  || wired_cvar_value_equal_ascii( text, "false" )
	  || wired_cvar_value_equal_ascii( text, "no" )
	  || wired_cvar_value_equal_ascii( text, "off" ) ) {
		parsed.integer = 0;
		parsed.number = 0.0f;
		parsed.normalized = "0";
		*out = parsed;
		return WIRED_CVAR_VALUE_OK;
	}
	return WIRED_CVAR_VALUE_INVALID;
}

wiredCvarValueStatus_t wired_cvar_value_int( const char *text, int minimum, int maximum,
	wiredCvarValue_t *out ) {
	wiredCvarValue_t parsed;
	char *end;
	long value;
	int parseErrno;
	int savedErrno;

	if ( !text || !text[0] || !out ) {
		return WIRED_CVAR_VALUE_INVALID;
	}
	savedErrno = errno;
	errno = 0;
	value = strtol( text, &end, 10 );
	parseErrno = errno;
	errno = savedErrno;
	if ( end == text || *end != '\0' || parseErrno == ERANGE
	  || value < INT_MIN || value > INT_MAX ) {
		return WIRED_CVAR_VALUE_INVALID;
	}
	if ( minimum != maximum && ( value < minimum || value > maximum ) ) {
		return WIRED_CVAR_VALUE_OUT_OF_RANGE;
	}
	parsed.integer = (int)value;
	parsed.number = (float)parsed.integer;
	parsed.normalized = NULL;
	*out = parsed;
	return WIRED_CVAR_VALUE_OK;
}

wiredCvarValueStatus_t wired_cvar_value_float( const char *text, float minimum, float maximum,
	wiredCvarValue_t *out ) {
	wiredCvarValue_t parsed;
	char *end;
	float value;
	int parseErrno;
	int savedErrno;

	if ( !text || !text[0] || !out ) {
		return WIRED_CVAR_VALUE_INVALID;
	}
	savedErrno = errno;
	errno = 0;
	value = strtof( text, &end );
	parseErrno = errno;
	errno = savedErrno;
	/* ERANGE covers both overflow and underflow. Reject both deliberately:
	 * silently collapsing an authored nonzero underflow to zero is not a valid
	 * typed-cvar normalization. */
	if ( end == text || *end != '\0' || parseErrno == ERANGE || !isfinite( value ) ) {
		return WIRED_CVAR_VALUE_INVALID;
	}
	/* cvar_t caches an integer view. Reject values whose conversion would be
	 * undefined instead of admitting a valid float with corrupt cached state.
	 * This representability check precedes the configured range so every such
	 * value has the same INVALID result, even when it also exceeds that range. */
	if ( (double)value < (double)INT_MIN || (double)value > (double)INT_MAX ) {
		return WIRED_CVAR_VALUE_INVALID;
	}
	if ( minimum != maximum && ( value < minimum || value > maximum ) ) {
		return WIRED_CVAR_VALUE_OUT_OF_RANGE;
	}
	parsed.integer = (int)value;
	parsed.number = value;
	parsed.normalized = NULL;
	*out = parsed;
	return WIRED_CVAR_VALUE_OK;
}

wiredCvarValueStatus_t wired_cvar_value_enum( const char *text,
	const char *const *values, size_t valueCount, wiredCvarValue_t *out ) {
	wiredCvarValue_t parsed;
	size_t index;

	if ( !text || !values || !out ) {
		return WIRED_CVAR_VALUE_INVALID;
	}
	for ( index = 0; index < valueCount; index++ ) {
		if ( values[index] && wired_cvar_value_equal_ascii( text, values[index] ) ) {
			if ( index > (size_t)INT_MAX ) {
				return WIRED_CVAR_VALUE_INVALID;
			}
			parsed.integer = (int)index;
			parsed.number = (float)index;
			parsed.normalized = values[index];
			*out = parsed;
			return WIRED_CVAR_VALUE_OK;
		}
	}
	return WIRED_CVAR_VALUE_INVALID;
}
