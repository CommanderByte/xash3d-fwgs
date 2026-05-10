# Connection Response Formatter Migration

Phase 59 adds target-neutral helpers for server challenge and rejection response
text:

- `src/include/engine/server/connection_response.hpp`
- `src/engine/server/connection_response.cpp`

The helpers format packet/report strings only. They do not generate challenge
numbers, inspect network addresses, validate clients, log to the console, or
send packets.

## Extracted Formatting

The modern helper currently formats:

- `challenge <challenge> <bandwidth-test-flag>`
- rejection console report text
- `errormsg` rejection packet body
- `print` rejection packet body
- bare `disconnect` rejection packet body

The legacy C adapter exposes one function per formatted string, allowing
`sv_client.c` to preserve its current send order and side effects.

## Legacy Ownership Kept

`sv_client.c` still owns:

- `SV_GetChallenge()` and `SV_CheckChallenge()`;
- address-type handling and MD5 challenge hashing;
- formatting the caller-supplied rejection reason from varargs;
- `Con_Reportf()` and `Netchan_OutOfBandPrint()`;
- connect validation, user-agent policy calls, and game DLL rejection.

This keeps the phase compatible with future work on a deeper connection manager
without moving protocol decisions prematurely.
