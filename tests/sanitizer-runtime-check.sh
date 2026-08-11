#!/usr/bin/env bash
set -u

probe=${1:-}
if [[ -z "$probe" || ! -x "$probe" ]]; then
  echo "sanitizer runtime check: missing probe executable" >&2
  exit 2
fi

run_root=$(mktemp -d "${TMPDIR:-/tmp}/wired-sanitizer-runtime.XXXXXX") || exit 2
trap 'rm -r "$run_root"' EXIT

run_probe() {
  local mode=$1
  local pattern=$2
  local log="$run_root/$mode.log"
  local rc

  set +e
  "$probe" "$mode" >"$log" 2>&1
  rc=$?
  set -e

  if [[ $rc -eq 0 ]]; then
    echo "sanitizer runtime check: $mode fault exited zero" >&2
    return 1
  fi
  if ! grep -Eq "$pattern" "$log"; then
    echo "sanitizer runtime check: $mode fault lacked sanitizer diagnostic" >&2
    return 1
  fi
}

set -e
run_probe address 'AddressSanitizer.*heap-use-after-free'
run_probe undefined 'runtime error: signed integer overflow'
echo "sanitizer runtime contract: ASan and UBSan fail-fast probes PASS"
