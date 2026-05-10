# Connectionless Classifier Migration

Phase 57 extracts the `SV_ConnectionlessPacket()` decision table into a
target-neutral helper:

- `src/include/engine/server/connectionless_classifier.hpp`
- `src/engine/server/connectionless_classifier.cpp`

The helper accepts the full command line, first token, server initialization
state, and master-server classification. It returns a small enum describing the
legacy handler that should run.

## Intent

The classifier is deliberately narrow. It does not parse network buffers,
tokenize commands, send packets, call the game DLL, or inspect global server
state. The legacy adapter supplies already-known facts and keeps the actual
effects in `sv_client.c`.

## Compatibility Rules

- Uninitialized servers accept only `rcon`.
- Master-server packets preempt public commands once the server is initialized.
- `TSource Engine Query` must be matched against the full line, not only the
  first token.
- First tokens beginning with `U` or `V` are still routed to the source-query
  handler, matching the old loose first-character behavior.
- `ack` and GoldSrc `j` share one acknowledgement classification.
- Unknown initialized non-master packets still fall through to the game DLL
  connectionless callback.

## Next Step

This phase leaves the game DLL fallback in place. A later command-router phase
can decide whether connectionless handlers should become registered operations,
but that should happen after server command registration and game DLL bridge
ownership are clearer.
