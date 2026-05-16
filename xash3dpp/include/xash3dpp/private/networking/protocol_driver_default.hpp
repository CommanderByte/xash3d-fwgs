#pragma once
// xash3dpp — accessor for the built-in default IProtocolDriverRegistry.
// Returns a registry that always exposes the GoldSrc protocol driver
// (wire protocols 48 and 49); resolve() returns nullptr for anything else.
//
// NetworkContext uses this when NetworkInitParams::protocol_registry is
// nullptr — see docs/architecture/networking/context-lifecycle.md and
// docs/architecture/networking/protocol-driver.md.
//
// This header is private; downstream code must go through
// NetworkInitParams::protocol_registry.

#include <xash3dpp/networking/protocol_driver.hpp>

namespace xash::networking {

// Returns a reference to a process-wide, immutable default registry.
// Safe to call from any thread; the returned object's resolve() is
// const-correct and side-effect free.
[[nodiscard]] IProtocolDriverRegistry &default_protocol_driver_registry() noexcept;

} // namespace xash::networking
