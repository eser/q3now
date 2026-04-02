#!/usr/bin/env bash
# sync-upstream.sh — merge latest Quake3e engine changes into q3now
#
# Run this quarterly (or whenever upstream has relevant fixes):
#
#   bash sync-upstream.sh
#
# What it does:
#   1. Fetches latest quake3e/main
#   2. Creates a sync branch (sync/quake3e-YYYY-MM-DD)
#   3. Merges quake3e/main into the branch
#   4. Prints next steps
#
# If there are conflicts:
#   - Resolve them (see CONFLICT GUIDE below)
#   - git add <resolved files>
#   - git merge --continue
#   - Push the branch and open a PR
#
# CONFLICT GUIDE
# ──────────────────────────────────────────────────────────────────────────────
#  CMakeLists.txt
#    → Accept upstream version. Re-apply q3now identity (CNAME, DNAME,
#      MACOSX_BUNDLE_GUI_IDENTIFIER) and vm_powerpc regex fix and
#      game module wiring (BUILD_GAME_LIBRARIES, INCLUDE(basegame)).
#
#  code/qcommon/**, code/client/**, code/server/**
#    → Accept upstream (engine-only, no q3now changes here).
#
#  code/game/bg_public.h
#    → Keep q3now gameplay defines. Reapply include guard if removed.
#      Keep PLAYER_WIDTH etc — game code (bg_pmove.c) needs them.
#
#  code/botlib/be_ai_goal.c
#    → Accept upstream static/const improvements. Preserve q3now's
#      g_singlePlayer logic and GT_KINGOFTHEHILL/GT_LASTMANSTANDING.
#
#  code/game/*.c, code/cgame/*.c
#    → These are game-only — upstream (Quake3e) will never touch them.
#      No conflicts expected.
#
# After a successful merge, run:
#   make              — verify it still builds (engine + QVMs)
#   make check        — verify QVM and dylib outputs
# ──────────────────────────────────────────────────────────────────────────────

set -e

UPSTREAM_REMOTE="quake3e"
UPSTREAM_BRANCH="main"
SYNC_DATE=$(date +%Y-%m-%d)
SYNC_BRANCH="sync/quake3e-${SYNC_DATE}"

# ── Preflight checks ──────────────────────────────────────────────────────────

if ! git remote get-url "$UPSTREAM_REMOTE" > /dev/null 2>&1; then
    echo "ERROR: remote '$UPSTREAM_REMOTE' not found."
    echo "Add it with:"
    echo "  git remote add quake3e https://github.com/ec-/Quake3e.git"
    exit 1
fi

if [ -n "$(git status --porcelain)" ]; then
    echo "ERROR: working tree is not clean. Commit or stash changes first."
    exit 1
fi

# ── Fetch + branch ────────────────────────────────────────────────────────────

echo "==> Fetching ${UPSTREAM_REMOTE}/${UPSTREAM_BRANCH}..."
git fetch "$UPSTREAM_REMOTE"

UPSTREAM_SHA=$(git rev-parse "${UPSTREAM_REMOTE}/${UPSTREAM_BRANCH}")
echo "    upstream HEAD: ${UPSTREAM_SHA}"

COMMITS_BEHIND=$(git rev-list HEAD..${UPSTREAM_REMOTE}/${UPSTREAM_BRANCH} --count)
if [ "$COMMITS_BEHIND" -eq 0 ]; then
    echo "==> Already up to date with upstream. Nothing to do."
    exit 0
fi

echo "==> ${COMMITS_BEHIND} new upstream commit(s) to merge."
echo ""
echo "Recent upstream commits:"
git log HEAD..${UPSTREAM_REMOTE}/${UPSTREAM_BRANCH} --oneline | head -20
echo ""

echo "==> Creating branch ${SYNC_BRANCH}..."
git switch -c "$SYNC_BRANCH"

# ── Merge ─────────────────────────────────────────────────────────────────────

echo "==> Merging ${UPSTREAM_REMOTE}/${UPSTREAM_BRANCH}..."
if git merge "${UPSTREAM_REMOTE}/${UPSTREAM_BRANCH}" \
        --no-edit \
        -m "Merge Quake3e @ ${UPSTREAM_SHA:0:8} (${SYNC_DATE})"; then
    echo ""
    echo "==> Merge succeeded with no conflicts."
    echo ""
    echo "Next steps:"
    echo "  1. make                            # verify build"
    echo "  2. make check                      # verify QVMs and dylibs"
    echo "  3. git push origin ${SYNC_BRANCH}"
    echo "  4. Open a PR: ${SYNC_BRANCH} → main"
else
    echo ""
    echo "==> Merge stopped due to conflicts. Resolve them, then:"
    echo "  git add <resolved files>"
    echo "  git merge --continue"
    echo "  make"
    echo "  make check"
    echo "  git push origin ${SYNC_BRANCH}"
    echo "  Open a PR: ${SYNC_BRANCH} → main"
    echo ""
    echo "Conflicted files:"
    git diff --name-only --diff-filter=U
    exit 1
fi
