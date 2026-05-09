# Game Launch Modernization Notes

This folder documents the intended modernized shape of the native launcher.

The launcher pilot keeps executable wiring with the rest of the modern launcher
sources in `src/wscript`. Reusable, target-neutral behavior lives in
`src/launcher/`; executable entry points live in `src/launcher/platform/`.

## Documents

- [architecture.md](architecture.md) describes the current responsibilities and
  intended modular boundaries.
- [layout-policy.md](layout-policy.md) describes the intended source, entry
  shell, and launcher resource layout.
- [platform-support.md](platform-support.md) records the current platform seams
  and which ones have been verified locally.
- [json-policy.md](json-policy.md) records when the launcher should keep its
  minimal JSON reader versus adopting a shared library.
