#!/usr/bin/env python3
"""Validate every factory-preset representation against the canonical C++ table.

Milestone B.1 makes juce_plugin/Source/FactoryProgramTable.h the single runtime
authority for the 26 factory programs. This script fails if any mirror drifts:

  * pra32-u2-prog-factory-presets.json
  * web_app/data/presets.json
  * pra32-u2-editor.html   (its factory bank, i.e. programs 8..25)

The editor also ships an unrelated legacy "editor only" bank (#128..#135) that
predates the 26-program plugin table; it is intentionally not compared.

It is intentionally dependency-free (standard library only) so the Windows CI
job can run it with the system Python.

Usage:  python scripts/validate_factory_presets.py
"""

import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLE = os.path.join(REPO, "juce_plugin", "Source", "FactoryProgramTable.h")
JSON_FILES = [
    os.path.join(REPO, "pra32-u2-prog-factory-presets.json"),
    os.path.join(REPO, "web_app", "data", "presets.json"),
]
EDITOR = os.path.join(REPO, "pra32-u2-editor.html")

PROGRAM_COUNT = 26
EDITOR_BANK_OFFSET = 8


def parse_table():
    """Return (order, values) where values[presetKey] is a list of 26 ints."""
    with open(TABLE, "r", encoding="utf-8") as handle:
        text = handle.read()

    pattern = re.compile(
        r'\{\s*"(\w+)"\s*,\s*"(\w+)"\s*,\s*(\d+)\s*,\s*\{\{([^}]*)\}\}'
    )

    order = []
    values = {}

    for match in pattern.finditer(text):
        parameter_id, preset_key, _cc, raw = match.groups()
        ints = [int(part) for part in raw.split(",") if part.strip()]
        assert len(ints) == PROGRAM_COUNT, (parameter_id, len(ints))
        order.append((parameter_id, preset_key))
        values[preset_key] = ints

    return order, values


def parse_json():
    out = {}

    for path in JSON_FILES:
        with open(path, "r", encoding="utf-8") as handle:
            raw = json.load(handle)

        trimmed = {key.strip(): value for key, value in raw.items()}
        out[path] = {
            key: value[1]
            for key, value in trimmed.items()
            if isinstance(value, list) and len(value) >= 2 and isinstance(value[1], list)
        }

    return out


def parse_editor():
    with open(EDITOR, "r", encoding="utf-8") as handle:
        text = handle.read()

    factory = {}
    editor_only = {}

    for name, target in (("presetControllers", factory),
                         ("editorOnlyPresetControllers", editor_only)):
        pattern = re.compile(
            r"\b%s\s*\[\s*(\w+)\s*\]\s*=\s*\[([^\]]*)\]\s*;" % name,
            re.DOTALL,
        )

        for match in pattern.finditer(text):
            key, raw = match.groups()
            target[key] = [int(part) for part in raw.split(",") if part.strip()]

    return factory, editor_only


def main():
    order, table = parse_table()
    json_mirrors = parse_json()
    editor_factory, _editor_only = parse_editor()

    failures = []
    checked = 0

    for _parameter_id, preset_key in order:
        canonical = table[preset_key]

        for path, mirror in json_mirrors.items():
            checked += 1
            actual = mirror.get(preset_key)
            if actual != canonical:
                failures.append("%s: %s" % (os.path.basename(path), preset_key))

        # The editor's factory bank starts at program 8.
        checked += 1
        actual_factory = editor_factory.get(preset_key)
        if actual_factory != canonical[EDITOR_BANK_OFFSET:]:
            failures.append("editor factory: %s" % preset_key)

    if failures:
        print("Factory preset drift detected (%d mismatches):" % len(failures))
        for item in failures[:40]:
            print("  " + item)
        return 1

    print("Factory presets consistent across table, JSON and editor (%d checks)." % checked)
    return 0


if __name__ == "__main__":
    sys.exit(main())
