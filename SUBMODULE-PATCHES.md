# Vendored-submodule patches

Some `src/libs/*` submodules need changes that are not upstream. Wired keeps
those changes as **patch files in this repository**, applied to the submodule
working tree at CMake configure time. This document explains why that rule
exists, how the mechanism works, and how to add a patch correctly.

---

## 1. Why: a superproject records a submodule's commit, never its content

This is the single trap that motivates everything below.

When `src/libs/recastnavigation` is a submodule, the superproject stores exactly
one thing about it: the commit SHA to check out. It does **not** store the files.
So if you edit a file inside a submodule:

```
$ git status
	modified:   src/libs/recastnavigation (modified content)
```

That line is not a staged change waiting for you. `git add -A` will not pick the
content up. `git commit` will not carry it. You can commit, push, and see green
CI on other branches, and the edit still exists on exactly one machine.

**This has already cost this project once.** On 2026-08-10 the WiredIntel nav arc
was committed and pushed (`ccf5237b`), but the Detour extensions it depends on —
five new `dtNavMesh` methods called from `code/qcommon/nav/nav_impl.cpp` — lived
only in the submodule working tree of the machine they were written on. A fresh
clone of that commit does not compile:

```
nav_impl.cpp:950: error: no member named 'reconnectUnlinkedOffMeshStarts' in 'dtNavMesh'
nav_impl.cpp:966: error: no member named 'reconnectSplitFloorSeams' in 'dtNavMesh'
nav_impl.cpp:1115: error: no member named 'isPreferredWalkInFloor' in 'dtNavMesh'
nav_impl.cpp:2174: error: no member named 'getReachablePolyCount' in 'dtNavMesh'
nav_impl.cpp:3926: error: no member named 'finalizeOmcEndpoints' in 'dtNavMesh'
```

> **Rule.** Anything a submodule needs beyond its pinned commit MUST live in
> `patches/<name>/` in this repository. If you find yourself editing a file under
> `src/libs/`, you are creating a patch — finish the job and commit it here.

---

## 2. How it works

`CMakeLists.txt` defines `WIRED_APPLY_SUBMODULE_PATCHES(<label> <submodule_dir> <patch_dir>)`
and calls it once per patched submodule:

| Submodule | Patch dir | Applied on |
|---|---|---|
| `src/libs/picoquic` | `patches/picoquic-mingw/` | Windows only (`IF(WIN32)`) |
| `src/libs/recastnavigation` | `patches/recastnavigation/` | all platforms |
| `src/libs/sentry-native` | `patches/sentry-native/` | when `USE_SENTRY_CRASH=ON` (the default) |

`sentry-native` carries one patch, `01-chain-previous-crash-handler.patch`.
sentry's signal handler saves the previously installed handler in
`g_previous_handlers` and then terminates via `raise()` without ever calling
it — so enabling sentry silently disabled ours, and a crash produced only a
minidump: no JSON crash report, no playtest timeline, and nothing in the log
saying why. Measured on macOS, Linux and Windows alike.

sentry has **two** handler paths with the same defect, one per platform family,
and both are patched:

- POSIX — `g_previous_handlers` is saved by `sigaction` and never called; the
  handler ends at `raise()`, which with `SIG_DFL` installed terminates the
  process, so the chain call has to come immediately before it.
- Windows — `g_previous_filter` is saved by `SetUnhandledExceptionFilter` and
  only touched again at shutdown. Returning `EXCEPTION_CONTINUE_SEARCH` is *not*
  equivalent to calling it: that API holds a single slot rather than a chain, so
  whoever installs last replaces the previous filter outright and
  `CONTINUE_SEARCH` proceeds to the system default instead. The displaced filter
  runs only if called explicitly, and its verdict is returned to the caller
  because a filter answering `EXCEPTION_EXECUTE_HANDLER` intends to shut the
  process down its own way.

In both, the double-fault early exit deliberately does *not* chain — a handler
is already running there. Together they turn enabling sentry into *adding* a
minidump rather than trading the other two artefacts away for one.

The same patch carries a second, related fix in `sentry_crash_daemon.c`.
`is_parent_alive()` decides whether the daemon should stop waiting, and its two
platform branches disagreed about what a failed query means. POSIX reports dead
only for `ESRCH` and treats every other error — `EPERM` above all — as still
alive; the Windows branch collapsed "the process is gone" and "I could not ask"
into one `return false`, so any failure to open the handle ended the daemon.

That asymmetry destroys evidence rather than merely shortening a process: the
daemon breaks out of its wait loop, finalizes the session, and a crash arriving
afterwards finds the run slot closed, so no minidump is ever written. Observed
on Windows — the session was stamped `"status":"crashed"` while the engine was
still healthy, ten seconds before the real fault. The Windows branch now mirrors
the POSIX rule: only `ERROR_INVALID_PARAMETER` (no process carries that id, the
counterpart of `ESRCH`) counts as dead, and a `GetExitCodeProcess` that fails
outright is treated as "unknown", not as "exited".

Upstreamable: nothing in it is specific to this engine.

Behaviour:

- Patches in a directory are applied in **sorted filename order** — hence the
  `01-`, `02-`, … prefixes.
- **Idempotent.** Each patch is first tested with `git apply --check --reverse`.
  That exits 0 only when the patch is already cleanly applied, in which case it
  is skipped. Re-running `cmake` is therefore safe.
- **Whitespace-tolerant.** Both the check and the apply pass
  `--ignore-whitespace`, so CRLF/LF drift in a working tree does not break hunk
  context matching.
- **Fails loudly.** A patch that will not apply is a `FATAL_ERROR` with recovery
  instructions, not a warning. A silently unpatched submodule produces confusing
  compile errors far from the cause.
- **No-op when absent.** An empty or missing patch directory is skipped. A patch
  directory that exists while its submodule is not checked out is a fatal error
  telling you to run `git submodule update --init --recursive`.

Nothing needs to be run by hand. `make`, `make configure`, and a bare
`cmake -S . -B build/release` all apply patches as part of configure.

---

## 3. Adding a patch

Work in the submodule as usual, then capture the result.

```bash
cd src/libs/<submodule>
# ... make your edits ...

# Include new files too: -N marks untracked files "intent to add" so they show
# up in `git diff`. It touches only the index; the working tree is untouched.
git add -N .
git diff --binary > ../../../patches/<submodule>/NN-short-description.patch
git reset            # drop the intent-to-add marks again

cd -
git add patches/<submodule>/NN-short-description.patch
```

If the submodule is not yet registered in `CMakeLists.txt`, add the call next to
the code that consumes it:

```cmake
WIRED_APPLY_SUBMODULE_PATCHES(<label> src/libs/<submodule> patches/<submodule>)
```

Place the call **before** the `FILE(GLOB ...)` that collects the submodule's
sources, so the patched files are the ones that get compiled.

### Verify before committing

```bash
git -C src/libs/<submodule> reset --hard      # back to the pinned commit
rm -rf build/release
make configure                                 # must print "applied <label> patch: ..."
make build
```

A patch that only works against your already-modified tree is not a patch.

---

## 4. The CRLF trap when generating patches

`git diff` is **not** a reliable patch source on a working tree whose files were
rewritten with CRLF line endings by a non-git-aware tool (some editors, some
scripts). Git hides line-ending conversion that *it* performed; it does not hide
a rewrite someone else performed. The result is a diff where the real change is
buried in whole-file noise:

```
 Detour/Source/DetourNavMesh.cpp | 4059 ++++++++++++++++++--------------
 3 files changed, 2543 insertions(+), 1599 deletions(-)
```

Roughly 1600 of those lines are line-ending churn; the real work is the ~944-line
net difference.

**Diagnosing it.** Compare the two stats:

```bash
git -C src/libs/<submodule> diff --stat                    # includes EOL noise
git -C src/libs/<submodule> diff --ignore-cr-at-eol --stat # real delta only
```

A file reported as `N insertions(+), N deletions(-)` where `N` equals the file's
total line count has been rewritten wholesale. Note that this does **not** by
itself prove there is no real change — an edit that preserves the line count
hides perfectly inside such a rewrite. Only the `--ignore-cr-at-eol` stat
settles it.

**Fixing it.** Normalise before diffing, on a copy — never on the only copy of
unbacked-up work:

```bash
cp -r src/libs/<submodule> /tmp/<submodule>-backup     # back up first
cd src/libs/<submodule>
for f in $(git diff --name-only); do
	tr -d '\r' < "$f" > "$f.lf" && mv "$f.lf" "$f"
done
git diff --binary > ../../../patches/<submodule>/NN-....patch
```

**Preventing it.** Submodules do not inherit the superproject's
`.gitattributes`. This repo's root `.gitattributes` sets `* text=auto eol=lf`,
but that rule stops at the submodule boundary — each submodule follows its own
(or none). On Windows, configure your editor to preserve LF inside `src/libs/`.

---

## 5. The third mechanism: in-place rewrite at configure time

Not every submodule change is a patch file. `libjpeg-turbo` is handled a third
way, and it is deliberate — do **not** "fix" it by moving it into `patches/`.

Upstream libjpeg-turbo refuses `add_subdirectory()` with a `FATAL_ERROR` guard.
`CMakeLists.txt` neutralises that one line by rewriting the file at configure
time (`FILE(READ)` → `STRING(REPLACE)` → `FILE(WRITE)`, with the write skipped
when the replacement changes nothing).

This suits a change that is a **single mechanical substitution against a stable
string**: it carries no patch context, so it survives submodule pointer bumps
that would force a patch rebase.

The trade-off is a silent permanent trace. Because the match target is the
*original* line, a tree that was already rewritten is never rewritten again —
trees first configured before the Wired rebrand still carry
`# q3now: add_subdirectory() guard neutralized` where a fresh tree gets
`# wired: ...`. Both behave identically; they simply never converge. Expect to
see `src/libs/libjpeg-turbo` permanently listed as "modified content" in
`git status`, on every machine. That is normal and must not be committed.

Choosing between the three mechanisms:

| Situation | Mechanism |
|---|---|
| One mechanical substitution against a stable string | in-place `STRING(REPLACE)` at configure time |
| Real code changes, reviewable, must survive a fresh clone | patch file under `patches/` |
| Sustained divergence, hundreds of lines, ongoing maintenance | maintained fork + repointed submodule URL |

## 6. When a patch is the wrong answer

Prefer upstreaming when the change is generally useful — a patch is maintenance
debt that has to be rebased every time the submodule pointer moves. Use a patch
when the change is Wired-specific (the Detour extensions), platform-specific
(the MinGW picoquic fixes), or when upstream has declined it.

If a patch set grows past a few hundred lines of genuinely divergent behaviour,
reconsider: a maintained fork with the submodule URL repointed may be cheaper
than rebasing a large patch. That is a deliberate call to make explicitly, not
something to drift into.
