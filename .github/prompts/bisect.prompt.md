---
name: "Bisect — find the commit that broke something"
description: "Run git bisect to locate the exact commit that introduced a build failure, test regression, or behavioural change. Provide a failing symptom and an optional known-good commit. Read-only analysis; does not modify source files."
argument-hint: "description of the failure + optional known-good ref. E.g. 'test_filesystem crashes on open_file — last known good: main'"
agent: agent
tools: [read, search, execute, Build_CMakeTools, RunCtest_CMakeTools, ListTests_CMakeTools]
model: claude-sonnet-4-6
---

# Bisect: `$ARGUMENTS`

Find the commit that introduced the failure described in `$ARGUMENTS`.

---

## Step 0 — Parse the argument

Extract from `$ARGUMENTS`:

- **SYMPTOM** — what is broken (test name, build error, behavioural change)
- **GOOD_REF** — known-good commit or branch (default: `master` if not specified)

---

## Step 1 — Confirm the failure reproduces on HEAD

First verify the failure is real and reproducible:

```powershell
# Build
cd "c:\git\xash3d-fwgs\xash3dpp\build\Debug"
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build . 2>&1 | Select-String "error C[0-9]|error:"

# Test
cd "c:\git\xash3d-fwgs\xash3dpp\build"
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" -C Debug --output-on-failure 2>&1 | Select-Object -Last 20
```

If the failure does NOT reproduce on HEAD, report: "Failure not reproducible on
HEAD — may already be fixed. Last passing state is current." and stop.

Record the exact failure output for use as the "bad" signal in Step 3.

---

## Step 2 — Confirm GOOD_REF is actually good

Check out GOOD_REF in a worktree to avoid disturbing the current branch:

```powershell
git -C "c:\git\xash3d-fwgs" worktree add "c:\git\xash3d-fwgs\bisect-good" $GOOD_REF
```

Build and test at GOOD_REF. If it also fails, ask the user for an earlier
known-good ref and stop.

Clean up the worktree when done:

```powershell
git -C "c:\git\xash3d-fwgs" worktree remove bisect-good --force
```

---

## Step 3 — Run git bisect

```powershell
cd "c:\git\xash3d-fwgs"
git bisect start
git bisect bad HEAD
git bisect good $GOOD_REF
```

For each commit git bisect checks out:

1. **Reconfigure CMake if needed** (only if `CMakeLists.txt` changed):
   ```powershell
   cd "c:\git\xash3d-fwgs\xash3dpp\build"
   & cmake.exe .. -G "Visual Studio 17 2022"
   ```
2. **Build**:
   ```powershell
   & cmake.exe --build "c:\git\xash3d-fwgs\xash3dpp\build\Debug" 2>&1 | Select-String "error:"
   ```
3. **Test** (or apply the specific failure check from Step 1):
   ```powershell
   & ctest.exe -C Debug --output-on-failure 2>&1 | Select-Object -Last 10
   ```
4. Mark the result:
   ```powershell
   git bisect good   # if no failure
   git bisect bad    # if failure reproduces
   ```

Repeat until git bisect identifies the culprit commit.

**Build failures**: if a commit does not compile, mark it `bad` unless the
build failure is clearly unrelated to the symptom (e.g. a different subsystem
entirely) — in that case mark it `skip`:
```powershell
git bisect skip
```

**Max iterations**: `log2(N)` where N = commits between good and bad. Stop and
report if the bisect has not converged after 20 iterations.

---

## Step 4 — Analyse the culprit commit

Once bisect identifies the commit:

```powershell
git bisect reset
git show --stat <culprit-hash>
git show <culprit-hash>
```

Read the diff and explain:
- What changed
- Why it caused the failure (one paragraph)
- Which file and line is the root cause

---

## Step 5 — Report

```
Bisect result
=============
Culprit commit: <hash> <short message>
Author:         <name>
Date:           <date>

Root cause:
<one paragraph — what changed and why it broke the symptom>

Files changed:
<list from git show --stat>

Suggested fix:
<one sentence — what to revert or what to change, or "see root cause above">
```
