# Modern Architecture Notes

This folder documents the intended modernized internals for this fork.

Use this folder for new subsystem-independent designs and future C++ module
contracts. Keep `Documentation/codex/legacy/` focused on how the current code
works today, and keep `Documentation/codex/modularization-plan/` focused on
migration phases and decisions.

## Documents

- [debugging/](debugging/README.md) contains detailed architecture notes for
  the modern shared debugging utility layer.
- [engine/](engine/README.md) contains modern engine migration notes, starting
  with the BaseCmd registry pilot.
- [filesystem/](filesystem/) contains filesystem modernization audits,
  ownership boundaries, and compatibility notes.
- [game-launch/](game-launch/README.md) contains the launcher modernization
  pilot notes and resource layout policy.
- [thread-safe-debugging-utilities.md](thread-safe-debugging-utilities.md)
  describes the shared debugging, snapshot, serialization, and trace utility
  model intended for the rewrite.

## Rules

- Preserve legacy C ABI boundaries unless a later decision explicitly changes
  them.
- Keep third-party utility types behind private facades.
- Prefer snapshot-based diagnostics over formatting while holding runtime
  locks.
- Treat thread-safety as an interface contract, not an afterthought hidden
  inside subsystem code.
