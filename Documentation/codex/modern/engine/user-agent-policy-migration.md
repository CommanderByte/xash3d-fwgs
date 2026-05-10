# User-Agent Policy Migration

Phase 55 moves the deterministic `SV_ProcessUserAgent()` validation rules into
modern server code.

## Implemented Shape

Modern code lives in:

- `src/include/engine/server/user_agent_policy.hpp`;
- `src/engine/server/user_agent_policy.cpp`;
- `tests/engine/user_agent_policy.cpp`.

Legacy glue lives in:

- `engine/server/user_agent_policy_adapter.h`;
- `engine/server/user_agent_policy_adapter.cpp`;
- `engine/server/sv_main.c`.

The modern helper validates a UUID, an optional input-device string, and a
plain policy object. It returns a `UserAgentValidationCode`, plus exact legacy
rejection text for the adapter.

The adapter keeps the live boundary in legacy code. `sv_main.c` still extracts
info-string keys, reads cvars, checks `SV_CheckID()`, and calls
`SV_RejectConnection()`.

## Compatibility Rules

- UUIDs must remain exactly 32 lowercase hex characters.
- Invalid UUIDs are rejected before ban-list lookup.
- Banned valid IDs still return `You are banned!\n`.
- Missing input-device lists are allowed unless `sv_allow_noinputdevices` is
  false.
- Input-device strings use legacy `Q_atoi` parsing.
- Disallowed device checks keep the legacy order: touch, mouse, joystick, VR.
- Rejection messages preserve line breaks and spacing.

## Tests

`tests/engine/user_agent_policy.cpp` covers:

- valid, missing, short, long, non-hex, and uppercase UUIDs;
- accepted user agents;
- banned IDs;
- missing input-device list rejection;
- touch/mouse/joystick/VR disallow cases;
- legacy hex input-device parsing;
- exact rejection message text.

## Later Work

This helper intentionally does not parse the full info string. If connection
handling later gains a structured connection-request object, that object can
own info-string parsing and pass the extracted values into this policy.
