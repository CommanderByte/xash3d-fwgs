# Game Launch Modernization Notes

This folder documents the intended modernized shape of the native launcher.

The launcher pilot keeps executable wiring in `game_launch/wscript`, but moves
source code toward `src/launcher/`. Reusable, target-neutral behavior lives in
`src/launcher/`; executable entry points live in `src/launcher/platform/`.

## Documents

- [architecture.md](architecture.md) describes the current responsibilities and
  intended modular boundaries.
- [layout-policy.md](layout-policy.md) describes the intended source, entry
  shell, and launcher resource layout.
- [platform-support.md](platform-support.md) records the current platform seams
  and which ones have been verified locally.
