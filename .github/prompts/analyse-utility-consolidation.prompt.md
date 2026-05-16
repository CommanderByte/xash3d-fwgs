---
name: "Analyse utility consolidation"
description: "Scan a folder for duplicated or near-duplicated utility code across its files, identify what can be extracted into a shared utility, and determine where that utility belongs under separation of concerns."
argument-hint: "folder path relative to repo root (e.g. xash3dpp/src/filesystem, engine/server)"
mode: agent
tools: [read, search, edit]
model: claude-sonnet-4-6
---

# Utility Consolidation Audit: $ARGUMENTS

You are scanning **$ARGUMENTS** for code that is duplicated or near-duplicated
across multiple files and could be extracted into a shared utility. This is an
**analysis and planning task** — do not modify any source files unless the final
step explicitly says so.

---

## Step 1 — Enumerate all files in the folder

List every source and header file directly inside `$ARGUMENTS` (and immediate
sub-folders if the tree is shallow). For each file note:

- Language (C / C++)
- Approximate line count
- Its apparent single responsibility (one sentence)

---

## Step 2 — Read each file and catalogue reusable patterns

For every file, identify constructs that look like general-purpose utility code —
not business logic specific to this module. Common signals:

| Pattern | Examples |
|---------|---------|
| String helpers | case-insensitive compare, trimming, splitting, glob matching |
| Numeric helpers | clamping, alignment, bit manipulation, endian swap |
| Memory / buffer helpers | span slicing, zero-init wrappers, safe memcpy |
| File-path helpers | extension extraction, join, normalise separators |
| Error / result wrappers | tiny `expected<T,E>` or `optional`-like helpers |
| Iteration helpers | reverse wrappers, index-pair ranges |
| Platform-guard macros | repeated `#ifdef` blocks for the same condition |
| Logging / assertion macros | `ASSERT`, `WARN`, debug-format wrappers |

For each pattern found, record:

```
File: <file>
Lines: <start>–<end>
Pattern type: <type from table above>
Description: <one sentence>
Also found in: <other files where the same or very similar code exists>
```

---

## Step 3 — Group duplicates and near-duplicates

Cluster the patterns from Step 2 by **semantic similarity** (same behaviour,
possibly different variable names or minor variations). For each cluster:

- Name the cluster (e.g. `ci_string_equal`, `align_up_pow2`)
- List every occurrence (file + line range)
- Note whether they are **identical**, **functionally equivalent**, or
  **nearly equivalent with a known difference** (and what that difference is)
- Estimate the deduplication payoff (lines removed if extracted)

Discard any cluster with only one occurrence or where the variations are
intentional (e.g. different data types that cannot share a template).

---

## Step 4 — Determine where each extraction belongs

For each surviving cluster, apply the separation-of-concerns rules below to
decide where a new or existing shared file should live.

### Placement rules (xash3dpp tree)

| Nature of utility | Suggested home |
|-------------------|----------------|
| Pure C++ (no OS, no engine types) — math, strings, containers | `xash3dpp/include/xash3dpp/utilities/<name>.hpp` (header-only if trivial) or `xash3dpp/src/utilities/<name>.cpp` |
| Pure C++ but needs a TU (avoids ODR / code-bloat) | `xash3dpp/src/utilities/<name>.cpp` + matching header |
| Platform I/O abstraction (file descriptors, paths) | `xash3dpp/include/xash3dpp/private/filesystem/platform/os_io.hpp` or a new `platform/<name>.hpp` |
| Filesystem-specific (only useful inside `filesystem/`) | `xash3dpp/include/xash3dpp/private/filesystem/<name>.hpp` |
| Engine-wide internal (not public API, used by ≥2 subsystems) | `xash3dpp/include/xash3dpp/internal/<name>.hpp` |
| Must remain legacy-compatible (C linkage, no C++ types) | `public/<name>.h` + `public/<name>.c` in the legacy tree |

If the folder under audit is **not** inside `xash3dpp/`, adapt the placement
using the legacy tree conventions (`public/`, `engine/common/`, etc.) and note
the constraint.

For each cluster, produce a placement decision record:

```
Cluster: <name>
Proposed file: <path>
Rationale: <why this location, not another>
Public API? <yes — part of xash3dpp public headers / no — private / internal>
Depends on: <other headers or subsystems it would need>
```

---

## Step 5 — Write the consolidation plan

Create the file `xash3dpp/docs/consolidation/$ARGUMENTS_SLUG-consolidation.md`
(replace `/` with `-` in `$ARGUMENTS` to form the slug) with the following
structure:

```markdown
# Utility Consolidation Plan — <$ARGUMENTS>

## Summary
<Total clusters found, total estimated lines saved, overall assessment.>

## Clusters

### <cluster-name>

**Occurrences**
| File | Lines | Notes |
|------|-------|-------|
| ... | ... | ... |

**Proposed extraction**
- Target file: `<path>`
- Proposed signature / interface sketch:
  ```cpp
  // minimal declaration
  ```
- Rationale: <one paragraph>
- Caveats / risks: <e.g. needs template, breaks ABI, requires header reshuffle>

---
```

Repeat the cluster section for every surviving cluster.

---

## Step 6 — (Optional, only if user confirms) Apply the extractions

Do **not** start this step automatically. Present the plan to the user and ask
for explicit approval.

If approved for a specific cluster:

1. Create the new shared file with the extracted utility.
2. Replace every duplicate occurrence with a call/use of the new shared code.
3. Add the new file to the appropriate `CMakeLists.txt` (if it needs a TU).
4. Verify the build compiles without errors before moving to the next cluster.
5. Mark the cluster as **Done** in the consolidation plan document.
