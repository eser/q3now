#!/usr/bin/env bash
set -euo pipefail
unset GREP_OPTIONS GREP_COLORS || true
export LC_ALL=C

SELF_TEST=0
if [[ ${1:-} == --self-test ]]; then
  SELF_TEST=1
  shift
fi

ROOT=${1:-$(cd "$(dirname "$0")/.." && pwd)}
MAKEFILE_PATH=${MAKEFILE_PATH:-$ROOT/Makefile}
MAKE_BIN=${MAKE_BIN:-make}
PROBE_VERSION=q0-provenance-probe
PROBE_SOURCE=source-provenance-probe
PROBE_DATE=2030-01-02

fail() {
  echo "FAIL release provenance contract: $*" >&2
  exit 1
}

require_exact_count() {
  local body=$1 needle=$2 expected=$3 label=$4 count
  count=$(grep -Fxc "$needle" <<<"$body" || true)
  [[ $count == "$expected" ]] || fail "$label: expected $expected exact row(s), got $count"
}

run_case() {
  local uname_s=$1 uname_m=$2 bundle_target=$3 artifact=$4 summary_label=$5
  local output recursive_row pack_recursive_row artifact_row summary_row description_rows rsync_rows

  output=$(
    "$MAKE_BIN" --no-print-directory --always-make -n -C "$ROOT" -f "$MAKEFILE_PATH" release \
      UNAME_S="$uname_s" UNAME_M="$uname_m" JOBS=1 \
      CHANNEL=preview \
      VERSION="$PROBE_VERSION" SOURCE_VERSION="$PROBE_SOURCE" \
      BUILD_DATE_ISO="$PROBE_DATE" 2>&1
  ) || fail "$uname_s dry-run failed"

  recursive_row="$bundle_target VERSION=\"$PROBE_VERSION\" SOURCE_VERSION=\"$PROBE_SOURCE\" BUILD_DATE_ISO=\"$PROBE_DATE\""
  [[ $(grep -Fc "$recursive_row" <<<"$output" || true) == 1 ]] \
    || fail "$uname_s recursive bundle did not receive the exact frozen tuple"

  pack_recursive_row="build/base/pax21.sw3z build/base/pax21-client.sw3z build/base/pax21-server.sw3z VERSION=\"$PROBE_VERSION\" SOURCE_VERSION=\"$PROBE_SOURCE\" BUILD_DATE_ISO=\"$PROBE_DATE\""
  [[ $(grep -Fc "$pack_recursive_row" <<<"$output" || true) -ge 1 ]] \
    || fail "$uname_s recursive pack did not receive the exact frozen tuple"

  description_rows=$(grep -F 'description.txt' <<<"$output" || true)
  [[ -n $description_rows ]] || fail "$uname_s emitted no pack description recipe"
  if grep -Fv "echo \"q3now-preview $PROBE_SOURCE ($PROBE_DATE)\" > build/pak-staging/shared/description.txt" \
      <<<"$description_rows" >/dev/null; then
    fail "$uname_s pack description recomputed or changed source provenance"
  fi

  artifact_row="test -f \"$artifact\" || { echo \"ERROR: expected release artifact missing: $artifact\"; exit 1; }"
  require_exact_count "$output" "$artifact_row" 1 "$uname_s artifact postcondition"

  summary_row="echo \"  │  $summary_label: $artifact\""
  require_exact_count "$output" "$summary_row" 1 "$uname_s final summary"

  if [[ $uname_s == Darwin ]]; then
    rsync_rows=$(grep -F 'rsync -a --checksum --delete' <<<"$output" || true)
    [[ -n $rsync_rows ]] || fail "Darwin emitted no app-skeleton rsync recipe"
    if grep -Fv -- "--filter='H /Contents/MacOS/base/' --filter='H /Contents/MacOS/*.jsonl'" \
        <<<"$rsync_rows" >/dev/null; then
      fail "Darwin app staging lost the sender-only runtime-artifact filters"
    fi
    [[ $(grep -Fc 'test ! -e ' <<<"$output" || true) -ge 1 ]] \
      || fail "Darwin app staging lost its runtime-base fail-closed postcondition"
    [[ $(grep -Fc 'runtime JSONL diagnostic leaked into release MacOS staging' <<<"$output" || true) -ge 1 ]] \
      || fail "Darwin app staging lost its JSONL fail-closed postcondition"
  fi
}

run_contract() {
  run_case Darwin x86_64 bundle-dmg \
    "build/q3now-preview-$PROBE_VERSION-x86_64.dmg" DMG
  run_case Linux x86_64 bundle-tar \
    "build/q3now-preview-$PROBE_VERSION-linux-x86_64.tar.gz" TAR
  run_case MINGW64_NT-10.0 x86_64 bundle-zip \
    "build/q3now-preview-$PROBE_VERSION-windows-x86_64.zip" ZIP
}

run_contract

if [[ $SELF_TEST == 1 ]]; then
  mutation_root=$(mktemp -d "${TMPDIR:-/tmp}/release-provenance.XXXXXX")
  trap 'rm -rf "$mutation_root"' EXIT

  expect_reject() {
    local label=$1 expression=$2 mutated
    mutated=$mutation_root/$label.mk
    sed "$expression" "$ROOT/Makefile" >"$mutated"
    if (MAKEFILE_PATH=$mutated run_contract) >/dev/null 2>&1; then
      fail "self-test mutation passed: $label"
    fi
  }

  expect_reject missing-source-forward 's/ SOURCE_VERSION="$(SOURCE_VERSION)"//g'
  expect_reject missing-pack-source-forward '/$(MAKE) $(PAK_OUTPUTS)/s/ SOURCE_VERSION="$(SOURCE_VERSION)"//'
  expect_reject stale-pack-stamp 's/echo "$(APP_NAME) $(SOURCE_VERSION) ($(BUILD_DATE_ISO))"/echo "$(APP_NAME) stale-source (stale-date)"/'
  expect_reject missing-artifact-postcondition '/expected release artifact missing/d'
  expect_reject missing-macos-base-filter "/--filter='H \/Contents\/MacOS\/base\/'/d"

  echo "PASS release provenance self-test: clean +5 mutations"
  exit 0
fi

echo "PASS release provenance contract: frozen tuple, exact artifacts, clean macOS staging"
