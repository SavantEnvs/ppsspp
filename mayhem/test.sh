#!/usr/bin/env bash
#
# mayhem/test.sh — runs the known-answer selftest that mayhem/build.sh built (mayhem/support/selftest.cpp,
# normal flags, dynamically linked). Each check parses/evaluates a fixed input and asserts the computed
# value: expression results, ini/json fields, a PARAM.SFO round trip, PNG and ZIM pixels, the Atrac3 decoder.
# Counts come from the selftest's own PASS/FAIL lines, so a neutered binary (no output) fails loudly.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

BIN="$SRC/mayhem/build/selftest"
if [ ! -x "$BIN" ]; then
  echo "missing $BIN: mayhem/build.sh did not build the selftest" >&2
  emit_ctrf ppsspp-selftest 0 1
  exit $?
fi

OUT="$("$BIN" 2>&1)"
rc=$?
printf '%s\n' "$OUT"

passed=$(printf '%s\n' "$OUT" | grep -c '^PASS ')
failed=$(printf '%s\n' "$OUT" | grep -c '^FAIL ')
summary=$(printf '%s\n' "$OUT" | grep -E '^SELFTEST-SUMMARY passed=[0-9]+ failed=[0-9]+$' | tail -1)

# The summary line must agree with the per-check lines, and the suite must have its full size.
if [ "$rc" -ne 0 ] || [ "$summary" != "SELFTEST-SUMMARY passed=$passed failed=$failed" ] || [ "$passed" -lt 50 ]; then
  failed=$(( failed + 1 ))
fi
emit_ctrf ppsspp-selftest "$passed" "$failed"
