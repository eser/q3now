#!/usr/bin/env bash
# headless-map-transition-zonecheck.sh — display-free second-map Z_Free gate (#96).
#
# #96 is the "FATAL: Server crashed" that fired on the FIRST FRAME OF THE SECOND
# MAP (originally Windows, 2026-06-15). The zone allocator's consistency
# terminate is `Z_Free: freed a pointer without ZONEID` (code/qcommon/common.c:740).
#
# WHY THIS IS HEADLESS-ABLE (the whole point of this file):
#   The suspect path is server-side memory lifetime across a map change. The GUI
#   and headless binaries share the SAME object code for it — qcommon_engine_shared
#   (CMakeLists.txt:245-249) is injected byte-identically into both (:2872, :2903),
#   and the transition's zone work is unguarded:
#     code/server/sv_init.c:434-435  Hunk_ClearLevel(); CM_ClearMap();  (no #ifndef HEADLESS)
#     code/server/sv_init.c:531      CM_LoadMap()                       (unconditional)
#     code/server/sv_init.c:115,344,371,1116  the Z_Free() call sites
#   FEAT_HEADLESS_RENDERER is 0 (code/qcommon/q_feats.h:95), so no renderer is
#   linked or reachable. A GPU is NOT required to exercise this crash class.
#
# 🔴 BUILD-CONFIG CONTRACT — READ BEFORE TRUSTING A PASS:
#   The ZONEID assertion is compiled ONLY into _DEBUG builds:
#     code/qcommon/common.c:409-412   #ifdef _DEBUG / #define USE_ZONE_ID / #endif
#     code/qcommon/common.c:739-743   the terminate is #ifdef USE_ZONE_ID
#   and _DEBUG is set only for Debug (CMakeLists.txt:2835). In a RELEASE build the
#   check does not exist, so a release run cannot witness #96 — it would corrupt
#   silently instead. This gate therefore REFUSES a binary without the assertion
#   (fail-closed, exit 1) rather than reporting a vacuous PASS. Point it at a
#   DEBUG wired-headless.
#
# The chain is arena1 -> e1m1 -> arena7: three loads, two transitions, and e1m1
# crosses the Q1 BSP format (code/qcommon/cm_q1.c is dense with Z_Free), matching
# the recorded probe in the wiki (alpha-ux-defect-sweep, 2026-08-11).
#
# NOTE: `+waitForMap` is CLIENT-ONLY (registered at code/client/cl_main.c:5430),
# so a headless run must gate with bare `+wait N` (cmd.c:1287).
#
# Usage:   tests/headless-map-transition-zonecheck.sh /path/to/wired-headless
#          tests/headless-map-transition-zonecheck.sh --repeat N /path/to/wired-headless
#          tests/headless-map-transition-zonecheck.sh --self-test
#          tests/headless-map-transition-zonecheck.sh --analyze <qconsole> <stdout> <rc>
#          tests/headless-map-transition-zonecheck.sh --flag-inventory <binary> [maps...]
#
# --repeat N runs the chain N times, each a fresh process with a fresh log, and
# fails if ANY iteration fails. The class this gate targets is lifecycle/race,
# which a single run cannot bound: see the "repeat mode" block below for what
# actually varies between iterations.
# Exit:    0 PASS   1 FAIL   64 usage   77 SKIP (no binary / no packs)
#
# --flag-inventory reuses this script's pack discovery + scratch-home setup to
# answer a DIFFERENT question: which team-game flag entities does a given map
# actually spawn? It exists because that question kept getting answered by hand,
# and because `modfiles/maps/*.ent` is a REPO-LOCAL OVERRIDE set (one file,
# arena1) — not the shipped map list. The BSPs live in pax01.sw3z, so grepping
# the repo systematically under-reports and had already produced one wrong
# conclusion ("arena1 is the only shipped map, so CTF cannot work").
# Unlike the #96 gate this mode does NOT require a debug build: entity spawning
# is independent of USE_ZONE_ID.
#
# It is also a GATE, not merely informational: since the engine now places a
# neutral flag at the centre of the walkable area on maps that lack one, a map
# reporting NEUTRAL=no is a real defect (1FCTF unplayable there) and exits 1.
# NEUTRAL=via-fb is that fallback having engaged; NEUTRAL=yes is an authored flag.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

MAP_CHAIN="arena1 e1m1 arena7"

# The zone allocator's consistency terminates (code/qcommon/common.c) plus the
# crash funnel string emitted by log.c:878.
ZONE_RE='Z_Free: freed a pointer without ZONEID|Z_Free: freed a freed pointer|Z_Free: memory block wrote past end|Z_CheckHeap: block size does not touch the next block|Z_CheckHeap: next block does not have proper back link|Z_CheckHeap: two consecutive free blocks'
CRASH_RE='Server crashed|Server fatal crashed'

# 🔴 A hard fault is NOT covered by CRASH_RE. When the process takes SIGSEGV/
# SIGBUS the platform signal handler prints its own backtrace and funnels
# through a DIFFERENT string — `Signal caught (N)`, at INFO severity, via
# `----- Server Shutdown (Signal caught (10)) -----`:
#   code/unix/linux_signals.c:72,88,92   (=== CRASH BACKTRACE / Received signal / Signal caught)
#   code/win32/win_main.c:855            (Exception Code: <NAME>)
# None of that is a FATAL record, none of it says "Server crashed", and every
# map in the chain can already have logged its `Server:` line before the fault
# lands. A SIGBUS in CM_TraceThroughTree on the first frame of the third map
# therefore passed all five original checks — the exact lifecycle class this
# gate exists for, reported green. Matched on BOTH channels because the
# backtrace goes to stderr while the shutdown line goes to the JSONL sink.
SIGNAL_RE='CRASH BACKTRACE \(signal|Received signal [0-9]+, exiting|Signal caught \([0-9]+\)|crash log written to|Exception Code:'

# ── analyzer ─────────────────────────────────────────────────────────────────
# Pure function of (qconsole.jsonl, stdout, rc). Factored out so --self-test can
# feed it synthetic logs with no engine. Echoes a report; returns 0 PASS / 1 FAIL.
analyze_contract() {
    local LOG="$1" OUT="$2" RC="$3"
    local rows fail=0

    [ -s "$LOG" ] || { echo "  FAIL: qconsole.jsonl absent/empty — the engine did not run this invocation"; return 1; }

    # 1. every map in the chain actually loaded (a silent no-op must not pass).
    # Parsed as JSON rather than grepped: the msg is `Server: arena1\n`, and an
    # exact token match keeps `arena1` from being satisfied by `arena17`.
    local MISSING
    MISSING="$(python3 - "$LOG" $MAP_CHAIN <<'PYEOF' 2>/dev/null
import json,sys
log,maps=sys.argv[1],sys.argv[2:]
seen=set()
for line in open(log):
    line=line.strip()
    if not line: continue
    try: row=json.loads(line)
    except ValueError: continue
    msg=str(row.get("msg","")).strip()
    if msg.startswith("Server: "):
        seen.add(msg[len("Server: "):].strip())
print(" ".join(m for m in maps if m not in seen))
PYEOF
)"
    if [ -n "$MISSING" ]; then
        local m
        for m in $MISSING; do
            echo "  FAIL: map '$m' never loaded (chain did not complete)"
        done
        fail=1
    fi

    # 2. zone-consistency complaints anywhere (log or stdout)
    local ZHIT
    ZHIT="$( { cat "$LOG"; [ -f "$OUT" ] && cat "$OUT"; } 2>/dev/null | grep -nE "$ZONE_RE" )"
    if [ -n "$ZHIT" ]; then
        echo "  FAIL: zone-consistency complaint (#96 class) —"
        printf '%s\n' "$ZHIT" | sed 's/^/      /'
        fail=1
    fi

    # 3. server crash funnel
    local CHIT
    CHIT="$( { cat "$LOG"; [ -f "$OUT" ] && cat "$OUT"; } 2>/dev/null | grep -nE "$CRASH_RE" )"
    if [ -n "$CHIT" ]; then
        echo "  FAIL: server crash —"
        printf '%s\n' "$CHIT" | sed 's/^/      /'
        fail=1
    fi

    # 3b. hard fault: signal handler / structured exception. See SIGNAL_RE.
    local SIGHIT
    SIGHIT="$( { cat "$LOG"; [ -f "$OUT" ] && cat "$OUT"; } 2>/dev/null | grep -nE "$SIGNAL_RE" )"
    if [ -n "$SIGHIT" ]; then
        echo "  FAIL: process took a fatal signal / structured exception —"
        printf '%s\n' "$SIGHIT" | head -6 | sed 's/^/      /'
        fail=1
    fi

    # 4. no FATAL severity records
    local SEV
    SEV="$(python3 - "$LOG" <<'PYEOF' 2>/dev/null
import json,sys
n=0
for line in open(sys.argv[1]):
    line=line.strip()
    if not line: continue
    try: row=json.loads(line)
    except ValueError: continue
    if str(row.get("sev","")).upper() in ("FATAL",): n+=1
print(n)
PYEOF
)"
    if [ "${SEV:-0}" -gt 0 ]; then
        echo "  FAIL: $SEV FATAL severity record(s) in qconsole.jsonl"
        fail=1
    fi

    # 5. clean shutdown
    if [ "$RC" -ne 0 ]; then
        echo "  FAIL: exit code $RC (want 0 — clean +quit)"
        fail=1
    fi

    [ "$fail" -eq 0 ] || return 1
    echo "  PASS: 3 loads / 2 transitions (incl. the Q1-format e1m1 leg), zero zone complaints, clean rc=0"
    return 0
}

if [ "${1:-}" = "--analyze" ]; then
    [ "$#" -eq 4 ] || { echo "usage: $0 --analyze <qconsole> <stdout> <rc>"; exit 64; }
    analyze_contract "$2" "$3" "$4"; exit $?
fi

# ── playtest evidence analyzer (TASK-120 #3) ─────────────────────────────────
# The map-race consumer of wired_playtest.jsonl v1. Where analyze_contract
# above reads the free-text console transcript looking for one crash class,
# this reads the TYPED artefact and asks whether the session it describes can
# be reconstructed at all: does every record carry the v1 envelope, is the
# order intact, did each map in the chain get bracketed by a load/loaded pair,
# and — the part that makes the artefact trustworthy — does the file's own
# drop accounting say it is complete?
#
# Pure function of (artefact, expected map chain), for the same reason
# analyze_contract is: --playtest-self-test feeds it synthetic files with no
# engine, so the gate can be shown to have teeth on a machine with no packs.
analyze_playtest() {
    local ART="$1"; shift
    local WANT_MAPS="$*"
    local fail=0

    [ -s "$ART" ] || {
        echo "  FAIL: $ART absent/empty — no playtest evidence was produced"
        return 1
    }

    local REPORT
    REPORT="$(python3 - "$ART" $WANT_MAPS <<'PYEOF' 2>&1
import json, sys

art, want_maps = sys.argv[1], sys.argv[2:]
rows, bad = [], 0
for n, line in enumerate(open(art), 1):
    line = line.strip()
    if not line:
        continue
    try:
        rows.append(json.loads(line))
    except ValueError:
        bad += 1
        print(f"FAIL: line {n} is not valid JSON")

if bad:
    print(f"FAIL: {bad} unparseable line(s) — the artefact is not JSONL")
if not rows:
    print("FAIL: artefact contains no records")
    sys.exit(0)

# 1. v1 envelope on EVERY record. This is the promise a consumer is given;
#    if it does not hold, nothing downstream can rely on anything.
REQUIRED = ("v", "seq", "t", "sid", "build", "head", "plat", "app", "map", "ev")
missing = {}
for r in rows:
    for k in REQUIRED:
        if k not in r:
            missing[k] = missing.get(k, 0) + 1
for k, n in sorted(missing.items()):
    print(f"FAIL: envelope field '{k}' missing from {n} record(s)")

# 2. schema version is the one this consumer understands.
vers = {r.get("v") for r in rows}
if vers != {1}:
    print(f"FAIL: expected schema v1 throughout, saw {sorted(vers)}")

# 3. one session, and the ordering guarantee the contract states.
sids = {r.get("sid") for r in rows}
if len(sids) != 1:
    print(f"FAIL: artefact mixes {len(sids)} sessions: {sorted(sids)}")

seqs = [r.get("seq") for r in rows]
if seqs != sorted(seqs):
    print("FAIL: seq is not monotonic — the ordering guarantee is broken")
ts = [r.get("t") for r in rows]
if ts != sorted(ts):
    print("FAIL: t is not non-decreasing — timeline is not reconstructible")

# 4. the session opened and closed.
evs = [r.get("ev") for r in rows]
if "lifecycle.session_begin" not in evs:
    print("FAIL: no lifecycle.session_begin — session start is unknown")
if evs[-1] != "lifecycle.session_end":
    print(f"FAIL: last record is '{evs[-1]}', want lifecycle.session_end")

# 5. every map in the chain was bracketed load -> loaded. An unclosed
#    bracket is the signature of dying mid-transition, which is exactly the
#    distinction this artefact exists to preserve.
loaded = {r.get("map") for r in rows if r.get("ev") == "lifecycle.map_loaded"}
for m in want_maps:
    if m not in loaded:
        print(f"FAIL: map '{m}' never reached lifecycle.map_loaded")

# 6. the self-report of the artefact: is it complete, and does it agree with
#    what is actually in the file?
end = rows[-1] if evs[-1] == "lifecycle.session_end" else None
if end:
    for k in ("records_written", "seq_first", "seq_last",
              "dropped_overwritten", "dropped_refused",
              "ring_capacity", "complete"):
        if k not in end:
            print(f"FAIL: session_end lacks drop-accounting field '{k}'")
    if end.get("dropped_overwritten", 0) or end.get("dropped_refused", 0):
        print("WARN: session is INCOMPLETE — "
              f"{end.get('dropped_overwritten')} overwritten, "
              f"{end.get('dropped_refused')} refused")
    body = len(rows) - 1
    if end.get("records_written") != body:
        print(f"FAIL: session_end claims {end.get('records_written')} records, "
              f"file carries {body}")

fams = sorted({e.split(".")[0] for e in evs})
print(f"INFO: {len(rows)} records, families: {', '.join(fams)}")
print(f"INFO: build={rows[0].get('build')} head={rows[0].get('head')} "
      f"plat={rows[0].get('plat')} app={rows[0].get('app')}")
if end:
    print(f"INFO: complete={end.get('complete')} "
          f"overwritten={end.get('dropped_overwritten')} "
          f"refused={end.get('dropped_refused')} "
          f"capacity={end.get('ring_capacity')}")
PYEOF
)"

    printf '%s\n' "$REPORT" | sed 's/^/      /'
    printf '%s' "$REPORT" | grep -q '^FAIL:' && fail=1

    [ "$fail" -eq 0 ] || return 1
    return 0
}

if [ "${1:-}" = "--analyze-playtest" ]; then
    [ "$#" -ge 2 ] || { echo "usage: $0 --analyze-playtest <artefact> [maps...]"; exit 64; }
    ART="$2"; shift 2
    analyze_playtest "$ART" "${*:-$MAP_CHAIN}"; exit $?
fi

# ── playtest timeline (TASK-120 #5) ──────────────────────────────────────────
#
# --analyze-playtest asks "is this artefact intact?" — a gate, pass or fail.
# This asks the question someone actually has when a playtester sends a file
# after a crash: WHAT HAPPENED? Which build, which map, how far in, and what was
# the last thing the engine did before it stopped.
#
# Deliberately a separate mode. A validator that also narrated would have to
# decide whether a missing session_end is a defect — it is, for the gate — or
# simply the shape of a crashed session, which it also is, for a timeline.
# Conflating the two makes one of the answers wrong.
#
# Reads ONLY the artefact: no engine, no source tree, no companion log, because
# the artefact is the one thing a reporter can actually send.
playtest_timeline() {
    local artefact="$1"

    [ -f "$artefact" ] || { echo "FAIL: no artefact at '$artefact'"; return 1; }

    python3 - "$artefact" <<'PYEOF'
import json, sys

path = sys.argv[1]
rows, bad = [], 0
with open(path) as fh:
    for line in fh:
        line = line.strip()
        if not line:
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            # A crash can cut the file mid-line. That is evidence about how the
            # session ended, not a reason to refuse to read the rest.
            bad += 1

if not rows:
    print("FAIL: artefact holds no readable records")
    sys.exit(1)

first = rows[0]
print("=== session ===")
print(f"  build          : {first.get('build')}  (rev {first.get('head')})")
print(f"  platform       : {first.get('plat')}   app: {first.get('app')}")
print(f"  session id     : {first.get('sid')}")
print(f"  records        : {len(rows)}" + (f"  (+{bad} unreadable)" if bad else ""))

# The last lifecycle.session_end carries the accounting of the ring. Its ABSENCE
# is the strongest signal the artefact holds that the process died rather than
# exited — which is exactly the case this mode exists for.
# A fault record beats the session_end record. The crash handler emits one and
# THEN flushes, and the flush always appends a session_end — so an artefact from
# a crashed process carries both, and reading only the session_end would report
# a clean shutdown for a process that segfaulted.
fault = next((r for r in reversed(rows) if r.get("ev") == "lifecycle.session_fault"), None)
end = next((r for r in reversed(rows) if r.get("ev") == "lifecycle.session_end"), None)
if fault is not None:
    detail = ", ".join(f"{k}={v}" for k, v in fault.items()
                       if k in ("signal", "reason"))
    print(f"  termination    : CRASHED at {fault.get('t')} ms ({detail})")
elif end is None:
    print("  termination    : NO session_end — the process did not shut down cleanly")
else:
    complete = end.get("complete")
    print(f"  termination    : clean (complete={complete})")
    if complete is False:
        print(f"                   ring overflowed: {end.get('dropped_overwritten')} record(s) "
              f"overwritten, timeline starts at seq {end.get('seq_first')}")

# Map progression in order, with the time each load landed: the build->map->
# failure spine the criterion asks for.
print("=== map progression ===")
seen_any = False
for r in rows:
    ev = r.get("ev")
    if ev == "lifecycle.map_load":
        print(f"  {r.get('t'):>8} ms  loading {r.get('map') or '?'}"
              + (f"  [{r.get('phase')}]" if r.get("phase") else ""))
        seen_any = True
    elif ev == "lifecycle.map_loaded":
        print(f"  {r.get('t'):>8} ms  LOADED  {r.get('map') or '?'}")
        seen_any = True
if not seen_any:
    print("  (no map lifecycle events — the session ended before loading a map)")

# What the engine was doing last. On a crashed session this is the closest thing
# to a cause the artefact can offer, so it is printed even when it looks dull.
print("=== final activity ===")
tail = [r for r in rows if r.get("ev") != "lifecycle.session_end"][-8:]
for r in tail:
    extra = {k: v for k, v in r.items()
             if k not in ("v", "seq", "t", "sid", "build", "head", "plat", "app", "map", "ev")}
    detail = ("  " + " ".join(f"{k}={v}" for k, v in extra.items())) if extra else ""
    print(f"  {r.get('t'):>8} ms  {r.get('ev')}{detail}")

last = tail[-1] if tail else first
print("=== summary ===")
print(f"  Build {first.get('build')} ({first.get('head')}) on {first.get('plat')}, "
      f"last map '{last.get('map') or '?'}', "
      f"ran {last.get('t')} ms, ended on '{last.get('ev')}'"
      + (f", CRASHED (signal {fault.get('signal')})" if fault is not None
         else "" if end is not None else ", NO clean shutdown"))
PYEOF
}

if [ "${1:-}" = "--playtest-timeline" ]; then
    [ "$#" -ge 2 ] || { echo "usage: $0 --playtest-timeline <artefact>"; exit 64; }
    playtest_timeline "$2"; exit $?
fi

# ── playtest consumer self-test (engine-free, gate-has-teeth) ─────────────────
# Same discipline as --self-test above: build a clean synthetic artefact, prove
# the analyzer ACCEPTS it, then break one property at a time and prove it
# REJECTS each. An evidence gate that cannot reject corrupt evidence would
# certify every artefact, including the ones that lost records.
write_playtest_self() {
    local art="$1" mode="$2"
    python3 - "$art" "$mode" <<'PYEOF'
import json, sys
art, mode = sys.argv[1], sys.argv[2]

def env(seq, t, ev, mapname="arena1", **payload):
    r = {"v": 1, "seq": seq, "t": t, "sid": "deadbeefcafe0001",
         "build": "999", "head": "abc1234", "plat": "macos-arm64",
         "app": "server", "map": mapname, "ev": ev}
    r.update(payload)
    return r

rows = [
    env(0, 10, "lifecycle.session_begin"),
    env(1, 10, "lifecycle.map_load", phase="p1_teardown"),
    env(2, 900, "lifecycle.map_loaded", clients=8, gametype=0),
    env(3, 900, "perf.frame_marker", svtime=450, msec=50, residual=0),
    env(4, 900, "route.progress", svtime=450, clients=0),
    env(5, 1200, "death.player", victim=1, attacker=2, mod=22),
    env(6, 1300, "weapon.fired", attacker=2, victim=1, damage=75, mod=3, sample=16),
    env(7, 1400, "ai.decision", bot=1, kind="strafejump", p1=1, p2=320),
    env(8, 2000, "lifecycle.map_load", mapname="e1m1", phase="p1_teardown"),
    env(9, 3000, "lifecycle.map_loaded", mapname="e1m1", clients=8, gametype=0),
]
end = env(10, 3100, "lifecycle.session_end", mapname="e1m1",
          records_written=10, seq_first=0, seq_last=9,
          dropped_overwritten=0, dropped_refused=0,
          ring_capacity=4096, complete=True)

if mode == "no-begin":
    rows = [r for r in rows if r["ev"] != "lifecycle.session_begin"]
    end["records_written"] = len(rows)
elif mode == "seq-scrambled":
    rows[3]["seq"], rows[4]["seq"] = rows[4]["seq"], rows[3]["seq"]
    rows[3], rows[4] = rows[4], rows[3]
    rows[3]["seq"], rows[4]["seq"] = rows[4]["seq"], rows[3]["seq"]
elif mode == "time-goes-backwards":
    rows[5]["t"] = 5
elif mode == "missing-envelope-field":
    del rows[4]["sid"]
elif mode == "wrong-schema-version":
    rows[6]["v"] = 2
elif mode == "unclosed-map":
    rows = [r for r in rows if not (r["ev"] == "lifecycle.map_loaded"
                                    and r["map"] == "e1m1")]
    end["records_written"] = len(rows)
elif mode == "no-session-end":
    end = None
elif mode == "accounting-mismatch":
    end["records_written"] = 999
elif mode == "accounting-field-missing":
    del end["dropped_overwritten"]
elif mode == "two-sessions":
    rows[7]["sid"] = "deadbeefcafe0002"
elif mode == "corrupt-json":
    with open(art, "w") as f:
        for r in rows:
            f.write(json.dumps(r) + "\n")
        f.write("{not json at all\n")
        f.write(json.dumps(end) + "\n")
    sys.exit(0)
elif mode == "empty":
    open(art, "w").close()
    sys.exit(0)

with open(art, "w") as f:
    for r in rows:
        f.write(json.dumps(r) + "\n")
    if end is not None:
        f.write(json.dumps(end) + "\n")
PYEOF
}

if [ "${1:-}" = "--playtest-self-test" ]; then
    echo "==> playtest evidence consumer SELF-TEST (gate-has-teeth, engine-free)"
    PST="$(mktemp -d -t pt-self-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$PST"' EXIT INT TERM

    write_playtest_self "$PST/clean.jsonl" clean
    if ! analyze_playtest "$PST/clean.jsonl" arena1 e1m1 >"$PST/clean.report" 2>&1; then
        echo "  FAIL: analyzer rejected a CLEAN artefact"
        sed 's/^/      /' "$PST/clean.report"
        exit 1
    fi
    echo "  ok   clean                          accepted"

    pt_defects="no-begin seq-scrambled time-goes-backwards missing-envelope-field
                wrong-schema-version unclosed-map no-session-end accounting-mismatch
                accounting-field-missing two-sessions corrupt-json empty"
    pt_fails=0; pt_n=0
    for d in $pt_defects; do
        pt_n=$((pt_n+1))
        write_playtest_self "$PST/$d.jsonl" "$d"
        if analyze_playtest "$PST/$d.jsonl" arena1 e1m1 >"$PST/$d.report" 2>&1; then
            echo "  FAIL $d — analyzer ACCEPTED a defective artefact"
            pt_fails=$((pt_fails+1))
        else
            printf '  ok   %-30s rejected\n' "$d"
        fi
    done

    if [ "$pt_fails" -eq 0 ]; then
        echo "==> PLAYTEST SELF-TEST PASS: clean accepted, $pt_n mutations rejected"
        exit 0
    fi
    echo "==> PLAYTEST SELF-TEST FAIL: $pt_fails/$pt_n wrongly accepted"
    exit 1
fi

# ── self-test (gate-has-teeth, no engine, no packs, no display) ───────────────
# Builds a clean synthetic log, asserts the analyzer ACCEPTS it, then mutates it
# one defect at a time and asserts the analyzer REJECTS each. A gate that cannot
# reject the crash it was written for is worthless.
write_self() {
    local log="$1" out="$2" mode="$3"
    : > "$out"
    python3 - "$log" "$mode" <<'PYEOF'
import json,sys
log,mode=sys.argv[1],sys.argv[2]
rows=[]
def add(sev,cat,msg): rows.append({"sev":sev,"cat":cat,"msg":msg+"\n"})
for m in ("arena1","e1m1","arena7"):
    add("INFO","server",f"Server: {m}")
    add("INFO","game",f"InitGame: \\mapname\\{m}\\protocol\\74")
add("INFO","server","----- Server Shutdown (Server quit) -----")

if mode=="zoneid":
    rows.insert(3,{"sev":"FATAL","cat":"system","msg":"Z_Free: freed a pointer without ZONEID\n"})
elif mode=="freed-freed":
    rows.insert(3,{"sev":"FATAL","cat":"system","msg":"Z_Free: freed a freed pointer\n"})
elif mode=="past-end":
    rows.insert(3,{"sev":"FATAL","cat":"system","msg":"Z_Free: memory block wrote past end\n"})
elif mode=="checkheap":
    rows.insert(3,{"sev":"FATAL","cat":"system","msg":"Z_CheckHeap: two consecutive free blocks\n"})
elif mode=="crashed":
    rows.insert(4,{"sev":"INFO","cat":"server","msg":"----- Server Shutdown (Server crashed: Z_Free: freed a pointer without ZONEID) -----\n"})
elif mode=="fatal-sev":
    rows.insert(3,{"sev":"FATAL","cat":"system","msg":"unspecified fatal\n"})
elif mode=="missing-second":
    rows=[r for r in rows if "e1m1" not in r["msg"]]
elif mode=="missing-third":
    rows=[r for r in rows if "arena7" not in r["msg"]]
elif mode=="signal-shutdown":
    # A hard fault funnels through the shutdown line at INFO severity, AFTER
    # every map in the chain already logged its `Server:` line. Nothing else
    # in the log distinguishes it from a clean run.
    rows.append({"sev":"INFO","cat":"server","msg":"----- Server Shutdown (Signal caught (10)) -----\n"})
elif mode=="empty":
    rows=[]

with open(log,"w") as f:
    for r in rows: f.write(json.dumps(r)+"\n")
PYEOF
    # stdout-channel defect: the crash can surface outside the JSONL sink.
    if [ "$mode" = "stdout-zone" ]; then
        printf 'Server fatal crashed: Z_Free: freed a pointer without ZONEID\n' > "$out"
    fi
    # A hard fault writes its backtrace to STDERR only — the JSONL sink never
    # sees it. Verbatim shape from code/unix/linux_signals.c:72-88.
    if [ "$mode" = "stdout-signal" ]; then
        printf '=== CRASH BACKTRACE (signal 10) ===\n0   wired-headless.arm64  0x0 CM_TraceThroughTree + 128\n=== crash log written to /tmp/wired_crash.txt ===\nReceived signal 10, exiting...\n' > "$out"
    fi
}

if [ "${1:-}" = "--self-test" ]; then
    echo "==> headless map-transition zonecheck SELF-TEST (gate-has-teeth, engine-free)"
    ST="$(mktemp -d -t hmtz-self-XXXXXX 2>/dev/null || mktemp -d)"
    trap 'rm -rf "$ST"' EXIT INT TERM

    write_self "$ST/clean.jsonl" "$ST/clean.out" clean
    if ! analyze_contract "$ST/clean.jsonl" "$ST/clean.out" 0 >"$ST/clean.report" 2>&1; then
        echo "  FAIL: analyzer rejected a CLEAN log"; sed 's/^/      /' "$ST/clean.report"; exit 1
    fi
    echo "  ok   clean                        accepted"

    # Each defect must be REJECTED. rc-nonzero is checked via the rc argument.
    defects="zoneid freed-freed past-end checkheap crashed fatal-sev missing-second missing-third empty stdout-zone signal-shutdown stdout-signal"
    fails=0; n=0
    for d in $defects; do
        n=$((n+1))
        write_self "$ST/$d.jsonl" "$ST/$d.out" "$d"
        if analyze_contract "$ST/$d.jsonl" "$ST/$d.out" 0 >"$ST/$d.report" 2>&1; then
            echo "  FAIL $d — analyzer ACCEPTED a defective log"; fails=$((fails+1))
        else
            printf '  ok   %-28s rejected\n' "$d"
        fi
    done

    # rc mutation: a clean log but a non-zero exit must still fail.
    n=$((n+1))
    if analyze_contract "$ST/clean.jsonl" "$ST/clean.out" 1 >"$ST/rc.report" 2>&1; then
        echo "  FAIL nonzero-rc — analyzer ACCEPTED rc=1"; fails=$((fails+1))
    else
        printf '  ok   %-28s rejected\n' "nonzero-rc"
    fi

    if [ "$fails" -eq 0 ]; then
        echo "==> SELF-TEST PASS: clean accepted, $n mutations rejected (gate has teeth)"
        exit 0
    fi
    echo "==> SELF-TEST FAIL: $fails/$n mutations wrongly accepted"
    exit 1
fi

# ── flag inventory mode ──────────────────────────────────────────────────────
# Boots each map headless and reports which team-game flag entities the server
# actually spawned, read from the engine's own log rather than inferred from
# repo files. Informational: exits 0 whenever every map loaded, because "this
# map has no neutral flag" is a map property, not a defect.
# Accept --flag-inventory in ANY argument position, not just $1. Recognising it
# only as the first token meant the natural `<binary> --flag-inventory` left the
# flag unset, fell through to the ZONEID gate, and failed with a message about
# debug builds that had nothing to do with the actual mistake.
FLAG_INVENTORY=0
_zc_args=()
for _a in "$@"; do
    if [ "$_a" = "--flag-inventory" ]; then FLAG_INVENTORY=1; else _zc_args+=( "$_a" ); fi
done
set -- "${_zc_args[@]:-}"

# ── repeat mode ──────────────────────────────────────────────────────────────
# `--repeat N` runs the whole chain N times, each in a FRESH process with a
# FRESH scratch home, and fails if ANY iteration fails.
#
# WHY REPEATS ARE NOT JUST A LOOP: the defect class here is lifecycle/race, not
# a deterministic assertion. One pass proves nothing about it — the SIGBUS in
# CM_TraceThroughTree on the first frame after the second transition reproduces
# INTERMITTENTLY on the same binary and the same map chain. What varies between
# iterations is the engine's own nondeterminism, not the harness input: each
# process re-seeds bot/game RNG, re-allocates hunk and zone from a fresh
# address space (ASLR), re-bakes the navmesh on a background thread whose join
# point floats against the main thread's map load, and re-runs the async QUIC
# transport bring-up. The map chain, the cvars and the wait counts are held
# IDENTICAL on purpose, so a divergence between iteration k and iteration k+1
# is attributable to engine state lifetime rather than to a changed stimulus.
#
# Each iteration is scored by the same analyze_contract() the single-shot mode
# uses, so a repeat run cannot pass on a weaker gate than a normal run.
REPEAT=1
if [ "${1:-}" = "--repeat" ]; then
    REPEAT="${2:-}"
    case "$REPEAT" in
        ''|*[!0-9]*) echo "usage: $0 --repeat <positive-integer> <binary>"; exit 64 ;;
    esac
    [ "$REPEAT" -ge 1 ] || { echo "usage: $0 --repeat <positive-integer> <binary>"; exit 64; }
    shift 2
fi

# ── product run ──────────────────────────────────────────────────────────────
HEADLESS="${1:-}"
[ -n "$HEADLESS" ] || { echo "usage: $0 /absolute/path/to/wired-headless | --self-test"; exit 64; }
[ -x "$HEADLESS" ] || { echo "SKIP: wired-headless not executable: $HEADLESS"; exit 77; }
HEADLESS="$(cd "$(dirname "$HEADLESS")" && pwd)/$(basename "$HEADLESS")"
WD="$(dirname "$HEADLESS")"

# Pack discovery mirrors the sibling headless gates: pax21 is build-produced,
# pax01 carries the BSPs and comes from the launcher asset pipeline.
PACK=""
for candidate in "$WD" "$WD/../Resources" "$REPO_ROOT/build"; do
    [ -f "$candidate/base/pax21.sw3z" ] && PACK="$(cd "$candidate" && pwd)" && break
done
[ -n "$PACK" ] || { echo "SKIP: current pax21 unavailable"; exit 77; }
# No archive-name check. Which file carries the BSPs is an asset-pipeline
# detail the engine cannot observe — it mounts every sw3z/pk3 it finds as one
# namespace — and asserting `pax01` here made this gate SKIP on installs that
# were perfectly complete. Content reachability is proven where it matters:
# the per-map load check below fails loudly if a map does not open.

# 🔴 Fail-closed build-config gate. A release binary has no ZONEID assertion
# (common.c:409-412), so it CANNOT witness #96 — passing one would manufacture a
# vacuous green. Refuse it loudly instead of skipping.
# `grep -q` exits early and SIGPIPEs `strings`, which `pipefail` would then
# report as a failure of the whole pipeline — count matches instead.
# Not applied to --flag-inventory: entity spawning does not depend on
# USE_ZONE_ID, so that mode runs on a release binary too.
ZONEID_HITS="$(strings "$HEADLESS" 2>/dev/null | grep -c "freed a pointer without ZONEID" || true)"
if [ "$FLAG_INVENTORY" = 0 ] && [ "${ZONEID_HITS:-0}" -lt 1 ]; then
    echo "FAIL: '$HEADLESS' has no ZONEID assertion — USE_ZONE_ID is _DEBUG-only"
    echo "      (code/qcommon/common.c:409-412; _DEBUG set only for Debug, CMakeLists.txt:2835)."
    echo "      A release build cannot witness #96. Point this gate at a DEBUG wired-headless."
    exit 1
fi

ROOT="$(mktemp -d -t hmtz-XXXXXX 2>/dev/null || mktemp -d)"
HOME_DIR="$ROOT/home/q3now-preview"
cleanup(){ local status=$?; trap - EXIT INT TERM; [ "${WIRED_KEEP_ARTIFACTS:-0}" = 1 ] || rm -rf "$ROOT"; exit "$status"; }
trap cleanup EXIT; trap 'exit 130' INT; trap 'exit 143' TERM

# No scratch home, and nothing copied. The engine's own defaults already
# resolve BOTH content layers — install Resources plus ~/wired/<app>/base — so a
# run that overrides nothing mounts everything (measured: 2228 files in 2 paks,
# arena1 loads). Redirecting fs_homepath was what broke that: the content layer
# resolves relative to it, so a scratch home silently lost the downloaded pak,
# and the copying below existed only to put it back — which in turn forced this
# script to know which archive held the maps.
#
# The one thing that genuinely needed isolating is the LOG, so each map gets its
# own artefact instead of appending to the user's. That is now a narrow knob:
# log_file_path names an absolute destination and leaves the filesystem alone.
# A headless run does not write config.cfg (measured: hash unchanged), so there
# is nothing else here worth isolating.
mkdir -p "$HOME_DIR"

if [ "$FLAG_INVENTORY" = 1 ]; then
    shift || true
    INV_MAPS="${*:-arena1 arenat2 arenat4 arenat7 arena7 e1m1}"
    echo "==> flag-entity inventory: $INV_MAPS"
    echo "    binary : $HEADLESS"
    echo "    content: engine defaults (install + $HOME/wired/<app>)"
    printf '\n    %-12s %-6s %-6s %-8s %s\n' map red blue NEUTRAL 1FCTF-playable
    printf '    %s\n' "------------------------------------------------------------"
    INV_RC=0
    # +wait 900, not 400: each map gets a FRESH scratch home, so every navmesh
    # bake here is a COLD one, and the centre-of-map fallback flag cannot be
    # placed until the mesh is ready. A wait shorter than the bake measures the
    # pre-fallback moment and reports a playable map as unplayable — e1m1 (a Q1
    # BSP, ~2030 reachable polys) does not settle within 400 frames. 900 costs
    # ~45s per map; raise it if a slower machine starts reporting NEUTRAL=no on a
    # map whose log shows the bake still running at +quit.
    for m in $INV_MAPS; do
        MLOG="$HOME_DIR/qconsole.jsonl"
        rm -f "$MLOG"
        ( cd "$WD" && exec "$HEADLESS" \
            +set log_file_path "$MLOG" +set com_automated 1 \
            +set com_noHardReboot 1 +set sv_pure 0 \
            +set log_severity DEBUG +set log_file_severity DEBUG \
            +set log_file_mode overwrite_synced \
            +set g_gametype 6 +map "$m" +wait 900 +quit ) >"$ROOT/stdout.$m" 2>&1 || true
        BLOB="$( { cat "$MLOG"; cat "$ROOT/stdout.$m"; } 2>/dev/null )"
        # Did the map load at all? A missing BSP must not read as "no flags".
        # 🔴 grep -c, not grep -q: `-q` exits at the first match and SIGPIPEs the
        # upstream printf, which `set -o pipefail` (line 57) then reports as a
        # failed pipeline — so a map that DID load read as "did not load". Same
        # hazard, and the same fix, as the ZONEID probe above.
        # 🔴 A map that FAILED to load must not satisfy this. `$m\.bsp` alone
        # does: the engine's own failure line is `Can't find map maps/<m>.bsp`,
        # which contains that exact substring. Combined with the absence-based
        # flag detection below — a map that never loads emits no warnings, so
        # every flag reads as present — that turned an unreachable-content run
        # into a full green table. Reject the failure line explicitly, and
        # require positive evidence the server actually came up.
        if [ "$( printf '%s' "$BLOB" | grep -ciE "Can't find map .*$m\.bsp" || true )" -ge 1 ]; then
            printf '    %-12s %-6s %-6s %-8s %s\n' "$m" "-" "-" "-" "MAP NOT FOUND"
            INV_RC=1
            continue
        fi
        if [ "$( printf '%s' "$BLOB" | grep -ciE "Server: *$m|spawning server" || true )" -lt 1 ]; then
            printf '    %-12s %s\n' "$m" "(map did not load — inconclusive)"
            INV_RC=1
            continue
        fi
        # 🔴 POLARITY: the entity name appears in the game's ABSENCE warning too
        # ("WARNING: No team_CTF_neutralflag in map", g_items.c:707-728). Counting
        # bare occurrences reads a missing flag as a present one — an earlier
        # revision of this mode did exactly that and reported every map as
        # 1FCTF-playable. Presence is therefore the ABSENCE of the warning, on a
        # map that loaded.
        #
        # 🔴 Match ONLY the item-level warning (g_items.c G_CheckTeamItems, "No
        # team_CTF_neutralflag in map"). The botlib warning ("One Flag CTF without
        # Neutral Flag", ai_dmq3.c) answers a DIFFERENT question — whether the bot
        # could resolve a goal — and is emitted when the navmesh is still baking
        # even though the flag ENTITY exists. Treating the two as one signal made
        # arenam3, the one shipped map with an authored neutral flag, report "no
        # flags at all" on a cold navmesh cache.
        # grep -c for the same SIGPIPE reason as the load check above.
        absent() { [ "$( printf '%s' "$BLOB" | grep -ciE "No $1 in map" || true )" -ge 1 ]; }
        absent team_CTF_redflag     && R="no" || R="yes"
        absent team_CTF_blueflag    && B="no" || B="yes"
        absent team_CTF_neutralflag && N="no" || N="yes"

        # The AUTHORED-flag warning above is a point-in-time signal, emitted during
        # G_InitGame. 1FCTF playability is an END-STATE property: when the map has
        # no authored neutral flag the engine spawns one at the centre of the
        # walkable area. On a WARM navmesh cache that happens before the warning is
        # even reached (so N is already "yes"), but on a COLD cache the mesh is
        # still baking at G_InitGame and the flag lands a few seconds later — the
        # warning is honest at the time it prints, and would nonetheless read as
        # "unplayable" for a round that is in fact playable.
        #
        # So a fallback spawn counts as a neutral flag, and is reported distinctly
        # from an authored one: they are equally playable but not the same fact,
        # and collapsing them would hide a map silently losing its authored flag.
        FB="no"
        if [ "$( printf '%s' "$BLOB" | grep -ciE "spawned fallback at" || true )" -ge 1 ]; then FB="yes"; fi
        if [ "$N" = no ] && [ "$FB" = yes ]; then N="via-fb"; fi

        if   [ "$N" = yes ];    then PLAY="yes (authored)"
        elif [ "$N" = via-fb ]; then PLAY="yes (centre fallback)"
        elif [ "$R" = yes ] || [ "$B" = yes ]; then PLAY="NO (CTF map, needs centre fallback)"
        else PLAY="NO (no flags at all)"; fi
        # A map that reports no neutral flag AND no fallback is the regression this
        # mode exists to catch, now that the fallback is supposed to cover it.
        if [ "$N" = no ]; then INV_RC=1; fi
        printf '    %-12s %-6s %-6s %-8s %s\n' "$m" "$R" "$B" "$N" "$PLAY"
    done
    echo
    echo "    NEUTRAL=via-fb means the map had no authored neutral flag and the"
    echo "    engine placed one at the centre of the walkable area. A NEUTRAL=no"
    echo "    row means neither happened — 1FCTF is unplayable there."
    exit "$INV_RC"
fi

echo "==> headless map-transition zonecheck (#96): $MAP_CHAIN"
echo "    binary : $HEADLESS"
echo "    content: engine defaults (install + $HOME/wired/<app>)"
echo "    repeats: $REPEAT"

# com_noHardReboot 1 keeps the watchdog from RELAUNCHING on a crash and masking
# it (code/unix/unix_main.c:1087-1098). +wait, not +waitForMap (client-only).
LAUNCH_ARGS=(
    +set fs_homepath "$HOME_DIR"
    +set com_automated 1
    +set com_noHardReboot 1
    +set sv_pure 0
    +set log_severity DEBUG
    +set log_file_severity DEBUG
    +set log_file_mode overwrite_synced
)
for m in $MAP_CHAIN; do LAUNCH_ARGS+=( +map "$m" +wait 250 ); done
LAUNCH_ARGS+=( +quit )

LOG="$HOME_DIR/qconsole.jsonl"
PASSES=0
FAILED_ITERS=""

i=1
while [ "$i" -le "$REPEAT" ]; do
    # Fresh process AND fresh log. `log_file_mode overwrite_synced` already
    # truncates, but removing the file first makes "the engine never wrote a
    # log this iteration" distinguishable from "iteration k-1's log survived" —
    # analyze_contract's first check treats an absent log as a failure, so a
    # process that dies before opening its sink cannot inherit a stale PASS.
    rm -f "$LOG"
    ITER_OUT="$ROOT/stdout.$i"

    RC=0
    ( cd "$WD" && exec "$HEADLESS" "${LAUNCH_ARGS[@]}" ) >"$ITER_OUT" 2>&1 || RC=$?

    if [ "$REPEAT" -gt 1 ]; then printf '  -- iteration %d/%d --\n' "$i" "$REPEAT"; fi
    if analyze_contract "$LOG" "$ITER_OUT" "$RC"; then
        PASSES=$((PASSES + 1))
    else
        FAILED_ITERS="$FAILED_ITERS $i"
        # Keep the evidence for the failing iteration; a later green iteration
        # must not be able to overwrite the artefact that proves the defect.
        cp "$LOG" "$ROOT/qconsole.fail.$i.jsonl" 2>/dev/null || true
    fi
    i=$((i + 1))
done

if [ -n "$FAILED_ITERS" ]; then
    echo "==> FAIL headless map-transition zonecheck: $PASSES/$REPEAT iterations passed"
    echo "    failing iterations:$FAILED_ITERS"
    echo "    (re-run with WIRED_KEEP_ARTIFACTS=1 to retain $ROOT)"
    exit 1
fi
echo "==> PASS headless map-transition zonecheck: $PASSES/$REPEAT iterations clean (#96 not reproduced on this build)"
exit 0
