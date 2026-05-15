// xash3dpp — cmd_cvar: CmdCvarContext — context object and pimpl body
// Legacy reference: engine/common/cmd.c, cvar.c, base_cmd.c (file-scope globals)
//
// Existing subsystems used:
//   xash3dpp_memory    — pool_handle for all subsystem allocations
//   xash3dpp_utilities — utilities::stricmp, hash helpers
//   xash3dpp_platform  — platform::console output

#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/private/cmd_cvar/compat_policy.hpp>
#include <xash3dpp/private/cmd_cvar/circular_buffer.hpp>
#include <xash3dpp/private/cmd_cvar/cmd_hash_map.hpp>
#include <xash3dpp/private/cmd_cvar/registry_types.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/platform/console.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <atomic>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace xash::cmd_cvar {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Pool-duplicate a NUL-terminated string.  Returns nullptr on OOM or src==nullptr.
static char *pool_dup(memory::PoolHandle pool, const char *src) noexcept
{
    if (!src) return nullptr;
    const std::size_t n = utilities::strlen(src) + 1;
    char *dst = static_cast<char *>(memory::mem_alloc(pool, n));
    if (!dst) return nullptr;
    std::memcpy(dst, src, n);
    return dst;
}

// ---------------------------------------------------------------------------
// Impl definition
// ---------------------------------------------------------------------------

struct CmdCvarContext::Impl {
    memory::PoolHandle pool;

    // Injected dependencies (non-owning)
    ITrustOracle  *trust_oracle  = nullptr;
    ICompatPolicy *compat_policy = nullptr;

    // Observer table: fixed at init, never mutated during runtime.
    struct ObserverEntry {
        ICvarObserver *observer;
        std::uint32_t  flag_mask;
    };
    ObserverEntry observers[limits::cmd_observer_max] {};
    std::size_t   observer_count { 0 };

    // Command queues (deque provides O(1) push_front / push_back)
    std::deque<std::string> cmd_text;        // trusted queue
    std::deque<std::string> filteredcmd_text;// stuffcmd (unprivileged) queue

    // Scripting state
    int           cmd_wait      { 0 };  // frame-skip counter for 'wait' command
    int           condlevel     { 0 };  // nesting depth for if/else blocks
    std::uint32_t cmd_condition { 0 };  // bitmask of active condition results

    // Stats
    CmdCvarStats stats_block;

    // ---------------------------------------------------------------------------
    // Registry — populated during init(); torn down during shutdown().
    // ---------------------------------------------------------------------------

    // Cvar registry
    CmdHashMap<Cvar>   cvar_map;                 // case-insensitive lookup
    Cvar              *cvar_list_head { nullptr }; // ABI linked list (CvarAbi.next chain)

    // Command registry
    CmdHashMap<Command>   cmd_map;
    Command              *cmd_list_head { nullptr };

    // Alias registry
    CmdHashMap<AliasDef>  alias_map;
    AliasDef             *alias_list_head { nullptr };

    // Tokenizer scratch — valid only while cmd_dispatch_line() is on the call stack.
    // Written by cbuf_execute before invoking a CommandFn; reset after.
    // Built-in commands access these via the TLS context pointer (see below).
    static constexpr int k_max_argc = static_cast<int>(limits::cmd_tokens_max);
    int         tok_argc              { 0 };
    const char *tok_argv[k_max_argc]  {};
    char        tok_argsBuffer[limits::cmd_line_max] {};
    bool        tok_is_privileged     { false };

    // DLL lifecycle flags — set by the host layer via CmdCvarContext::set_*_dll_loaded().
    // cvar_unlink / cmd_unlink check these to prevent double-unlink during reload.
    bool server_dll_loaded { false };
    bool client_dll_loaded { false };

    // Pending safe-unlink list: populated by cvar_prepare_to_unlink() before DLL
    // unload while cvar structs are still valid; consumed by unlink_pending_cvars().
    struct PendingUnlinkEntry {
        const char   *name;        // pool-owned copy — remains valid after DLL frees its struct
        std::uint32_t owner_flags;
    };
    std::vector<PendingUnlinkEntry> pending_unlink;

#if XASH_DEBUG_CVARS
    // Break-on-write cvar name.  Set via debug_break_on_cvar_write().
    const char *break_on_write_name { nullptr };

    // Circular change log.
    detail::CircularBuffer<CvarChangeRecord, limits::cvar_change_log_capacity> change_log;
#endif
};

// ---------------------------------------------------------------------------
// Built-in command handler design
// ---------------------------------------------------------------------------
//
// CommandFn = void(*)() is a plain function pointer with no context parameter
// (frozen ABI — game DLLs register commands via pfnAddCommand with this type).
//
// Built-in engine commands (echo, wait, alias, exec, ...) need access to the
// context to read Cmd_Argv() and call other methods.  The solution is a
// thread-local pointer set by cbuf_execute() before invoking any CommandFn,
// cleared after.  Built-in handlers are registered as non-capturing lambdas
// (which decay to CommandFn) that read the TLS pointer:
//
//   cmd_add("echo", []() noexcept {
//       const char *msg = tls_ctx->cmd_args();
//       // ...
//   });
//
// Since all dispatch is game-thread-only, no synchronisation is needed.

namespace {
thread_local CmdCvarContext *tls_ctx = nullptr;

// ---------------------------------------------------------------------------
// Storage for built-in (engine-owned) cvars.
// Lifetime: process; re-initialized on each CmdCvarContext::init() call.
// CvarAbi::name/string are char* for legacy ABI — we cast from literals here.
// ---------------------------------------------------------------------------
namespace builtin_cvars {
constexpr char k_scripting_name[]  = "cmd_scripting";
constexpr char k_scripting_def[]   = "0";
constexpr char k_filter_name[]     = "cl_filterstuffcmd";
constexpr char k_filter_def[]      = "1";

Cvar g_cmd_scripting    {};
Cvar g_cl_filterstuffcmd {};
} // namespace builtin_cvars
} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructor / destructor
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

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool CmdCvarContext::init(const CmdCvarInitParams &params) noexcept
{
    impl_->pool = memory::create_pool("cmd_cvar");
    if (!impl_->pool)
        return false;

    impl_->trust_oracle  = params.trust_oracle;
    impl_->compat_policy = params.compat_policy;

    // Wire the hash maps to the subsystem pool now that it exists.
    impl_->cvar_map.set_pool(impl_->pool);
    impl_->cmd_map.set_pool(impl_->pool);
    impl_->alias_map.set_pool(impl_->pool);

    // ---------------------------------------------------------------------------
    // Built-in cvars
    // ---------------------------------------------------------------------------
    // Set individual fields; never aggregate-assign a Cvar (std::atomic member
    // deletes the copy-assignment operator).  The generation counter is reset to
    // 0 via store() so re-init after shutdown begins with a clean epoch.

    {
        using namespace builtin_cvars;
        Cvar &sc = g_cmd_scripting;
        sc.abi.name   = const_cast<char *>(k_scripting_name);
        sc.abi.string = const_cast<char *>(k_scripting_def);
        sc.abi.flags  = FCVAR_ARCHIVE | FCVAR_PRIVILEGED;
        sc.abi.value  = 0.0f;
        sc.abi.next   = nullptr;
        sc.desc       = "enable simple condition checking and variable operations";
        sc.def_string = k_scripting_def;
        sc.generation.store(0, std::memory_order_relaxed);
        cvar_register_engine(sc);

        Cvar &fc = g_cl_filterstuffcmd;
        fc.abi.name   = const_cast<char *>(k_filter_name);
        fc.abi.string = const_cast<char *>(k_filter_def);
        fc.abi.flags  = FCVAR_ARCHIVE | FCVAR_PRIVILEGED;
        fc.abi.value  = 1.0f;
        fc.abi.next   = nullptr;
        fc.desc       = "filter commands coming from server";
        fc.def_string = k_filter_def;
        fc.generation.store(0, std::memory_order_relaxed);
        cvar_register_engine(fc);
    }

    // ---------------------------------------------------------------------------
    // Built-in commands
    //
    // All handlers are non-capturing lambdas — they read context state via the
    // thread-local tls_ctx pointer set by cbuf_execute() before each dispatch.
    // ---------------------------------------------------------------------------

    // echo — print arguments to the console separated by spaces.
    cmd_add("echo", []() noexcept {
        // TODO: iterate tls_ctx->cmd_argc() / cmd_argv() and print to platform console
    }, 0, "print a message to the console (useful in scripts)");

    // wait — skip the rest of the command buffer for N frames.
    cmd_add("wait", []() noexcept {
        if (!tls_ctx) return;
        int frames = (tls_ctx->cmd_argc() > 1) ? utilities::atoi(tls_ctx->cmd_argv(1)) : 1;
        if (frames < 1) frames = 1;
        tls_ctx->impl_->cmd_wait = frames;
    }, 0, "delay command buffer execution by N frames (default: 1)");

    // alias — create or list command aliases.
    cmd_add("alias", []() noexcept {
        // TODO: use tls_ctx to read argc/argv; create/update AliasDef in alias_map
    }, 0, "create a command alias");

    // unalias — remove an alias.
    cmd_add("unalias", []() noexcept {
        // TODO: use tls_ctx->cmd_argv(1) to remove from alias_map
    }, 0, "remove a command alias");

    // stuffcmds — replay +cmd arguments from the host command line.
    cmd_add("stuffcmds", []() noexcept {
        // TODO: host layer injects this; stub until host integration
    }, 0, "execute command-line + arguments");

    // exec — execute a .cfg file (requires filesystem subsystem).
    cmd_add("exec", []() noexcept {
        // TODO: depends on xash3dpp_filesystem; stub until that subsystem exists
    }, 0, "execute a script file");

    return true;
}

void CmdCvarContext::shutdown() noexcept
{
    if (!impl_->pool)
        return; // never successfully init()'d

    // Free pool-owned resources on cvars.
    // Ownership model:
    //   FCVAR_USER_CREATED  → cv, name, abi.string, def_string all pool-owned
    //   owner_flags == 0    → engine-static cv; def_string is pool-dup'd; abi.string may be FCVAR_ALLOCATED
    //   owner_flags != 0 && !USER_CREATED → DLL-registered; cv/name/def_string DLL-owned; abi.string may be FCVAR_ALLOCATED
    // Use a manual while-loop so we can free cv before advancing.
    {
        Cvar *cv = impl_->cvar_list_head;
        while (cv) {
            Cvar *next = reinterpret_cast<Cvar *>(cv->abi.next);

            // Free pool-owned current string.
            if (cv->abi.flags & FCVAR_ALLOCATED) {
                memory::mem_free(cv->abi.string);
                cv->abi.string = nullptr;
                cv->abi.flags &= ~static_cast<std::uint32_t>(FCVAR_ALLOCATED);
            }

            if (cv->abi.flags & FCVAR_USER_CREATED) {
                // All of name, def_string, and cv itself are pool-owned.
                memory::mem_free(const_cast<char *>(cv->abi.name));
                if (cv->def_string)
                    memory::mem_free(const_cast<char *>(cv->def_string));
                memory::mem_free(cv);
            } else if (cv->abi.flags & FCVAR_DLL_WRAPPER) {
                // Engine-allocated wrapper around a DLL CvarAbi.
                // name, string, def_string are borrowed from DLL — do NOT free.
                // The Cvar struct itself is pool-owned.
                memory::mem_free(cv);
            } else if (cv->owner_flags == 0) {
                // Engine-static cvar: def_string was pool-dup'd by register_engine.
                if (cv->def_string) {
                    memory::mem_free(const_cast<char *>(cv->def_string));
                    cv->def_string = nullptr;
                }
                // cv and name are static; don't free.
            }
            // else: DLL-owned cvar without wrapper — nothing to free except abi.string above.

            cv = next;
        }
    }

    // Free pool-owned command structs (name + desc are pool-dup'd; cmd itself is pool-alloc'd).
    {
        Command *cmd = impl_->cmd_list_head;
        while (cmd) {
            Command *next = cmd->abi_next;
            memory::mem_free(const_cast<char *>(cmd->name));
            if (cmd->desc)
                memory::mem_free(const_cast<char *>(cmd->desc));
            memory::mem_free(cmd);
            cmd = next;
        }
    }

    // Free alias expansion strings (pool-owned); AliasDef is pool-alloc'd too.
    {
        AliasDef *al = impl_->alias_list_head;
        while (al) {
            AliasDef *next = al->abi_next;
            if (al->value)
                memory::mem_free(const_cast<char *>(al->value));
            memory::mem_free(al);
            al = next;
        }
    }

    // Clear hash-map chain nodes (the Node* allocations; V* objects freed above).
    impl_->cvar_map.clear_nodes();
    impl_->cmd_map.clear_nodes();
    impl_->alias_map.clear_nodes();

    // Null the list heads so any use-after-shutdown is easier to diagnose.
    impl_->cvar_list_head  = nullptr;
    impl_->cmd_list_head   = nullptr;
    impl_->alias_list_head = nullptr;
    impl_->pending_unlink.clear();

    memory::destroy_pool(impl_->pool);
    impl_->pool = {};
}

// ---------------------------------------------------------------------------
// Observer registration
// ---------------------------------------------------------------------------

void CmdCvarContext::add_cvar_observer(ICvarObserver *observer,
                                       std::uint32_t  flag_mask) noexcept
{
    if (impl_->observer_count >= limits::cmd_observer_max)
        return; // silently drop; limit enforced by InlineVector capacity

    impl_->observers[impl_->observer_count++] = { observer, flag_mask };
}

// ---------------------------------------------------------------------------
// Cvar registry — implementations
// ---------------------------------------------------------------------------

Cvar *CmdCvarContext::cvar_find(const char *name) noexcept
{
    if (!name) return nullptr;

    // GoldSrc compat: redirect legacy renamed cvars before lookup.
    if (impl_->compat_policy) {
        const char *redir = impl_->compat_policy->redirect_cvar_name(name);
        if (redir) name = redir;
    }
    return impl_->cvar_map.find(name);
}

void CmdCvarContext::cvar_register_engine(Cvar &cv) noexcept
{
    // Idempotent: if already registered (e.g. second init() after shutdown()),
    // skip to avoid double-insert.
    if (impl_->cvar_map.find(cv.abi.name))
        return;

    // Pool-copy def_string so it survives DLL unlinks that restore it.
    // (The original def_string may point into read-only BSS; we need a
    //  mutable copy so cvar_unlink() can free it safely if the cvar was
    //  marked FCVAR_USER_CREATED, and so cvar_set_direct can compare pointers.)
    if (cv.def_string) {
        char *copy = pool_dup(impl_->pool, cv.def_string);
        if (copy) cv.def_string = copy;
    }

    // Prepend to the ABI linked list.
    cv.abi.next       = reinterpret_cast<CvarAbi *>(impl_->cvar_list_head);
    impl_->cvar_list_head = &cv;

    // Insert into hash map (key borrowed from cv.abi.name).
    impl_->cvar_map.insert(cv.abi.name, &cv);
}

Cvar *CmdCvarContext::cvar_register_dll(CvarAbi *abi_ptr) noexcept
{
    if (!abi_ptr || !abi_ptr->name) return nullptr;

    // Idempotent.
    Cvar *existing = impl_->cvar_map.find(abi_ptr->name);
    if (existing) return existing;

    // Allocate a full engine-internal Cvar in the pool.
    // The DLL only provides CvarAbi-sized storage; writing extended fields
    // into the cast DLL pointer would be out-of-bounds UB.
    // The pool Cvar is marked FCVAR_DLL_WRAPPER so shutdown/unlink can free it.
    Cvar *cv = static_cast<Cvar *>(memory::mem_calloc(impl_->pool, sizeof(Cvar)));
    if (!cv) return nullptr;

    // Copy ABI fields from the DLL's struct.
    cv->abi.name   = abi_ptr->name;   // borrowed — DLL owns the string
    cv->abi.string = abi_ptr->string; // borrowed — DLL owns the string
    cv->abi.flags  = abi_ptr->flags | FCVAR_DLL_WRAPPER;
    cv->abi.value  = abi_ptr->value;
    cv->abi.next   = nullptr;
    cv->def_string = abi_ptr->string; // borrow as default too
    cv->desc       = nullptr;
    cv->generation.store(0, std::memory_order_relaxed);

    // Derive owner_flags from the registration flags.
    cv->owner_flags = abi_ptr->flags &
        (FCVAR_EXTDLL | FCVAR_CLIENTDLL | FCVAR_GAMEUIDLL | FCVAR_REFDLL);

    // Prepend to ABI list + hash map.
    cv->abi.next          = reinterpret_cast<CvarAbi *>(impl_->cvar_list_head);
    impl_->cvar_list_head = cv;
    impl_->cvar_map.insert(cv->abi.name, cv);
    return cv;
}

Cvar *CmdCvarContext::cvar_get_or_create(const char    *name,
                                          const char    *default_value,
                                          std::uint32_t  flags) noexcept
{
    if (!name) return nullptr;

    // Compat redirect.
    if (impl_->compat_policy) {
        const char *redir = impl_->compat_policy->redirect_cvar_name(name);
        if (redir) name = redir;
    }

    // Return existing cvar if already registered.
    Cvar *existing = impl_->cvar_map.find(name);
    if (existing) return existing;

    // Pool-allocate a new Cvar.
    Cvar *cv = static_cast<Cvar *>(memory::mem_calloc(impl_->pool, sizeof(Cvar)));
    if (!cv) return nullptr;

    cv->abi.name   = pool_dup(impl_->pool, name);
    // Allocate SEPARATE copies for abi.string and def_string so that
    // cvar_set_direct() can free abi.string without dangling def_string.
    cv->abi.string = pool_dup(impl_->pool, default_value ? default_value : "");
    cv->def_string = pool_dup(impl_->pool, default_value ? default_value : "");
    cv->abi.flags  = flags | FCVAR_USER_CREATED | FCVAR_ALLOCATED;
    cv->abi.value  = utilities::atof(cv->abi.string);
    cv->abi.next   = nullptr;
    cv->desc       = nullptr;

    if (!cv->abi.name || !cv->abi.string || !cv->def_string) {
        // OOM cleanup.  Pool will reclaim on destroy.
        return nullptr;
    }

    // Prepend to ABI list + hash map.
    cv->abi.next          = reinterpret_cast<CvarAbi *>(impl_->cvar_list_head);
    impl_->cvar_list_head = cv;
    impl_->cvar_map.insert(cv->abi.name, cv);
    return cv;
}

void CmdCvarContext::cvar_set_direct(Cvar           *cv,
                                      const char     *value,
                                      CvarWriteSource source) noexcept
{
    if (!cv || !value) return;

    // FCVAR_READ_ONLY: never writable.
    if (cv->abi.flags & FCVAR_READ_ONLY) {
        // Log: attempted write to read-only cvar.
        return;
    }

    // FCVAR_CHEAT: block if cheats are not enabled.
    // We don't have sv_cheats in the registry yet; once it's registered the
    // cvar_find("sv_cheats") path will work.  For now, only EngineInternal
    // writes bypass the cheat guard.
    if ((cv->abi.flags & FCVAR_CHEAT) && source != CvarWriteSource::EngineInternal) {
        Cvar *sv_cheats = impl_->cvar_map.find("sv_cheats");
        if (!sv_cheats || sv_cheats->abi.value == 0.0f) {
            // Cheats off: reset to default instead of applying the new value.
            value = cv->def_string ? cv->def_string : "";
        }
    }

    // FCVAR_NOEXTRAWHITESPACE: skip leading/trailing whitespace.
    // We use a small stack buffer; long values fall back to the truncated form.
    char trimmed[limits::cmd_line_max];
    if (cv->abi.flags & FCVAR_NOEXTRAWHITESPACE) {
        // Skip leading whitespace.
        while (*value == ' ' || *value == '\t') ++value;
        utilities::strncpy(trimmed, value, sizeof(trimmed));
        // Strip trailing whitespace.
        std::size_t len = utilities::strlen(trimmed);
        while (len > 0 && (trimmed[len - 1] == ' ' || trimmed[len - 1] == '\t'))
            trimmed[--len] = '\0';
        value = trimmed;
    }

    // FCVAR_PRINTABLEONLY: reject non-printable characters.
    if (cv->abi.flags & FCVAR_PRINTABLEONLY) {
        for (const char *p = value; *p; ++p) {
            const unsigned char c = static_cast<unsigned char>(*p);
            if (c < 32 || c > 126) {
                return; // silently reject
            }
        }
    }

    // Skip no-op writes.
    if (cv->abi.string && utilities::stricmp(cv->abi.string, value) == 0)
        return;

    // Free the old pool-owned string if present.
    if (cv->abi.flags & FCVAR_ALLOCATED) {
        memory::mem_free(cv->abi.string);
        cv->abi.flags &= ~static_cast<std::uint32_t>(FCVAR_ALLOCATED);
    }

    // Pool-duplicate the new string.
    cv->abi.string = pool_dup(impl_->pool, value);
    if (!cv->abi.string) {
        cv->abi.string = const_cast<char *>(""); // OOM fallback
        return;
    }
    cv->abi.flags |= FCVAR_ALLOCATED;

    // Update float value.
    cv->abi.value = utilities::atof(value);

    // Set FCVAR_CHANGED (polled by legacy DLLs).
    cv->abi.flags |= FCVAR_CHANGED;

    // Bump generation counter for lock-free change detection.
    cv->generation.fetch_add(1u, std::memory_order_release);

#if XASH_STATS
    cv->write_count.fetch_add(1u, std::memory_order_relaxed);
    cv->last_write_source = source;
    // last_write_frame: filled by the host when it knows the current frame.
#endif

#if XASH_DEBUG_CVARS
    if (impl_->break_on_write_name &&
        utilities::stricmp(cv->abi.name, impl_->break_on_write_name) == 0) {
        // Platform debug break.  The host layer sets break_on_write_name.
        // We emit a console message as a lightweight alternative.
        platform::console::write("[cvar] break-on-write: ");
        platform::console::write(cv->abi.name);
        platform::console::write("\n");
    }
    // TODO: append CvarChangeRecord to change_log (needs old value snapshot).
#endif

    // Notify registered observers whose flag_mask overlaps this cvar's flags.
    const std::uint32_t cvar_flags = cv->abi.flags;
    const char *old_value = cv->def_string ? cv->def_string : "";
    for (std::size_t i = 0; i < impl_->observer_count; ++i) {
        const auto &entry = impl_->observers[i];
        if (entry.observer && (entry.flag_mask & cvar_flags))
            entry.observer->on_cvar_changed(cv, old_value);
    }

    // Accumulate stats.
#if XASH_STATS
    impl_->stats_block.cvars_written.fetch_add(1u, std::memory_order_relaxed);
#endif
}

void CmdCvarContext::cvar_set(const char     *name,
                               const char     *value,
                               CvarWriteSource source) noexcept
{
    if (!name) return;
    Cvar *cv = cvar_find(name);
    if (!cv) {
        // Auto-create user cvars on first write.
        cv = cvar_get_or_create(name, value, 0);
        if (!cv) return;
        // Already set to value by get_or_create; no need to call set_direct.
        return;
    }
    cvar_set_direct(cv, value, source);
}

void CmdCvarContext::cvar_unlink(std::uint32_t owner_flags_mask) noexcept
{
    // Unlink guard: don't remove server DLL cvars if the server is still loaded.
    if ((owner_flags_mask & FCVAR_EXTDLL)    && impl_->server_dll_loaded) return;
    if ((owner_flags_mask & FCVAR_CLIENTDLL) && impl_->client_dll_loaded) return;

    // Walk the ABI list; collect entries to remove.
    // We rebuild the list rather than remove mid-iteration to keep the logic
    // simple and avoid pointer-chasing bugs.
    Cvar *new_head = nullptr;
    Cvar **tail    = &new_head;

    for (Cvar *cv = impl_->cvar_list_head; cv; ) {
        Cvar *next = reinterpret_cast<Cvar *>(cv->abi.next);

        if (cv->owner_flags & owner_flags_mask) {
            // Remove from hash map.
            impl_->cvar_map.remove(cv->abi.name);

            // Free FCVAR_ALLOCATED string.
            if (cv->abi.flags & FCVAR_ALLOCATED) {
                memory::mem_free(cv->abi.string);
                cv->abi.flags &= ~static_cast<std::uint32_t>(FCVAR_ALLOCATED);
            }

            // Restore string to def_string so legacy DLLs see a sane value if they
            // still hold the pointer (DLL is about to be unloaded, but be safe).
            cv->abi.string = cv->def_string
                ? const_cast<char *>(cv->def_string)
                : const_cast<char *>("");

            // Pool-allocated Cvar objects:
            //   FCVAR_USER_CREATED  — pool name + def_string + Cvar struct
            //   FCVAR_DLL_WRAPPER   — pool Cvar struct; name/string are DLL-owned
            if (cv->abi.flags & FCVAR_USER_CREATED) {
                memory::mem_free(const_cast<char *>(cv->abi.name));
                if (cv->def_string)
                    memory::mem_free(const_cast<char *>(cv->def_string));
                memory::mem_free(cv);
            } else if (cv->abi.flags & FCVAR_DLL_WRAPPER) {
                // Wrapper is pool-owned; name/string/def_string are borrowed.
                memory::mem_free(cv);
            }
            // else: DLL owns the struct; leave it alone (legacy non-wrapper path).
        } else {
            // Keep this entry in the list.
            *tail       = cv;
            cv->abi.next = nullptr;
            tail        = reinterpret_cast<Cvar **>(&cv->abi.next);
        }

        cv = next;
    }

    impl_->cvar_list_head = new_head;
}

CvarAbi *CmdCvarContext::cvar_get_list() const noexcept
{
    return impl_->cvar_list_head
        ? reinterpret_cast<CvarAbi *>(impl_->cvar_list_head)
        : nullptr;
}

CvarDesc CmdCvarContext::cvar_describe(const Cvar *cv) const noexcept
{
    if (!cv) return {};
    return {
        cv->abi.name,
        cv->abi.string,
        cv->def_string,
        cv->desc,
        cv->abi.flags,
        cv->type_hint,
        cv->range_min,
        cv->range_max,
    };
}

const char *CmdCvarContext::cvar_variable_string(const char *name) const noexcept
{
    const Cvar *cv = const_cast<CmdCvarContext *>(this)->cvar_find(name);
    return cv ? cv->abi.string : "";
}

float CmdCvarContext::cvar_variable_value(const char *name) const noexcept
{
    const Cvar *cv = const_cast<CmdCvarContext *>(this)->cvar_find(name);
    return cv ? cv->abi.value : 0.0f;
}

int CmdCvarContext::cvar_variable_integer(const char *name) const noexcept
{
    return static_cast<int>(cvar_variable_value(name));
}

void CmdCvarContext::cvar_full_set(const char     *name,
                                    const char     *value,
                                    std::uint32_t   flags) noexcept
{
    // TODO: find-or-create, then force-set bypassing all guards + update flags
    (void)name; (void)value; (void)flags;
}

void CmdCvarContext::cvar_set_cheat_state() noexcept
{
    for (Cvar *cv = impl_->cvar_list_head; cv; cv = reinterpret_cast<Cvar *>(cv->abi.next)) {
        if (!(cv->abi.flags & FCVAR_CHEAT)) continue;
        const char *def = cv->def_string ? cv->def_string : "";
        cvar_set_direct(cv, def, CvarWriteSource::EngineInternal);
    }
}

void CmdCvarContext::cvar_write_variables(void          *vfile,
                                           std::uint32_t  owner_flags_mask) noexcept
{
    // TODO: walk cvar_list_head; for each FCVAR_ARCHIVE cvar (filtered by mask)
    //       write  name "value"\n  via filesystem VFile API
    (void)vfile; (void)owner_flags_mask;
}

void CmdCvarContext::cvar_prepare_to_unlink(std::uint32_t owner_flags_mask) noexcept
{
    for (Cvar *cv = impl_->cvar_list_head; cv; cv = reinterpret_cast<Cvar *>(cv->abi.next)) {
        if (!(cv->owner_flags & owner_flags_mask)) continue;
        char *name_copy = pool_dup(impl_->pool, cv->abi.name);
        if (name_copy)
            impl_->pending_unlink.push_back({ name_copy, cv->owner_flags });
    }
}

void CmdCvarContext::unlink_pending_cvars() noexcept
{
    for (const auto &entry : impl_->pending_unlink)
        cvar_unlink(entry.owner_flags);
    impl_->pending_unlink.clear();
}

// ---------------------------------------------------------------------------
// Command registry
// ---------------------------------------------------------------------------

void CmdCvarContext::cmd_add(const char    *name,
                              CommandFn      fn,
                              std::uint32_t  flags,
                              const char    *desc) noexcept
{
    if (!name || !*name) return;

    // If a command already exists with the same name:
    Command *existing = impl_->cmd_map.find(name);
    if (existing) {
        if (existing->flags & FCMD_OVERRIDABLE) {
            // Silently replace: update fn + flags + desc.
            existing->fn    = fn;
            existing->flags = flags;
            if (existing->desc) memory::mem_free(const_cast<char *>(existing->desc));
            existing->desc = pool_dup(impl_->pool, desc ? desc : "");
        }
        // else: duplicate — silently ignore (matches legacy behaviour).
        return;
    }

    // Check compat policy: should this command be flagged FCMD_OVERRIDABLE?
    std::uint32_t effective_flags = flags;
    if (impl_->compat_policy && impl_->compat_policy->is_overridable_command(name))
        effective_flags |= FCMD_OVERRIDABLE;

    Command *cmd = static_cast<Command *>(memory::mem_calloc(impl_->pool, sizeof(Command)));
    if (!cmd) return;

    cmd->name        = pool_dup(impl_->pool, name);
    cmd->desc        = pool_dup(impl_->pool, desc ? desc : "");
    cmd->fn          = fn;
    cmd->flags       = effective_flags;
    cmd->owner_flags = 0; // set by the DLL registration wrapper
    cmd->abi_next    = nullptr;

    if (!cmd->name) { memory::mem_free(cmd); return; } // OOM

    // Prepend to ABI list + hash map.
    cmd->abi_next        = impl_->cmd_list_head;
    impl_->cmd_list_head = cmd;
    impl_->cmd_map.insert(cmd->name, cmd);
}

void CmdCvarContext::cmd_remove(const char *name) noexcept
{
    // TODO
    (void)name;
}

void CmdCvarContext::cmd_unlink(std::uint32_t flags_mask) noexcept
{
    // TODO
    (void)flags_mask;
}

CommandDesc CmdCvarContext::cmd_describe(const char *name) const noexcept
{
    // TODO
    (void)name;
    return {};
}

bool CmdCvarContext::cmd_exists(const char *name) const noexcept
{
    // TODO: hash map lookup
    (void)name;
    return false;
}

void CmdCvarContext::cmd_execute_string(std::string_view text) noexcept
{
    // TODO: tokenise and dispatch immediately as privileged
    (void)text;
}

// ---------------------------------------------------------------------------
// Command buffer — stub forwards (implementations in cmd.cpp)
// ---------------------------------------------------------------------------

void CmdCvarContext::cbuf_add_text(std::string_view text) noexcept
{
    impl_->cmd_text.emplace_back(text);

#if XASH_STATS
    const auto depth = static_cast<std::uint32_t>(impl_->cmd_text.size());
    auto &hw = impl_->stats_block.buffer_high_water;
    std::uint32_t cur = hw.load(std::memory_order_relaxed);
    while (depth > cur && !hw.compare_exchange_weak(cur, depth,
                                                     std::memory_order_relaxed))
        {}
#endif
}

void CmdCvarContext::cbuf_insert_text(std::string_view text) noexcept
{
    impl_->cmd_text.emplace_front(text);
}

void CmdCvarContext::cbuf_stuff_text(std::string_view text) noexcept
{
    impl_->filteredcmd_text.emplace_back(text);
}

void CmdCvarContext::cbuf_execute() noexcept
{
    // TODO: tokenise, privilege-check, dispatch
    // Honour cmd_wait: if > 0, decrement and return.
    // Set tls_ctx = this before invoking each CommandFn; clear after.
}

void CmdCvarContext::cbuf_clear() noexcept
{
    impl_->cmd_text.clear();
    impl_->filteredcmd_text.clear();
}

// ---------------------------------------------------------------------------
// Tokenizer accessors
// ---------------------------------------------------------------------------

int CmdCvarContext::cmd_argc() const noexcept
{
    return impl_->tok_argc;
}

const char *CmdCvarContext::cmd_argv(int i) const noexcept
{
    if (i < 0 || i >= impl_->tok_argc)
        return "";
    return impl_->tok_argv[i] ? impl_->tok_argv[i] : "";
}

const char *CmdCvarContext::cmd_args() const noexcept
{
    // Returns the argsBuffer which cmd_dispatch_line() fills with everything
    // after token 0.  Empty string when not dispatching.
    return impl_->tok_argsBuffer;
}

bool CmdCvarContext::cmd_current_is_privileged() const noexcept
{
    return impl_->tok_is_privileged;
}

// ---------------------------------------------------------------------------
// DLL lifecycle
// ---------------------------------------------------------------------------

void CmdCvarContext::set_server_dll_loaded(bool loaded) noexcept
{
    impl_->server_dll_loaded = loaded;
}

void CmdCvarContext::set_client_dll_loaded(bool loaded) noexcept
{
    impl_->client_dll_loaded = loaded;
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

const CmdCvarStats &CmdCvarContext::stats() const noexcept
{
    return impl_->stats_block;
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

#if XASH_DEBUG_CVARS
void CmdCvarContext::debug_break_on_cvar_write(const char *cvar_name) noexcept
{
    impl_->break_on_write_name = cvar_name;
}

void CmdCvarContext::dump_hash_stats() const noexcept
{
    // TODO: print bucket fill distribution to platform console
}
#endif

} // namespace xash::cmd_cvar
