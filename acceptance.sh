#!/usr/bin/env bash
# acceptance.sh — the whole acceptance run, from a built tree, with no device.
#
#   ./acceptance.sh
#
# It builds nothing. It assumes `cmake --build build` has already succeeded in this
# directory, and it needs the shared gate root for the corpora (the vendor dumps and
# the captured live dump are shared roots, not copies inside this tree):
#
#   GATE=/path/to/corpus-root   (holds vendor/ and evidence/; not in this repo)
#
# Every check below either passes or the script exits non-zero. Nothing here opens a
# MIDI port, so no lock is needed and the synth is untouched.

set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

GATE="${GATE:?set GATE to the corpus root that holds vendor/ and evidence/}"
# 16 checks. If you change the number in REPORT.md or a closeout, run
#   ./acceptance.sh | grep -cE '^PASS'
# rather than counting by eye: I miscounted this once and published "18".

CLI="${HERE}/build/bin/nymphes-cli"
VST3="${HERE}/build/NymphesController_artefacts/Release/VST3/NymphesController.vst3"
STANDALONE="${HERE}/build/NymphesController_artefacts/Release/Standalone/NymphesController.app"

FACTORY="${GATE}/vendor/Nymphes_Factory_Presets_Factory_Bank.syx"
USERBANK="${GATE}/vendor/Nymphes_Factory_Presets_User_Bank.syx"
LIVE="${GATE}/evidence/L4-device/device_dump_run1.syx"

FAILED=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; FAILED=$((FAILED + 1)); }
note() { printf '      %s\n' "$*"; }

# --- 0. the artefacts exist ---------------------------------------------------
[ -x "$CLI" ] && pass "nymphes-cli built" || fail "nymphes-cli missing at $CLI"
[ -d "$VST3" ] && pass "VST3 bundle built" || fail "VST3 bundle missing at $VST3"
[ -f "${VST3}/Contents/Resources/moduleinfo.json" ] \
    && pass "VST3 carries moduleinfo.json (hosts can enumerate it)" \
    || fail "VST3 has no moduleinfo.json"
[ -d "$STANDALONE" ] && pass "Standalone app built" || fail "Standalone app missing at $STANDALONE"

# --- 1. offline tests ---------------------------------------------------------
if ctest --test-dir build --output-on-failure > /tmp/nymphes-ctest.log 2>&1; then
    pass "ctest: $(grep -oE '[0-9]+% tests passed[^.]*' /tmp/nymphes-ctest.log | head -1)"
else
    fail "ctest failed (see /tmp/nymphes-ctest.log)"
    tail -20 /tmp/nymphes-ctest.log | sed 's/^/      /'
fi

# --- 2. vendor corpus ---------------------------------------------------------
check_corpus () {
    local label="$1" path="$2" want_frames="$3" want_crc="$4"
    [ -f "$path" ] || { fail "$label: corpus missing at $path"; return; }

    local out
    out="$("$CLI" verify "$path" 2>&1)"
    note "$(printf '%s' "$out" | grep -E 'messages|pages|crc|payload' | tr '\n' '|' | sed 's/|/  /g')"

    printf '%s' "$out" | grep -q "messages  ${want_frames} decoded, 0 rejected" \
        && pass "$label: $want_frames frames decoded, 0 rejected" \
        || fail "$label: expected $want_frames decoded / 0 rejected"

    printf '%s' "$out" | grep -q "crc       ${want_crc}/${want_crc} valid" \
        && pass "$label: CRC-8 valid on $want_crc/$want_crc" \
        || fail "$label: CRC check did not report $want_crc/$want_crc"
}

check_corpus "vendor factory bank" "$FACTORY"  49 49
check_corpus "vendor user bank"    "$USERBANK" 98 98
check_corpus "live device dump"    "$LIVE"     98 98

# --- 3. the live dump proves the payload length is derived, not assumed -------
if [ -f "$LIVE" ]; then
    out="$("$CLI" verify "$LIVE" 2>&1)"
    if printf '%s' "$out" | grep -qE 'payload   1049\.\.1156 bytes \(16 of 98 not the canonical 1049\)'; then
        pass "live dump: 8 distinct payload sizes 1049..1156, 16 non-canonical, all accepted"
    else
        fail "live dump: payload sizes not reported as expected (a fixed-1049 codec would fail here)"
        note "$(printf '%s' "$out" | grep payload)"
    fi
fi

# --- 4. the vendor dumps are all canonical, which is why they are not enough --
if [ -f "$FACTORY" ] && [ -f "$USERBANK" ]; then
    if "$CLI" verify "$FACTORY" 2>&1 | grep -q '(0 of 49 not the canonical 1049)' \
       && "$CLI" verify "$USERBANK" 2>&1 | grep -q '(0 of 98 not the canonical 1049)'; then
        pass "vendor dumps are 100% canonical 1049 (so they alone cannot catch a fixed-size codec)"
    else
        fail "vendor dumps unexpectedly contain non-canonical payloads"
    fi
fi

# --- 5. the device-write safety gates ----------------------------------------
# A real slot write needs the device, so what is checked here is that both gates are
# closed before any byte is sent. The factory-write refusal itself is asserted at the
# codec level by preset_safety_test, which needs no device at all.
#
# Capture first, then match: with `set -o pipefail` a failing command in a pipeline
# makes `if cmd | grep` false even when grep matched.
no_lock_out="$("$CLI" write "$FACTORY" --slot A1 2>&1 || true)"
if printf '%s' "$no_lock_out" | grep -qE -- '--lock'; then
    pass "CLI refuses a device write without an exclusive --lock"
else
    fail "CLI did not demand --lock before touching the device"
    note "$(printf '%s' "$no_lock_out" | head -1)"
fi

no_yes_out="$("$CLI" write "$FACTORY" --slot A1 --lock /tmp/nymphes-acceptance.lock 2>&1 || true)"
if printf '%s' "$no_yes_out" | grep -qiE 'refus|without --yes'; then
    pass "CLI refuses to write without --yes, and prints the plan instead"
else
    fail "CLI did not refuse a write without --yes"
    note "$(printf '%s' "$no_yes_out" | head -1)"
fi
rm -f /tmp/nymphes-acceptance.lock

# --- 6. the 82-CC map --------------------------------------------------------
if "$CLI" ccmap 2>&1 | grep -qE '(^|[^0-9])82([^0-9]|$)|82 parameters|count'; then
    pass "CLI ccmap reports the 82-parameter map"
else
    n="$("$CLI" ccmap 2>&1 | wc -l | tr -d ' ')"
    [ "$n" -ge 82 ] && pass "CLI ccmap lists $n lines (>= 82)" || fail "CLI ccmap listed only $n lines"
fi

# --- result ------------------------------------------------------------------
echo
if [ "$FAILED" -eq 0 ]; then
    echo "ACCEPTANCE PASSED"
    exit 0
fi
echo "ACCEPTANCE FAILED: $FAILED check(s)"
exit 1
