#!/usr/bin/env python3
"""Read-only MCP bridge for a local LM Studio OpenAI-compatible endpoint."""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any


SERVER_NAME = "xash-lmstudio-bridge"
SERVER_VERSION = "0.1.0"
DEFAULT_BASE_URL = "http://localhost:1234/v1"
DEFAULT_SYSTEM_PROMPT = (
    "You are a read-only local analysis subagent for the Xash3D modernization "
    "repo. Summarize findings, suggest tests, and flag uncertainty. Do not "
    "claim files were edited."
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def base_url(value: str | None = None) -> str:
    raw = value or os.environ.get("LMSTUDIO_BASE_URL") or DEFAULT_BASE_URL
    return raw.rstrip("/")


def api_key() -> str:
    return os.environ.get("LMSTUDIO_API_KEY") or "lm-studio"


def post_json(url: str, payload: dict[str, Any], timeout: int = 300) -> dict[str, Any]:
    data = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=data,
        method="POST",
        headers={
            "Authorization": f"Bearer {api_key()}",
            "Content-Type": "application/json",
        },
    )

    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def get_json(url: str, timeout: int = 20) -> dict[str, Any]:
    request = urllib.request.Request(
        url,
        method="GET",
        headers={"Authorization": f"Bearer {api_key()}"},
    )

    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def loaded_model(url: str) -> str:
    models = get_json(f"{url}/models")
    data = models.get("data") or []
    if not data:
        raise RuntimeError(f"LM Studio responded, but no models were listed by {url}/models")

    model_id = data[0].get("id")
    if not model_id:
        raise RuntimeError("LM Studio returned a model entry without an id")

    return str(model_id)


def safe_file_excerpt(path_text: str, max_chars: int) -> tuple[str, str]:
    root = repo_root()
    path = Path(path_text)
    if not path.is_absolute():
        path = root / path

    resolved = path.resolve()
    allow_outside = os.environ.get("LMSTUDIO_MCP_ALLOW_OUTSIDE_REPO") == "1"
    if not allow_outside and root not in (resolved, *resolved.parents):
        raise RuntimeError(f"Refusing to read outside repo: {path_text}")

    text = resolved.read_text(encoding="utf-8", errors="replace")
    if len(text) > max_chars:
        text = text[:max_chars] + f"\n...[truncated after {max_chars} chars]..."

    try:
        label = str(resolved.relative_to(root))
    except ValueError:
        label = str(resolved)

    return label.replace("\\", "/"), text


def tool_status(arguments: dict[str, Any]) -> str:
    url = base_url(arguments.get("base_url"))
    try:
        models = get_json(f"{url}/models")
    except Exception as exc:  # noqa: BLE001 - report any local endpoint failure.
        return f"LM Studio endpoint unavailable at {url}: {exc}"

    names = [str(item.get("id", "<missing-id>")) for item in models.get("data", [])]
    if not names:
        return f"LM Studio endpoint is reachable at {url}, but no models are loaded."

    return "LM Studio endpoint is reachable at {0}. Loaded models:\n{1}".format(
        url,
        "\n".join(f"- {name}" for name in names),
    )


def tool_ask(arguments: dict[str, Any]) -> str:
    prompt = str(arguments.get("prompt") or "").strip()
    if not prompt:
        raise RuntimeError("prompt is required")

    url = base_url(arguments.get("base_url"))
    model = str(arguments.get("model") or os.environ.get("LMSTUDIO_MODEL") or loaded_model(url))
    system = str(arguments.get("system") or DEFAULT_SYSTEM_PROMPT)
    temperature = float(arguments.get("temperature", 0.2))
    max_tokens = int(arguments.get("max_tokens", 1024))
    max_file_chars = int(arguments.get("max_file_chars", 12000))
    files = arguments.get("files") or []

    content = [prompt]
    for file_path in files:
        label, excerpt = safe_file_excerpt(str(file_path), max_file_chars)
        content.append(f"\n## File: {label}\n```\n{excerpt}\n```")

    payload = {
        "model": model,
        "temperature": temperature,
        "max_tokens": max_tokens,
        "messages": [
            {"role": "system", "content": system},
            {"role": "user", "content": "\n".join(content)},
        ],
    }
    response = post_json(f"{url}/chat/completions", payload)
    choices = response.get("choices") or []
    if not choices:
        raise RuntimeError("LM Studio returned no choices")

    message = choices[0].get("message") or {}
    text = message.get("content") or message.get("reasoning_content") or choices[0].get("text")
    if not text:
        raise RuntimeError("LM Studio returned an empty response")

    return str(text)


TOOLS = [
    {
        "name": "lmstudio_status",
        "description": "Check the local LM Studio endpoint and list loaded model ids.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "base_url": {
                    "type": "string",
                    "description": "OpenAI-compatible base URL. Defaults to LMSTUDIO_BASE_URL or http://localhost:1234/v1.",
                }
            },
        },
    },
    {
        "name": "ask_lmstudio",
        "description": "Ask the local model for read-only analysis, optionally including selected repo files.",
        "inputSchema": {
            "type": "object",
            "required": ["prompt"],
            "properties": {
                "prompt": {"type": "string"},
                "system": {"type": "string"},
                "files": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Repo-relative files to include as excerpts.",
                },
                "max_file_chars": {"type": "integer", "default": 12000},
                "temperature": {"type": "number", "default": 0.2},
                "max_tokens": {"type": "integer", "default": 1024},
                "model": {"type": "string"},
                "base_url": {"type": "string"},
            },
        },
        "annotations": {
            "readOnlyHint": True,
            "destructiveHint": False,
            "idempotentHint": True,
        },
    },
]


def result_text(text: str) -> dict[str, Any]:
    return {"content": [{"type": "text", "text": text}]}


def error_response(message_id: Any, code: int, message: str) -> dict[str, Any]:
    return {
        "jsonrpc": "2.0",
        "id": message_id,
        "error": {"code": code, "message": message},
    }


def handle_request(message: dict[str, Any]) -> dict[str, Any] | None:
    method = message.get("method")
    message_id = message.get("id")
    params = message.get("params") or {}

    if method == "initialize":
        protocol_version = params.get("protocolVersion") or "2025-06-18"
        return {
            "jsonrpc": "2.0",
            "id": message_id,
            "result": {
                "protocolVersion": protocol_version,
                "capabilities": {"tools": {"listChanged": False}},
                "serverInfo": {"name": SERVER_NAME, "version": SERVER_VERSION},
            },
        }

    if method == "notifications/initialized":
        return None

    if method == "ping":
        return {"jsonrpc": "2.0", "id": message_id, "result": {}}

    if method == "tools/list":
        return {"jsonrpc": "2.0", "id": message_id, "result": {"tools": TOOLS}}

    if method == "tools/call":
        name = params.get("name")
        arguments = params.get("arguments") or {}
        try:
            if name == "lmstudio_status":
                text = tool_status(arguments)
            elif name == "ask_lmstudio":
                text = tool_ask(arguments)
            else:
                raise RuntimeError(f"Unknown tool: {name}")
        except Exception as exc:  # noqa: BLE001 - surface tool failures to client.
            return {
                "jsonrpc": "2.0",
                "id": message_id,
                "result": {
                    "isError": True,
                    "content": [{"type": "text", "text": str(exc)}],
                },
            }

        return {"jsonrpc": "2.0", "id": message_id, "result": result_text(text)}

    return error_response(message_id, -32601, f"Method not found: {method}")


def main() -> int:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        try:
            message = json.loads(line)
            response = handle_request(message)
        except json.JSONDecodeError as exc:
            response = error_response(None, -32700, f"Parse error: {exc}")
        except Exception as exc:  # noqa: BLE001 - last-resort protocol error.
            response = error_response(None, -32603, f"Internal error: {exc}")

        if response is not None:
            sys.stdout.write(json.dumps(response, separators=(",", ":")) + "\n")
            sys.stdout.flush()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
