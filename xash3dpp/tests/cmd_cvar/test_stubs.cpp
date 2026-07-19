// xash3dpp — cmd_cvar test stubs
// Provides minimal ITrustOracle, ICompatPolicy implementations
// for use across all cmd_cvar unit tests.

#include "test_stubs.hpp"

namespace xash::cmd_cvar::test {

// TrustOracle that always considers stuffcmd untrusted (default test stance).
bool UntrustedOracle::stuffcmd_is_trusted() const noexcept { return false; }

// TrustOracle that always trusts (singleplayer/local-server simulation).
bool TrustedOracle::stuffcmd_is_trusted() const noexcept { return true; }

// NullPolicy — no quirks active.
const char *NullPolicy::redirect_cvar_name(std::string_view)     const noexcept { return nullptr; }
bool        NullPolicy::is_filterable_exempt(std::string_view)   const noexcept { return false; }
bool        NullPolicy::is_overridable_command(std::string_view) const noexcept { return false; }

} // namespace xash::cmd_cvar::test
