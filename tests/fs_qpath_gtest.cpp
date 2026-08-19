// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// GoogleTest port of the lexical qpath containment contract.
//
// Why this exists alongside tests/fs_qpath_test.c: the C test asserts the same
// inputs, but every case reports through one shared string ("unsafe qpath
// rejected"), so a regression tells you that one of eighteen inputs broke
// without saying which. Parameterised cases give each input its own CTest entry
// and its own name, so a failure names the offending path directly.
//
// Same standalone-TU rule as the C contracts: this includes the production
// HEADER and CMake compiles the production .c beside it. The engine is never
// linked, so nothing here can pass because some unrelated subsystem initialised
// a global.

extern "C" {
#include "wired/core/vfs/fs_qpath.h"
}

#include <gtest/gtest.h>

#include <string>

namespace {

// Parent-directory escapes and the legacy "::" spelling. A qpath reaching the
// filesystem layer must be rejected before a handle is allocated or an OS path
// is built, so each of these is a security boundary, not a style preference.
class QPathRejected : public ::testing::TestWithParam<const char *> {};

TEST_P( QPathRejected, IsRejected ) {
	EXPECT_FALSE( wired_fs_qpath_is_safe( GetParam() ) )
		<< "qpath escapes containment but was accepted: " << GetParam();
}

INSTANTIATE_TEST_SUITE_P( ParentEscapes, QPathRejected, ::testing::Values(
	"..",
	"../secret",
	"..\\secret",
	"base/..",
	"base\\..",
	"base/../secret",
	"base\\..\\secret",
	"base//..//secret",
	"base\\\\..\\\\secret",
	"base/..\\secret",
	"base\\../secret",
	"/../secret",
	"\\..\\secret",
	"base/child/..",
	"::",
	"base::secret",
	"base/child::leaf" ) );

// Paths that merely CONTAIN dots or a colon are legitimate and must survive.
// Over-rejection is a real failure mode here: a validator that refuses
// "..hidden" or "foo..bar" silently breaks content that has every right to load,
// and that shows up as a missing asset rather than as a security message.
class QPathAccepted : public ::testing::TestWithParam<const char *> {};

TEST_P( QPathAccepted, IsAccepted ) {
	EXPECT_TRUE( wired_fs_qpath_is_safe( GetParam() ) )
		<< "legitimate qpath was rejected: " << GetParam();
}

INSTANTIATE_TEST_SUITE_P( NonEscaping, QPathAccepted, ::testing::Values(
	"",
	".",
	"./",
	"base",
	"base/file.cfg",
	"base\\file.cfg",
	"base//child///file",
	"base\\\\child/file",
	"foo..bar",
	"...",
	"..hidden",
	"hidden..",
	"base/.../file",
	"base/./file",
	"base/.hidden/file",
	"base:leaf" ) );

TEST( QPathNull, NullIsRejected ) {
	// A null qpath must be refused, not crash: callers reach this from parsing
	// paths out of untrusted content, where a null is a plausible input.
	EXPECT_FALSE( wired_fs_qpath_is_safe( nullptr ) );
}

// Length is not a containment property — a long but well-formed path stays
// valid. Pinned because a future bounds check is a plausible place to
// accidentally introduce a limit that silently truncates content paths.
TEST( QPathLength, LongWellFormedPathAccepted ) {
	std::string deep;
	for ( int i = 0; i < 64; ++i ) {
		deep += "segment/";
	}
	deep += "file.cfg";
	EXPECT_TRUE( wired_fs_qpath_is_safe( deep.c_str() ) );
}

// The escape has to be caught wherever it sits, not just near the front. A
// scanner that only inspects a prefix would pass the rejection list above (its
// escapes are all early) while still letting a deep path out of containment.
TEST( QPathLength, EscapeDeepInLongPathRejected ) {
	std::string deep;
	for ( int i = 0; i < 64; ++i ) {
		deep += "segment/";
	}
	deep += "../secret";
	EXPECT_FALSE( wired_fs_qpath_is_safe( deep.c_str() ) );
}

} // namespace
