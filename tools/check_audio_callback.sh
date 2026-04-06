#!/usr/bin/env bash
# check_audio_callback.sh
#
# Static analysis: ensure S_MiniaudioCallback in snd_miniaudio.c is lock-free.
# The audio callback runs on a separate thread; it must NOT use mutexes,
# allocators, logging, or cvar mutation. Math, memcpy/memset, and atomic
# operations are allowed.
#
# Exit 0 = clean, exit 1 = forbidden token found.
#
# Usage:  tools/check_audio_callback.sh [path/to/snd_miniaudio.c]
# CI hook: add to .github/workflows/*.yml or pre-commit
#

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="${1:-$REPO_ROOT/code/client/snd_miniaudio.c}"

if [ ! -f "$SOURCE" ]; then
    echo "ERROR: $SOURCE not found"
    exit 1
fi

# Extract S_MiniaudioCallback function body using awk.
# Find the function definition line, then track brace depth until closing }.
BODY=$(awk '
    /^static void S_MiniaudioCallback/ { in_func=1 }
    in_func {
        line = $0
        for (i=1; i<=length(line); i++) {
            c = substr(line, i, 1)
            if (c == "{") { depth++; started=1 }
            else if (c == "}") {
                depth--
                if (depth == 0 && started) {
                    print line
                    exit
                }
            }
        }
        print line
    }
' "$SOURCE")

if [ -z "$BODY" ]; then
    echo "ERROR: could not locate S_MiniaudioCallback in $SOURCE"
    exit 1
fi

# Strip /* ... */ block comments and // line comments so that
# documentation containing words like "lock-free" or "no mutex" does not
# trigger false positives.
BODY_NO_COMMENTS=$(printf '%s\n' "$BODY" | awk '
    BEGIN { in_block_comment = 0 }
    {
        line = $0
        result = ""
        i = 1
        n = length(line)
        while (i <= n) {
            two = substr(line, i, 2)
            if (in_block_comment) {
                if (two == "*/") {
                    in_block_comment = 0
                    i += 2
                } else {
                    i++
                }
            } else {
                if (two == "/*") {
                    in_block_comment = 1
                    i += 2
                } else if (two == "//") {
                    break
                } else {
                    result = result substr(line, i, 1)
                    i++
                }
            }
        }
        print result
    }
')

# Forbidden token list. Each entry is an extended regex.
FORBIDDEN_TOKENS=(
    "pthread_mutex"
    "pthread_cond"
    "pthread_rwlock"
    "Sys_Mutex"
    "Sys_LockMutex"
    "Sys_UnlockMutex"
    "EnterCriticalSection"
    "LeaveCriticalSection"
    "WaitForSingleObject"
    "WaitForMultipleObjects"
    "sem_wait"
    "sem_post"
    "MA_MUTEX"
    "ma_mutex_lock"
    "ma_mutex_unlock"
    "malloc\\("
    "calloc\\("
    "realloc\\("
    "(^|[^_[:alnum:]])free\\("
    "Z_Malloc"
    "Z_TagMalloc"
    "Hunk_Alloc"
    "Hunk_TempAlloc"
    "Com_Printf"
    "Com_DPrintf"
    "Com_Error"
    "(^|[^_[:alnum:]])printf\\("
    "(^|[^_[:alnum:]])fprintf\\("
    "Cvar_Set"
    "Cvar_SetValue"
    "Cvar_Get"
)

VIOLATIONS=()

for token in "${FORBIDDEN_TOKENS[@]}"; do
    if printf '%s\n' "$BODY_NO_COMMENTS" | grep -qE -- "$token"; then
        match=$(printf '%s\n' "$BODY_NO_COMMENTS" | grep -nE -- "$token" | head -3)
        VIOLATIONS+=("$token")
        echo "VIOLATION: '$token' found in S_MiniaudioCallback body"
        printf '%s\n' "$match" | sed 's/^/    /'
    fi
done

if [ ${#VIOLATIONS[@]} -gt 0 ]; then
    echo ""
    echo "FAIL: ${#VIOLATIONS[@]} forbidden token(s) found in S_MiniaudioCallback."
    echo "The audio callback runs on a separate thread and must remain lock-free."
    echo "See code/client/snd_miniaudio.c header comment for the audio thread rules."
    exit 1
fi

echo "PASS: S_MiniaudioCallback is lock-free (zero forbidden tokens)."
exit 0
