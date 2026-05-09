# Private Utility Headers

This folder contains subsystem-neutral C++ utility headers for modern internals.

Current headers:

- `checksum.hpp`: modern CRC32 helpers that mirror the legacy `crclib` CRC
  behavior without exposing engine state
- `hash.hpp`: legacy-compatible hash-key helpers shared by modern internals and
  C compatibility exports
- `registry.hpp`: fixed-capacity ordered registry helper with explicit
  duplicate handling and stable enumeration

Rules:

- keep utility contracts private unless a later decision promotes them
- prefer deterministic ownership and allocation behavior
- keep subsystem policy outside generic utilities
- add focused tests before a utility is used by filesystem modernization code
