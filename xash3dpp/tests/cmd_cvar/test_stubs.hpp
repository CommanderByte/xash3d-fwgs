#pragma once
// xash3dpp — cmd_cvar test stub types
// Include this in each test file instead of re-declaring the stubs.

#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/private/cmd_cvar/compat_policy.hpp>

namespace xash::cmd_cvar::test {

struct UntrustedOracle final : ITrustOracle {
    bool stuffcmd_is_trusted() const noexcept override;
};

struct TrustedOracle final : ITrustOracle {
    bool stuffcmd_is_trusted() const noexcept override;
};

struct NullPolicy final : ICompatPolicy {
    const char *redirect_cvar_name(std::string_view name)         const noexcept override;
    bool        is_filterable_exempt(std::string_view cmd_name)   const noexcept override;
    bool        is_overridable_command(std::string_view cmd_name) const noexcept override;
};

// Convenience: build an initialised context for a test.
inline CmdCvarContext make_test_context(ITrustOracle  &oracle,
                                        ICompatPolicy &policy)
{
    CmdCvarContext ctx;
    const bool ok = ctx.init({ &oracle, &policy });
    (void)ok; // in tests, init failure will manifest as nullptr returns in individual checks
    return ctx;
}

} // namespace xash::cmd_cvar::test
