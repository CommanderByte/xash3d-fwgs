// xash3dpp — cmd_cvar: CmdCvarContext query helpers, DLL lifecycle, stats, debug.
// Legacy reference: engine/common/cmd.c, cvar.c

#include <xash3dpp/private/cmd_cvar/context_impl.hpp>
#include <xash3dpp/platform/console.hpp>

namespace xash::cmd_cvar {

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
    return impl_->tok_argsBuffer.data();
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
    struct MapStats {
        const char *label;
        std::size_t used_buckets;
        std::size_t total_entries;
        std::size_t max_chain;
    };

    auto gather = [](const auto &map, const char *label) -> MapStats {
        std::array<std::size_t, limits::cvar_hash_buckets> hist{};
        map.bucket_histogram(hist);
        MapStats s{ label, 0, 0, 0 };
        for (std::size_t i = 0; i < limits::cvar_hash_buckets; ++i) {
            if (hist[i]) ++s.used_buckets;
            s.total_entries += hist[i];
            if (hist[i] > s.max_chain) s.max_chain = hist[i];
        }
        return s;
    };

    const std::array<MapStats, 3> maps = {{
        gather(impl_->cvar_map,  "cvar_map "),
        gather(impl_->cmd_map,   "cmd_map  "),
        gather(impl_->alias_map, "alias_map"),
    }};

    char buf[128];
    platform::console::write("cmd_cvar hash stats:\n");
    for (const auto &m : maps) {
        utilities::snprintf(buf, sizeof(buf),
                      "  %s: %zu/%zu buckets used, %zu entries, max chain %zu\n",
                      m.label,
                      m.used_buckets, static_cast<std::size_t>(limits::cvar_hash_buckets),
                      m.total_entries, m.max_chain);
        platform::console::write(buf);
    }
}
#endif

} // namespace xash::cmd_cvar
