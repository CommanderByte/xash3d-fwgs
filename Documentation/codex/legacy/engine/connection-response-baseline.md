# Server Connection Response Baseline

This note captures the `engine/server/sv_client.c` challenge and rejection
response formatting before Phase 59 routing.

## Challenge Response

`SV_SendChallenge()` computes the challenge number from the requester address,
the server challenge salt, and the current challenge time window. If address
handling fails, it sends nothing.

When formatting succeeds, the packet body is:

```text
challenge <challenge> <bandwidth-test-flag>
```

The second value is `0` when bandwidth testing should be skipped and `1` when
the client should perform the bandwidth-test path.

The challenge hash, time window, `netadr_t` handling, and packet send are still
legacy-owned.

## Rejection Response

`SV_RejectConnection()` first formats the reason into a 1024-byte local buffer.
It then reports to the console and sends three out-of-band packets:

```text
<address> connection refused. Reason: <reason>\n
```

```text
errormsg
^1Server was reject the connection:^7 <reason>
```

```text
print
^1Server was reject the connection:^7 <reason>
```

```text
disconnect
```

The awkward phrase `Server was reject the connection` is preserved exactly for
wire compatibility. Callers usually include their own trailing newline in the
reason; the console report adds another newline after the reason, matching the
legacy blank-line behavior for newline-terminated reasons.

## Common Rejection Reasons

`SV_ConnectClient()` emits these stock reasons before client allocation:

- `insufficient connection info`
- `unsupported protocol (<got> should be <expected>)`
- `LAN servers are restricted to local clients (class C)`
- `no challenge for your address`
- `invalid protinfo in connect command`
- user-agent policy rejection messages
- `invalid userinfo in connect command`
- `invalid password`
- `server is full`

Game DLL rejection during `SV_Spawn_f()` also routes through
`SV_RejectConnection()` with the game-provided reason.

## Migration Boundary

Phase 59 may move target-neutral string construction into `src/engine/server`,
but these remain legacy-owned:

- challenge-number generation and validation;
- `netadr_t` address handling and address string conversion;
- `Q_vsnprintf()` formatting of the caller-supplied rejection reason;
- `Con_Reportf()` and `Netchan_OutOfBandPrint()` side effects;
- all connection validation decisions.
