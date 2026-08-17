// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors
//
// Contract for the wired_playtest.jsonl v1 evidence envelope (TASK-120 #1).
//
// WHAT A CONTRACT TEST IS FOR HERE
//   wired_playtest.jsonl is read by PROGRAMS, not people: the alpha map-race
//   harness dispatches on `ev`, and any future triage tooling will rely on the
//   envelope being the same shape on every record. That makes the schema an
//   interface, and an interface that nothing pins drifts silently — a renamed
//   field or a reordered dictionary still compiles, still runs, still writes a
//   plausible-looking file, and breaks every consumer at once. Nothing at
//   runtime can catch that, because a wrong-but-well-formed record is
//   indistinguishable from a right one without knowing what was intended.
//
// SO WHAT IS PINNED
//   1. The schema VERSION. v1 is frozen; bumping it is a deliberate act that
//      must break this test and be re-blessed, not a silent side effect.
//   2. Every ENVELOPE FIELD a consumer may rely on, checked in the actual
//      emitter format string — so deleting or renaming one fails here.
//   3. The EVENT DICTIONARY: the enum and the name table must stay
//      index-aligned and complete. A name inserted in the middle of the table
//      without matching the enum would silently relabel every event of every
//      later family — the artefact would look fine and mean something else.
//   4. That all eight declared FAMILIES are present, since the task's whole
//      point is one timeline covering lifecycle/route/stuck/death/weapon/AI/
//      network/perf.
//   5. The DROP-ACCOUNTING fields, because a bounded ring whose overflow is
//      not reported is the specific failure this feature exists to prevent.
//
// WHY IT READS SOURCE INSTEAD OF LINKING THE TU
//   playtest.c reaches the filesystem, the cvar registry and the mutex layer,
//   so linking it would pull in most of the engine and turn a subsystem
//   contract into an integration build. The properties above are all
//   statically present in the source, which is exactly the tactic
//   tests/bg-layer-zorder-test.c uses for the same reason.
//
// PLAYTEST_SOURCE / PLAYTEST_HEADER are set by CMake to absolute paths.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PLAYTEST_SOURCE
#error "PLAYTEST_SOURCE must be defined (absolute path to playtest.c)"
#endif
#ifndef PLAYTEST_HEADER
#error "PLAYTEST_HEADER must be defined (absolute path to playtest.h)"
#endif

#define MAX_SRC ( 1024 * 1024 )

static int failures;

static void Check( const char *what, int got, int want )
{
	if ( got != want ) {
		printf( "  FAIL %-58s got %d, want %d\n", what, got, want );
		failures++;
	} else {
		printf( "  ok   %-58s (%d)\n", what, got );
	}
}

static char *SlurpOrDie( const char *path, size_t *len_out )
{
	static char bufs[2][MAX_SRC];
	static int  which = 0;
	char       *buf   = bufs[which++];
	FILE       *f     = fopen( path, "rb" );
	size_t      len;

	if ( !f ) {
		printf( "  FAIL cannot open %s\n", path );
		exit( 1 );
	}
	len = fread( buf, 1, MAX_SRC - 1, f );
	fclose( f );
	buf[len] = '\0';
	if ( len_out ) *len_out = len;
	return buf;
}

// Every event id in the v1 dictionary, in enum order. The test owns this list
// INDEPENDENTLY of the production table: that is the whole point — if the two
// disagree, one of them changed and a human has to say which is right.
static const struct {
	const char *enumerator;
	const char *name;
} k_dictionary[] = {
	{ "PT_EV_SESSION_BEGIN",     "lifecycle.session_begin" },
	{ "PT_EV_SESSION_END",       "lifecycle.session_end"   },
	{ "PT_EV_MAP_LOAD",          "lifecycle.map_load"      },
	{ "PT_EV_MAP_LOADED",        "lifecycle.map_loaded"    },
	{ "PT_EV_ROUTE_PROGRESS",    "route.progress"          },
	{ "PT_EV_STUCK_DETECTED",    "stuck.detected"          },
	{ "PT_EV_STUCK_RECOVERED",   "stuck.recovered"         },
	{ "PT_EV_DEATH_PLAYER",      "death.player"            },
	{ "PT_EV_WEAPON_FIRED",      "weapon.fired"            },
	{ "PT_EV_AI_DECISION",       "ai.decision"             },
	{ "PT_EV_NET_EVENT",         "net.event"               },
	{ "PT_EV_PERF_FRAME_MARKER", "perf.frame_marker"       }
};
#define DICT_COUNT ( (int)( sizeof( k_dictionary ) / sizeof( k_dictionary[0] ) ) )

// The eight payload families the contract promises.
static const char *k_families[] = {
	"lifecycle.", "route.", "stuck.", "death.",
	"weapon.", "ai.", "net.", "perf."
};
#define FAMILY_COUNT ( (int)( sizeof( k_families ) / sizeof( k_families[0] ) ) )

// Envelope keys, as they appear in the emitter's format string. A consumer is
// told it may rely on these being present on EVERY record.
static const char *k_envelope[] = {
	"\\\"v\\\":", "\\\"seq\\\":", "\\\"t\\\":", "\\\"sid\\\":",
	"\\\"build\\\":", "\\\"head\\\":", "\\\"plat\\\":", "\\\"app\\\":",
	"\\\"map\\\":", "\\\"ev\\\":"
};
#define ENVELOPE_COUNT ( (int)( sizeof( k_envelope ) / sizeof( k_envelope[0] ) ) )

// Drop-accounting keys in the trailing session_end record.
static const char *k_accounting[] = {
	"records_written", "seq_first", "seq_last",
	"dropped_overwritten", "dropped_refused", "ring_capacity", "complete"
};
#define ACCOUNTING_COUNT ( (int)( sizeof( k_accounting ) / sizeof( k_accounting[0] ) ) )

int main( void )
{
	size_t  srclen, hdrlen;
	char   *src = SlurpOrDie( PLAYTEST_SOURCE, &srclen );
	char   *hdr = SlurpOrDie( PLAYTEST_HEADER, &hdrlen );
	int     i;

	printf( "playtest wired_playtest.jsonl v1 envelope contract\n" );

	// A file we failed to really read would make every strstr below vacuous
	// and pass the whole suite on nothing.
	Check( "production source is non-empty", srclen > 2048, 1 );
	Check( "production header is non-empty", hdrlen > 2048, 1 );
	Check( "source is the playtest emitter",
		strstr( src, "Playtest_WriteRecord" ) != NULL, 1 );

	// ── 1. schema version is frozen at 1 ────────────────────────────────
	// Pinned in BOTH files: the header defines it, the emitter must actually
	// write it. A version defined but not emitted is not a versioned format.
	Check( "header freezes PLAYTEST_SCHEMA_VERSION at 1",
		strstr( hdr, "#define PLAYTEST_SCHEMA_VERSION   1" ) != NULL, 1 );
	Check( "emitter writes the schema version into every record",
		strstr( src, "PLAYTEST_SCHEMA_VERSION" ) != NULL, 1 );
	Check( "artefact name is wired_playtest.jsonl",
		strstr( hdr, "\"wired_playtest.jsonl\"" ) != NULL, 1 );

	// ── 2. every envelope field is emitted ──────────────────────────────
	for ( i = 0; i < ENVELOPE_COUNT; i++ ) {
		char label[128];
		snprintf( label, sizeof( label ), "envelope field emitted: %s", k_envelope[i] );
		Check( label, strstr( src, k_envelope[i] ) != NULL, 1 );
	}

	// ── 3. dictionary: enum and name table agree, in order ──────────────
	// Checked positionally, not just by presence: a table whose entries are
	// all present but permuted relabels whole families while still parsing.
	{
		const char *table = strstr( src, "s_eventNames[PT_EV_COUNT]" );
		Check( "event-name table is present", table != NULL, 1 );

		if ( table ) {
			const char *cursor = table;
			int         inOrder = 1;

			for ( i = 0; i < DICT_COUNT; i++ ) {
				char quoted[96];
				const char *at;
				snprintf( quoted, sizeof( quoted ), "\"%s\"", k_dictionary[i].name );
				at = strstr( cursor, quoted );
				if ( !at ) {
					printf( "  FAIL dictionary entry %d (%s) missing from table\n",
						i, k_dictionary[i].name );
					failures++;
					inOrder = 0;
					break;
				}
				cursor = at + strlen( quoted );
			}
			Check( "name table lists all v1 events in enum order", inOrder, 1 );
		}

		for ( i = 0; i < DICT_COUNT; i++ ) {
			char label[128];
			snprintf( label, sizeof( label ), "enumerator declared: %s",
				k_dictionary[i].enumerator );
			Check( label, strstr( hdr, k_dictionary[i].enumerator ) != NULL, 1 );
		}
	}

	// ── 4. all eight families are represented ───────────────────────────
	for ( i = 0; i < FAMILY_COUNT; i++ ) {
		char label[128];
		int  found = 0, j;
		for ( j = 0; j < DICT_COUNT; j++ ) {
			if ( strncmp( k_dictionary[j].name, k_families[i],
			              strlen( k_families[i] ) ) == 0 ) {
				found = 1;
				break;
			}
		}
		snprintf( label, sizeof( label ), "payload family present: %s", k_families[i] );
		Check( label, found, 1 );
	}

	// ── 5. drop accounting reaches the artefact ─────────────────────────
	// The counters existing in memory is not the contract; the contract is
	// that a consumer reading ONLY the file can see them.
	for ( i = 0; i < ACCOUNTING_COUNT; i++ ) {
		char label[128];
		snprintf( label, sizeof( label ), "drop accounting emitted: %s", k_accounting[i] );
		Check( label, strstr( src, k_accounting[i] ) != NULL, 1 );
	}
	Check( "session_end carries the accounting record",
		strstr( src, "PT_EV_SESSION_END" ) != NULL, 1 );

	// ── 6. boundedness and its refusal path ─────────────────────────────
	Check( "ring capacity is bounded by a cvar",
		strstr( src, "playtest_ring_capacity" ) != NULL, 1 );
	Check( "oversized payloads are refused and counted",
		strstr( src, "s_droppedRefused++" ) != NULL, 1 );
	Check( "overwrites are counted, not silent",
		strstr( src, "s_droppedOverwrite++" ) != NULL, 1 );

	// ── 7. privacy posture: opt-in by construction ──────────────────────
	// Provisional pending Eser's policy call (#6), but pinned so that
	// flipping the default to opt-out cannot happen unnoticed.
	Check( "collection is opt-in (playtest_enabled defaults to 0)",
		strstr( src, "CVAR_BOOL( \"playtest_enabled\", \"0\"" ) != NULL, 1 );

	printf( "\n" );
	if ( failures ) {
		printf( "FAILED: %d contract violation(s)\n", failures );
		return 1;
	}
	printf( "PASS: wired_playtest.jsonl v1 envelope contract intact\n" );
	return 0;
}
