# Modern Common Implementations

This folder is reserved for implementation units that support private common
C++ contracts under `src/include/engine/common/`.

Keep implementation here only when the behavior is truly shared across engine
domains. If a helper belongs specifically to server, client, renderer,
filesystem, launcher, or platform code, place it in that domain instead.
