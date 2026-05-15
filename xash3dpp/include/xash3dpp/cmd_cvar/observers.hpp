#pragma once
// xash3dpp — cmd_cvar observer and trust-oracle interfaces
// Legacy reference: engine/common/cvar.c (Cvar_UpdateInfo), engine/common/cmd.c (Cbuf_Execute)
//
// These interfaces break the circular dependency between cmd_cvar and the
// server/client subsystems (see boundary doc D2, D3).
//
// Rules:
//   • All methods are noexcept — no exceptions, no RTTI (engine policy).
//   • Implementations must not call back into CmdCvarContext during a callback
//     (no re-entrant cvar_set from inside on_cvar_changed).
//   • Observer list is populated at init time and never mutated during runtime.

#include <xash3dpp/cmd_cvar/cvar.hpp>

#include <cstdint>

namespace xash::cmd_cvar {

// ---------------------------------------------------------------------------
// ICvarObserver — notified synchronously on the game thread when a cvar
// whose flags intersect the observer's registered flag_mask is written.
//
// Replaces the legacy pattern of Cvar_UpdateInfo calling SV_Serverinfo /
// CL_Userinfo / CL_UpdateInfo directly.
//
// Registration: CmdCvarContext::add_cvar_observer(observer, flag_mask)
//   • flag_mask is a bitwise OR of CvarFlags values.
//   • The observer is only called when the changed cvar's flags & flag_mask != 0.
//   • Typical registrations:
//       server: FCVAR_SERVER | FCVAR_MOVEVARS
//       client: FCVAR_USERINFO
//       host:   FCVAR_VIDRESTART
// ---------------------------------------------------------------------------

struct ICvarObserver {
    // Called after the cvar value has been updated and FCVAR_CHANGED has been set.
    // old_value points to the previous string (valid only for this call's duration).
    virtual void on_cvar_changed(Cvar *cvar, const char *old_value) noexcept = 0;

protected:
    ICvarObserver()          = default;
    virtual ~ICvarObserver() = default;
};

// ---------------------------------------------------------------------------
// ITrustOracle — answers whether the stuffcmd command queue should be
// executed with full privilege in the current game state.
//
// Replaces the legacy inline check:
//   SV_Active() && SV_GetMaxClients() == 1
// inside Cbuf_Execute / Cbuf_ExecStuffCmds.
//
// The server subsystem constructs a concrete implementation and injects it
// into CmdCvarContext at init time.  cmd_cvar never imports server headers.
// ---------------------------------------------------------------------------

struct ITrustOracle {
    // Return true if stuffcmd commands from the server should run as privileged
    // (i.e. a local listen server is running in singleplayer mode).
    [[nodiscard]] virtual bool stuffcmd_is_trusted() const noexcept = 0;

protected:
    ITrustOracle()          = default;
    virtual ~ITrustOracle() = default;
};

} // namespace xash::cmd_cvar
