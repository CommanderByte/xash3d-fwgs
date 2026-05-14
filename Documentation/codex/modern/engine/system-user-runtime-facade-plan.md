# System User Runtime Facade Plan

## Direction

`Sys_GetCurrentUser` and `Sys_GetNativeObject` should stay as legacy C facade
functions. Modern code can move underneath them when a branch is either
target-neutral or verifiable on the current platform.

Phase 49 applies a conservative rule:

- move the Win32 current-user lookup because it can be built and smoke-tested
  locally;
- keep POSIX, Vita, Switch, and Android behavior in legacy code until the 800
  series validation phases can build or manually test those targets.

## Current User Helper

The modern helper is split in two:

- `current_user.cpp` owns target-neutral name policy;
- `current_user_adapter.cpp` owns the C adapter and the Win32 lookup.

The target-neutral policy is intentionally small:

```cpp
const char *SelectCurrentUserName(const char *candidate);
```

It returns the candidate when it is non-empty and returns `Player` otherwise.
That matches the old fallback rule without requiring tests to fake OS APIs.

## Legacy Facade

`engine/common/system.c` now routes only the Win32 branch through:

```c
Xash_GetCurrentUserName()
```

The POSIX and Vita branches remain in `system.c` for now. This avoids changing
unvalidated platform behavior while still proving the platform-selected adapter
pattern.

## Native Object Helper

`Sys_GetNativeObject` remains untouched in this phase. Its routing order matters
because filesystem native objects must win before Android platform objects.
Future work should validate the Android path before replacing it with a modern
provider chain.

## Future Shape

A later platform-runtime phase can grow this into a small provider model:

```text
CurrentUserProvider -> UserRuntimeFacade
NativeObjectProvider -> RuntimeObjectFacade
```

That should wait until POSIX/Vita/Android validation exists. The current phase
keeps the seam narrow on purpose.
