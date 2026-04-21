#!/usr/bin/env bash
set -euo pipefail

status_file="docs/migration-status/status.md"

if [[ ! -f "$status_file" ]]; then
  echo "NEXT_PHASE="
  exit 0
fi

phase_status() {
  local phase="$1"
  awk -v phase="$phase" '
    $0 ~ "^- Phase " phase ":" {
      split($0, parts, ": ")
      if (length(parts) >= 2) {
        print parts[2]
      }
    }
  ' "$status_file"
}

p1="$(phase_status 1)"
p2="$(phase_status 2)"
p3="$(phase_status 3)"
p4="$(phase_status 4)"
p5="$(phase_status 5)"

next=""

if [[ "$p1" == "complete" && "$p2" == "not-started" ]]; then
  next="2"
elif [[ "$p2" == "complete" && "$p3" == "not-started" ]]; then
  next="3"
elif [[ "$p3" == "complete" && "$p4" == "not-started" ]]; then
  next="4"
elif [[ "$p4" == "complete" && "$p5" == "not-started" ]]; then
  next="5"
fi

echo "NEXT_PHASE=$next"
