// xash3dpp — cmd_cvar: CmdCvarContext command registry and buffer methods.
// Legacy reference: engine/common/cmd.c

#include <xash3dpp/private/cmd_cvar/context_impl.hpp>

namespace xash::cmd_cvar {

// ---------------------------------------------------------------------------
// Command registry
// ---------------------------------------------------------------------------

void CmdCvarContext::cmd_add(std::string_view name,
                              CommandFn      fn,
                              std::uint32_t  flags,
                              const char    *desc) noexcept
{
    if (name.empty()) return;

    // If a command already exists with the same name:
    Command *existing = impl_->cmd_map.find(name);
    if (existing) {
        if (existing->flags & FCMD_OVERRIDABLE) {
            // Silently replace: update fn + flags + desc.
            existing->fn    = fn;
            existing->flags = flags;
            if (existing->desc) ::xash::memory::mem_free(existing->desc);
            existing->desc = pool_dup(impl_->pool, desc ? desc : "");
        }
        // else: duplicate — silently ignore (matches legacy behaviour).
        return;
    }

    // Check compat policy: should this command be flagged FCMD_OVERRIDABLE?
    std::uint32_t effective_flags = flags;
    if (impl_->compat_policy && impl_->compat_policy->is_overridable_command(name))
        effective_flags |= FCMD_OVERRIDABLE;

    Command *cmd = static_cast<Command *>(::xash::memory::mem_calloc(impl_->pool, sizeof(Command)));
    if (!cmd) return;

    cmd->name        = pool_dup(impl_->pool, name); // bounded string_view dup
    cmd->desc        = pool_dup(impl_->pool, desc ? desc : "");
    cmd->fn          = fn;
    cmd->flags       = effective_flags;
    cmd->owner_flags = 0; // set by the DLL registration wrapper
    cmd->abi_next    = nullptr;

    if (!cmd->name) { ::xash::memory::mem_free(cmd); return; } // OOM

    // Prepend to ABI list + hash map.
    cmd->abi_next        = impl_->cmd_list_head;
    impl_->cmd_list_head = cmd;
    impl_->cmd_map.insert(cmd->name, cmd);
}

void CmdCvarContext::cmd_remove(std::string_view name) noexcept
{
    if (name.empty()) return;

    Command *cmd = impl_->cmd_map.remove(name);
    if (!cmd) return;

    // Rebuild ABI list without this entry.
    Command *new_head = nullptr;
    Command **tail    = &new_head;
    for (Command *c = impl_->cmd_list_head; c; ) {
        Command *next = c->abi_next;
        if (c != cmd) {
            *tail       = c;
            c->abi_next = nullptr;
            tail        = &c->abi_next;
        }
        c = next;
    }
    impl_->cmd_list_head = new_head;

    // Free pool-owned fields.
    if (cmd->name) ::xash::memory::mem_free(cmd->name);
    if (cmd->desc) ::xash::memory::mem_free(cmd->desc);
    ::xash::memory::mem_free(cmd);
}

void CmdCvarContext::cmd_unlink(std::uint32_t flags_mask) noexcept
{
    // Unlink guard: don't remove DLL commands while that DLL is still loaded.
    if ((flags_mask & FCMD_EXTDLL)    && impl_->server_dll_loaded) return;
    if ((flags_mask & FCMD_CLIENTDLL) && impl_->client_dll_loaded) return;

    Command *new_head = nullptr;
    Command **tail    = &new_head;
    for (Command *cmd = impl_->cmd_list_head; cmd; ) {
        Command *next = cmd->abi_next;
        if (cmd->flags & flags_mask) {
            impl_->cmd_map.remove(cmd->name);
            if (cmd->name) ::xash::memory::mem_free(cmd->name);
            if (cmd->desc) ::xash::memory::mem_free(cmd->desc);
            ::xash::memory::mem_free(cmd);
        } else {
            *tail         = cmd;
            cmd->abi_next = nullptr;
            tail          = &cmd->abi_next;
        }
        cmd = next;
    }
    impl_->cmd_list_head = new_head;
}

CommandDesc CmdCvarContext::cmd_describe(std::string_view name) const noexcept
{
    if (name.empty()) return {};
    const Command *cmd = impl_->cmd_map.find(name);
    if (!cmd) return {};
    return { cmd->name, cmd->desc, cmd->flags };
}

bool CmdCvarContext::cmd_exists(std::string_view name) const noexcept
{
    if (name.empty()) return false;
    return impl_->cmd_map.find(name) != nullptr;
}

// ---------------------------------------------------------------------------
// Command buffer — split-and-push helper
// ---------------------------------------------------------------------------

// Split text at newline/semicolon boundaries and push each non-empty command
// to either the back or front of dest so that cbuf_execute can honour 'wait'
// between individual commands.
static void cbuf_split_push(std::deque<std::string> &dest, std::string_view text, bool front) noexcept
{
    const char *p   = text.data();
    const char *end = p + text.size();

    std::vector<std::string_view> pieces;

    const char *start = p;
    bool in_quotes = false;
    while (p <= end) {
        const bool at_end = (p == end);
        const char c = at_end ? '\0' : *p;

        if (c == '"') { in_quotes = !in_quotes; ++p; continue; }

        if (!in_quotes && (c == ';' || c == '\n' || at_end)) {
            std::string_view piece{ start, static_cast<std::size_t>(p - start) };
            piece = ::xash::utilities::trim_sv(piece, " \t\r");
            if (!piece.empty()) {
                if (front)
                    pieces.push_back(piece);
                else
                    dest.emplace_back(piece);
            }
            if (at_end) break;
            start = p + 1;
            ++p;
            continue;
        }
        ++p;
    }

    if (front) {
        for (auto it = pieces.rbegin(); it != pieces.rend(); ++it)
            dest.emplace_front(*it);
    }
}

// ---------------------------------------------------------------------------
// Command buffer — public interface
// ---------------------------------------------------------------------------

void CmdCvarContext::cbuf_add_text(std::string_view text) noexcept
{
    cbuf_split_push(impl_->cmd_text, text, /*front=*/false);

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
    cbuf_split_push(impl_->cmd_text, text, /*front=*/true);
}

void CmdCvarContext::cbuf_stuff_text(std::string_view text) noexcept
{
    impl_->filteredcmd_text.emplace_back(text);
}

void CmdCvarContext::cbuf_clear() noexcept
{
    impl_->cmd_text.clear();
    impl_->filteredcmd_text.clear();
}

} // namespace xash::cmd_cvar
