# Model Selection and Session Management Guide

Quick reference for choosing the right model and knowing when to start fresh.

---

## Model tiers (from the Copilot model picker)

| Tier | Models | Multiplier | Best for |
|------|--------|------------|----------|
| **Budget** | GPT-4.1, GPT-5 mini | 0x (free) | Fast reads, grep-style analysis, status checks |
| **Budget+** | Claude Haiku 4.5, GPT-5.4 mini | 0.33x | Pattern matching, mechanical transforms, simple lookups |
| **Standard** | Claude Sonnet 4.6, GPT-5.2, GPT-5.3-Codex, GPT-5.4 | 1x | The everyday workhorse — most coding and analysis tasks |
| **High-context** | GPT-5.2-Codex | 1x, High context | Sweeping many files at once, large-module reads |
| **Premium** | GPT-5.5 | 7.5x | Large-context reasoning, complex refactors |
| **Top** | Claude Opus 4.7 | 15x | Deep architectural reasoning, subtle concurrency bugs, last resort when Sonnet is wrong |

> **Note on model ID strings**: the display names above may differ from the string
> accepted by the `model:` frontmatter field. Check your Copilot configuration or
> experiment — common patterns are `claude-opus-4-7`, `claude-sonnet-4-6`,
> `gpt-4.1`, etc.

---

## Prompt-to-model recommendations

### Read-only / analysis (no file edits)

| Prompt | Recommended tier | Reasoning |
|--------|-----------------|-----------|
| `status-and-next` | Budget (GPT-4.1) | Just reads files and formats a table |
| `dependency-graph` | Budget+ (Haiku 4.5) | Read + cross-reference, no deep reasoning needed |
| `limits-audit` | Budget+ (Haiku 4.5) | Pattern matching against a known list |
| `assess-impact` | Standard (Sonnet 4.6) | Needs to reason about blast radius across subsystems |
| `analyse-subsystem` | Standard (Sonnet 4.6) | Needs to understand legacy C code intent |
| `finish-subsystem` | Budget+ (Haiku 4.5) | Checklist — reads files and compares against criteria |
| `analyse-modernization` | **Top (Opus 4.7)** | Requires deep C++ knowledge and ABI judgment |
| `analyse-threading` | **Top (Opus 4.7)** for complex subsystems; Standard for simple ones | Race conditions and memory-ordering bugs are subtle — don't shortchange this |
| `analyse-utility-consolidation` | Standard (Sonnet 4.6) | Semantic similarity judgment across files |

### Edit prompts

| Prompt | Recommended tier | Reasoning |
|--------|-----------------|-----------|
| `scaffold-subsystem` | Standard (Sonnet 4.6) | Template expansion — Sonnet handles this well |
| `plan-implementation` | Standard (Sonnet 4.6) | Dependency reasoning over a known structure |
| `write-unit-tests` | Standard or High-context | Needs to hold many function signatures in mind at once |
| `sweep-module` | **High-context (GPT-5.2-Codex)** | Reads every file in a module simultaneously; context window matters most |
| `migrate-to-memory` | Standard (Sonnet 4.6) | Mechanical substitution with correctness judgment |
| `document-architecture` | Standard (Sonnet 4.6) | Writing task with structured output |
| `retriever` | Standard (Sonnet 4.6) | Needs consistency across many passes; Sonnet's 1x cost is important since it may loop 6 times |

### Gates and reviews

| Prompt / Agent | Recommended tier | Reasoning |
|----------------|-----------------|-----------|
| `pre-pr` | Standard (Sonnet 4.6) | Multi-phase gate; cost multiplied by phases |
| `bisect` | Standard (Sonnet 4.6) | Needs to read diffs and reason about causality |
| `xash3dpp-reviewer` (agent) | Budget+ (Haiku 4.5) | Checklist application — not deep reasoning |
| `abi-watchdog` (agent) | Standard (Sonnet 4.6) | ABI mistakes are expensive; don't cheap out |

---

## When Opus 4.7 is worth 15x the cost

Use Opus only when:

1. **`analyse-threading` on a large, stateful subsystem** — networking, audio, filesystem.
   Race conditions involving memory ordering, lock-free queues, or callback re-entry
   are exactly the class of bug where a cheaper model gives confident but wrong answers.
2. **`analyse-modernization` on a large legacy subsystem** — deep C++ ABI implications
   that require understanding of ODR, calling conventions, and vtable layout.
3. **Sonnet gave an answer that felt wrong** — if you're second-guessing a
   Sonnet response on an architectural decision, escalate to Opus for a second opinion.
4. **Novel design decisions** — first time through a subsystem design with no prior
   boundary spec, no legacy survey, and multiple valid approaches.

Everything else: Sonnet or cheaper.

---

## Session lifecycle — when to start fresh

### The compaction problem

Every compaction cycle compresses prior conversation into a summary. The summary
preserves *what* happened but loses *why* — the subtle reasoning, the rejected
alternatives, the "I noticed X so I did Y instead." After 3+ compaction cycles
on an edit-heavy session:

- The model may redo work it already did (it forgot the fix is in)
- It may contradict earlier decisions it made (the nuance is gone)
- It becomes slower and more expensive (large compressed context)
- Read-only analysis becomes less reliable (stale assumptions bleed through)

### Start a new session when

| Situation | Why |
|-----------|-----|
| Moving to a new workflow stage (e.g. sweep done → document-architecture) | Each stage reads its inputs fresh from files; old context adds noise |
| Before any read-only analysis prompt | You want unbiased analysis — prior session's implementation choices will anchor the model |
| After a commit milestone | A commit is a contract: what was true before the commit is now canonical. Fresh context reads the canonical state. |
| After 2–3 compaction cycles on an edit session | Diminishing returns; stale context causes confused fixes |
| When the model starts repeating or contradicting itself | Classic sign of context rot |
| Starting `retriever` | Needs clean slate to track violation count honestly |

### Stay in the same session when

| Situation | Why |
|-----------|-----|
| Mid-`retriever` run (between loops) | The agent must remember which sites it already fixed |
| Mid-`sweep-module` (audit → fix → build) | Needs to hold the full violation table while applying fixes |
| Asking follow-up questions after a large file read | Don't throw away an expensive read |
| Implementing a subsystem from scaffold to first green test | Linear progression; context is all useful |

### The one-session-forever anti-pattern

A session that runs through many subsystems, workflow stages, and compaction
cycles accumulates **false confidence**: the model "knows" things about files it
read three compactions ago, but the files have since changed. It will make
plausible-sounding but wrong claims about current state.

**Rule of thumb**: one session per commit. Each commit is a stable snapshot;
the session that produced it should end there. The next session starts from
that snapshot and reads the files fresh.

---

## Adding `model:` to agent/prompt frontmatter

GitHub Copilot supports a `model:` key in `.github/agents/*.agent.md` and
`.github/prompts/*.prompt.md` frontmatter. This sets the default model for
that agent/prompt without requiring the user to manually select it each time.

Example:

```yaml
---
name: "ABI Watchdog"
description: "..."
tools: [read, search]
model: claude-sonnet-4-6   # adjust to your Copilot's model ID string
---
```

To find the correct model ID string for your Copilot configuration:
1. Open Copilot Chat and select a model from the picker
2. Run `/help` or check the session metadata — some interfaces expose the model ID
3. Or check your GitHub organisation's Copilot policy page for the allowed model list

Not every Copilot plan has access to every model. If a specified model is
unavailable, Copilot falls back to the user's current selection.

---

## Codebase-size adjustments

| Codebase state | Adjustment |
|----------------|-----------|
| Only 1–2 subsystems implemented (current) | Standard tier works everywhere; Opus is rarely needed |
| 5+ subsystems, large legacy survey | Upgrade `sweep-module` and `retriever` to High-context (GPT-5.2-Codex) |
| Full engine (networking, renderer, audio) | Upgrade `analyse-threading` to Opus for every non-trivial subsystem; consider Premium (GPT-5.5) for `sweep-module` |
| Pre-release / stabilisation | Run `abi-watchdog` and `pre-pr` on every PR; keep on Standard — the thoroughness matters more than the model tier |
