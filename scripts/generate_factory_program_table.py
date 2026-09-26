#!/usr/bin/env python3
"""Regenerate the canonical JUCE factory program table from the preset JSON.

Milestone B.1 makes juce_plugin/Source/FactoryProgramTable.h the single runtime
authority for the 26 factory programs. This script derives that header from
pra32-u2-prog-factory-presets.json (the same data mirrored to the web editor).

The regeneration is not part of the build: FactoryProgramTable.h is checked in
and the plugin regression test fails if it ever drifts from either JSON mirror.
Run this only when the preset JSON intentionally changes.

Usage:  python scripts/generate_factory_program_table.py
"""

import json
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "pra32-u2-prog-factory-presets.json")
OUT = os.path.join(REPO, "juce_plugin", "Source", "FactoryProgramTable.h")

# (parameter id, preset key, cc, default) in SynthParameters::getParameters() order.
ORDER = [
    ("osc1Wave",      "OSC_1_WAVE",      102, 0),
    ("osc1Shape",     "OSC_1_SHAPE",      19, 64),
    ("osc1Morph",     "OSC_1_MORPH",      20, 0),
    ("oscDrift",      "OSC_DRIFT",        82, 32),
    ("sawWMode",      "OSC_SAW_W_MODE",   83, 127),
    ("osc2Wave",      "OSC_2_WAVE",      104, 0),
    ("osc2Coarse",    "OSC_2_COARSE",     85, 64),
    ("osc2Pitch",     "OSC_2_PITCH",      76, 72),
    ("oscMix",        "MIXER_OSC_MIX",    21, 64),
    ("subOsc",        "MIXER_SUB_OSC",    23, 64),
    ("filterCutoff",  "FILTER_CUTOFF",    74, 112),
    ("filterReso",    "FILTER_RESO",      71, 48),
    ("filterMode",    "FILTER_MODE",      39, 0),
    ("egFltAmt",      "FILTER_EG_AMT",    24, 40),
    ("filterKeyTrk",  "FILTER_KEY_TRK",    9, 96),
    ("bthFltAmt",     "BTH_FILTER_AMT",   60, 64),
    ("relEqDcy",      "REL_EQ_DECAY",    105, 127),
    ("egAttack",      "EG_ATTACK",        73, 96),
    ("egDecay",       "EG_DECAY",         75, 96),
    ("egSustain",     "EG_SUSTAIN",       30, 0),
    ("egRelease",     "EG_RELEASE",       72, 32),
    ("ampAttack",     "AMP_ATTACK",       52, 32),
    ("ampDecay",      "AMP_DECAY",        53, 32),
    ("ampSustain",    "AMP_SUSTAIN",      54, 127),
    ("ampRelease",    "AMP_RELEASE",      55, 32),
    ("egOscAmt",      "EG_OSC_AMT",       89, 64),
    ("egOscDst",      "EG_OSC_DST",        8, 0),
    ("lfoWave",       "LFO_WAVE",         33, 0),
    ("lfoRate",       "LFO_RATE",          3, 80),
    ("lfoFadeTime",   "LFO_FADE_TIME",    56, 0),
    ("lfoDepth",      "LFO_DEPTH",        17, 0),
    ("lfoFltAmt",     "LFO_FILTER_AMT",   25, 76),
    ("lfoOscAmt",     "LFO_OSC_AMT",      13, 64),
    ("lfoOscDst",     "LFO_OSC_DST",     103, 0),
    ("pbRange",       "P_BEND_RANGE",     57, 2),
    ("portaTime",     "PORTAMENTO",        5, 48),
    ("chorusMix",     "CHORUS_MIX",       93, 127),
    ("choRate",       "CHORUS_RATE",      58, 64),
    ("choDepth",      "CHORUS_DEPTH",     59, 64),
    ("delayTime",     "DELAY_TIME",       90, 87),
    ("delayDepth",    "DELAY_LEVEL",      94, 64),
    ("delayFeedback", "DELAY_FEEDBACK",   92, 64),
    ("delayMode",     "DELAY_MODE",       35, 0),
    ("pan",           "PAN",              10, 64),
    ("ampGain",       "AMP_GAIN",         15, 100),
    ("ampExpnt",      "EG_AMP_MOD",       36, 0),
    ("portaMode",     "VOICE_MODE",       18, 0),
    ("voiceAsgnMode", "VOICE_ASGN_MODE", 110, 0),
    ("bthAmpMod",     "BTH_AMP_MOD",      61, 0),
    ("egVelSens",     "EG_VEL_SENS",      62, 0),
    ("ampVelSens",    "AMP_VEL_SENS",     63, 0),
    ("aftTlfoAmt",    "AFT_T_LFO_AMT",   109, 0),
]


def main():
    with open(SRC, "r", encoding="utf-8") as handle:
        raw = json.load(handle)

    data = {key.strip(): value for key, value in raw.items()}

    lines = [
        "#pragma once",
        "",
        "// -----------------------------------------------------------------------------",
        "// Canonical factory program table (Milestone B.1).",
        "//",
        "// This constexpr, framework-free table is the single authority for the 26",
        "// factory programs exposed by the JUCE/VST wrapper. It is read directly on",
        "// the realtime thread (O(1), zero allocation, zero JSON parsing) and mirrored",
        "// to the APVTS/UI on the message thread.",
        "//",
        "// Rows are in SynthParameters::getParameters() order, so row i always describes",
        "// the same parameter the APVTS reports at index i. Column p (0..25) is the value",
        "// applied for program p; column 0 is INITIALIZATION and must equal the",
        "// SynthParameters default. The JSON mirrors are generated from / validated",
        "// against this table:",
        "//   * pra32-u2-prog-factory-presets.json",
        "//   * web_app/data/presets.json",
        "// Regenerate with scripts/generate_factory_program_table.py.",
        "// -----------------------------------------------------------------------------",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace FactoryPrograms",
        "{",
        "",
        "inline constexpr int kProgramCount = 26;",
        "inline constexpr int kParameterCount = %d;" % len(ORDER),
        "",
        "struct Row",
        "{",
        "    const char* parameterId;",
        "    const char* presetKey;",
        "    int         cc;",
        "    std::array<std::uint8_t, kProgramCount> values; // [program]",
        "};",
        "",
        "inline constexpr std::array<Row, kParameterCount> kRows = {{",
    ]

    for index, (parameter_id, preset_key, cc, default) in enumerate(ORDER):
        entry = data.get(preset_key)
        values = entry[1]
        assert len(values) == 26, (preset_key, len(values))
        assert values[0] == default, (preset_key, values[0], default)
        assert all(0 <= value <= 127 for value in values), preset_key

        formatted = ", ".join("%3d" % value for value in values)
        comma = "," if index < len(ORDER) - 1 else ""
        lines.append(
            '    { %-15s, %-17s, %3d, {{ %s }} }%s'
            % ('"%s"' % parameter_id, '"%s"' % preset_key, cc, formatted, comma)
        )

    lines += [
        "}};",
        "",
        "// Convenience accessor for the engine/APVTS path.",
        "inline constexpr int valueFor (int parameterIndex, int program) noexcept",
        "{",
        "    return kRows[(std::size_t) parameterIndex].values[(std::size_t) program];",
        "}",
        "",
        "} // namespace FactoryPrograms",
        "",
    ]

    with open(OUT, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))

    print("wrote %s (%d rows)" % (OUT, len(ORDER)))


if __name__ == "__main__":
    main()
