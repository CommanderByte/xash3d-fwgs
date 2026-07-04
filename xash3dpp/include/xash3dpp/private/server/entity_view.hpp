#pragma once
// xash3dpp — EntityView: the Q-20 zero-cost typed accessor facade.
// Decision ref: decisions-architecture.md Q-20 (EDICT_STORE) — the
// ABI-exact edict array is the single store; ENGINE-INTERNAL server code
// reads/writes entvars through this facade, never via `->v.` directly.
// Raw access is confined to src/server/abi/, the pmove bridge, and the
// Chunk 8 save serializer (compliance rule lands at sweep).
//
// Accessors are value-semantic (copies, not references into the store)
// so a future ABI flavor can swap the backing arena behind this seam
// without touching callers.  Entity cross-links stay `edict_t*` at this
// layer — the pointer identity IS the ABI handle.  All accessors compile
// to direct loads/stores on the array.

#include <xash3dpp/abi/edict.hpp>
#include <xash3dpp/utilities/math.hpp>

namespace xash::server {

using Vec3 = ::xash::utilities::Vec3;

[[nodiscard]] inline float vec_axis( const Vec3 &v, int axis ) noexcept
{
    return axis == 0 ? v.x : axis == 1 ? v.y : v.z;
}

[[nodiscard]] inline Vec3 to_vec3( const float ( &a )[3] ) noexcept
{
    return { a[0], a[1], a[2] };
}

inline void store_vec3( float ( &a )[3], const Vec3 &v ) noexcept
{
    a[0] = v.x;
    a[1] = v.y;
    a[2] = v.z;
}

class EntityView
{
public:
    explicit EntityView( ::xash::abi::edict_t *e ) noexcept : e_( e ) {}

    // Raw escape hatch — legal only in the ABI shim, the pmove bridge,
    // and the save serializer (Q-20).
    [[nodiscard]] ::xash::abi::edict_t *raw() const noexcept { return e_; }

    // Legacy SV_IsValidEdict: non-null and not freed.
    [[nodiscard]] bool valid() const noexcept
    {
        return e_ != nullptr && !e_->free;
    }

    // --- vectors --------------------------------------------------------
    [[nodiscard]] Vec3 origin() const noexcept { return to_vec3( e_->v.origin ); }
    [[nodiscard]] Vec3 angles() const noexcept { return to_vec3( e_->v.angles ); }
    [[nodiscard]] Vec3 mins() const noexcept { return to_vec3( e_->v.mins ); }
    [[nodiscard]] Vec3 maxs() const noexcept { return to_vec3( e_->v.maxs ); }
    [[nodiscard]] Vec3 absmin() const noexcept { return to_vec3( e_->v.absmin ); }
    [[nodiscard]] Vec3 absmax() const noexcept { return to_vec3( e_->v.absmax ); }
    [[nodiscard]] Vec3 size() const noexcept { return to_vec3( e_->v.size ); }

    void set_absmin( const Vec3 &v ) noexcept { store_vec3( e_->v.absmin, v ); }
    void set_absmax( const Vec3 &v ) noexcept { store_vec3( e_->v.absmax, v ); }

    // --- scalars ---------------------------------------------------------
    [[nodiscard]] int solid() const noexcept { return e_->v.solid; }
    [[nodiscard]] int skin() const noexcept { return e_->v.skin; }
    [[nodiscard]] int movetype() const noexcept { return e_->v.movetype; }
    [[nodiscard]] int modelindex() const noexcept { return e_->v.modelindex; }
    [[nodiscard]] int groupinfo() const noexcept { return e_->v.groupinfo; }
    [[nodiscard]] int flags() const noexcept { return e_->v.flags; }
    [[nodiscard]] int effects() const noexcept { return e_->v.effects; }
    [[nodiscard]] int rendermode() const noexcept { return e_->v.rendermode; }
    [[nodiscard]] int light_level() const noexcept { return e_->v.light_level; }

    // --- entity cross-links ----------------------------------------------
    [[nodiscard]] ::xash::abi::edict_t *aiment() const noexcept { return e_->v.aiment; }
    [[nodiscard]] ::xash::abi::edict_t *owner() const noexcept { return e_->v.owner; }

private:
    ::xash::abi::edict_t *e_;
};

} // namespace xash::server
