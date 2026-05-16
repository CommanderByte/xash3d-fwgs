# Address and Errors

> **Defined in**: `networking/address.hpp`, `networking/errors.hpp`  
> **Source**: `src/networking/address.cpp`  
> **Namespace**: `xash::networking`

## Overview

This page covers the two foundation types that every other networking
component depends on: `NetAddress` (a clean IPv4/IPv6 endpoint) and
`NetError`/`Result<T>` (the typed error propagation mechanism).

---

## NetAddress

`NetAddress` is the canonical internal address type, replacing the legacy
packed `netadr_t` SDK struct. It stores an address family, a port in
host byte order, and the raw IP bytes in a V4/V6 union.

### Fields

| Name | Type | Role |
|------|------|------|
| `family` | `IpFamily` | `V4` or `V6` |
| `port` | `std::uint16_t` | Host byte order. Platform layer converts to network order when building `sockaddr`. |
| `ip6_0[2]` | `std::uint8_t[2]` | Must be `{0,0}` for V4 addresses — matches legacy `netadr_t` layout invariant |
| `addr.v4[4]` | `std::uint8_t[4]` | IPv4 address bytes (union member) |
| `addr.v6[16]` | `std::uint8_t[16]` | Full IPv6 address bytes (union member) |

### Key operations

**Factories (`constexpr` static)**:

- `loopback_v4(port)` — 127.0.0.1 with the given port
- `any_v4(port)` — INADDR_ANY (all-zeros) with the given port

**Parsing / formatting** (in `address.cpp`):

- `from_string(sv)` → `Result<NetAddress>` — parses `"1.2.3.4:port"`,
  `"[::1]:port"`, etc. Calls platform DNS for hostnames (blocking).
- `from_string_nb(sv)` → non-blocking variant; returns `DnsAgain` when the
  resolver needs more time.
- `to_string()` → `std::string` — formats as `"ip:port"` or `"[ipv6]:port"`.
- `base_to_string()` → address only, no port.

**Comparison**:

- `operator==` / `operator!=` — full equality (family + ip bytes + port)
- `compare_base(a, b)` → `bool` — equality ignoring port
- `compare_by_mask(a, b, mask)` — CIDR-style masked comparison for firewall
  rules; used by the legacy `NET_CompareAdrByMask` path.
- `is_reserved()` → `bool` — returns `true` for loopback (127.x), link-local
  (169.254.x), private ranges (10.x, 172.16-31.x, 192.168.x) and broadcast;
  used to suppress master-server queries for LAN-only servers.
- `is_loopback()` → `bool` — 127.0.0.1 or ::1 check.

### Lifecycle / ownership

Value type; trivially copyable and `constexpr`-constructible. No heap allocation.

### ABI note

The legacy `netadr_t` (`common/netadr.h`) is a distinct packed-20-byte SDK
struct consumed by game DLLs. The rewrite keeps `NetAddress` as a clean
internal type; the `xash3dpp_abi` shim layer converts between them at the
engine DLL boundary. Do not conflate the two.

---

## IpFamily

```cpp
enum class IpFamily : std::uint8_t { V4, V6 };
```

Address-family discriminator. `networking/address.hpp` defines it here;
`platform/os_socket.hpp` re-exports it via `using` so platform code shares the
same type without a circular dependency.

---

## NetError

```cpp
enum class NetError : std::uint32_t { WouldBlock, SocketInvalid, BindFailed,
    Overflow, BadAddress, BufferTooSmall, NotInitialised, DnsAgain, DnsFailure };
```

Typed error codes covering both transport-layer and DNS failures.

| Value | Meaning |
|-------|---------|
| `WouldBlock` | EAGAIN/EWOULDBLOCK — no data available; normal on non-blocking recv |
| `SocketInvalid` | EBADF or invalid fd/HANDLE |
| `BindFailed` | EADDRINUSE — port already in use |
| `Overflow` | EMSGSIZE — packet too large; caller must fragment |
| `BadAddress` | EINVAL from bind; also used by decoders for malformed headers |
| `BufferTooSmall` | Receive buffer too small for the incoming datagram |
| `NotInitialised` | I/O called before `NetworkContext::init()` |
| `DnsAgain` | EAI_AGAIN — transient DNS failure; retry later |
| `DnsFailure` | Any other `getaddrinfo` error |

**Design note**: the codec helpers (`lzss::decompress`, `compressed_packet::decode`,
`oob::decode`) reuse `BadAddress` and `BufferTooSmall` rather than adding new
error categories. `NetError` is intentionally focused on transport semantics.

---

## Result\<T\>

```cpp
template<typename T>
using Result = std::expected<T, NetError>;
```

Success-or-error alias used throughout the networking and platform-sockets
layers. Requires C++23 (`std::expected`).

Usage patterns:

```cpp
// Return an error:
return std::unexpected( NetError::WouldBlock );

// Return a value:
return bytes_written;

// Check at call site:
auto r = ctx.get_packet( sock, from, buf );
if( !r ) { /* r.error() == NetError::WouldBlock */ }
else      { /* *r is bytes read */ }
```

`Result<void>` is used when the operation either succeeds silently or fails;
`return {}` on the success path.

## Threading model

`NetAddress` is a plain value type; it may be constructed, copied, and compared
from any thread.  `NetError` and `Result<T>` are similarly value-semantic with
no shared mutable state.

## See also

- [context-lifecycle.md](./context-lifecycle.md) — `NetworkContext` uses `Result<T>` for all I/O return values
- [message-buf.md](./message-buf.md) — `MessageBuf` uses its own overflow flag, not `Result<T>`
- Legacy: `common/netadr.h`, `engine/common/net_ws.c` (address utils)
