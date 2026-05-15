#pragma once
// xash3dpp — CmdCvarContext public API
// Legacy reference: engine/common/cmd.c, cvar.c, base_cmd.c
//
// All command-buffer and cvar-registry state lives in CmdCvarContext.
// The global g_cmd_cvar pointer (set in the ABI shim in the host layer) is
// the only place a singleton reference exists; tests construct local instances.

#include <xash3dpp/cmd_cvar/cvar.hpp>
#include <xash3dpp/cmd_cvar/command.hpp>
#include <xash3dpp/cmd_cvar/observers.hpp>
#include <xash3dpp/limits.hpp>

#include <atomic>
#include <cstdint>
#include <string_view>

namespace xash::cmd_cvar {

// ICompatPolicy is an implementation-detail interface defined in
// include/xash3dpp/private/cmd_cvar/compat_policy.hpp.
// Callers that need to construct one (e.g. the host layer) include that header.
struct ICompatPolicy;

// ---------------------------------------------------------------------------
// CmdCvarInitParams — passed to CmdCvarContext::init()
// ---------------------------------------------------------------------------

struct CmdCvarInitParams {
    // Required: answers whether the stuffcmd queue is currently trusted.
    // The context takes a non-owning pointer; lifetime must exceed the context.
    ITrustOracle  *trust_oracle  = nullptr;

    // Required: routes GoldSrc compat quirks.  Pass a NullCompatPolicy
    // instance when XASH_GOLDSRC_COMPAT is not enabled.
    // The context takes a non-owning pointer; lifetime must exceed the context.
    ICompatPolicy *compat_policy = nullptr;
};

// ---------------------------------------------------------------------------
// CmdCvarStats — lifetime counters exposed for profiling / debug tooling.
// The reference returned by CmdCvarContext::stats() is valid for the
// lifetime of the context.
// ---------------------------------------------------------------------------

struct CmdCvarStats {
#if XASH_STATS
    std::atomic<std::uint64_t> commands_executed { 0 };     // total dispatched commands
    std::atomic<std::uint64_t> commands_dropped  { 0 };     // filtered by privilege check
    std::atomic<std::uint32_t> buffer_high_water { 0 };     // peak command-queue depth (entries)
    std::uint32_t              peak_cvar_count   { 0 };     // max cvars registered at once
    std::uint32_t              peak_command_count{ 0 };     // max commands registered at once
#endif
};

// ---------------------------------------------------------------------------
// CmdCvarContext — owns all command-buffer and cvar-registry state.
// ---------------------------------------------------------------------------

class CmdCvarContext {
public:
    CmdCvarContext() noexcept;
    ~CmdCvarContext();

    CmdCvarContext(const CmdCvarContext &)            = delete;
    CmdCvarContext &operator=(const CmdCvarContext &) = delete;

    // Move is supported (transfers ownership of the pimpl pointer).
    // Defined in context.cpp where Impl is complete (pimpl rule).
    CmdCvarContext(CmdCvarContext &&) noexcept;
    CmdCvarContext &operator=(CmdCvarContext &&) noexcept;

    // ---- Lifecycle --------------------------------------------------------

    [[nodiscard]] bool init(const CmdCvarInitParams &params) noexcept;
    void               shutdown() noexcept;

    // ---- Cvar registry ----------------------------------------------------

    // Look up a cvar by name (case-insensitive). Returns nullptr if not found.
    [[nodiscard]] Cvar *cvar_find(std::string_view name) noexcept; // @lifetime: engine

    // Look up or create a cvar.  If the name does not exist, a new cvar is
    // created with FCVAR_USER_CREATED.  Never returns nullptr after a
    // successful init().
    [[nodiscard]] Cvar *cvar_get_or_create(std::string_view name,
                             const char *default_value,
                             std::uint32_t flags) noexcept; // @lifetime: engine

    // Register an engine-owned cvar (defined in a Cvar static declared in the
    // engine source).  Idempotent: no-op if the name is already registered.
    void cvar_register_engine(Cvar &cv) noexcept;

    // Register a cvar whose cvar_t-compatible struct is owned by a legacy DLL.
    // The pointer is treated as a CvarAbi* (first-five-field layout only).
    // Returns the same pointer cast to Cvar* (no new allocation).
    [[nodiscard]] Cvar *cvar_register_dll(CvarAbi *cv) noexcept; // @lifetime: engine

    // Set a cvar's string value.
    void cvar_set(std::string_view name,
                  const char *value,
                  CvarWriteSource source = CvarWriteSource::EngineInternal) noexcept;

    // Set directly on a known Cvar pointer (avoids a second hash lookup).
    void cvar_set_direct(Cvar *cv,
                         const char *value,
                         CvarWriteSource source = CvarWriteSource::EngineInternal) noexcept;

    // Unlink all cvars whose owner_flags intersects mask.
    // Called on DLL unload to remove server/client/menu cvars.
    void cvar_unlink(std::uint32_t owner_flags_mask) noexcept;

    // Build a copyable descriptor snapshot for the given cvar.
    [[nodiscard]] CvarDesc cvar_describe(const Cvar *cv) const noexcept;

    // Read helpers — return sensible defaults when name is not found.
    [[nodiscard]] const char *cvar_variable_string (std::string_view name) const noexcept;
    [[nodiscard]] float       cvar_variable_value   (std::string_view name) const noexcept;
    [[nodiscard]] int         cvar_variable_integer (std::string_view name) const noexcept;

    // Force-set: updates value AND flags, bypasses privilege/cheat/latch.
    void cvar_full_set(std::string_view name, const char *value,
                       std::uint32_t flags) noexcept;

    // Reset all FCVAR_CHEAT cvars to their registered default values.
    void cvar_set_cheat_state() noexcept;

    // Returns the head of the ABI linked list (for legacy Cvar_GetList()).
    [[nodiscard]] CvarAbi *cvar_get_list() const noexcept; // @lifetime: engine

    // Persist all FCVAR_ARCHIVE cvars (filtered by owner_flags_mask; 0 = all)
    // to an open filesystem VFile handle (opaque void* to avoid a header dep).
    void cvar_write_variables(void *vfile,
                              std::uint32_t owner_flags_mask) noexcept;

    // Safe DLL-unlink: call cvar_prepare_to_unlink() BEFORE the DLL is
    // unloaded (while its cvar structs are still valid), then unload the DLL,
    // then call unlink_pending_cvars().  Handles the case where the DLL freed
    // its cvar structs without notifying the engine.
    void cvar_prepare_to_unlink(std::uint32_t owner_flags_mask) noexcept;
    void unlink_pending_cvars() noexcept;

    // ---- Command registry -------------------------------------------------

    void cmd_add(std::string_view name,
                 CommandFn      fn,
                 std::uint32_t  flags = 0,
                 const char    *desc  = nullptr) noexcept;

    void cmd_remove(std::string_view name) noexcept;

    // Unlink all commands whose flags intersect mask (mirrors cvar_unlink).
    void cmd_unlink(std::uint32_t flags_mask) noexcept;

    // Build a copyable descriptor snapshot for a command by name.
    [[nodiscard]] CommandDesc cmd_describe(std::string_view name) const noexcept;

    // Returns true if a command with this name is registered.
    [[nodiscard]] bool cmd_exists(std::string_view name) const noexcept;

    // Execute a single command line immediately in the privileged context.
    void cmd_execute_string(std::string_view text) noexcept;

    // ---- Command buffer ---------------------------------------------------

    // Append text to the end of the command queue (runs after current frame).
    void cbuf_add_text(std::string_view text) noexcept;

    // Insert text at the front of the command queue (runs before any queued cmds).
    void cbuf_insert_text(std::string_view text) noexcept;

    // Append text to the unprivileged stuffcmd queue.
    void cbuf_stuff_text(std::string_view text) noexcept;

    // Execute pending commands from both queues.
    // Privilege: stuffcmd queue only runs privileged if ITrustOracle says so.
    // Stops when cmd_wait > 0 (decremented once per call).
    void cbuf_execute() noexcept;

    // Discard all pending commands from both queues.
    void cbuf_clear() noexcept;

    // ---- Tokenizer accessors (valid only during cbuf_execute dispatch) -----
    // Calling these outside of a command dispatch returns 0 / "" / false.

    [[nodiscard]] int         cmd_argc()                 const noexcept;
    [[nodiscard]] const char *cmd_argv(int i)            const noexcept;
    [[nodiscard]] const char *cmd_args()                 const noexcept;
    [[nodiscard]] bool        cmd_current_is_privileged() const noexcept;

    // ---- DLL lifecycle (called by the host layer) -------------------------

    // Tell the context whether a game-server or client DLL is currently loaded.
    // cvar_unlink / cmd_unlink refuse to run while the matching DLL is loaded
    // (prevents double-unlink during DLL reload).
    void set_server_dll_loaded(bool loaded) noexcept;
    void set_client_dll_loaded(bool loaded) noexcept;

    // ---- Observer registration (call before init or immediately after) ----

    // Register an observer.  Called when a changed cvar's flags & flag_mask != 0.
    // Maximum registrations: limits::cmd_observer_max.
    void add_cvar_observer(ICvarObserver *observer, std::uint32_t flag_mask) noexcept;

    // ---- Statistics -------------------------------------------------------

    [[nodiscard]] const CmdCvarStats &stats() const noexcept;

    // ---- Debug (XASH_DEBUG_CVARS only) ------------------------------------

#if XASH_DEBUG_CVARS
    // Trigger XASH_DEBUG_BREAK() the next time cvar_name is written.
    // Pass nullptr to clear.
    void debug_break_on_cvar_write(const char *cvar_name) noexcept;

    // Print hash-bucket fill statistics to the platform console.
    // Use to tune limits::cvar_hash_buckets for large mods.
    void dump_hash_stats() const noexcept;
#endif

private:
    struct Impl;
    // Allocated via memory::pool_new(k_null_pool) in the constructor so that OOM
    // is handled through the memory subsystem rather than throwing bad_alloc.
    // k_null_pool: the cmd_cvar pool does not yet exist when the Impl is created
    // (it is created inside init()); the Impl is the struct that holds the pool
    // handle, so it cannot itself be pool-allocated from that pool.
    Impl *impl_ = nullptr;
};

} // namespace xash::cmd_cvar
