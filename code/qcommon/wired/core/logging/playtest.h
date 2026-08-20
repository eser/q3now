// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
playtest.h — wired_playtest.jsonl v1 evidence contract

WHAT THIS IS FOR
    The "no crash, but what happened?" session. An external alpha tester
    reports that a run went wrong; nobody was watching. This artefact is
    what makes that session reconstructible after the fact: one bounded,
    versioned, ordered timeline of typed events from build through map
    through failure.

WHY IT IS NOT JUST qconsole.jsonl
    qconsole.jsonl is a free-text console transcript: whatever a printf
    happened to say, unbounded in shape, with no per-record identity. It
    is written for a human reading a console. This file is written for a
    PROGRAM: every record carries the same envelope, every payload names
    a declared event type, and a consumer may rely on both. The two live
    side by side and share the same emission machinery (Com_Logv) — this
    is a second FORMAT, deliberately not a second logging PATH.

THE ENVELOPE (v1) — every record, no exceptions
    {"v":1,"seq":N,"t":<ms>,"sid":"<session>","build":"<id>","head":"<sha>",
     "plat":"<os-arch>","app":"<client|server>","map":"<name>",
     "ev":"<family.event>", ...payload }

    v      schema version. Bumped ONLY on a breaking change (see below).
    seq    monotonic per-session record counter, gap-free in the artefact
           unless drops occurred — a gap is therefore evidence, not noise.
    t      milliseconds since session start, from the monotonic clock
           (Sys_NanoTime), NOT wall clock: immune to NTP steps and DST.
    sid    session id, unique per engine boot. Correlates the artefact
           with the qconsole.jsonl of the same run.
    build  WIRED_BUILD_ID — which binary produced this.
    head   source revision the binary was built from, "unknown" if absent.
    plat   os-arch token, e.g. "darwin-arm64".
    app    which process wrote it: "client" or "server".
    map    current map at emit time, "" before any map has loaded.
    ev     "<family>.<event>", from the dictionary below. A consumer
           dispatches on this and may treat an unknown value as skippable.

    A consumer may rely on: v, seq, t, sid, ev being present on EVERY
    record; on t being non-decreasing within one `app`; and on the file
    being newline-delimited JSON with exactly one object per line.

EVENT DICTIONARY (v1) — the eight families named by the contract
    lifecycle.*   session_begin, session_end, map_load, map_loaded
    route.*       progress
    stuck.*       detected, recovered
    death.*       player
    weapon.*      fired
    ai.*          decision
    net.*         event
    perf.*        frame_marker

    Families are stable; new EVENTS may be added to a family within v1
    (a consumer dispatching on `ev` ignores what it does not know). What
    requires a v-bump: removing or renaming an envelope field, changing a
    field's type or units, or changing the meaning of an existing `ev`.

ORDERING GUARANTEE (stated, because #2 requires one to be stated)
    Records are assigned `seq` and `t` under the ring mutex at emit time,
    so seq order IS emit order across every thread. The artefact is
    written in seq order. Therefore: TOTAL order across all threads,
    established at emit, preserved through flush. This is stronger than
    the per-thread rings of log_buffer.c, which merge by timestamp and
    can interleave ambiguously when two threads share a nanosecond.

BOUNDED, WITH DROP ACCOUNTING
    The ring holds playtest_ring_capacity records (default 4096). When it
    wraps, the oldest record is overwritten — but never silently: the
    overwritten count is carried in `dropped_overwritten`, and the FIRST
    surviving record's seq tells a consumer exactly where the hole is.
    Records refused because the ring was not accepting (pre-init, disabled,
    payload too large) are counted separately. Both counters, plus the
    seq range, are written into the trailing `lifecycle.session_end`
    record, so the artefact SELF-REPORTS its own completeness. A consumer
    that reads only the artefact can tell whether it is looking at a whole
    session or a truncated one.

PRIVACY, RETENTION AND EXPORT (ratified 2026-08-20)
    What payloads may carry
        Typed scalars and short enum-like tokens only. No chat text, no
        player names, no auth tokens, no filesystem paths, no network
        addresses. Client SLOT NUMBERS are permitted: a slot is an index
        into a running server, meaningless once the session ends, and it
        is what makes "which player got stuck" answerable at all.
        A producer that needs something not on this list needs a decision
        first, not a workaround.

    Collection is OPT-IN
        playtest_enabled defaults to 0, so a build collects nothing until
        a run asks for it. This holds for alpha builds too — the default
        is not relaxed because a build is pre-release.

    Retention: one session
        Each session overwrites the artefact. There is no history and no
        rotation to reason about: the file always describes the run that
        just happened, which is the run someone is reporting.

    Size is bounded by construction, not by a separate cap
        The ring holds playtest_ring_capacity records (default 4096) and
        a payload cannot exceed PLAYTEST_PAYLOAD_MAX. Worst case is
        therefore ~2.6 MB, and it is reached by overwriting the OLDEST
        record — never by refusing new ones, and never silently: see the
        drop accounting above. No further cap is needed, and adding one
        would only introduce a second, less informative way to lose data.

    Export: manual, or automatic WITH CONSENT
        Today the only path out is the `playtestFlush` command — the
        artefact is written locally and goes nowhere on its own. Automatic
        upload is approved in principle but MUST be gated on an explicit
        per-session confirmation from the person running the build; it is
        not implemented yet. Until it is, nothing in this subsystem may
        open a network connection.
===========================================================================
*/
#pragma once

// q_shared.h and log.h must be included by the translation unit first.

// -------------------------------------------------------------------------
// Schema version. Bumped only on a breaking envelope change (see header).
// -------------------------------------------------------------------------

#define PLAYTEST_SCHEMA_VERSION   1

// Default artefact name, written under fs_homepath alongside qconsole.jsonl.
#define PLAYTEST_ARTEFACT_NAME    "wired_playtest.jsonl"

// Payload budget per record. A producer that exceeds it is counted as a
// refused record rather than emitting a truncated, unparseable line.
#define PLAYTEST_PAYLOAD_MAX      512

// Sampling interval for the periodic server-side perf/route markers, in
// server milliseconds. Chosen so a long session still fits the default ring:
// at 5s a 4096-record ring spans hours rather than the ~100 seconds a
// per-frame marker would give. See the emit site in sv_main.c.
#define PLAYTEST_PERF_INTERVAL_MS 5000

// Damage-event sampling divisor for the weapon family: one record per N
// damage events. Damage is by far the highest-rate signal that reaches the
// engine, and an unsampled tap would wrap the ring on a single firefight and
// bury the lifecycle records a report needs most. Emitted records carry
// "sample":N so a consumer never reads them as a complete damage count.
#define PLAYTEST_WEAPON_SAMPLE_N  16

// -------------------------------------------------------------------------
// Event dictionary. The string form is what lands in `ev`; keeping the ids
// in one enum is what lets the contract test enumerate the whole dictionary.
// -------------------------------------------------------------------------

typedef enum {
    PT_EV_SESSION_BEGIN = 0,   // "lifecycle.session_begin"
    PT_EV_SESSION_END,         // "lifecycle.session_end"
    PT_EV_MAP_LOAD,            // "lifecycle.map_load"
    PT_EV_MAP_LOADED,          // "lifecycle.map_loaded"
    PT_EV_ROUTE_PROGRESS,      // "route.progress"
    PT_EV_STUCK_DETECTED,      // "stuck.detected"
    PT_EV_STUCK_RECOVERED,     // "stuck.recovered"
    PT_EV_DEATH_PLAYER,        // "death.player"
    PT_EV_WEAPON_FIRED,        // "weapon.fired"
    PT_EV_AI_DECISION,         // "ai.decision"
    PT_EV_NET_EVENT,           // "net.event"
    PT_EV_PERF_FRAME_MARKER,   // "perf.frame_marker"
    // Emitted from a fault handler, immediately before the crash flush. It is
    // what separates "this session crashed" from "this session ended", because
    // the flush appends a lifecycle.session_end either way — so without this
    // record a crashed artefact reads as a clean shutdown, which is the exact
    // opposite of what a reader needs from it.
    // APPENDED, never inserted: these ids are index-aligned with the name table
    // and with tests/playtest-envelope-test.c, so a new value in the middle
    // would silently re-map every event after it.
    PT_EV_SESSION_FAULT,       // "lifecycle.session_fault"

    PT_EV_COUNT
} playtest_event_t;

// Maps an event id to its dotted "<family>.<event>" name. Returns
// "unknown.unknown" for an out-of-range id (never NULL).
const char *Playtest_EventName( playtest_event_t ev );

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------

// Playtest_Init: register cvars + the `playtestFlush` command, mint the
// session id, allocate the ring. Call after LogBuffer_Init in Com_Init.
// Collects nothing until playtest_enabled is 1 (opt-in by construction).
//
// `app_identity` is the `app` envelope field ("client" / "server"). It is a
// PARAMETER rather than an #ifdef inside playtest.c because that file compiles
// once into qcommon_engine_shared and links into both binaries; only a
// variant-compiled caller (common.c) knows which binary this is.
void Playtest_Init( const char *app_identity );

// Playtest_Shutdown: flush the ring to the artefact and release it. Call
// alongside LogBuffer_Shutdown in Com_Shutdown. Safe if Init never ran.
void Playtest_Shutdown( void );

// Playtest_Emit: record one typed event. `payload_json` is the payload's
// JSON object BODY without the surrounding braces — e.g.
//     Playtest_Emit( PT_EV_DEATH_PLAYER, "\"mod\":7,\"attacker\":2" );
// Pass "" for an event that needs no payload. Thread-safe; returns
// immediately when disabled. Never blocks on I/O: the flush is what
// touches the filesystem, never this call.
void Playtest_Emit( playtest_event_t ev, const char *payload_json );

// Playtest_EmitFmt: printf-style convenience over Playtest_Emit. The
// formatted result must be a payload BODY, same as above.
void FORMAT_PRINTF(2, 3) Playtest_EmitFmt( playtest_event_t ev, const char *fmt, ... );

// Playtest_SetMap: records the map name stamped into every subsequent
// envelope. Called by the server when a map spawns.
void Playtest_SetMap( const char *mapname );

// Playtest_Flush: write the ring to `filename` (NULL = the default
// artefact name) in seq order, terminated by a lifecycle.session_end
// record carrying the drop accounting. Returns the number of records
// written, or -1 if the file could not be opened.
int Playtest_Flush( const char *filename );

// Introspection, for the contract test and for `playtestStatus`.
// Playtest_Dropped* are cumulative counts since Init.
unsigned Playtest_DroppedOverwritten( void );  // ring wrapped over them
unsigned Playtest_DroppedRefused    ( void );  // never entered the ring
unsigned Playtest_Recorded          ( void );  // total accepted emits
