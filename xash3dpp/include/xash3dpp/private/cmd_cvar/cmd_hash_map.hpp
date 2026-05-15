#pragma once
// xash3dpp — CmdHashMap<V>: case-insensitive fixed-bucket hash map (PRIVATE)
// Used by the cmd_cvar subsystem to store cvars, commands, and aliases.
//
// Key type  : const char* (NUL-terminated, case-insensitive via ASCII fold)
// Value     : non-owning V* pointer; caller manages the lifetime of *V.
// Nodes     : pool-backed via memory::mem_alloc / mem_free.
// Bucket cnt: limits::cvar_hash_buckets (compile-time constant, override-able).
//
// Design constraints:
//   • find()   — O(1) average; nullptr on miss.
//   • insert() — caller guarantees the name is not already present.
//   • remove() — unlinks node and returns the stored V* for caller cleanup.
//   • for_each()  — non-mutating iteration; no insert/remove during callback.
//   • clear_nodes() — free all nodes without touching V*; use before pool
//                     destroy during shutdown.

#include <xash3dpp/limits.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/utilities/string.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace xash::cmd_cvar {

template <typename V, std::size_t kBuckets = limits::cvar_hash_buckets>
class CmdHashMap {
public:
    // Construct with a null pool; call set_pool() before first insert().
    CmdHashMap() noexcept : pool_(memory::kNullPool) {}

    // Construct with a live pool ready for immediate use.
    explicit CmdHashMap(memory::PoolHandle pool) noexcept : pool_(pool) {}

    CmdHashMap(const CmdHashMap &)            = delete;
    CmdHashMap &operator=(const CmdHashMap &) = delete;
    CmdHashMap &operator=(CmdHashMap &&)      = delete;

    // Move: transfer pool + bucket array; source becomes empty.
    CmdHashMap(CmdHashMap &&o) noexcept : pool_(o.pool_) {
        for (std::size_t i = 0; i < kBuckets; ++i) {
            buckets_[i]   = o.buckets_[i];
            o.buckets_[i] = nullptr;
        }
        o.pool_ = memory::kNullPool;
    }

    // ---------------------------------------------------------------------------
    // Pool management
    // ---------------------------------------------------------------------------

    // Set (or replace) the allocator pool.  Must be called before insert()
    // when the map was default-constructed.
    void set_pool(memory::PoolHandle pool) noexcept { pool_ = pool; }

    [[nodiscard]] memory::PoolHandle pool() const noexcept { return pool_; }

    // ---------------------------------------------------------------------------
    // Core operations
    // ---------------------------------------------------------------------------

    // find() — case-insensitive lookup; nullptr on miss.
    [[nodiscard]] V *find(const char *name) const noexcept {
        if (!name) return nullptr;
        Node *n = buckets_[hash(name)];
        while (n) {
            if (utilities::stricmp(n->key, name) == 0)
                return n->value;
            n = n->next;
        }
        return nullptr;
    }

    // insert() — caller guarantees (name) is unique in the map.
    // key must remain valid for the lifetime of the entry (typically borrowed
    // from the V struct, e.g. cv->abi.name or cmd->name).
    // Returns false only on OOM.
    bool insert(const char *key, V *value) noexcept {
        Node *n = static_cast<Node *>(memory::mem_alloc(pool_, sizeof(Node)));
        if (!n) return false;
        n->key   = key;
        n->value = value;
        const std::size_t b = hash(key);
        n->next    = buckets_[b];
        buckets_[b] = n;
        return true;
    }

    // remove() — unlinks and frees the node; returns the stored V*, or nullptr.
    V *remove(const char *name) noexcept {
        if (!name) return nullptr;
        const std::size_t b  = hash(name);
        Node            **pp = &buckets_[b];
        while (*pp) {
            Node *n = *pp;
            if (utilities::stricmp(n->key, name) == 0) {
                *pp = n->next;
                V *v = n->value;
                memory::mem_free(n);
                return v;
            }
            pp = &n->next;
        }
        return nullptr;
    }

    // for_each() — iterate all values.  Fn signature: void(V*).
    // Do NOT insert or remove inside Fn.
    template <typename Fn>
    void for_each(Fn &&fn) const noexcept {
        for (std::size_t i = 0; i < kBuckets; ++i) {
            for (Node *n = buckets_[i]; n; n = n->next)
                fn(n->value);
        }
    }

    // clear_nodes() — free every chain node without touching V*.
    // Intended for shutdown where the pool is destroyed immediately after.
    void clear_nodes() noexcept {
        for (std::size_t i = 0; i < kBuckets; ++i) {
            Node *n = buckets_[i];
            while (n) {
                Node *nxt = n->next;
                memory::mem_free(n);
                n = nxt;
            }
            buckets_[i] = nullptr;
        }
    }

    // ---------------------------------------------------------------------------
    // Debug helpers (compiled only with XASH_DEBUG_CVARS)
    // ---------------------------------------------------------------------------

#if XASH_DEBUG_CVARS
    // Fill out[i] with per-bucket chain lengths for i in [0, out.size()).
    void bucket_histogram(std::span<std::size_t> out) const noexcept {
        const std::size_t n = out.size() < kBuckets ? out.size() : kBuckets;
        for (std::size_t i = 0; i < n; ++i) {
            std::size_t len = 0;
            for (const Node *nd = buckets_[i]; nd; nd = nd->next)
                ++len;
            out[i] = len;
        }
    }
#endif

private:
    struct Node {
        const char *key;
        V          *value;
        Node       *next { nullptr };
    };

    // djb2-style hash with ASCII case-fold.
    static std::size_t hash(const char *s) noexcept {
        std::uint32_t h = 5381u;
        while (*s) {
            unsigned char c = static_cast<unsigned char>(*s++);
            if (c >= 'A' && c <= 'Z')
                c = static_cast<unsigned char>(c + ('a' - 'A'));
            h = ((h << 5u) + h) ^ c;
        }
        return static_cast<std::size_t>(h) % kBuckets;
    }

    memory::PoolHandle pool_;
    Node              *buckets_[kBuckets] {};
};

} // namespace xash::cmd_cvar
