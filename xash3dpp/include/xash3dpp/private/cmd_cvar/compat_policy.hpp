#pragma once
// xash3dpp — ICompatPolicy: GoldSrc compatibility quirks interface (PRIVATE)
//
// This header is NOT part of the cmd_cvar public API.  Only the host layer
// (which chooses between GoldSrcCompatPolicy and NullCompatPolicy at link time)
// and the two compat TUs themselves should include it.
//
// Core cmd_cvar files forward-declare ICompatPolicy via context.hpp and
// never include this header directly.
//
// Implementations:
//   src/cmd_cvar/compat_goldsrc.cpp — enabled when XASH_GOLDSRC_COMPAT=1
//   src/cmd_cvar/compat_null.cpp    — enabled when XASH_GOLDSRC_COMPAT=0
//
// See boundary doc D12 for the full isolation rationale.

#include <cstdint>
#include <string_view>

namespace xash::cmd_cvar {

// All name parameters are bounded std::string_view (HB-1/M-5): callers may
// pass slices of larger buffers; implementations must never assume a NUL at
// data()+size(). Q-17 signature change 2026-07-19 — impls: compat_goldsrc,
// compat_null, tests/cmd_cvar/test_stubs; callers: cvar_ops (find /
// get_or_create), cmd_ops (cmd_add).
struct ICompatPolicy {
    // Return a replacement cvar name if this name should be silently redirected.
    // Example: "gl_widescreen_yfov" → "r_adjust_fov" (HL25 quirk).
    // Return nullptr if no redirect applies; a non-null return is a static
    // NUL-terminated string.
    [[nodiscard]] virtual const char *redirect_cvar_name(std::string_view name) const noexcept = 0;

    // Return true if this command name is in the GoldSrc HL-mod exemption table
    // (ricochet / dod cl_filterstuffcmd bypass).
    [[nodiscard]] virtual bool is_filterable_exempt(std::string_view cmd_name) const noexcept = 0;

    // Return true if the cmd_overridable flag should be applied to the named command.
    // The GoldSrc implementation uses the CMD_OVERRIDABLE list from engine code.
    [[nodiscard]] virtual bool is_overridable_command(std::string_view cmd_name) const noexcept = 0;

protected:
    ICompatPolicy()          = default;
    virtual ~ICompatPolicy() = default;
};

} // namespace xash::cmd_cvar
