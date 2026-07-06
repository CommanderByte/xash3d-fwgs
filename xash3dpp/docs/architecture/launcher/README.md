# launcher — architecture

`launcher` is the engine **process entry point** — a single translation unit
(`src/launcher/main.cpp`) with no state and no engine logic. It registers the
main thread role, parses argv into a `HostArgs`, constructs a `Host`, and calls
`Host::Main`. Full contract: [`../../boundaries/launcher-boundary.md`](../../boundaries/launcher-boundary.md).

## Layout

| File | Contents |
|---|---|
| `src/launcher/main.cpp` | `WinMain`/`main` entry, `get_arg`/`has_flag` argv helpers, `argv → HostArgs` fill, `Host::Main` call |
| `src/launcher/CMakeLists.txt` | `add_executable(xash3dpp …)`, `cxx_std_23`, links `xash3dpp_host` |

## Flow

```
register_thread_role(ThreadRole::Main)   // MUST be first — QN prerequisite
resolve rootdir  = platform::get_executable_dir()
parse  -game / -basedir / -rodir / -dedicated / -dev  ->  HostArgs
Host host; return host.Main(args);        // blocks until shutdown
```

## Threading

The launcher **establishes** the engine main thread: the process's first thread
is the main thread, and `register_thread_role(ThreadRole::Main)` runs before any
engine call. This is the production half of the QN thread-assert prerequisite —
without it, the `assert_thread_role(ThreadRole::Main)` guards in `host` /
`engine_context` / downstream subsystems would fire fatally on an unregistered
thread. The launcher performs no threading of its own; it remains on the main
thread for the entire `Host::Main` lifetime. (Test mains register the same role
to present as the main thread.)

## Non-goals

No engine logic, no subsystem ownership, no DLL loading, no window management, no
allocation, no stats (`stats exempt`), no cvars/limits. Everything beyond argv
parsing and rootdir/envvar resolution belongs to `host`.
