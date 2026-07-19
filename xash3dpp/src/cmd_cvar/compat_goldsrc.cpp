// xash3dpp — cmd_cvar: GoldSrc compatibility policy
// Legacy reference: engine/common/cvar.c (#ifdef HACKS_RELATED_HLMODS),
//                   engine/common/cmd.c  (CMD_OVERRIDABLE list)
//
// Enabled when CMake option XASH_GOLDSRC_COMPAT=1.
// The NullCompatPolicy in compat_null.cpp is linked otherwise.
//
// All quirks live in this single TU.  Core cmd_cvar files contain zero
// #ifdef HACKS_RELATED_HLMODS / #ifdef XASH_GOLDSRC_COMPAT guards (boundary D12).

#include <xash3dpp/private/cmd_cvar/compat_policy.hpp>

#include <array>
#include <string_view>

namespace xash::cmd_cvar {

// ---------------------------------------------------------------------------
// HL25 cvar redirect table
// ---------------------------------------------------------------------------

namespace {

struct CvarRedirect {
    std::string_view from;
    std::string_view to;
};

// Keep this table sorted alphabetically by 'from' for easy review.
// Source: engine/common/cvar.c Cvar_FindVar / Cvar_DirectSet quirk sites.
constexpr std::array<CvarRedirect, 1> kCvarRedirects = {{
    { "gl_widescreen_yfov", "r_adjust_fov" },
}};

// ---------------------------------------------------------------------------
// cl_filterstuffcmd exemption table (ricochet / dod)
// Source: engine/common/cmd.c Cmd_ShouldAllowCommand, #ifdef HACKS_RELATED_HLMODS
// ---------------------------------------------------------------------------

constexpr std::array<std::string_view, 15> kFilterableExemptions = {{
    "slot1", "slot2", "slot3", "slot4", "slot5",
    "slot6", "slot7", "slot8", "slot9", "slot10",
    "cancelselect",
    "+commandmenu", "-commandmenu",
    "+voicerecord", "-voicerecord",
}};

// ---------------------------------------------------------------------------
// CMD_OVERRIDABLE list — game DLLs may silently replace these
// Source: engine/common/cmd.c
// ---------------------------------------------------------------------------

constexpr std::array<std::string_view, 5> kOverridableCommands = {{
    "pause",
    "save",
    "load",
    "quit",
    "restart",
}};

// Returns true if any element of arr is exactly equal to needle.
template<typename Arr>
static constexpr bool cstr_span_contains(const Arr &arr, std::string_view needle) noexcept
{
    for (std::string_view s : arr)
        if (s == needle) return true;
    return false;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// GoldSrcCompatPolicy
// ---------------------------------------------------------------------------

class GoldSrcCompatPolicy final : public ICompatPolicy {
public:
    const char *redirect_cvar_name(std::string_view name) const noexcept override
    {
        for (const auto &r : kCvarRedirects) {
            if (r.from == name)
                return r.to.data();
        }
        return nullptr;
    }

    bool is_filterable_exempt(std::string_view cmd_name) const noexcept override
    {
        return cstr_span_contains(kFilterableExemptions, cmd_name);
    }

    bool is_overridable_command(std::string_view cmd_name) const noexcept override
    {
        return cstr_span_contains(kOverridableCommands, cmd_name);
    }
};

// ---------------------------------------------------------------------------
// Factory — returns the single static instance
// ---------------------------------------------------------------------------

ICompatPolicy &get_compat_policy() noexcept
{
    static GoldSrcCompatPolicy instance;
    return instance;
}

} // namespace xash::cmd_cvar
