#!/usr/bin/env bash
# check_audio_callback.sh
#
# Static analysis: ensure every function that runs on the miniaudio audio
# thread is lock-free. Those functions must NOT use mutexes, allocators,
# logging, or cvar mutation. Math, memcpy/memset, and atomic operations are
# allowed.
#
# Scanned functions: the raw-device callback (S_MiniaudioCallback) and the
# ma_engine output tap (S_EngineProcess). Both are pulled by miniaudio on its
# internal audio thread, so both are held to the same rule.
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

# The audio-thread functions to scan. Each name is matched at the start of its
# definition line (any return type). Add new audio-thread callbacks here so the
# lock-free rule keeps its teeth.
#
# S_EngineProcess is ma_engine's post-mix output tap, invoked on ma_engine's
# internal audio thread when the engine owns a device. (In read-driven capture
# mode there is no audio thread; the tap runs on the calling main thread.)
AUDIO_THREAD_FUNCS=(
    "S_EngineProcess"
)

# Extract a single function body from $SOURCE by name using awk.
# Finds the definition line (name followed by '('), then tracks brace depth
# until the matching closing '}'.
extract_body() {
    local func="$1"
    awk -v fn="$func" '
        $0 ~ ("(^|[^_[:alnum:]])" fn "[[:space:]]*\\(") && !in_func { in_func=1 }
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
    ' "$SOURCE"
}

# Strip /* ... */ block comments and // line comments from a body so that
# documentation containing words like "lock-free" or "no mutex" does not
# trigger false positives.
strip_comments() {
    awk '
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
'
}

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
    "Com_Log"
    "Com_Terminate"
    "(^|[^_[:alnum:]])printf\\("
    "(^|[^_[:alnum:]])fprintf\\("
    "Cvar_Set"
    "Cvar_SetValue"
    "Cvar_Get"
)

TOTAL_VIOLATIONS=0

for func in "${AUDIO_THREAD_FUNCS[@]}"; do
    BODY=$(extract_body "$func")
    if [ -z "$BODY" ]; then
        echo "ERROR: could not locate $func in $SOURCE"
        exit 1
    fi

    BODY_NO_COMMENTS=$(printf '%s\n' "$BODY" | strip_comments)

    func_violations=0
    for token in "${FORBIDDEN_TOKENS[@]}"; do
        if printf '%s\n' "$BODY_NO_COMMENTS" | grep -qE -- "$token"; then
            match=$(printf '%s\n' "$BODY_NO_COMMENTS" | grep -nE -- "$token" | head -3)
            func_violations=$((func_violations + 1))
            TOTAL_VIOLATIONS=$((TOTAL_VIOLATIONS + 1))
            echo "VIOLATION: '$token' found in $func body"
            printf '%s\n' "$match" | sed 's/^/    /'
        fi
    done

    if [ "$func_violations" -eq 0 ]; then
        echo "PASS: $func is lock-free (zero forbidden tokens)."
    fi
done

if [ "$TOTAL_VIOLATIONS" -gt 0 ]; then
    echo ""
    echo "FAIL: $TOTAL_VIOLATIONS forbidden token(s) found across audio-thread functions."
    echo "These functions run on the miniaudio audio thread and must remain lock-free."
    echo "See code/client/snd_miniaudio.c header comment for the audio thread rules."
    exit 1
fi

echo "PASS: all audio-thread functions are lock-free."
exit 0
