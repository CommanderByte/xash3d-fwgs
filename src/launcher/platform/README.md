# Launcher Platform Entry

This folder owns executable entry points and process-level platform glue for
the native launcher.

Keep this layer thin:

- POSIX `main` and Windows `WinMain` signatures.
- Windows GPU-selection exports.
- User-visible fatal launch errors.
- Calls into reusable launcher code under `src/launcher/`.

Do not put reusable startup policy here. If behavior can be unit-tested without
owning an executable entry point, place it in `src/launcher/` with a matching
header under `src/include/launcher/`.
