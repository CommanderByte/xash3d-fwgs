# Rendered Console Sink Boundary

## Purpose

Document how the in-game rendered console should fit into the modern
console/logging plan. This is intentionally not a rendering refactor. It keeps
Phase 43 focused on message ownership and platform backends while avoiding an
accidental rewrite of client UI code.

## Current Owner

`engine/client/console.c` owns the rendered Half-Life style console:

- scrollback storage and wrapping;
- notify lines;
- console and chat edit fields;
- console key handling through `Key_Console()` and `Key_Message()`;
- console commands such as `toggleconsole`, `clear`, `messagemode`,
  `messagemode2`, and `contimes`;
- drawing through `Con_DrawConsole()`, `Con_DrawNotify()`, and helper draw
  routines.

`Sys_Print()` currently calls `Con_Print()` directly for non-dedicated builds.
That makes the rendered console a sink in the output fanout, but the sink is
not isolated as an object yet.

## Decision

Keep the rendered console as a legacy sink for now.

Do not move `engine/client/console.c` into `src/engine/console` during Phase
43. It is mixed with client state, key destinations, fonts, renderer state,
screen updates, chat, UI activation, and history. Moving it now would turn a
console/logging phase into a client UI/rendering phase.

The short-term "franken-console" shape is acceptable:

- platform console backends become modern internal objects;
- `Sys_Print()` still calls `Con_Print()` for the rendered console;
- `Con_Printf`/`Con_DPrintf`/`Con_Reportf` remain the public C print surface;
- a future router can treat `Con_Print()` as a rendered-console sink adapter.

## Future Shape

When the router phase starts, introduce a narrow adapter rather than a full
console UI rewrite:

```cpp
class IRenderedConsoleSink {
public:
    virtual ~IRenderedConsoleSink() = default;
    virtual void print(const char *text) = 0;
};
```

The first implementation can simply delegate to `Con_Print()`. Later, a
client/rendering phase can decide whether scrollback, notify lines, edit
fields, and drawing should move into smaller C++ components.

`Con_NPrintf()` and `Con_NXPrintf()` should stay separate from normal logging.
They are transient on-screen debug overlays, not normal console log messages.

## Compatibility Notes

- Preserve `Con_Print()` wrapping, carriage-return replacement, notify timing,
  and color-prefix aware width handling.
- Preserve `Key_Console()` and `Key_Message()` command-buffer behavior.
- Preserve dedicated stubs in `engine/common/dedicated.c`.
- Avoid making platform backends depend on renderer or client globals.
