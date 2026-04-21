#!/usr/bin/env bash
set -euo pipefail

required_files=(
  "codex/prompts/phase-1.md"
  "codex/prompts/phase-2.md"
  "codex/prompts/phase-3.md"
  "codex/prompts/phase-4.md"
  "codex/prompts/phase-5.md"
  "docs/migration-status/status.md"
  "docs/codex-phased-workflow.md"
  ".github/workflows/phase-scaffold-check.yml"
  ".github/workflows/phase-manual-status.yml"
  ".github/workflows/phase-next-reminder.yml"
  "scripts/next_phase_from_status.sh"
  "scripts/render_phase_kickoff_issue.sh"
)

for f in "${required_files[@]}"; do
  if [[ ! -f "$f" ]]; then
    echo "Missing required file: $f" >&2
    exit 1
  fi
done

echo "Phase scaffolding check passed."
