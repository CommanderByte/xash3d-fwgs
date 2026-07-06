// xash3dpp — cmd_cvar: CmdCvarContext::init() and shutdown().
// Legacy reference: engine/common/cmd.c, cvar.c

#include <xash3dpp/private/cmd_cvar/context_impl.hpp>
#include <xash3dpp/platform/console.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>

namespace xash::cmd_cvar {

namespace {
// ---------------------------------------------------------------------------
// Storage for built-in (engine-owned) cvars.
// Lifetime: process; re-initialized on each CmdCvarContext::init() call.
// These are only referenced inside init(); they live here rather than in a
// shared header to keep them as narrow-scope as possible.
// ---------------------------------------------------------------------------
namespace builtin_cvars {
constexpr char k_scripting_name[]  = "cmd_scripting";
constexpr char k_scripting_def[]   = "0";
constexpr char k_filter_name[]     = "cl_filterstuffcmd";
constexpr char k_filter_def[]      = "1";
} // namespace builtin_cvars
} // anonymous namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool CmdCvarContext::init(const CmdCvarInitParams &params) noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    impl_->pool = ::xash::memory::create_pool("cmd_cvar");
    if (!impl_->pool) {
        ::xash::core::log(::xash::core::LogLevel::Error, "cmd_cvar", "failed to create memory pool");
        return false;
    }

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
        Cvar &sc = impl_->builtin_cmd_scripting;
        sc.abi.name   = const_cast<char *>(k_scripting_name); // SAFETY: static string literal stored in the ABI char* field; never written or freed
        sc.abi.string = const_cast<char *>(k_scripting_def);  // SAFETY: static string literal stored in the ABI char* field; never written or freed
        sc.abi.flags  = FCVAR_ARCHIVE | FCVAR_PRIVILEGED;
        sc.abi.value  = 0.0f;
        sc.abi.next   = nullptr;
        sc.desc       = "enable simple condition checking and variable operations";
        sc.def_string = k_scripting_def;
        sc.generation.store(0, std::memory_order_relaxed);
        cvar_register_engine(sc);

        Cvar &fc = impl_->builtin_cl_filterstuffcmd;
        fc.abi.name   = const_cast<char *>(k_filter_name); // SAFETY: static string literal stored in the ABI char* field; never written or freed
        fc.abi.string = const_cast<char *>(k_filter_def);  // SAFETY: static string literal stored in the ABI char* field; never written or freed
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
        if (!tls_ctx) return;
        const int argc = tls_ctx->cmd_argc();
        for (int i = 1; i < argc; ++i) {
            if (i > 1) ::xash::platform::console::write(" ");
            ::xash::platform::console::write(tls_ctx->cmd_argv(i));
        }
        ::xash::platform::console::write("\n");
    }, 0, "print a message to the console (useful in scripts)");

    // wait — skip the rest of the command buffer for N frames.
    cmd_add("wait", []() noexcept {
        if (!tls_ctx) return;
        int frames = (tls_ctx->cmd_argc() > 1) ? ::xash::utilities::atoi(tls_ctx->cmd_argv(1)) : 1;
        if (frames < 1) frames = 1;
        tls_ctx->impl_->cmd_wait = frames;
    }, 0, "delay command buffer execution by N frames (default: 1)");

    // alias — create or list command aliases.
    cmd_add("alias", []() noexcept {
        if (!tls_ctx) return;
        Impl &impl           = *tls_ctx->impl_;
        const int   argc     = tls_ctx->cmd_argc();
        const char *al_name  = (argc >= 2) ? tls_ctx->cmd_argv(1) : nullptr;
        const char *al_value = (argc >= 3) ? tls_ctx->cmd_argv(2) : nullptr;

        if (!al_name) {
            // No args: list all aliases.
            for (AliasDef *a = impl.alias_list_head; a; a = a->abi_next) {
                ::xash::platform::console::write(a->name);
                if (a->value) {
                    ::xash::platform::console::write(" = ");
                    ::xash::platform::console::write(a->value);
                }
                ::xash::platform::console::write("\n");
            }
            return;
        }

        AliasDef *al = impl.alias_map.find(al_name);

        if (!al_value) {
            // One arg: delete the alias if it exists.
            if (!al) return;
            impl.alias_map.remove(al_name);
            AliasDef *new_head = nullptr, **tail = &new_head;
            for (AliasDef *a = impl.alias_list_head; a; ) {
                AliasDef *next = a->abi_next;
                if (a != al) { *tail = a; a->abi_next = nullptr; tail = &a->abi_next; }
                a = next;
            }
            impl.alias_list_head = new_head;
            if (al->value) ::xash::memory::mem_free(al->value);
            ::xash::memory::mem_free(al);
            return;
        }

        if (al) {
            // Update existing alias value.
            if (al->value) ::xash::memory::mem_free(al->value);
            al->value = pool_dup(impl.pool, al_value);
            return;
        }

        // create new alias.
        al = static_cast<AliasDef *>(::xash::memory::mem_calloc(impl.pool, sizeof(AliasDef)));
        if (!al) return;
        ::xash::utilities::strncpy(al->name, al_name, sizeof(al->name));
        al->value    = pool_dup(impl.pool, al_value);
        al->abi_next = impl.alias_list_head;
        impl.alias_list_head = al;
        impl.alias_map.insert(al->name, al);
    }, 0, "create a command alias");

    // unalias — remove an alias.
    cmd_add("unalias", []() noexcept {
        if (!tls_ctx) return;
        Impl &impl          = *tls_ctx->impl_;
        const char *al_name = (tls_ctx->cmd_argc() >= 2) ? tls_ctx->cmd_argv(1) : nullptr;
        if (!al_name) return;
        AliasDef *al = impl.alias_map.remove(al_name);
        if (!al) return;
        AliasDef *new_head = nullptr, **tail = &new_head;
        for (AliasDef *a = impl.alias_list_head; a; ) {
            AliasDef *next = a->abi_next;
            if (a != al) { *tail = a; a->abi_next = nullptr; tail = &a->abi_next; }
            a = next;
        }
        impl.alias_list_head = new_head;
        if (al->value) ::xash::memory::mem_free(al->value);
        ::xash::memory::mem_free(al);
    }, 0, "remove a command alias");

    // stuffcmds — replay +cmd arguments from the host command line.
    cmd_add("stuffcmds", []() noexcept {
        // TODO: host layer injects this; stub until host integration
    }, 0, "execute command-line + arguments");

    // exec — execute a .cfg file (requires filesystem subsystem).
    cmd_add("exec", []() noexcept {
        // TODO: depends on xash3dpp_filesystem; stub until that subsystem exists
    }, 0, "execute a script file");

    // cmdlist — list all registered commands to the console.
    cmd_add("cmdlist", []() noexcept {
        if (!tls_ctx) return;
        const Impl &impl = *tls_ctx->impl_;
        std::size_t count = 0;
        for (const Command *cmd = impl.cmd_list_head; cmd; cmd = cmd->abi_next) {
            ::xash::platform::console::write(cmd->name);
            if (cmd->desc && *cmd->desc) {
                ::xash::platform::console::write(" \xe2\x80\x94 ");
                ::xash::platform::console::write(cmd->desc);
            }
            ::xash::platform::console::write("\n");
            ++count;
        }
        char buf[64];
        ::xash::utilities::snprintf(buf, sizeof(buf), "%zu command(s)\n", count);
        ::xash::platform::console::write(buf);
    }, 0, "list registered commands");

    // cvarlist — list all registered cvars to the console.
    cmd_add("cvarlist", []() noexcept {
        if (!tls_ctx) return;
        const Impl &impl = *tls_ctx->impl_;
        std::size_t count = 0;
        for (const Cvar *cv = impl.cvar_list_head; cv; cv = cvar_list_next(cv)) {
            ::xash::platform::console::write(cv->abi.name);
            ::xash::platform::console::write(" = \"");
            if (cv->abi.string) ::xash::platform::console::write(cv->abi.string);
            ::xash::platform::console::write("\"\n");
            ++count;
        }
        char buf[64];
        ::xash::utilities::snprintf(buf, sizeof(buf), "%zu cvar(s)\n", count);
        ::xash::platform::console::write(buf);
    }, 0, "list registered cvars");

#if XASH_DEBUG_CVARS
    // hashstats — dump hash map bucket statistics.
    cmd_add("hashstats", []() noexcept {
        if (tls_ctx) tls_ctx->dump_hash_stats();
    }, 0, "dump cmd_cvar hash map statistics");
#endif

    return true;
}

void CmdCvarContext::shutdown() noexcept
{
    ::xash::core::assert_thread_role(::xash::core::ThreadRole::Main);
    if (!impl_->pool)
        return; // never successfully init()'d

    // Free pool-owned resources on cvars.
    // Ownership model:
    //   FCVAR_USER_CREATED  → cv, name, abi.string, def_string all pool-owned
    //   owner_flags == 0    → engine-static cv; def_string is pool-dup'd; abi.string may be FCVAR_ALLOCATED
    //   owner_flags != 0 && !USER_CREATED → DLL-registered; cv/name/def_string DLL-owned; abi.string may be FCVAR_ALLOCATED
    {
        Cvar *cv = impl_->cvar_list_head;
        while (cv) {
            Cvar *next = cvar_list_next(cv);

            // Free pool-owned current string.
            if (cv->abi.flags & FCVAR_ALLOCATED) {
                ::xash::memory::mem_free(cv->abi.string);
                cv->abi.string = nullptr;
                cv->abi.flags &= ~static_cast<std::uint32_t>(FCVAR_ALLOCATED);
            }

            if (cv->abi.flags & FCVAR_USER_CREATED) {
                // All of name, def_string, and cv itself are pool-owned.
                ::xash::memory::mem_free(cv->abi.name);
                if (cv->def_string)
                    ::xash::memory::mem_free(const_cast<char *>(cv->def_string)); // SAFETY: reclaiming pool memory (def_string was pool_dup'd as char* then stored const)
                ::xash::memory::mem_free(cv);
            } else if (cv->abi.flags & FCVAR_DLL_WRAPPER) {
                // Engine-allocated wrapper around a DLL CvarAbi.
                // name, string, def_string are borrowed from DLL — do NOT free.
                ::xash::memory::mem_free(cv);
            } else if (cv->owner_flags == 0) {
                // Engine-static cvar: def_string was pool-dup'd by register_engine.
                if (cv->def_string) {
                    ::xash::memory::mem_free(const_cast<char *>(cv->def_string)); // SAFETY: reclaiming pool memory (def_string was pool_dup'd as char* then stored const)
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
            ::xash::memory::mem_free(cmd->name);
            if (cmd->desc)
                ::xash::memory::mem_free(cmd->desc);
            ::xash::memory::mem_free(cmd);
            cmd = next;
        }
    }

    // Free alias expansion strings (pool-owned); AliasDef is pool-alloc'd too.
    {
        AliasDef *al = impl_->alias_list_head;
        while (al) {
            AliasDef *next = al->abi_next;
            if (al->value)
                ::xash::memory::mem_free(al->value);
            ::xash::memory::mem_free(al);
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

    ::xash::memory::destroy_pool(impl_->pool);
    impl_->pool = {};
}

} // namespace xash::cmd_cvar
