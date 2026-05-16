# networking — Index

## Public API headers

| Header | Namespace | Key symbols |
|--------|-----------|-------------|
| `networking/networking.hpp` | `xash::networking` | `NetworkContext`, `NetworkInitParams`, `SocketKind` |
| `networking/address.hpp` | `xash::networking` | `NetAddress`, `IpFamily` |
| `networking/errors.hpp` | `xash::networking` | `NetError`, `Result<T>` |
| `networking/message_buf.hpp` | `xash::networking` | `MessageBuf`, `SeekOrigin` |
| `networking/lag_queue.hpp` | `xash::networking` | `LagQueue`, `DelayedPacket` |
| `networking/stats.hpp` | `xash::networking` | `NetworkingStats` |
| `networking/master_list.hpp` | `xash::networking` | `IMasterListConfig`, `IMasterListClient` |
| `networking/protocol_driver.hpp` | `xash::networking` | `IProtocolDriver`, `IProtocolDriverRegistry`, `SplitFormat`, `DeltaTableSet`, `FrameMeta` |
| `networking/netchan.hpp` | `xash::networking` | `Netchan`, `NetchanConfig`, `NetchanFlags`, `FragStream`, `FragSize`, `IBlockSizeProvider`, `Fragbuf`, `FragbufBatch`, `IncomingStream` (Layer 3 — fully implemented) |

## Private / internal headers

| Header | Purpose |
|--------|---------|
| `private/networking/context_impl.hpp` | `NetworkContext::Impl` — pimpl body visible only to TUs in `xash3dpp_networking` |
| `private/networking/wire/wire_format.hpp` | Wire-frozen packet header magic constants and `#pragma pack` POD structs (`SplitHeaderXash`, `SplitHeaderGoldSrc`, `LongPacket`) |
| `private/networking/codec/compress.hpp` | LZSS codec API (`xash::networking::lzss` sub-namespace) |
| `private/networking/codec/compressed_packet.hpp` | `compressed_packet` sub-namespace encode/decode wrappers |
| `private/networking/wire/split_packet.hpp` | `SplitProducerXash`, `SplitProducerGoldSrc`, `SplitFragmentInfo`, single-packet decoders |
| `private/networking/transport/split_reassembler.hpp` | `SplitReassembler` — stateful single-slot fragment accumulator |
| `private/networking/transport/packet_pool.hpp` | `PacketPool`, `PacketSlot` — fixed-slab datagram buffer allocator |
| `private/networking/wire/oob_packet.hpp` | `oob` sub-namespace OOB packet encode/decode |
| `private/networking/transport/loopback_transport.hpp` | `LoopbackTransport` — in-process dual-ring loopback |
| `private/networking/master_list.hpp` | Redirect → `networking/master_list.hpp` (public tree) |
| `private/networking/protocol_driver.hpp` | Redirect → `networking/protocol_driver.hpp` (public tree) |

## Source files

| File | Responsibility |
|------|---------------|
| `src/networking/context.cpp` | `NetworkContext` lifecycle — `init`, `shutdown`, `is_active`, `config`, `get_packet`, `send_packet`, `stats` |
| `src/networking/address.cpp` | `NetAddress` — string parse/format, comparison, reserved-address check |
| `src/networking/message_buf.cpp` | `MessageBuf` — full read/write/seek implementation |
| `src/networking/transport/lag_queue.cpp` | `LagQueue::enqueue`, `try_dequeue` |
| `src/networking/transport/loopback_transport.cpp` | `LoopbackTransport::send`, `receive`, `clear`, `pending` |
| `src/networking/transport/packet_pool.cpp` | `PacketPool::acquire`, `PacketSlot::release` |
| `src/networking/wire/oob_packet.cpp` | `oob::is_oob`, `encode`, `decode` |
| `src/networking/transport/split_reassembler.cpp` | `SplitReassembler::ingest`, `reset` and internal helpers |
| `src/networking/codec/compressed_packet.cpp` | `compressed_packet::encode`, `decode`, `inflated_size` |
| `src/networking/codec/compress_lzss.cpp` | LZSS `compress`, `decompress`, `is_compressed`, `actual_size` |
| `src/networking/codec/compress_null.cpp` | Null codec adapter (non-compression builds) |
| `src/networking/wire/compat_goldsrc.cpp` | GoldSrc nibble-split decode helpers; uses `goldsrc_nibble_max` |
| `src/networking/wire/compat_xash.cpp` | Xash split-packet encode/decode helpers |
| `src/networking/wire/protocol_driver_goldsrc.cpp` | `GoldSrcProtocolDriver` — GoldSrc wire protocol (protocol 48) |
| `src/networking/master_list.cpp` | `MasterListClient`: iterates `master_addresses()`, sends GoldSrc OOB heartbeat/shutdown |
| `src/networking/netchan.cpp` | `Netchan` Layer-3 channel — fully implemented reliable channel, fragment assembly, bandwidth choking |

## Key types

| Type | Kind | Defined in | Role |
|------|------|-----------|------|
| `NetworkContext` | class | `networking/networking.hpp` | Lifecycle hub; owns transport state and pool |
| `NetworkContext::Impl` | struct | `private/networking/context_impl.hpp` | Pimpl body |
| `NetworkInitParams` | struct | `networking/networking.hpp` | Init-time dependency injection bag |
| `SocketKind` | enum class | `networking/networking.hpp` | Client vs. Server socket selector |
| `NetAddress` | struct | `networking/address.hpp` | Unified IPv4/IPv6 endpoint; host-byte-order port |
| `IpFamily` | enum class | `networking/address.hpp` | `V4` / `V6` discriminator |
| `NetError` | enum class | `networking/errors.hpp` | Typed transport/DNS error codes |
| `Result<T>` | alias | `networking/errors.hpp` | `std::expected<T, NetError>` |
| `MessageBuf` | class | `networking/message_buf.hpp` | Bit-level read/write over caller-owned storage |
| `SeekOrigin` | enum class | `networking/message_buf.hpp` | Begin/Current/End seek selector |
| `LagQueue` | class | `networking/lag_queue.hpp` | Delayed-delivery datagram queue (fake-lag) |
| `DelayedPacket` | struct | `networking/lag_queue.hpp` | One slot in the lag queue |
| `NetworkingStats` | struct | `networking/stats.hpp` | Tiered atomic observability counters |
| `IMasterListConfig` | struct (pure virt) | `networking/master_list.hpp` | Master-server policy injectable |
| `IMasterListClient` | struct (pure virt) | `networking/master_list.hpp` | Master-server actions injectable |
| `IProtocolDriver` | struct (pure virt) | `networking/protocol_driver.hpp` | Per-channel protocol compat seam |
| `IProtocolDriverRegistry` | struct (pure virt) | `networking/protocol_driver.hpp` | Factory registry for alternative drivers |
| `SplitFormat` | enum class | `networking/protocol_driver.hpp` | `Xash` vs. `GoldSrc` on-wire split framing |
| `DeltaTableSet` | enum class | `networking/protocol_driver.hpp` | Delta-table flavour selection |
| `FrameMeta` | struct | `networking/protocol_driver.hpp` | Decoded packet-header metadata |
| `SplitHeaderXash` | struct (packed) | `private/networking/wire/wire_format.hpp` | 10-byte Xash SPLITPACKET header |
| `SplitHeaderGoldSrc` | struct (packed) | `private/networking/wire/wire_format.hpp` | 9-byte GoldSrc SPLITPACKETGS header |
| `LongPacket` | struct | `private/networking/wire/wire_format.hpp` | Single in-flight reassembly scratch buffer |
| `SplitFragmentInfo` | struct | `private/networking/wire/split_packet.hpp` | Decoded view of one received split fragment |
| `SplitProducerXash` | class | `private/networking/wire/split_packet.hpp` | Pull-style Xash fragment iterator |
| `SplitProducerGoldSrc` | class | `private/networking/wire/split_packet.hpp` | Pull-style GoldSrc fragment iterator |
| `SplitReassembler` | class | `private/networking/transport/split_reassembler.hpp` | Stateful single-slot fragment accumulator |
| `PacketPool` | class | `private/networking/transport/packet_pool.hpp` | Fixed-slab datagram buffer allocator |
| `PacketSlot` | class | `private/networking/transport/packet_pool.hpp` | RAII handle for one `PacketPool` slot |
| `LoopbackTransport` | class | `private/networking/transport/loopback_transport.hpp` | In-process dual-ring loopback |
| `Netchan` | class | `networking/netchan.hpp` | One reliable channel to a peer (Layer 3 — fully implemented) |
| `PacketHeaderInput` | struct | `networking/protocol_driver.hpp` | Input bag for `IProtocolDriver::write_packet_header` |
| `NetchanConfig` | struct | `networking/netchan.hpp` | Setup-time parameters for `Netchan::setup()` |
| `NetchanFlags` | struct | `networking/netchan.hpp` | Per-channel codec/munge bits (framing comes from `IProtocolDriver`) |
| `FragStream` | enum class | `networking/netchan.hpp` | `Normal` vs. `File` fragment stream |
| `FragSize` | enum class | `networking/netchan.hpp` | `Fragment` / `Split` / `Unreliable` block-size query |
| `IBlockSizeProvider` | struct (pure virt) | `networking/netchan.hpp` | Host-supplied per-channel fragment sizing callback |

## CMake targets

| Target | Type | Public deps | Private deps |
|--------|------|-------------|--------------|
| `xash3dpp_networking` | STATIC | include dir (`xash3dpp`), C++23, `xash3dpp_utilities`, `xash3dpp_memory` | `xash3dpp_core`, `xash3dpp_platform` |
| — | — | LZSS TU always compiled in; `XASH_NET_COMPRESSION` selects `codec/compress_bz2.cpp` (ON) vs `codec/compress_null.cpp` (OFF) at link time | bzip2 backend wiring deferred — `codec/compress_bz2.cpp` is a stub TU until 3rdparty/bzip2 lands |

> **Note**: `xash3dpp_utilities` and `xash3dpp_memory` are PUBLIC so downstream
> targets (e.g. `xash3dpp_host`) that link `xash3dpp_networking` transitively
> see their headers. `xash3dpp_core` and `xash3dpp_platform` are PRIVATE
> (implementation details only).
