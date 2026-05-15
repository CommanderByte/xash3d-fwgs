// xash3dpp — cmd_cvar: CmdCvarContext — TLS definition, ctor/dtor/move.
//
// This file is intentionally small: it is the only TU where Impl is complete
// AND where unique_ptr<Impl> is destructed (standard pimpl rule).
// All method implementations live in context_init.cpp, cvar_ops.cpp,
// cmd_ops.cpp, cmd_dispatch.cpp, and context_misc.cpp.

#include <xash3dpp/private/cmd_cvar/context_impl.hpp>

namespace xash::cmd_cvar {

// Thread-local context pointer — one definition; all other TUs in this
// subsystem see the extern declaration from context_impl.hpp.
thread_local CmdCvarContext *tls_ctx = nullptr;

// ---------------------------------------------------------------------------
// Constructor / destructor / move
// ---------------------------------------------------------------------------

CmdCvarContext::CmdCvarContext() noexcept
    : impl_{ std::make_unique<Impl>() }
{
}

CmdCvarContext::~CmdCvarContext() = default;

// Defined here (not in the header) so that unique_ptr<Impl> is destructed only
// in TUs where Impl is fully defined — the standard pimpl move pattern.
CmdCvarContext::CmdCvarContext(CmdCvarContext &&) noexcept            = default;
CmdCvarContext &CmdCvarContext::operator=(CmdCvarContext &&) noexcept = default;

} // namespace xash::cmd_cvar
