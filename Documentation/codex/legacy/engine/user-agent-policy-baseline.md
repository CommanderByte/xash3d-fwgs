# User-Agent Policy Baseline

Phase 55 documents `SV_ProcessUserAgent()` before the validation policy moves
behind a modern helper.

## Current Inputs

`SV_ProcessUserAgent(netadr_t from, const char *useragent)` extracts two fields
from the connection user-agent info string:

- `uuid`: authentication certificate/client ID used for ban checks and stored
  later as `hashedcdkey`;
- `d`: decimal or legacy `Q_atoi`-parseable input-device bit mask.

The live policy comes from server cvars:

- `sv_allow_noinputdevices`;
- `sv_allow_touch`;
- `sv_allow_mouse`;
- `sv_allow_joystick`;
- `sv_allow_vr`.

The ID ban decision comes from `SV_CheckID(id)`.

## UUID Compatibility

The UUID must be exactly 32 characters. Each character must be either:

- `0` through `9`;
- lowercase `a` through `f`.

Uppercase hex letters are rejected. Missing, short, long, or non-hex strings
are rejected before the ID ban list is checked.

The exact rejection message is:

```text
invalid authentication certificate
```

including a trailing newline in the actual packet text.

## Ban Compatibility

If the UUID is valid and `SV_CheckID(id)` matches, the connection is rejected
with:

```text
You are banned!
```

including a trailing newline in the actual packet text.

## Input-Device Compatibility

If `sv_allow_noinputdevices` is false, a missing or empty `d` field is rejected
with:

```text
This server does not allow
connect without input devices list.
Please update your engine.
```

When a `d` field is present, it is parsed with legacy `Q_atoi` semantics. That
means decimal, hexadecimal strings such as `0x2`, and other legacy quirks are
accepted the same way as other engine integer parsing.

Device bits are checked in this order:

1. touch;
2. mouse;
3. joystick;
4. VR.

The first disallowed matching device decides the rejection message. Unknown
bits are ignored.

## Migration Boundary

Phase 55 may move the validation policy. It must leave these legacy-owned:

- `Info_ValueForKey()` extraction from the user-agent string;
- cvar reads;
- `SV_CheckID()`;
- `SV_RejectConnection()`;
- `netadr_t` and live connection routing.
