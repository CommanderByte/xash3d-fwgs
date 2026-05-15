---
name: "Plan implementation order for scaffolded subsystem"
description: "Scan all TODO stubs in a scaffolded xash3dpp subsystem, infer the internal dependency graph, and emit a prioritised implementation plan with a live todo list. Run after scaffold-subsystem and before writing any real code."
argument-hint: "subsystem name, e.g. 'cmd_cvar', 'sound', 'host'"
agent: agent
tools: [read, search]
---

# Implementation plan: `$ARGUMENTS`

Analyse the scaffolded `$ARGUMENTS` subsystem and produce a concrete, ordered
work plan. **Do not write any implementation code.** Output the plan to chat
and create a todo list using the `manage_todo_list` tool.

---

## Step 1 — Inventory all scaffolded files

Locate every file in the subsystem:

- **Boundary spec**: `xash3dpp/docs/boundaries/$ARGUMENTS-boundary.md`
- **Public headers**: all `.hpp` under `xash3dpp/include/xash3dpp/$ARGUMENTS/`
- **Private headers**: all `.hpp` under `xash3dpp/include/xash3dpp/private/$ARGUMENTS/`
- **Source stubs**: all `.cpp` under `xash3dpp/src/$ARGUMENTS/`
- **Test files**: all `.cpp` under `xash3dpp/tests/$ARGUMENTS/`

Read every file in full.  Produce a brief one-line summary per file.

---

## Step 2 — Classify what is done vs. stubbed

Search every source (`.cpp`) file for the pattern `// TODO`.  For each
occurrence, record the enclosing function/method name, file, and line number.

Also examine each test function:
- **Live** — has at least one `CHECK(...)` call with a real expression
- **Stub** — body is empty, or contains only `// TODO` comments

Build two tables:

```
### Already complete
| Symbol | Kind | Where |
|--------|------|-------|
| ...    | enum / struct / inline fn | header |

### Stubbed (needs implementation)
| Symbol | File | Line | Called by live test? |
|--------|------|------|----------------------|
| ...    | ...  | ...  | yes / no             |
```

---

## Step 3 — Build the internal dependency graph

Group all stubbed items into layers by internal dependency.  Use the rules
below to assign each item to the earliest layer it qualifies for:

| Layer | Name | Contents | Rule |
|-------|------|----------|------|
| 0 | **Data types** | Enums, `constexpr` values, POD structs | Declaration-only; already done from scaffold — skip if complete |
| 1 | **Private helpers** | Template data structures (e.g. `CircularBuffer`), compat `.cpp` files (`compat_goldsrc.cpp`, `compat_null.cpp`) | Self-contained; no callers within the subsystem |
| 2 | **`Impl` fields** | The fields that `init()` populates inside the `Impl` struct | Everything else depends on these existing |
| 3 | **Lifecycle** | `init()` and `shutdown()` | Pool creation lives here; the pool must exist before any allocation |
| 4 | **Test-driven core API** | Functions directly called by at least one live test | Highest priority for getting green tests quickly |
| 5 | **Remaining API** | All other public functions, ordered callee-before-caller | Simple / leaf helpers before complex orchestrators |
| 6 | **Stats / debug** | Functions guarded by `XASH_STATS`, `XASH_DEBUG_*`, or similar | Can be deferred until the core API is green |

To assign a function to a layer, ask:
1. Is it called by a live test? → Layer 4
2. Does it call another stubbed function? → must come *after* that function
3. Does it only read/write `Impl` fields that are set in `init()`? → Layer 5
4. Is it guarded by a feature macro? → Layer 6

For multi-file subsystems (e.g. a subsystem with `compat_goldsrc.cpp` and
`compat_null.cpp`), treat each `.cpp` independently.  The compat files are
typically Layer 1 (self-contained) while the main context file spans Layers
2–6.

---

## Step 4 — Produce the ordered work plan

For each layer that has at least one unimplemented item, print a section:

```
### Layer N — <name>

Expected outcome when complete: "<e.g. test_init_shutdown passes>"

| Item | File | Notes |
|------|------|-------|
| `Impl` fields: pool_, observers_[], cmd_text_ | context.cpp | Decide array vs. deque for text buffer |
| `init()` body | context.cpp | create_pool, store injected deps |
| `cvar_find()` | context.cpp | hash lookup; depends on Impl pool field |
```

Keep each Notes cell to one sentence — name the dependency or the
non-obvious design decision, nothing else.

---

## Step 5 — Create the todo list

Call `manage_todo_list` to register one todo item per **concrete unit of work**
identified in Step 4.  Use specific, self-contained titles:

- **Good**: `"Impl: pool_, trust_oracle, observers array fields"`
- **Bad**: `"Implement the Impl struct"`
- **Good**: `"init(): create_pool + store injected deps"`
- **Bad**: `"Implement init"`
- **Good**: `"cvar_find(): hash lookup + linear probe"`

Ordering in the todo list must match the layer order from Step 4.  Set every
item to `not-started`.

---

## Step 6 — Highlight risks and open decisions

Before finishing, scan the boundary spec's **Open questions** section and the
stubbed function list for any items that require a design decision before they
can be coded.  Flag:

- Functions that touch a frozen external ABI surface (game DLL `eiface.h`,
  client DLL `cdll_int.h`, `pm_shared/`)
- Functions whose behaviour depends on an unresolved Open Question in the
  boundary spec
- Compat-flag-sensitive code paths — functions that behave differently when
  `XASH_GOLDSRC_COMPAT` is defined
- Any function that depends on a subsystem not yet fully implemented

Print these as a bulleted warning list at the end of the chat output.
