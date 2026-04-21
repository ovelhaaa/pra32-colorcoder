# Codex Phased Workflow (Scaffold)

This repository uses five explicit phase prompts under `codex/prompts/`:

- `phase-1.md`
- `phase-2.md`
- `phase-3.md`
- `phase-4.md`
- `phase-5.md`

## How to run each phase

1. Start a new Codex task.
2. Paste the full content of the target phase prompt file into the task.
3. Tell Codex to work only on that phase scope.
4. Review and merge the resulting PR.
5. Update `docs/migration-status/status.md`.
6. Move to the next phase.

## What is automatic

- CI checks that phase scaffold files exist.
- CI runs a lightweight scaffold validator script.
- A manual GitHub Actions workflow can print current scaffold status and reminders.
- When `docs/migration-status/status.md` changes on `main`/`master`, a workflow checks for a completed-to-next-phase transition and opens a kickoff reminder issue for the next phase.

## What is manual (cannot be fully automated from repo alone)

- Starting each Codex task.
- Choosing exact implementation details inside each phase.
- Hardware validation on actual encoder/ST7789 setup.
- Final subjective UI/readability judgment.
- Deciding when to mark each phase complete.

## About automatic phase triggering

- The repository can automatically create a **next-phase reminder issue** after a merge updates phase status.
- The repository cannot autonomously execute a Codex implementation round from GitHub Actions alone.
- Use the reminder issue to start the next manual Codex task with `codex/prompts/phase-N.md`.

## Conservative workflow guidance

- Do one phase per PR.
- Avoid cross-phase implementation in the same PR.
- Preserve synth/audio behavior and page-table-driven parameter semantics in every phase.
