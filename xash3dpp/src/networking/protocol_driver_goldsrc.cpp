// xash3dpp — default GoldSrc / Xash protocol driver (scaffold stub)
// Boundary spec: docs/boundaries/networking-boundary.md
//                §"Pluggable game protocol per client"
//
// The GoldSrc driver is always linked.  Alternative drivers are added via
// IProtocolDriverRegistry per the Q-12 per-subsystem compat paradigm.

#include <xash3dpp/private/networking/protocol_driver.hpp>

namespace xash::networking {

namespace {

class GoldSrcProtocolDriver final : public IProtocolDriver
{
public:
    [[nodiscard]] const char *name() const noexcept override { return "goldsrc"; }

    [[nodiscard]] SplitFormat   split_format() const noexcept override { return SplitFormat::GoldSrc; }
    [[nodiscard]] DeltaTableSet delta_tables() const noexcept override { return DeltaTableSet::GoldSrc; }
};

// detail-audit: accepted — temporary file-scope singleton; registry accessor
// will replace this in Chunk 4 per the TODO below.
[[maybe_unused]] GoldSrcProtocolDriver g_goldsrc_driver;

} // namespace

// TODO(Chunk 4): expose a registry accessor so NetworkContext can resolve
// protocol 48/49 to &g_goldsrc_driver without a global from the host layer.

} // namespace xash::networking
