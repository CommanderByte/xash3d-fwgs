# Prompt Authoring Guide

Standards for writing and maintaining `.prompt.md` and `.agent.md` files in `.github/`.

---

## Frontmatter spec

Every `.prompt.md` file must have these fields in this order:

```yaml
---
name: "Short display name (≤ 60 chars)"
description: "One sentence: what it does, when to invoke it, what it produces."
argument-hint: "what $ARGUMENTS expects"   # omit only if the prompt takes no argument
mode: agent
tools: [read, search]                       # extend as needed — see Tools policy below
model: claude-sonnet-4-6                    # see MODEL-GUIDE.md for tier recommendations
---
```

**Forbidden**: `agent: agent` or `agent: "agent"` — this is a legacy field. Use `mode: agent`.

Every `.agent.md` file uses:

```yaml
---
name: "Display name"
description: "One sentence: role, scope, what it reports."
tools: [read, search]
model: claude-sonnet-4-6
---
```

Agents never need `mode:` or `argument-hint:`.

---

## Tools policy

| Access level | `tools:` value | Use for |
|---|---|---|
| Read-only | `[read, search]` | Analysis, audits that produce reports only |
| Docs-write | `[read, search, edit]` | Prompts that write docs but not source code |
| Full | `[read, search, edit, run, terminal]` | Prompts that modify source files, build, and test |

**Rule**: grant the minimum access needed. Analysis prompts that do not modify source
files must not include `edit` in their tools list.

---

## Required sections by prompt type

| Section | Analysis prompt | Implementation prompt | Scaffolding prompt |
|---|---|---|---|
| **Opening paragraph** — scope + `$ARGUMENTS` usage | ✓ required | ✓ required | ✓ required |
| **Guardrails** — explicit "do not modify X" or safety constraints | one-liner: "Do not modify source files." | full block | minimal |
| **Reference Documents** | if ≥ 2 external docs needed | always | if ≥ 2 external docs needed |
| **Step/Check structure** | ✓ required | ✓ required | ✓ required |
| **Done Condition** | optional — add when the definition of "done" is non-obvious | ✓ required | ✓ required |
| **Commit step** | never — analysis prompts do not commit | ✓ required | if files are created |

---

## $ARGUMENTS conventions

| Convention | Value | Examples |
|---|---|---|
| **Subsystem name** (default) | snake_case directory name under `xash3dpp/src/` | `networking`, `cmd_cvar`, `utilities` |
| **Folder path** | repo-relative path when a folder is the subject | `xash3dpp/src/filesystem` |
| **Rule token** | `RULE_NAME: description` colon-separated pair | `NODISCARD: add [[nodiscard]] to all Result<T> returns` |

The `argument-hint` field must state which convention the prompt uses.
Omit `argument-hint` only when the prompt takes no argument at all (e.g. `init`, `status-and-next`).

---

## Commit message format

All prompts that produce a commit must use the project convention:

```
tag: short description

- one line per logical change
- group by check category or subsystem
```

Where `tag` is the subsystem name or a short file-based tag (e.g. `networking`, `limits`,
`prompts`). **Never** use Conventional Commits format (`feat(scope):`, `refactor(scope):` etc.).

---

## Structural patterns

Use **numbered steps** (`## Step 1 — Name`) for linear workflows.  
Use **named checks** (`### CHECK-NAME — description`) for audit/compliance passes.  
Use **phases** (`## Phase N — Name`) only for prompts with genuinely distinct macro-stages
(e.g. analysis → gate decision → apply).

Do not mix patterns within one prompt.

---

## Model selection

See [MODEL-GUIDE.md](MODEL-GUIDE.md) for the authoritative tier recommendations per prompt.
The `model:` field in frontmatter is the default; the user can override at invocation time.

Defaults:
- Analysis prompts: `claude-haiku-4-5-20251001` for mechanical checks, `claude-sonnet-4-6` for
  reasoning-heavy analysis, `claude-opus-4-7` for deep architectural or concurrency work.
- Implementation prompts: `claude-sonnet-4-6`.
- Scaffolding prompts: `claude-sonnet-4-6`.
