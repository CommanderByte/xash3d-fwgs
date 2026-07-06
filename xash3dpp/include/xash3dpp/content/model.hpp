#pragma once
// xash3dpp — content model types (OQ-2 opaque handle + the cached Model)
// Legacy reference: common/com_model.h (modtype_t), engine/common/mod_local.h
//   (NL_* needload states, FCRC_* flags).
//
// The frozen model_t / studiohdr_t are produced only at the ABI edge; the
// registry traffics in the opaque ModelHandle and the internal Model.
//
// @thread-safety: plain value types — no shared state.

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace xash::content {

// Model kind (legacy modtype_t: mod_bad=-1, brush, sprite, alias, studio).
enum class ModelType : std::int8_t
{
    Bad = -1,
    Brush,
    Sprite,
    Alias,
    Studio,
};

// Slot load state (legacy needload NL_*). Not a bool despite the qboolean
// storage in the frozen model_t (boundary quirk).
enum class NeedLoad : std::uint8_t
{
    Unreferenced = 0, // free slot
    NeedsLoaded,      // referenced, awaiting a load
    Present,          // loaded and live
    FreeUnused,       // reap candidate after level-transition precaching
};

// Per-model CRC state (legacy FCRC_*), for the cheat-detection surface (OQ-6).
enum class CrcFlags : std::uint8_t
{
    None          = 0,
    ShouldChecksum = 1u << 0,
    ChecksumDone   = 1u << 1,
};

[[nodiscard]] constexpr CrcFlags operator|( CrcFlags a, CrcFlags b ) noexcept
{
    return static_cast<CrcFlags>( static_cast<std::uint8_t>( a ) | static_cast<std::uint8_t>( b ) );
}
[[nodiscard]] constexpr bool any( CrcFlags f ) noexcept { return static_cast<std::uint8_t>( f ) != 0; }

// ---------------------------------------------------------------------------
// ModelHandle — opaque {slot index, generation} (OQ-2). Generation 0 is the
// null handle; a live slot has generation >= 1 and is bumped on free, so a
// stale handle fails to resolve (use-after-free safe).
// ---------------------------------------------------------------------------

struct ModelHandle
{
    std::uint16_t index      = 0;
    std::uint16_t generation = 0;

    [[nodiscard]] constexpr bool valid() const noexcept { return generation != 0; }
    constexpr explicit operator bool() const noexcept   { return valid(); }
    constexpr bool operator==( ModelHandle o ) const noexcept
    {
        return index == o.index && generation == o.generation;
    }
};

// ---------------------------------------------------------------------------
// Model — one cached model. The format payload (studio/sprite/alias data or a
// brush world reference) is attached by the loaders; this is the shared head.
// ---------------------------------------------------------------------------

class Model
{
public:
    Model() = default;
    explicit Model( std::string name ) noexcept : name_( std::move( name ) ) {}

    [[nodiscard]] std::string_view name() const noexcept { return name_; }
    [[nodiscard]] ModelType type() const noexcept        { return type_; }
    [[nodiscard]] NeedLoad  needload() const noexcept    { return needload_; }

    void set_type( ModelType t ) noexcept     { type_ = t; }
    void set_needload( NeedLoad n ) noexcept  { needload_ = n; }

    // Inline brush submodels are "*N" — they share the world's data and are
    // never file-loaded or individually freed (boundary quirk).
    [[nodiscard]] bool is_inline_submodel() const noexcept
    {
        return !name_.empty() && name_.front() == '*';
    }

private:
    std::string name_;
    ModelType   type_     = ModelType::Bad;
    NeedLoad    needload_ = NeedLoad::Unreferenced;
};

} // namespace xash::content
