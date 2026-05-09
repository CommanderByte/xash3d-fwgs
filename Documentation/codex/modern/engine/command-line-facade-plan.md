# Command-Line Facade Plan

## Direction

Command-line behavior has two different owners:

- the launcher owns process startup, platform argument conversion, and config
  defaults;
- the engine `Sys_*` facade owns compatibility helpers used after startup.

Phase 45 keeps that split intact. The engine now has a small command-line view
helper that can answer legacy-style questions without depending on `host`.
`system.c` remains the compatibility surface and still owns legacy copying and
numeric conversion.

## Modern Helper Shape

`CommandLineView` is deliberately small:

```cpp
struct CommandLineView
{
	int argc;
	const char **argv;
};
```

The helper layer provides:

- `FindCommandLineArgument`: legacy `Sys_CheckParm` semantics without host
  globals;
- `FindCommandLineValue`: bounded lookup for the raw value following a flag;
- change-game censor helpers from Phase 44.

The C adapter exposes the same behavior to legacy C files without requiring C++
headers in `engine/common`.

## Compatibility Choices

- Argument index 0 is still ignored.
- Matching remains ASCII case-insensitive.
- Null argument entries are skipped.
- First match wins.
- Missing values return null before the C facade decides how to report failure.
- `Sys_GetIntFromCmdLine` still calls `Q_atoi` so existing decimal, hex, and
  character literal behavior stays centralized in the legacy CRT helpers.

## Future Follow-Up

If the engine eventually gains a process-wide runtime settings object, this
helper can become the read-only view feeding that object. For now, keeping it
stateless is cleaner and avoids tying command-line lookup to launcher config,
host globals, or console command registration.
