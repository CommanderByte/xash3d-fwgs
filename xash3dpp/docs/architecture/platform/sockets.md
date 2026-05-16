# Platform sockets — requirements draft

> **Status**: DRAFT — not yet implemented. Picked up in a dedicated session
> before Chunk 4 (networking) begins.\
> **Target library**: `xash3dpp_platform` (extends the existing OS-abstraction
> tier alongside `os_io.hpp` / `os_fd.hpp`).\
> **Consumers**: `xash3dpp_networking` (transport layer), `xash3dpp_http`
> (TCP downloader, future).\
> **Legacy reference**: `engine/common/net_ws.c` (socket bind/recv/send paths),
> `engine/common/net_ws_private.h`.

## Purpose

Provide a thin, testable abstraction over the OS socket API so that **every
real `socket()`, `bind()`, `sendto()`, `recvfrom()`, `select()` /
`WSAPoll()` call in the engine is confined to this layer**. The networking
subsystem then talks to a small `IPlatformSockets` (production) or a fake
(in tests) — no `<winsock2.h>` or `<sys/socket.h>` includes leak into any
other translation unit.

This is the equivalent of `os_io.hpp` for sockets and follows the same
patterns:

- RAII wrapper type (`OsSocket`, mirroring `OsFd`).
- Bit-flag `enum class` for option sets where flags compose naturally.
- Free functions in `xash::platform` for stateless operations.
- A single `IPlatformSockets` interface for the few stateful operations
  the networking subsystem needs to inject as a test seam.
- Platform-specific `.cpp` files (`src/platform/{win32,posix,android}/os_socket.cpp`).

## Threading contract

Per `docs/design/threading-model.md §7`, all socket I/O **must** stay inside
`NET_GetPacket` / `NET_SendPacket` in the networking subsystem today and run
on the main thread (`ThreadRole::Main`). The sockets layer therefore:

- Does **not** spawn background threads (the DNS resolver is a separate
  concern handled inside networking).
- Does **not** maintain any mutable global state beyond the per-process
  Winsock initialisation refcount.
- Marks every public function `// @thread-safety: T_NetIO-ready` —
  callable from `Main` today, callable from a future `T_NetIO` thread with
  no code change.

## Public API surface — required

### `OsSocket` — RAII wrapper

```cpp
class OsSocket {
public:
    OsSocket() noexcept = default;
    explicit OsSocket(SocketHandle h) noexcept;
    ~OsSocket();                                  // closes if valid
    OsSocket(OsSocket&&) noexcept;
    OsSocket& operator=(OsSocket&&) noexcept;
    OsSocket(const OsSocket&)            = delete;
    OsSocket& operator=(const OsSocket&) = delete;

    [[nodiscard]] bool          valid()   const noexcept;
    [[nodiscard]] SocketHandle  get()     const noexcept;
    [[nodiscard]] SocketHandle  release() noexcept;  // hand off ownership
    void                        close()   noexcept;  // safe to call on invalid
};
```

`SocketHandle` is an opaque `using` — typically `int` on POSIX,
`uintptr_t` on Win32 (to match `SOCKET`'s width). Invalid handles compare
equal to `k_invalid_socket`.

### Free functions (`xash::platform`)

| Function | Purpose |
|----------|---------|
| `open_udp_socket(family, port, bind_iface) → Result<OsSocket>` | Create + bind a non-blocking UDP socket. `family` is `IpFamily::V4` or `IpFamily::V6`. `port == 0` requests a kernel-chosen port. `bind_iface` is an optional dotted/colon-hex string or empty for `INADDR_ANY` / `IN6ADDR_ANY`. |
| `open_tcp_socket(family) → Result<OsSocket>` | Create a non-blocking TCP socket (no bind). Used by HTTP downloader. |
| `set_non_blocking(sock, on) → bool` | Toggle non-blocking mode. Sockets returned from `open_*` are already non-blocking; this is for handles obtained externally. |
| `set_broadcast(sock, on) → bool` | Enable `SO_BROADCAST` for master-server discovery packets. |
| `set_reuse_addr(sock, on) → bool` | `SO_REUSEADDR` for dedicated-server restart scenarios. |
| `set_recv_buffer(sock, bytes) → bool` | Set `SO_RCVBUF`. Networking calls this with a per-cvar value at config time. |
| `set_send_buffer(sock, bytes) → bool` | Set `SO_SNDBUF`. |
| `bind_socket(sock, address) → bool` | Bind to a `NetAddress` (for sockets opened without an immediate bind). |
| `sendto(sock, data, to) → Result<size_t>` | Send a UDP datagram. Returns bytes written or `NetError::{WouldBlock,SocketInvalid,…}`. |
| `recvfrom(sock, buffer, from_out) → Result<size_t>` | Receive a UDP datagram. Returns `NetError::WouldBlock` when no data is queued. |
| `send_stream(sock, data) → Result<size_t>` | TCP write; partial writes are reported via the returned byte count. |
| `recv_stream(sock, buffer) → Result<size_t>` | TCP read; `0` indicates orderly shutdown. |
| `connect_stream(sock, to) → Result<void>` | Issue a non-blocking connect. `NetError::WouldBlock` is the normal "in progress" return. |
| `get_local_address(sock) → optional<NetAddress>` | Equivalent of legacy `NET_GetLocalAddress`; for "your address is…" replies. |
| `socket_init() / socket_shutdown()` | Win32 `WSAStartup` / `WSACleanup` ref-counted around all networking lifetimes. POSIX no-ops. |
| `resolve_blocking(host, family) → Result<NetAddress>` | Synchronous `getaddrinfo`. Networking's DNS resolver calls this from its background thread; the rest of the engine must not. |

`Result<T>` is the `std::expected<T, NetError>` alias from
`xash3dpp/include/xash3dpp/networking/errors.hpp`. The sockets layer
**depends on** the `NetError` enum but does **not** depend on the rest of
networking.

### `IPlatformSockets` — injectable seam

```cpp
struct IPlatformSockets {
    virtual ~IPlatformSockets() = default;

    virtual Result<OsSocket> open_udp(IpFamily, std::uint16_t port,
                                      std::string_view bind_iface) noexcept = 0;
    virtual Result<OsSocket> open_tcp(IpFamily) noexcept = 0;
    virtual Result<size_t>   sendto(const OsSocket&,
                                    std::span<const std::byte>,
                                    const NetAddress&)  noexcept = 0;
    virtual Result<size_t>   recvfrom(const OsSocket&,
                                      std::span<std::byte>,
                                      NetAddress& from_out) noexcept = 0;
    // ... TCP equivalents
};

[[nodiscard]] IPlatformSockets& default_platform_sockets() noexcept;
```

The networking subsystem stores an `IPlatformSockets*` in its
`NetworkInitParams`; production code passes `&default_platform_sockets()`;
tests pass an in-memory fake (`FakePlatformSockets`) that records sends and
replays canned receives.

The free functions above and the interface are both required: free
functions for one-off uses (e.g. `socket_init()` at process start),
interface for the hot path (one `recvfrom` per packet per frame).

## NetAddress conversion

`NetAddress` is owned by the networking subsystem
(`include/xash3dpp/networking/address.hpp`). The platform layer must accept
a `const NetAddress&` and convert to `sockaddr_in` / `sockaddr_in6`
internally — the conversion lives in `os_socket.cpp` so that
`<winsock2.h>` / `<arpa/inet.h>` stay confined.

`NetAddress` already enforces the `ip6_0[0..1] == 0` invariant; the
conversion routine asserts this before populating the kernel struct.

## IPv4 / IPv6 dual stack

`open_udp_socket` opens one socket per family. The networking subsystem
holds two handles per logical socket pair (one V4, one V6) and round-robins
between them in `NET_GetPacket`. The platform layer does **not** attempt
`IPV6_V6ONLY=0` dual binding — separate sockets simplifies bind-failure
handling and matches the legacy behaviour byte-for-byte.

## Error mapping

| OS error | `NetError` |
|----------|------------|
| `EAGAIN` / `EWOULDBLOCK` / `WSAEWOULDBLOCK` | `WouldBlock` |
| `EBADF` / `WSAENOTSOCK` / `INVALID_SOCKET` returned by syscall | `SocketInvalid` |
| `EADDRINUSE` / `WSAEADDRINUSE` | `BindFailed` |
| `EMSGSIZE` / `WSAEMSGSIZE` | `Overflow` (caller is expected to fragment) |
| `getaddrinfo` failure with `EAI_AGAIN` | `DnsAgain` |
| Any other `getaddrinfo` failure | `DnsFailure` |
| `EINVAL` from `bind` with malformed address | `BadAddress` |
| Receive buffer truncated | `BufferTooSmall` |
| Socket layer not initialised (Win32 pre-`WSAStartup`) | `NotInitialised` |

Unmapped errors fall through to `SocketInvalid` and the operation logs
with `LogLevel::Error` at the public-API entry point per the standard
"first entry point logs" rule (decisions-architecture §Q-5).

## Win32 specifics

- `socket_init()` calls `WSAStartup(MAKEWORD(2,2), &wsadata)` once per
  process via an atomic refcount; `socket_shutdown()` decrements and calls
  `WSACleanup()` on zero.
- `SocketHandle` is `uintptr_t` (`SOCKET` is `UINT_PTR`).
- `ioctlsocket(FIONBIO)` is used for non-blocking toggle.
- All Win32 paths use UTF-8 strings on the API surface and convert to
  UTF-16 internally where the underlying API requires it (matches the
  existing `os_io.hpp` pattern).

## POSIX / Android specifics

- `socket_init()` / `socket_shutdown()` are no-ops.
- `SocketHandle` is `int`; `k_invalid_socket` is `-1`.
- `fcntl(F_GETFL/F_SETFL | O_NONBLOCK)` for non-blocking toggle.
- Android shares the POSIX implementation — no extra surface.

## Testability

- `FakePlatformSockets` lives under `xash3dpp/tests/test_helpers/` (or a
  per-test fake) and implements `IPlatformSockets` with deterministic
  queues. The networking unit tests target this fake, not the real OS.
- The `socket_init`/`socket_shutdown` free functions are kept call-safe
  multiple times so test fixtures may invoke them without ordering
  ceremony.

## File layout

| Path | Role |
|------|------|
| `xash3dpp/include/xash3dpp/platform/os_socket.hpp` | Public: `OsSocket`, `SocketHandle`, `IpFamily`, free-function declarations |
| `xash3dpp/include/xash3dpp/platform/platform_sockets.hpp` | Public: `IPlatformSockets`, `default_platform_sockets()` |
| `xash3dpp/src/platform/win32/os_socket.cpp` | Win32 implementation |
| `xash3dpp/src/platform/posix/os_socket.cpp` | POSIX implementation |
| `xash3dpp/src/platform/android/os_socket.cpp` | Android — typically `#include`s the POSIX file |

## Dependencies

- `xash3dpp_utilities` (for `string_view` parsing, IP literal helpers).
- `xash3dpp/include/xash3dpp/networking/{address.hpp,errors.hpp}` —
  declared in the public header; no link-time dependency on the
  networking library (these two headers must remain header-only or be
  factored into a tiny `xash3dpp_net_types` interface library if a cycle
  appears).

A cycle resolution path: if making the platform layer depend on
networking-public headers proves awkward, factor `NetAddress` and
`NetError` into their own `xash3dpp_net_types` INTERFACE library that
both `xash3dpp_platform` and `xash3dpp_networking` consume.

## Out of scope for this draft

- Async I/O multiplexing (`select`/`epoll`/`IOCP`). Single-call
  `recvfrom` / `sendto` are sufficient until `T_NetIO` is justified
  (threading-model §7.4).
- TLS / certificate handling. The HTTP downloader handles plain TCP
  only in Chunk 4b; a TLS extension is a separate boundary spec.
- Raw sockets / ICMP / unusual address families.

## Acceptance criteria for the implementation session

1. `OsSocket` RAII wrapper compiles and is non-copyable / movable on
   all three platforms.
2. `FakePlatformSockets` exists in `tests/test_helpers/` and round-trips
   a UDP datagram between two fake sockets without involving the kernel.
3. Real `open_udp_socket(V4, 0, "")` + `sendto` + `recvfrom` succeed on
   loopback in a per-platform smoke test (gated as integration, not a
   default ctest).
4. No `<winsock2.h>` / `<sys/socket.h>` include appears anywhere in
   `xash3dpp/` other than the three `os_socket.cpp` files.
5. Error mapping table above is verified by a unit test that injects
   each error via the fake.
6. `decisions-architecture.md §Q-7` interface rule satisfied —
   `IPlatformSockets` is the **internal seam**; `OsSocket` and the free
   functions are the **same-binary** API.
