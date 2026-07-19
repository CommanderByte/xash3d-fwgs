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

#include <cstdint>
#include <span>

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

    // edict_t::free query through the facade (edict metadata, not entvars —
    // keeps the raw ->free access out of engine-internal server code).
    [[nodiscard]] bool freed() const noexcept { return e_->free != 0; }

    // --- vectors --------------------------------------------------------
    [[nodiscard]] Vec3 origin() const noexcept { return to_vec3( e_->v.origin ); }
    [[nodiscard]] Vec3 angles() const noexcept { return to_vec3( e_->v.angles ); }
    [[nodiscard]] Vec3 mins() const noexcept { return to_vec3( e_->v.mins ); }
    [[nodiscard]] Vec3 maxs() const noexcept { return to_vec3( e_->v.maxs ); }
    [[nodiscard]] Vec3 absmin() const noexcept { return to_vec3( e_->v.absmin ); }
    [[nodiscard]] Vec3 absmax() const noexcept { return to_vec3( e_->v.absmax ); }
    [[nodiscard]] Vec3 size() const noexcept { return to_vec3( e_->v.size ); }
    [[nodiscard]] Vec3 velocity() const noexcept { return to_vec3( e_->v.velocity ); }
    [[nodiscard]] Vec3 avelocity() const noexcept { return to_vec3( e_->v.avelocity ); }
    [[nodiscard]] Vec3 basevelocity() const noexcept { return to_vec3( e_->v.basevelocity ); }
    [[nodiscard]] Vec3 v_angle() const noexcept { return to_vec3( e_->v.v_angle ); }
    [[nodiscard]] Vec3 oldorigin() const noexcept { return to_vec3( e_->v.oldorigin ); }
    [[nodiscard]] Vec3 movedir() const noexcept { return to_vec3( e_->v.movedir ); }
    [[nodiscard]] Vec3 view_ofs() const noexcept { return to_vec3( e_->v.view_ofs ); }

    void set_absmin( const Vec3 &v ) noexcept { store_vec3( e_->v.absmin, v ); }
    void set_absmax( const Vec3 &v ) noexcept { store_vec3( e_->v.absmax, v ); }
    void set_origin( const Vec3 &v ) noexcept { store_vec3( e_->v.origin, v ); }
    void set_angles( const Vec3 &v ) noexcept { store_vec3( e_->v.angles, v ); }
    void set_mins( const Vec3 &v ) noexcept { store_vec3( e_->v.mins, v ); }
    void set_maxs( const Vec3 &v ) noexcept { store_vec3( e_->v.maxs, v ); }
    void set_velocity( const Vec3 &v ) noexcept { store_vec3( e_->v.velocity, v ); }
    void set_avelocity( const Vec3 &v ) noexcept { store_vec3( e_->v.avelocity, v ); }
    void set_basevelocity( const Vec3 &v ) noexcept { store_vec3( e_->v.basevelocity, v ); }
    void set_oldorigin( const Vec3 &v ) noexcept { store_vec3( e_->v.oldorigin, v ); }
    void set_v_angle( const Vec3 &v ) noexcept { store_vec3( e_->v.v_angle, v ); }

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
    [[nodiscard]] int waterlevel() const noexcept { return e_->v.waterlevel; }
    [[nodiscard]] int watertype() const noexcept { return e_->v.watertype; }
    [[nodiscard]] int deadflag() const noexcept { return e_->v.deadflag; }
    [[nodiscard]] int fixangle() const noexcept { return e_->v.fixangle; }
    [[nodiscard]] float gravity() const noexcept { return e_->v.gravity; }
    [[nodiscard]] float friction() const noexcept { return e_->v.friction; }
    [[nodiscard]] float health() const noexcept { return e_->v.health; }
    [[nodiscard]] float speed() const noexcept { return e_->v.speed; }
    [[nodiscard]] float nextthink() const noexcept { return e_->v.nextthink; }
    [[nodiscard]] float ltime() const noexcept { return e_->v.ltime; }
    [[nodiscard]] float dmg() const noexcept { return e_->v.dmg; }
    [[nodiscard]] float dmgtime() const noexcept { return e_->v.dmgtime; }
    [[nodiscard]] float air_finished() const noexcept { return e_->v.air_finished; }
    [[nodiscard]] float pain_finished() const noexcept { return e_->v.pain_finished; }
    [[nodiscard]] float radsuit_finished() const noexcept { return e_->v.radsuit_finished; }

    // --- studio pose (OQ-2 hull gating + the studio pfns) -----------------
    [[nodiscard]] float frame() const noexcept { return e_->v.frame; }
    [[nodiscard]] int   sequence() const noexcept { return e_->v.sequence; }
    [[nodiscard]] int   gamestate() const noexcept { return e_->v.gamestate; }
    [[nodiscard]] std::span<const std::uint8_t, 4> controller() const noexcept
    {
        return std::span<const std::uint8_t, 4>( e_->v.controller, 4 );
    }
    [[nodiscard]] std::span<const std::uint8_t, 2> blending() const noexcept
    {
        return std::span<const std::uint8_t, 2>( e_->v.blending, 2 );
    }

    void set_solid( int v ) noexcept { e_->v.solid = v; }
    void set_movetype( int v ) noexcept { e_->v.movetype = v; }
    void set_modelindex( int v ) noexcept { e_->v.modelindex = v; }
    void set_flags( int v ) noexcept { e_->v.flags = v; }
    void set_effects( int v ) noexcept { e_->v.effects = v; }
    void set_waterlevel( int v ) noexcept { e_->v.waterlevel = v; }
    void set_watertype( int v ) noexcept { e_->v.watertype = v; }
    void set_fixangle( int v ) noexcept { e_->v.fixangle = v; }
    void set_friction( float v ) noexcept { e_->v.friction = v; }
    void set_nextthink( float v ) noexcept { e_->v.nextthink = v; }
    void set_ltime( float v ) noexcept { e_->v.ltime = v; }
    void set_dmg( float v ) noexcept { e_->v.dmg = v; }
    void set_dmgtime( float v ) noexcept { e_->v.dmgtime = v; }
    void set_air_finished( float v ) noexcept { e_->v.air_finished = v; }
    void set_pain_finished( float v ) noexcept { e_->v.pain_finished = v; }

    // Flag convenience (legacy SetBits/ClearBits on entvars flags).
    void add_flags( int bits ) noexcept { e_->v.flags |= bits; }
    void clear_flags( int bits ) noexcept { e_->v.flags &= ~bits; }

    // client-slot bookkeeping (SV_PutClientInServer / SV_FakeConnect).
    // set_flags/add_flags come from the physics accessor block above.
    [[nodiscard]] int colormap() const noexcept { return e_->v.colormap; }
    void set_colormap( int v ) noexcept { e_->v.colormap = v; }

    // --- string_t fields (offsets into the server string pool) -----------
    [[nodiscard]] ::xash::abi::string_t classname() const noexcept
    {
        return e_->v.classname;
    }
    void set_classname( ::xash::abi::string_t s ) noexcept { e_->v.classname = s; }
    void set_model( ::xash::abi::string_t s ) noexcept { e_->v.model = s; }
    [[nodiscard]] ::xash::abi::string_t netname() const noexcept { return e_->v.netname; }
    void set_netname( ::xash::abi::string_t s ) noexcept { e_->v.netname = s; }

    // --- entity cross-links ----------------------------------------------
    [[nodiscard]] ::xash::abi::edict_t *aiment() const noexcept { return e_->v.aiment; }
    [[nodiscard]] ::xash::abi::edict_t *owner() const noexcept { return e_->v.owner; }
    [[nodiscard]] ::xash::abi::edict_t *groundentity() const noexcept { return e_->v.groundentity; }
    void set_groundentity( ::xash::abi::edict_t *g ) noexcept { e_->v.groundentity = g; }

private:
    ::xash::abi::edict_t *e_; // @lifetime: arena (viewed edict; facade is non-owning)
};

} // namespace xash::server
