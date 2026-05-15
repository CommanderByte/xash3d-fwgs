---
name: "Limits audit — magic number enforcement"
description: "Scan xash3dpp/ (or a specific subsystem) for magic number literals that should be in limits.hpp, verify existing XASH_LIMIT_* entries are used correctly, and report any missing or misplaced limits. Read-only."
argument-hint: "optional subsystem name to scope the scan, e.g. 'filesystem'. Omit to scan all of xash3dpp/."
agent: agent
tools: [read, search]
mode: agent
model: claude-haiku-4-5-20251001
---

# Limits Audit: `$ARGUMENTS`

Verify that all fixed buffer sizes, pool capacities, and count limits are
declared in `xash3dpp/include/xash3dpp/limits.hpp` with the `XASH_LIMIT_*`
override pattern. **Read-only — no files are modified.**

Scope: `xash3dpp/src/$ARGUMENTS/` and `xash3dpp/include/xash3dpp/$ARGUMENTS/`
(or all of `xash3dpp/` if no argument given).

---

## Step 1 — Read limits.hpp

Read `xash3dpp/include/xash3dpp/limits.hpp` in full. List every defined limit:

| Name | Default value | Subsystem group | Override macro |
|------|---------------|-----------------|----------------|
| `k_max_path` | ... | `// filesystem subsystem` | `XASH_LIMIT_MAX_PATH` |
| ...  | ...           | ...             | ...            |

---

## Step 2 — Scan for magic numbers in source files

Search the scoped tree for integer literals that look like limits — constants
used as array sizes, loop bounds, capacity arguments, or buffer sizes.

Patterns that indicate a limit candidate:

```powershell
$scope = if ("$ARGUMENTS") { "xash3dpp\src\$ARGUMENTS\*","xash3dpp\include\xash3dpp\$ARGUMENTS\*" } else { "xash3dpp\src\**\*","xash3dpp\include\xash3dpp\**\*" }

# Array sizes and buffer capacities
Select-String -Path $scope -Pattern "\[\s*[0-9]{2,}\s*\]" -Recurse
# Capacity arguments (e.g. create_pool, CircularBuffer<T,N>)
Select-String -Path $scope -Pattern "<[A-Za-z_]+,\s*[0-9]{2,}>" -Recurse
# Named max/limit constants not in limits.hpp
Select-String -Path $scope -Pattern "constexpr.*=\s*[0-9]{2,}" -Recurse
```

For each hit, record:

| File | Line | Literal | Context | In limits.hpp? |
|------|------|---------|---------|----------------|
| ...  | ...  | ...     | ...     | Yes / No / N/A |

Mark as **N/A** if the literal is clearly not a tunable limit (e.g. `3` in a
loop over RGB channels, `1` as an offset, `0` as a sentinel).

---

## Step 3 — Verify XASH_LIMIT_* usage

For each limit already in `limits.hpp`, verify it is referenced correctly in
source:

- The `inline constexpr` value is used, not a duplicated raw literal
- The `#ifndef XASH_LIMIT_<NAME>` / `#else` / `#endif` override guard is
  present and syntactically correct
- No source file redeclares the same constant with a different name or value

Flag:
- Limit defined in `limits.hpp` but used nowhere → **WARNING — dead limit**
- Same value hard-coded in source despite a `limits.hpp` entry → **WARNING — shadow**
- `XASH_LIMIT_*` macro defined in source (instead of via build system) → **WARNING**

---

## Step 4 — Report

```
Limits Audit — $ARGUMENTS
==========================

limits.hpp entries: <N>

Missing limits (magic numbers not in limits.hpp):
| File | Line | Literal | Suggested name | Suggested group |
|------|------|---------|----------------|-----------------|
| ...  |      |         |                |                 |
(or "None found ✓")

Warnings on existing limits:
| Issue | Limit name | Details |
|-------|-----------|---------|
| ...   | ...        | ...     |
(or "None ✓")

Recommended additions to limits.hpp:
<For each missing limit: the #ifndef / inline constexpr / #else / #endif block
 to add, under the appropriate subsystem group comment.>
```
