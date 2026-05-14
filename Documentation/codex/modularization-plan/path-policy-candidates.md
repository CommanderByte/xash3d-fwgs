# Path Policy Candidates

## Purpose

This note identifies path handling behavior that can later move into a
`PathPolicy` component.

## Current Policy Sites

| Function | Behavior |
| --- | --- |
| `FS_CheckNastyPath` | Rejects empty, colon, double slash, parent path, absolute, and `/.` paths unless direct paths are enabled. |
| `FS_AllowDirectPaths` | Globally enables or disables path rejection bypass. |
| `FS_FindFile` | Uses direct-path fallback and strips a leading `../` under direct-path mode. |
| `FS_Open` | Routes writes through `fs_writepath` and directory case repair. |
| `FS_Rename` / `FS_Delete` | Use write path and case repair for mutable operations. |
| `FS_FindLibrary` | Temporarily enables direct paths and resets them before returning. |

## Extraction Candidates

- `PathPolicy::isRejected(path, directPathMode)`.
- `PathPolicy::stripDirectRelativeHack(path)`.
- `WritePathPolicy::resolveWritePath(fs_writepath, path, createPath)`.
- `LibraryPathPolicy::normalizeShortPath(dllname)`.

## Compatibility Notes

- Direct-path mode is global today and must always reset after temporary use.
- The `../` strip behavior in `FS_FindFile` is a compatibility quirk, not a
  general path normalization rule.
- Write operations intentionally use `fs_writepath`, not arbitrary search paths.
- DLL lookup lowercases short paths and applies the platform library extension.
