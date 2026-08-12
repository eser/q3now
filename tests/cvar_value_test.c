// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../code/qcommon/wired/core/cvars/cvar_value.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void expect_true( int condition, const char *name ) {
	if ( !condition ) {
		fprintf( stderr, "FAIL: %s\n", name );
		failures++;
	}
}

static wiredCvarValue_t sentinel( void ) {
	wiredCvarValue_t value;
	memset( &value, 0xa5, sizeof( value ) );
	return value;
}

static void expect_unchanged( const wiredCvarValue_t *before,
	const wiredCvarValue_t *after, const char *name ) {
	expect_true( memcmp( before, after, sizeof( *before ) ) == 0, name );
}

static void test_bool( void ) {
	static const char *const truthy[] = { "1", "true", "TRUE", "Yes", "oN" };
	static const char *const falsey[] = { "0", "false", "FALSE", "No", "oFf" };
	static const char *const invalid[] = { "2", "01", "1.0", "-0" };
	wiredCvarValue_t out;
	wiredCvarValue_t before;
	size_t i;

	for ( i = 0; i < sizeof( truthy ) / sizeof( truthy[0] ); i++ ) {
		expect_true( wired_cvar_value_bool( truthy[i], &out ) == WIRED_CVAR_VALUE_OK,
			"truthy alias accepted" );
		expect_true( out.integer == 1 && out.number == 1.0f
			&& strcmp( out.normalized, "1" ) == 0, "truthy canonicalized" );
	}
	for ( i = 0; i < sizeof( falsey ) / sizeof( falsey[0] ); i++ ) {
		expect_true( wired_cvar_value_bool( falsey[i], &out ) == WIRED_CVAR_VALUE_OK,
			"falsey alias accepted" );
		expect_true( out.integer == 0 && out.number == 0.0f
			&& strcmp( out.normalized, "0" ) == 0, "falsey canonicalized" );
	}
	before = sentinel();
	out = before;
	for ( i = 0; i < sizeof( invalid ) / sizeof( invalid[0] ); i++ ) {
		expect_true( wired_cvar_value_bool( invalid[i], &out ) == WIRED_CVAR_VALUE_INVALID,
			"non-alias boolean rejected" );
		expect_unchanged( &before, &out, "non-alias bool rejection is atomic" );
	}
	expect_true( wired_cvar_value_bool( " true", &out ) == WIRED_CVAR_VALUE_INVALID,
		"bool leading whitespace rejected" );
	expect_unchanged( &before, &out, "bool rejection leaves output unchanged" );
	expect_true( wired_cvar_value_bool( "false ", &out ) == WIRED_CVAR_VALUE_INVALID,
		"bool trailing whitespace rejected" );
	expect_unchanged( &before, &out, "bool trailing-space rejection is atomic" );
	expect_true( wired_cvar_value_bool( NULL, &out ) == WIRED_CVAR_VALUE_INVALID,
		"null bool rejected" );
	expect_true( wired_cvar_value_bool( "", &out ) == WIRED_CVAR_VALUE_INVALID,
		"empty bool rejected" );
	expect_true( wired_cvar_value_bool( "1", NULL ) == WIRED_CVAR_VALUE_INVALID,
		"null bool output rejected" );
	expect_unchanged( &before, &out, "null/empty bool rejection is atomic" );
}

static void test_int( void ) {
	wiredCvarValue_t out;
	wiredCvarValue_t before;

	expect_true( wired_cvar_value_int( "  +42", 0, 0, &out ) == WIRED_CVAR_VALUE_OK
		&& out.integer == 42 && out.number == 42.0f && out.normalized == NULL,
		"int preserves leading-whitespace and plus acceptance" );
	expect_true( wired_cvar_value_int( "00042", 0, 0, &out ) == WIRED_CVAR_VALUE_OK
		&& out.integer == 42, "int leading zero accepted as decimal" );
	expect_true( wired_cvar_value_int( "-2147483648", 0, 0, &out ) == WIRED_CVAR_VALUE_OK
		&& out.integer == INT_MIN, "INT_MIN accepted" );
	expect_true( wired_cvar_value_int( "2147483647", 0, 0, &out ) == WIRED_CVAR_VALUE_OK
		&& out.integer == INT_MAX, "INT_MAX accepted" );
	expect_true( wired_cvar_value_int( "1", 1, 10, &out ) == WIRED_CVAR_VALUE_OK,
		"int lower endpoint accepted" );
	expect_true( wired_cvar_value_int( "10", 1, 10, &out ) == WIRED_CVAR_VALUE_OK,
		"int upper endpoint accepted" );
	expect_true( wired_cvar_value_int( "42", 7, 7, &out ) == WIRED_CVAR_VALUE_OK,
		"equal nonzero int bounds remain unbounded" );
	errno = EDOM;
	expect_true( wired_cvar_value_int( "17", 0, 0, &out ) == WIRED_CVAR_VALUE_OK
		&& errno == EDOM, "int parse preserves caller errno" );

	before = sentinel();
	out = before;
	expect_true( wired_cvar_value_int( "4294967297", 0, 10, &out ) == WIRED_CVAR_VALUE_INVALID,
		"wrap-to-in-range integer rejected" );
	expect_unchanged( &before, &out, "wrap rejection leaves output unchanged" );
	expect_true( wired_cvar_value_int( "2147483648", 0, 0, &out ) == WIRED_CVAR_VALUE_INVALID,
		"INT_MAX plus one rejected" );
	expect_unchanged( &before, &out, "positive endpoint rejection is atomic" );
	expect_true( wired_cvar_value_int( "-2147483649", 0, 0, &out ) == WIRED_CVAR_VALUE_INVALID,
		"INT_MIN minus one rejected" );
	expect_true( wired_cvar_value_int( "999999999999999999999999", 0, 0, &out )
		== WIRED_CVAR_VALUE_INVALID, "huge integer rejected" );
	expect_true( wired_cvar_value_int( "0", 1, 10, &out ) == WIRED_CVAR_VALUE_OUT_OF_RANGE,
		"int below range rejected" );
	expect_unchanged( &before, &out, "int range rejection is atomic" );
	expect_true( wired_cvar_value_int( "11", 1, 10, &out ) == WIRED_CVAR_VALUE_OUT_OF_RANGE,
		"int above range rejected" );
	expect_true( wired_cvar_value_int( "42 ", 0, 0, &out ) == WIRED_CVAR_VALUE_INVALID,
		"int trailing whitespace rejected" );
	expect_true( wired_cvar_value_int( "42x", 0, 0, &out ) == WIRED_CVAR_VALUE_INVALID,
		"int trailing garbage rejected" );
	expect_true( wired_cvar_value_int( "1.0", 0, 0, &out ) == WIRED_CVAR_VALUE_INVALID,
		"decimal float spelling rejected as int" );
	expect_true( wired_cvar_value_int( "0x10", 0, 0, &out ) == WIRED_CVAR_VALUE_INVALID,
		"hex spelling rejected as base-10 int" );
	expect_true( wired_cvar_value_int( NULL, 0, 0, &out ) == WIRED_CVAR_VALUE_INVALID,
		"null int rejected" );
	expect_true( wired_cvar_value_int( "", 0, 0, &out ) == WIRED_CVAR_VALUE_INVALID,
		"empty int rejected" );
	expect_true( wired_cvar_value_int( "1", 0, 0, NULL ) == WIRED_CVAR_VALUE_INVALID,
		"null int output rejected" );
	expect_unchanged( &before, &out, "all int rejection paths preserve sentinel" );
}

static void test_float( void ) {
	wiredCvarValue_t out;
	wiredCvarValue_t before;

	expect_true( wired_cvar_value_float( "  +1.5", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_OK && out.number == 1.5f && out.integer == 1
		&& out.normalized == NULL, "float preserves leading-whitespace acceptance" );
	expect_true( wired_cvar_value_float( "1e2", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_OK && out.number == 100.0f && out.integer == 100,
		"float exponent accepted" );
	expect_true( wired_cvar_value_float( "-0.0", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_OK && out.number == 0.0f && out.integer == 0,
		"negative zero float accepted" );
	expect_true( wired_cvar_value_float( "-2", -2.0f, 2.0f, &out ) == WIRED_CVAR_VALUE_OK,
		"float lower endpoint accepted" );
	expect_true( wired_cvar_value_float( "2", -2.0f, 2.0f, &out ) == WIRED_CVAR_VALUE_OK,
		"float upper endpoint accepted" );
	expect_true( wired_cvar_value_float( "42", 7.0f, 7.0f, &out ) == WIRED_CVAR_VALUE_OK,
		"equal nonzero float bounds remain unbounded" );
	expect_true( wired_cvar_value_float( "2147483520", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_OK && out.number == 2147483520.0f
		&& out.integer == 2147483520, "largest nearby representable int-cache float accepted" );
	expect_true( wired_cvar_value_float( "-2147483648", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_OK && out.number == -2147483648.0f
		&& out.integer == INT_MIN, "INT_MIN float accepted" );
	errno = EDOM;
	expect_true( wired_cvar_value_float( "1.25", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_OK && errno == EDOM, "float parse preserves caller errno" );

	before = sentinel();
	out = before;
	expect_true( wired_cvar_value_float( "2147483647", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "INT_MAX text rounding above cache range rejected" );
	expect_unchanged( &before, &out, "rounded INT_MAX rejection is atomic" );
	expect_true( wired_cvar_value_float( "2147483648", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "INT_MAX plus one float rejected" );
	expect_unchanged( &before, &out, "INT_MAX plus one float rejection is atomic" );
	expect_true( wired_cvar_value_float( "nan", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "NaN rejected" );
	expect_unchanged( &before, &out, "NaN rejection leaves output unchanged" );
	expect_true( wired_cvar_value_float( "inf", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "positive infinity rejected" );
	expect_true( wired_cvar_value_float( "-INFINITY", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "negative infinity rejected" );
	expect_true( wired_cvar_value_float( "1e1000", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "float overflow rejected" );
	expect_true( wired_cvar_value_float( "1e-1000", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "float underflow rejected" );
	expect_true( wired_cvar_value_float( "1e30", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "finite float unsafe for integer cache rejected" );
	expect_true( wired_cvar_value_float( "3", -2.0f, 2.0f, &out )
		== WIRED_CVAR_VALUE_OUT_OF_RANGE, "float range rejection reported" );
	expect_true( wired_cvar_value_float( "-3", -2.0f, 2.0f, &out )
		== WIRED_CVAR_VALUE_OUT_OF_RANGE, "float below-range rejection reported" );
	expect_true( wired_cvar_value_float( "1.5 ", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "float trailing whitespace rejected" );
	expect_true( wired_cvar_value_float( "1.5x", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "float trailing garbage rejected" );
	expect_true( wired_cvar_value_float( NULL, 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "null float rejected" );
	expect_true( wired_cvar_value_float( "", 0.0f, 0.0f, &out )
		== WIRED_CVAR_VALUE_INVALID, "empty float rejected" );
	expect_true( wired_cvar_value_float( "1", 0.0f, 0.0f, NULL )
		== WIRED_CVAR_VALUE_INVALID, "null float output rejected" );
	expect_unchanged( &before, &out, "all float rejection paths preserve sentinel" );
}

static void test_enum( void ) {
	static const char *const values[] = { "nearest", "linear", "trilinear" };
	wiredCvarValue_t out;
	wiredCvarValue_t before;

	expect_true( wired_cvar_value_enum( "TrIlInEaR", values, 3, &out )
		== WIRED_CVAR_VALUE_OK, "enum case-insensitive match accepted" );
	expect_true( out.integer == 2 && out.number == 2.0f
		&& out.normalized == values[2], "enum canonical spelling and index returned" );
	before = sentinel();
	out = before;
	expect_true( wired_cvar_value_enum( " trilinear", values, 3, &out )
		== WIRED_CVAR_VALUE_INVALID, "enum leading whitespace rejected" );
	expect_unchanged( &before, &out, "enum rejection leaves output unchanged" );
	expect_true( wired_cvar_value_enum( "linear ", values, 3, &out )
		== WIRED_CVAR_VALUE_INVALID, "enum trailing whitespace rejected" );
	expect_true( wired_cvar_value_enum( "unknown", values, 3, &out )
		== WIRED_CVAR_VALUE_INVALID, "unknown enum rejected" );
	expect_true( wired_cvar_value_enum( "1", values, 3, &out )
		== WIRED_CVAR_VALUE_INVALID, "numeric enum index rejected" );
	expect_true( wired_cvar_value_enum( "linear", values, 0, &out )
		== WIRED_CVAR_VALUE_INVALID, "zero-count enum rejected" );
	expect_true( wired_cvar_value_enum( "linear", NULL, 3, &out )
		== WIRED_CVAR_VALUE_INVALID, "null enum values rejected" );
	expect_true( wired_cvar_value_enum( NULL, values, 3, &out )
		== WIRED_CVAR_VALUE_INVALID, "null enum rejected" );
	expect_true( wired_cvar_value_enum( "", values, 3, &out )
		== WIRED_CVAR_VALUE_INVALID, "empty enum rejected" );
	expect_true( wired_cvar_value_enum( "linear", values, 3, NULL )
		== WIRED_CVAR_VALUE_INVALID, "null enum output rejected" );
	expect_unchanged( &before, &out, "all enum rejection paths preserve sentinel" );
}

int main( void ) {
	test_bool();
	test_int();
	test_float();
	test_enum();
	if ( failures ) {
		fprintf( stderr, "%d typed-cvar assertion(s) failed\n", failures );
		return 1;
	}
	puts( "typed cvar value contract: PASS" );
	return 0;
}
