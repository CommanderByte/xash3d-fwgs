// xash3dpp — lightstyle storage + illumination guards (Chunk 6 S5c)
// Legacy reference: engine/server/sv_world.c :1605-1664
//
// Existing subsystems used:
//   xash3dpp_utilities — bounded string copy (Q_strncpy semantics)
//   xash3dpp_core      — thread-role assertion (OQ-9)

#include <xash3dpp/private/server/lightstyles.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/abi/entity_view.hpp>
#include <xash3dpp/utilities/string.hpp>

namespace xash::server {

namespace {

// SV_ClearWorld resets every style value to full (256).
inline constexpr float k_lightstyle_full_value = 256.0f;

} // namespace

void LightStyles::reset() noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    for ( LightStyle &s : styles_ )
    {
        s        = LightStyle{};
        s.value  = k_lightstyle_full_value;
    }
}

bool LightStyles::set( int style, const char *pattern, float time ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( style < 0 ||
         static_cast<std::size_t>( style ) >=
             ::xash::limits::server_lightstyles )
        return false; // hardening: legacy indexes unchecked

    LightStyle &s = styles_[static_cast<std::size_t>( style )];

    ::xash::utilities::strncpy( s.pattern, pattern, sizeof( s.pattern ));
    s.time = time;

    // Q_strncpy returns the copied length; recompute from the stored,
    // bounded pattern for the same result.
    int length = 0;
    while ( s.pattern[length] != '\0' )
        ++length;
    s.length = length;

    for ( int k = 0; k < length; ++k )
        s.map[k] = static_cast<float>( s.pattern[k] - 'a' );

    return true;
}

void LightStyles::run_frame( float frametime ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    // SV_RunLightStyles (sv_phys.c:1789-1803): map values are 'a'-relative
    // (0..25); the 12.0 divisor yields the legacy normal-brightness scale.
    for ( LightStyle &ls : styles_ )
    {
        ls.time += frametime;
        const int ofs = static_cast<int>( ls.time * 10.0f );

        if ( ls.length == 0 )
            ls.value = 1.0f; // disable this light
        else if ( ls.length == 1 )
            ls.value = ls.map[0] / 12.0f;
        else
            ls.value = ls.map[ofs % ls.length] / 12.0f;
    }
}

int light_for_entity( ::xash::abi::edict_t *ed ) noexcept
{
    const EntityView view( ed );

    if ( !view.valid() )
        return -1;

    if (( view.effects() & ::xash::abi::k_ef_fullbright ) != 0 )
        return 255;

    // XASH3DPP-STUB(chunk6): no LUMP_LIGHTING in WorldData yet — the
    // legacy !worldmodel->lightdata branch always fires from here on
    // (matches legacy behaviour on unlit maps; see lightstyles.hpp).
    return 255;
}

} // namespace xash::server
