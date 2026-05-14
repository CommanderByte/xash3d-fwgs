# Restart Branch Guide

This branch intentionally starts from `origin/master` and carries forward only
the planning layer from `codex-repo-onboarding-modular-rewrite`: Codex
documentation, Codex helper scripts, and README breadcrumbs under `src/` and
`tests/`. It does not carry forward the experimental engine/filesystem source
rewrite.

## What Was Preserved

- `Documentation/codex/` contains the architecture notes, legacy audits,
  migration guides, task history, decision log, deferred work, and completed
  TODO records from the modular rewrite effort.
- `scripts/append-phase-evidence.ps1`, `scripts/new-phase.ps1`,
  `scripts/phase-status.ps1`, `scripts/precommit-phase.ps1`,
  `scripts/run-phase-validation.ps1`, and the related runtime/local-model
  helpers preserve the workflow automation used while building the notes.
- `src/**/README.md` and `tests/**/README.md` preserve the intended ownership
  map for future source and test folders without adding the implementation
  files that made the previous branch heavy.

## Useful First Reads

- `Documentation/codex/codebase-map.md` for repository orientation.
- `Documentation/codex/agent-instructions.md` for the working rules that kept
  the modernization effort behavior-preserving.
- `Documentation/codex/modern/cpp-ownership-target.md` for the important
  correction: C++ facades are scaffolding, not the final architecture.
- `Documentation/codex/modern/engine/pre-159-simplification-checkpoint.md` and
  `Documentation/codex/modern/engine/module-cleanup-checkpoint.md` for lessons
  from the over-modularized direction.
- `Documentation/codex/modern/common/header-ownership-audit.md` and
  `Documentation/codex/modern/engine/platform-portability-architecture.md` for
  the most recent audit work that should survive a reset.
- `Documentation/codex/tasks.md` for the historical task ledger and decision
  records. Treat older phases as evidence, not as an obligation to continue the
  same implementation shape.

## Recommended Restart Shape

1. Keep compatibility contracts stable first: public C ABI, SDK-facing
   structures, renderer/client/game DLL callback tables, wire formats, disk
   formats, and platform compile-time behavior.
2. Start new code work with narrow audits and parity tests before adding modern
   wrappers.
3. Prefer small grouped concepts that match engine vocabulary over one-helper
   facades or abstract module names.
4. Use the preserved scripts to maintain task/evidence hygiene, but validate
   each script on the clean master-based branch before treating it as required
   workflow.
5. When a preserved note conflicts with current `master`, trust `master` first
   and update the note with a dated correction instead of deleting the history.

## Branch Provenance

- Source branch preserved before extraction:
  `codex-repo-onboarding-modular-rewrite` at commit `10f4005b`.
- Restart branch base: `origin/master`.
- Extraction policy: documentation, scripts, and README breadcrumbs only.
