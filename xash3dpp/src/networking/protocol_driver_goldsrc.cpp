// xash3dpp — default GoldSrc / Xash protocol driver
// Boundary spec: docs/boundaries/networking-boundary.md
//                §"Pluggable game protocol per client"
//
// The GoldSrc driver is always linked.  Alternative drivers are added via
// IProtocolDriverRegistry per the Q-12 per-subsystem compat paradigm.
//
// This TU also exposes the built-in default registry accessor
// (default_protocol_driver_registry()) that NetworkContext falls back to
// when NetworkInitParams::protocol_registry is nullptr.

#include <xash3dpp/private/networking/protocol_driver.hpp>
#include <xash3dpp/private/networking/protocol_driver_default.hpp>

namespace xash::networking {

namespace {

class GoldSrcProtocolDriver final : public IProtocolDriver
{
public:
    [[nodiscard]] const char *name() const noexcept override { return "goldsrc"; }

    [[nodiscard]] SplitFormat   split_format() const noexcept override { return SplitFormat::GoldSrc; }
    [[nodiscard]] DeltaTableSet delta_tables() const noexcept override { return DeltaTableSet::GoldSrc; }
};

// Wire-protocol identifiers recognised by the built-in registry.  GoldSrc
// proper uses 48; the Xash bridge uses 49.  Both currently resolve to the
// same driver because the framing is identical at this layer — the delta
// table set differentiates them later in Layer 4.
inline constexpr std::uint16_t k_protocol_goldsrc = 48;
inline constexpr std::uint16_t k_protocol_xash    = 49;

class DefaultProtocolDriverRegistry final : public IProtocolDriverRegistry
{
public:
    [[nodiscard]] IProtocolDriver *resolve( std::uint16_t protocol ) noexcept override
    {
        if( protocol == k_protocol_goldsrc || protocol == k_protocol_xash )
            return &driver_;
        return nullptr;
    }

private:
    GoldSrcProtocolDriver driver_ {};
};

} // namespace

IProtocolDriverRegistry &default_protocol_driver_registry() noexcept
{
    // Meyers singleton: thread-safe init under C++23, lazily constructed on
    // first use, never destroyed during normal teardown so callers holding
    // a reference past shutdown() do not dangle.
    static DefaultProtocolDriverRegistry instance;
    return instance;
}

} // namespace xash::networking
