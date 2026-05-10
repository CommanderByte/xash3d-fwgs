# Local LM Studio Subagent

This repo can use a local LM Studio model as a read-only helper for repeated
analysis work. The intended jobs are small and bounded:

- summarize a legacy file before migration;
- suggest compatibility cases for a helper test;
- compare an adapter against an existing pattern;
- scan selected files for suspicious edge cases.

The local model is not an authority. Treat its output as scratch-pad context,
not as proof. Codex still owns final code changes, ABI-sensitive decisions,
test evidence, and commits.

## LM Studio Setup

1. Load a code-oriented instruct model in LM Studio.
2. Start the local server.
3. Use the default OpenAI-compatible base URL unless you changed it:
   `http://localhost:1234/v1`.
4. Prefer at least 8k context for code review or migration scouting. A 2k
   context can work for status checks and tiny prompts, but it is too tight for
   useful file excerpts.
5. Optional environment variables:
   - `LMSTUDIO_BASE_URL`
   - `LMSTUDIO_MODEL`
   - `LMSTUDIO_API_KEY`

If `LMSTUDIO_MODEL` is not set, the helper scripts ask `/v1/models` and use the
first loaded model id.

For this repo, prefer a code-oriented model for the helper. On the current test
machine, `qwen/qwen3-coder-next` worked well for smoke tests. Smaller coder
models are fine for quick summaries; larger coder models are better for
cross-file migration scouting if VRAM and context permit.

If LM Studio is running with a large context window, such as 64K, use it as an
asynchronous sidecar. Start a bounded analysis prompt, continue direct local
inspection, then fold in only the useful findings after reviewing them. This is
especially helpful when the loaded model spills beyond VRAM and may answer more
slowly.

## Immediate Script

Use `scripts/ask-local-model.ps1` when Codex or a human wants a quick report:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ask-local-model.ps1 `
  -Prompt "Summarize migration risks in this file." `
  -Files engine/server/sv_init.c `
  -MaxTokens 1200
```

The script writes the response to `.codex-cache/local-agent/last-response.md`.

## MCP Server

Use `scripts/lmstudio-mcp-server.py` as a stdio MCP server for clients that can
launch local MCP tools. It exposes:

- `lmstudio_status`: check the endpoint and loaded models;
- `ask_lmstudio`: ask the model with selected repo-relative file excerpts.

Example client config shape:

```json
{
  "mcpServers": {
    "xash-lmstudio": {
      "command": "python",
      "args": ["C:/git/xash3d-fwgs/scripts/lmstudio-mcp-server.py"],
      "env": {
        "LMSTUDIO_BASE_URL": "http://localhost:1234/v1"
      }
    }
  }
}
```

Smoke-test the server protocol:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-lmstudio-mcp.ps1
```

Smoke-test an actual model call:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-lmstudio-mcp.ps1 `
  -AskSmoke -Model "qwen/qwen3-coder-next"
```

By default, the MCP tool refuses to read files outside the repo. Set
`LMSTUDIO_MCP_ALLOW_OUTSIDE_REPO=1` only for explicit manual experiments.
