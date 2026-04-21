#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <phase-number>" >&2
  exit 1
fi

phase="$1"
prompt_file="codex/prompts/phase-${phase}.md"

if [[ ! -f "$prompt_file" ]]; then
  echo "Prompt file not found: $prompt_file" >&2
  exit 1
fi

printf '# Kick off Phase %s Codex task\n\n' "$phase"
printf 'Phase %s was marked complete and Phase %s is ready to start.\n\n' "$((phase - 1))" "$phase"
printf 'This repository cannot run Codex Cloud automatically from GitHub Actions alone.\n'
printf 'Use the checklist below from your Android phone.\n\n'
printf '## Mobile checklist (Codex Cloud)\n\n'
printf -- '- [ ] Open Codex Cloud in your phone browser/app.\n'
printf -- '- [ ] Start a new task in repository "ovelhaaa/pra32-colorcoder".\n'
printf -- '- [ ] Copy the phase prompt below into the task.\n'
printf -- '- [ ] Add one line: "Work only on this phase scope."\n'
printf -- '- [ ] Submit task, review PR, and merge.\n'
printf -- '- [ ] Update "docs/migration-status/status.md".\n\n'
printf '## Prompt to paste\n\n'
printf 'Path: "%s"\n\n' "$prompt_file"
printf '```\n'
cat "$prompt_file"
printf '\n```\n\n'
printf '## Notes\n\n'
printf -- '- This issue is auto-generated when status indicates a valid next phase.\n'
printf -- '- If this phase has already started, close this issue.\n'
