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

#include <cstring>

namespace xash::cmd_cvar {

// ---------------------------------------------------------------------------
// HL25 cvar redirect table
// ---------------------------------------------------------------------------

namespace {

struct CvarRedirect {
    const char *from;
    const char *to;
};

// Keep this table sorted alphabetically by 'from' for easy review.
// Source: engine/common/cvar.c Cvar_FindVar / Cvar_DirectSet quirk sites.
constexpr CvarRedirect kCvarRedirects[] = {
    { "gl_widescreen_yfov", "r_adjust_fov" },
};

// ---------------------------------------------------------------------------
// cl_filterstuffcmd exemption table (ricochet / dod)
// Source: engine/common/cmd.c Cmd_ShouldAllowCommand, #ifdef HACKS_RELATED_HLMODS
// ---------------------------------------------------------------------------

constexpr const char *kFilterableExemptions[] = {
    "slot1", "slot2", "slot3", "slot4", "slot5",
    "slot6", "slot7", "slot8", "slot9", "slot10",
    "cancelselect",
    "+commandmenu", "-commandmenu",
    "+voicerecord", "-voicerecord",
};

// ---------------------------------------------------------------------------
// CMD_OVERRIDABLE list — game DLLs may silently replace these
// Source: engine/common/cmd.c
// ---------------------------------------------------------------------------

constexpr const char *kOverridableCommands[] = {
    "pause",
    "save",
    "load",
    "quit",
    "restart",
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// GoldSrcCompatPolicy
// ---------------------------------------------------------------------------

class GoldSrcCompatPolicy final : public ICompatPolicy {
public:
    const char *redirect_cvar_name(const char *name) const noexcept override
    {
        for (const auto &r : kCvarRedirects) {
            if (std::strcmp(r.from, name) == 0)
                return r.to;
        }
        return nullptr;
    }

    bool is_filterable_exempt(const char *cmd_name) const noexcept override
    {
        for (const char *exempt : kFilterableExemptions) {
            if (std::strcmp(exempt, cmd_name) == 0)
                return true;
        }
        return false;
    }

    bool is_overridable_command(const char *cmd_name) const noexcept override
    {
        for (const char *cmd : kOverridableCommands) {
            if (std::strcmp(cmd, cmd_name) == 0)
                return true;
        }
        return false;
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
