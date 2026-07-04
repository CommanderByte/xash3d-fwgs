// xash3dpp — server ban filters: ID (uuid) + IP/CIDR (Chunk 6 S9 satellite)
// Legacy reference: engine/server/sv_filter.c — SV_CheckID (:62), SV_BanID_f
// (:91), SV_RemoveID (:36), SV_CheckIP (:352), SV_AddIP_f (:400), the
// NET_CompareAdrByMask inclusion logic.
// Deep dive: docs/legacy-survey/deep-dive-server-clients.md §6 (sv_filter.c).
//
// Reuses the networking address layer's mask_compare (NET_CompareAdrByMask)
// so the CIDR containment semantics — incl. the IPv4/IPv6 split — stay in one
// place.  Bug-compat kept: SV_CheckID is a mutual-prefix (min-length) match
// with lazy expiry pruning; addip minutes < 0.1 ⇒ permanent.  The latent
// SV_RemoveIPFilter use-after-free is NOT reproduced (semantics only).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <xash3dpp/private/server/clients.hpp>

#include <xash3dpp/networking/address.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cstring>

namespace xash::server {

namespace {

namespace ut  = ::xash::utilities;
namespace net = ::xash::networking;

[[nodiscard]] std::size_t min_size( std::size_t a, std::size_t b ) noexcept
{
    return a < b ? a : b;
}

} // namespace

// --- ID filter --------------------------------------------------------------

bool filter_check_id( ClientMachinery &cm, const char *id ) noexcept
{
    if ( id == nullptr || id[0] == '\0' )
        return false;

    const std::size_t id_len = ut::strlen( id );

    for ( IdBan &f : cm.filters.id_bans )
    {
        if ( !f.used )
            continue;

        // lazy expiry prune (legacy prunes mid-walk).
        if ( f.endtime != 0.0 && cm.realtime > f.endtime )
        {
            f = IdBan{};
            continue;
        }

        const std::size_t len = min_size( id_len, ut::strlen( f.id ) );
        if ( len != 0 && std::strncmp( id, f.id, len ) == 0 )
            return true;
    }

    return false;
}

void filter_add_id( ClientMachinery &cm, float minutes, const char *id ) noexcept
{
    if ( id == nullptr || id[0] == '\0' )
        return;

    const double endtime =
        minutes != 0.0f ? cm.realtime + static_cast<double>( minutes ) * 60.0
                        : 0.0;

    // refresh an existing exact entry, else claim a free slot.
    IdBan *free_slot = nullptr;
    for ( IdBan &f : cm.filters.id_bans )
    {
        if ( f.used && ut::strcmp( f.id, id ) == 0 )
        {
            f.endtime = endtime;
            return;
        }
        if ( !f.used && free_slot == nullptr )
            free_slot = &f;
    }

    if ( free_slot == nullptr )
        return; // table full (legacy is an unbounded list; we cap)

    free_slot->used    = true;
    free_slot->endtime = endtime;
    ut::strncpy( free_slot->id, id, sizeof( free_slot->id ) );
}

void filter_remove_id( ClientMachinery &cm, const char *id ) noexcept
{
    if ( id == nullptr )
        return;
    for ( IdBan &f : cm.filters.id_bans )
    {
        if ( f.used && ut::strcmp( f.id, id ) == 0 )
        {
            f = IdBan{};
            return;
        }
    }
}

// --- IP / CIDR filter -------------------------------------------------------

bool filter_check_ip( ClientMachinery &cm, net::NetAddress adr ) noexcept
{
    for ( IpBan &f : cm.filters.ip_bans )
    {
        if ( !f.used )
            continue;

        // expired entries are skipped but not removed (legacy SV_CheckIP).
        if ( f.endtime != 0.0 && cm.realtime > f.endtime )
            continue;

        if ( net::mask_compare( adr, f.adr, f.prefix ) )
            return true;
    }
    return false;
}

void filter_add_ip( ClientMachinery &cm, float minutes, net::NetAddress adr,
                    std::uint8_t cidr ) noexcept
{
    if ( cidr == 0 )
        cidr = adr.family == net::IpFamily::V6 ? 128 : 32;

    const double endtime =
        minutes < 0.1f ? 0.0 // permanent (legacy: < 0.1 min)
                       : cm.realtime + static_cast<double>( minutes ) * 60.0;

    IpBan *free_slot = nullptr;
    for ( IpBan &f : cm.filters.ip_bans )
    {
        if ( f.used && f.prefix == cidr && net::compare_base( f.adr, adr ) )
        {
            f.endtime = endtime;
            return;
        }
        if ( !f.used && free_slot == nullptr )
            free_slot = &f;
    }

    if ( free_slot == nullptr )
        return;

    free_slot->used    = true;
    free_slot->endtime = endtime;
    free_slot->adr     = adr;
    free_slot->prefix  = cidr;

    // XASH3DPP-STUB(S8-seam): legacy addip immediately drops every connected
    // client matching the mask (sv_filter.c:436-445).  That needs drop_client
    // over the live client array + the packet loop the host owns; the host
    // frame path re-checks filter_check_ip on the next SV_CheckTimeouts pass.
}

void filter_remove_ip( ClientMachinery &cm, net::NetAddress adr,
                       std::uint8_t cidr ) noexcept
{
    if ( cidr == 0 )
        cidr = adr.family == net::IpFamily::V6 ? 128 : 32;

    // removeip drops every filter *included by* the argument (legacy semantics
    // without the use-after-free).
    for ( IpBan &f : cm.filters.ip_bans )
    {
        if ( f.used && f.prefix >= cidr && net::mask_compare( f.adr, adr, cidr ) )
            f = IpBan{};
    }
}

} // namespace xash::server
