#pragma once
// xash3dpp — server lightstyle table + entity illumination
// Legacy reference: engine/server/sv_world.c — SV_ClearWorld lightstyle
// reset (:473-477), SV_SetLightStyle (:1612), SV_LightForEntity (:1639),
// SV_RecursiveLightPoint (:1516); common/lightstyle.h (lightstyle_t).
//
// SV_SetLightStyle's svc_lightstyle broadcast belongs to the message
// layer (S9): the caller broadcasts after set() when the server is
// active.  The lightmap sample walk needs LUMP_LIGHTING data that
// map_loader does not load yet — see light_for_entity.
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/limits.hpp>
#include <xash3dpp/private/server/world_trace.hpp>

#include <cstddef>

namespace xash::server {

struct LightStyle
{
    char  pattern[::xash::limits::server_lightstyle_pattern]{};
    float map[::xash::limits::server_lightstyle_pattern]{};
    int   length = 0;
    float value  = 0.0f;
    float time   = 0.0f;
};

class LightStyles
{
public:
    LightStyles() { reset(); }

    // SV_ClearWorld: every style back to full value (256), time 0.
    void reset() noexcept;

    // SV_SetLightStyle storage half: pattern copy + 'a'-relative map.
    // Returns false on an out-of-range style index (legacy indexes
    // unchecked — hardening).
    bool set( int style, const char *pattern, float time ) noexcept;

    [[nodiscard]] const LightStyle *style( int index ) const noexcept
    {
        if ( index < 0 ||
             static_cast<std::size_t>( index ) >=
                 ::xash::limits::server_lightstyles )
            return nullptr;
        return &styles_[static_cast<std::size_t>( index )];
    }

private:
    LightStyle styles_[::xash::limits::server_lightstyles];
};

// SV_LightForEntity: -1 for invalid edicts; 255 for EF_FULLBRIGHT.
// XASH3DPP-STUB(chunk6): map_loader does not load LUMP_LIGHTING, so the
// no-lightdata branch (legacy → 255) always fires past the guards —
// including for players, exactly as legacy behaves on unlit maps.  Real
// lightmap sampling (SV_RecursiveLightPoint + FL_CLIENT light_level) is
// a tracked follow-up gated on a map_loader lighting-lump extension.
[[nodiscard]] int light_for_entity( ::xash::abi::edict_t *ed ) noexcept;

} // namespace xash::server
