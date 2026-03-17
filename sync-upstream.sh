#!/usr/bin/env bash
# sync-upstream.sh — merge latest ioquake3 changes into q3now
#
# Run this quarterly (or whenever upstream has relevant fixes):
#
#   bash sync-upstream.sh
#
# What it does:
#   1. Fetches latest upstream/main
#   2. Creates a sync branch (sync/upstream-YYYY-MM-DD)
#   3. Merges upstream/main into the branch
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
#  code/game/*.c, code/cgame/*.c
#    → Keep custom logic. Accept upstream's surrounding improvements.
#
#  CMakeLists.txt / cmake/*.cmake
#    → Accept upstream version. Re-apply any q3now cmake additions
#      (bg_promode.c in basegame.cmake) if they were overwritten.
#
#  README.md
#    → Accept upstream version. The q3now section is at the very top
#      (before the ioquake3 ASCII art) so it rarely conflicts.
#
#  code/game/bg_public.h, code/game/g_local.h
#    → Merge carefully: keep custom enums/structs AND upstream fixes.
#
# After a successful merge, run:
#   make dev      — verify it still compiles
#   make help     — confirm Makefile targets still work
# ──────────────────────────────────────────────────────────────────────────────

set -e

UPSTREAM_REMOTE="upstream"
UPSTREAM_BRANCH="main"
SYNC_DATE=$(date +%Y-%m-%d)
SYNC_BRANCH="sync/upstream-${SYNC_DATE}"

# ── Preflight checks ──────────────────────────────────────────────────────────

if ! git remote get-url "$UPSTREAM_REMOTE" > /dev/null 2>&1; then
    echo "ERROR: remote '$UPSTREAM_REMOTE' not found."
    echo "Add it with:"
    echo "  git remote add upstream https://github.com/ioquake/ioq3.git"
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
        -m "Merge upstream ioquake3 @ ${UPSTREAM_SHA:0:8} (${SYNC_DATE})"; then
    echo ""
    echo "==> Merge succeeded with no conflicts."
    echo ""
    echo "Next steps:"
    echo "  1. make dev                        # verify build"
    echo "  2. git push origin ${SYNC_BRANCH}"
    echo "  3. Open a PR: ${SYNC_BRANCH} → master"
else
    echo ""
    echo "==> Merge stopped due to conflicts. Resolve them, then:"
    echo "  git add <resolved files>"
    echo "  git merge --continue"
    echo "  make dev"
    echo "  git push origin ${SYNC_BRANCH}"
    echo "  Open a PR: ${SYNC_BRANCH} → master"
    echo ""
    echo "Conflicted files:"
    git diff --name-only --diff-filter=U
    exit 1
fi
