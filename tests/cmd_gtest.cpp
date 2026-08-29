// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// Command layer: tokenizer and registration/dispatch contracts.
//
// Everything a user types, every line of a .cfg, and every command a server
// sends arrives through this tokenizer. Its edge cases are the kind that lose
// data quietly — a token silently truncated at a "//" inside a URL looks like a
// connection bug, not a parser bug — so they are pinned here rather than
// rediscovered from a symptom.
//
// Unlike the other suites here, this one links stubs (tests/cmd_gtest_stubs.c)
// because cmd.c is not a carved-out seam; see that file's header for why that
// tradeoff was made and why it should not be copied by default.

extern "C" {
#include "q_shared.h"
#include "qcommon.h"

void CmdTest_SetServerDispatch( qboolean running, qboolean spawnIdle,
	qboolean handled );
int CmdTest_ServerGameCalls( void );
}

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

// Tokenize and snapshot the result. Cmd_Argv returns pointers into the
// tokenizer's own buffer, which the next tokenize call overwrites, so tests must
// copy rather than hold.
std::vector<std::string> Tokenize( const char *text, bool ignoreQuotes = false ) {
	if ( ignoreQuotes ) {
		Cmd_TokenizeStringIgnoreQuotes( text );
	} else {
		Cmd_TokenizeString( text );
	}
	std::vector<std::string> out;
	const int argc = Cmd_Argc();
	out.reserve( static_cast<size_t>( argc ) );
	for ( int i = 0; i < argc; ++i ) {
		out.emplace_back( Cmd_Argv( i ) );
	}
	return out;
}

// ── basic splitting ─────────────────────────────────────────────────────────

TEST( CmdTokenize, SplitsOnWhitespace ) {
	EXPECT_EQ( ( std::vector<std::string>{ "set", "name", "value" } ),
		Tokenize( "set name value" ) );
}

TEST( CmdTokenize, CollapsesRunsOfWhitespace ) {
	EXPECT_EQ( ( std::vector<std::string>{ "set", "name" } ),
		Tokenize( "  set   \t  name  " ) );
}

TEST( CmdTokenize, EmptyInputYieldsNoTokens ) {
	EXPECT_EQ( 0, static_cast<int>( Tokenize( "" ).size() ) );
	EXPECT_EQ( 0, static_cast<int>( Tokenize( "   " ).size() ) );
}

// Argv past the end must be a stable empty string, not a null. Callers index it
// unconditionally — `Cmd_Argv(2)` on a one-argument command is routine — and a
// null there turns a missing argument into a crash.
TEST( CmdTokenize, ArgvBeyondArgcIsEmptyNotNull ) {
	Tokenize( "one two" );
	ASSERT_EQ( 2, Cmd_Argc() );
	const char *past = Cmd_Argv( 2 );
	ASSERT_NE( nullptr, past );
	EXPECT_STREQ( "", past );
	const char *wayPast = Cmd_Argv( 99 );
	ASSERT_NE( nullptr, wayPast );
	EXPECT_STREQ( "", wayPast );
}

TEST( CmdTokenize, NegativeArgvIndexIsEmptyNotACrash ) {
	Tokenize( "one two" );
	const char *neg = Cmd_Argv( -1 );
	ASSERT_NE( nullptr, neg );
	EXPECT_STREQ( "", neg );
}

// ── quoting ─────────────────────────────────────────────────────────────────

TEST( CmdTokenize, QuotedStringIsOneToken ) {
	EXPECT_EQ( ( std::vector<std::string>{ "say", "hello there" } ),
		Tokenize( "say \"hello there\"" ) );
}

TEST( CmdTokenize, UnterminatedQuoteTakesTheRest ) {
	// Not an error: the remainder becomes the token. Pinned because the
	// alternative (dropping it) would silently swallow a user's chat line.
	EXPECT_EQ( ( std::vector<std::string>{ "say", "hello there" } ),
		Tokenize( "say \"hello there" ) );
}

TEST( CmdTokenize, EmptyQuotedStringIsAToken ) {
	// An empty argument is meaningful — `set var ""` clears a cvar — so the
	// quotes must produce a token rather than vanishing.
	const auto t = Tokenize( "set var \"\"" );
	ASSERT_EQ( 3u, t.size() );
	EXPECT_EQ( "", t[2] );
}

// There is no backslash escaping (cmd.c says so in a note). A test asserting
// \" produced a literal quote would be asserting a feature that does not exist,
// so this pins the actual behaviour: the backslash is an ordinary character and
// the quote still closes the string.
TEST( CmdTokenize, BackslashDoesNotEscapeQuote ) {
	const auto t = Tokenize( "say \"a\\\"b\"" );
	ASSERT_GE( t.size(), 2u );
	EXPECT_EQ( "a\\", t[1] );
}

TEST( CmdTokenizeIgnoreQuotes, QuotesAreOrdinaryCharacters ) {
	// The ignore-quotes spelling exists for input that must not be re-quoted,
	// e.g. forwarding a raw line. The quote characters survive into the tokens.
	const auto t = Tokenize( "say \"hello there\"", /*ignoreQuotes=*/true );
	ASSERT_EQ( 3u, t.size() );
	EXPECT_EQ( "\"hello", t[1] );
	EXPECT_EQ( "there\"", t[2] );
}

// ── comments ────────────────────────────────────────────────────────────────

TEST( CmdTokenize, LineCommentEndsTheLine ) {
	EXPECT_EQ( ( std::vector<std::string>{ "set", "name" } ),
		Tokenize( "set name // trailing comment" ) );
}

TEST( CmdTokenize, BlockCommentIsSkipped ) {
	EXPECT_EQ( ( std::vector<std::string>{ "set", "name" } ),
		Tokenize( "set /* inline */ name" ) );
}

TEST( CmdTokenize, UnterminatedBlockCommentEatsTheRest ) {
	EXPECT_EQ( ( std::vector<std::string>{ "set" } ),
		Tokenize( "set /* never closed" ) );
}

// The one case where "//" must NOT start a comment: a protocol separator. The
// tokenizer specifically allows [a-z]:// so addresses survive. Losing this turns
// `connect http://host` into `connect http:` — a connection failure with no
// visible cause, which is exactly why it is pinned.
TEST( CmdTokenize, ProtocolSeparatorIsNotAComment ) {
	const auto t = Tokenize( "connect http://example.com/path" );
	ASSERT_EQ( 2u, t.size() );
	EXPECT_EQ( "http://example.com/path", t[1] );
}

TEST( CmdTokenize, CommentImmediatelyAfterTokenStillEndsTheLine ) {
	// No space before "//": the guard is a URL exception, not a blanket
	// "// is fine when attached to a token".
	EXPECT_EQ( ( std::vector<std::string>{ "set", "name" } ),
		Tokenize( "set name// comment" ) );
}

// ── newlines ────────────────────────────────────────────────────────────────

TEST( CmdTokenize, NewlineIsOrdinaryWhitespace ) {
	// The tokenizer does NOT treat a newline as a line terminator — it splits on
	// it like any other whitespace. Splitting a multi-line paste into separate
	// commands is the command buffer's job, one layer up. Pinned because the
	// opposite is the intuitive guess, and a "fix" that made this stop at the
	// newline would silently drop the tail of every multi-line exec.
	EXPECT_EQ( ( std::vector<std::string>{ "first", "second" } ),
		Tokenize( "first\nsecond" ) );
}

// ── Cmd_Args / ArgsFrom ─────────────────────────────────────────────────────

TEST( CmdArgs, ArgsReturnsEverythingAfterTheCommand ) {
	Tokenize( "say hello there world" );
	EXPECT_STREQ( "hello there world", Cmd_ArgsFrom( 1 ) );
}

TEST( CmdArgs, ArgsFromIndexIsInclusive ) {
	Tokenize( "say hello there world" );
	EXPECT_STREQ( "there world", Cmd_ArgsFrom( 2 ) );
}

TEST( CmdArgs, ArgsOnBareCommandIsEmpty ) {
	Tokenize( "status" );
	EXPECT_STREQ( "", Cmd_ArgsFrom( 1 ) );
}

TEST( CmdArgs, ArgsFromPastEndIsEmptyNotNull ) {
	Tokenize( "say hi" );
	const char *past = Cmd_ArgsFrom( 9 );
	ASSERT_NE( nullptr, past );
	EXPECT_STREQ( "", past );
}

// ── registration / dispatch ─────────────────────────────────────────────────

int g_alphaCalls = 0;
int g_betaCalls  = 0;

void AlphaHandler() { ++g_alphaCalls; }
void BetaHandler()  { ++g_betaCalls; }

class CmdRegistry : public ::testing::Test {
protected:
	void SetUp() override {
		g_alphaCalls = 0;
		g_betaCalls  = 0;
	}
	void TearDown() override {
		// Registration is global process state; leaving entries behind would
		// make later tests depend on execution order.
		Cmd_RemoveCommand( "gtest_alpha" );
		Cmd_RemoveCommand( "gtest_beta" );
	}
};

TEST_F( CmdRegistry, RegisteredCommandIsDispatched ) {
	Cmd_AddCommand( "gtest_alpha", AlphaHandler );
	Cmd_ExecuteString( "gtest_alpha" );
	EXPECT_EQ( 1, g_alphaCalls );
}

TEST_F( CmdRegistry, DispatchIsCaseInsensitive ) {
	// Console input is typed by hand and .cfg files are written by many people;
	// "Quit" and "quit" have to reach the same handler.
	Cmd_AddCommand( "gtest_alpha", AlphaHandler );
	Cmd_ExecuteString( "GTEST_ALPHA" );
	Cmd_ExecuteString( "GtEsT_aLpHa" );
	EXPECT_EQ( 2, g_alphaCalls );
}

TEST_F( CmdRegistry, HandlerSeesItsOwnArguments ) {
	static std::vector<std::string> seen;
	seen.clear();
	Cmd_AddCommand( "gtest_alpha", [] {
		for ( int i = 0; i < Cmd_Argc(); ++i ) {
			seen.emplace_back( Cmd_Argv( i ) );
		}
	} );
	Cmd_ExecuteString( "gtest_alpha one \"two three\"" );
	ASSERT_EQ( 3u, seen.size() );
	EXPECT_EQ( "gtest_alpha", seen[0] );
	EXPECT_EQ( "one", seen[1] );
	EXPECT_EQ( "two three", seen[2] );
}

TEST_F( CmdRegistry, RemovedCommandNoLongerDispatches ) {
	Cmd_AddCommand( "gtest_alpha", AlphaHandler );
	Cmd_RemoveCommand( "gtest_alpha" );
	Cmd_ExecuteString( "gtest_alpha" );
	EXPECT_EQ( 0, g_alphaCalls );
}

TEST_F( CmdRegistry, RemovingAnUnknownCommandIsHarmless ) {
	Cmd_RemoveCommand( "gtest_never_registered" );
	SUCCEED();
}

TEST_F( CmdRegistry, CommandsAreIndependent ) {
	Cmd_AddCommand( "gtest_alpha", AlphaHandler );
	Cmd_AddCommand( "gtest_beta",  BetaHandler );
	Cmd_ExecuteString( "gtest_beta" );
	EXPECT_EQ( 0, g_alphaCalls );
	EXPECT_EQ( 1, g_betaCalls );
}

TEST_F( CmdRegistry, UnknownCommandDoesNotDispatchAnything ) {
	Cmd_AddCommand( "gtest_alpha", AlphaHandler );
	Cmd_ExecuteString( "gtest_no_such_command" );
	EXPECT_EQ( 0, g_alphaCalls );
}

TEST_F( CmdRegistry, ServerGameCommandWaitsForAsyncMapCompletion ) {
	Cbuf_Init();
	CmdTest_SetServerDispatch( qtrue, qfalse, qtrue );
	Cmd_ExecuteString( "addbot grunt 4 free" );
	EXPECT_EQ( 0, CmdTest_ServerGameCalls() );

	CmdTest_SetServerDispatch( qtrue, qtrue, qtrue );
	/* Frame-end releases the one-frame yield armed by the deferral; the next
	 * command-buffer cycle owns the preserved command exactly once. */
	Cbuf_Wait();
	Cbuf_Execute();
	EXPECT_EQ( 1, CmdTest_ServerGameCalls() );
	CmdTest_SetServerDispatch( qfalse, qtrue, qfalse );
}

TEST_F( CmdRegistry, ReRegisteringKeepsTheFirstHandler ) {
	// First registration wins: a duplicate Cmd_AddCommand is refused outright
	// (it logs "already defined" and returns) rather than replacing the entry.
	//
	// This is the engine's actual policy, and it has a consequence worth knowing
	// when reading a bug report: a module that re-registers a command on reload
	// keeps dispatching into whatever was registered FIRST. Removing the command
	// before re-adding it is the way to actually swap a handler.
	Cmd_AddCommand( "gtest_alpha", AlphaHandler );
	Cmd_AddCommand( "gtest_alpha", BetaHandler );
	Cmd_ExecuteString( "gtest_alpha" );
	EXPECT_EQ( 1, g_alphaCalls );
	EXPECT_EQ( 0, g_betaCalls );
}

TEST_F( CmdRegistry, RemoveThenAddDoesSwapTheHandler ) {
	// The supported way to replace a handler, given the policy above.
	Cmd_AddCommand( "gtest_alpha", AlphaHandler );
	Cmd_RemoveCommand( "gtest_alpha" );
	Cmd_AddCommand( "gtest_alpha", BetaHandler );
	Cmd_ExecuteString( "gtest_alpha" );
	EXPECT_EQ( 0, g_alphaCalls );
	EXPECT_EQ( 1, g_betaCalls );
}

} // namespace
