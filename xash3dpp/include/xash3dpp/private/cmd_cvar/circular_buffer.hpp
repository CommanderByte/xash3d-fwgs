#pragma once
// xash3dpp — fixed-capacity circular buffer (PRIVATE to cmd_cvar)
//
// Used for the XASH_DEBUG_CVARS change log.  A standalone template is used
// rather than pulling in a general-purpose container library.
//
// Behaviour:
//   • When full, the oldest entry is overwritten (ring semantics).
//   • Not thread-safe; only the game thread writes to the change log.
//   • Iteration is oldest-first via begin()/end() (range-for compatible).

#include <cstddef>
#include <utility>

namespace xash::cmd_cvar::detail {

template<typename T, std::size_t N>
class CircularBuffer {
    static_assert(N > 0, "CircularBuffer size must be > 0");
public:
    CircularBuffer() noexcept = default;

    // Push a value.  Overwrites the oldest entry when full.
    void push(const T &value) noexcept
    {
        data_[head_ % N] = value;
        ++head_;
        if (count_ < N) ++count_;
    }

    void push(T &&value) noexcept
    {
        data_[head_ % N] = std::move(value);
        ++head_;
        if (count_ < N) ++count_;
    }

    [[nodiscard]] std::size_t size()     const noexcept { return count_; }
    [[nodiscard]] bool        empty()    const noexcept { return count_ == 0; }
    [[nodiscard]] std::size_t capacity() const noexcept { return N; }

    // Access by logical index 0 = oldest, size()-1 = newest.
    [[nodiscard]] const T &operator[](std::size_t i) const noexcept
    {
        const std::size_t oldest = (count_ < N) ? 0 : (head_ % N);
        return data_[(oldest + i) % N];
    }

    void clear() noexcept { head_ = 0; count_ = 0; }

    // ---------------------------------------------------------------------------
    // Range-for support (forward iterator, oldest → newest)
    // ---------------------------------------------------------------------------

    struct Iterator {
        const CircularBuffer *buf;
        std::size_t           idx;

        [[nodiscard]] const T &operator*()  const noexcept { return (*buf)[idx]; }
        Iterator &operator++()      noexcept { ++idx; return *this; }
        [[nodiscard]] bool operator!=(const Iterator &o) const noexcept { return idx != o.idx; }
    };

    [[nodiscard]] Iterator begin() const noexcept { return { this, 0 }; }
    [[nodiscard]] Iterator end()   const noexcept { return { this, count_ }; }

private:
    T           data_[N] {};
    std::size_t head_  { 0 };
    std::size_t count_ { 0 };
};

} // namespace xash::cmd_cvar::detail
