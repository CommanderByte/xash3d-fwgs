// xash3dpp — cmd_cvar: CmdCvarContext cvar registry methods.
// Legacy reference: engine/common/cvar.c

#include <xash3dpp/private/cmd_cvar/context_impl.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/platform/console.hpp>

namespace xash::cmd_cvar {

// ---------------------------------------------------------------------------
// Observer registration
// ---------------------------------------------------------------------------

void CmdCvarContext::add_cvar_observer(ICvarObserver *observer,
                                       std::uint32_t  flag_mask) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if (impl_->observer_count >= ::xash::limits::cmd_observer_max)
        return; // silently drop; limit enforced by table capacity

    impl_->observers[impl_->observer_count++] = { observer, flag_mask };
}

// ---------------------------------------------------------------------------
// Cvar registry
// ---------------------------------------------------------------------------

Cvar *CmdCvarContext::cvar_find(std::string_view name) noexcept
{
    if (name.empty()) return nullptr;

    // GoldSrc compat: redirect legacy renamed cvars before lookup.
    // (Bounded string_view path throughout — HB-1/M-5; a returned redirect
    // is a static NUL-terminated string.)
    if (impl_->compat_policy) {
        const char *redir = impl_->compat_policy->redirect_cvar_name(name);
        if (redir) return impl_->cvar_map.find(redir);
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
    if (cv.def_string) {
        char *copy = pool_dup(impl_->pool, cv.def_string);
        if (copy) cv.def_string = copy;
    }

    // Prepend to the ABI linked list.
    cvar_list_set_next(&cv, impl_->cvar_list_head);
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
    Cvar *cv = static_cast<Cvar *>(::xash::memory::mem_calloc(impl_->pool, sizeof(Cvar)));
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
    cvar_list_set_next(cv, impl_->cvar_list_head);
    impl_->cvar_list_head = cv;
    impl_->cvar_map.insert(cv->abi.name, cv);
    return cv;
}

Cvar *CmdCvarContext::cvar_get_or_create(std::string_view name,
                                          const char    *default_value,
                                          std::uint32_t  flags) noexcept
{
    if (name.empty()) return nullptr;

    // Compat redirect (a returned redirect is a static NUL-terminated string).
    std::string_view effective = name;
    if (impl_->compat_policy) {
        const char *redir = impl_->compat_policy->redirect_cvar_name(name);
        if (redir) effective = redir;
    }

    // Return existing cvar if already registered.
    Cvar *existing = impl_->cvar_map.find(effective);
    if (existing) return existing;

    // Pool-allocate a new Cvar.
    Cvar *cv = static_cast<Cvar *>(::xash::memory::mem_calloc(impl_->pool, sizeof(Cvar)));
    if (!cv) return nullptr;

    cv->abi.name   = pool_dup(impl_->pool, effective); // bounded string_view dup
    // Allocate SEPARATE copies for abi.string and def_string so that
    // cvar_set_direct() can free abi.string without dangling def_string.
    cv->abi.string = pool_dup(impl_->pool, default_value ? default_value : "");
    cv->def_string = pool_dup(impl_->pool, default_value ? default_value : "");
    cv->abi.flags  = flags | FCVAR_USER_CREATED | FCVAR_ALLOCATED;
    cv->abi.value  = ::xash::utilities::atof(cv->abi.string);
    cv->abi.next   = nullptr;
    cv->desc       = nullptr;

    if (!cv->abi.name || !cv->abi.string || !cv->def_string) {
        // OOM cleanup.  Pool will reclaim on destroy.
        return nullptr;
    }

    // Prepend to ABI list + hash map.
    cvar_list_set_next(cv, impl_->cvar_list_head);
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
        return;
    }

    // FCVAR_CHEAT: block if cheats are not enabled.
    if ((cv->abi.flags & FCVAR_CHEAT) && source != CvarWriteSource::EngineInternal) {
        Cvar *sv_cheats = impl_->cvar_map.find("sv_cheats");
        if (!sv_cheats || sv_cheats->abi.value == 0.0f) {
            // Cheats off: reset to default instead of applying the new value.
            value = cv->def_string ? cv->def_string : "";
        }
    }

    // FCVAR_NOEXTRAWHITESPACE: skip leading/trailing whitespace.
    char trimmed[::xash::limits::cmd_line_max];
    if (cv->abi.flags & FCVAR_NOEXTRAWHITESPACE) {
        const std::string_view sv = ::xash::utilities::trim_sv(std::string_view{ value });
        ::xash::utilities::strncpy(trimmed, sv.data(),
                           sv.size() + 1 < sizeof(trimmed) ? sv.size() + 1 : sizeof(trimmed));
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
    if (cv->abi.string && ::xash::utilities::stricmp(cv->abi.string, value) == 0)
        return;

    // Free the old pool-owned string if present.
    if (cv->abi.flags & FCVAR_ALLOCATED) {
        ::xash::memory::mem_free(cv->abi.string);
        cv->abi.flags &= ~static_cast<std::uint32_t>(FCVAR_ALLOCATED);
    }

    // Pool-duplicate the new string.
    cv->abi.string = pool_dup(impl_->pool, value);
    if (!cv->abi.string) {
        cv->abi.string = const_cast<char *>(""); // OOM fallback; SAFETY: "" is a static literal, never written or freed
        return;
    }
    cv->abi.flags |= FCVAR_ALLOCATED;

    // Update float value.
    cv->abi.value = ::xash::utilities::atof(value);

    // Set FCVAR_CHANGED (polled by legacy DLLs).
    cv->abi.flags |= FCVAR_CHANGED;

    // Bump generation counter for lock-free change detection.
    cv->generation.fetch_add(1u, std::memory_order_release);

#if XASH_STATS
    cv->write_count.fetch_add(1u, std::memory_order_relaxed);
    cv->last_write_source = source;
#endif

#if XASH_DEBUG_CVARS
    if (impl_->break_on_write_name &&
        ::xash::utilities::stricmp(cv->abi.name, impl_->break_on_write_name) == 0) {
        ::xash::platform::console::write("[cvar] break-on-write: ");
        ::xash::platform::console::write(cv->abi.name);
        ::xash::platform::console::write("\n");
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

    impl_->stats_block.cvars_written.fetch_add(1u, std::memory_order_relaxed);
}

void CmdCvarContext::cvar_set(std::string_view name,
                               const char     *value,
                               CvarWriteSource source) noexcept
{
    if (name.empty()) return;
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

    Cvar *new_head = nullptr;
    Cvar *new_tail = nullptr;

    for (Cvar *cv = impl_->cvar_list_head; cv; ) {
        Cvar *next = cvar_list_next(cv);

        if (cv->owner_flags & owner_flags_mask) {
            // Remove from hash map.
            impl_->cvar_map.remove(cv->abi.name);

            // Free FCVAR_ALLOCATED string.
            if (cv->abi.flags & FCVAR_ALLOCATED) {
                ::xash::memory::mem_free(cv->abi.string);
                cv->abi.flags &= ~static_cast<std::uint32_t>(FCVAR_ALLOCATED);
            }

            // Restore string to def_string so legacy DLLs see a sane value.
            cv->abi.string = cv->def_string
                ? const_cast<char *>(cv->def_string) // SAFETY: restoring pool/DLL-owned def_string into the ABI char* field; not mutated through this pointer
                : const_cast<char *>("");            // SAFETY: "" is a static literal, never written or freed

            if (cv->abi.flags & FCVAR_USER_CREATED) {
                ::xash::memory::mem_free(cv->abi.name);
                if (cv->def_string)
                    ::xash::memory::mem_free(const_cast<char *>(cv->def_string)); // SAFETY: reclaiming pool memory (def_string was pool_dup'd as char* then stored const)
                ::xash::memory::mem_free(cv);
            } else if (cv->abi.flags & FCVAR_DLL_WRAPPER) {
                ::xash::memory::mem_free(cv);
            }
            // else: DLL owns the struct; leave it alone.
        } else {
            cvar_list_set_next(cv, nullptr);
            if (new_tail) cvar_list_set_next(new_tail, cv);
            else          new_head = cv;
            new_tail = cv;
        }

        cv = next;
    }

    impl_->cvar_list_head = new_head;
}

CvarAbi *CmdCvarContext::cvar_get_list() const noexcept
{
    return impl_->cvar_list_head ? &impl_->cvar_list_head->abi : nullptr;
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

const char *CmdCvarContext::cvar_variable_string(std::string_view name) const noexcept
{
    // SAFETY: cvar_find has no const overload; this query performs only a read-only lookup.
    const Cvar *cv = const_cast<CmdCvarContext *>(this)->cvar_find(name);
    return cv ? cv->abi.string : "";
}

float CmdCvarContext::cvar_variable_value(std::string_view name) const noexcept
{
    // SAFETY: cvar_find has no const overload; this query performs only a read-only lookup.
    const Cvar *cv = const_cast<CmdCvarContext *>(this)->cvar_find(name);
    return cv ? cv->abi.value : 0.0f;
}

int CmdCvarContext::cvar_variable_integer(std::string_view name) const noexcept
{
    return static_cast<int>(cvar_variable_value(name));
}

void CmdCvarContext::cvar_full_set(std::string_view name,
                                    const char     *value,
                                    std::uint32_t   flags) noexcept
{
    if (name.empty() || !value) return;

    Cvar *cv = cvar_find(name);
    if (!cv) {
        // Legacy Cvar_FullSet on a missing name → Cvar_Get(name, value,
        // flags): created with the requested flags (cvar.c:769-772).
        (void)cvar_get_or_create(name, value, flags);
        return;
    }

    // Legacy Cvar_DirectFullSet (cvar.c:594-605): force-write bypassing
    // EVERY guard, READ_ONLY included.  Quirks kept: the flags argument is
    // IGNORED for an existing cvar (cvar.c:775 passes var->flags back into
    // SetBits — a FullSet never adds flags after creation), and there is
    // no same-value skip (Cvar_Changed fires unconditionally).
    if (cv->abi.flags & FCVAR_ALLOCATED) {
        ::xash::memory::mem_free(cv->abi.string);
        cv->abi.flags &= ~static_cast<std::uint32_t>(FCVAR_ALLOCATED);
    }
    cv->abi.string = pool_dup(impl_->pool, value);
    if (!cv->abi.string) {
        cv->abi.string = const_cast<char *>(""); // OOM fallback; SAFETY: "" is a static literal, never written or freed
        return;
    }
    cv->abi.flags |= FCVAR_ALLOCATED;
    cv->abi.value  = ::xash::utilities::atof(value);

    // Cvar_Changed equivalent (same write-tail as cvar_set_direct).
    cv->abi.flags |= FCVAR_CHANGED;
    cv->generation.fetch_add(1u, std::memory_order_release);

#if XASH_STATS
    cv->write_count.fetch_add(1u, std::memory_order_relaxed);
    cv->last_write_source = CvarWriteSource::EngineInternal;
#endif

    const std::uint32_t cvar_flags = cv->abi.flags;
    const char *old_value = cv->def_string ? cv->def_string : "";
    for (std::size_t i = 0; i < impl_->observer_count; ++i) {
        const auto &entry = impl_->observers[i];
        if (entry.observer && (entry.flag_mask & cvar_flags))
            entry.observer->on_cvar_changed(cv, old_value);
    }

    impl_->stats_block.cvars_written.fetch_add(1u, std::memory_order_relaxed);
}

void CmdCvarContext::cvar_set_cheat_state() noexcept
{
    for (Cvar *cv = impl_->cvar_list_head; cv; cv = cvar_list_next(cv)) {
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
    for (Cvar *cv = impl_->cvar_list_head; cv; cv = cvar_list_next(cv)) {
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

} // namespace xash::cmd_cvar
