---
name: "Audit extension doors — Q-21 north-star lens"
description: "Read-only audit of one subsystem's extension posture: verifies the boundary spec's 'Extension axes (Q-21)' section covers the current north-star axis set (G-*/P-* in extension-goals.md), that its claims match the code, and that door-rule deviations are recorded as door-debt. Produces a per-axis verdict table and stops. No files are changed."
argument-hint: "subsystem name, e.g. 'server', 'content', 'networking'"
agent: agent
tools: [read, search, execute, cpp-lsp/*, xash-tools/*]
model: claude-sonnet-4-6
---

# Extension Door Audit: `$ARGUMENTS`

Audit the `$ARGUMENTS` subsystem's **extension posture (Q-21)** against the
north-star requirements.

**Analysis only — do not make any changes to any file.**
Present the per-axis table and findings, then stop.

---

## Method

The authoritative methodology — reference documents, the three check
classes (axis completeness, claim accuracy, door-debt honesty), the
`addressed | na-justified | missing | contradicted` vocabulary, severities,
and the output format — lives in
`.github/agents/extension-door-auditor.agent.md`. **Read that charter first
and follow it exactly** for the `$ARGUMENTS` subsystem.

Ground the P-3 statics probe mechanically before judging:

```powershell
& .venv\Scripts\python.exe xash3dpp\tools\compliance_scan.py $ARGUMENTS --checks all --json
```

*(MCP: xash-tools `compliance_scan` with `subsystem="$ARGUMENTS"` — same
data.)* Use its globals/statics findings as the fact base; the charter's
judgment calls remain yours.

---

## Guardrails

- Read-only. If a fix seems obvious, record it as a finding — do not apply it.
- Do not restate the whole boundary doc; only the Extension axes section is
  under audit (plus the code probes the charter names).
- A reasoned "none apply" is a valid verdict — do not manufacture findings
  where the justification holds.
- Finish with the charter's summary block
  (`Verdict: COVERED | GAPS | CONTRADICTED`).
