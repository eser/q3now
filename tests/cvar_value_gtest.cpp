// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// GoogleTest port of the typed-cvar value ingress contract.
//
// This is where text from outside the engine — a config file, a console line, a
// command-line +set, a server's userinfo — first becomes a number. Everything
// downstream trusts the result, so the interesting cases are the ones that must
// be REFUSED rather than coerced: partial parses, out-of-range values, and
// spellings that look plausible but are not.
//
// Standalone TU: production header in, production .c compiled beside it by
// CMake, engine never linked (see tests/fs_qpath_gtest.cpp for the same shape).

extern "C" {
#include "wired/core/cvars/cvar_value.h"
}

#include <gtest/gtest.h>

#include <cstring>
#include <limits>
#include <string>

namespace {

// ── bool ────────────────────────────────────────────────────────────────────

struct BoolCase {
	const char *text;
	int         expected;
};

class CvarBoolAccepted : public ::testing::TestWithParam<BoolCase> {};

TEST_P( CvarBoolAccepted, ParsesToExpected ) {
	const BoolCase &c = GetParam();
	wiredCvarValue_t v{};
	ASSERT_EQ( WIRED_CVAR_VALUE_OK, wired_cvar_value_bool( c.text, &v ) )
		<< "rejected: " << c.text;
	EXPECT_EQ( c.expected, v.integer ) << "wrong value for: " << c.text;
	// normalized is a borrowed pointer to a static literal, so it must be usable
	// after the call returns — a caller storing it outlives this frame.
	ASSERT_NE( nullptr, v.normalized );
	EXPECT_STREQ( c.expected ? "1" : "0", v.normalized );
}

// The word spellings are deliberate, not incidental: config files and console
// input are written by people, so "true"/"yes"/"on" are accepted alongside "1"
// and matching is ASCII case-insensitive (wired_cvar_value_equal_ascii). Pinned
// here because nothing else guarded it — a later "tighten this to 0/1 only"
// would silently invalidate every config using the word forms.
INSTANTIATE_TEST_SUITE_P( Spellings, CvarBoolAccepted, ::testing::Values(
	BoolCase{ "1",     1 },
	BoolCase{ "true",  1 },
	BoolCase{ "yes",   1 },
	BoolCase{ "on",    1 },
	BoolCase{ "TRUE",  1 },
	BoolCase{ "Yes",   1 },
	BoolCase{ "ON",    1 },
	BoolCase{ "0",     0 },
	BoolCase{ "false", 0 },
	BoolCase{ "no",    0 },
	BoolCase{ "off",   0 },
	BoolCase{ "FALSE", 0 },
	BoolCase{ "No",    0 },
	BoolCase{ "OfF",   0 } ) );

class CvarBoolRejected : public ::testing::TestWithParam<const char *> {};

TEST_P( CvarBoolRejected, IsRejected ) {
	wiredCvarValue_t v{};
	EXPECT_EQ( WIRED_CVAR_VALUE_INVALID, wired_cvar_value_bool( GetParam(), &v ) )
		<< "accepted a non-boolean: " << GetParam();
}

// "2" and "-1" matter most here: C's atoi-style coercion would happily turn them
// into truthy values, which is how a bool cvar silently acquires a third state.
// The surrounding-whitespace cases pin that the accepted spellings are matched
// whole — " 1" is not trimmed into "1", so leading space stays an error the user
// can see rather than a value that quietly applies.
INSTANTIATE_TEST_SUITE_P( NonBooleans, CvarBoolRejected, ::testing::Values(
	"", " ", "2", "-1", "01", "1.0", "0x1", "1 ", " 1",
	"tru", "truex", "onn", "y", "n", "enable", "disable" ) );

TEST( CvarBoolNull, NullTextRejected ) {
	wiredCvarValue_t v{};
	EXPECT_NE( WIRED_CVAR_VALUE_OK, wired_cvar_value_bool( nullptr, &v ) );
}

// ── int ─────────────────────────────────────────────────────────────────────

TEST( CvarInt, AcceptsInsideRange ) {
	wiredCvarValue_t v{};
	ASSERT_EQ( WIRED_CVAR_VALUE_OK, wired_cvar_value_int( "42", 0, 100, &v ) );
	EXPECT_EQ( 42, v.integer );
}

// Bounds are inclusive on both ends. Pinned explicitly because an off-by-one
// here is invisible in normal use and only shows up as a legitimate setting
// being refused at exactly the documented limit.
TEST( CvarInt, BoundsAreInclusive ) {
	wiredCvarValue_t lo{}, hi{};
	EXPECT_EQ( WIRED_CVAR_VALUE_OK, wired_cvar_value_int( "0", 0, 100, &lo ) );
	EXPECT_EQ( 0, lo.integer );
	EXPECT_EQ( WIRED_CVAR_VALUE_OK, wired_cvar_value_int( "100", 0, 100, &hi ) );
	EXPECT_EQ( 100, hi.integer );
}

TEST( CvarInt, OutOfRangeIsDistinctFromInvalid ) {
	// The two statuses are not interchangeable: a caller reports "3 is not a
	// number" very differently from "3 is outside 10..20", and collapsing them
	// makes the console message useless.
	wiredCvarValue_t v{};
	EXPECT_EQ( WIRED_CVAR_VALUE_OUT_OF_RANGE, wired_cvar_value_int( "9", 10, 20, &v ) );
	EXPECT_EQ( WIRED_CVAR_VALUE_OUT_OF_RANGE, wired_cvar_value_int( "21", 10, 20, &v ) );
	EXPECT_EQ( WIRED_CVAR_VALUE_INVALID,      wired_cvar_value_int( "abc", 10, 20, &v ) );
}

TEST( CvarInt, NegativeInsideRangeAccepted ) {
	wiredCvarValue_t v{};
	ASSERT_EQ( WIRED_CVAR_VALUE_OK, wired_cvar_value_int( "-5", -10, 10, &v ) );
	EXPECT_EQ( -5, v.integer );
}

class CvarIntRejected : public ::testing::TestWithParam<const char *> {};

TEST_P( CvarIntRejected, IsRejected ) {
	wiredCvarValue_t v{};
	EXPECT_EQ( WIRED_CVAR_VALUE_INVALID,
		wired_cvar_value_int( GetParam(), -1000, 1000, &v ) )
		<< "accepted a non-integer: " << GetParam();
}

// Trailing garbage is the important family. A parser that stops at the first
// non-digit turns "12abc" into 12 and "1 2" into 1, so a typo in a config file
// becomes a silently different setting instead of an error the user can see.
INSTANTIATE_TEST_SUITE_P( NonIntegers, CvarIntRejected, ::testing::Values(
	"", " ", "abc", "12abc", "1 2", "1.5", "0x10", "+", "-", "--1", "1-", "1e3" ) );

// The range check must survive values that overflow int. If the text is parsed
// into a wrapped int first and only then compared, an enormous number can land
// back inside the range as a negative — accepted, and wildly wrong.
TEST( CvarInt, HugeMagnitudeDoesNotWrapIntoRange ) {
	wiredCvarValue_t v{};
	EXPECT_NE( WIRED_CVAR_VALUE_OK,
		wired_cvar_value_int( "99999999999999999999", 0, 100, &v ) );
	EXPECT_NE( WIRED_CVAR_VALUE_OK,
		wired_cvar_value_int( "-99999999999999999999", 0, 100, &v ) );
}

// ── float ───────────────────────────────────────────────────────────────────

TEST( CvarFloat, AcceptsInsideRange ) {
	wiredCvarValue_t v{};
	ASSERT_EQ( WIRED_CVAR_VALUE_OK, wired_cvar_value_float( "0.5", 0.0f, 1.0f, &v ) );
	EXPECT_FLOAT_EQ( 0.5f, v.number );
}

TEST( CvarFloat, BoundsAreInclusive ) {
	wiredCvarValue_t lo{}, hi{};
	EXPECT_EQ( WIRED_CVAR_VALUE_OK, wired_cvar_value_float( "0", 0.0f, 1.0f, &lo ) );
	EXPECT_EQ( WIRED_CVAR_VALUE_OK, wired_cvar_value_float( "1", 0.0f, 1.0f, &hi ) );
}

TEST( CvarFloat, OutOfRangeIsDistinctFromInvalid ) {
	wiredCvarValue_t v{};
	EXPECT_EQ( WIRED_CVAR_VALUE_OUT_OF_RANGE,
		wired_cvar_value_float( "1.5", 0.0f, 1.0f, &v ) );
	EXPECT_EQ( WIRED_CVAR_VALUE_INVALID,
		wired_cvar_value_float( "wat", 0.0f, 1.0f, &v ) );
}

TEST( CvarFloat, IntegerTextIsAValidFloat ) {
	// Config files are full of "1" for float cvars; refusing them would break
	// existing content for no benefit.
	wiredCvarValue_t v{};
	ASSERT_EQ( WIRED_CVAR_VALUE_OK, wired_cvar_value_float( "2", 0.0f, 10.0f, &v ) );
	EXPECT_FLOAT_EQ( 2.0f, v.number );
}

class CvarFloatRejected : public ::testing::TestWithParam<const char *> {};

TEST_P( CvarFloatRejected, IsRejected ) {
	wiredCvarValue_t v{};
	EXPECT_EQ( WIRED_CVAR_VALUE_INVALID,
		wired_cvar_value_float( GetParam(), -1000.0f, 1000.0f, &v ) )
		<< "accepted a non-float: " << GetParam();
}

// "nan" and "inf" are the ones that matter: strtof accepts both, and either one
// reaching a renderer or physics cvar poisons every later computation while
// passing any naive min/max comparison (all comparisons against NaN are false).
INSTANTIATE_TEST_SUITE_P( NonFloats, CvarFloatRejected, ::testing::Values(
	"", " ", "abc", "1.5x", "1,5", "--1", "1.2.3", "nan", "inf", "-inf", "NaN" ) );

// ── enum ────────────────────────────────────────────────────────────────────

const char *const kPalette[] = { "classic", "modern", "high-contrast" };
constexpr size_t kPaletteCount = sizeof( kPalette ) / sizeof( kPalette[0] );

TEST( CvarEnum, AcceptsEachMemberAndReportsItsIndex ) {
	for ( size_t i = 0; i < kPaletteCount; ++i ) {
		wiredCvarValue_t v{};
		ASSERT_EQ( WIRED_CVAR_VALUE_OK,
			wired_cvar_value_enum( kPalette[i], kPalette, kPaletteCount, &v ) )
			<< "rejected its own member: " << kPalette[i];
		EXPECT_EQ( static_cast<int>( i ), v.integer );
		ASSERT_NE( nullptr, v.normalized );
		EXPECT_STREQ( kPalette[i], v.normalized );
	}
}

TEST( CvarEnum, UnknownMemberRejected ) {
	wiredCvarValue_t v{};
	EXPECT_NE( WIRED_CVAR_VALUE_OK,
		wired_cvar_value_enum( "retro", kPalette, kPaletteCount, &v ) );
}

TEST( CvarEnum, MatchIsExactNotPrefix ) {
	// A prefix match would make "class" silently select "classic", so a typo
	// would apply a setting the user did not ask for.
	wiredCvarValue_t v{};
	EXPECT_NE( WIRED_CVAR_VALUE_OK,
		wired_cvar_value_enum( "class", kPalette, kPaletteCount, &v ) );
	EXPECT_NE( WIRED_CVAR_VALUE_OK,
		wired_cvar_value_enum( "classicx", kPalette, kPaletteCount, &v ) );
}

TEST( CvarEnum, EmptyDescriptorAcceptsNothing ) {
	// A cvar declared with no members must refuse every input rather than
	// reading past the end of a zero-length array.
	wiredCvarValue_t v{};
	EXPECT_NE( WIRED_CVAR_VALUE_OK,
		wired_cvar_value_enum( "classic", kPalette, 0, &v ) );
	EXPECT_NE( WIRED_CVAR_VALUE_OK,
		wired_cvar_value_enum( "", kPalette, 0, &v ) );
}

TEST( CvarEnum, EmptyTextRejected ) {
	wiredCvarValue_t v{};
	EXPECT_NE( WIRED_CVAR_VALUE_OK,
		wired_cvar_value_enum( "", kPalette, kPaletteCount, &v ) );
}

// The descriptor is borrowed, so the returned pointer must alias the caller's
// storage rather than a copy — the header documents this and callers rely on it
// for cheap comparisons.
TEST( CvarEnum, NormalizedAliasesTheCallersDescriptor ) {
	wiredCvarValue_t v{};
	ASSERT_EQ( WIRED_CVAR_VALUE_OK,
		wired_cvar_value_enum( "modern", kPalette, kPaletteCount, &v ) );
	EXPECT_EQ( kPalette[1], v.normalized );
}

} // namespace
