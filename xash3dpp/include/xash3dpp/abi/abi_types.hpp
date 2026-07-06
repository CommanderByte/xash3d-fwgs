#pragma once
// xash3dpp — primitive typedefs shared by the vendored frozen SDK structs
// Legacy reference: common/xash3d_types.h, common/const.h (color24)
//
// These aliases exist so the vendored ABI structs below read like the SDK
// originals while staying self-contained (no repo-root includes).  Widths are
// identical on every supported target: `int` is 32-bit on all GoldSrc/Xash
// platforms, and every other member is explicitly fixed-width.
//
// Legacy type names are retained verbatim (ABI naming exemption — see
// .github/instructions/xash3dpp.instructions.md, Naming Conventions).

#include <cstdint>

// @annotation-exempt: abi-pod — width-frozen primitive typedefs + the color24
// POD are verbatim mirrors of the legacy SDK types; the QN annotation matrix
// does not apply and @thread-safety is a caller contract (decisions-style QN).
namespace xash::abi {

using vec_t    = float;
using vec3_t   = vec_t[3];          // legacy: typedef vec_t vec3_t[3]
using byte     = std::uint8_t;      // legacy: typedef uint8_t byte
using qboolean = std::int32_t;      // legacy: typedef int qboolean

// legacy: common/const.h — 3-byte RGB pack (no alignment padding of its own)
struct color24
{
    byte r, g, b;
};
static_assert( sizeof( color24 ) == 3 );

// legacy: pm_shared/pm_info.h MAX_PHYSINFO_STRING — frozen array dimension
// inside clientdata_t.
inline constexpr int k_max_physinfo_string = 256;

} // namespace xash::abi
