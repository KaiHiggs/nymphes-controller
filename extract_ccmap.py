#!/usr/bin/env python3
"""Build the authoritative 82-CC Nymphes control map.

Three independent public sources are merged, each field carrying provenance:

  1. reference/nymphes-osc/src/nymphes_midi/NymphesPreset.py  (79 distinct CCs)
     -> the CC *set*. Its `mod_source` field is the key structural fact: the
        modulation-depth CCs (31..37, 39..63, 86..89) are SHARED by all four
        modulation sources and reinterpreted according to the MS selector (CC 30).
  2. https://midi.guide/d/dreadbox/nymphes/  (81 CCs)
     -> display names, groups, ranges and discrete value labels.
  3. reference/nymphescc/nymphescc/messages.dhall (44 CCs)
     -> nicer section grouping, `tics` (labelled range endpoints), long descriptions.

The union of (1) and the three CCs that only midi.guide lists (1 mod wheel,
30 MS selector, 64 sustain pedal) is exactly 82. CC 38 is genuinely absent from
every source - it is not a hole in this table.

Nothing here was read out of the firmware image: prior/L2-firmware.md section 6
establishes that the CC dispatch table is NOT present in the application region
(the SysEx dispatcher is reached through a function pointer). The 28-entry
`basic_inputs` order that prior/L1-firmware.md section 4 decoded from the preset
protobuf is used only as a cross-check on 28 of the 82 CC numbers, and it agrees.

Usage:
  extract_ccmap.py <repo-root> <out.json> <out-header.h>
"""
import hashlib
import json
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
OUT_JSON = sys.argv[2] if len(sys.argv) > 2 else "-"
OUT_H = sys.argv[3] if len(sys.argv) > 3 else None

OSC_PY = ROOT + "/reference/nymphes-osc/src/nymphes_midi/NymphesPreset.py"
DHALL = ROOT + "/reference/nymphescc/nymphescc/messages.dhall"

# --------------------------------------------------------------------------
# midi.guide table, transcribed verbatim from the page (2026-10-30).
# cc: (group, display name, min, max, [labels])
# --------------------------------------------------------------------------
MIDIGUIDE = {
    1:  ("General",     "Mod wheel",                      0, 127, None),
    3:  ("Filter",      "LPF EG depth",                   0, 127, None),
    4:  ("Filter",      "LPF tracking",                   0, 127, None),
    5:  ("General",     "Glide",                          0, 127, None),
    7:  ("Amp",         "Amp level",                      0, 127, None),
    8:  ("Filter",      "LFO cutoff depth",               0, 127, None),
    9:  ("Oscillator",  "Oscillator level",               0, 127, None),
    10: ("Oscillator",  "Sub level",                      0, 127, None),
    11: ("Oscillator",  "Noise level",                    0, 127, None),
    12: ("Oscillator",  "Pulse width",                    0, 127, None),
    13: ("Oscillator",  "LFO pitch depth",                0, 127, None),
    14: ("Oscillator",  "EG pitch depth",                 0, 127, None),
    15: ("Oscillator",  "Detune",                         0, 127, None),
    16: ("Oscillator",  "Chord selector",                 0, 127, None),
    17: ("General",     "Play mode",                      0, 5,
         ["POLY", "UNI A", "UNI B", "TRI", "DUO", "MONO"]),
    18: ("LFO 1",       "LFO 1 rate",                     0, 127, None),
    19: ("LFO 1",       "LFO 1 wave",                     0, 127, None),
    20: ("LFO 1",       "LFO 1 delay",                    0, 127, None),
    21: ("LFO 1",       "LFO 1 fade",                     0, 127, None),
    22: ("LFO 1",       "LFO 1 type",                     0, 3,
         ["BPM", "LOW", "HIGH", "TRACK"]),
    23: ("LFO 1",       "LFO 1 key sync",                 0, 1, ["OFF", "ON"]),
    24: ("LFO 2",       "LFO 2 rate",                     0, 127, None),
    25: ("LFO 2",       "LFO 2 wave",                     0, 127, None),
    26: ("LFO 2",       "LFO 2 delay",                    0, 127, None),
    27: ("LFO 2",       "LFO 2 fade",                     0, 127, None),
    28: ("LFO 2",       "LFO 2 type",                     0, 3,
         ["BPM", "LOW", "HIGH", "TRACK"]),
    29: ("LFO 2",       "LFO 2 key sync",                 0, 1, ["OFF", "ON"]),
    30: ("Modulation",  "Modulation source selector",     0, 3,
         ["LFO 2", "MOD WHEEL", "VELOCITY", "AFTERTOUCH"]),
    31: ("Modulation",  "MS OSC wave depth",              0, 127, None),
    32: ("Modulation",  "MS OSC level depth",             0, 127, None),
    33: ("Modulation",  "MS SUB level depth",             0, 127, None),
    34: ("Modulation",  "MS noise level depth",           0, 127, None),
    35: ("Modulation",  "MS LFO pitch depth",             0, 127, None),
    36: ("Modulation",  "MS pulse width depth",           0, 127, None),
    37: ("Modulation",  "MS glide depth",                 0, 127, None),
    39: ("Modulation",  "MS detune depth",                0, 127, None),
    40: ("Modulation",  "MS chord selector depth",        0, 127, None),
    41: ("Modulation",  "MS EG pitch depth",              0, 127, None),
    42: ("Modulation",  "MS LPF cutoff depth",            0, 127, None),
    43: ("Modulation",  "MS resonance depth",             0, 127, None),
    44: ("Modulation",  "MS LPF EG depth",                0, 127, None),
    45: ("Modulation",  "MS HPF cutoff depth",            0, 127, None),
    46: ("Modulation",  "MS LPF tracking depth",          0, 127, None),
    47: ("Modulation",  "MS LPF cutoff LFO depth",        0, 127, None),
    48: ("Modulation",  "MS filter EG attack depth",      0, 127, None),
    49: ("Modulation",  "MS filter EG decay depth",       0, 127, None),
    50: ("Modulation",  "MS filter EG sustain depth",     0, 127, None),
    51: ("Modulation",  "MS filter EG release depth",     0, 127, None),
    52: ("Modulation",  "MS amp EG attack depth",         0, 127, None),
    53: ("Modulation",  "MS amp EG decay depth",          0, 127, None),
    54: ("Modulation",  "MS amp EG sustain depth",        0, 127, None),
    55: ("Modulation",  "MS amp EG release depth",        0, 127, None),
    56: ("Modulation",  "MS LFO 1 rate depth",            0, 127, None),
    57: ("Modulation",  "MS LFO 1 wave depth",            0, 127, None),
    58: ("Modulation",  "MS LFO 1 delay depth",           0, 127, None),
    59: ("Modulation",  "MS LFO 1 fade depth",            0, 127, None),
    60: ("Modulation",  "MS LFO 2 rate depth",            0, 127, None),
    61: ("Modulation",  "MS LFO 2 wave depth",            0, 127, None),
    62: ("Modulation",  "MS LFO 2 delay depth",           0, 127, None),
    63: ("Modulation",  "MS LFO 2 fade depth",            0, 127, None),
    64: ("General",     "Sustain pedal",                  0, 1, ["OFF", "ON"]),
    68: ("General",     "Legato",                         0, 1, ["OFF", "ON"]),
    70: ("Oscillator",  "OSC wave",                       0, 127, None),
    71: ("Filter",      "Resonance",                      0, 127, None),
    72: ("Amp",         "Amp EG release",                 0, 127, None),
    73: ("Amp",         "Amp EG attack",                  0, 127, None),
    74: ("Filter",      "LPF cutoff",                     0, 127, None),
    75: ("Reverb",      "Reverb size",                    0, 127, None),
    76: ("Reverb",      "Reverb decay",                   0, 127, None),
    77: ("Reverb",      "Reverb filter",                  0, 127, None),
    78: ("Reverb",      "Reverb mix",                     0, 127, None),
    79: ("Filter",      "Filter EG attack",               0, 127, None),
    80: ("Filter",      "Filter EG decay",                0, 127, None),
    81: ("Filter",      "HPF cutoff",                     0, 127, None),
    82: ("Filter",      "Filter EG sustain",              0, 127, None),
    83: ("Filter",      "Filter EG release",              0, 127, None),
    84: ("Amp",         "Amp EG decay",                   0, 127, None),
    85: ("Amp",         "Amp EG sustain",                 0, 127, None),
    86: ("Modulation",  "MS reverb size depth",           0, 127, None),
    87: ("Modulation",  "MS reverb decay depth",          0, 127, None),
    88: ("Modulation",  "MS reverb filter depth",         0, 127, None),
    89: ("Modulation",  "MS reverb mix depth",            0, 127, None),
}

# The 28-entry basic_inputs order decoded from the preset protobuf by
# prior/L1-firmware.md section 4. Used as an independent cross-check only.
BASIC_INPUTS_CC_ORDER = [70, 9, 10, 11, 13, 74, 71, 3, 79, 80, 82, 83,
                         18, 19, 12, 5, 15, 16, 14, 81, 4, 8, 73, 84, 85, 72, 20, 21]


# --------------------------------------------------------------------------
# 1. CC set from nymphes-osc
# --------------------------------------------------------------------------
def osc_cc_set():
    src = open(OSC_PY, encoding="utf-8").read()
    ccs = sorted({int(m) for m in re.findall(r"'cc':\s*(\d+)", src)})
    # a modulation-depth CC is one that appears with more than one mod_source
    mod_sources = {}
    for m in re.finditer(
        r"'cc':\s*(\d+),\s*\n\s*'mod_source':\s*(None|'[a-z0-9_]+')", src
    ):
        cc = int(m.group(1))
        src_name = m.group(2).strip("'")
        mod_sources.setdefault(cc, set()).add(src_name)
    shared = sorted(cc for cc, s in mod_sources.items() if len(s) > 1)
    return ccs, shared


# --------------------------------------------------------------------------
# 2. Dhall extras: descriptions, tics, labels
# --------------------------------------------------------------------------
def find_matching(s, i):
    pairs = {"{": "}", "[": "]", "(": ")"}
    if s.startswith("''", i):
        return s.find("''", i + 2) + 1
    opener, closer = s[i], pairs[s[i]]
    depth, j = 0, i
    while j < len(s):
        c = s[j]
        if c == opener:
            depth += 1
        elif c == closer:
            depth -= 1
            if depth == 0:
                return j
        elif c == '"':
            j += 1
            while j < len(s) and s[j] != '"':
                j += 2 if s[j] == "\\" else 1
        elif s.startswith("''", j):
            k = s.find("''", j + 2)
            j = (k + 1) if k >= 0 else len(s)
        j += 1
    return len(s) - 1


def dhall_extras():
    src = open(DHALL, encoding="utf-8").read()
    out = {}
    for gm in re.finditer(r'\{\s*name\s*=\s*"([^"]+)"\s*,\s*long\s*=\s*"([^"]+)"', src):
        gname, glong = gm.group(1), gm.group(2)
        brace = src.rfind("{", 0, gm.start() + 1)
        body = src[brace + 1 : find_matching(src, brace)]
        cm = re.search(r"content\s*=\s*", body)
        if not cm:
            continue
        a0 = body.find("[", cm.end())
        if a0 < 0:
            continue
        arr = body[a0 + 1 : find_matching(body, a0)]
        pos = 0
        while True:
            sm = re.search(r"Setting::\{", arr[pos:])
            if not sm:
                break
            b = pos + sm.end() - 1
            e = find_matching(arr, b)
            rec = arr[b + 1 : e]
            pos = e
            cmm = re.search(r"cc\s*=\s*(\d+)", rec)
            if not cmm:
                continue
            cc = int(cmm.group(1))
            desc = None
            dm = re.search(r"description\s*=\s*Some\s*''", rec, re.S)
            if dm:
                k = rec.find("''", dm.end())
                desc = " ".join(
                    ln.strip() for ln in rec[dm.end() : k].split("\n") if ln.strip()
                )
            tics = None
            tm = re.search(r"tics\s*=\s*Some\s*\[(.*?)\]\s*\n", rec, re.S)
            if tm:
                tics = [
                    {"label": a, "value": int(v)}
                    for a, v in re.findall(
                        r'\{\s*label\s*=\s*"((?:[^"\\]|\\.)*)"\s*,\s*value\s*=\s*(\d+)\s*\}',
                        tm.group(1),
                    )
                ] or None
            out[cc] = {
                "section": gname,
                "section_long": glong,
                "description": desc,
                "tics": tics,
            }
    return out


# --------------------------------------------------------------------------
# 3. Merge
# --------------------------------------------------------------------------
def main():
    osc_ccs, shared_mod_ccs = osc_cc_set()
    dh = dhall_extras()

    only_midiguide = sorted(set(MIDIGUIDE) - set(osc_ccs))
    only_osc = sorted(set(osc_ccs) - set(MIDIGUIDE))
    everything = sorted(set(MIDIGUIDE) | set(osc_ccs))

    params = []
    for cc in everything:
        mg = MIDIGUIDE.get(cc)
        group, name, lo, hi, labels = (mg if mg else
                                       ("Modulation", "CC %d" % cc, 0, 127, None))
        e = dh.get(cc, {})
        prov = []
        if mg:
            prov.append("midi.guide")
        if cc in osc_ccs:
            prov.append("nymphes-osc")
        if e:
            prov.append("nymphescc")
        params.append({
            "cc": cc,
            "name": name,
            "group": group,
            "section": e.get("section"),
            "min": lo,
            "max": hi,
            "labels": labels,
            "tics": e.get("tics"),
            "description": e.get("description"),
            "isModDepth": cc in shared_mod_ccs,
            "provenance": prov,
        })

    basic_ok = all(c in {p["cc"] for p in params} for c in BASIC_INPUTS_CC_ORDER)

    doc = {
        "map_version": "1.0.0",
        "count": len(params),
        "sources": {
            "nymphes-osc": {
                "path": "reference/nymphes-osc/src/nymphes_midi/NymphesPreset.py",
                "sha256": hashlib.sha256(open(OSC_PY, "rb").read()).hexdigest(),
                "distinct_ccs": len(osc_ccs),
            },
            "nymphescc": {
                "path": "reference/nymphescc/nymphescc/messages.dhall",
                "sha256": hashlib.sha256(open(DHALL, "rb").read()).hexdigest(),
                "ccs_with_metadata": len(dh),
            },
            "midi.guide": {
                "url": "https://midi.guide/d/dreadbox/nymphes/",
                "ccs": len(MIDIGUIDE),
                "fetched": "2026-10-30",
            },
        },
        "reconciliation": {
            "ccs_only_in_midi_guide": only_midiguide,
            "ccs_only_in_nymphes_osc": only_osc,
            "cc38_present": 38 in everything,
            "note": ("CC 38 is absent from every source; the modulation-depth run is "
                     "31-37 then 39-63, so 38 is a genuine gap, not a hole in this table. "
                     "The three CCs only midi.guide lists are the ones the modulation "
                     "matrix shares rather than owning: 1 (mod wheel, an input), "
                     "30 (modulation-source selector) and 64 (sustain pedal)."),
        },
        "modulation_model": {
            "selector_cc": 30,
            "sources": ["LFO 2", "MOD WHEEL", "VELOCITY", "AFTERTOUCH"],
            "shared_depth_ccs": shared_mod_ccs,
            "note": ("The depth CCs are shared by all four modulation sources and "
                     "reinterpreted according to CC 30. A controller UI must show which "
                     "source is selected or a depth edit is ambiguous."),
        },
        "cross_check": {
            "basic_inputs_cc_order": BASIC_INPUTS_CC_ORDER,
            "all_28_present_in_map": basic_ok,
            "source": "prior/L1-firmware.md section 4 (decoded preset protobuf order)",
        },
        "provenance": (
            "Merged from three public sources; no entry was read out of the firmware "
            "image, because prior/L2-firmware.md section 6 establishes that the CC "
            "dispatch table is not in the application region."
        ),
        "params": params,
    }

    txt = json.dumps(doc, indent=2, ensure_ascii=False)
    if OUT_JSON == "-":
        sys.stdout.write(txt + "\n")
    else:
        open(OUT_JSON, "w", encoding="utf-8").write(txt + "\n")
        sys.stderr.write("wrote %s: %d CCs\n" % (OUT_JSON, len(params)))

    if OUT_H:
        emit_header(params, doc, OUT_H)


def cstr(s):
    if s is None:
        return "nullptr"
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit_header(params, doc, path):
    L = []
    L.append("// GENERATED by tools/extract_ccmap.py - do not edit by hand.")
    L.append("// %d MIDI-controllable parameters. Sources: nymphes-osc (CC set),".replace("%d", str(len(params))))
    L.append("// midi.guide (names/ranges/labels), nymphescc (sections/descriptions).")
    L.append("// CC 38 is genuinely absent. See tools/ccmap.json for provenance.")
    L.append("#pragma once")
    L.append("#include <cstdint>")
    L.append("#include <cstddef>")
    L.append("")
    L.append("namespace nymphes::ccmap {")
    L.append("")
    L.append("struct CcSpec")
    L.append("{")
    L.append("    std::uint8_t cc;")
    L.append("    const char*  name;")
    L.append("    const char*  group;")
    L.append("    std::uint8_t minValue;")
    L.append("    std::uint8_t maxValue;")
    L.append("    const char* const* labels;   // discrete value names, or nullptr")
    L.append("    std::uint8_t numLabels;")
    L.append("    bool isModDepth;             // shared across the 4 mod sources")
    L.append("};")
    L.append("")
    for p in params:
        if p["labels"]:
            L.append("inline constexpr const char* kLabels_%d[] = { %s };"
                     % (p["cc"], ", ".join(cstr(x) for x in p["labels"])))
    L.append("")
    L.append("inline constexpr CcSpec kCcMap[] = {")
    for p in params:
        lab = ("kLabels_%d" % p["cc"]) if p["labels"] else "nullptr"
        n = len(p["labels"]) if p["labels"] else 0
        L.append("    { %3d, %-34s %-15s %3d, %3d, %-14s %d, %s },"
                 % (p["cc"], cstr(p["name"]) + ",",
                    cstr(p["group"]) + ",", p["min"], p["max"],
                    lab + ",", n, "true " if p["isModDepth"] else "false"))
    L.append("};")
    L.append("")
    L.append("inline constexpr std::size_t kCcMapSize = sizeof (kCcMap) / sizeof (kCcMap[0]);")
    L.append("")
    L.append("inline constexpr int kModSourceSelectorCc = %d;" % doc["modulation_model"]["selector_cc"])
    L.append("inline constexpr int kSustainCc = 64;")
    L.append("inline constexpr int kModWheelCc = 1;")
    L.append("")
    L.append("// Returns nullptr for a CC number that the Nymphes does not use.")
    L.append("inline const CcSpec* find (int cc) noexcept")
    L.append("{")
    L.append("    for (std::size_t i = 0; i < kCcMapSize; ++i)")
    L.append("        if (kCcMap[i].cc == cc)")
    L.append("            return &kCcMap[i];")
    L.append("    return nullptr;")
    L.append("}")
    L.append("")
    L.append("} // namespace nymphes::ccmap")
    L.append("")
    open(path, "w", encoding="utf-8").write("\n".join(L) + "\n")
    sys.stderr.write("wrote %s: %d entries\n" % (path, len(params)))


if __name__ == "__main__":
    main()
